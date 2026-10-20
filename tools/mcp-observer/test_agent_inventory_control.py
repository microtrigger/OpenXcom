# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
"""Synthetic /11 inventory contracts. No real reload, item identity or gameplay proof."""
import copy
import unittest
import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, result, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate

NEW = {'open_inventory':'none','inventory_hover':'inventory_target','inventory_activate':'inventory_target',
       'cancel_inventory_hold':'none','unload_inventory':'none','next_inventory_ground_page':'none'}


def projection():
    return dict(close_label='Close inventory',selected_name='Jane',time_units='TIME UNITS> 58',
        weight='WEIGHT> 12/30',item_name=None,ammo=None,warning=None,held=None,
        targets=[dict(handle='v0',area='hand',label='LEFT HAND',x=16,y=40,width=32,height=48,
                      point=dict(x=32,y=64),item='presented'),
                 dict(handle='v1',area='ground',label='GROUND',x=0,y=160,width=16,height=16,
                      point=dict(x=8,y=168),item='unknown')])


def inventory(visible=None,**kwargs):
    obj=state(screen='inventory',**kwargs);obj['visible']=copy.deepcopy(projection() if visible is None else visible)
    ids=a.STATE_ACTIONS['inventory']-({'cancel_inventory_hold'} if obj['visible']['held'] is None else set())
    if not obj['visible']['targets']:ids=ids-{'inventory_hover','inventory_activate'}
    obj['actions']=[dict(id=x,label=x.replace('_',' ').title(),label_source='visible_control_semantics',
                        argument_kind=a.ARGUMENT_KINDS[x]) for x in sorted(ids)]
    return obj


class InventoryFixture(NativeFixture):
    def __init__(self,**kwargs):
        self.visible=projection();super().__init__(screen='inventory',**kwargs)
    def observation(self,challenge,capability):
        if self.screen!='inventory':return super().observation(challenge,capability)
        return inventory(self.visible,challenge=challenge,capability=capability,revision=self.revision,event_sequence=self.sequence)


