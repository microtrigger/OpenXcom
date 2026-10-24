# SPDX-License-Identifier: GPL-3.0-or-later
"""Current commendation title and read-only viewport and explicit acknowledgement; synthetic authority tests."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

ACTIONS = {'close_commendations'}

class CommendationTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def fixture(self, **kwargs):
        peer = NativeFixture(screen='commendations', **kwargs)
        self.addCleanup(peer.close)
        return peer

    def test_exact_popup_shape_and_nullable_presented_text(self):
        obj = state(screen='commendations')
        self.checked(obj)
        for field in ('title',):
            for value in (None, '', 'Agents report a base\nShown location'):
                candidate = copy.deepcopy(obj); candidate['visible'][field] = value
                self.checked(candidate)
            for value in (True, {}, [], 'x' * 1025):
                candidate = copy.deepcopy(obj); candidate['visible'][field] = value
                with self.assertRaises(Unavailable): self.checked(candidate)
        for field in ('soldier_id', 'criteria', 'diary', 'decoration_level', 'award', 'medal_id', 'success'):
            candidate = copy.deepcopy(obj); candidate['visible'][field] = 1
            with self.assertRaises(Unavailable): self.checked(candidate)
        for field in obj['visible']:
            candidate = copy.deepcopy(obj); del candidate['visible'][field]
            with self.assertRaises(Unavailable): self.checked(candidate)

    def test_readonly_cells_preserve_exact_strings_empty_null_and_duplicates(self):
        obj = state(screen='commendations')
        obj['visible']['rows'] = [dict(cells=['Medal', '']), dict(cells=['   Name', 'Decoration']),
                                  dict(cells=['   Name', None]), dict(cells=['long' * 200, 'a\nb'])]
        self.checked(obj)
        self.assertEqual(obj['visible']['rows'][1]['cells'][0], '   Name')
        for field in ('handle', 'selected', 'logicalIndex', 'index', 'soldier_id', 'total'):
            candidate = copy.deepcopy(obj); candidate['visible']['rows'][0][field] = 0
            with self.subTest(field=field), self.assertRaises(Unavailable): self.checked(candidate)
        for cells in ([], ['one'], ['one','two','three'], [True,''], [{},''], ['x'*1025,'']):
            candidate = copy.deepcopy(obj); candidate['visible']['rows'] = [dict(cells=cells)]
            with self.assertRaises(Unavailable): self.checked(candidate)

    def test_readonly_viewport_resource_bounds_and_no_row_action(self):
        obj = state(screen='commendations'); obj['visible']['rows'] = [dict(cells=['','']) for _ in range(128)]
        self.checked(obj)
        obj['visible']['rows'].append(dict(cells=['','']))
        with self.assertRaises(Unavailable): self.checked(obj)
        for rows in (None, {}, True):
            obj = state(screen='commendations'); obj['visible']['rows'] = rows
            with self.assertRaises(Unavailable): self.checked(obj)
        for action in ('select_commendation','scroll_visible_list','toggle_psi_training'):
            obj = state(screen='commendations'); obj['actions'][0]['id'] = action
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_explicit_choices_require_own_shown_button_and_exact_label(self):
        for action, button in a.INTERCEPTION_BUTTON_ACTIONS['commendations'].items():
            obj = state(screen='commendations')
            obj['visible']['buttons'] = [b for b in obj['visible']['buttons'] if b['id'] != button]
            with self.assertRaises(Unavailable): self.checked(obj)
            obj['actions'] = [x for x in obj['actions'] if x['id'] != action]
            if not obj['actions']: obj['capability'] = None
            self.checked(obj)
            obj = state(screen='commendations')
            next(x for x in obj['actions'] if x['id'] == action)['label'] = 'Cancel'
            with self.assertRaises(Unavailable): self.checked(obj)
        obj = state(screen='commendations'); obj['visible']['buttons'].append(dict(id='big_ok',label='OK'))
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_no_payload_or_generic_cancel(self):
        for action in ACTIONS:
            self.assertTrue(a.canonical_action_request(arguments(action=action)).endswith(action.encode()))
            for payload in ({'craft_id': 1}, {'confirm': True}, {'mode': 'return'}, {'x': 0, 'y': 0}):
                with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action=action, args=payload))
        for action in ('cancel_interception', 'cancel_interception_confirmation', 'choose_dogfight_mode'):
            obj = state(screen='commendations')
            obj['actions'] = [dict(id=action, label='Cancel', label_source='visible_text', argument_kind='none')]
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_neighbour_states_cannot_authorize_popup_choices(self):
        for screen in a.STATE_ACTIONS:
            if screen == 'commendations': continue
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
            peer = self.fixture(transitions={('commendations',action):'geoscape'})
            request = arguments(peer.client.observe(), action=action)
            receipt = peer.client.execute(**request)
            self.assertEqual(receipt['status'], 'dispatched')
            self.assertEqual(peer.client.execute(**request), receipt)
            self.assertEqual(peer.client.observe()['state'], 'geoscape')
            self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)
            other = 'close_commendations_5_seconds'
            with self.assertRaises(Unavailable): peer.client.execute(**dict(request,action=other))
            self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)

    def test_unsupported_transition_and_absent_choice_never_auto_acknowledge(self):
        peer=self.fixture(transitions={('commendations','close_commendations'):'unsupported'})
        request=arguments(peer.client.observe(),action='close_commendations')
        peer.client.execute(**request)
        observed=peer.client.observe()
        self.assertEqual(observed['state'],'unsupported')
        self.assertEqual(observed['actions'],[])
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(observed,action='close_commendations_5_seconds'))
        self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)
        obj=state(screen='commendations');obj['visible']['buttons']=[];obj['actions']=[];obj['capability']=None
        self.checked(obj)

if __name__ == '__main__': unittest.main()
