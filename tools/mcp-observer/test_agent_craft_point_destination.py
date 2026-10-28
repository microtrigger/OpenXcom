# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded front-globe point authority; native paint/geometry proof is separate."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

ACTION = 'choose_visible_craft_point'

class VisibleCraftPointTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def test_exact_semantic_descriptor_and_argument_encoding(self):
        obj=state(screen='craft_destination_selection'); self.checked(obj)
        for key,value in (('label','Hidden destination'),('label_source','visible_text'),('argument_kind','visible_row')):
            bad=copy.deepcopy(obj); next(x for x in bad['actions'] if x['id']==ACTION)[key]=value
            with self.assertRaises(Unavailable): self.checked(bad)
        self.assertTrue(a.canonical_action_request(arguments(action=ACTION,args={'x':64,'y':80})).endswith(b'choose_visible_craft_point 64 80'))
        for args in ({},{'x':64},{'x':True,'y':80},{'x':64,'y':80.0},{'x':-1,'y':80},{'x':64,'y':80,'target_id':2}):
            with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action=ACTION,args=args))

    def test_exact_globe_and_front_sample_shape(self):
        obj=state(screen='craft_destination_selection')
        for key,values in {'width':(0,513,True),'height':(0,401,False),'sample_step':(0,15,17,True)}.items():
            for value in values:
                bad=copy.deepcopy(obj);bad['visible']['globe'][key]=value
                with self.subTest(key=key,value=value),self.assertRaises(Unavailable):self.checked(bad)
        for key,values in {'x':(-1,256,True,1.0),'y':(-1,0,27,200,False),'terrain':('hidden',None,1)}.items():
            for value in values:
                bad=copy.deepcopy(obj);bad['visible']['globe']['samples'][0][key]=value
                with self.subTest(key=key,value=value),self.assertRaises(Unavailable):self.checked(bad)
        for key in ('longitude','latitude','target_id','hidden'):
            bad=copy.deepcopy(obj);bad['visible']['globe']['samples'][0][key]=0
            with self.assertRaises(Unavailable):self.checked(bad)
        bad=copy.deepcopy(obj);bad['visible']['globe']['samples']*=2
        with self.assertRaises(Unavailable):self.checked(bad)
        for key in obj['visible']['globe']:
            bad=copy.deepcopy(obj);del bad['visible']['globe'][key]
            with self.assertRaises(Unavailable):self.checked(bad)

    def test_absent_or_empty_globe_grants_no_point_action(self):
        for globe in (None,dict(width=256,height=200,sample_step=16,samples=[])):
            obj=state(screen='craft_destination_selection');obj['visible']['globe']=globe
            with self.assertRaises(Unavailable):self.checked(obj)
            obj['actions']=[x for x in obj['actions'] if x['id']!=ACTION]
            self.checked(obj) # Existing marker action remains independent.

    def test_neighbour_has_no_point_authority(self):
        descriptor=next(x for x in state(screen='craft_destination_selection')['actions'] if x['id']==ACTION)
        for screen in ('geoscape','build_base','geoscape_craft_navigation','landing_confirmation','unsupported'):
            obj=state(screen=screen);obj['actions']=[descriptor]
            with self.subTest(screen=screen),self.assertRaises(Unavailable):self.checked(obj)

    def test_exact_fresh_sample_membership_and_receipt_only(self):
        peer=NativeFixture(screen='craft_destination_selection');self.addCleanup(peer.close)
        old=peer.client.observe();fresh=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action=ACTION,args={'x':64,'y':80}))
        for point in ({'x':65,'y':80},{'x':100,'y':70},{'x':64,'y':27},{'x':256,'y':80}):
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,action=ACTION,args=point))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        request=arguments(fresh,action=ACTION,args={'x':64,'y':80})
        receipt=peer.client.execute(**request)
        self.assertEqual(receipt['status'],'dispatched')
        self.assertEqual(peer.client.execute(**request),receipt)
        self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)
        self.assertNotIn('arrived',receipt)
        self.assertIsNone(peer.client.viewport_bounds)
        self.assertFalse(peer.client.viewport_samples)

if __name__=='__main__':unittest.main()
