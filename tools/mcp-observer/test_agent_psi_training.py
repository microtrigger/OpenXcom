# SPDX-License-Identifier: GPL-3.0-or-later
"""Current psi selector captions and explicit close; synthetic authority tests."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

ACTIONS = {'close_psi_training'}

class PsiTrainingTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def fixture(self, **kwargs):
        peer = NativeFixture(screen='psi_training', **kwargs)
        self.addCleanup(peer.close)
        return peer

    def test_exact_popup_shape_and_nullable_presented_text(self):
        obj = state(screen='psi_training')
        self.checked(obj)
        for field in ('title',):
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

    def test_read_only_base_captions_bound_and_no_hidden_rows(self):
        obj = state(screen='psi_training')
        for labels in ([], ['Same'], ['Same'] * 8, ['', 'Visible']):
            candidate = copy.deepcopy(obj); candidate['visible']['bases'] = labels
            self.checked(candidate)
        for labels in (None, {}, ['x'] * 9, [None], [True], [{'id': 'b0', 'label': 'Base'}], ['x' * 1025]):
            candidate = copy.deepcopy(obj); candidate['visible']['bases'] = labels
            with self.assertRaises(Unavailable): self.checked(candidate)
        for action in ('select_psi_base', 'choose_visible_row', 'allocate_psi_training'):
            candidate = copy.deepcopy(obj)
            candidate['actions'] = [dict(id=action, label='Same', label_source='visible_text', argument_kind='none')]
            with self.assertRaises(Unavailable): self.checked(candidate)

    def test_explicit_choices_require_own_shown_button_and_exact_label(self):
        for action, button in a.INTERCEPTION_BUTTON_ACTIONS['psi_training'].items():
            obj = state(screen='psi_training')
            obj['visible']['buttons'] = [b for b in obj['visible']['buttons'] if b['id'] != button]
            with self.assertRaises(Unavailable): self.checked(obj)
            obj['actions'] = [x for x in obj['actions'] if x['id'] != action]
            if not obj['actions']: obj['capability'] = None
            self.checked(obj)
            obj = state(screen='psi_training')
            next(x for x in obj['actions'] if x['id'] == action)['label'] = 'Cancel'
            with self.assertRaises(Unavailable): self.checked(obj)
        obj = state(screen='psi_training'); obj['visible']['buttons'].append(dict(id='big_ok',label='OK'))
        with self.assertRaises(Unavailable): self.checked(obj)

    def test_no_payload_or_generic_cancel(self):
        for action in ACTIONS:
            self.assertTrue(a.canonical_action_request(arguments(action=action)).endswith(action.encode()))
            for payload in ({'craft_id': 1}, {'confirm': True}, {'mode': 'return'}, {'x': 0, 'y': 0}):
                with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action=action, args=payload))
        for action in ('cancel_interception', 'cancel_interception_confirmation', 'choose_dogfight_mode'):
            obj = state(screen='psi_training')
            obj['actions'] = [dict(id=action, label='Cancel', label_source='visible_text', argument_kind='none')]
            with self.assertRaises(Unavailable): self.checked(obj)

    def test_neighbour_states_cannot_authorize_popup_choices(self):
        for screen in a.STATE_ACTIONS:
            if screen == 'psi_training': continue
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
            peer = self.fixture(transitions={('psi_training',action):'geoscape'})
            request = arguments(peer.client.observe(), action=action)
            receipt = peer.client.execute(**request)
            self.assertEqual(receipt['status'], 'dispatched')
            self.assertEqual(peer.client.execute(**request), receipt)
            self.assertEqual(peer.client.observe()['state'], 'geoscape')
            self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)
            other = 'close_psi_training_5_seconds'
            with self.assertRaises(Unavailable): peer.client.execute(**dict(request,action=other))
            self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)

    def test_unsupported_transition_and_absent_choice_never_auto_acknowledge(self):
        peer=self.fixture(transitions={('psi_training','close_psi_training'):'unsupported'})
        request=arguments(peer.client.observe(),action='close_psi_training')
        peer.client.execute(**request)
        observed=peer.client.observe()
        self.assertEqual(observed['state'],'unsupported')
        self.assertEqual(observed['actions'],[])
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(observed,action='close_psi_training_5_seconds'))
        self.assertEqual(sum(packet.startswith(b'E ') for packet in peer.requests),1)
        obj=state(screen='psi_training');obj['visible']['buttons']=[];obj['visible']['rows']=[];obj['actions']=[];obj['capability']=None
        self.checked(obj)

    def test_basechoice_exact_rows_and_empty_duplicate_captions(self):
        for labels in ([''],['Same','Same'],['Shown']*8):
            obj=state(screen='psi_training');obj['visible']['rows']=[dict(handle='r'+str(i),cells=[x]) for i,x in enumerate(labels)]
            self.checked(obj)
        for rows in (None,{},[dict(handle='r1',cells=['x'])],[dict(handle='r0',cells=['x']),dict(handle='r0',cells=['y'])],
                     [dict(handle='r0',cells=[None])],[dict(handle='r0',cells=['a','b'])],[dict(handle='r0',cells=['x'*1025])],
                     [dict(handle='r'+str(i),cells=['x']) for i in range(9)]):
            obj=state(screen='psi_training');obj['visible']['rows']=rows
            with self.subTest(rows=rows),self.assertRaises(Unavailable):self.checked(obj)
        for field in ('index','base_id','selected','total','capacity'):
            obj=state(screen='psi_training');obj['visible']['rows'][0][field]=1
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_basechoice_action_requires_rows_and_fixed_semantics(self):
        obj=state(screen='psi_training');obj['visible']['rows']=[]
        with self.assertRaises(Unavailable):self.checked(obj)
        obj['actions']=[x for x in obj['actions'] if x['id']!='open_psi_training_base'];self.checked(obj)
        # Readonly captions remain even when every action hitbox is unavailable.
        self.assertEqual(obj['visible']['bases'],['Shown base'])
        obj=state(screen='psi_training');obj['actions']=[x for x in obj['actions'] if x['id']!='open_psi_training_base']
        with self.assertRaises(Unavailable):self.checked(obj)
        for key,value in [('label','Shown base'),('label_source','visible_text'),('argument_kind','none')]:
            obj=state(screen='psi_training');next(x for x in obj['actions'] if x['id']=='open_psi_training_base')[key]=value
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_basechoice_strict_args_and_foreign_current_handle(self):
        for payload in ({},{'row':'r0','base_id':0},{'row':0},{'row':'b0'},{'row':'r999'}):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='open_psi_training_base',args=payload))
        peer=self.fixture();obj=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='open_psi_training_base',args={'row':'r1'}))
        self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))

    def test_basechoice_stale_grant_and_hidden_row_never_send(self):
        peer=self.fixture();old=peer.client.observe();peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='open_psi_training_base',args={'row':'r0'}))
        self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
        def hidden(op,obj):
            if op=='R':
                obj['visible']['rows']=[];obj['actions']=[x for x in obj['actions'] if x['id']!='open_psi_training_base']
            return obj
        peer=self.fixture(transform=hidden);obj=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='open_psi_training_base',args={'row':'r0'}))
        self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))

    def test_basechoice_one_push_receipt_duplicate_no_replay(self):
        peer=self.fixture(transitions={('psi_training','open_psi_training_base'):'allocate_psi_training'})
        request=arguments(peer.client.observe(),action='open_psi_training_base',args={'row':'r0'})
        receipt=peer.client.execute(**request);self.assertEqual(receipt['status'],'dispatched')
        self.assertEqual(peer.client.execute(**request),receipt)
        after=peer.client.observe();self.assertEqual(after['state'],'allocate_psi_training')
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(after,action='open_psi_training_base',args={'row':'r0'},command_id='f'*32))
        self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)

    def test_basechoice_unknown_receipt_closes_without_replay(self):
        peer=self.fixture(transform=lambda op,obj:b'{"private":"SECRET"}' if op=='E' else obj)
        request=arguments(peer.client.observe(),action='open_psi_training_base',args={'row':'r0'})
        with self.assertRaises(a.ActionOutcomeUnknown) as caught:peer.client.execute(**request)
        self.assertNotIn('SECRET',str(caught.exception.public_error()));self.assertEqual(peer.client.state,'CLOSED')
        with self.assertRaises(Unavailable):peer.client.execute(**request)
        self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)

    def test_basechoice_other_screen_cannot_authorize(self):
        for screen in a.STATE_ACTIONS:
            if screen=='psi_training':continue
            obj=state(screen=screen);obj['actions']=[dict(id='open_psi_training_base',label='Open psi training base',label_source='visible_control_semantics',argument_kind='visible_row')]
            with self.subTest(screen=screen),self.assertRaises(Unavailable):self.checked(obj)

if __name__ == '__main__': unittest.main()
