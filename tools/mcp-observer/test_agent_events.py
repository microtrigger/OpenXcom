# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
"""Synthetic /5 event/tactical contracts; no game, renderer or audio-device proof."""
import copy
import json
import subprocess
import sys
from pathlib import Path
import unittest
from unittest.mock import patch
import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, result, arguments, dump, call, EPOCH, CHALLENGE, COMMAND


def tactical(mode='map'):
    obj=copy.deepcopy(state()['visible']['tactical'])
    if mode=='map':
        obj['grid']=[dict(handle='g0',polygon=[dict(x=8,y=0),dict(x=16,y=8),dict(x=8,y=16),dict(x=0,y=8)],target=dict(x=8,y=8))]
        obj['fragment_status']='presented'
        obj['fragments']=[dict(handle='p0',kind='painted_fragment',classification=None,runs=[dict(x=5,y=6,width=3)])]
    else:
        obj.update(mode=mode,grid=[],fragments=[],fragment_status='unavailable',message='Hidden Movement' if mode=='hidden_movement' else None)
        if mode=='unavailable': obj['viewport']=None
    return obj


def presentation(seq=1,screen='battlescape'):
    return dict(seq=seq,kind='presentation',state=screen,visible=state(screen=screen)['visible'])


def audio(seq=1,nonzero=True):
    return dict(seq=seq,kind='audio_output',duration_ms=100,nonzero=nonzero,source=None,sound=None,position=None)


def page(events=(),after=0,newest=None,history='mixed_output',challenge=CHALLENGE):
    events=copy.deepcopy(list(events));newest=(events[-1]['seq'] if events else after) if newest is None else newest
    oldest=max(1,newest-a.EVENT_CAPACITY+1)
    return dict(schema=a.SCHEMA,mode=a.MODE,type='events',challenge=challenge,epoch=EPOCH,audio_history=history,
        audio_status='active' if history=='mixed_output' else 'unavailable',
        after=after,oldest=oldest,newest=newest,next_cursor=events[-1]['seq'] if events else after,
        gap=after+1<oldest,events=events)


def validate(obj,after=0,limit=16,history='mixed_output'):
    return a.validate_events(dump(obj),CHALLENGE,EPOCH,after,limit,history)


