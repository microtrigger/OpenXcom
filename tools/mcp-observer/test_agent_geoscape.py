# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-19.
"""Closed wire17 Geoscape/base contracts. Synthetic peers are not live proof."""
import copy
import json
import unittest

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate

NEW_ACTIONS = ('choose_geoscape_speed','open_bases','select_base','open_base_info','close_base_info',
               'return_geoscape','open_base_stores','close_base_stores','open_base_transfers','close_base_transfers')
NEW_STATES = ('basescape','base_info','base_stores','base_transfers')


def descriptors(obj, ids):
    buttons={x['id']:x['label'] for x in obj['visible'].get('buttons',[])}
    mapping=({'open_funding':'funding','open_bases':'bases','open_pause_menu':'options'} if obj['state']=='geoscape'
             else a.BASE_BUTTON_ACTIONS.get(obj['state'],{}))
    def label(action):
        if action=='choose_geoscape_speed':return obj['visible']['speeds'][0]['label']
        if action=='select_base':return 'Select base'
        return buttons[mapping[action]] if action in mapping else action.replace('_',' ').title()
    obj['actions']=[dict(id=x,label=label(x),label_source=a.label_source(obj['state'],x),argument_kind=a.ARGUMENT_KINDS[x]) for x in ids]
    if not ids:obj['capability']=None
    return obj


def geo():
    obj=state(screen='geoscape')
    obj['visible'].update(speed='5 Secs',speeds=[dict(handle='s0',label='5 Secs',selected=True),dict(handle='s1',label='1 Min',selected=False)])
    return descriptors(obj,sorted(a.STATE_ACTIONS['geoscape']))


def checked(obj):return a.validate_state(obj,CHALLENGE,EPOCH)


def sized_table(size):
    obj=state(screen='base_stores');obj['visible']['rows']=[dict(handle=f'r{i}',cells=['','',''],selected=False) for i in range(32)]
    left=size-len(json.dumps(obj,ensure_ascii=True,separators=(',',':')).encode('ascii'))
    if left<0:raise ValueError('too small')
    for row in obj['visible']['rows']:
        for i in range(3):
            n=min(left,1024);row['cells'][i]='x'*n;left-=n
    if left:raise ValueError('too large')
    return obj


class StrategyFixture(NativeFixture):
    def observation(self,challenge,capability):
        obj=super().observation(challenge,capability)
        if self.screen=='geoscape':
            rich=geo();obj['visible']=rich['visible'];obj['actions']=rich['actions']
        return obj


