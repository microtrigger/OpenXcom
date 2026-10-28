# SPDX-License-Identifier: GPL-3.0-or-later
"""Reserve-for-kneeling indicator, not the soldier's pose or future eligibility."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, arguments, NativeFixture, CHALLENGE, EPOCH

ACTION = 'toggle_kneel_reserve'


def observation(value='off', offered=True):
    obj = state()
    obj['visible']['kneel_reserve_indicator'] = value
    obj['actions'] = [dict(id=ACTION, label='Reserve time units for kneeling',
                          label_source='visible_control_semantics', argument_kind='none')] if offered else []
    if not offered: obj['capability'] = None
    return obj


class KneelReserveFixture(NativeFixture):
    def observation(self, challenge, capability):
        obj = super().observation(challenge, capability)
        obj['visible']['kneel_reserve_indicator'] = 'off'
        obj['actions'].append(copy.deepcopy(observation()['actions'][0]))
        return obj


class KneelReserveTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(copy.deepcopy(obj), CHALLENGE, EPOCH)

    def test_schema30_and_visible_enum_independent_of_pose(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/46')
        for value in ('on', 'off'):
            for pose in ('standing', 'kneeling', None):
                obj = observation(value)
                obj['visible']['kneel_indicator'] = pose
                self.checked(obj)
        self.checked(observation(None, False))

    def test_unknown_indicator_has_no_authority(self):
        with self.assertRaises(Unavailable): self.checked(observation(None))

    def test_required_closed_enum(self):
        for value in ('none', 'snap', 'kneeling', 0, False, {}, []):
            with self.assertRaises(Unavailable): self.checked(observation(value, False))
        obj = observation()
        del obj['visible']['kneel_reserve_indicator']
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_none_arguments_only(self):
        self.assertEqual(a.ARGUMENT_KINDS[ACTION], 'none')
        a.validate_action_arguments(arguments(action=ACTION, args={}))
        for value in ({'value': 'on'}, {'kneel': True}, {'target': 'g0'}, [], None):
            request = arguments(action=ACTION, args={})
            request['args'] = value
            with self.assertRaises(Unavailable): a.validate_action_arguments(request)

    def test_state_and_semantics_exclusive(self):
        for screen in ('next_turn', 'geoscape', 'inventory', 'crash_recovery'):
            obj = state(screen=screen)
            obj['actions'] = observation()['actions']
            with self.assertRaises(Unavailable): self.checked(obj)
        obj = observation()
        obj['actions'][0]['label_source'] = 'visible_text'
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_schema29_refused(self):
        obj = observation()
        obj['schema'] = 'openxcom-agent-control/29'
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_stale_grant_zero_send(self):
        peer = KneelReserveFixture()
        try:
            old = peer.client.observe()
            peer.client.observe()
            with self.assertRaises(Unavailable): peer.client.execute(**arguments(old, action=ACTION))
            self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
        finally: peer.close()


if __name__ == '__main__': unittest.main()