class TacticalValidationTests(unittest.TestCase):
    def test_three_modes_and_explicit_unclassified_geometry(self):
        for mode in ('map','hidden_movement','unavailable'):
            with self.subTest(mode=mode): a.validate_tactical(tactical(mode))
        obj=state();obj['visible']['tactical']=tactical();a.validate_state(obj,CHALLENGE,EPOCH)
        self.assertIn('end_turn',a.STATE_ACTIONS['battlescape'])
        self.assertEqual(a.ARGUMENT_KINDS['end_turn'],'none')

    def test_hidden_movement_and_unavailable_never_export_geometry(self):
        for mode in ('hidden_movement','unavailable'):
            for key in ('grid','fragments'):
                obj=tactical(mode);obj[key]=tactical()[key]
                with self.subTest(mode=mode,key=key),self.assertRaises(Unavailable):a.validate_tactical(obj)
        for key,value in (('viewport',dict(x=0,y=0,width=320,height=140)),('message','private')):
            obj=tactical('unavailable');obj[key]=value
            with self.subTest(key=key),self.assertRaises(Unavailable):a.validate_tactical(obj)

    def test_closed_tactical_fields_null_identity_and_modes(self):
        for key,value in (('world_id',1),('view_level',0),('mode','enemy_turn'),('fragment_status','guessed'),
                          ('truncated',1),('message','not a map overlay')):
            obj=tactical();obj[key]=value
            with self.subTest(key=key),self.assertRaises(Unavailable):a.validate_tactical(obj)
        for key,value in (('classification','alien'),('kind','enemy'),('unit_id',3)):
            obj=tactical();obj['fragments'][0][key]=value
            with self.subTest(key=key),self.assertRaises(Unavailable):a.validate_tactical(obj)

    def test_viewport_and_every_coordinate_are_strict_local_integers(self):
        for key,bad in (('x',-1),('y',4096),('width',0),('width',641),('height',401),('width',True)):
            obj=tactical();obj['viewport'][key]=bad
            with self.subTest(key=key,bad=bad),self.assertRaises(Unavailable):a.validate_tactical(obj)
        for coordinate in (-1,320,1.0,True):
            for collection in ('target','polygon','runs'):
                obj=tactical()
                point=obj['grid'][0]['target'] if collection=='target' else obj['grid'][0]['polygon'][0] if collection=='polygon' else obj['fragments'][0]['runs'][0]
                point['x']=coordinate
                with self.subTest(collection=collection,coordinate=coordinate),self.assertRaises(Unavailable):a.validate_tactical(obj)
        obj=tactical();obj['fragments'][0]['runs'][0].update(x=319,width=2)
        with self.assertRaises(Unavailable):a.validate_tactical(obj)

    def test_frame_local_handles_and_global_run_bounds(self):
        for key,bad in (('grid','g1'),('fragments','p17')):
            obj=tactical();obj[key][0]['handle']=bad
            with self.subTest(key=key),self.assertRaises(Unavailable):a.validate_tactical(obj)
        for count in (0,3,5):
            obj=tactical();obj['grid'][0]['polygon']=[dict(x=0,y=0)]*count
            with self.subTest(count=count),self.assertRaises(Unavailable):a.validate_tactical(obj)
        obj=tactical();obj['fragment_status']='unavailable'
        with self.assertRaises(Unavailable):a.validate_tactical(obj)
        obj=tactical();obj['fragments'][0]['runs']*=512;a.validate_tactical(obj)
        obj['fragments'].append(dict(handle='p1',kind='painted_fragment',classification=None,runs=[dict(x=0,y=0,width=1)]))
        with self.assertRaises(Unavailable):a.validate_tactical(obj)
        obj=tactical();obj['fragments'][0]['runs']=[]
        with self.assertRaises(Unavailable):a.validate_tactical(obj)


