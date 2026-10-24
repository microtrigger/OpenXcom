"""Save/load DTO and bounded argument contract; no game, disk operations or socket."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable


def visible(save=True):
    obj=dict(title='Save' if save else 'Load',name_heading='Name',date_heading='Date',details=None,
             rows=[dict(handle='r0',cells=['NEW'] if save else ['Duplicate','2026-10-24','14:00'])])
    if save: obj['new_save_name']=None
    return obj


def request(action,args):
    return dict(command_id='1'*32,epoch='2'*32,capability='3'*32,revision=1,action=action,args=args)


class SaveLoadContract(unittest.TestCase):
    def test_painted_popup_empty_schema_has_no_target_authority(self):
        for kind in ('save_list','load_list'):
            obj=dict(title=None,name_heading=None,date_heading=None,details=None,rows=[])
            if kind=='save_list':obj['new_save_name']=None
            a.validate_visible(kind,obj,set())
            action='select_new_save_slot' if kind=='save_list' else 'load_saved_game'
            with self.subTest(kind=kind),self.assertRaises(Unavailable):a.validate_visible(kind,obj,{action})
    def test_new_slot_not_save(self):
        a.validate_visible('save_list',visible(),{'select_new_save_slot','cancel_menu'})
        with self.assertRaises(Unavailable): a.validate_visible('save_list',visible(),{'save_new_game'})
    def test_rendered_edit(self):
        obj=visible();obj['new_save_name']='MCP Test';obj['rows']=[]
        a.validate_visible('save_list',obj,{'replace_new_save_name','save_new_game'})
        obj['new_save_name']=''
        a.validate_visible('save_list',obj,{'replace_new_save_name'})
        with self.assertRaises(Unavailable):a.validate_visible('save_list',obj,{'save_new_game'})
    def test_private_fields_rejected(self):
        for key in ('filename','path','mods','timestamp','ironman'):
            with self.subTest(key=key):
                obj=visible();obj[key]='PRIVATE'
                with self.assertRaises(Unavailable):a.validate_visible('save_list',obj,set())
    def test_row_identity_and_shape(self):
        for change in ('handle','null','four'):
            obj=visible(False)
            if change=='handle':obj['rows'][0]['handle']='file0'
            elif change=='null':obj['rows'][0]['cells'][0]=None
            else:obj['rows'][0]['cells'].append('PRIVATE')
            with self.subTest(change=change),self.assertRaises(Unavailable):a.validate_visible('load_list',obj,set())
    def test_duplicate_captions_distinct_handles(self):
        obj=visible(False);second=copy.deepcopy(obj['rows'][0]);second['handle']='r1';obj['rows'].append(second)
        a.validate_visible('load_list',obj,{'load_saved_game'})
        obj['rows']=[]
        with self.assertRaises(Unavailable):a.validate_visible('load_list',obj,{'load_saved_game'})
    def test_no_existing_row_as_new(self):
        obj=visible();obj['rows'][0]['cells']=['Existing','Date','Time']
        with self.assertRaises(Unavailable):a.validate_visible('save_list',obj,{'select_new_save_slot'})
    def test_readonly_progress(self):
        for state in ('save_in_progress','load_in_progress'):
            self.assertFalse(a.STATE_ACTIONS[state])
            a.validate_visible(state,{'message':'Saving...'},set())
            a.validate_visible(state,{'message':None},set())
            with self.assertRaises(Unavailable):a.validate_visible(state,{'message':'Saving...','filename':'PRIVATE'},set())
    def test_input_bounds_and_no_paths(self):
        for name in ('','MCP Test','é'*32):a.validate_action_arguments(request('replace_new_save_name',{'text':name}))
        for name in ('../file','a\\b','x'*129,'bad\nname'):
            with self.subTest(name=name),self.assertRaises(Unavailable):a.validate_action_arguments(request('replace_new_save_name',{'text':name}))
    def test_explicit_action_schema(self):
        for action in ('select_new_save_slot','save_new_game'):
            a.validate_action_arguments(request(action,{}))
            with self.assertRaises(Unavailable):a.validate_action_arguments(request(action,{'row':'r0'}))
        a.validate_action_arguments(request('load_saved_game',{'row':'r0'}))
        self.assertEqual(a.label_source('save_list','replace_new_save_name'),'visible_control_semantics')
        self.assertEqual(a.label_source('load_list','load_saved_game'),'visible_control_semantics')
        for action in ('overwrite_save','delete_save','import_save'):
            with self.assertRaises(Unavailable):a.validate_action_arguments(request(action,{}))

class SaveLoadClient(unittest.TestCase):
    def fixture(self,screen,after_send_unknown=False):
        from test_agent_control import NativeFixture
        def transform(op,obj):
            if op=='E' and after_send_unknown:
                return {'schema':a.SCHEMA,'status':'unavailable'}
            if op=='R':
                obj['visible']=visible(screen=='save_list')
                if screen=='save_list':
                    obj['visible']['new_save_name']='MCP Test'
                    obj['visible']['rows']=[]
                    obj['actions']=[x for x in obj['actions'] if x['id'] in ('save_new_game','cancel_menu')]
            return obj
        native=NativeFixture(screen=screen,transform=transform)
        self.addCleanup(native.close)
        return native
    def test_load_row_nonexistent_and_stale_never_send(self):
        from test_agent_control import arguments
        native=self.fixture('load_list');old=native.client.observe()
        for row in ('r1','r99'):
            with self.subTest(row=row),self.assertRaises(Unavailable):native.client.execute(**arguments(old,action='load_saved_game',args={'row':row}))
        fresh=native.client.observe()
        with self.assertRaises(Unavailable):native.client.execute(**arguments(old,action='load_saved_game',args={'row':'r0'}))
        self.assertNotEqual(old['capability'],fresh['capability'])
        self.assertFalse(any(p.startswith(b'E ')for p in native.requests))
    def test_save_unknown_after_send_no_replay(self):
        from test_agent_control import arguments
        native=self.fixture('save_list',True);obj=native.client.observe();req=arguments(obj,action='save_new_game',args={})
        with self.assertRaises(a.ActionOutcomeUnknown):native.client.execute(**req)
        before=len(native.requests)
        for op in (lambda:native.client.execute(**req),native.client.observe):
            with self.assertRaises(Unavailable):op()
        self.assertEqual(before,len(native.requests))
        self.assertEqual(sum(p.startswith(b'E ')for p in native.requests),1)

if __name__=='__main__':unittest.main()
