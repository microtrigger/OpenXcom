# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded visible-cell click and non-lift confirmation; no model authority."""
import copy
import unittest
from unittest.mock import patch
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, arguments, NativeFixture, CHALLENGE, EPOCH

class DismantleTests(unittest.TestCase):
    def check(self,o):return a.validate_state(o,CHALLENGE,EPOCH)
    def test_schema_closed_state_fields_and_labels(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/45')
        self.assertEqual(a.STATE_ACTIONS['dismantle_facility'],{'confirm_dismantle_facility','cancel_dismantle_facility'})
        for screen in ('basescape','dismantle_facility'):
            obj=state(screen=screen);self.check(obj)
            for key in obj['visible']:
                bad=copy.deepcopy(obj);del bad['visible'][key]
                with self.assertRaises(Unavailable):self.check(bad)
            for key in ('base_id','facility_id','is_lift','eligible','occupancy','refund','destroy_base'):
                bad=copy.deepcopy(obj);bad['visible'][key]=None
                with self.assertRaises(Unavailable):self.check(bad)
            bad=copy.deepcopy(obj);bad['schema']='openxcom-agent-control/33'
            with self.assertRaises(Unavailable):self.check(bad)
        for action,screen in [('click_base_facility_cell','basescape'),('confirm_dismantle_facility','dismantle_facility'),('cancel_dismantle_facility','dismantle_facility')]:
            for key,value in [('label','hidden'),('argument_kind','arbitrary'),('label_source','wrong')]:
                obj=state(screen=screen);next(x for x in obj['actions'] if x['id']==action)[key]=value
                with self.assertRaises(Unavailable):self.check(obj)
    def test_confirmation_safety_and_painted_names(self):
        obj=state(screen='dismantle_facility');obj['visible']['confirmation_allowed']=False
        with self.assertRaises(Unavailable):self.check(obj)
        obj['actions']=[x for x in obj['actions'] if x['id']=='cancel_dismantle_facility']
        with self.assertRaises(Unavailable):self.check(obj) # Even an unoffered OK button forbidden for lift.
        obj['visible']['buttons']=[x for x in obj['visible']['buttons'] if x['id']=='cancel'];self.check(obj)
        for key in ('title','facility'):
            for value in (None,''):
                bad=state(screen='dismantle_facility');bad['visible'][key]=value
                with self.assertRaises(Unavailable):self.check(bad)
                bad['actions']=[x for x in bad['actions'] if x['id']=='cancel_dismantle_facility']
                with self.assertRaises(Unavailable):self.check(bad)
                bad['visible']['buttons']=[x for x in bad['visible']['buttons'] if x['id']=='cancel'];self.check(bad)
        for value in (0,1,'true',None):
            bad=state(screen='dismantle_facility');bad['visible']['confirmation_allowed']=value
            with self.assertRaises(Unavailable):self.check(bad)
        for buttons in ([],[dict(id='cancel',label='Cancel'),dict(id='ok',label='OK')],[dict(id='ok',label='')]):
            bad=state(screen='dismantle_facility');bad['visible']['buttons']=buttons
            with self.assertRaises(Unavailable):self.check(bad)
    def test_exact_arguments_canonical_and_state_matrix(self):
        for action in ('confirm_dismantle_facility','cancel_dismantle_facility'):
            a.validate_action_arguments(arguments(action=action,args={}))
            for bad in ({'base_id':'x'},{'facility':1},{'approved':True},{'x':0,'y':0}):
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=bad))
        self.assertTrue(a.canonical_action_request(arguments(action='click_base_facility_cell',args=dict(x=5,y=5))).endswith(b' click_base_facility_cell 5 5'))
        for v in (True,False,-1,6,'0',0.0,None):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='click_base_facility_cell',args=dict(x=v,y=0)))
        new={'click_base_facility_cell','confirm_dismantle_facility','cancel_dismantle_facility'}
        for screen in a.STATE_ACTIONS:
            for action in new-a.STATE_ACTIONS[screen]:
                o=state(screen=screen);o['actions']=[dict(id=action,label='Shown',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action])]
                with self.assertRaises(Unavailable):self.check(o)
    def test_grid_absent_empty_and_current_membership(self):
        for grid in (None,dict(columns=6,rows=6,origin_x=0,origin_y=0,cell_size=32,cells=[])):
            o=state(screen='basescape');o['visible']['grid']=grid
            with self.assertRaises(Unavailable):self.check(o)
            o['actions']=[x for x in o['actions'] if x['id'] not in ('click_base_facility_cell','right_click_base_facility_cell')];self.check(o)
        for mode in ('stale','unlisted','mutated','changed_base'):
            peer=NativeFixture(screen='basescape')
            try:
                o=peer.client.observe();args=dict(x=0,y=0)
                if mode in ('stale','changed_base'):peer.client.observe()
                if mode in ('unlisted','mutated'):args=dict(x=5,y=5)
                if mode=='mutated':o['visible']['grid']['cells'].append(args.copy())
                with self.assertRaises(Unavailable):peer.client.execute(**arguments(o,action='click_base_facility_cell',args=args))
                self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
            finally:peer.close()
    def test_snapshot_never_authorizes_click_or_confirmation(self):
        from test_player_bases import Peer,sample
        facility=sample('facility');facility.update(x=5,y=5)
        peer=Peer([facility])
        try:
            peer.client.read_player_bases();self.assertIsNone(peer.client.capability)
            with patch('test_player_bases.state',side_effect=lambda:state(screen='basescape')):o=peer.client.observe()
            peer.client.read_player_bases();self.assertEqual(peer.client.facility_cells,{(0,0),(1,0)})
            for action,args in [('click_base_facility_cell',dict(x=5,y=5)),('confirm_dismantle_facility',{})]:
                with self.assertRaises(Unavailable):peer.client.execute(**arguments(o,action=action,args=args))
            self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        finally:peer.close()
    def test_click_confirmation_cancel_duplicate_and_lift_zero_send(self):
        for action in ('confirm_dismantle_facility','cancel_dismantle_facility'):
            peer=NativeFixture(screen='basescape',transitions={('basescape','click_base_facility_cell'):'dismantle_facility',('dismantle_facility',action):'basescape'})
            try:
                o=peer.client.observe();r=peer.client.execute(**arguments(o,action='click_base_facility_cell',args=dict(x=0,y=0)));self.assertEqual(r['status'],'dispatched')
                o=peer.client.observe();self.assertEqual(o['state'],'dismantle_facility')
                args=arguments(o,action=action,args={},command_id='e'*32);r=peer.client.execute(**args)
                self.assertEqual(r['status'],'dispatched');self.assertNotIn('success',r)
                self.assertEqual(peer.client.execute(**args),r);self.assertTrue(peer.requests[-1].startswith(b'S '))
                self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),2)
            finally:peer.close()
        def lift(op,o):
            if op=='R':
                o['visible']['confirmation_allowed']=False;o['visible']['buttons']=o['visible']['buttons'][1:];o['actions']=o['actions'][1:]
            return o
        peer=NativeFixture(screen='dismantle_facility',transform=lift)
        try:
            o=peer.client.observe()
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(o,action='confirm_dismantle_facility',args={}))
            self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        finally:peer.close()
if __name__=='__main__':unittest.main()
