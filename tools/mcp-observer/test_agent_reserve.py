# SPDX-License-Identifier: GPL-3.0-or-later
"""TU reserve wire contract; synthetic observations, not live qualification."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, arguments, NativeFixture, CHALLENGE, EPOCH

RESERVES = ('reserve_none', 'reserve_snap', 'reserve_aimed', 'reserve_auto')


class ReserveFixture(NativeFixture):
    def observation(self, challenge, capability):
        obj = super().observation(challenge, capability)
        obj['actions'].extend(dict(id=action, label=action,
            label_source='visible_control_semantics', argument_kind='none')
            for action in RESERVES)
        return obj

class ReserveContract(unittest.TestCase):
    def observation(self, value='none', actions=RESERVES):
        obj = state()
        obj['visible']['reserve_indicator'] = value
        exemplar = copy.deepcopy(obj['actions'][0])
        obj['actions'] = []
        if not actions: obj['capability'] = None
        for action in actions:
            item = dict(exemplar, id=action, label=action)
            obj['actions'].append(item)
        return obj

    def test_visible_states(self):
        for value in ('none', 'snap', 'aimed', 'auto'):
            a.validate_state(self.observation(value), CHALLENGE, EPOCH)

    def test_unknown_with_no_grant(self):
        a.validate_state(self.observation(None, ()), CHALLENGE, EPOCH)

    def test_no_control_from_unknown_indicator(self):
        for action in RESERVES:
            with self.assertRaises(Unavailable):
                a.validate_state(self.observation(None, (action,)), CHALLENGE, EPOCH)

    def test_closed_indicator(self):
        for value in ('snapshot', 'hidden', 0, True, {}, []):
            with self.assertRaises(Unavailable):
                a.validate_state(self.observation(value, ()), CHALLENGE, EPOCH)

    def test_no_new_target_argument(self):
        for action in RESERVES:
            self.assertIn(action, a.ACTIONS)
            self.assertIn(action, a.STATE_ACTIONS['battlescape'])
            self.assertNotIn(action, a.STATE_ACTIONS['next_turn'])
            self.assertEqual(a.ARGUMENT_KINDS[action], 'none')
            a.validate_action_arguments(arguments(action=action, args={}))
            for args in ({'mode': 'aimed'}, {'target': 'g0'}, {'confirm': True}, []):
                request = arguments(action=action, args={})
                request['args'] = args
                with self.assertRaises(Unavailable): a.validate_action_arguments(request)

    def test_aimed_auto_exclusive_state_and_semantics(self):
        for action in ('reserve_aimed', 'reserve_auto'):
            for screen in ('next_turn', 'geoscape', 'inventory'):
                obj = state(screen=screen)
                obj['actions'] = self.observation(actions=(action,))['actions']
                with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
            obj = self.observation(actions=(action,))
            obj['actions'][0]['label_source'] = 'visible_text'
            with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_aimed_auto_stale_grant_zero_send(self):
        for action in ('reserve_aimed', 'reserve_auto'):
            peer = ReserveFixture()
            try:
                old = peer.client.observe()
                peer.client.observe()
                with self.assertRaises(Unavailable):
                    peer.client.execute(**arguments(old, action=action))
                self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
            finally: peer.close()

    def test_unoffered_reserve_zero_send(self):
        # Native idle/turn/overlay guards remove controls. The adapter must not
        # synthesize authority merely because the painted indicator is known.
        for action in ('reserve_aimed', 'reserve_auto'):
            peer = NativeFixture()
            try:
                shown = peer.client.observe()
                with self.assertRaises(Unavailable):
                    peer.client.execute(**arguments(shown, action=action))
                self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
            finally: peer.close()

    def test_schema28_refused(self):
        obj = self.observation()
        obj['schema'] = 'openxcom-agent-control/28'
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

if __name__ == '__main__': unittest.main()
