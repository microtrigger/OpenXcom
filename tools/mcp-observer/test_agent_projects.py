# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-19.
"""Wire18 project contracts. Synthetic peers never certify actual gameplay."""
import copy
import json
import unittest
import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate

NEW_ACTIONS=('open_research','open_new_research','select_research_project','adjust_scientists',
 'accept_research','cancel_research_project','close_research_list','open_manufacture','open_new_production',
 'select_production_project','start_production','cancel_production_start','adjust_engineers',
 'accept_production','stop_production','close_production_list')
NEW_STATES=tuple(a.PROJECT_BUTTONS)


def checked(obj):return a.validate_state(obj,CHALLENGE,EPOCH)


def descriptors(obj, ids):
    visible=obj['visible'];buttons={x['id']:x['label'] for x in visible['buttons']}
    mapping=a.PROJECT_BUTTON_ACTIONS.get(obj['state'],a.BASE_BUTTON_ACTIONS.get(obj['state'],{}))
    def label(action):
        if action in a.QUANTITY_SEMANTIC_LABELS:return a.QUANTITY_SEMANTIC_LABELS[action]
        if action=='set_production_sell':return visible['sell']['label']
        if obj['state']=='new_research' and action=='select_research_project':return 'Select research project'
        if action==a.PROJECT_SELECTION_ACTIONS.get(obj['state']):return visible['rows'][0]['cells'][0]
        if action==a.PROJECT_STAFF_ACTIONS.get(obj['state']):return next(buttons[x] for x in a.STAFF_DIRECTIONS if x in buttons)
        return buttons[mapping[action]] if action in mapping else 'Scroll list'
    obj['actions']=[dict(id=x,label=label(x),label_source=a.label_source(obj['state'],x),argument_kind=a.ARGUMENT_KINDS[x]) for x in ids]
    if not ids:obj['capability']=None
    return obj


def sized_project(size):
    obj=state(screen='manufacture')
    obj['visible']['rows']=[dict(handle=f'r{i}',cells=['P','','','',''],selected=False) for i in range(32)]
    descriptors(obj,[x['id'] for x in obj['actions']])
    left=size-len(json.dumps(obj,ensure_ascii=True,separators=(',',':')).encode('ascii'))
    if left<0:raise ValueError('size too small')
    for row in obj['visible']['rows']:
        for i in range(1,5):
            n=min(left,1024);row['cells'][i]='x'*n;left-=n
    if left:raise ValueError('size too large')
    # Preserve descriptor binding after changing the first row text.
    return descriptors(obj,[x['id'] for x in obj['actions']])