class EventValidationTests(unittest.TestCase):
    def test_audio_lifecycle_and_current_status_are_separate_from_old_page(self):
        events=[dict(seq=1,kind='audio_status',status='pending',partial_window_discarded=False),
                dict(seq=2,kind='audio_status',status='active',partial_window_discarded=False),audio(3),
                dict(seq=4,kind='audio_status',status='unavailable',partial_window_discarded=True)]
        obj=page(events);obj['audio_status']='unavailable';validate(obj)
        old=page(events[:2],newest=4);old['audio_status']='unavailable';validate(old)
        for status,partial in (('reset',False),('pending',True),('active',True),('unavailable',1)):
            event=dict(seq=1,kind='audio_status',status=status,partial_window_discarded=partial)
            with self.subTest(status=status,partial=partial),self.assertRaises(Unavailable):validate(page([event]))
        for value in ('attached',None,True):
            obj=page();obj['audio_status']=value
            with self.subTest(value=value),self.assertRaises(Unavailable):validate(obj)

    def test_exact_request_args_and_no_authority(self):
        a.validate_event_arguments(dict(epoch=EPOCH,after=0,limit=16))
        for obj in ({},{'epoch':EPOCH,'after':False,'limit':1},{'epoch':EPOCH,'after':-1,'limit':1},
                    {'epoch':EPOCH,'after':2**53,'limit':1},{'epoch':EPOCH,'after':0,'limit':True},
                    {'epoch':EPOCH,'after':0,'limit':0},{'epoch':EPOCH,'after':0,'limit':17},
                    {'epoch':EPOCH,'after':0,'limit':1,'capability':'2'*32}):
            with self.subTest(obj=obj),self.assertRaises(Unavailable):a.validate_event_arguments(obj)

    def test_empty_and_partial_byte_bounded_page(self):
        validate(page())
        validate(page([presentation()],newest=8))
        validate(page([presentation(i) for i in range(1,17)]))
        validate(page([],after=8,newest=8),after=8)

    def test_public_ring_gap_and_audio_callback_gap_are_distinct(self):
        obj=page([presentation(257)],newest=512);validate(obj)
        self.assertTrue(obj['gap']);self.assertEqual(obj['oldest'],257)
        obj=page([dict(seq=1,kind='audio_gap',dropped_windows=27),audio(2,False)]);validate(obj)
        self.assertFalse(obj['gap'])
        for bad in (0,-1,False,1.0,2**53):
            changed=copy.deepcopy(obj);changed['events'][0]['dropped_windows']=bad
            with self.subTest(bad=bad),self.assertRaises(Unavailable):validate(changed)

    def test_response_identity_echo_cursors_and_closed_keys(self):
        for key,value in (('schema','openxcom-agent-control/4'),('mode','supervised'),('type','state'),
                          ('epoch','5'*32),('challenge','5'*32),('after',1),('oldest',0),('oldest',2),
                          ('newest',True),('newest',-1),('next_cursor',0),('gap',1),('gap',True),
                          ('audio_history','unavailable'),('capability','2'*32)):
            obj=page([presentation()]);obj[key]=value
            with self.subTest(key=key,value=value),self.assertRaises(Unavailable):validate(obj)
        with self.assertRaises(Unavailable):validate(page([],after=2,newest=1),after=2)
        with self.assertRaises(Unavailable):validate(page([],newest=1))

    def test_event_sequence_no_holes_duplicates_or_future_records(self):
        for events in ([presentation(2)],[presentation(1),presentation(1)],
                       [presentation(1),presentation(3)],[presentation(1),presentation(2)]):
            obj=page(events,newest=2)
            with self.subTest(events=events),self.assertRaises(Unavailable):validate(obj,limit=1)

    def test_events_never_mint_actions_or_private_presentation_fields(self):
        for key,value in (('actions',[]),('capability',None),('revision',3),('actor_id',7)):
            event=presentation();event[key]=value
            with self.subTest(key=key),self.assertRaises(Unavailable):validate(page([event]))
        obj=page([presentation()]);obj['events'][0]['visible']['hidden_unit']='PRIVATE'
        with self.assertRaises(Unavailable):validate(obj)

    def test_audio_is_digital_output_only_and_capability_bound(self):
        for nonzero in (True,False):validate(page([audio(nonzero=nonzero)]))
        for key,value in (('duration_ms',101),('duration_ms',100.0),('nonzero',1),('source','enemy'),
                          ('sound','shot'),('position',{'x':1,'y':2}),('channel_id',1)):
            event=audio();event[key]=value
            with self.subTest(key=key),self.assertRaises(Unavailable):validate(page([event]))
        validate(page([presentation()],history='unavailable'),history='unavailable')
        for event in (audio(),dict(seq=1,kind='audio_gap',dropped_windows=1)):
            with self.assertRaises(Unavailable):validate(page([event],history='unavailable'),history='unavailable')

    def test_event_and_mcp_ascii_expansion_budgets(self):
        events=[]
        for seq in range(1,9):
            event=presentation(seq,'main_menu');event['visible']['new_battle_label']='\U0001f600'*1000;events.append(event)
        raw=json.dumps(page(events),ensure_ascii=False,separators=(',',':')).encode('utf-8')
        self.assertLess(len(raw),a.MAX_PACKET)
        with self.assertRaises(Unavailable):a.validate_events(raw,CHALLENGE,EPOCH,0,16,'mixed_output')
        event=presentation(screen='new_battle')
        event['visible']={key:'\U0001f600'*1000 for key in a.CHOICE_CONTROLS}
        raw=json.dumps(page([event]),ensure_ascii=False,separators=(',',':')).encode('utf-8')
        self.assertLess(len(raw),a.MAX_PACKET)
        with self.assertRaises(Unavailable):a.validate_events(raw,CHALLENGE,EPOCH,0,16,'mixed_output')