class InventoryValidationTests(unittest.TestCase):
    def valid(self,obj):return a.validate_state(obj,CHALLENGE,EPOCH)
    def invalid(self,obj):
        with self.assertRaises(Unavailable):self.valid(obj)

    def test_closed_counts_state_action_matrix_and_prebattle_close_only(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/11')
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS),len(a.definitions())),(45,23,4))
        self.assertEqual({x:a.ARGUMENT_KINDS[x] for x in NEW},NEW)
        self.assertIn('open_inventory',a.STATE_ACTIONS['battlescape'])
        self.assertEqual(a.STATE_ACTIONS['inventory'],set(NEW)-{'open_inventory'}|{'close_inventory'})
        self.valid(inventory());self.valid(state(screen='inventory'))

    def test_null_and_empty_text_unknown_item_do_not_imply_zero_or_emptiness(self):
        obj=inventory();obj['visible'].update(dict.fromkeys(a.INVENTORY_TEXT_FIELDS));self.valid(obj)
        for text in ('','0','Not Enough Time Units!','Alien Artifact','AMMO> 0'):
            obj['visible']['ammo']=text;obj['visible']['warning']=text;self.valid(obj)
        self.assertEqual(obj['visible']['targets'][1]['item'],'unknown')
        for bad in (0,False,{},[],3.0):
            obj['visible']['ammo']=bad;self.invalid(obj)

    def test_text_utf8_byte_bound_and_exact_close_label(self):
        for key in a.INVENTORY_TEXT_FIELDS:
            obj=inventory();obj['visible'][key]='é'*512;self.valid(obj)
            for bad in ('é'*513,'x'*1025,'\ud800'):
                obj['visible'][key]=bad;self.invalid(obj)
        for bad in (None,'Close','CLOSE INVENTORY',False):
            obj=inventory();obj['visible']['close_label']=bad;self.invalid(obj)
        obj=inventory();obj['visible']['targets'][0]['label']='é'*513;self.invalid(obj)

    def test_rectangles_points_hard_bounds_and_strict_integers(self):
        for key,value in [('x',-1),('y',400),('x',640),('width',0),('height',0),('width',625),
                          ('height',361),('x',True),('y',1.0),('width','32')]:
            obj=inventory();obj['visible']['targets'][0][key]=value;self.invalid(obj)
        for point in ({'x':15,'y':64},{'x':48,'y':64},{'x':32,'y':88},{'x':32,'y':39},{'x':True,'y':64}):
            obj=inventory();obj['visible']['targets'][0]['point']=point;self.invalid(obj)
        obj=inventory();obj['visible']['targets']=[dict(handle='v0',area='ground',label=None,x=639,y=399,
            width=1,height=1,point=dict(x=639,y=399),item='unknown')];self.valid(obj)

    def test_target_sequence_bound_and_closed_enums(self):
        for key,value in [('handle','v1'),('handle','v00'),('handle','v64'),('area','belt'),
                          ('item','empty'),('item','weapon'),('item',False)]:
            obj=inventory();obj['visible']['targets'][0][key]=value;self.invalid(obj)
        obj=inventory();row=obj['visible']['targets'][0]
        obj['visible']['targets']=[dict(copy.deepcopy(row),handle=f'v{i}') for i in range(64)];self.valid(obj)
        obj['visible']['targets'].append(dict(row,handle='v64'));self.invalid(obj)
        obj=inventory();obj['visible']['targets']=tuple(obj['visible']['targets']);self.invalid(obj)

    def test_held_cancel_needs_presented_positive_rectangle(self):
        obj=inventory();obj['visible']['held']=dict(status='presented',x=10,y=10,width=16,height=16)
        obj=inventory(obj['visible']);self.valid(obj)
        for key,value in [('status','unknown'),('status','empty'),('x',-1),('width',0),('height',391)]:
            bad=copy.deepcopy(obj);bad['visible']['held'][key]=value;self.invalid(bad)
        obj['visible']['held']=None;self.invalid(obj)

    def test_extra_fields_rejected_at_all_projection_and_descriptor_levels(self):
        for where,key in [('visible','item_id'),('visible','ground_offset'),('target','compatible'),
                          ('target','slot_id'),('target','empty'),('point','z'),('held','weapon_type')]:
            obj=inventory();obj['visible']['held']=dict(status='presented',x=0,y=0,width=1,height=1)
            target={'visible':obj['visible'],'target':obj['visible']['targets'][0],
                    'point':obj['visible']['targets'][0]['point'],'held':obj['visible']['held']}[where]
            target[key]='hidden';self.invalid(obj)
        for key,value in [('argument_kind','point'),('label_source','visible_text'),('preconditions',{'enough_tu':True})]:
            obj=inventory();obj['actions'][0][key]=value;self.invalid(obj)

    def test_target_action_requires_nonempty_list_unload_needs_no_hidden_eligibility(self):
        obj=inventory();obj['visible']['targets']=[];self.invalid(obj)
        obj=inventory(obj['visible']);self.valid(obj)
        self.assertIn('unload_inventory',[x['id'] for x in obj['actions']])

    def test_closed_canonical_args_no_coordinates_or_internal_metadata(self):
        for action,kind in NEW.items():
            args={'target':'v63'} if kind=='inventory_target' else {};obj=arguments(action=action,args=args)
            self.assertTrue(a.canonical_action_request(obj).endswith((action+(' v63' if args else '')).encode()))
            for key in ('x','y','item_id','slot','ammo','cost','eligible'):
                with self.assertRaises(Unavailable):a.validate_action_arguments(dict(obj,args=dict(args,**{key:0})))
        for bad in ('v64','v00','v-1','r0','g0','V0','v0 ','v0\n',0,False,None):
            for action in ('inventory_hover','inventory_activate'):
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args={'target':bad}))

    def test_all_earlier_wire_versions_rejected(self):
        for version in range(1,11):
            schema=f'openxcom-agent-control/{version}'
            with self.assertRaises(Unavailable):a.validate_hello(dump(dict(schema=schema,mode=a.MODE,epoch=EPOCH,ledger_capacity=256,audio_history='mixed_output')))
            obj=inventory();obj['schema']=schema;self.invalid(obj)
            obj=result();obj['schema']=schema
            with self.assertRaises(Unavailable):a.validate_result(dump(obj),COMMAND,EPOCH)
            obj=page([]);obj['schema']=schema
            with self.assertRaises(Unavailable):validate(obj)

    def test_inventory_retained_presentation_has_no_action_authority(self):
        event=dict(seq=1,kind='presentation',state='inventory',visible=projection());validate(page([event]))
        for key in ('capability','actions','item_ids','ground_offset'):
            obj=dict(event,**{key:'hidden'})
            with self.assertRaises(Unavailable):validate(page([obj]))


