"""Protocol-41 ledger lifecycle over an owned synthetic socket peer.

Exercises actual client exchange/authority code, not a game or native ledger.
Rollover acknowledges dispatch receipts, never successful gameplay outcomes.
"""
import copy
import json
import socket
import threading
import unittest

import agent_control as a
from snapshots import Unavailable
from test_agent_control import EPOCH, arguments, dump, result, state


class LedgerPeer:
    def __init__(self, *, mutate_ack=None, drop_ack=False, delay_ack=False, blocked=False):
        channel, self.peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.client = a.AgentControlClient(channel, timeout=.15, action_timeout=.15, startup_timeout=.15)
        self.peer.settimeout(2)
        self.requests = []
        self.errors = []
        self.generation = 1
        self.receipts = {}
        self.retired = None
        self.retired_ack = None
        self.revision = 1
        self.sequence = 0
        self.cap_counter = 100
        self.latest = None
        self.mutate_ack = mutate_ack
        self.drop_ack = drop_ack
        self.delay_ack = delay_ack
        self.release_ack = threading.Event()
        self.blocked = blocked
        self.dispatches = 0
        self.thread = threading.Thread(target=self.run)
        self.thread.start()

    def ack(self, challenge, previous, count, status):
        return dict(schema=a.SCHEMA, mode=a.MODE, type='ledger_rollover',
                    epoch=EPOCH, challenge=challenge, status=status,
                    previous_generation=previous, generation=self.generation,
                    retired_count=count)

    def run(self):
        try:
            self.peer.send(dump(dict(schema=a.SCHEMA, mode=a.MODE, epoch=EPOCH,
                                    ledger_capacity=256, audio_history='mixed_output',
                                    command_generation=1)))
            while True:
                raw = self.peer.recv(4096)
                if not raw:
                    return
                self.requests.append(raw)
                parts = raw.decode('ascii').split(' ')
                op = parts[0]
                if parts == ['H', a.SCHEMA]:
                    obj = dict(schema=a.SCHEMA, mode=a.MODE, status='ready')
                elif op == 'R' and len(parts) == 2:
                    self.cap_counter += 1
                    self.sequence += 1
                    obj = state(parts[1], f'{self.cap_counter:032x}', self.revision, self.sequence)
                    self.latest = copy.deepcopy(obj)
                elif op == 'E' and len(parts) == 7:
                    _, ident, generation, epoch, capability, revision, action = parts
                    if (int(generation) != self.generation or epoch != EPOCH
                            or self.latest is None or capability != self.latest['capability']
                            or int(revision) != self.revision or action != 'kneel'
                            or ident in self.receipts or len(self.receipts) >= 256):
                        raise AssertionError('invalid_synthetic_execution')
                    self.dispatches += 1
                    self.revision += 1
                    self.sequence += 1
                    obj = result(ident, event_sequence=self.sequence)
                    obj['command_generation'] = self.generation
                    self.receipts[ident] = copy.deepcopy(obj)
                    self.latest = None
                elif op == 'S' and len(parts) == 3:
                    _, generation, ident = parts
                    if int(generation) != self.generation:
                        raise AssertionError('old_generation_query_sent')
                    obj = copy.deepcopy(self.receipts[ident])
                elif op == 'L' and len(parts) == 5:
                    _, challenge, epoch, generation, count = parts
                    previous, count = int(generation), int(count)
                    if epoch != EPOCH or previous != self.generation or count != len(self.receipts):
                        raise AssertionError('invalid_retirement_request')
                    if self.blocked:
                        obj = self.ack(challenge, previous, 0, 'blocked')
                    else:
                        self.generation += 1
                        self.retired = (previous, count)
                        self.receipts.clear()
                        self.latest = None
                        obj = self.ack(challenge, previous, count, 'retired')
                        self.retired_ack = copy.deepcopy(obj)
                    if self.drop_ack:
                        self.drop_ack = False
                        continue  # No queued late response; K must resolve the retained receipt.
                    if self.delay_ack:
                        self.delay_ack = False
                        if not self.release_ack.wait(2):
                            raise AssertionError('deferred_ack_barrier_timeout')
                    if self.mutate_ack:
                        self.mutate_ack(obj)
                elif op == 'K' and len(parts) == 4:
                    _, challenge, epoch, previous = parts
                    if (epoch != EPOCH or self.retired is None or int(previous) != self.retired[0]
                            or challenge != self.retired_ack['challenge']):
                        raise AssertionError('invalid_recovery_query')
                    obj = copy.deepcopy(self.retired_ack)
                else:
                    raise AssertionError('unexpected_protocol_request')
                self.peer.send(dump(obj))
        except (BrokenPipeError, ConnectionResetError):
            pass
        except BaseException as error:
            self.errors.append(type(error).__name__)
        finally:
            self.peer.close()

    def close(self):
        self.client.close()
        self.release_ack.set()
        self.thread.join(3)
        self.peer.close()
        if self.thread.is_alive() or self.errors:
            raise AssertionError('ledger_peer_cleanup_failed: ' + repr(self.errors))


