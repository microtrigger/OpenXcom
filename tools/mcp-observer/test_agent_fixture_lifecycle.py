"""Deterministic synthetic fixture lifecycle checks; no game or historical loop."""
import copy
import unittest
from unittest.mock import patch
import test_agent_control as f


class Peer:
    def __init__(self, packets=(), receive_error=None, close_error=False):
        self.packets = iter(packets)
        self.receive_error = receive_error
        self.close_error = close_error
        self.sent = []
        self.closes = 0

    def settimeout(self, value):
        self.timeout = value

    def send(self, raw):
        self.sent.append(raw)
        return len(raw)

    def recv(self, size):
        if self.receive_error is not None:
            raise self.receive_error
        return next(self.packets, b'')

    def close(self):
        self.closes += 1
        if self.close_error:
            raise RuntimeError('PRIVATE-CLOSE')


class Thread:
    def __init__(self, target=None, alive=False, join_error=False):
        self.target = target
        self.alive = alive
        self.join_error = join_error
        self.joins = []

    def start(self):
        pass

    def join(self, timeout):
        self.joins.append(timeout)
        if self.join_error:
            raise RuntimeError('PRIVATE-JOIN')

    def is_alive(self):
        return self.alive


class Client:
    def __init__(self, *args, **kwargs):
        self.closes = 0

    def close(self):
        self.closes += 1


