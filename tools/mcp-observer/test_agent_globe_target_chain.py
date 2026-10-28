# SPDX-License-Identifier: GPL-3.0-or-later
"""Painted target controls; socket fixtures do not establish native outcomes."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture,state,arguments,CHALLENGE,EPOCH
from test_agent_events import page,validate

def checked(obj):return a.validate_state(obj,CHALLENGE,EPOCH)
def geo():
    obj=state(screen='geoscape');obj['visible']['targets']=[dict(row='r0',x=100,y=70,width=1,height=1)]
    obj['actions'].append(dict(id='click_visible_globe_target',label='Inspect visible target',label_source='visible_control_semantics',argument_kind='visible_row'))
    return obj

class GlobeTargetTests(unittest.TestCase):
    def fixture(self,**kwargs):
        p=NativeFixture(**kwargs);self.addCleanup(p.close);return p
    def test_catalog_and_semantics(self):
        self.assertEqual((a.SCHEMA,len(a.ACTIONS),len(a.STATE_ACTIONS)),('openxcom-agent-control/48',250,105))
        for obj in (geo(),state(screen='multiple_targets'),state(screen='target_info')):
            checked(obj)
            validate(page([dict(seq=1,kind='presentation',state=obj['state'],visible=obj['visible'])]))
    def test_marker_is_bounded_pixel_not_world_position(self):
        for key,value in (('x',-1),('x',640),('y',400),('width',2),('height',0),('row','r9'),('longitude',1)):
            obj=geo();obj['visible']['targets'][0][key]=value
            with self.assertRaises(Unavailable):checked(obj)
        obj=geo();obj['visible']['targets']=[]
        with self.assertRaises(Unavailable):checked(obj)
    def test_marker_control_requires_fresh_visible_handle(self):
        def transform(op,obj):
            if op=='R':
                obj['visible']['targets']=geo()['visible']['targets']
                obj['actions'].append(geo()['actions'][-1])
                p.latest=copy.deepcopy(obj)
            return obj
        p=self.fixture(screen='geoscape',transform=transform);old=p.client.observe();fresh=p.client.observe()
        for ob,args in ((old,{'row':'r0'}),(fresh,{'row':'r1'})):
            with self.assertRaises(Unavailable):p.client.execute(**arguments(ob,action='click_visible_globe_target',args=args))
        self.assertFalse(any(x.startswith(b'E ')for x in p.requests))
        self.assertEqual(p.client.execute(**arguments(fresh,action='click_visible_globe_target',args={'row':'r0'}))['status'],'dispatched')
    def test_chooser_rows_no_hidden_targets(self):
        for key,value in (('selected',True),('selected',1),('handle','r5'),('cells',['A','B']),('id',123)):
            obj=state(screen='multiple_targets');obj['visible']['rows'][0][key]=value
            with self.assertRaises(Unavailable):checked(obj)
        obj=state(screen='multiple_targets');obj['visible']['buttons']=[dict(id='cancel',label='Cancel')]
        with self.assertRaises(Unavailable):checked(obj)
    def test_chooser_select_and_keyboard_cancel_dispatch(self):
        for action in ('choose_globe_target','cancel_globe_targets'):
            p=self.fixture(screen='multiple_targets');obj=p.client.observe()
            self.assertEqual(p.client.execute(**arguments(obj,action=action))['status'],'dispatched')
    def test_hidden_edit_does_not_grant_rename(self):
        obj=state(screen='target_info');obj['visible']['name']=None
        with self.assertRaises(Unavailable):checked(obj)
        obj['actions']=[x for x in obj['actions']if x['id']!='focus_target_name'];checked(obj)
    def test_target_buttons_exact_pair(self):
        for action,key in (('open_target_interception','intercept'),('close_target_info','close')):
            obj=state(screen='target_info');obj['actions']=[x for x in obj['actions']if x['id']!=action]
            with self.assertRaises(Unavailable):checked(obj)
            obj=state(screen='target_info');obj['visible']['buttons']=[x for x in obj['visible']['buttons']if x['id']!=key]
            with self.assertRaises(Unavailable):checked(obj)
    def test_rename_safe_text_and_native_request(self):
        for text in ('x'*129,'a\nb','\x00'):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='replace_target_name',args={'text':text}))
        def focused(op,obj):
            target=obj.get('observation') if op=='E' else obj
            if target and target.get('state')=='target_info':
                target['visible']['buttons']=[]
                target['actions']=[dict(id=x,label=a.TARGET_SEMANTICS[x],label_source='visible_control_semantics',argument_kind=a.ARGUMENT_KINDS[x])for x in ('replace_target_name','finish_target_name')]
                if op=='R':p.latest=copy.deepcopy(obj)
            return obj
        p=self.fixture(screen='target_info',transform=focused);obj=p.client.observe()
        self.assertEqual(p.client.execute(**arguments(obj,action='replace_target_name',args={'text':'Test waypoint'}))['status'],'dispatched')
    def test_native_editor_focus_and_finish_controls(self):
        obj=state(screen='target_info')
        for action in ('focus_target_name','finish_target_name'):
            current=copy.deepcopy(obj)
            current['actions']=[dict(id=action,label=a.TARGET_SEMANTICS[action],label_source='visible_control_semantics',argument_kind='none')]
            current['visible']['buttons']=[]
            if action=='finish_target_name':current['actions'].append(dict(id='replace_target_name',label='Rename target',label_source='visible_control_semantics',argument_kind='text'))
            checked(current)
        current['actions'].append(dict(id='focus_target_name',label='Focus target name',label_source='visible_control_semantics',argument_kind='none'))
        with self.assertRaises(Unavailable):checked(current)
        current=copy.deepcopy(obj);current['visible']['buttons']=[]
        current['actions']=[dict(id=x,label=a.TARGET_SEMANTICS[x],label_source='visible_control_semantics',argument_kind=a.ARGUMENT_KINDS[x])for x in ('replace_target_name','finish_target_name')];checked(current)
        current['actions'].pop()
        with self.assertRaises(Unavailable):checked(current)

    def test_no_hidden_target_or_world_fields(self):
        for screen in ('multiple_targets','target_info'):
            for key in ('target_id','longitude','latitude','hidden_targets','followers_model'):
                obj=state(screen=screen);obj['visible'][key]=[]
                with self.assertRaises(Unavailable):checked(obj)
    def test_speed_and_marker_handles_stay_independent(self):
        obj=geo();obj['visible']['speed']='5 Secs';obj['visible']['speeds']=[dict(handle='s0',label='5 Secs',selected=True)]
        obj['actions'].append(dict(id='choose_geoscape_speed',label='5 Secs',label_source='visible_text',argument_kind='geoscape_speed'));checked(obj)
        with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='choose_geoscape_speed',args={'speed':'r0'}))
        with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='click_visible_globe_target',args={'row':'s0'}))

if __name__=='__main__':unittest.main()