class LedgerRolloverTests(unittest.TestCase):
    def peer(self, **kwargs):
        peer = LedgerPeer(**kwargs)
        self.addCleanup(peer.close)
        return peer

    def execute(self, peer, number, generation=1):
        visible = peer.client.observe()
        args = arguments(visible, command_id=f'{number:032x}')
        args['command_generation'] = generation
        return peer.client.execute(**args), args

    def test_more_than_512_dispatches_and_bare_id_reuse_with_explicit_retirement(self):
        peer = self.peer()
        for number in range(1, 257):
            receipt, _ = self.execute(peer, number)
            self.assertEqual(receipt['status'], 'dispatched')
            self.assertEqual(receipt['command_generation'], 1)
        self.assertEqual(len(peer.client.commands), 256)
        ack = peer.client.retire_receipts(acknowledge=True)
        self.assertEqual((ack['status'], ack['previous_generation'], ack['generation'], ack['retired_count']),
                         ('retired', 1, 2, 256))
        self.assertEqual(peer.client.commands, {})
        self.assertIsNone(peer.client.capability)
        for number in range(257, 513):
            receipt, _ = self.execute(peer, number, generation=2)
            self.assertEqual(receipt['command_generation'], 2)
        peer.client.retire_receipts(acknowledge=True)
        receipt, _ = self.execute(peer, 1, generation=3)
        self.assertEqual(receipt['command_generation'], 3)
        self.assertEqual(peer.dispatches, 513)
        self.assertEqual(len(peer.client.commands), 1)
        self.assertEqual(sum(raw.startswith(b'L ') for raw in peer.requests), 2)

    def test_old_generation_replay_and_query_rejected_before_send(self):
        peer = self.peer()
        _, old_args = self.execute(peer, 1)
        peer.client.retire_receipts(acknowledge=True)
        fresh = peer.client.observe()
        before = len(peer.requests)
        with self.assertRaises(Unavailable):
            peer.client.execute(**old_args)
        with self.assertRaises(Unavailable):
            peer.client.read_result(old_args['command_id'], command_generation=1)
        # Omitted generation remains 1, not an implicit grant to the current generation.
        with self.assertRaises(Unavailable):
            peer.client.execute(**arguments(fresh, command_id=f'{2:032x}'))
        self.assertEqual(len(peer.requests), before)
        self.assertEqual(peer.dispatches, 1)

    def test_generation_is_exact_integer_and_not_a_ui_snapshot_grant(self):
        peer = self.peer()
        visible = peer.client.observe()
        self.assertNotIn('command_generation', visible)
        before = len(peer.requests)
        for generation in (True, False, 0, 1.0, '1', None, 9007199254740992):
            with self.subTest(generation=generation):
                with self.assertRaises(Unavailable):
                    peer.client.execute(**arguments(visible), command_generation=generation)
                with self.assertRaises(Unavailable):
                    peer.client.read_result('0' * 32, command_generation=generation)
        self.assertEqual(len(peer.requests), before)

    def test_retirement_invalidates_current_ui_grant(self):
        peer = self.peer()
        self.execute(peer, 1)
        visible = peer.client.observe()
        peer.client.retire_receipts(acknowledge=True)
        before = len(peer.requests)
        with self.assertRaises(Unavailable):
            peer.client.execute(**arguments(visible, command_id=f'{2:032x}'), command_generation=2)
        self.assertEqual(len(peer.requests), before)
        receipt, _ = self.execute(peer, 2, generation=2)
        self.assertEqual(receipt['status'], 'dispatched')

    def test_explicit_true_acknowledgement_required_without_send(self):
        for ack in (False, None, 1, 'yes'):
            with self.subTest(ack=ack):
                peer = self.peer()
                self.execute(peer, 1)
                before = len(peer.requests)
                ledger = copy.deepcopy(peer.client.commands)
                with self.assertRaises(Unavailable):
                    peer.client.retire_receipts(acknowledge=ack)
                self.assertEqual(len(peer.requests), before)
                self.assertEqual(peer.client.commands, ledger)

    def test_unresolved_receipt_cannot_be_retired(self):
        peer = self.peer()
        _, args = self.execute(peer, 1)
        # Controlled local admission seam: no receipt was canonically acknowledged.
        peer.client.commands[args['command_id']]['result'] = None
        before = len(peer.requests)
        with self.assertRaises(Unavailable):
            peer.client.retire_receipts(acknowledge=True)
        self.assertEqual(len(peer.requests), before)
        self.assertEqual(len(peer.client.commands), 1)

    def test_non_dispatched_terminal_receipt_cannot_be_retired(self):
        peer = self.peer()
        _, args = self.execute(peer, 1)
        stored = json.loads(peer.client.commands[args['command_id']]['result'])
        stored['status'] = 'rejected'
        peer.client.commands[args['command_id']]['result'] = dump(stored)
        before = len(peer.requests)
        with self.assertRaises(Unavailable):
            peer.client.retire_receipts(acknowledge=True)
        self.assertEqual(len(peer.requests), before)
        self.assertEqual(len(peer.client.commands), 1)

    def test_malformed_ack_does_not_clear_ledger(self):
        mutations = [lambda o: o.update(generation=True),
                     lambda o: o.update(generation=3),
                     lambda o: o.update(previous_generation=2),
                     lambda o: o.update(retired_count=0),
                     lambda o: o.update(epoch='f' * 32),
                     lambda o: o.update(challenge='f' * 32),
                     lambda o: o.update(extra='not_authority')]
        for index, mutate in enumerate(mutations):
            with self.subTest(index=index):
                peer = self.peer(mutate_ack=mutate)
                self.execute(peer, 1)
                ledger = copy.deepcopy(peer.client.commands)
                with self.assertRaises(Unavailable):
                    peer.client.retire_receipts(acknowledge=True)
                self.assertEqual(peer.client.commands, ledger)
                self.assertEqual(peer.dispatches, 1)

    def test_delayed_retirement_ack_recovers_with_query_never_replays_mutation(self):
        peer = self.peer(delay_ack=True)
        self.execute(peer, 1)
        ledger = copy.deepcopy(peer.client.commands)
        with self.assertRaises(Unavailable):
            peer.client.retire_receipts(acknowledge=True)
        self.assertEqual(peer.client.commands, ledger)
        self.assertEqual(peer.client.state, 'ROLLOVER_RECOVERY_REQUIRED')
        before = len(peer.requests)
        with self.assertRaises(Unavailable):
            peer.client.read_result(f'{1:032x}', command_generation=1)
        self.assertEqual(len(peer.requests), before)
        peer.release_ack.set()
        ack = peer.client.retire_receipts(acknowledge=True)
        self.assertEqual((ack['status'], ack['generation']), ('retired', 2))
        self.assertEqual(peer.client.commands, {})
        self.assertEqual(sum(raw.startswith(b'L ') for raw in peer.requests), 1)
        self.assertEqual(sum(raw.startswith(b'K ') for raw in peer.requests), 1)
        self.assertEqual(peer.dispatches, 1)

    def test_permanently_missing_ack_retains_evidence_without_mutating_retry(self):
        peer = self.peer(drop_ack=True)
        self.execute(peer, 1)
        ledger = copy.deepcopy(peer.client.commands)
        for attempt in range(2):
            with self.subTest(attempt=attempt), self.assertRaises(Unavailable):
                peer.client.retire_receipts(acknowledge=True)
            self.assertEqual(peer.client.commands, ledger)
            self.assertEqual(peer.client.command_generation, 1)
            self.assertEqual(peer.client.state, 'ROLLOVER_RECOVERY_REQUIRED')
        self.assertEqual(sum(raw.startswith(b'L ') for raw in peer.requests), 1)
        self.assertEqual(sum(raw.startswith(b'K ') for raw in peer.requests), 0)
        self.assertEqual(peer.dispatches, 1)

    def test_native_blocked_reply_retains_ledger_and_requires_fresh_context(self):
        peer = self.peer(blocked=True)
        self.execute(peer, 1)
        ledger = copy.deepcopy(peer.client.commands)
        ack = peer.client.retire_receipts(acknowledge=True)
        self.assertEqual((ack['status'], ack['generation'], ack['retired_count']), ('blocked', 1, 0))
        self.assertEqual(peer.client.commands, ledger)
        self.assertEqual(peer.client.command_generation, 1)
        self.assertIsNone(peer.client.capability)
        self.assertEqual(peer.client.state, 'NEED_CONTEXT')
