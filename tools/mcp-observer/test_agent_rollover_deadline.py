"""Consumed rollover ACK at deadline must reconcile via K, never wait/replay L."""
import copy
import unittest
from unittest.mock import patch
import agent_control as a
from snapshots import Unavailable
from test_agent_ledger_rollover import LedgerPeer
from test_agent_control import arguments

class LedgerDeadlineTests(unittest.TestCase):
    def test_consumed_ack_post_receive_timeout_recovers_by_query(self):
        peer = LedgerPeer()
        self.addCleanup(peer.close)
        client = peer.client
        visible = client.observe()
        client.execute(**arguments(visible))
        original = copy.deepcopy(client.commands)
        actual_packet = client.packet
        monotonic = a.time.monotonic
        clock = [None]
        def consume_at_deadline(deadline, **kwargs):
            raw = actual_packet(deadline, **kwargs)
            clock[0] = deadline + .001
            return raw
        def now():
            return monotonic() if clock[0] is None else clock[0]
        with patch.object(client, 'packet', side_effect=consume_at_deadline), patch.object(a.time, 'monotonic', side_effect=now):
            with self.assertRaises(Unavailable):
                client.retire_receipts(acknowledge=True)
        self.assertEqual(client.state, 'ROLLOVER_RECOVERY_REQUIRED')
        self.assertFalse(client._rollover_pending['awaiting_ack'])
        self.assertEqual(client.commands, original)
        self.assertEqual(client.command_generation, 1)
        # The same deadline edge on the read-only K response must also be
        # recoverable without inventing an outstanding response or sending L.
        clock[0] = None
        with patch.object(client, 'packet', side_effect=consume_at_deadline), patch.object(a.time, 'monotonic', side_effect=now):
            with self.assertRaises(Unavailable):
                client.retire_receipts(acknowledge=True)
        self.assertFalse(client._rollover_pending['awaiting_ack'])
        self.assertEqual(client.commands, original)
        self.assertEqual(client.command_generation, 1)
        ack = client.retire_receipts(acknowledge=True)
        self.assertEqual(ack['status'], 'retired')
        self.assertEqual(client.command_generation, 2)
        self.assertEqual(client.commands, {})
        self.assertEqual([r[:1] for r in peer.requests if r[:1] in (b'L', b'K')], [b'L', b'K', b'K'])
        self.assertEqual(peer.dispatches, 1)

if __name__ == '__main__':
    unittest.main()