class GeoscapeContractTests(unittest.TestCase):
    def test_exact_wire_catalog_and_argument_domains(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/25')
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS),len(a.definitions())),(131,55,4))
        self.assertEqual(a.GEOSCAPE_SPEED_HANDLES,tuple(f's{i}' for i in range(6)))
        self.assertEqual(a.BASE_TARGET_HANDLES,tuple(f'b{i}' for i in range(8)))
        self.assertEqual({x:a.ARGUMENT_KINDS[x] for x in NEW_ACTIONS},dict.fromkeys(set(NEW_ACTIONS)-{'choose_geoscape_speed','select_base'},'none')|{'choose_geoscape_speed':'geoscape_speed','select_base':'base_target'})
        self.assertEqual((a.MAX_COMMANDS,a.MAX_REQUEST,a.MAX_TEXT_PACKET,a.MAX_PACKET,a.EVENT_CAPACITY),(256,512,32768,49152,256))

    def test_all_prior_wires_fail_closed(self):
        for version in range(1,17):
            obj=geo();obj['schema']=f'openxcom-agent-control/{version}'
            with self.subTest(version=version),self.assertRaises(Unavailable):checked(obj)
            with self.assertRaises(Unavailable):a.validate_hello(dump(dict(schema=obj['schema'],mode=a.MODE,epoch=EPOCH,ledger_capacity=256,audio_history='mixed_output')))

    def test_canonical_arguments_no_hidden_selection(self):
        for action in NEW_ACTIONS:
            for args in ([{'speed':'s0'},{'speed':'s5'}] if action=='choose_geoscape_speed' else [{'target':'b0'},{'target':'b7'}] if action=='select_base' else [{}]):
                arg=arguments(action=action,args=args)
                self.assertTrue(a.canonical_action_request(arg).endswith((action+(' '+next(iter(args.values())) if args else '')).encode()))
                for extra in ('base_id','index','exists','eligible','world_position','ticks','paused'):
                    with self.subTest(action=action,extra=extra),self.assertRaises(Unavailable):a.validate_action_arguments(dict(arg,args=dict(args,**{extra:0})))

    def test_closed_handles_and_schema_enums(self):
        for action,key,bad in [('choose_geoscape_speed','speed',('s6','s00','S0','s-1','s0\n','5sec',1,True,None,[])),('select_base','target',('b8','b00','B0','b-1','b0\n','0',0,True,None,{}))]:
            for value in bad:
                with self.subTest(action=action,value=value),self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args={key:value}))
        branches=a.definitions()[1]['inputSchema']['oneOf']
        for action,key,expected in [('choose_geoscape_speed','speed',list(a.GEOSCAPE_SPEED_HANDLES)),('select_base','target',list(a.BASE_TARGET_HANDLES))]:
            branch=next(x for x in branches if x['properties']['action']['enum']==[action])
            self.assertEqual(branch['properties']['args']['properties'][key]['enum'],expected)

    def test_wrong_states_never_gain_new_authority(self):
        for screen in a.STATE_ACTIONS:
            for action in set(NEW_ACTIONS)-a.STATE_ACTIONS[screen]:
                obj=state(screen=screen);obj['actions']=[dict(id=action,label='Visible',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action])]
                with self.subTest(screen=screen,action=action),self.assertRaises(Unavailable):checked(obj)

    def test_speed_selection_exactly_matches_shown_pressed_label(self):
        obj=geo();checked(obj)
        for selected in (None,0,1):
            obj=geo()
            for i,speed in enumerate(obj['visible']['speeds']):speed['selected']=i==selected
            obj['visible']['speed']=None if selected is None else obj['visible']['speeds'][selected]['label'];checked(obj)
        for value in (None,'1 Min','hidden_speed'):
            obj=geo();obj['visible']['speed']=value
            with self.assertRaises(Unavailable):checked(obj)
        obj=geo();obj['visible']['speeds'][1]['selected']=True
        with self.assertRaises(Unavailable):checked(obj)

    def test_speed_order_types_labels_and_absent_choices(self):
        for key,value in [('handle','s1'),('label',''),('label',None),('label','é'*513),('selected',1),('selected',None),('ticks',300)]:
            obj=geo();obj['visible']['speeds'][0][key]=value
            with self.subTest(key=key,value=value),self.assertRaises(Unavailable):checked(obj)
        obj=geo();obj['visible'].update(speeds=[],speed=None)
        with self.assertRaises(Unavailable):checked(obj)
        descriptors(obj,['open_bases']);checked(obj)
        obj=geo();obj['visible']['speeds']=[dict(handle=f's{i}',label=str(i),selected=False) for i in range(7)];obj['visible']['speed']=None
        with self.assertRaises(Unavailable):checked(obj)

    def test_shown_buttons_required_and_descriptor_labels_bound(self):
        for action,button in [('open_bases','bases'),('open_pause_menu','options'),('open_funding','funding')]:
            obj=descriptors(geo(),[action]);checked(obj)
            obj['visible']['buttons']=[b for b in obj['visible']['buttons'] if b['id']!=button]
            with self.assertRaises(Unavailable):checked(obj)
        for action in ('open_bases','open_pause_menu','choose_geoscape_speed'):
            obj=descriptors(geo(),[action]);obj['actions'][0]['label']='Inferred'
            with self.assertRaises(Unavailable):checked(obj)

    def test_geoscape_unknown_model_and_old_field_limits(self):
        for key in ('base_id','bases','hidden_exists','time_paused','time_speed','elapsed_ticks'):
            obj=geo();obj['visible'][key]=None
            with self.assertRaises(Unavailable):checked(obj)
        obj=geo();obj['visible']['funds']='x'*4096;checked(obj)
        obj['visible']['funds']+='x'
        with self.assertRaises(Unavailable):checked(obj)

    def test_base_slots_compact_strict_and_no_identity_fields(self):
        for screen in ('basescape','base_info'):
            obj=state(screen=screen);checked(obj)
            obj['visible']['base_slots']=[dict(handle=f'b{i}',selected=i==7) for i in range(8)];checked(obj)
            for field,value in [('handle','b8'),('handle','b00'),('selected',1),('exists',True),('base_id',1),('name','Private')]:
                bad=copy.deepcopy(obj);bad['visible']['base_slots'][0][field]=value
                with self.subTest(screen=screen,field=field),self.assertRaises(Unavailable):checked(bad)
            obj['visible']['base_slots'][0]['selected']=True
            with self.assertRaises(Unavailable):checked(obj)

    def test_empty_slots_allow_no_selection_authority(self):
        for screen in ('basescape','base_info'):
            obj=state(screen=screen);obj['visible']['base_slots']=[]
            with self.assertRaises(Unavailable):checked(obj)
            obj['actions']=[x for x in obj['actions'] if x['id']!='select_base'];checked(obj)
            obj=state(screen=screen);next(x for x in obj['actions'] if x['id']=='select_base')['label']='Select existing base'
            with self.assertRaises(Unavailable):checked(obj)

    def test_base_buttons_canonical_subset_and_membership(self):
        for screen in ('basescape','base_info'):
            for action,key in a.BASE_BUTTON_ACTIONS[screen].items():
                obj=descriptors(state(screen=screen),[action]);checked(obj)
                obj['visible']['buttons']=[b for b in obj['visible']['buttons'] if b['id']!=key]
                with self.assertRaises(Unavailable):checked(obj)
            for change in ('reverse','duplicate','extra','empty_label'):
                obj=state(screen=screen)
                if change=='reverse':obj['visible']['buttons'].reverse()
                elif change=='duplicate':obj['visible']['buttons'].append(copy.deepcopy(obj['visible']['buttons'][0]))
                elif change=='extra':obj['visible']['buttons'][0]['id']='hidden_action'
                else:obj['visible']['buttons'][0]['label']=''
                with self.subTest(screen=screen,change=change),self.assertRaises(Unavailable):checked(obj)

    def test_base_strings_null_utf8_limits_and_exact_fields(self):
        for screen,fields in [('basescape',('base_name','location','funds')),('base_info',('base_name','personnel_heading','space_heading'))]:
            for field in fields:
                for value in (None,'','é'*512):
                    obj=state(screen=screen);obj['visible'][field]=value;checked(obj)
                for value in ('é'*513,True,1,[],{}):
                    obj=state(screen=screen);obj['visible'][field]=value
                    with self.subTest(screen=screen,field=field),self.assertRaises(Unavailable):checked(obj)
            for extra in ('id','base_id','capacity','facility_grid','exists','selected_index'):
                obj=state(screen=screen);obj['visible'][extra]=None
                with self.assertRaises(Unavailable):checked(obj)

    def test_metrics_exact_rendered_strings_duplicate_labels_valid(self):
        obj=state(screen='base_info');obj['visible']['metrics']=[dict(label='Personnel',value=None),dict(label='Personnel',value='10:20')];checked(obj)
        for key,value in [('label',''),('label',None),('value',12),('value','x'*1025),('total',20),('id','soldiers')]:
            bad=copy.deepcopy(obj);bad['visible']['metrics'][0][key]=value
            with self.subTest(key=key),self.assertRaises(Unavailable):checked(bad)
        for count in (0,12):
            obj['visible']['metrics']=[dict(label='Shown',value='') for _ in range(count)];checked(obj)
        obj['visible']['metrics'].append(dict(label='Shown',value=''))
        with self.assertRaises(Unavailable):checked(obj)

    def test_readonly_three_column_rows_and_no_row_activation(self):
        for screen in ('base_stores','base_transfers'):
            obj=state(screen=screen);obj['visible']['rows']=[dict(handle=f'r{i}',cells=['Rifle','2',''],selected=False) for i in range(32)];checked(obj)
            for field,value in [('handle','r32'),('cells',['A','B']),('cells',['A','B','C','D']),('selected',True),('selected',0),('item_id',1),('remaining_hours',10)]:
                bad=copy.deepcopy(obj);bad['visible']['rows'][0][field]=value
                with self.subTest(screen=screen,field=field),self.assertRaises(Unavailable):checked(bad)
            obj['actions'].append(dict(id='choose_visible_row',label='Select',label_source='visible_control_semantics',argument_kind='visible_row'))
            with self.assertRaises(Unavailable):checked(obj)

    def test_table_empty_slice_null_headings_and_scroll_boundaries(self):
        for screen in ('base_stores','base_transfers'):
            obj=state(screen=screen);obj['visible'].update(title=None,headings=[None,None,None],rows=[],scroll=dict(up=False,down=False))
            with self.assertRaises(Unavailable):checked(obj)
            obj['actions']=[x for x in obj['actions'] if x['id']!='scroll_visible_list'];checked(obj)
            for key in ('up','down'):
                bad=copy.deepcopy(obj);bad['visible']['scroll'][key]=1
                with self.assertRaises(Unavailable):checked(bad)
            obj['visible']['hidden_total']=0
            with self.assertRaises(Unavailable):checked(obj)

    def test_table_utf8_bounds_do_not_change_funding_contract(self):
        for screen in ('base_stores','base_transfers'):
            obj=state(screen=screen);obj['visible']['rows'][0]['cells'][0]='é'*512;checked(obj)
            obj['visible']['rows'][0]['cells'][0]+='x'
            with self.assertRaises(Unavailable):checked(obj)
        obj=state(screen='funding');obj['visible']['rows'][0]['cells'][0]='x'*4096;checked(obj)

    def test_state_canonical_ascii_exact_budget(self):
        checked(sized_table(32768))
        with self.assertRaises(Unavailable):checked(sized_table(32769))
        obj=state(screen='base_stores');obj['visible']['rows']=[dict(handle=f'r{i}',cells=['\x01'*400]*3,selected=False) for i in range(32)]
        with self.assertRaises(Unavailable):checked(obj)

    def test_new_presentation_union_and_event_budget(self):
        events=[dict(seq=i+1,kind='presentation',state=screen,visible=state(screen=screen)['visible']) for i,screen in enumerate(NEW_STATES)]
        validate(page(events))
        bad=copy.deepcopy(events);bad[1]['visible']['model']=[]
        with self.assertRaises(Unavailable):validate(page(bad))
        obj=sized_table(30000);event=dict(seq=1,kind='presentation',state='base_stores',visible=obj['visible'])
        validate(page([event],newest=2),limit=16)
        two=[event,dict(copy.deepcopy(event),seq=2)]
        with self.assertRaises(Unavailable):validate(page(two))

    def test_destructive_or_commit_actions_remain_absent(self):
        for action in ('rename_base','build_facility','buy_item','sell_item','start_research','stop_research','start_manufacture','assign_personnel','create_transfer','cancel_transfer'):
            self.assertNotIn(action,a.ACTIONS)
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action))


