# SPDX-License-Identifier: GPL-3.0-or-later
"""Current tracking-lost caption and explicit acknowledgement; synthetic authority tests."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

ACTIONS = {'acknowledge_ufo_lost'}

class UfoLostTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def fixture(self, **kwargs):
        peer = NativeFixture(screen='ufo_lost', **kwargs)
        self.addCleanup(peer.close)
        return peer

    def test_exact_popup_shape_and_nullable_presented_text(self):
        obj = state(screen='ufo_lost')
        self.checked(obj)
        for field in ('message',):
            for value in (None, '', 'INTERCEPTOR-1: Not enough ammunition\nAt base'):
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
        for action, button in a.INTERCEPTION_BUTTON_ACTIONS['ufo_lost'].items():
            obj = state(screen='ufo_lost')
            obj['visible']['buttons'] = [b for b in obj['visible']['buttons'] if b['id'] != button]
            with self.assertRaises(Unavailable): self.checked(obj)
            obj['actions'] = [x for x in obj['actions'] if x['id'] != action]
            if not obj['actions']: obj['capability'] = None
            self.checked(obj)
            obj = state(screen='ufo_lost')
            next(x for x in obj['actions'] if x['id'] == action)['label'] = 'Cancel'
            with self.assertRaises(Unavailable): self.checked(obj)
        obj = state(screen='ufo_lost'); obj['visible']['buttons'].append(dict(id='big_ok',label='OK'))
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_no_payload_or_generic_cancel(self):
        for action in ACTIONS:
            self.assertTrue(a.canonical_action_request(arguments(action=action)).endswith(action.encode()))
            for payload in ({'craft_id': 1}, {'confirm': True}, {'mode': 'return'}, {'x': 0, 'y': 0}):
                with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action=action, args=payload))
        for action in ('cancel_interception', 'cancel_interception_confirmation', 'choose_dogfight_mode'):
            obj = state(screen='ufo_lost')
            obj['actions'] = [dict(id=action, label='Cancel', label_source='visible_text', argument_kind='none')]
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_neighbour_states_cannot_authorize_popup_choices(self):
        for screen in a.STATE_ACTIONS:
            if screen == 'ufo_lost': continue
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
            peer = self.fixture(transitions={('ufo_lost',action):'geoscape'})
            request = arguments(peer.client.observe(), action=action)
            receipt = peer.client.execute(**request)
            self.assertEqual(receipt['status'], 'dispatched')
            self.assertEqual(peer.client.execute(**request), receipt)
            self.assertEqual(peer.client.observe()['state'], 'geoscape')
            self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)
            other = 'acknowledge_ufo_lost_5_seconds'
            with self.assertRaises(Unavailable): peer.client.execute(**dict(request,action=other))
            self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)

    def test_unsupported_transition_and_absent_choice_never_auto_acknowledge(self):
        peer=self.fixture(transitions={('ufo_lost','acknowledge_ufo_lost'):'unsupported'})
        request=arguments(peer.client.observe(),action='acknowledge_ufo_lost')
        peer.client.execute(**request)
        observed=peer.client.observe()
        self.assertEqual(observed['state'],'unsupported')
        self.assertEqual(observed['actions'],[])
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(observed,action='acknowledge_ufo_lost_5_seconds'))
        self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)
        obj=state(screen='ufo_lost');obj['visible']['buttons']=[];obj['actions']=[];obj['capability']=None
        self.checked(obj)

if __name__ == '__main__': unittest.main()
