# SPDX-License-Identifier: GPL-3.0-or-later
"""Build selection/details/cancel only: never construction or a base grid."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, arguments, NativeFixture, CHALLENGE, EPOCH
NEW=('open_build_facilities','select_build_facility','close_build_facilities','cancel_place_facility')
class FacilityTests(unittest.TestCase):
    def check(self,obj):return a.validate_state(obj,CHALLENGE,EPOCH)
    def test_closed_painted_fields_no_placement_authority(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/32')
        self.assertEqual(a.STATE_ACTIONS['place_facility'],{'cancel_place_facility'})
        for screen in ('build_facilities','place_facility'):
            obj=state(screen=screen);self.check(obj)
            for k in obj['visible']:
                bad=copy.deepcopy(obj);del bad['visible'][k]
                with self.assertRaises(Unavailable):self.check(bad)
            for k in ('rule_id','occupancy','eligible','grid','target','construction','placed'):
                bad=copy.deepcopy(obj);bad['visible'][k]=None
                with self.assertRaises(Unavailable):self.check(bad)
        obj=state(screen='place_facility')
        for k in obj['visible']:
            if k!='buttons':obj['visible'][k]=None
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
            a.validate_action_arguments(arguments(action=action,args={'row':'r0'} if action=='select_build_facility' else {}))
            for bad in ({'rule_id':'LAB'},{'x':0,'y':0},{'row':'r00'},{'row':'r32'}):
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=bad))
    def test_schema31_refused(self):
        obj=state(screen='build_facilities');obj['schema']='openxcom-agent-control/31'
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
if __name__=='__main__':unittest.main()