class FixtureLifecycleTests(unittest.TestCase):
    def fixture(self, peer=None, **kwargs):
        peer = Peer() if peer is None else peer
        with patch.object(f.socket, 'socketpair', return_value=(object(), peer)), \
             patch.object(f.a, 'AgentControlClient', Client), \
             patch.object(f.threading, 'Thread', Thread):
            fixture = f.NativeFixture(**kwargs)
        return fixture

    def check_safe(self, fixture, reason):
        self.assertTrue(fixture.worker_finished)
        self.assertEqual(fixture.worker_phase, 'exit')
        self.assertEqual(fixture.worker_reason, reason)
        self.assertEqual(len(fixture.errors), 1)
        error = fixture.errors[0]
        self.assertEqual(str(error), reason)
        self.assertIsNone(error.__context__)
        self.assertIsNone(error.__cause__)
        self.assertIsNone(error.__traceback__)

    def test_all_four_execute_checks_are_explicit_and_precede_mutation(self):
        original = ['E', f.COMMAND, f.EPOCH, f.CAPABILITY, '1', 'next_soldier']
        cases = []
        bad = original.copy(); bad[2] = '0' * 32
        cases.append((bad, 'verification_epoch_capability', False))
        bad = original.copy(); bad[4] = '2'
        cases.append((bad, 'verification_revision_action', False))
        cases.append((original.copy(), 'verification_duplicate', True))
        cases.append((original + ['PRIVATE-EXTRA'], 'verification_arity', False))
        for command, reason, duplicate in cases:
            with self.subTest(reason=reason):
                fixture = self.fixture(Peer([' '.join(command).encode()]))
                fixture.latest = f.state()
                if duplicate:
                    fixture.results[f.COMMAND] = {'sentinel': True}
                before = copy.deepcopy(fixture.results)
                fixture.run()
                self.check_safe(fixture, reason)
                self.assertEqual(fixture.worker_failure_phase, 'verify')
                self.assertEqual(fixture.revision, 1)
                self.assertEqual(fixture.sequence, 0)
                self.assertEqual(fixture.results, before)
                self.assertEqual(len(fixture.peer.sent), 1) # hello only
                self.assertEqual(fixture.worker_close, 'closed')

    def test_valid_execute_retains_reply_and_state_transition(self):
        wire = f'E {f.COMMAND} {f.EPOCH} {f.CAPABILITY} 1 next_soldier'.encode()
        fixture = self.fixture(Peer([wire]))
        fixture.latest = f.state()
        fixture.run()
        self.assertEqual(fixture.errors, [])
        self.assertEqual((fixture.revision, fixture.sequence), (2, 1))
        self.assertEqual(fixture.results[f.COMMAND]['status'], 'dispatched')
        self.assertEqual(len(fixture.peer.sent), 2)
        self.assertEqual(fixture.worker_reason, 'eof')
        self.assertEqual(fixture.peer.closes, 1)

    def test_transform_failure_drops_original_exception_and_first_fatal_survives_close(self):
        original = RuntimeError('PRIVATE-TRANSFORM')
        def transform(command, obj):
            raise original
        fixture = self.fixture(Peer([b'R ' + f.CHALLENGE.encode()], close_error=True), transform=transform)
        fixture.run()
        self.check_safe(fixture, 'worker_other')
        self.assertIsNot(fixture.errors[0], original)
        self.assertEqual(fixture.worker_failure_phase, 'transform')
        self.assertEqual(fixture.worker_close, 'failed')
        self.assertEqual(len(fixture.peer.sent), 1)

    def test_exception_classification_and_finally_close(self):
        cases = ((TimeoutError('PRIVATE'), 'worker_timeout'),
                 (OSError('PRIVATE'), 'worker_os_error'),
                 (ValueError('PRIVATE'), 'worker_value_error'),
                 (AssertionError('PRIVATE'), 'worker_assertion'),
                 (KeyboardInterrupt('PRIVATE'), 'worker_other'))
        for error, reason in cases:
            with self.subTest(reason=reason):
                fixture = self.fixture(Peer(receive_error=error))
                fixture.run()
                self.check_safe(fixture, reason)
                self.assertEqual(fixture.worker_failure_phase, 'receive')
                self.assertEqual(fixture.peer.closes, 1)

    def test_nonfatal_eof_and_broken_pipe_or_reset(self):
        for error in (None, BrokenPipeError('PRIVATE'), ConnectionResetError('PRIVATE')):
            fixture = self.fixture(Peer(receive_error=error))
            fixture.run()
            self.assertEqual(fixture.errors, [])
            self.assertEqual(fixture.worker_reason, 'eof' if error is None else 'broken_pipe_or_reset')
            self.assertEqual(fixture.worker_close, 'closed')
            self.assertTrue(fixture.worker_finished)

    def test_transform_broken_pipe_retains_phase_without_causal_claim(self):
        def transform(command, obj):
            raise BrokenPipeError('PRIVATE-TRANSFORM')
        fixture = self.fixture(Peer([b'R ' + f.CHALLENGE.encode()]), transform=transform)
        fixture.run()
        self.assertEqual(fixture.errors, [])
        self.assertEqual(fixture.worker_reason, 'broken_pipe_or_reset')
        self.assertEqual(fixture.worker_failure_phase, 'transform')
        self.assertEqual(fixture.worker_phase, 'exit')
        self.assertEqual(fixture.worker_close, 'closed')

    def test_unexpected_packet_and_close_failure_are_bounded(self):
        fixture = self.fixture(Peer([b'PRIVATE-WIRE']))
        fixture.run()
        self.check_safe(fixture, 'unexpected_request')
        fixture = self.fixture(Peer(close_error=True))
        fixture.run()
        self.check_safe(fixture, 'peer_close_failure')

    def test_cleanup_alive_and_join_failure_keep_single_original_join_bound(self):
        for alive, join_error, reason in ((True, False, 'cleanup_worker_alive'),
                                           (False, True, 'cleanup_join_failure')):
            fixture = self.fixture()
            fixture.thread = Thread(alive=alive, join_error=join_error)
            with self.assertRaises(f.FixtureFailure) as caught:
                fixture.close()
            self.assertEqual(str(caught.exception), reason)
            self.assertEqual(fixture.thread.joins, [2])
            self.assertEqual(fixture.peer.closes, 1)
            self.assertEqual(fixture.client.closes, 1)

    def test_cleanup_success_and_worker_error_propagation(self):
        fixture = self.fixture()
        fixture.run()
        fixture.close()
        self.assertEqual(fixture.thread.joins, [2])
        self.assertEqual(fixture.peer.closes, 2) # worker plus idempotent fallback
        fixture = self.fixture(Peer([b'PRIVATE-WIRE']))
        fixture.run()
        with self.assertRaises(f.FixtureFailure) as caught:
            fixture.close()
        self.assertEqual(str(caught.exception), 'unexpected_request')
        self.assertIsNot(caught.exception, fixture.errors[0])
        self.check_safe(fixture, 'unexpected_request')

    def test_cleanup_close_failures_are_fixed_and_fallback_runs(self):
        fixture = self.fixture(Peer(close_error=True))
        with self.assertRaises(f.FixtureFailure) as caught:
            fixture.close()
        self.assertEqual(str(caught.exception), 'peer_close_failure')
        self.assertEqual(fixture.thread.joins, [2])
        self.assertEqual(fixture.cleanup_peer_close, 'failed')
        fixture = self.fixture()
        def fail_close():
            raise RuntimeError('PRIVATE-CLIENT-CLOSE')
        fixture.client.close = fail_close
        with self.assertRaises(f.FixtureFailure) as caught:
            fixture.close()
        self.assertEqual(str(caught.exception), 'cleanup_client_close_failure')
        self.assertEqual(fixture.thread.joins, [])
        self.assertEqual(fixture.peer.closes, 1)

    def test_reason_allowlist_never_formats_untrusted_value(self):
        class Secret:
            def __str__(self):
                raise RuntimeError('must not format')
            __repr__ = __str__
        for value in (Secret(), 'PRIVATE-REASON', [], None):
            self.assertEqual(str(f.FixtureFailure(value)), 'worker_other')
        with self.assertRaises(f.FixtureFailure):
            f.fixture_check(False, 'verification_duplicate')
        f.fixture_check(True, 'verification_duplicate')


if __name__ == '__main__':
    unittest.main()