class ProjectsContractTests(unittest.TestCase):
    def test_wire_catalog_limits_and_exact_new_kinds(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/26')
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS),len(a.definitions())),(145,61,4))
        self.assertEqual(len(NEW_STATES),7);self.assertEqual(len(NEW_ACTIONS),16)
        typed={'select_research_project':'project_row','select_production_project':'project_row',
               'adjust_scientists':'staff_direction','adjust_engineers':'staff_direction'}
        self.assertEqual({x:a.ARGUMENT_KINDS[x] for x in NEW_ACTIONS},dict.fromkeys(set(NEW_ACTIONS)-typed.keys(),'none')|typed)
        self.assertEqual((a.MAX_COMMANDS,a.EVENT_CAPACITY,a.MAX_TEXT_PACKET,a.MAX_PACKET),(256,256,32768,49152))

    def test_every_prior_wire_rejected(self):
        for version in range(1,22):
            obj=state(screen='research');obj['schema']=f'openxcom-agent-control/{version}'
            with self.subTest(version=version),self.assertRaises(Unavailable):checked(obj)
            with self.assertRaises(Unavailable):a.validate_hello(dump(dict(schema=obj['schema'],mode=a.MODE,epoch=EPOCH,ledger_capacity=256,audio_history='mixed_output')))

    def test_canonical_request_and_schema_exact_domains(self):
        for action in NEW_ACTIONS:
            choices=([{'row':'r0'},{'row':'r31'}] if a.ARGUMENT_KINDS[action]=='project_row' else
                     [{'direction':'increase'},{'direction':'decrease'}] if a.ARGUMENT_KINDS[action]=='staff_direction' else [{}])
            for args in choices:
                request=a.canonical_action_request(arguments(action=action,args=args))
                self.assertTrue(request.endswith((action+(' '+next(iter(args.values())) if args else '')).encode()))
        branches=a.definitions()[1]['inputSchema']['oneOf']
        for action,key,values in [('select_research_project','row',list(a.ROW_HANDLES)),('adjust_engineers','direction',list(a.STAFF_DIRECTIONS))]:
            branch=next(x for x in branches if action in x['properties']['action']['enum'])
            self.assertEqual(branch['properties']['args']['properties'][key]['enum'],values)

    def test_bad_rows_staff_steps_and_hidden_arguments_rejected(self):
        for action,key,values in [('select_research_project','row',('r32','r00','r-1','R0','r0\n',0,True,None,[])),
                                  ('adjust_engineers','direction',('max','up','+1',1,True,None,{}))]:
            for value in values:
                with self.subTest(value=value),self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args={key:value}))
        for action in NEW_ACTIONS:
            for extra in ('project_id','rule_id','eligible','count','delta','max','all','sell','quantity','scientists'):
                arg=arguments(action=action);arg['args'][extra]=1
                with self.subTest(action=action,extra=extra),self.assertRaises(Unavailable):a.validate_action_arguments(arg)

    def test_wrong_states_reject_authority(self):
        for screen in a.STATE_ACTIONS:
            for action in set(NEW_ACTIONS)-a.STATE_ACTIONS[screen]:
                obj=state(screen=screen);obj['actions']=[dict(id=action,label='Shown',label_source='visible_text',argument_kind=a.ARGUMENT_KINDS[action])]
                with self.subTest(screen=screen,action=action),self.assertRaises(Unavailable):checked(obj)

    def test_fixed_summary_heading_and_row_dimensions(self):
        for screen,(summary,columns) in a.PROJECT_LISTS.items():
            obj=state(screen=screen);checked(obj)
            self.assertEqual((len(obj['visible']['summary']),len(obj['visible']['headings'])),(summary,columns))
            for key in ('summary','headings'):
                for change in (-1,1):
                    bad=copy.deepcopy(obj);bad['visible'][key]=[None]*max(0,len(bad['visible'][key])+change)
                    if bad['visible'][key]==obj['visible'][key]:continue
                    if summary==0 and key=='summary' and change==-1:continue
                    with self.subTest(screen=screen,key=key,change=change),self.assertRaises(Unavailable):checked(bad)
            for cells in ([],['x']*(columns+1))+(() if screen=='new_research' else ([None]*columns,)):
                bad=copy.deepcopy(obj);bad['visible']['rows'][0]['cells']=cells
                with self.assertRaises(Unavailable):checked(bad)

    def test_compact_rows_max32_selection_and_duplicate_labels(self):
        for screen,(_,columns) in a.PROJECT_LISTS.items():
            obj=state(screen=screen);obj['visible']['rows']=[dict(handle=f'r{i}',cells=['Project']+['0']*(columns-1),selected=i==31) for i in range(32)];checked(obj)
            for key,value in [('handle','r32'),('handle','r01'),('selected',1),('project_id','STR_MEDI_KIT')]:
                bad=copy.deepcopy(obj);bad['visible']['rows'][0][key]=value
                with self.subTest(screen=screen,key=key),self.assertRaises(Unavailable):checked(bad)
            obj['visible']['rows'][0]['selected']=True
            with self.assertRaises(Unavailable):checked(obj)
            obj['visible']['rows'][0]['selected']=False;obj['visible']['rows'].append(dict(handle='r32',cells=['x']*columns,selected=False))
            with self.assertRaises(Unavailable):checked(obj)

    def test_selection_requires_shown_title_rows_and_nonempty_first_cells(self):
        for screen in a.PROJECT_LISTS:
            for key,value in [('title',None),('rows',[])]:
                obj=state(screen=screen);obj['visible'][key]=value
                with self.assertRaises(Unavailable):checked(obj)
                descriptors(obj,[x['id'] for x in obj['actions'] if x['id']!=a.PROJECT_SELECTION_ACTIONS[screen]]);checked(obj)
            obj=state(screen=screen);obj['visible']['rows'][0]['cells'][0]=''
            with self.assertRaises(Unavailable):checked(obj)
            descriptors(obj,[x['id'] for x in obj['actions'] if x['id']!=a.PROJECT_SELECTION_ACTIONS[screen]]);checked(obj)

    def test_materials_are_readonly_three_cells_never_selected(self):
        obj=state(screen='manufacture_start');obj['visible']['rows']=[dict(handle='r0',cells=['Alloy','2','4'],selected=False)];checked(obj)
        for key,value in [('selected',True),('cells',['x','y']),('cells',['x',None,'y']),('recipe_id',1)]:
            bad=copy.deepcopy(obj);bad['visible']['rows'][0][key]=value
            with self.assertRaises(Unavailable):checked(bad)
        obj['actions'].append(dict(id='select_production_project',label='Alloy',label_source='visible_text',argument_kind='project_row'))
        with self.assertRaises(Unavailable):checked(obj)

    def test_project_text_nullable_exact_utf8_and_preserved_renderer_tokens(self):
        for screen in NEW_STATES:
            obj=descriptors(state(screen=screen),[])
            fields=[key for key,value in obj['visible'].items() if type(value) is str or value is None]
            for key in fields:
                for value in (None,'','é'*512,'Allocated>\x0150','\x02line'):
                    bad=copy.deepcopy(obj);bad['visible'][key]=value;self.assertEqual(checked(bad)['visible'][key],value)
                for value in ('é'*513,1,True,[],{}):
                    bad=copy.deepcopy(obj);bad['visible'][key]=value
                    with self.subTest(screen=screen,key=key),self.assertRaises(Unavailable):checked(bad)
        obj=state(screen='research');obj['visible']['rows'][0]['cells'][1]='é'*512;checked(obj)
        obj['visible']['rows'][0]['cells'][1]+='x'
        with self.assertRaises(Unavailable):checked(obj)

    def test_exact_fields_reject_hidden_model_and_commit_flags(self):
        for screen in NEW_STATES:
            for key in ('project_id','recipe','eligible','completed','can_afford','remaining_hours','private_staff','start_charged','success'):
                obj=state(screen=screen);obj['visible'][key]=None
                with self.subTest(screen=screen,key=key),self.assertRaises(Unavailable):checked(obj)
            for key in state(screen=screen)['visible']:
                obj=state(screen=screen);del obj['visible'][key]
                with self.subTest(screen=screen,missing=key),self.assertRaises(Unavailable):checked(obj)

    def test_buttons_are_canonical_shown_subset_with_bound_labels(self):
        for screen in NEW_STATES:
            for action,key in a.PROJECT_BUTTON_ACTIONS[screen].items():
                obj=descriptors(state(screen=screen),[action]);checked(obj)
                obj['visible']['buttons']=[x for x in obj['visible']['buttons'] if x['id']!=key]
                with self.assertRaises(Unavailable):checked(obj)
            for mutation in ('duplicate','unknown','empty','extra'):
                obj=state(screen=screen)
                if mutation=='duplicate':obj['visible']['buttons'].append(copy.deepcopy(obj['visible']['buttons'][0]))
                elif mutation=='unknown':obj['visible']['buttons'][0]['id']='max'
                elif mutation=='empty':obj['visible']['buttons'][0]['label']=''
                else:obj['visible']['buttons'][0]['enabled']=True
                with self.subTest(screen=screen,mutation=mutation),self.assertRaises(Unavailable):checked(obj)
            if len(a.PROJECT_BUTTONS[screen])>1:
                obj=state(screen=screen);obj['visible']['buttons'].reverse()
                with self.assertRaises(Unavailable):checked(obj)

    def test_every_new_descriptor_exact_text_and_source(self):
        for screen in ('basescape',)+NEW_STATES:
            for descriptor in state(screen=screen)['actions']:
                if descriptor['id'] not in NEW_ACTIONS:continue
                for key,value in [('label','Unshown interpretation'),('label_source','visible_text' if descriptor['label_source']=='visible_control_semantics' else 'visible_control_semantics'),('argument_kind','none' if descriptor['argument_kind']!='none' else 'project_row')]:
                    obj=state(screen=screen);next(x for x in obj['actions'] if x['id']==descriptor['id'])[key]=value
                    with self.subTest(screen=screen,action=descriptor['id'],key=key),self.assertRaises(Unavailable):checked(obj)

    def test_staff_requires_public_title_allocation_and_shown_direction(self):
        for screen,action in a.PROJECT_STAFF_ACTIONS.items():
            for key in ('title','allocated'):
                obj=state(screen=screen);obj['visible'][key]=None
                with self.assertRaises(Unavailable):checked(obj)
                descriptors(obj,[x['id'] for x in obj['actions'] if x['id']!=action]);checked(obj)
            obj=state(screen=screen);obj['visible']['buttons']=[x for x in obj['visible']['buttons'] if x['id'] not in a.STAFF_DIRECTIONS]
            with self.assertRaises(Unavailable):checked(obj)
            obj=state(screen=screen);obj['visible']['buttons']=[x for x in obj['visible']['buttons'] if x['id']!='increase']
            if screen=='manufacture_info':obj['visible']['quantity_controls'][0]['increase']=False
            descriptors(obj,[x['id'] for x in obj['actions']]);checked(obj)
            self.assertEqual(next(x['label'] for x in obj['actions'] if x['id']==action),'Decrease')

    def test_scroll_depends_on_shown_arrow_only(self):
        for screen in tuple(a.PROJECT_LISTS)+('manufacture_start',):
            obj=state(screen=screen);obj['visible']['scroll']=dict(up=False,down=False)
            ids=[x['id'] for x in obj['actions'] if x['id']!='scroll_visible_list'];descriptors(obj,ids);checked(obj)
            descriptors(obj,ids+['scroll_visible_list'])
            with self.assertRaises(Unavailable):checked(obj)
            obj['visible']['scroll']['up']=True;checked(obj)
            obj['visible']['scroll']['up']=1
            with self.assertRaises(Unavailable):checked(obj)

    def test_complete_projection_canonical_ascii_budget(self):
        checked(sized_project(32768))
        with self.assertRaises(Unavailable):checked(sized_project(32769))
        obj=state(screen='manufacture');obj['visible']['rows']=[dict(handle=f'r{i}',cells=['x'*1024]*5,selected=False) for i in range(32)]
        descriptors(obj,[x['id'] for x in obj['actions']])
        with self.assertRaises(Unavailable):checked(obj)

    def test_retained_presentation_union_and_large_page_boundary(self):
        events=[dict(seq=i+1,kind='presentation',state=screen,visible=state(screen=screen)['visible']) for i,screen in enumerate(NEW_STATES)]
        self.assertEqual(validate(page(events))['events'],events)
        for i in range(len(events)):
            bad=copy.deepcopy(events);bad[i]['visible']['project_id']=0
            with self.assertRaises(Unavailable):validate(page(bad))
        event=dict(seq=1,kind='presentation',state='manufacture',visible=sized_project(30000)['visible'])
        self.assertEqual(validate(page([event],newest=2))['next_cursor'],1)
        with self.assertRaises(Unavailable):validate(page([event,dict(event,seq=2)]))

    def test_no_expanded_quantity_sell_categories_or_generic_cancel(self):
        for action in ('set_quantity','set_sell','assign_all_scientists','assign_all_engineers','cancel_production','cancel_project_info','select_hidden_project'):
            self.assertNotIn(action,a.ACTIONS)
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action))


