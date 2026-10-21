# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded rendered dogfight DTO/authority; synthetic transport, not combat proof."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH
class DogfightTests(unittest.TestCase):
    def checked(self,obj):return a.validate_state(obj,CHALLENGE,EPOCH)
    def fixture(self,**kwargs):
        peer=NativeFixture(screen='dogfight',**kwargs);self.addCleanup(peer.close);return peer
    def test_exact_shape_nullable_paint_no_hidden_damage(self):
        obj=state(screen='dogfight');self.checked(obj)
        for key in obj['visible']:
            bad=copy.deepcopy(obj);del bad['visible'][key]
            with self.assertRaises(Unavailable):self.checked(bad)
        for key in ('ufo_health','terminal','destroyed','craft_id','target_id','hit','radar_range','model_distance'):
            bad=copy.deepcopy(obj);bad['visible'][key]=0
            with self.assertRaises(Unavailable):self.checked(bad)
        for key in ('status','distance','ammo1','ammo2','interception'):
            obj['visible'][key]=None
        self.checked(obj)
        for damage in (0,100,'damaged',False,{},[]):
            obj['visible']['damage']=damage
            with self.assertRaises(Unavailable):self.checked(obj)
    def test_buttons_canonical_subset_and_selection(self):
        obj=state(screen='dogfight')
        for buttons in (list(reversed(obj['visible']['buttons'])),[dict(id='fire',selected=False)],[dict(id='standoff',selected=1)], [dict(id='standoff',selected=False)]*2, [dict(id='standoff',selected=True),dict(id='cautious',selected=True)], [dict(id='standoff',selected=False,label='extra')],[]):
            bad=copy.deepcopy(obj);bad['visible']['buttons']=buttons
            with self.assertRaises(Unavailable):self.checked(bad)
        obj['visible']['buttons']=[dict(id='disengage',selected=False)];self.checked(obj)
        obj['visible']['buttons']=[];obj['actions']=[];obj['capability']=None;self.checked(obj)
    def test_closed_enum_wire_schema_and_no_payload_extensions(self):
        for mode in a.DOGFIGHT_MODES:
            self.assertTrue(a.canonical_action_request(arguments(action='choose_dogfight_mode',args={'mode':mode})).endswith((' '+mode).encode()))
        for mode in ('attack','Standard','standard\n','r0',0,None,True,[],{}):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='choose_dogfight_mode',args={'mode':mode}))
        for args in ({},{'mode':'standard','target':0},{'mode':'standard','repeat':2},{'choice':'standard'}):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='choose_dogfight_mode',args=args))
        branch=next(x for x in a.definitions()[1]['inputSchema']['oneOf'] if 'choose_dogfight_mode' in x['properties']['action']['enum'])
        self.assertEqual(branch['properties']['args'],dict(type='object',properties={'mode':{'type':'string','enum':list(a.DOGFIGHT_MODES)}},required=['mode'],additionalProperties=False))
    def test_cross_screen_and_semantic_label_provenance(self):
        descriptor=state(screen='dogfight')['actions'][0]
        for screen in a.STATE_ACTIONS:
            if screen=='dogfight':continue
            obj=state(screen=screen);obj['actions']=[copy.deepcopy(descriptor)]
            with self.subTest(screen=screen),self.assertRaises(Unavailable):self.checked(obj)
        for key,value in [('label','Attack target'),('label_source','visible_text'),('argument_kind','none')]:
            obj=state(screen='dogfight');obj['actions'][0][key]=value
            with self.assertRaises(Unavailable):self.checked(obj)
    def test_fresh_mode_dispatch_and_consumption(self):
        for mode in a.DOGFIGHT_MODES:
            peer=self.fixture();obj=peer.client.observe()
            self.assertEqual(peer.client.execute(**arguments(obj,action='choose_dogfight_mode',args={'mode':mode}))['status'],'dispatched')
            self.assertFalse(peer.client.dogfight_modes);self.assertIsNone(peer.client.capability)
    def test_unshown_and_old_modes_fail_before_send(self):
        def transform(op,obj):
            if op=='R':obj['visible']['buttons']=[dict(id='standoff',selected=True)]
            return obj
        peer=self.fixture(transform=transform);old=peer.client.observe();fresh=peer.client.observe()
        for obj,mode in ((old,'standoff'),(fresh,'aggressive')):
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='choose_dogfight_mode',args={'mode':mode}))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))

if __name__=='__main__':unittest.main()
