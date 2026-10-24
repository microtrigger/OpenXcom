"""Owned synthetic queue boundaries; no historical-method execution or runtime edit."""
import ast
import hashlib
from pathlib import Path
import threading
import unittest
from unittest.mock import patch
import supervised_stance as s

from snapshots import Unavailable
from test_supervised_map_left_click import LeftClickFixture
from test_supervised_stance import dump, status, COMMAND


class BarrierFixture(LeftClickFixture):
    """Only L's second receipt is gated. Original client timeouts are unchanged."""
    def __init__(self, *, queued):
        self.release_second = threading.Event()
        self.both_sent = threading.Event()
        self.worker_done = threading.Event()
        self.worker_reason = None
        self.worker_close = 'not_attempted'
        if queued:
            self.release_second.set()
        super().__init__()

    def run(self):
        try:
            self.peer.send(self.hello)
            while True:
                raw = self.peer.recv(4096)
                if not raw:
                    return
                self.requests.append(raw)
                if raw[:1] == b'L':
                    packet = dump(status(raw[65:97].decode(), 'pending_confirmation'))
                    if self.peer.send(packet) != len(packet):
                        raise AssertionError('short_send')
                    if not self.release_second.wait(0.3):
                        raise AssertionError('barrier_timeout')
                    if self.peer.send(packet) != len(packet):
                        raise AssertionError('short_send')
                    self.both_sent.set()
                else:
                    reply = self.reply(raw)
                    if reply is None or isinstance(reply, list):
                        raise AssertionError('unexpected_reply_shape')
                    if self.peer.send(reply) != len(reply):
                        raise AssertionError('short_send')
        except (BrokenPipeError, ConnectionResetError):
            self.worker_reason = 'broken_pipe_or_reset'
        except BaseException:
            self.worker_reason = 'worker_error'
        finally:
            try:
                self.peer.close()
                self.worker_close = 'closed'
            except BaseException:
                self.worker_close = 'failed'
                if self.worker_reason is None:
                    self.worker_reason = 'close_error'
            self.worker_done.set()

    def close(self):
        failed = False
        self.release_second.set()
        try:
            self.client.close()
            self.thread.join(2)
            if self.thread.is_alive():
                failed = True
        except BaseException:
            failed = True
        finally:
            try:
                self.peer.close()
            except BaseException:
                failed = True
        if (failed or self.worker_close != 'closed' or self.worker_reason in ('worker_error', 'close_error')
                or (self.worker_reason == 'broken_pipe_or_reset' and not self.both_sent.is_set())):
            raise AssertionError('owned_fixture_cleanup_failed')


