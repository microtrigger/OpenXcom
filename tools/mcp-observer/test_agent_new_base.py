# SPDX-License-Identifier: GPL-3.0-or-later
"""Additional-base UI flow, not transactional success or refund authority."""
import copy
import unittest
from unittest.mock import patch
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,arguments,NativeFixture,CHALLENGE,EPOCH
NEW={'open_new_base','cancel_new_base','confirm_new_base','cancel_new_base_confirmation','place_access_lift_cell'}
def additional_site(obj):
    obj['visible']['buttons']=[dict(id='cancel',label='Cancel')]
    obj['actions'].append(dict(id='cancel_new_base',label='Cancel',label_source='visible_text',argument_kind='none'))
    return obj
class NewBaseTests(unittest.TestCase):
    def check(self,o):return a.validate_state(o,CHALLENGE,EPOCH)
    def test_closed_fields_old_schema_and_first_base_preserved(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/43')
        for screen in ('build_base','confirm_new_base','place_lift'):
            obj=state(screen=screen);self.check(obj)
            for key in obj['visible']:
                bad=copy.deepcopy(obj);del bad['visible'][key]
                with self.assertRaises(Unavailable):self.check(bad)
            for key in ('base_id','cost_value','funds','refund','first_base','facility_id','lift_type'):
                bad=copy.deepcopy(obj);bad['visible'][key]=None
                with self.assertRaises(Unavailable):self.check(bad)
            obj['schema']='openxcom-agent-control/34'
            with self.assertRaises(Unavailable):self.check(obj)
        first=state(screen='build_base');self.assertEqual(first['visible']['buttons'],[])
        self.assertEqual([x['id'] for x in first['actions']],['place_base_site'])
        second=additional_site(copy.deepcopy(first));self.check(second)
        first['actions']=second['actions']
        with self.assertRaises(Unavailable):self.check(first)
    def test_exact_labels_and_state_matrix(self):
        screens={'open_new_base':'basescape','cancel_new_base':'build_base','confirm_new_base':'confirm_new_base','cancel_new_base_confirmation':'confirm_new_base','place_access_lift_cell':'place_lift'}
        for action,screen in screens.items():
            for key,value in [('label','invented'),('argument_kind','hidden'),('label_source','wrong')]:
                obj=state(screen=screen)
                if action=='cancel_new_base':additional_site(obj)
                next(x for x in obj['actions'] if x['id']==action)[key]=value
                with self.assertRaises(Unavailable):self.check(obj)
        for screen in a.STATE_ACTIONS:
            for action in NEW-a.STATE_ACTIONS[screen]:
                o=state(screen=screen);o['actions']=[dict(id=action,label='Shown',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action])]
                with self.assertRaises(Unavailable):self.check(o)
    def test_cost_confirmation_text_and_button_authority(self):
        for field in ('cost','area'):
            for value in (None,''):
                o=state(screen='confirm_new_base');o['visible'][field]=value
                with self.assertRaises(Unavailable):self.check(o)
                o['actions']=[x for x in o['actions'] if x['id']=='cancel_new_base_confirmation'];self.check(o)
        for buttons in ([],[dict(id='cancel',label='Cancel'),dict(id='ok',label='OK')],[dict(id='ok',label='')],[dict(id='ok',label='OK',refund=True)]):
            o=state(screen='confirm_new_base');o['visible']['buttons']=buttons
            with self.assertRaises(Unavailable):self.check(o)
        o=state(screen='place_lift');o['visible']['buttons']=[dict(id='cancel',label='Cancel')]
        with self.assertRaises(Unavailable):self.check(o)
    def test_exact_args_and_null_grid_no_grant(self):
        for action in NEW-{'place_access_lift_cell'}:
            a.validate_action_arguments(arguments(action=action,args={}))
            for args in ({'base_id':'b1'},{'confirmed':True},{'x':0,'y':0}):
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=args))
        for value in (-1,6,True,False,0.0,'0',None):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='place_access_lift_cell',args=dict(x=value,y=0)))
        self.assertTrue(a.canonical_action_request(arguments(action='place_access_lift_cell',args=dict(x=5,y=5))).endswith(b' place_access_lift_cell 5 5'))
        for grid in (None,dict(columns=6,rows=6,origin_x=0,origin_y=0,cell_size=32,cells=[])):
            o=state(screen='place_lift');o['visible']['grid']=grid
            with self.assertRaises(Unavailable):self.check(o)
            o['actions']=[];o['capability']=None;self.check(o)
    def test_stale_unlisted_lift_and_no_snapshot_grant(self):
        for mode in ('stale','unlisted','mutated'):
            peer=NativeFixture(screen='place_lift')
            try:
                o=peer.client.observe();args=dict(x=0,y=0)
                if mode=='stale':peer.client.observe()
                else:args=dict(x=5,y=5)
                if mode=='mutated':o['visible']['grid']['cells'].append(args.copy())
                with self.assertRaises(Unavailable):peer.client.execute(**arguments(o,action='place_access_lift_cell',args=args))
                self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
            finally:peer.close()
        from test_player_bases import Peer,sample
        peer=Peer([sample('base')])
        try:
            peer.client.read_player_bases();self.assertIsNone(peer.client.capability)
            with patch('test_player_bases.state',side_effect=lambda:state(screen='place_lift')):o=peer.client.observe()
            peer.client.read_player_bases();self.assertEqual(peer.client.facility_cells,{(0,0),(1,0)})
            for action,args in [('place_access_lift_cell',dict(x=5,y=5)),('confirm_new_base',{})]:
                with self.assertRaises(Unavailable):peer.client.execute(**arguments(o,action=action,args=args))
            self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        finally:peer.close()
    def test_full_separate_handler_flow_duplicates_and_cancel(self):
        transitions={('basescape','open_new_base'):'build_base',('build_base','place_base_site'):'confirm_new_base',('confirm_new_base','confirm_new_base'):'base_name',('base_name','confirm_base_name'):'place_lift',('place_lift','place_access_lift_cell'):'basescape'}
        class AdditionalFixture(NativeFixture):
            def observation(self,challenge,capability):
                o=super().observation(challenge,capability)
                return additional_site(o) if o['state']=='build_base' else o
        peer=AdditionalFixture(screen='basescape',transitions=transitions)
        try:
            actions=[('open_new_base',{}),('place_base_site',dict(x=64,y=80)),('confirm_new_base',{}),('replace_base_name',dict(text='Test Beta')),('confirm_base_name',{}),('place_access_lift_cell',dict(x=0,y=0))]
            for i,(action,args) in enumerate(actions):
                o=peer.client.observe();request=arguments(o,action=action,args=args,command_id=f'{i+1:032x}')
                receipt=peer.client.execute(**request);self.assertEqual(receipt['status'],'dispatched');self.assertNotIn('success',receipt);self.assertNotIn('refund',receipt)
                self.assertEqual(peer.client.execute(**request),receipt);self.assertTrue(peer.requests[-1].startswith(b'S '))
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),6)
        finally:peer.close()
        for screen,action in [('build_base','cancel_new_base'),('confirm_new_base','cancel_new_base_confirmation')]:
            peer=AdditionalFixture(screen=screen)
            try:
                o=peer.client.observe();self.assertEqual(peer.client.execute(**arguments(o,action=action,args={}))['status'],'dispatched')
            finally:peer.close()
if __name__=='__main__':unittest.main()
