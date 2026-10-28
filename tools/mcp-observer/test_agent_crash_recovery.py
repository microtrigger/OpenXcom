# SPDX-License-Identifier: GPL-3.0-or-later
"""Crash-site recovery caption/control only; no inferred mission outcome."""
import copy
import unittest

import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

ACTION = 'acknowledge_crash_recovery'


class CrashRecoveryTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(copy.deepcopy(obj), CHALLENGE, EPOCH)

    def test_fresh_nullable_caption_and_exact_catalog(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/28')
        self.assertEqual(a.STATE_ACTIONS['crash_recovery'], {ACTION})
        self.assertEqual(a.ARGUMENT_KINDS[ACTION], 'none')
        for caption in (None, '', 'ALIEN SUB is recovered', 'é' * 512):
            obj = state(screen='crash_recovery')
            obj['visible']['message'] = caption
            self.checked(obj)
        obj.update(actions=[], capability=None)
        self.checked(obj)

    def test_missing_extra_and_malformed_caption_rejected(self):
        for visible in ({}, {'message': None, 'outcome': 'won'},
                        {'message': True}, {'message': 1}, {'message': {}},
                        {'message': 'x' * 4097}):
            obj = state(screen='crash_recovery')
            obj['visible'] = visible
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_empty_arguments_only(self):
        a.validate_action_arguments(arguments(action=ACTION, args={}))
        for args in ({'confirm': True}, {'row': 'r0'}, {'x': 1}, None, []):
            obj = arguments(action=ACTION, args={})
            obj['args'] = args
            with self.assertRaises(Unavailable): a.validate_action_arguments(obj)
        obj = arguments(action=ACTION, args={})
        del obj['args']
        with self.assertRaises(Unavailable): a.validate_action_arguments(obj)

    def test_action_is_exclusive_to_crash_recovery(self):
        descriptor = state(screen='crash_recovery')['actions']
        for screen in ('geoscape', 'infobox_ok', 'lost_in_service', 'unsupported'):
            obj = state(screen=screen)
            obj['actions'] = copy.deepcopy(descriptor)
            with self.assertRaises(Unavailable): self.checked(obj)
        obj = state(screen='crash_recovery')
        obj['actions'] = state(screen='infobox_ok')['actions']
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_schema_27_is_not_accepted(self):
        obj = state(screen='crash_recovery')
        obj['schema'] = 'openxcom-agent-control/27'
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_stale_grant_never_sends(self):
        peer = NativeFixture(screen='crash_recovery')
        try:
            old = peer.client.observe()
            peer.client.observe()
            with self.assertRaises(Unavailable):
                peer.client.execute(**arguments(old, action=ACTION))
            self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
        finally: peer.close()

    def test_one_native_dispatch_no_successor_assumption(self):
        peer = NativeFixture(screen='crash_recovery',
                             transitions={('crash_recovery', ACTION): 'unsupported'})
        try:
            request = arguments(peer.client.observe(), action=ACTION)
            receipt = peer.client.execute(**request)
            self.assertEqual(receipt['status'], 'dispatched')
            self.assertEqual(peer.client.execute(**request), receipt)
            self.assertEqual(peer.client.observe()['state'], 'unsupported')
            self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests), 1)
        finally: peer.close()


if __name__ == '__main__':
    unittest.main()