class GeoscapeClientTests(unittest.TestCase):
    def fixture(self,**kwargs):
        peer=StrategyFixture(**kwargs);self.addCleanup(peer.close);return peer

    def test_connected_navigation_each_handler_once(self):
        transitions={('geoscape','open_bases'):'basescape',('basescape','open_base_info'):'base_info',
                     ('base_info','open_base_stores'):'base_stores',('base_stores','close_base_stores'):'base_info',
                     ('base_info','open_base_transfers'):'base_transfers',('base_transfers','close_base_transfers'):'base_info',
                     ('base_info','close_base_info'):'basescape',('basescape','return_geoscape'):'geoscape',
                     ('geoscape','open_pause_menu'):'pause_menu',('pause_menu','resume_game'):'geoscape'}
        peer=self.fixture(screen='geoscape',transitions=transitions);client=peer.client
        sequence=[('choose_geoscape_speed',{'speed':'s1'}),('open_bases',{}),('select_base',{'target':'b1'}),('open_base_info',{}),
                  ('open_base_stores',{}),('scroll_visible_list',{'direction':'down'}),('close_base_stores',{}),('open_base_transfers',{}),
                  ('scroll_visible_list',{'direction':'up'}),('close_base_transfers',{}),('close_base_info',{}),('return_geoscape',{}),('open_pause_menu',{}),('resume_game',{})]
        for i,(action,args) in enumerate(sequence):
            before=client.observe();reply=client.execute(**arguments(before,command_id=f'{100+i:032x}',action=action,args=args))
            self.assertEqual(reply['status'],'dispatched');self.assertIsNone(client.capability)
            self.assertFalse(client.base_targets);self.assertFalse(client.geoscape_speeds)
        self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),14)
        self.assertEqual(client.observe()['state'],'geoscape')

    def test_unlisted_handles_do_not_consume_fresh_grant(self):
        for screen,action,args in [('geoscape','choose_geoscape_speed',{'speed':'s5'}),('basescape','select_base',{'target':'b7'})]:
            peer=self.fixture(screen=screen);obj=peer.client.observe();before=len(peer.requests)
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action=action,args=args))
            self.assertEqual(len(peer.requests),before);self.assertEqual(peer.client.capability,obj['capability'])
            good={'speed':'s0'} if screen=='geoscape' else {'target':'b0'}
            self.assertEqual(peer.client.execute(**arguments(obj,action=action,args=good))['status'],'dispatched')

    def test_same_handle_after_fresh_read_does_not_save_old_authority(self):
        peer=self.fixture(screen='basescape');old=peer.client.observe();fresh=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='select_base',args={'target':'b0'}))
        self.assertEqual(peer.client.capability,fresh['capability'])
        self.assertEqual(peer.client.execute(**arguments(fresh,action='select_base',args={'target':'b0'}))['status'],'dispatched')

    def test_fresh_projection_replaces_membership_and_state_clear(self):
        peer=self.fixture(screen='basescape');old=peer.client.observe();self.assertIn('b1',peer.client.base_targets)
        def shrink(op,obj):
            if op=='R':obj['visible']['base_slots']=obj['visible']['base_slots'][:1]
            return obj
        peer.transform=shrink;fresh=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,action='select_base',args={'target':'b1'}))
        peer.transform=None;peer.screen='geoscape';geo_state=peer.client.observe()
        self.assertFalse(peer.client.base_targets);self.assertEqual(peer.client.geoscape_speeds,{'s0','s1'})
        peer.client.close();self.assertFalse(peer.client.geoscape_speeds)

    def test_largest_fresh_handles_are_actionable_without_identity(self):
        for screen,action,args in [('geoscape','choose_geoscape_speed',{'speed':'s5'}),('basescape','select_base',{'target':'b7'})]:
            class Expanded(StrategyFixture):
                def observation(self,challenge,capability):
                    obj=super().observation(challenge,capability)
                    if self.screen=='geoscape':
                        obj['visible']['speeds']=[dict(handle=f's{i}',label=str(i),selected=False) for i in range(6)]
                        obj['visible']['speed']=None
                        for d in obj['actions']:
                            if d['id']=='choose_geoscape_speed':d['label']='0'
                    else:obj['visible']['base_slots']=[dict(handle=f'b{i}',selected=False) for i in range(8)]
                    return obj
            peer=Expanded(screen=screen);self.addCleanup(peer.close);obj=peer.client.observe()
            self.assertEqual(peer.client.execute(**arguments(obj,action=action,args=args))['status'],'dispatched')
            self.assertTrue(peer.requests[-1].endswith(next(iter(args.values())).encode()))

    def test_duplicate_status_only_changed_args_conflict(self):
        peer=self.fixture(screen='geoscape');obj=peer.client.observe();args=arguments(obj,action='choose_geoscape_speed',args={'speed':'s1'})
        first=peer.client.execute(**args);second=peer.client.execute(**args);self.assertEqual(first,second)
        self.assertEqual(peer.requests[-1],('S '+COMMAND).encode())
        with self.assertRaises(Unavailable):peer.client.execute(**dict(args,args={'speed':'s0'}))
        self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)

    def test_unknown_outcome_terminal_never_replayed(self):
        peer=self.fixture(screen='basescape',transform=lambda op,obj:b'{' if op=='E' else obj)
        obj=peer.client.observe();args=arguments(obj,action='select_base',args={'target':'b0'})
        with self.assertRaises(a.ActionOutcomeUnknown):peer.client.execute(**args)
        self.assertEqual(peer.client.state,'CLOSED')
        with self.assertRaises(Unavailable):peer.client.execute(**args)
        self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)

    def test_known_unavailable_requires_new_context(self):
        def refuse(op,obj):
            if op=='E':obj.update(status='unavailable',observation=None)
            return obj
        peer=self.fixture(screen='basescape',transform=refuse);obj=peer.client.observe()
        reply=peer.client.execute(**arguments(obj,action='select_base',args={'target':'b0'}))
        self.assertEqual(reply['status'],'unavailable');self.assertEqual(peer.client.state,'NEED_CONTEXT')
        self.assertFalse(peer.client.base_targets)
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,command_id='5'*32,action='select_base',args={'target':'b0'}))
        self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)

    def test_event_replay_preserves_new_membership_and_grant(self):
        peer=self.fixture(screen='basescape');obj=peer.client.observe()
        peer.public_events=[dict(seq=1,kind='presentation',state='base_info',visible=state(screen='base_info')['visible'])]
        saved=(peer.client.capability,peer.client.revision,peer.client.base_targets,dict(peer.client.commands))
        first=peer.client.read_events(EPOCH,0,16);second=peer.client.read_events(EPOCH,0,16)
        self.assertEqual(first['events'],second['events']);self.assertEqual(saved,(peer.client.capability,peer.client.revision,peer.client.base_targets,dict(peer.client.commands)))
        peer.public_events[0]['visible']['base_name']='Changed retained data'
        with self.assertRaises(Unavailable):peer.client.read_events(EPOCH,0,16)
        self.assertEqual(peer.client.state,'CLOSED')

    def test_table_scroll_uses_current_shown_direction(self):
        def arrows(op,obj):
            if op=='R':obj['visible']['scroll']['down']=False
            return obj
        peer=self.fixture(screen='base_stores',transform=arrows);obj=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='scroll_visible_list',args={'direction':'down'}))
        self.assertEqual(peer.client.execute(**arguments(obj,action='scroll_visible_list',args={'direction':'up'}))['status'],'dispatched')

    def test_real_server_invalid_params_and_structured_output(self):
        peer=self.fixture(screen='geoscape');server=adapter.Server(agent_control=peer.client)
        response=call(server,'read_ui_state')['result'];obj=response['structuredContent'];self.assertEqual(json.loads(response['content'][0]['text']),obj)
        before=len(peer.requests)
        bad=call(server,'execute_ui_action',arguments(obj,action='choose_geoscape_speed',args={'speed':'s6'}))
        self.assertEqual(bad['error']['code'],-32602);self.assertEqual(len(peer.requests),before)
        response=call(server,'execute_ui_action',arguments(obj,action='choose_geoscape_speed',args={'speed':'s1'}))['result']
        self.assertFalse(response.get('isError',False));self.assertEqual(response['structuredContent']['semantics'],'dispatch_not_success')


if __name__=='__main__':unittest.main()