class InventoryClientTests(unittest.TestCase):
    def fixture(self,**kwargs):
        native=InventoryFixture(**kwargs);self.addCleanup(native.close);return native

    def test_unseen_forged_target_preserves_grant_and_ledger(self):
        native=self.fixture();obj=native.client.observe();count=len(native.requests)
        for action in ('inventory_hover','inventory_activate'):
            with self.assertRaises(Unavailable):native.client.execute(**arguments(obj,action=action,args={'target':'v63'}))
        self.assertEqual(len(native.requests),count);self.assertEqual(native.client.commands,{})
        self.assertEqual(native.client.capability,obj['capability'])

    def test_returned_mutation_cannot_add_authority_or_remove_cached_authority(self):
        native=self.fixture();obj=native.client.observe();obj['visible']['targets'].append(dict(obj['visible']['targets'][0],handle='v2'))
        with self.assertRaises(Unavailable):native.client.execute(**arguments(obj,action='inventory_activate',args={'target':'v2'}))
        obj['visible']['targets']=[]
        self.assertEqual(native.client.execute(**arguments(obj,action='inventory_hover',args={'target':'v0'}))['status'],'dispatched')

    def test_stale_page_held_or_same_label_actor_context_rejected_before_send(self):
        for dimension in ('page','held','actor'):
            native=self.fixture();old=native.client.observe()
            if dimension=='page':native.visible['targets']=[]
            elif dimension=='held':native.visible['held']=dict(status='presented',x=1,y=1,width=1,height=1)
            else:native.visible['selected_name']='Jane' # identical label is never an actor identity
            native.revision+=1;new=native.client.observe();count=len(native.requests)
            with self.assertRaises(Unavailable):native.client.execute(**arguments(old,action='inventory_hover',args={'target':'v0'}))
            self.assertEqual(len(native.requests),count);self.assertEqual(native.client.capability,new['capability'])
            self.assertEqual(native.client.commands,{})

    def test_exact_duplicate_uses_status_changed_target_and_action_conflict(self):
        native=self.fixture();obj=native.client.observe();args=arguments(obj,action='inventory_activate',args={'target':'v0'})
        first=native.client.execute(**args);count=len(native.requests)
        for change in (dict(args={'target':'v1'}),dict(action='inventory_hover')):
            with self.assertRaises(Unavailable):native.client.execute(**dict(args,**change))
        self.assertEqual(len(native.requests),count);self.assertEqual(native.client.execute(**args),first)
        self.assertEqual([x.split()[0] for x in native.requests].count(b'E'),1)
        self.assertTrue(native.requests[-1].startswith(b'S '));self.assertEqual(native.client.inventory_targets,frozenset())

    def test_leaving_inventory_clears_target_authority(self):
        native=self.fixture(transitions={('inventory','close_inventory'):'battlescape'})
        obj=native.client.observe();native.client.execute(**arguments(obj,action='close_inventory'))
        current=native.client.observe();self.assertEqual(current['state'],'battlescape')
        self.assertEqual(native.client.inventory_targets,frozenset());count=len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**arguments(current,command_id='5'*32,action='inventory_activate',args={'target':'v0'}))
        self.assertEqual(len(native.requests),count)

    def test_open_inventory_uses_only_current_offered_battlescape_action(self):
        native=NativeFixture(transitions={('battlescape','open_inventory'):'inventory'});self.addCleanup(native.close)
        obj=native.client.observe();reply=native.client.execute(**arguments(obj,action='open_inventory'))
        self.assertEqual(reply['observation']['state'],'inventory');self.assertIsNone(reply['observation']['capability'])
        self.assertEqual(reply['status'],'dispatched')

    def test_inventory_commands_share_non_evicting_256_ledger(self):
        native=self.fixture()
        for index in range(a.MAX_COMMANDS):
            obj=native.client.observe()
            native.client.execute(**arguments(obj,command_id=f'{index+1:032x}',action='inventory_hover',args={'target':'v0'}))
        self.assertEqual(len(native.client.commands),256)
        obj=native.client.observe();count=len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**arguments(obj,command_id='f'*32,action='inventory_activate',args={'target':'v0'}))
        self.assertEqual(len(native.requests),count);self.assertEqual(len(native.client.commands),256)
        self.assertEqual(native.client.state,'CLOSED')

    def test_argument_mutation_cannot_change_immutable_request_identity(self):
        native=self.fixture();obj=native.client.observe();args=arguments(obj,action='inventory_activate',args={'target':'v0'})
        original=copy.deepcopy(args);first=native.client.execute(**args);args['args']['target']='v1'
        with self.assertRaises(Unavailable):native.client.execute(**args)
        self.assertEqual(native.client.execute(**original),first)

    def test_event_read_keeps_target_grant_and_does_not_admit_command(self):
        native=self.fixture();obj=native.client.observe();native.public_events=[dict(seq=1,kind='presentation',state='inventory',visible=projection())]
        before=(native.client.capability,native.client.revision,native.client.inventory_targets,native.client.capability_deadline)
        events=native.client.read_events(EPOCH,0,16);events['events'][0]['visible']['targets']=[]
        self.assertEqual(before,(native.client.capability,native.client.revision,native.client.inventory_targets,native.client.capability_deadline))
        self.assertEqual(native.client.commands,{})
        self.assertEqual(native.client.execute(**arguments(obj,action='inventory_hover',args={'target':'v1'}))['status'],'dispatched')

    def test_unavailable_receipt_has_no_observation_or_game_refusal_reason(self):
        def transform(kind,obj):return result(obj['command_id'],'unavailable',obj['event_sequence']) if kind=='E' else obj
        native=self.fixture(transform=transform);obj=native.client.observe()
        reply=native.client.execute(**arguments(obj,action='unload_inventory'))
        self.assertEqual(reply['status'],'unavailable');self.assertIsNone(reply['observation'])
        self.assertEqual(len(native.client.commands),1);self.assertIsNone(native.client.capability)

    def test_mutating_loss_unknown_terminal_no_later_send_or_retry(self):
        native=self.fixture(transform=lambda kind,obj:None if kind=='E' else obj);obj=native.client.observe()
        args=arguments(obj,action='inventory_activate',args={'target':'v0'})
        with self.assertRaises(a.ActionOutcomeUnknown) as error:native.client.execute(**args)
        self.assertEqual(error.exception.public_error()['command_id'],COMMAND);self.assertFalse(error.exception.public_error()['retryable'])
        count=len(native.requests)
        for operation in (lambda:native.client.execute(**args),native.client.observe,lambda:native.client.read_result(COMMAND),lambda:native.client.read_events(EPOCH,0,16)):
            with self.assertRaises(Unavailable):operation()
        self.assertEqual(len(native.requests),count);self.assertEqual(native.client.state,'CLOSED')

    def test_malformed_inventory_receipt_is_unknown_not_fabricated_native_receipt(self):
        def transform(kind,obj):
            if kind=='E':obj['observation']['visible']['targets'][0]['item']='empty'
            return obj
        native=self.fixture(transform=transform);obj=native.client.observe()
        with self.assertRaises(a.ActionOutcomeUnknown):native.client.execute(**arguments(obj,action='inventory_hover',args={'target':'v0'}))
        self.assertEqual(native.client.state,'CLOSED')

    def test_semantic_tools_have_no_images_and_closed_target_json_schema(self):
        native=self.fixture();server=adapter.Server(agent_control=native.client)
        observed=call(server,'read_ui_state')['result'];self.assertTrue(all(x['type']=='text' for x in observed['content']))
        obj=observed['structuredContent'];args=arguments(obj,action='inventory_hover',args={'target':'v0'})
        executed=call(server,'execute_ui_action',args)['result'];self.assertTrue(all(x['type']=='text' for x in executed['content']))
        for name,params in [('read_ui_action_result',{'command_id':COMMAND}),('read_public_events',{'epoch':EPOCH,'after':0,'limit':16})]:
            value=call(server,name,params)['result'];self.assertTrue(all(x['type']=='text' for x in value['content']))
        execute=next(x for x in a.definitions() if x['name']=='execute_ui_action')['inputSchema']
        branch=next(x for x in execute['oneOf'] if 'inventory_hover' in x['properties']['action']['enum'])
        self.assertEqual(branch['properties']['args']['required'],['target']);self.assertFalse(branch['properties']['args']['additionalProperties'])
        self.assertEqual(branch['properties']['args']['properties']['target']['enum'],list(a.INVENTORY_TARGET_HANDLES))


if __name__=='__main__':unittest.main()
