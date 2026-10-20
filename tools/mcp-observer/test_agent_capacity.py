# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
"""/7 bounded process/session ledger; no durable recovery or game execution."""
import copy
import unittest
from unittest.mock import patch

import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, arguments, dump, EPOCH, COMMAND


class CapacityTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs); self.addCleanup(native.close); return native

    def test_hello_requires_exact_256_before_any_request(self):
        self.assertEqual(a.MAX_COMMANDS, 256)
        for capacity in (0, 64, 255, 257, 512, 256.0, True, '256', None):
            hello = dict(schema=a.SCHEMA, mode=a.MODE, epoch=EPOCH,
                         ledger_capacity=capacity, audio_history='mixed_output')
            native = self.fixture(hello=hello)
            with self.subTest(capacity=capacity), self.assertRaises(Unavailable): native.client.observe()
            self.assertEqual(native.requests, [])

    def test_saturated_ledger_preserves_first_last_duplicates_then_overflow_is_terminal(self):
        native = self.fixture(); original = []
        for index in range(256):
            args = arguments(native.client.observe(), command_id=f'{index:032x}', action='next_soldier')
            original.append((args, native.client.execute(**args)))
        fresh = native.client.observe()
        for args, receipt in (original[0], original[-1]):
            count = len(native.requests)
            self.assertEqual(native.client.execute(**args), receipt)
            self.assertEqual(native.requests[count:], [('S ' + args['command_id']).encode()])
            self.assertEqual(native.client.capability, fresh['capability'])
            self.assertEqual(native.client.read_result(args['command_id']), receipt)
        before = copy.deepcopy(native.client.commands); count = len(native.requests)
        changed = dict(original[0][0], action='previous_soldier')
        with self.assertRaises(Unavailable): native.client.execute(**changed)
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.commands, before)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(fresh, command_id=COMMAND))
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.commands, before)
        self.assertEqual(native.client.state, 'CLOSED')
        for operation in (native.client.observe, lambda: native.client.read_result(original[0][0]['command_id']),
                          lambda: native.client.read_events(EPOCH, 0, 1),
                          lambda: native.client.execute(**original[0][0])):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)
        self.assertEqual(sum(packet.startswith(b'E ') for packet in native.requests), 256)

    def test_refused_and_unknown_native_receipts_also_spend_shared_capacity(self):
        def transform(operation, value):
            if operation in ('E', 'S'):
                status = ('dispatched', 'unavailable', 'unknown_after_dispatch')[int(value['command_id'], 16) % 3]
                value['status'] = status
                if status != 'dispatched': value['observation'] = None
            return value
        native = self.fixture(transform=transform)
        statuses = set()
        for index in range(256):
            action = ('next_soldier', 'previous_soldier', 'end_turn')[index % 3]
            args = arguments(native.client.observe(), command_id=f'{index:032x}', action=action)
            statuses.add(native.client.execute(**args)['status'])
        self.assertEqual(statuses, {'dispatched', 'unavailable', 'unknown_after_dispatch'})
        self.assertEqual(len(native.client.commands), 256)
        fresh = native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(fresh))
        self.assertEqual(len(native.requests), count)

    def test_local_preflight_rejections_and_unknown_status_do_not_allocate(self):
        native = self.fixture(); old = native.client.observe(); fresh = native.client.observe()
        count = len(native.requests)
        for index in range(300):
            with self.assertRaises(Unavailable): native.client.execute(**arguments(old, command_id=f'{index:032x}'))
            with self.assertRaises(Unavailable): native.client.read_result(f'{index:032x}')
        self.assertEqual(native.client.commands, {}); self.assertEqual(len(native.requests), count)
        self.assertEqual(native.client.capability, fresh['capability'])

    def test_reserved_but_definitely_unsent_failure_cannot_recover_or_replay(self):
        native = self.fixture(); args = arguments(native.client.observe())
        count = len(native.requests)
        with patch.object(native.client, 'exchange', side_effect=Unavailable('Unavailable')):
            with self.assertRaises(Unavailable) as caught: native.client.execute(**args)
        self.assertNotIsInstance(caught.exception, a.ActionOutcomeUnknown)
        self.assertIn(COMMAND, native.client.commands); self.assertEqual(len(native.requests), count)
        self.assertEqual(native.client.state, 'CLOSED')
        with self.assertRaises(Unavailable): native.client.execute(**args)
        self.assertEqual(len(native.requests), count)

    def test_lost_mutating_reply_is_unknown_and_never_replayed(self):
        native = self.fixture(transform=lambda op, value: None if op == 'E' else value)
        args = arguments(native.client.observe())
        with self.assertRaises(a.ActionOutcomeUnknown) as caught: native.client.execute(**args)
        self.assertEqual(caught.exception.public_error()['status'], 'outcome_unknown')
        self.assertFalse(caught.exception.public_error()['retryable'])
        count = len(native.requests)
        for operation in (lambda: native.client.execute(**args), native.client.observe,
                          lambda: native.client.read_result(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)
        self.assertEqual(sum(packet.startswith(b'E ') for packet in native.requests), 1)

    def test_new_epoch_is_new_session_without_old_receipts_or_authority(self):
        old = self.fixture(); old_args = arguments(old.client.observe()); old.client.execute(**old_args)
        old.client.close(); self.assertIn(COMMAND, old.client.commands)
        new_epoch = '9' * 32
        hello = dict(schema=a.SCHEMA, mode=a.MODE, epoch=new_epoch, ledger_capacity=256, audio_history='mixed_output')
        def transform(operation, value):
            if operation == 'R': value['epoch'] = new_epoch
            return value
        new = self.fixture(hello=hello, transform=transform); new.client.observe()
        count = len(new.requests)
        self.assertEqual(new.client.commands, {})
        with self.assertRaises(Unavailable): new.client.read_result(COMMAND)
        with self.assertRaises(Unavailable): new.client.execute(**old_args)
        self.assertEqual(len(new.requests), count); self.assertEqual(new.client.state, 'READY')
        self.assertEqual(new.client.epoch, new_epoch)
        # No durable exactly-once claim: only current-session IDs/receipts exist.
        self.assertNotIn(COMMAND, new.client.commands)

    def test_returned_mutable_receipt_cannot_rewrite_terminal_identity(self):
        native = self.fixture(); args = arguments(native.client.observe())
        receipt = native.client.execute(**args); expected = copy.deepcopy(receipt)
        self.assertIsInstance(native.client.commands[COMMAND]['result'], bytes)
        receipt['observation']['visible']['selected_name'] = 'Caller-mutated'
        receipt['event_sequence'] = 9999
        self.assertEqual(native.client.execute(**args), expected)
        status = native.client.read_result(COMMAND)
        status['observation']['visible']['selected_name'] = 'Native-mutated'
        native.results[COMMAND]['observation']['visible']['selected_name'] = 'Native-mutated'
        with self.assertRaises(Unavailable): native.client.read_result(COMMAND)
        self.assertEqual(native.client.state, 'CLOSED')
        self.assertEqual(sum(packet.startswith(b'E ') for packet in native.requests), 1)

    def test_changed_typed_arguments_conflict_without_replacement(self):
        native = self.fixture(screen='new_game')
        args = arguments(native.client.observe(), action='set_ironman', args={'value': True})
        receipt = native.client.execute(**args); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**dict(args, args={'value': False}))
        self.assertEqual(len(native.requests), count)
        self.assertEqual(native.client.execute(**args), receipt)
        self.assertEqual(sum(packet.startswith(b'E ') for packet in native.requests), 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
