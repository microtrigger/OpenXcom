# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-19.
"""Synthetic closed wire16 viewport contracts; no raster or live-game proof.

These fabricated socket peers test Python parsing and authority only. Separate
frozen native socket replay is required for the provider/controller boundary.
"""
import copy
import json
import unittest

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate

NEW_ACTIONS = ('view_level_up', 'view_level_down', 'toggle_layers', 'open_minimap',
               'close_minimap', 'minimap_center', 'camera_pan', 'unit_up', 'unit_down')
BATTLE_ACTIONS = set(NEW_ACTIONS) - {'close_minimap', 'minimap_center'}
MINIMAP_ACTIONS = {'view_level_up', 'view_level_down', 'close_minimap', 'minimap_center'}


def descriptor(action, screen):
    return dict(id=action, label=action.replace('_', ' '), label_source=a.label_source(screen, action),
                argument_kind=a.ARGUMENT_KINDS[action])


def minimap():
    return state(screen='minimap')


def maximal_minimap():
    obj = minimap(); visible = obj['visible']
    visible.update(level_label='\x01' * 1024, viewport=dict(x=0,y=0,width=640,height=400),
                   terrain=[[x, y, 1, 1] for y in range(392,400) for x in range(576,640)],
                   markers=[dict(kind='unit_marker',x=x,y=y,width=4,height=4)
                            for y in range(392,396) for x in range(600,632)],
                   targets=[dict(handle=f'm{i}',x=480+i,y=399) for i in range(160)], truncated=True)
    return obj


def validate_state(obj):
    return a.validate_state(obj, CHALLENGE, EPOCH)