class ProjectsClientTests(unittest.TestCase):
    def fixture(self,**kwargs):
        peer=NativeFixture(**kwargs);self.addCleanup(peer.close);return peer

    def test_connected_research_path_synthetic_dispatch_not_completion(self):
        transitions={('basescape','open_research'):'research',('research','open_new_research'):'new_research',
          ('new_research','select_research_project'):'research_info',('research_info','cancel_research_project'):'research',
          ('research','select_research_project'):'research_info',('research_info','accept_research'):'research',('research','close_research_list'):'basescape'}
        peer=self.fixture(screen='basescape',transitions=transitions)
        sequence=['open_research','open_new_research','select_research_project','adjust_scientists','adjust_scientists','cancel_research_project',
                  'select_research_project','accept_research','close_research_list']
        for i,action in enumerate(sequence):
            obj=peer.client.observe();reply=peer.client.execute(**arguments(obj,command_id=f'{i+1:032x}',action=action))
            self.assertEqual(reply['semantics'],'dispatch_not_success');self.assertFalse(peer.client.project_rows);self.assertFalse(peer.client.staff_directions)
        self.assertEqual(peer.client.observe()['state'],'basescape');self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),len(sequence))

    def test_connected_manufacture_start_cancel_accept_stop_scopes(self):
        transitions={('basescape','open_manufacture'):'manufacture',('manufacture','open_new_production'):'new_manufacture',
          ('new_manufacture','select_production_project'):'manufacture_start',('manufacture_start','cancel_production_start'):'new_manufacture',
          ('manufacture_start','start_production'):'manufacture_info',('manufacture_info','accept_production'):'manufacture',
          ('manufacture','select_production_project'):'manufacture_info',('manufacture_info','stop_production'):'manufacture',('manufacture','close_production_list'):'basescape'}
        peer=self.fixture(screen='basescape',transitions=transitions)
        sequence=['open_manufacture','open_new_production','select_production_project','cancel_production_start','select_production_project',
                  'start_production','adjust_engineers','accept_production','select_production_project','stop_production','close_production_list']
        for i,action in enumerate(sequence):
            obj=peer.client.observe();self.assertEqual(peer.client.execute(**arguments(obj,command_id=f'{i+1:032x}',action=action))['status'],'dispatched')
        self.assertEqual(peer.client.observe()['state'],'basescape');self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),len(sequence))

    def test_unlisted_project_does_not_consume_current_grant(self):
        for screen,action in a.PROJECT_SELECTION_ACTIONS.items():
            peer=self.fixture(screen=screen);obj=peer.client.observe();count=len(peer.requests)
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action=action,args={'row':'r31'}))
            self.assertEqual(len(peer.requests),count);self.assertEqual(peer.client.capability,obj['capability'])
            self.assertEqual(peer.client.execute(**arguments(obj,action=action))['status'],'dispatched')

    def test_largest_fresh_project_row_and_removed_row(self):
        def expand(op,obj):
            if op=='R':obj['visible']['rows']=[dict(handle=f'r{i}',cells=['Project'],selected=False) for i in range(32)]
            return obj
        peer=self.fixture(screen='new_research',transform=expand);obj=peer.client.observe()
        self.assertEqual(peer.client.execute(**arguments(obj,action='select_research_project',args={'row':'r31'}))['status'],'dispatched')
        self.assertTrue(peer.requests[-1].endswith(b' r31'))
        peer.transform=None;fresh=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,command_id='5'*32,action='select_research_project',args={'row':'r31'}))
        self.assertEqual(peer.client.project_rows,{'r0'})

    def test_same_handle_new_grant_old_request_rejected(self):
        peer=self.fixture(screen='research');old=peer.client.observe();fresh=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='select_research_project'))
        self.assertEqual(peer.client.capability,fresh['capability'])
        self.assertEqual(peer.client.execute(**arguments(fresh,action='select_research_project'))['status'],'dispatched')

    def test_direction_membership_from_current_button_only(self):
        def only_decrease(op,obj):
            if op=='R':
                obj['visible']['buttons']=[x for x in obj['visible']['buttons'] if x['id']!='increase']
                if obj['state']=='manufacture_info':obj['visible']['quantity_controls'][0]['increase']=False
                descriptors(obj,[x['id'] for x in obj['actions']])
            return obj
        for screen,action in a.PROJECT_STAFF_ACTIONS.items():
            peer=self.fixture(screen=screen,transform=only_decrease);obj=peer.client.observe()
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action=action,args={'direction':'increase'}))
            self.assertEqual(peer.client.staff_directions,{'decrease'})
            self.assertEqual(peer.client.execute(**arguments(obj,action=action,args={'direction':'decrease'}))['status'],'dispatched')

    def test_authority_clears_on_state_change_and_close(self):
        peer=self.fixture(screen='research');peer.client.observe();self.assertEqual(peer.client.project_rows,{'r0'})
        peer.screen='research_info';peer.client.observe();self.assertFalse(peer.client.project_rows);self.assertEqual(peer.client.staff_directions,{'increase','decrease'})
        peer.screen='unsupported';peer.client.observe();self.assertFalse(peer.client.project_rows);self.assertFalse(peer.client.staff_directions)
        peer.client.close();self.assertFalse(peer.client.staff_directions)

    def test_exact_duplicate_status_only_changed_row_conflicts(self):
        peer=self.fixture(screen='research');obj=peer.client.observe();args=arguments(obj,action='select_research_project')
        self.assertEqual(peer.client.execute(**args),peer.client.execute(**args));self.assertEqual(peer.requests[-1],('S '+COMMAND).encode())
        with self.assertRaises(Unavailable):peer.client.execute(**dict(args,args={'row':'r1'}))
        self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)

    def test_changed_staff_direction_conflict_and_unknown_no_replay(self):
        peer=self.fixture(screen='research_info');obj=peer.client.observe();args=arguments(obj,action='adjust_scientists')
        peer.client.execute(**args)
        with self.assertRaises(Unavailable):peer.client.execute(**dict(args,args={'direction':'decrease'}))
        self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)
        peer=self.fixture(screen='manufacture_info',transform=lambda op,obj:b'{' if op=='E' else obj);obj=peer.client.observe();args=arguments(obj,action='accept_production')
        with self.assertRaises(a.ActionOutcomeUnknown):peer.client.execute(**args)
        self.assertEqual(peer.client.state,'CLOSED')
        with self.assertRaises(Unavailable):peer.client.execute(**args)
        self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)

    def test_unavailable_clears_grant_and_requires_fresh_context(self):
        def refuse(op,obj):
            if op=='E':obj.update(status='unavailable',observation=None)
            return obj
        peer=self.fixture(screen='research_info',transform=refuse);obj=peer.client.observe()
        reply=peer.client.execute(**arguments(obj,action='adjust_scientists'));self.assertEqual(reply['status'],'unavailable')
        self.assertEqual(peer.client.state,'NEED_CONTEXT');self.assertFalse(peer.client.staff_directions)
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,command_id='5'*32,action='adjust_scientists'))
        self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)

    def test_retained_event_replay_neutral_to_rows_staff_and_grant(self):
        for screen in ('research','manufacture_info'):
            peer=self.fixture(screen=screen);obj=peer.client.observe()
            peer.public_events=[dict(seq=1,kind='presentation',state='new_research',visible=state(screen='new_research')['visible'])]
            saved=(peer.client.capability,peer.client.revision,peer.client.project_rows,peer.client.staff_directions,dict(peer.client.commands))
            first=peer.client.read_events(EPOCH,0,16);second=peer.client.read_events(EPOCH,0,16)
            self.assertEqual(first['events'],second['events']);self.assertEqual(saved,(peer.client.capability,peer.client.revision,peer.client.project_rows,peer.client.staff_directions,dict(peer.client.commands)))
            peer.public_events[0]['visible']['title']='Changed immutable event'
            with self.assertRaises(Unavailable):peer.client.read_events(EPOCH,0,16)
            self.assertEqual(peer.client.state,'CLOSED')

    def test_current_scroll_arrows_for_project_and_material_lists(self):
        for screen in ('research','manufacture_start'):
            def up_only(op,obj):
                if op=='R':
                    obj['visible']['scroll']=dict(up=True,down=False)
                    descriptors(obj,list(dict.fromkeys([x['id'] for x in obj['actions']]+['scroll_visible_list'])))
                return obj
            class ArrowFixture(NativeFixture):
                def observation(self,challenge,capability):
                    return up_only('R',super().observation(challenge,capability))
            peer=ArrowFixture(screen=screen);self.addCleanup(peer.close);obj=peer.client.observe()
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='scroll_visible_list',args={'direction':'down'}))
            self.assertEqual(peer.client.execute(**arguments(obj,action='scroll_visible_list',args={'direction':'up'}))['status'],'dispatched')

    def test_real_server_schema_errors_no_native_dispatch(self):
        peer=self.fixture(screen='research');server=adapter.Server(agent_control=peer.client)
        response=call(server,'read_ui_state')['result'];obj=response['structuredContent'];self.assertEqual(json.loads(response['content'][0]['text']),obj)
        count=len(peer.requests)
        for args in ({'row':'r32'},{'row':'r0','project_id':'private'}):
            reply=call(server,'execute_ui_action',arguments(obj,action='select_research_project',args=args));self.assertEqual(reply['error']['code'],-32602)
        self.assertEqual(len(peer.requests),count)
        reply=call(server,'execute_ui_action',arguments(obj,action='select_research_project'))['result']
        self.assertEqual(reply['structuredContent']['semantics'],'dispatch_not_success');self.assertFalse(reply.get('isError',False))


if __name__=='__main__':unittest.main()