class QueueBoundaryTests(unittest.TestCase):
    def fixture(self, *, queued):
        fixture = BarrierFixture(queued=queued)
        self.addCleanup(fixture.close)
        fixture.observe_tactical()
        self.assertEqual(fixture.client.timeout, 0.3)
        return fixture

    def test_already_queued_duplicate_refused_after_first_receive(self):
        fixture = self.fixture(queued=True)
        original_packet = fixture.client.packet
        original_select = s.select.select
        zero_reads = []
        packet_completed = []
        channel = fixture.client.channel
        def observed_select(readers, writers, errors, timeout):
            result = original_select(readers, writers, errors, timeout)
            if readers == [channel] and timeout == 0 and len(zero_reads) < 3:
                zero_reads.append(bool(result[0]))
            return result # Same object/result; no new poll or readiness substitution.
        def packet_after_peer_sends(deadline, maximum):
            self.assertTrue(fixture.both_sent.wait(0.3), 'queued_barrier_not_completed')
            # Pass the original absolute deadline through, never restart/extend it.
            result = original_packet(deadline, maximum)
            packet_completed.append(True)
            return result
        fixture.client.packet = packet_after_peer_sends
        with patch.object(s.select, 'select', side_effect=observed_select):
            with self.assertRaises(Unavailable):
                fixture.client.request_map_left_click(**fixture.left_args())
        self.assertEqual(packet_completed, [True])
        self.assertEqual(zero_reads, [False, True]) # Specifically post-receive extra readiness.
        self.assertEqual(fixture.client.state, 'CLOSED')
        self.assertEqual(sum(raw[:1] == b'L' for raw in fixture.requests), 1)
        self.assertEqual(sum(raw[:1] == b'S' for raw in fixture.requests), 0)

    def test_future_duplicate_not_visible_until_released_then_preflight_refuses(self):
        fixture = self.fixture(queued=False)
        reply = fixture.client.request_map_left_click(**fixture.left_args())
        self.assertEqual(reply['status'], 'pending_confirmation')
        self.assertFalse(fixture.both_sent.is_set())
        fixture.release_second.set()
        self.assertTrue(fixture.both_sent.wait(0.3), 'late_barrier_not_completed')
        # The late packet is now queued before the next request's preflight.
        original_select = s.select.select
        zero_reads = []
        channel = fixture.client.channel
        def observed_select(readers, writers, errors, timeout):
            result = original_select(readers, writers, errors, timeout)
            if readers == [channel] and timeout == 0 and len(zero_reads) < 2:
                zero_reads.append(bool(result[0]))
            return result
        with patch.object(s.select, 'select', side_effect=observed_select):
            with self.assertRaises(Unavailable):
                fixture.client.read_status(COMMAND)
        self.assertEqual(zero_reads, [True]) # Specifically existing preflight readiness.
        self.assertEqual(fixture.client.state, 'CLOSED')
        self.assertEqual(sum(raw[:1] == b'L' for raw in fixture.requests), 1)
        self.assertEqual(sum(raw[:1] == b'S' for raw in fixture.requests), 0)


class ObservabilityMetadataTests(unittest.TestCase):
    def method(self):
        module = ast.parse(Path(__file__).with_name('test_supervised_map_left_click.py').read_text())
        cls = next(n for n in module.body if isinstance(n, ast.ClassDef) and n.name == 'ClientTests')
        return next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == 'test_malformed_initial_dispatched_or_extra_receipt_closes')

    def test_only_safe_ordinal_message_and_enumeration_added(self):
        method = self.method()
        loop = method.body[0]
        self.assertIsInstance(loop, ast.For)
        self.assertEqual(ast.dump(loop.target, include_attributes=False),
                         ast.dump(ast.parse('case_ordinal, response').body[0].value, include_attributes=False).replace('Load()', 'Store()'))
        self.assertIsInstance(loop.iter, ast.Call)
        self.assertEqual(loop.iter.func.id, 'enumerate')
        self.assertEqual(loop.iter.args[1].value, 1)
        self.assertEqual(len(loop.iter.args[0].elts), 7)
        context = next(n for n in loop.body if isinstance(n, ast.With))
        keywords = context.items[0].context_expr.keywords
        self.assertEqual(len(keywords), 1)
        self.assertEqual(keywords[0].arg, 'msg')
        self.assertEqual(ast.dump(keywords[0].value, include_attributes=False),
                         ast.dump(ast.parse('"receipt_case_" + str(case_ordinal)').body[0].value, include_attributes=False))
        # Erase exactly the diagnostic additions; original method AST must match.
        loop.target = ast.Name(id='response', ctx=ast.Store())
        loop.iter = loop.iter.args[0]
        context.items[0].context_expr.keywords = []
        digest = hashlib.sha256(ast.dump(method, include_attributes=False).encode()).hexdigest()
        self.assertEqual(digest, '90645a478f31b4df09317fb24cc72f3627743c3e6fed1bb4e33fa23db8dab426')

    def test_ordinal_messages_are_bounded_and_payload_independent(self):
        messages = ['receipt_case_' + str(i) for i in range(1, 8)]
        self.assertEqual(len(set(messages)), 7)
        self.assertTrue(all(len(message.encode()) == 14 for message in messages))
        self.assertEqual(messages[0], 'receipt_case_1')
        self.assertEqual(messages[-1], 'receipt_case_7')


if __name__ == '__main__':
    unittest.main()