class ViewportValidationTests(unittest.TestCase):
    def test_exact_current_version_counts_kinds_and_tool_catalog(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/43')
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS),len(a.definitions())), (231, 99, 7))
        self.assertEqual(a.MINIMAP_TARGET_HANDLES, tuple(f'm{i}' for i in range(160)))
        self.assertEqual(a.CAMERA_DIRECTIONS, ('left','right','up','down'))
        self.assertEqual({x:a.ARGUMENT_KINDS[x] for x in NEW_ACTIONS},
                         dict.fromkeys(set(NEW_ACTIONS)-{'camera_pan','minimap_center'}, 'none') |
                         dict(camera_pan='camera_direction', minimap_center='minimap_target'))
        self.assertEqual(a.STATE_ACTIONS['minimap'], MINIMAP_ACTIONS)
        self.assertEqual(a.STATE_ACTIONS['battlescape'].intersection(NEW_ACTIONS), BATTLE_ACTIONS)
        for screen in a.STATE_ACTIONS.keys()-{'minimap','battlescape'}:
            self.assertFalse(a.STATE_ACTIONS[screen].intersection(NEW_ACTIONS))
        server = adapter.Server(agent_control=object())
        reply = server.handle(dict(jsonrpc='2.0', id=1, method='initialize', params=dict(protocolVersion=adapter.PROTOCOL, capabilities={}, clientInfo=dict(name='test',version='1'))))
        self.assertEqual(reply['result']['serverInfo']['version'],'0.25.0')
        self.assertEqual((a.MAX_COMMANDS,a.EVENT_CAPACITY,a.MAX_REQUEST,a.MAX_TEXT_PACKET,a.MAX_PACKET),
                         (256,256,512,32768,49152))

    def test_every_earlier_wire_rejected_without_downgrade(self):
        for version in range(1,16):
            obj=minimap();obj['schema']=f'openxcom-agent-control/{version}'
            with self.subTest(version=version), self.assertRaises(Unavailable): validate_state(obj)
            hello=dict(schema=obj['schema'],mode=a.MODE,epoch=EPOCH,ledger_capacity=256,command_generation=1,audio_history='mixed_output')
            with self.assertRaises(Unavailable): a.validate_hello(dump(hello))

    def test_new_arguments_canonicalize_without_coordinates_or_hidden_context(self):
        cases=[(x,{},'') for x in NEW_ACTIONS if x not in ('camera_pan','minimap_center')]
        cases += [('camera_pan',dict(direction=x),' '+x) for x in a.CAMERA_DIRECTIONS]
        cases += [('minimap_center',dict(target=x),' '+x) for x in ('m0','m159')]
        for action,args,suffix in cases:
            obj=arguments(action=action,args=args)
            self.assertTrue(a.canonical_action_request(obj).endswith((action+suffix).encode()))
            for extra in ('x','y','z','level','world_position','unit_id','eligible','steps'):
                with self.subTest(action=action,extra=extra), self.assertRaises(Unavailable):
                    a.validate_action_arguments(dict(obj,args=dict(args,**{extra:0})))

    def test_canonical_direction_and_handle_closed_domains(self):
        for action,key,values in [('camera_pan','direction',('north','LEFT','left ',0,True,None,[])),
                 ('minimap_center','target',('m160','m-1','m00','M0','m0\n','g0',0,True,None,{}))]:
            for value in values:
                with self.subTest(action=action,value=value), self.assertRaises(Unavailable):
                    a.validate_action_arguments(arguments(action=action,args={key:value}))
        tool=next(d for d in a.definitions() if d['name']=='execute_ui_action')['inputSchema']
        branches={tuple(x['properties']['action']['enum']):x['properties']['args'] for x in tool['oneOf']}
        self.assertEqual(branches[('camera_pan',)]['properties']['direction']['enum'], list(a.CAMERA_DIRECTIONS))
        self.assertEqual(branches[('minimap_center',)]['properties']['target']['enum'][-1], 'm159')

    def test_layers_indicator_is_required_strict_null_or_display_mode(self):
        for value in (None,1,2):
            obj=state();obj['visible']['layers_indicator']=value;validate_state(obj)
        for value in (0,3,-1,True,False,1.0,'1',[],{}):
            obj=state();obj['visible']['layers_indicator']=value
            with self.subTest(value=value), self.assertRaises(Unavailable): validate_state(obj)
        obj=state();del obj['visible']['layers_indicator']
        with self.assertRaises(Unavailable): validate_state(obj)
        obj=state();obj['visible']['view_level']=1
        with self.assertRaises(Unavailable): validate_state(obj)

    def test_camera_pan_requires_current_nonempty_map_grid(self):
        obj=state();obj['actions'].append(descriptor('camera_pan','battlescape'));validate_state(obj)
        # Native projection emits paired grid/context cells. Make a coherent
        # empty viewport so this test reaches the camera-authority predicate.
        obj['visible']['tactical']['grid']=[]
        obj['visible']['tactical']['map_context']['cells']=[]
        obj['visible']['focus_target']=None
        obj['actions']=[x for x in obj['actions'] if x['id'] not in ('map_primary','map_secondary')]
        with self.assertRaises(Unavailable): validate_state(obj)
        obj['actions']=[x for x in obj['actions'] if x['id']!='camera_pan'];validate_state(obj)

    def test_map_context_must_match_current_grid_even_without_camera_action(self):
        obj=state()
        obj['actions']=[x for x in obj['actions'] if x['id']!='camera_pan']
        validate_state(obj)
        mismatched=copy.deepcopy(obj)
        mismatched['visible']['tactical']['map_context']['cells']=[]
        with self.assertRaises(Unavailable): validate_state(mismatched)
        mismatched=copy.deepcopy(obj)
        mismatched['visible']['tactical']['grid']=[]
        mismatched['visible']['focus_target']=None
        mismatched['actions']=[x for x in mismatched['actions'] if x['id'] not in ('map_primary','map_secondary')]
        with self.assertRaises(Unavailable): validate_state(mismatched)

    def test_map_context_handles_follow_current_grid_order(self):
        obj=state();tactical=obj['visible']['tactical']
        # A known-valid native-shaped fixture has multiple corresponding cells.
        self.assertGreaterEqual(len(tactical['grid']),2)
        validate_state(obj)
        tactical['map_context']['cells'].reverse()
        with self.assertRaises(Unavailable): validate_state(obj)
        tactical['map_context']['cells'].reverse()
        validate_state(obj)
        tactical['map_context']['cells'][0]['handle']='g127'
        with self.assertRaises(Unavailable): validate_state(obj)

    def test_new_actions_are_semantic_descriptors_only_and_screen_scoped(self):
        for screen,actions in [('battlescape',BATTLE_ACTIONS),('minimap',MINIMAP_ACTIONS)]:
            obj=state(screen=screen);obj['actions']=[descriptor(x,screen) for x in sorted(actions)];validate_state(obj)
            for key,value in [('label_source','visible_text'),('argument_kind','point')]:
                bad=copy.deepcopy(obj);bad['actions'][0][key]=value
                with self.subTest(screen=screen,key=key), self.assertRaises(Unavailable): validate_state(bad)
            for action in set(NEW_ACTIONS)-actions:
                bad=copy.deepcopy(obj);bad['actions']=[descriptor(action,screen)]
                with self.assertRaises(Unavailable): validate_state(bad)

    def test_exact_required_visible_fields_reject_private_or_inferred_metadata(self):
        for field in minimap()['visible']:
            obj=minimap();del obj['visible'][field]
            with self.subTest(missing=field), self.assertRaises(Unavailable): validate_state(obj)
        for field in ('world_position','tile','unit_id','faction','selected_actor','known_items',
                      'eligibility','traversable','camera_center','level','source','total_markers'):
            obj=minimap();obj['visible'][field]=None
            with self.subTest(extra=field), self.assertRaises(Unavailable): validate_state(obj)

    def test_nullable_level_label_utf8_bound_and_no_numeric_inference(self):
        for value in (None,'','É'*512,'\x01'*1024,'Level 3'):
            obj=minimap();obj['visible']['level_label']=value;validate_state(obj)
        for value in ('É'*513,'x'*1025,'\ud800',3,False,[],{}):
            obj=minimap();obj['visible']['level_label']=value
            with self.subTest(value=repr(value)[:30]), self.assertRaises(Unavailable): validate_state(obj)

    def test_viewport_positive_contained_and_exact(self):
        obj=minimap();obj['visible']['viewport']=dict(x=320,y=240,width=320,height=160);validate_state(obj)
        for key,value in [('x',-1),('x',641),('y',400),('width',0),('height',0),('width',641),
                          ('x',True),('height',160.0),('world_x',0)]:
            obj=minimap();obj['visible']['viewport'][key]=value
            with self.subTest(key=key,value=value), self.assertRaises(Unavailable): validate_state(obj)

    def test_unavailable_keeps_controls_but_no_geometry_targets_or_truncation(self):
        for viewport in (None,dict(x=0,y=0,width=320,height=160)):
            obj=minimap();obj['visible'].update(viewport=viewport,terrain=[],markers=[],targets=[],
                                               content_status='unavailable',truncated=False)
            obj['actions']=[x for x in obj['actions'] if x['id']!='minimap_center'];validate_state(obj)
            for field,value in [('terrain',[[0,0,1,1]]),('targets',[dict(handle='m0',x=1,y=1)]),
                                ('truncated',True)]:
                bad=copy.deepcopy(obj);bad['visible'][field]=value
                with self.subTest(field=field), self.assertRaises(Unavailable): validate_state(bad)
            obj['actions'].append(descriptor('minimap_center','minimap'))
            with self.assertRaises(Unavailable): validate_state(obj)

    def test_navigation_anchors_alone_cannot_claim_bounded_content(self):
        obj=minimap();obj['visible']['terrain']=[]
        with self.assertRaises(Unavailable): validate_state(obj)
        obj['visible']['markers']=[dict(kind='item_marker',x=0,y=0,width=4,height=4)];validate_state(obj)
        obj['visible']['viewport']=None
        with self.assertRaises(Unavailable): validate_state(obj)

    def test_terrain_strict_integer_positive_rectangles_contained_locally(self):
        for rectangle in ((),{},[0,0,4],[0,0,4,4,1],[-1,0,1,1],[0,-1,1,1],
                          [319,0,2,1],[0,159,1,2],[0,0,0,1],[0,0,1,0],
                          [True,0,1,1],[0,0,1.0,1]):
            obj=minimap();obj['visible']['terrain']=[rectangle]
            with self.subTest(rectangle=rectangle), self.assertRaises(Unavailable): validate_state(obj)
        obj=minimap();obj['visible']['terrain']=[[319,159,1,1]];validate_state(obj)

    def test_terrain_canonical_order_no_duplicates_or_pixel_overlap(self):
        for rectangles in ([[0,1,1,1],[0,0,1,1]], [[1,0,1,1],[0,0,1,1]],
                           [[0,0,1,1],[0,0,1,1]], [[0,0,4,4],[3,3,4,4]],
                           [[0,0,10,10],[5,5,1,1]]):
            obj=minimap();obj['visible']['terrain']=rectangles
            with self.subTest(rectangles=rectangles), self.assertRaises(Unavailable): validate_state(obj)
        obj=minimap();obj['visible']['terrain']=[[0,0,4,4],[4,0,4,4],[0,4,8,4]];validate_state(obj)

    def test_marker_closed_type_exact_four_pixel_bounds_and_no_identity(self):
        marker=dict(kind='unit_marker',x=316,y=156,width=4,height=4)
        obj=minimap();obj['visible']['markers']=[marker];validate_state(obj)
        for key,value in [('kind','enemy'),('kind','terrain'),('x',317),('y',157),('width',3),
                          ('height',5),('width',True),('x',0.0),('unit_id',None),('faction',None)]:
            bad=copy.deepcopy(obj);bad['visible']['markers'][0][key]=value
            with self.subTest(key=key,value=value), self.assertRaises(Unavailable): validate_state(bad)

    def test_marker_order_duplicates_and_cross_kind_overlap(self):
        def marker(kind,x=0,y=0):return dict(kind=kind,x=x,y=y,width=4,height=4)
        obj=minimap();obj['visible']['markers']=[marker('item_marker'),marker('unit_marker')];validate_state(obj)
        # Markers may also overlap terrain or different same-kind footprints.
        obj['visible']['markers'].append(marker('unit_marker',1));validate_state(obj)
        for records in ([marker('unit_marker'),marker('item_marker')],
                         [marker('unit_marker'),marker('unit_marker')],
                         [marker('unit_marker',1),marker('unit_marker',0)]):
            bad=minimap();bad['visible']['markers']=records
            with self.subTest(records=records), self.assertRaises(Unavailable): validate_state(bad)

    def test_target_sequential_handles_unique_local_points_and_membership(self):
        for target in (dict(handle='m1',x=0,y=0),dict(handle='m00',x=0,y=0),
                       dict(handle='m0',x=-1,y=0),dict(handle='m0',x=320,y=0),
                       dict(handle='m0',x=0,y=160),dict(handle='m0',x=True,y=0),
                       dict(handle='m0',x=0,y=0,tile=1)):
            obj=minimap();obj['visible']['targets']=[target]
            with self.subTest(target=target), self.assertRaises(Unavailable): validate_state(obj)
        obj=minimap();obj['visible']['targets'].append(dict(handle='m1',x=2,y=2))
        with self.assertRaises(Unavailable): validate_state(obj)
        obj=minimap();obj['visible']['targets']=[]
        with self.assertRaises(Unavailable): validate_state(obj)
        obj['actions']=[x for x in obj['actions'] if x['id']!='minimap_center'];validate_state(obj)

    def test_full_closed_counts_fit_ascii_packet_and_limits_reject(self):
        obj=maximal_minimap();validate_state(obj)
        self.assertLess(len(json.dumps(obj,ensure_ascii=True,separators=(',',':')).encode('ascii')),a.MAX_TEXT_PACKET)
        for field,value in [('terrain',[[0,0,1,1]]*513),('markers',[{}]*129),('targets',[{}]*161),
                            ('truncated',0),('truncated',None),('content_status','complete')]:
            bad=minimap();bad['visible'][field]=value
            with self.subTest(field=field), self.assertRaises(Unavailable): validate_state(bad)

    def test_canonical_ascii_state_budget_exact_boundary(self):
        obj=maximal_minimap()
        size=lambda:len(json.dumps(obj,ensure_ascii=True,separators=(',',':')).encode('ascii'))
        remaining=a.MAX_TEXT_PACKET-size()
        for action in obj['actions']:
            addition=min(4096-len(action['label']),remaining)
            action['label']+='x'*addition;remaining-=addition
        self.assertEqual(remaining,0);self.assertEqual(size(),a.MAX_TEXT_PACKET);validate_state(obj)
        next(x for x in obj['actions'] if len(x['label'])<4096)['label']+='x'
        with self.assertRaises(Unavailable): validate_state(obj)

    def test_large_presentation_page_rejects_overflow_skips_and_false_cursor(self):
        event=dict(seq=1,kind='presentation',state='minimap',visible=maximal_minimap()['visible'])
        second=copy.deepcopy(event);second['seq']=2
        oversized=page([event,second])
        self.assertGreater(len(dump(oversized)),a.MAX_PACKET)
        with self.assertRaises(Unavailable):validate(oversized)
        first=page([event]);first['newest']=2
        self.assertEqual(validate(first)['next_cursor'],1)
        for change in (dict(next_cursor=2),dict(events=[]),dict(gap=True)):
            with self.subTest(change=change), self.assertRaises(Unavailable):validate(dict(first,**change))
        second_page=page([second],after=1)
        self.assertEqual(validate(second_page,after=1)['next_cursor'],2)

    def test_presentation_event_uses_identical_minimap_validator_and_no_authority(self):
        event=dict(seq=1,kind='presentation',state='minimap',visible=maximal_minimap()['visible'])
        self.assertEqual(validate(page([event]))['events'][0],event)
        event['visible']['markers'][0]['actor']=None
        with self.assertRaises(Unavailable): validate(page([event]))
        event=dict(seq=1,kind='presentation',state='minimap',visible=minimap()['visible'],capability='1'*32)
        with self.assertRaises(Unavailable): validate(page([event]))


