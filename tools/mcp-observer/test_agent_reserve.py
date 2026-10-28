# SPDX-License-Identifier: GPL-3.0-or-later
"""TU reserve wire contract; synthetic observations, not live qualification."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, CHALLENGE, EPOCH

class ReserveContract(unittest.TestCase):
    def observation(self, value='none', actions=('reserve_none', 'reserve_snap')):
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
        for action in ('reserve_none', 'reserve_snap'):
            with self.assertRaises(Unavailable):
                a.validate_state(self.observation(None, (action,)), CHALLENGE, EPOCH)

    def test_closed_indicator(self):
        for value in ('snapshot', 'hidden', 0, True, {}, []):
            with self.assertRaises(Unavailable):
                a.validate_state(self.observation(value, ()), CHALLENGE, EPOCH)

    def test_no_new_target_argument(self):
        for action in ('reserve_none', 'reserve_snap'):
            self.assertIn(action, a.ACTIONS)
            self.assertIn(action, a.STATE_ACTIONS['battlescape'])
            self.assertNotIn(action, a.STATE_ACTIONS['next_turn'])

if __name__ == '__main__': unittest.main()
