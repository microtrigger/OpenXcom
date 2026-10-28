# SPDX-License-Identifier: GPL-3.0-or-later
"""Promotions title/headings and read-only viewport; explicit popup close, no row action."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

class PromotionsTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def readonly(self):
        obj=state(screen='promotions');obj['visible']['buttons']=[];obj['actions']=[];obj['capability']=None
        return obj

    def test_exact_readonly_shape(self):
        obj=self.readonly(); self.checked(obj)
        self.assertEqual(set(obj['visible']), {'title','headings','rows','buttons'})
        self.assertEqual(obj['actions'], []); self.assertIsNone(obj['capability'])
        self.assertEqual(a.STATE_ACTIONS['promotions'], frozenset(('close_promotions',)))
        for field in ('soldier_id','rank','base_id','promoted','total','selected','success'):
            bad=copy.deepcopy(obj);bad['visible'][field]=[]
            with self.subTest(field=field), self.assertRaises(Unavailable):self.checked(bad)

    def test_current_nullable_logical_captions(self):
        for value in (None,'','   Name','long'*200,'line1\nline2','é'*512):
            obj=state(screen='promotions');obj['visible']=dict(title=value,headings=[value]*3,rows=[],buttons=obj['visible']['buttons'])
            self.checked(obj)
        for value in (True,1,{},[],'x'*1025,'é'*513):
            obj=state(screen='promotions');obj['visible']['title']=value
            with self.assertRaises(Unavailable):self.checked(obj)
            obj=state(screen='promotions');obj['visible']['headings'][1]=value
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_headings_exact_three_cells(self):
        for headings in (None,{},True,[],['Name'],['Name','Rank'],['Name','Rank','Base','Extra']):
            obj=state(screen='promotions');obj['visible']['headings']=headings
            with self.assertRaises(Unavailable):self.checked(obj)
        for field in ('title','headings','rows'):
            obj=state(screen='promotions');del obj['visible'][field]
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_existing_semantic_actions_cannot_be_imported(self):
        for action in a.ACTIONS:
            if action in ('move_cursor','close_promotions'):continue # Existing independent global motion is not a popup capability.
            obj=state(screen='promotions')
            obj['actions']=[dict(id=action,label='Shown',label_source='visible_text',argument_kind=a.ARGUMENT_KINDS[action])]
            obj['capability']='1'*32
            with self.subTest(action=action),self.assertRaises(Unavailable):self.checked(obj)

    def test_no_new_close_or_selection_action(self):
        for action in ('acknowledge_promotions','select_promotion','scroll_visible_list'):
            self.assertNotIn(action,a.STATE_ACTIONS['promotions'])
            if action!='scroll_visible_list':
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action))

    def test_readonly_client_observation_never_sends_mutation(self):
        native=NativeFixture(screen='promotions');self.addCleanup(native.close)
        first=native.client.observe();second=native.client.observe()
        self.assertEqual(first['visible'],second['visible']);self.assertIsNotNone(second['capability'])
        self.assertFalse(any(p.startswith(b'E ') for p in native.requests))
        for action in ('close_commendations','close_psi_training','next_soldier'):
            with self.assertRaises(Unavailable):native.client.execute(**arguments(second,action=action))
        self.assertFalse(any(p.startswith(b'E ') for p in native.requests))

    def test_fake_capability_without_actions_refused(self):
        obj=self.readonly();obj['capability']='1'*32
        with self.assertRaises(Unavailable):self.checked(obj)

    def test_readonly_three_cells_exact_current_values(self):
        obj=self.readonly()
        obj['visible']['rows']=[dict(cells=['Same Name','Shown Rank','Shown Base']),dict(cells=['Same Name','',None]),dict(cells=['long'*200,'a\nb','é'*512])]
        self.checked(obj)
        for field in ('handle','index','logicalIndex','selected','soldier_id','base_id','total'):
            bad=copy.deepcopy(obj);bad['visible']['rows'][0][field]=0
            with self.subTest(field=field),self.assertRaises(Unavailable):self.checked(bad)
        for cells in ([],['one'],['one','two'],['one','two','three','four'],[True,'',''],[{},'',''],['x'*1025,'','']):
            bad=copy.deepcopy(obj);bad['visible']['rows']=[dict(cells=cells)]
            with self.assertRaises(Unavailable):self.checked(bad)
        self.assertEqual(obj['actions'],[]);self.assertIsNone(obj['capability'])

    def test_viewport_bounds_no_hidden_metadata_or_actions(self):
        obj=state(screen='promotions');obj['visible']['rows']=[dict(cells=['','','']) for _ in range(128)]
        self.checked(obj);obj['visible']['rows'].append(dict(cells=['','','']))
        with self.assertRaises(Unavailable):self.checked(obj)
        for rows in (None,True,{}):
            obj=state(screen='promotions');obj['visible']['rows']=rows
            with self.assertRaises(Unavailable):self.checked(obj)

    def test_close_requires_exact_current_button(self):
        obj=state(screen='promotions');self.checked(obj)
        for buttons in ([],[dict(id='ok',label='Wrong')],[dict(id='cancel',label='Ok')],[dict(id='ok',label='')],[dict(id='ok',label=None)]):
            bad=copy.deepcopy(obj);bad['visible']['buttons']=buttons
            with self.assertRaises(Unavailable):self.checked(bad)
        self.checked(self.readonly())
        for payload in ({'confirm':True},{'soldier_id':1},{'x':0}):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='close_promotions',args=payload))
        a.validate_action_arguments(arguments(action='close_promotions',args={}))

    def test_close_stale_grant_never_sends(self):
        peer=NativeFixture(screen='promotions');self.addCleanup(peer.close)
        old=peer.client.observe();peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='close_promotions'))
        self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))

    def test_close_single_dispatch_duplicate_only_queries(self):
        peer=NativeFixture(screen='promotions',transitions={('promotions','close_promotions'):'unsupported'});self.addCleanup(peer.close)
        request=arguments(peer.client.observe(),action='close_promotions')
        receipt=peer.client.execute(**request);self.assertEqual(receipt['status'],'dispatched')
        self.assertEqual(peer.client.execute(**request),receipt)
        after=peer.client.observe();self.assertEqual(after['state'],'unsupported')
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(after,action='close_promotions',command_id='f'*32))
        self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)

    def test_malformed_sent_receipt_unknown_stops_without_replay(self):
        peer=NativeFixture(screen='promotions',transform=lambda op,obj:b'{"private":"DO_NOT_EXPORT"}' if op=='E' else obj);self.addCleanup(peer.close)
        request=arguments(peer.client.observe(),action='close_promotions')
        with self.assertRaises(a.ActionOutcomeUnknown) as caught:peer.client.execute(**request)
        self.assertNotIn('DO_NOT_EXPORT',str(caught.exception.public_error()))
        self.assertEqual(peer.client.state,'CLOSED')
        with self.assertRaises(Unavailable):peer.client.execute(**request)
        self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)

    def test_neighbour_screens_cannot_authorize_close(self):
        for screen in a.STATE_ACTIONS:
            if screen=='promotions':continue
            obj=state(screen=screen);obj['actions']=[dict(id='close_promotions',label='Ok',label_source='visible_text',argument_kind='none')]
            with self.subTest(screen=screen),self.assertRaises(Unavailable):self.checked(obj)

    def test_catalog_adds_one_action_no_state(self):
        self.assertEqual(len(a.ACTIONS),261);self.assertEqual(len(a.STATE_ACTIONS),106)

if __name__=='__main__':unittest.main()
