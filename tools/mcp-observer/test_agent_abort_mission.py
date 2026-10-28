# SPDX-License-Identifier: GPL-3.0-or-later
"""Abort mission UI contract; synthetic authority checks, not live outcome proof."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

ACTIONS = ('open_abort_mission', 'confirm_abort_mission', 'cancel_abort_mission')
FIELDS = ('prompt', 'entrance', 'exit', 'outside')


def abort_state(**kwargs):
    obj = state(**kwargs)
    obj.update(state='abort_mission', visible=dict(prompt='Abort mission?',
        entrance='Units in entrance: 1', exit=None, outside='Units outside: 2',
        buttons=[dict(id='ok', label='OK'), dict(id='cancel', label='Cancel')]))
    obj['actions'] = [dict(id=action, label=label, label_source='visible_text', argument_kind='none')
                      for action, label in zip(ACTIONS[1:], ('OK', 'Cancel'))]
    return obj


class AbortFixture(NativeFixture):
    def observation(self, challenge, capability):
        obj = super().observation(challenge, capability)
        projected = abort_state(challenge=challenge, capability=capability,
                                revision=obj['revision'], event_sequence=obj['event_sequence'])
        return projected


class AbortMissionTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def test_schema_exact_shape_and_nullable_displayed_captions(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/44')
        self.checked(abort_state())
        for field in FIELDS:
            for value in (None, '', 'Displayed\nlocalized caption: 17'):
                obj = abort_state(); obj['visible'][field] = value; self.checked(obj)
            for value in (0, True, [], {}, 'x' * 1025):
                obj = abort_state(); obj['visible'][field] = value
                with self.subTest(field=field, value=value), self.assertRaises(Unavailable): self.checked(obj)
        for field in (*FIELDS, 'buttons'):
            obj = abort_state(); del obj['visible'][field]
            with self.assertRaises(Unavailable): self.checked(obj)
        for field in ('survivors', 'unitsInExit', 'mission_id', 'next_stage', 'success'):
            obj = abort_state(); obj['visible'][field] = 1
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_order_labels_and_action_consistency(self):
        for buttons in (None, {}, [dict(id='ok', label=None)], [dict(id='ok', label='')],
                        [dict(id='ok', label='OK', count=1)],
                        [dict(id='cancel', label='Cancel'), dict(id='ok', label='OK')],
                        [dict(id='ok', label='OK')]*2, [dict(id='confirm', label='OK')]):
            obj = abort_state(); obj['visible']['buttons'] = buttons
            with self.subTest(buttons=buttons), self.assertRaises(Unavailable): self.checked(obj)
        for index in range(2):
            obj = abort_state(); obj['actions'][index]['label'] = 'Invented'
            with self.assertRaises(Unavailable): self.checked(obj)
            obj = abort_state(); obj['visible']['buttons'].pop(index)
            with self.assertRaises(Unavailable): self.checked(obj)
            obj['actions'].pop(index); self.checked(obj)
        obj = abort_state(); obj['visible']['buttons'] = []; obj['actions'] = []; obj['capability'] = None
        self.checked(obj)
        obj = abort_state(); obj['visible']['buttons'][0]['label'] = 'Продолжить'; obj['actions'][0]['label'] = 'Продолжить'
        self.checked(obj)

    def test_screen_scoping_and_no_arguments(self):
        for action in ACTIONS:
            self.assertEqual(a.ARGUMENT_KINDS[action], 'none')
            self.assertTrue(a.canonical_action_request(arguments(action=action, args={})).endswith(action.encode()))
            for args in ({'confirm': True}, {'row': 'r0'}, {'count': 1}, {'x': 1, 'y': 2}, [], None, True, ''):
                request = arguments(action=action, args={}); request['args'] = args
                with self.subTest(action=action, args=args), self.assertRaises(Unavailable): a.validate_action_arguments(request)
        for screen in ('battlescape', 'abort_mission', 'main_menu'):
            for action in set(ACTIONS) - a.STATE_ACTIONS[screen]:
                obj = abort_state() if screen == 'abort_mission' else state(screen=screen)
                obj['actions'] = [dict(id=action, label='Abort mission', label_source=a.label_source(screen, action), argument_kind='none')]
                with self.assertRaises(Unavailable): self.checked(obj)
        obj = state(); obj['actions'].append(dict(id='open_abort_mission', label='Abort mission', label_source='visible_control_semantics', argument_kind='none'))
        self.checked(obj)

    def test_fresh_single_use_grant_no_automatic_confirmation(self):
        for action in ACTIONS[1:]:
            peer = AbortFixture(); self.addCleanup(peer.close)
            old = peer.client.observe(); fresh = peer.client.observe()
            with self.assertRaises(Unavailable): peer.client.execute(**arguments(old, action=action))
            self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
            request = arguments(fresh, action=action)
            receipt = peer.client.execute(**request)
            self.assertEqual(receipt['status'], 'dispatched')
            self.assertEqual(peer.client.execute(**request), receipt)
            self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests), 1)
            self.assertIsNone(peer.client.capability)


if __name__ == '__main__':
    unittest.main()