class ViewportClientTests(unittest.TestCase):
    def fixture(self,**kwargs):
        native=NativeFixture(screen='minimap',**kwargs);self.addCleanup(native.close);return native

    def test_unlisted_target_rejects_before_send_without_spending_grant(self):
        native=self.fixture();obj=native.client.observe();before=len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**arguments(obj,action='minimap_center',args=dict(target='m1')))
        self.assertEqual(len(native.requests),before);self.assertEqual(native.client.capability,obj['capability'])
        self.assertEqual(native.client.execute(**arguments(obj,action='minimap_center'))['status'],'dispatched')
        self.assertEqual(native.client.minimap_targets,frozenset())

    def test_fresh_context_replaces_membership_old_grant_rejected(self):
        native=self.fixture();old=native.client.observe();fresh=native.client.observe()
        with self.assertRaises(Unavailable):native.client.execute(**arguments(old,action='minimap_center'))
        self.assertEqual(native.client.execute(**arguments(fresh,action='minimap_center'))['status'],'dispatched')
        self.assertEqual(sum(x.startswith(b'E ') for x in native.requests),1)

    def test_new_state_discards_old_minimap_membership(self):
        native=self.fixture();native.client.observe();native.screen='inventory';obj=native.client.observe()
        self.assertEqual(native.client.minimap_targets,frozenset())
        with self.assertRaises(Unavailable):native.client.execute(**arguments(obj,action='minimap_center'))
        self.assertFalse(any(x.startswith(b'E ') for x in native.requests))

    def test_exact_duplicate_queries_receipt_changed_target_conflicts(self):
        native=self.fixture();obj=native.client.observe();request=arguments(obj,action='minimap_center')
        first=native.client.execute(**request);self.assertEqual(native.client.execute(**request),first)
        before=len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**dict(request,args=dict(target='m1')))
        self.assertEqual(len(native.requests),before)
        self.assertEqual(sum(x.startswith(b'E ') for x in native.requests),1)
        self.assertEqual(sum(x.startswith(b'S ') for x in native.requests),1)

    def test_public_minimap_event_reads_do_not_consume_action_grant(self):
        native=self.fixture();obj=native.client.observe()
        native.public_events=[dict(seq=1,kind='presentation',state='minimap',visible=obj['visible'])]
        event1=native.client.read_events(EPOCH,0,16);event2=native.client.read_events(EPOCH,0,16)
        self.assertEqual(event1['events'],event2['events']);self.assertEqual(native.client.capability,obj['capability'])
        self.assertEqual(len(native.client.commands),0)
        self.assertEqual(native.client.execute(**arguments(obj,action='minimap_center'))['status'],'dispatched')

    def test_changed_retained_minimap_record_closes_channel(self):
        native=self.fixture();obj=native.client.observe()
        native.public_events=[dict(seq=1,kind='presentation',state='minimap',visible=copy.deepcopy(obj['visible']))]
        native.client.read_events(EPOCH,0,16);native.public_events[0]['visible']['level_label']='Level 2'
        with self.assertRaises(Unavailable):native.client.read_events(EPOCH,0,16)
        self.assertEqual(native.client.state,'CLOSED');self.assertFalse(native.client.minimap_targets)

    def test_unknown_after_send_is_terminal_without_replay_or_reconnect(self):
        native=self.fixture(transform=lambda kind,obj:None if kind=='E' else obj)
        obj=native.client.observe();request=arguments(obj,action='minimap_center')
        with self.assertRaises(a.ActionOutcomeUnknown):native.client.execute(**request)
        self.assertEqual(native.client.state,'CLOSED')
        with self.assertRaises(Unavailable):native.client.execute(**request)
        self.assertEqual(sum(x.startswith(b'E ') for x in native.requests),1)
        self.assertFalse(native.client.minimap_targets)

    def test_multiple_large_pages_keep_sequence_projection_truncation_and_grant_separate(self):
        native=self.fixture();obj=native.client.observe();native.event_page_limit=1
        visible=maximal_minimap()['visible']
        native.public_events=[dict(seq=i,kind='presentation',state='minimap',visible=copy.deepcopy(visible)) for i in range(1,4)]
        authority=(native.client.capability,native.client.revision,native.client.event_sequence)
        events=[];cursor=0
        for i in range(3):
            page1=native.client.read_events(EPOCH,cursor,16)
            self.assertEqual(len(page1['events']),1);self.assertFalse(page1['gap'])
            self.assertTrue(page1['events'][0]['visible']['truncated'])
            replay=native.client.read_events(EPOCH,cursor,16)
            self.assertEqual(replay['events'],page1['events'])
            events+=page1['events'];cursor=page1['next_cursor']
        self.assertEqual([x['seq'] for x in events],[1,2,3])
        self.assertEqual(authority,(native.client.capability,native.client.revision,native.client.event_sequence))
        self.assertFalse(native.client.read_events(EPOCH,cursor,16)['events'])
        self.assertEqual(native.client.execute(**arguments(obj,action='minimap_center'))['status'],'dispatched')

    def test_last_minimap_handle_is_actionable_only_when_freshly_listed(self):
        def transform(kind,obj):
            if kind=='R': obj['visible']=maximal_minimap()['visible']
            return obj
        native=self.fixture(transform=transform);obj=native.client.observe()
        self.assertEqual(len(native.client.minimap_targets),160)
        self.assertEqual(native.client.execute(**arguments(obj,action='minimap_center',args=dict(target='m159')))['status'],'dispatched')
        self.assertTrue(next(x for x in native.requests if x.startswith(b'E ')).endswith(b' minimap_center m159'))

    def test_each_direction_and_new_none_command_uses_normal_single_admission(self):
        class BattleFixture(NativeFixture):
            def observation(self,challenge,capability):
                obj=super().observation(challenge,capability)
                obj['actions'] += [descriptor(x,'battlescape') for x in sorted(BATTLE_ACTIONS)]
                return obj
        native=BattleFixture();self.addCleanup(native.close)
        cases=[('camera_pan',dict(direction=value)) for value in a.CAMERA_DIRECTIONS]
        cases += [(value,{}) for value in sorted(BATTLE_ACTIONS-{'camera_pan'})]
        for i,(action,args) in enumerate(cases):
            obj=native.client.observe()
            receipt=native.client.execute(**arguments(obj,command_id=f'{100+i:032x}',action=action,args=args))
            self.assertEqual(receipt['status'],'dispatched')
        self.assertEqual(sum(x.startswith(b'E ') for x in native.requests),len(cases))
        self.assertEqual(len(native.client.commands),len(cases))

    def test_unavailable_minimap_has_no_center_membership_but_close_can_dispatch(self):
        class EmptyFixture(NativeFixture):
            def observation(self,challenge,capability):
                obj=super().observation(challenge,capability)
                obj['visible'].update(viewport=None,terrain=[],markers=[],targets=[],content_status='unavailable')
                obj['actions']=[x for x in obj['actions'] if x['id']!='minimap_center']
                return obj
        native=EmptyFixture(screen='minimap');self.addCleanup(native.close)
        obj=native.client.observe();self.assertFalse(native.client.minimap_targets)
        with self.assertRaises(Unavailable):native.client.execute(**arguments(obj,action='minimap_center'))
        self.assertEqual(native.client.execute(**arguments(obj,action='close_minimap'))['status'],'dispatched')

    def test_server_malformed_target_is_invalid_params_not_native_receipt(self):
        native=self.fixture();server=adapter.Server(agent_control=native.client)
        observed=call(server,'read_ui_state')['result']['structuredContent']
        reply=call(server,'execute_ui_action',arguments(observed,action='minimap_center',args=dict(target='m160')))
        self.assertEqual(reply['error']['code'],-32602)
        self.assertFalse(any(x.startswith(b'E ') for x in native.requests))


if __name__=='__main__':unittest.main()