class EventClientTests(unittest.TestCase):
    def fixture(self,**kwargs):
        native=NativeFixture(**kwargs);self.addCleanup(native.close);return native

    def append(self,native,event):
        event=copy.deepcopy(event);event['seq']=len(native.public_events)+1;native.public_events.append(event)

    def test_event_read_cannot_initialize_or_accept_wrong_epoch(self):
        native=self.fixture()
        with self.assertRaises(Unavailable):native.client.read_events(EPOCH,0,1)
        self.assertEqual(native.requests,[])
        native.client.observe();before=len(native.requests)
        with self.assertRaises(Unavailable):native.client.read_events('9'*32,0,1)
        self.assertEqual(len(native.requests),before);self.assertEqual(native.client.state,'READY')

    def test_canonical_v_request_preserves_every_action_context_field(self):
        native=self.fixture();observed=native.client.observe();self.append(native,presentation())
        keys=('state','capability','revision','event_sequence','capability_deadline','available_actions')
        before={key:getattr(native.client,key) for key in keys}
        with patch('agent_control.secrets.token_hex',return_value=CHALLENGE):obj=native.client.read_events(EPOCH,0,16)
        self.assertEqual(native.requests[-1],f'V {CHALLENGE} {EPOCH} 0 16'.encode())
        self.assertEqual(before,{key:getattr(native.client,key) for key in keys});self.assertEqual(native.client.commands,{})
        self.assertEqual(obj['audio_history'],'mixed_output')
        self.assertEqual(native.client.execute(**arguments(observed,action='end_turn'))['status'],'dispatched')

    def test_history_retained_independently_and_exact_public_ring_gap(self):
        native=self.fixture();native.client.observe()
        for _ in range(300):self.append(native,audio(nonzero=False))
        obj=native.client.read_events(EPOCH,0,16)
        self.assertEqual((obj['oldest'],obj['newest'],obj['next_cursor']),(45,300,60));self.assertTrue(obj['gap'])
        self.assertEqual(len(obj['events']),16)
        again=native.client.read_events(EPOCH,44,16);self.assertFalse(again['gap']);self.assertEqual(again['events'],obj['events'])

    def test_callback_gap_survives_read_as_its_own_event(self):
        native=self.fixture();native.client.observe()
        self.append(native,dict(kind='audio_gap',dropped_windows=19));self.append(native,audio())
        obj=native.client.read_events(EPOCH,0,16);self.assertFalse(obj['gap'])
        self.assertEqual(obj['events'][0]['dropped_windows'],19)

    def test_terminal_audio_status_remains_visible_after_ring_eviction(self):
        native=self.fixture();native.client.observe()
        self.append(native,dict(kind='audio_status',status='unavailable',partial_window_discarded=True))
        native.audio_status='unavailable'
        for _ in range(300):self.append(native,presentation())
        obj=native.client.read_events(EPOCH,0,16)
        self.assertTrue(obj['gap']);self.assertEqual(obj['audio_status'],'unavailable')
        self.assertEqual(obj['audio_history'],'mixed_output')
        self.assertFalse(any(event['kind']=='audio_status' for event in obj['events']))

    def test_partial_pages_and_new_events_keep_retained_prefix_identical(self):
        native=self.fixture();native.event_page_limit=1;native.client.observe()
        self.append(native,presentation());self.append(native,audio())
        first=native.client.read_events(EPOCH,0,16);self.assertEqual(len(first['events']),1)
        self.append(native,audio(nonzero=False));again=native.client.read_events(EPOCH,0,16)
        self.assertEqual(first['events'],again['events']);self.assertGreater(again['newest'],first['newest'])
        self.assertEqual(native.client.read_events(EPOCH,1,16)['next_cursor'],2)

    def test_changed_retained_record_and_counter_regression_close(self):
        native=self.fixture();native.client.observe();self.append(native,audio())
        native.client.read_events(EPOCH,0,16);native.public_events[0]['nonzero']=False
        with self.assertRaises(Unavailable):native.client.read_events(EPOCH,0,16)
        self.assertEqual(native.client.state,'CLOSED')
        native=self.fixture();native.client.observe();self.append(native,audio());native.client.read_events(EPOCH,0,16)
        native.public_events=[]
        with self.assertRaises(Unavailable):native.client.read_events(EPOCH,0,16)
        self.assertEqual(native.client.state,'CLOSED')

    def test_mutating_returned_history_does_not_change_cached_identity(self):
        native=self.fixture();native.client.observe();self.append(native,audio())
        obj=native.client.read_events(EPOCH,0,16);obj['events'][0]['nonzero']=False
        self.assertTrue(native.client.read_events(EPOCH,0,16)['events'][0]['nonzero'])

    def test_read_failure_is_generic_terminal_never_action_uncertainty(self):
        for transform in (lambda op,obj:None if op=='V' else obj,
                          lambda op,obj:b'{broken' if op=='V' else obj):
            native=self.fixture(transform=transform);observed=native.client.observe()
            with self.assertRaises(Unavailable) as caught:native.client.read_events(EPOCH,0,16)
            self.assertNotIsInstance(caught.exception,a.ActionOutcomeUnknown)
            count=len(native.requests)
            with self.assertRaises(Unavailable):native.client.execute(**arguments(observed,action='end_turn'))
            self.assertEqual(len(native.requests),count);self.assertEqual(native.client.state,'CLOSED')

    def test_future_cursor_native_unavailable_is_terminal(self):
        native=self.fixture();native.client.observe()
        with self.assertRaises(Unavailable):native.client.read_events(EPOCH,1,16)
        self.assertEqual(native.client.state,'CLOSED')

    def test_v4_peer_rejected_before_request_and_audio_hello_strict(self):
        for field,value in (('schema','openxcom-agent-control/4'),('audio_history','guessed')):
            hello=dict(schema=a.SCHEMA,mode=a.MODE,epoch=EPOCH,ledger_capacity=64,audio_history='mixed_output');hello[field]=value
            native=self.fixture(hello=hello)
            with self.assertRaises(Unavailable):native.client.observe()
            self.assertEqual(native.requests,[])

    def test_end_turn_duplicate_only_queries_status_after_event_reads(self):
        native=self.fixture(transitions={('battlescape','end_turn'):'next_turn'})
        observed=native.client.observe();args=arguments(observed,action='end_turn');receipt=native.client.execute(**args)
        self.append(native,presentation(screen='next_turn'));native.client.read_events(EPOCH,0,16)
        fresh=native.client.observe();self.assertEqual(native.client.execute(**args),receipt)
        self.assertEqual(native.client.capability,fresh['capability'])
        self.assertEqual(len([r for r in native.requests if r.startswith(b'E ')]),1)
        self.assertEqual(native.requests[-1],('S '+COMMAND).encode())
        with self.assertRaises(Unavailable):native.client.execute(**arguments(fresh,command_id='6'*32,action='end_turn'))

    def test_event_tool_closed_schema_no_images_and_disabled_default(self):
        native=self.fixture();server=adapter.Server(agent_control=native.client)
        call(server,'read_ui_state');self.append(native,audio())
        result=call(server,'read_public_events',dict(epoch=EPOCH,after=0,limit=16))['result']
        self.assertFalse(result['isError']);self.assertEqual([item['type'] for item in result['content']],['text'])
        self.assertEqual(json.loads(result['content'][0]['text']),result['structuredContent'])
        self.assertEqual(call(adapter.Server(),'read_public_events',dict(epoch=EPOCH,after=0,limit=1))['error']['code'],-32602)
        before=len(native.requests)
        for bad in ({},{'epoch':EPOCH,'after':True,'limit':1},{'epoch':EPOCH,'after':0,'limit':1,'action':'end_turn'}):
            self.assertEqual(call(server,'read_public_events',bad)['error']['code'],-32602)
        self.assertEqual(len(native.requests),before)

    def test_end_turn_uncertainty_forbids_event_or_action_retry(self):
        native=self.fixture(transform=lambda op,obj:None if op=='E' else obj)
        observed=native.client.observe();args=arguments(observed,action='end_turn')
        with self.assertRaises(a.ActionOutcomeUnknown):native.client.execute(**args)
        count=len(native.requests)
        with self.assertRaises(Unavailable):native.client.read_events(EPOCH,0,16)
        with self.assertRaises(Unavailable):native.client.execute(**args)
        self.assertEqual(len(native.requests),count)
        self.assertEqual(sum(request.startswith(b'E ') for request in native.requests),1)

    def test_end_turn_shares_non_evicting_ledger(self):
        native=self.fixture()
        for index in range(64):
            observed=native.client.observe()
            native.client.execute(**arguments(observed,command_id=f'{index+100:032x}',action='end_turn'))
        observed=native.client.observe();count=len(native.requests)
        with self.assertRaises(Unavailable):
            native.client.execute(**arguments(observed,command_id='e'*32,action='end_turn'))
        self.assertEqual(len(native.requests),count);self.assertEqual(len(native.client.commands),64)

    def test_real_stdio_event_read_then_end_turn_and_terminal_duplicate(self):
        observed=state();events=page([presentation()]);receipt=result(observation=state(None,None,2,2,'next_turn'))
        args=arguments(observed,action='end_turn')
        child=r'''
import json,os,socket
channel=socket.socket(fileno=int(os.environ['OPENXCOM_AGENT_CONTROL_FD']))
def send(value):channel.send(json.dumps(value,separators=(',',':')).encode())
send(dict(schema=SCHEMA,mode='agent-control',epoch=EPOCH,ledger_capacity=64,audio_history='mixed_output'))
assert channel.recv(512)==('H '+SCHEMA).encode();send(dict(schema=SCHEMA,mode='agent-control',status='ready'))
request=channel.recv(512).decode().split();assert request[0]=='R'
OBSERVED['challenge']=request[1];send(OBSERVED)
request=channel.recv(512).decode().split();assert request[0]=='V' and request[2:]==[EPOCH,'0','16']
EVENTS['challenge']=request[1];send(EVENTS)
assert channel.recv(512)==WIRE;send(RECEIPT)
for _ in range(2):
    assert channel.recv(512)==('S '+RECEIPT['command_id']).encode();send(RECEIPT)
assert channel.recv(512)==b''
'''
        child='\n'.join(f'{key}={value!r}' for key,value in dict(SCHEMA=a.SCHEMA,EPOCH=EPOCH,
            OBSERVED=observed,EVENTS=events,RECEIPT=receipt,WIRE=a.canonical_action_request(args)).items())+'\n'+child
        messages=[dict(jsonrpc='2.0',id=1,method='initialize',params=dict(protocolVersion=adapter.PROTOCOL,
                     capabilities={},clientInfo=dict(name='event-test',version='1'))),
                  dict(jsonrpc='2.0',method='notifications/initialized'),dict(jsonrpc='2.0',id=2,method='tools/list')]
        for index,(name,values) in enumerate((('read_ui_state',{}),('read_public_events',dict(epoch=EPOCH,after=0,limit=16)),
                ('execute_ui_action',args),('read_ui_action_result',dict(command_id=COMMAND)),('execute_ui_action',args)),3):
            messages.append(dict(jsonrpc='2.0',id=index,method='tools/call',params=dict(name=name,arguments=values)))
        completed=subprocess.run([sys.executable,str(Path(__file__).with_name('launch_presentation.py')),'--agent-control',
            '--',sys.executable,'-c',child],input=b''.join(dump(message)+b'\n' for message in messages),capture_output=True,timeout=8)
        self.assertEqual(completed.returncode,0,completed.stderr)
        replies={row['id']:row for row in map(json.loads,completed.stdout.splitlines())}
        self.assertTrue(set(a.TOOL_NAMES)<={tool['name'] for tool in replies[2]['result']['tools']})
        self.assertEqual(replies[4]['result']['structuredContent']['audio_status'],'active')
        for index in range(3,8):
            self.assertFalse(replies[index]['result']['isError'])
            self.assertEqual([item['type'] for item in replies[index]['result']['content']],['text'])
        self.assertEqual(replies[5]['result'],replies[6]['result']);self.assertEqual(replies[6]['result'],replies[7]['result'])


if __name__=='__main__':unittest.main()
