# SPDX-License-Identifier: GPL-3.0-or-later
"""Destination picker authority tests; no full-game retarget claim."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

ACTIONS = {'cancel_craft_destination'}

class CraftDestinationTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def fixture(self, **kwargs):
        peer = NativeFixture(screen='craft_destination_selection', **kwargs)
        self.addCleanup(peer.close)
        return peer

    def test_exact_popup_shape_and_nullable_presented_text(self):
        obj = state(screen='craft_destination_selection')
        self.checked(obj)
        for field in ('title',):
            for value in (None, '', 'INTERCEPTOR-1: Low fuel\nReturning to base'):
                candidate = copy.deepcopy(obj); candidate['visible'][field] = value
                self.checked(candidate)
            for value in (True, {}, [], 'x' * 1025):
                candidate = copy.deepcopy(obj); candidate['visible'][field] = value
                with self.assertRaises(Unavailable): self.checked(candidate)
        for field in ('craft_id', 'fuel', 'fuel_limit', 'altitude', 'depth', 'world_coordinates', 'target_status', 'can_engage', 'success'):
            candidate = copy.deepcopy(obj); candidate['visible'][field] = 1
            with self.assertRaises(Unavailable): self.checked(candidate)
        for field in obj['visible']:
            candidate = copy.deepcopy(obj); del candidate['visible'][field]
            with self.assertRaises(Unavailable): self.checked(candidate)

    def test_explicit_choices_require_own_shown_button_and_exact_label(self):
        for action, button in a.INTERCEPTION_BUTTON_ACTIONS['craft_destination_selection'].items():
            obj = state(screen='craft_destination_selection')
            obj['visible']['buttons'] = [b for b in obj['visible']['buttons'] if b['id'] != button]
            with self.assertRaises(Unavailable): self.checked(obj)
            obj['actions'] = [x for x in obj['actions'] if x['id'] != action]
            if not obj['actions']: obj['capability'] = None
            self.checked(obj)
            obj = state(screen='craft_destination_selection')
            next(x for x in obj['actions'] if x['id'] == action)['label'] = 'Unexpected label'
            with self.assertRaises(Unavailable): self.checked(obj)
        obj = state(screen='craft_destination_selection'); obj['visible']['buttons'].append(dict(id='unsupported',label='UNSUPPORTED'))
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_no_payload_or_generic_cancel(self):
        for action in ACTIONS:
            self.assertTrue(a.canonical_action_request(arguments(action=action)).endswith(action.encode()))
            for payload in ({'craft_id': 1}, {'confirm': True}, {'mode': 'return'}, {'x': 0, 'y': 0}):
                with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action=action, args=payload))
        for action in ('cancel_interception', 'cancel_interception_confirmation', 'choose_dogfight_mode'):
            obj = state(screen='craft_destination_selection')
            obj['actions'] = [dict(id=action, label='Cancel', label_source='visible_text', argument_kind='none')]
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_neighbour_states_cannot_authorize_popup_choices(self):
        for screen in a.STATE_ACTIONS:
            if screen == 'craft_destination_selection': continue
            for action in ACTIONS:
                obj = state(screen=screen)
                obj['actions'] = [dict(id=action, label='Shown', label_source='visible_text', argument_kind='none')]
                with self.subTest(screen=screen, action=action), self.assertRaises(Unavailable): self.checked(obj)

    def test_observe_never_selects_and_stale_grant_never_sends(self):
        peer = self.fixture(); old = peer.client.observe(); fresh = peer.client.observe()
        self.assertFalse(any(packet.startswith(b'E ') for packet in peer.requests))
        for action in ACTIONS:
            with self.assertRaises(Unavailable): peer.client.execute(**arguments(old, action=action))
        self.assertFalse(any(packet.startswith(b'E ') for packet in peer.requests))

    def test_each_choice_explicit_single_use_dispatch(self):
        for action in ACTIONS:
            with self.subTest(action=action):
                peer = self.fixture(); obj = peer.client.observe()
                result = peer.client.execute(**arguments(obj, action=action))
                self.assertEqual(result['status'], 'dispatched')
                self.assertIsNone(peer.client.capability)
                self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests), 1)

    def test_duplicate_queries_receipt_without_replaying_or_auto_choice(self):
        for action in ACTIONS:
            peer = self.fixture(transitions={('craft_destination_selection',action):'geoscape'})
            request = arguments(peer.client.observe(), action=action)
            receipt = peer.client.execute(**request)
            self.assertEqual(receipt['status'], 'dispatched')
            self.assertEqual(peer.client.execute(**request), receipt)
            self.assertEqual(peer.client.observe()['state'], 'geoscape')
            self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)
            other = 'unsupported_redirect'
            with self.assertRaises(Unavailable): peer.client.execute(**dict(request,action=other))
            self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)

    def test_unsupported_transition_and_absent_choice_never_auto_acknowledge(self):
        peer=self.fixture(transitions={('craft_destination_selection','cancel_craft_destination'):'unsupported'})
        request=arguments(peer.client.observe(),action='cancel_craft_destination')
        peer.client.execute(**request)
        observed=peer.client.observe()
        self.assertEqual(observed['state'],'unsupported')
        self.assertEqual(observed['actions'],[])
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(observed,action='cancel_craft_destination'))
        self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)
        obj=state(screen='craft_destination_selection');obj['visible']['buttons']=[];obj['visible']['targets']=[];obj['actions']=[];obj['capability']=None
        self.checked(obj)


    def test_three_explicit_fresh_commands_and_receipt_only_duplicates(self):
        transitions = {
            ('craft_patrol', 'redirect_patrolling_craft'): 'geoscape_craft_navigation',
            ('geoscape_craft_navigation', 'open_craft_destination'): 'craft_destination_selection',
            ('craft_destination_selection', 'cancel_craft_destination'): 'geoscape'}
        peer = NativeFixture(screen='craft_patrol', transitions=transitions)
        self.addCleanup(peer.close)
        old = None
        for index, (action, expected) in enumerate((
                ('redirect_patrolling_craft', 'geoscape_craft_navigation'),
                ('open_craft_destination', 'craft_destination_selection'),
                ('cancel_craft_destination', 'geoscape'))):
            current = peer.client.observe()
            if old is not None:
                with self.assertRaises(Unavailable):
                    peer.client.execute(**dict(old, action=action, command_id='f'*32))
            request = arguments(current, action=action, command_id=str(index+1)*32)
            receipt = peer.client.execute(**request)
            self.assertEqual(receipt['status'], 'dispatched')
            self.assertEqual(peer.client.execute(**request), receipt)
            self.assertEqual(peer.client.observe()['state'], expected)
            old = request
        self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests), 3)

    def test_picker_never_advertises_destination_or_map_controls(self):
        self.assertEqual(a.STATE_ACTIONS['craft_destination_selection'], frozenset(ACTIONS | {'choose_visible_craft_destination'}))
        for action in ('open_craft_destination', 'select_destination', 'confirm_destination',
                       'create_waypoint', 'map_primary', 'return_to_base', 'patrol', 'cydonia'):
            obj = state(screen='craft_destination_selection')
            obj['actions'] = [dict(id=action, label='Shown', label_source='visible_text', argument_kind='none')]
            with self.assertRaises(Unavailable): self.checked(obj)

if __name__ == '__main__': unittest.main()
