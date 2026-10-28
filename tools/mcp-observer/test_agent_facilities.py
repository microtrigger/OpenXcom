# SPDX-License-Identifier: GPL-3.0-or-later
"""Build selection and current painted grid dispatch; no eligibility or success inference."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, arguments, NativeFixture, CHALLENGE, EPOCH
NEW=('open_build_facilities','select_build_facility','close_build_facilities','cancel_place_facility','place_facility_cell')
class FacilityTests(unittest.TestCase):
    def check(self,obj):return a.validate_state(obj,CHALLENGE,EPOCH)
    def test_closed_painted_fields_no_placement_authority(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/44')
        self.assertEqual(a.STATE_ACTIONS['place_facility'],{'place_facility_cell','cancel_place_facility'})
        for screen in ('build_facilities','place_facility'):
            obj=state(screen=screen);self.check(obj)
            for k in obj['visible']:
                bad=copy.deepcopy(obj);del bad['visible'][k]
                with self.assertRaises(Unavailable):self.check(bad)
            for k in ('rule_id','occupancy','eligible','target','construction','placed'):
                bad=copy.deepcopy(obj);bad['visible'][k]=None
                with self.assertRaises(Unavailable):self.check(bad)
        obj=state(screen='place_facility')
        for k in obj['visible']:
            if k!='buttons':obj['visible'][k]=None
        obj['actions']=[x for x in obj['actions'] if x['id']!='place_facility_cell']
        self.check(obj)
    def test_closed_state_and_semantic_descriptors(self):
        for screen in a.STATE_ACTIONS:
            for action in set(NEW)-a.STATE_ACTIONS[screen]:
                obj=state(screen=screen);obj['actions']=[dict(id=action,label='Shown',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action])]
                with self.assertRaises(Unavailable):self.check(obj)
        for k,v in [('label','Stored facility'),('label_source','visible_text'),('argument_kind','point')]:
            obj=state(screen='build_facilities');next(d for d in obj['actions'] if d['id']=='select_build_facility')[k]=v
            with self.assertRaises(Unavailable):self.check(obj)
    def test_required_shown_row_scroll_buttons(self):
        for key,value,action in [('rows',[],'select_build_facility'),('scroll',dict(up=False,down=False),'scroll_visible_list'),('buttons',[],'close_build_facilities')]:
            obj=state(screen='build_facilities');obj['visible'][key]=value
            with self.assertRaises(Unavailable):self.check(obj)
            obj['actions']=[x for x in obj['actions'] if x['id']!=action];self.check(obj)
        for value in (None,'',True):
            obj=state(screen='build_facilities');obj['visible']['rows'][0]['cells']=[value]
            with self.assertRaises(Unavailable):self.check(obj)
        obj=state(screen='place_facility');obj['visible']['buttons']=[]
        with self.assertRaises(Unavailable):self.check(obj)
        obj['actions']=[];obj['capability']=None;self.check(obj)
    def test_only_exact_row_and_noargs(self):
        for action in NEW:
            a.validate_action_arguments(arguments(action=action,args={'row':'r0'} if action=='select_build_facility' else {'x':0,'y':0} if action=='place_facility_cell' else {}))
            for bad in ({'rule_id':'LAB'},{'x':0,'y':0,'hidden':True},{'row':'r00'},{'row':'r32'}):
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=bad))
    def test_schema32_refused(self):
        obj=state(screen='build_facilities');obj['schema']='openxcom-agent-control/32'
        with self.assertRaises(Unavailable):self.check(obj)
    def test_stale_unlisted_malformed_zero_send(self):
        for mode in ('stale','unlisted','malformed'):
            peer=NativeFixture(screen='build_facilities')
            try:
                obj=peer.client.observe()
                if mode=='stale':peer.client.observe()
                args={'row':'r1' if mode=='unlisted' else 'r0'}
                if mode=='malformed':args['place']=True
                with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='select_build_facility',args=args))
                self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
            finally:peer.close()
    def test_select_and_cancel_dispatch_not_construction(self):
        peer=NativeFixture(screen='build_facilities',transitions={('build_facilities','select_build_facility'):'place_facility',('place_facility','cancel_place_facility'):'build_facilities'})
        try:
            obj=peer.client.observe();peer.client.execute(**arguments(obj,action='select_build_facility',args={'row':'r0'}))
            obj=peer.client.observe();self.assertEqual(obj['state'],'place_facility')
            peer.client.execute(**dict(arguments(obj,action='cancel_place_facility',args={}), command_id='e'*32))
            self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),2)
        finally:peer.close()
    def test_grid_closed_geometry_sorted_cells_and_action_gate(self):
        for grid in (None,dict(columns=6,rows=6,origin_x=0,origin_y=0,cell_size=32,cells=[])):
            obj=state(screen='place_facility');obj['visible']['grid']=grid
            with self.assertRaises(Unavailable):self.check(obj)
            obj['actions']=[x for x in obj['actions'] if x['id']!='place_facility_cell'];self.check(obj)
        changes=[('columns',True),('rows',5),('origin_x',449),('origin_y',209),('origin_x',-1),('cell_size',31),
                 ('cells',[dict(x=0,y=0),dict(x=0,y=0)]),('cells',[dict(x=1,y=0),dict(x=0,y=0)]),
                 ('cells',[dict(x=True,y=0)]),('cells',[dict(x=6,y=0)]),('cells',[dict(x=0,y=0,occupied=False)]),('eligible',True)]
        for key,value in changes:
            obj=state(screen='place_facility');obj['visible']['grid'][key]=value
            with self.subTest(key=key,value=value),self.assertRaises(Unavailable):self.check(obj)
        obj=state(screen='place_facility');obj['visible']['grid'].update(origin_x=448,origin_y=208,cells=[dict(x=x,y=y) for y in range(6) for x in range(6)]);self.check(obj)
        for key,value in [('label','Build laboratory'),('label_source','visible_text'),('argument_kind','point')]:
            obj=state(screen='place_facility');next(x for x in obj['actions'] if x['id']=='place_facility_cell')[key]=value
            with self.assertRaises(Unavailable):self.check(obj)
    def test_place_exact_canonical_integer_coordinates(self):
        for x,y in ((0,0),(5,5)):
            args=arguments(action='place_facility_cell',args=dict(x=x,y=y))
            self.assertTrue(a.canonical_action_request(args).endswith(f' place_facility_cell {x} {y}'.encode()))
        for value in (True,False,-1,6,1.0,'1',None):
            for axis in ('x','y'):
                args=dict(x=0,y=0);args[axis]=value
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='place_facility_cell',args=args))
    def test_place_only_fresh_membership_no_snapshot_or_mutated_dto_authority(self):
        for mode in ('stale','unlisted','mutated','cross_state'):
            peer=NativeFixture(screen='place_facility')
            try:
                obj=peer.client.observe();args=dict(x=0,y=0)
                if mode=='stale':peer.client.observe()
                if mode in ('unlisted','mutated'):args=dict(x=5,y=5)
                if mode=='mutated':obj['visible']['grid']['cells'].append(dict(x=5,y=5))
                if mode=='cross_state':peer.screen='build_facilities';peer.client.observe()
                with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='place_facility_cell',args=args))
                self.assertFalse(any(x.startswith(b'E ') for x in peer.requests));self.assertEqual(peer.client.commands,{})
            finally:peer.close()
    def test_owner_snapshot_is_not_a_placement_grant(self):
        from unittest.mock import patch
        from test_player_bases import Peer, sample
        facility=sample('facility');facility.update(x=5,y=5)
        peer=Peer([facility])
        try:
            peer.client.read_player_bases()
            self.assertIsNone(peer.client.capability);self.assertEqual(peer.client.facility_cells,frozenset())
            with patch('test_player_bases.state',side_effect=lambda:state(screen='place_facility')):
                obj=peer.client.observe()
            peer.client.read_player_bases()
            self.assertEqual(peer.client.facility_cells,{(0,0),(1,0)})
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='place_facility_cell',args=dict(x=5,y=5)))
            self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        finally:peer.close()
    def test_place_dispatch_duplicate_status_only_and_refreshed_cells(self):
        peer=NativeFixture(screen='place_facility')
        try:
            obj=peer.client.observe();args=arguments(obj,action='place_facility_cell',args=dict(x=1,y=0))
            receipt=peer.client.execute(**args);self.assertEqual(receipt['status'],'dispatched')
            self.assertNotIn('success',receipt);self.assertEqual(peer.client.facility_cells,frozenset())
            self.assertEqual(peer.client.execute(**args),receipt)
            self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)
            self.assertTrue(peer.requests[-1].startswith(b'S '))
        finally:peer.close()
if __name__=='__main__':unittest.main()
