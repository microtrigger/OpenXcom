#!/usr/bin/env python3
"""Focused recorded-packet replay checks; supply independently generated native packets."""
import argparse
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
sys.dont_write_bytecode = True
import replay_client_server_v22 as r

class ReplayTests(unittest.TestCase):
    def setUp(self):
        self.schedule = r.load(PACKETS / 'xcom1-article-type-5-schedule.json')
        self.responses = [(PACKETS / e['response_file']).read_bytes() for e in self.schedule]

    def test_classification_keeps_offered_native_unavailable(self):
        mapped, indices, known, blocked, duplicates = r.plan(self.schedule, self.responses)
        self.assertEqual(list(blocked), [11])
        self.assertNotIn(11, indices)
        self.assertIn(14, indices)  # Offered close fails native queued-scroll ABA.
        self.assertEqual(mapped[indices.index(14)], self.schedule[14]['request'].encode())
        self.assertEqual(duplicates, 4)
        self.assertEqual(mapped[indices.index(6)][:2], b'S ')
        self.assertNotIn(blocked[11]['command_id'], known)

    def test_real_server_client_replay(self):
        result = r.replay(PACKETS / 'xcom1-article-type-5-schedule.json', self.schedule)
        self.assertEqual(result['preflight_rejected_native_cases'], [11])
        self.assertEqual(result['duplicate_E_mapped_to_status_S'], 4)
        self.assertIn('dispatched', result['receipt_statuses'])
        self.assertIn('unavailable', result['receipt_statuses'])

    def test_changed_duplicate_is_not_hidden(self):
        self.schedule[6]['request'] += ' up'
        with self.assertRaises(ValueError):
            r.plan(self.schedule, self.responses)

    def test_unadvertised_native_dispatch_is_failure(self):
        value = json.loads(self.responses[11]); value['status'] = 'dispatched'
        self.responses[11] = json.dumps(value).encode()
        with self.assertRaises(ValueError):
            r.plan(self.schedule, self.responses)

    def test_unadvertised_event_mutation_is_failure(self):
        value = json.loads(self.responses[11]); value['event_sequence'] += 1
        self.responses[11] = json.dumps(value).encode()
        with self.assertRaises(ValueError):
            r.plan(self.schedule, self.responses)

    def test_wrong_command_identity_is_failure(self):
        value = json.loads(self.responses[11]); value['command_id'] = 'f' * 32
        self.responses[11] = json.dumps(value).encode()
        with self.assertRaises(ValueError):
            r.plan(self.schedule, self.responses)

    def test_stale_authority_is_not_classified_as_forbidden_action(self):
        parts = self.schedule[11]['request'].split(); parts[4] = '8'
        self.schedule[11]['request'] = ' '.join(parts)
        with self.assertRaisesRegex(ValueError, 'stale or absent authority'):
            r.plan(self.schedule, self.responses)

    def test_client_mutation_at_preflight_is_detected(self):
        original = r.a.AgentControlClient.execute
        def mutated(client, **args):
            if args['action'] not in client.available_actions and args['command_id'] not in client.commands:
                client.event_sequence += 1
            return original(client, **args)
        with patch.object(r.a.AgentControlClient, 'execute', mutated):
            with self.assertRaisesRegex(ValueError, 'changed client state'):
                r.replay(PACKETS / 'xcom1-article-type-5-schedule.json', self.schedule)

    def test_native_exchange_at_preflight_is_detected(self):
        original = r.a.AgentControlClient.execute
        def sends(client, **args):
            if args['action'] not in client.available_actions and args['command_id'] not in client.commands:
                client.exchange(b'forbidden', 1)
            return original(client, **args)
        with patch.object(r.a.AgentControlClient, 'execute', sends):
            with self.assertRaisesRegex(AssertionError, 'Forbidden action attempted native exchange'):
                r.replay(PACKETS / 'xcom1-article-type-5-schedule.json', self.schedule)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--packets', type=Path, required=True)
    args, remaining = parser.parse_known_args()
    PACKETS = args.packets.resolve()
    r.configure(args.source)
    unittest.main(argv=[sys.argv[0]] + remaining)
