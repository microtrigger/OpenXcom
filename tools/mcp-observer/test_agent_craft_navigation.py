# SPDX-License-Identifier: GPL-3.0-or-later
"""Narrow craft window Cancel; synthetic authority tests, no model or navigation claims."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

ACTIONS = {'open_craft_destination', 'cancel_craft_navigation'}

class RedirectFixture(NativeFixture):
    def observation(self, challenge, capability):
        obj=super().observation(challenge, capability)
        ids={'open_craft_destination':'target','return_craft_to_base':'base','patrol_craft_current_position':'patrol','go_to_craft_last_known_position':'last_known'}
        obj['visible']['redirect']='REDIRECT CRAFT'
        obj['visible']['buttons']=[dict(id=b,label=b.title()) for b in ids.values()]
        obj['actions']=[dict(id=x,label=b.title(),label_source='visible_text',argument_kind='none') for x,b in ids.items()]
        return obj

class CraftNavigationTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def fixture(self, **kwargs):
        peer = NativeFixture(screen='geoscape_craft_navigation', **kwargs)
        self.addCleanup(peer.close)
        return peer

    def test_exact_popup_shape_and_nullable_presented_text(self):
        obj = state(screen='geoscape_craft_navigation')
        self.checked(obj)
        for field in ('title', 'status'):
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
        for action, button in {x:a.INTERCEPTION_BUTTON_ACTIONS['geoscape_craft_navigation'][x] for x in ACTIONS}.items():
            obj = state(screen='geoscape_craft_navigation')
            obj['visible']['buttons'] = [b for b in obj['visible']['buttons'] if b['id'] != button]
            with self.assertRaises(Unavailable): self.checked(obj)
            obj['actions'] = [x for x in obj['actions'] if x['id'] != action]
            if not obj['actions']: obj['capability'] = None
            self.checked(obj)
            obj = state(screen='geoscape_craft_navigation')
            next(x for x in obj['actions'] if x['id'] == action)['label'] = 'Unexpected label'
            with self.assertRaises(Unavailable): self.checked(obj)
        obj = state(screen='geoscape_craft_navigation'); obj['visible']['buttons'].append(dict(id='unsupported',label='UNSUPPORTED'))
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_no_payload_or_generic_cancel(self):
        for action in ACTIONS:
            self.assertTrue(a.canonical_action_request(arguments(action=action)).endswith(action.encode()))
            for payload in ({'craft_id': 1}, {'confirm': True}, {'mode': 'return'}, {'x': 0, 'y': 0}):
                with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action=action, args=payload))
        for action in ('cancel_interception', 'cancel_interception_confirmation', 'choose_dogfight_mode'):
            obj = state(screen='geoscape_craft_navigation')
            obj['actions'] = [dict(id=action, label='Cancel', label_source='visible_text', argument_kind='none')]
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_neighbour_states_cannot_authorize_popup_choices(self):
        for screen in a.STATE_ACTIONS:
            if screen == 'geoscape_craft_navigation': continue
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
            peer = self.fixture(transitions={('geoscape_craft_navigation',action):'geoscape'})
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
        peer=self.fixture(transitions={('geoscape_craft_navigation','cancel_craft_navigation'):'unsupported'})
        request=arguments(peer.client.observe(),action='cancel_craft_navigation')
        peer.client.execute(**request)
        observed=peer.client.observe()
        self.assertEqual(observed['state'],'unsupported')
        self.assertEqual(observed['actions'],[])
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(observed,action='cancel_craft_navigation'))
        self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)
        obj=state(screen='geoscape_craft_navigation');obj['visible']['buttons']=[];obj['actions']=[];obj['capability']=None
        self.checked(obj)


    def test_explicit_redirect_then_cancel_two_fresh_requests(self):
        peer = NativeFixture(screen='craft_patrol', transitions={
            ('craft_patrol','redirect_patrolling_craft'):'geoscape_craft_navigation',
            ('geoscape_craft_navigation','cancel_craft_navigation'):'geoscape'})
        self.addCleanup(peer.close)
        first = arguments(peer.client.observe(), action='redirect_patrolling_craft')
        peer.client.execute(**first)
        navigation = peer.client.observe()
        self.assertEqual(navigation['state'], 'geoscape_craft_navigation')
        with self.assertRaises(Unavailable):
            peer.client.execute(**dict(first, action='cancel_craft_navigation'))
        second = arguments(navigation, action='cancel_craft_navigation', command_id='b'*32)
        receipt = peer.client.execute(**second)
        self.assertEqual(peer.client.execute(**second), receipt)
        self.assertEqual(peer.client.observe()['state'], 'geoscape')
        self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests), 2)

    def test_redirect_mode_exact_choices_and_cross_shape_refusal(self):
        base=state(screen='geoscape_craft_navigation')
        old=copy.deepcopy(base);del old['visible']['redirect']
        with self.assertRaises(Unavailable):self.checked(old)
        redirect=copy.deepcopy(base);redirect['visible']['redirect']='REDIRECT CRAFT'
        ids={'open_craft_destination':'target','return_craft_to_base':'base','patrol_craft_current_position':'patrol','go_to_craft_last_known_position':'last_known'}
        redirect['visible']['buttons']=[dict(id=b,label=b.title()) for b in ids.values()]
        redirect['actions']=[dict(id=x,label=b.title(),label_source='visible_text',argument_kind='none') for x,b in ids.items()]
        self.checked(redirect)
        for action in ids:
            self.assertTrue(a.canonical_action_request(arguments(action=action)).endswith(action.encode()))
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args={'x':1}))
        mixed=copy.deepcopy(redirect);mixed['actions']+=base['actions'][:1]
        with self.assertRaises(Unavailable):self.checked(mixed)
        for value in (None,True,{},''):
            bad=copy.deepcopy(redirect);bad['visible']['redirect']=value
            with self.assertRaises(Unavailable):self.checked(bad)
        for action,b in ids.items():
            bad=copy.deepcopy(redirect);bad['visible']['buttons']=[x for x in bad['visible']['buttons'] if x['id']!=b]
            with self.assertRaises(Unavailable):self.checked(bad)
        # Old exact-key readers reject the newly synchronized shape rather than accepting a new mode.
        with self.assertRaises(Unavailable):a.exact(redirect['visible'],'title status buttons')

    def test_each_redirect_action_single_use_and_no_replay(self):
        for action in ('return_craft_to_base','patrol_craft_current_position','go_to_craft_last_known_position'):
            peer=RedirectFixture(screen='geoscape_craft_navigation')
            try:
                old=peer.client.observe();fresh=peer.client.observe()
                with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action=action))
                self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
                command=arguments(fresh,action=action);receipt=peer.client.execute(**command)
                self.assertEqual(receipt['status'],'dispatched')
                self.assertEqual(peer.client.execute(**command),receipt)
                self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)
            finally:peer.close()
            null=NativeFixture(screen='geoscape_craft_navigation')
            try:
                with self.assertRaises(Unavailable):null.client.execute(**arguments(null.client.observe(),action=action))
                self.assertFalse(any(p.startswith(b'E ') for p in null.requests))
            finally:null.close()

if __name__ == '__main__': unittest.main()
