# SPDX-License-Identifier: GPL-3.0-or-later
"""Global logical cursor contract; synthetic transport, never desktop clicks."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture,state,arguments,CHALLENGE,EPOCH,CAPABILITY
MOVE=dict(id='move_cursor',label='Move cursor',label_source='visible_control_semantics',argument_kind='cursor_position')
def cursor_state(screen='unsupported'):
    obj=state(screen=screen);obj['cursor']=dict(x=0,y=0,width=320,height=200,visible=True)
    obj['actions'].append(dict(MOVE));obj['capability']=CAPABILITY;return obj
class CursorTests(unittest.TestCase):
    def checked(self,obj):return a.validate_state(obj,CHALLENGE,EPOCH)
    def fixture(self,screen='unsupported'):
        class CursorFixture(NativeFixture):
            def observation(self,challenge,capability):
                obj=super().observation(challenge,capability)
                obj['cursor']=dict(x=0,y=0,width=320,height=200,visible=True)
                obj['actions'].append(dict(MOVE));obj['capability']=capability
                return obj
        peer=CursorFixture(screen=screen);self.addCleanup(peer.close);return peer
    def test_cursor_global_all_screens_and_null_without_action(self):
        for screen in a.STATE_ACTIONS:
            obj=cursor_state(screen);self.checked(obj)
            obj['cursor']['visible']=False
            with self.assertRaises(Unavailable):self.checked(obj)
            obj['actions']=[entry for entry in obj['actions'] if entry['id']!='move_cursor']
            if not obj['actions']:obj['capability']=None
            self.checked(obj)
            old=state(screen=screen);self.checked(old)
            del old['cursor']
            with self.assertRaises(Unavailable):self.checked(old)
    def test_strict_metadata_bounds_types_keys(self):
        for field,values in [('x',(-65536,65536,True,1.0,None)),('y',(-65536,65536,False,1.0)),('width',(0,4097,True,1.0)),('height',(0,4097,False,1.0)),('visible',(0,1,None,'true'))]:
            for value in values:
                obj=cursor_state();obj['cursor'][field]=value
                with self.subTest(field=field,value=value),self.assertRaises(Unavailable):self.checked(obj)
        for key in ('x','y','width','height','visible'):
            obj=cursor_state();del obj['cursor'][key]
            with self.assertRaises(Unavailable):self.checked(obj)
        obj=cursor_state();obj['cursor']['click']=False
        with self.assertRaises(Unavailable):self.checked(obj)
        for x,y in ((-65535,65535),(65535,-65535)):
            obj=cursor_state();obj['cursor'].update(x=x,y=y,width=4096,height=1)
            with self.assertRaises(Unavailable):self.checked(obj)
            obj.update(actions=[],capability=None);self.checked(obj)
    def test_null_duplicate_label_and_unsupported_semantics_fail(self):
        obj=cursor_state();obj['cursor']=None
        with self.assertRaises(Unavailable):self.checked(obj)
        obj=cursor_state();obj['actions'].append(dict(MOVE))
        with self.assertRaises(Unavailable):self.checked(obj)
        for key,value in [('label','Click'),('label_source','visible_text'),('argument_kind','point')]:
            obj=cursor_state();obj['actions'][0][key]=value
            with self.assertRaises(Unavailable):self.checked(obj)
        obj=cursor_state();obj['actions'].append(dict(id='new_battle',label='New Battle',label_source='visible_text',argument_kind='none'))
        with self.assertRaises(Unavailable):self.checked(obj)
        obj=cursor_state();obj['visible']={'hidden':1}
        with self.assertRaises(Unavailable):self.checked(obj)
    def test_postaction_has_no_grant(self):
        obj=cursor_state();obj.update(challenge=None,capability=None)
        a.validate_state(obj,None,EPOCH,post_action=True)
        obj['capability']=CAPABILITY
        with self.assertRaises(Unavailable):a.validate_state(obj,None,EPOCH,post_action=True)
    def test_closed_integer_target_and_canonical_wire(self):
        for x,y in ((0,0),(4095,4095),(319,199)):
            self.assertTrue(a.canonical_action_request(arguments(action='move_cursor',args=dict(x=x,y=y))).endswith(f' move_cursor {x} {y}'.encode()))
        for value in (-1,4096,True,False,1.0,'1',None,[],{}):
            for field in ('x','y'):
                args=dict(x=0,y=0);args[field]=value
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='move_cursor',args=args))
        for args in ({'x':1},{'x':1,'y':1,'click':True},{'x':1,'y':1,'duration':1}):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='move_cursor',args=args))
    def test_bounds_and_singleuse_before_send(self):
        # Fixture's native grant catalog must include global cursor too.
        for screen in ('unsupported','basescape'):
            peer=self.fixture(screen);obj=peer.client.observe()
            for x,y in ((320,0),(0,200),(4095,4095)):
                with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='move_cursor',args=dict(x=x,y=y)))
            self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
            self.assertEqual(peer.client.execute(**arguments(obj,action='move_cursor',args={'x':319,'y':199}))['status'],'dispatched')
            self.assertIsNone(peer.client.cursor_bounds);self.assertIsNone(peer.client.capability)
    def test_stale_grant_and_mutated_returned_bounds_cannot_expand(self):
        peer=self.fixture();old=peer.client.observe();fresh=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='move_cursor',args={'x':0,'y':0}))
        fresh['cursor']['width']=4096
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,action='move_cursor',args={'x':400,'y':0}))
        self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
    def test_cursor_position_schema_exact(self):
        branch=next(x for x in a.definitions()[1]['inputSchema']['oneOf'] if 'move_cursor' in x['properties']['action']['enum'])
        self.assertEqual(branch['properties']['args'],dict(type='object',properties={key:dict(type='integer',minimum=0,maximum=4095) for key in ('x','y')},required=['x','y'],additionalProperties=False))
    def test_offered_cursor_requires_current_in_canvas(self):
        for x,y in ((-1,0),(0,-1),(320,0),(0,200),(65535,65535)):
            obj=cursor_state();obj['cursor'].update(x=x,y=y)
            with self.assertRaises(Unavailable):self.checked(obj)
            obj.update(actions=[],capability=None);self.checked(obj)
    def test_schema25_refused(self):
        obj=cursor_state();obj['schema']='openxcom-agent-control/25'
        with self.assertRaises(Unavailable):self.checked(obj)
if __name__=='__main__':unittest.main()
