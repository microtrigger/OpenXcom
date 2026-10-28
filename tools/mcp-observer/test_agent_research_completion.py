# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-19.
"""Wire25 research/report contract; synthetic sockets are not native/live proof."""
import copy
import itertools
import json
import unittest
from unittest.mock import patch

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, result, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate

FIELDS = {
    'research_complete': 'title topic buttons',
    'new_possible_research': 'title rows scroll buttons',
    'new_possible_manufacture': 'title rows scroll buttons',
    'research_required': 'message buttons',
    'ufopaedia_article': 'title body_lines buttons details_status',
    'mission_alert': 'title city buttons',
}
BUTTONS = {'research_complete': ('ok','reports'), 'new_possible_research': ('ok','allocate'),
           'new_possible_manufacture': ('ok','allocate'), 'research_required': ('ok',),
           'ufopaedia_article': ('ok','previous','next'), 'mission_alert': ('intercept','center','cancel')}
MAPPING = {
    'research_complete': {'dismiss_research_complete':'ok','view_research_reports':'reports'},
    'new_possible_research': {'dismiss_new_research':'ok','open_research_allocation':'allocate'},
    'new_possible_manufacture': {'dismiss_new_manufacture':'ok','open_manufacture_allocation':'allocate'},
    'research_required': {'dismiss_research_required':'ok'},
    'ufopaedia_article': {'close_ufopaedia_article':'ok','previous_ufopaedia_article':'previous','next_ufopaedia_article':'next'},
    'mission_alert': {'intercept_mission_alert':'intercept','center_mission_alert':'center','dismiss_mission_alert':'cancel'},
}
NOTICES = ('new_possible_research','new_possible_manufacture')
NEW = tuple(action for mapping in MAPPING.values() for action in mapping) + ('scroll_article_text',)


def checked(obj): return a.validate_state(obj, CHALLENGE, EPOCH)
def event(obj, seq=1): return dict(seq=seq,kind='presentation',state=obj['state'],visible=copy.deepcopy(obj['visible']))
def size(obj): return len(json.dumps(obj,ensure_ascii=True,separators=(',',':')).encode('ascii'))

def only(obj, action):
    obj['actions'] = [d for d in obj['actions'] if d['id'] == action]
    button = MAPPING[obj['state']].get(action)
    obj['visible']['buttons'] = [b for b in obj['visible']['buttons'] if b['id'] == button]
    if not obj['actions']: obj['capability'] = None
    return obj


def sized_notice(count):
    obj=state(screen=NOTICES[0])
    obj['visible']['rows']=[dict(handle=f'r{i}',cells=[''],selected=False) for i in range(32)]
    remaining=count-size(obj)
    if remaining<0: raise ValueError('Too small')
    for row in obj['visible']['rows']:
        n=min(remaining,1024);row['cells']=['x'*n];remaining-=n
    if remaining: raise ValueError('Too large')
    return obj


class ResearchCompletionContract(unittest.TestCase):
    def reject_both(self,obj):
        with self.assertRaises(Unavailable): checked(obj)
        with self.assertRaises(Unavailable): validate(page([event(obj)]))

    def test_exact_catalog_and_old_wires(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/50')
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS),len(a.definitions())),(261, 106, 7))
        self.assertEqual(a.RESEARCH_COMPLETION_FIELDS,FIELDS)
        self.assertEqual(a.RESEARCH_COMPLETION_BUTTONS,BUTTONS)
        self.assertEqual(a.RESEARCH_COMPLETION_BUTTON_ACTIONS,MAPPING)
        self.assertEqual(len(NEW),14)
        self.assertEqual((a.MAX_REQUEST,a.MAX_TEXT_PACKET,a.MAX_PACKET,a.MAX_EVENTS,a.MAX_COMMANDS),(512,32768,49152,16,256))
        for v in range(1,26):
            obj=state(screen='research_complete');obj['schema']=f'openxcom-agent-control/{v}'
            with self.subTest(version=v),self.assertRaises(Unavailable): checked(obj)

    def test_all_six_shapes_and_action_kinds(self):
        for screen,fields in FIELDS.items():
            obj=state(screen=screen);self.assertEqual(checked(obj),obj)
            self.assertEqual(set(obj['visible']),set(fields.split()))
            self.assertEqual(validate(page([event(obj)]))['events'][0],event(obj))
            for action in MAPPING[screen]: self.assertEqual(a.ARGUMENT_KINDS[action],'none')
        self.assertEqual(a.ARGUMENT_KINDS['scroll_article_text'],'scroll_direction')

    def test_missing_keys_and_hidden_ids_rejected(self):
        for screen in FIELDS:
            original=state(screen=screen)
            for key in original['visible']:
                obj=copy.deepcopy(original);del obj['visible'][key]
                with self.subTest(screen=screen,missing=key): self.reject_both(obj)
            for key in ('research_id','article_id','bonus','lookup_id','cost','eligible','has_more','scroll_offset','total_lines','image','stats','mission_id'):
                obj=copy.deepcopy(original);obj['visible'][key]=None
                with self.subTest(screen=screen,extra=key): self.reject_both(obj)

    def test_new_actions_fail_in_every_wrong_state(self):
        for screen,action in itertools.product(a.STATE_ACTIONS,NEW):
            if action in a.STATE_ACTIONS[screen]: continue
            obj=state(screen=screen);obj['actions'].append(dict(id=action,label='Shown',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action]))
            with self.subTest(screen=screen,action=action),self.assertRaises(Unavailable): checked(obj)

    def test_null_empty_text_independent_buttons(self):
        for screen in FIELDS:
            obj=state(screen=screen)
            scalar=[k for k in obj['visible'] if k not in ('buttons','body_lines','details_status','rows','scroll')]
            for value in (None,'','\x01Shown','é漢字'):
                for key in scalar: obj['visible'][key]=value
                if 'body_lines' in obj['visible']: obj['visible']['body_lines']=None
                self.assertEqual(checked(obj),obj)
                self.assertEqual(validate(page([event(obj)]))['events'][0]['visible'],obj['visible'])

    def test_text_and_button_utf8_limits(self):
        for screen in FIELDS:
            for key in FIELDS[screen].split():
                if key in ('buttons','rows','scroll','body_lines','details_status'): continue
                for text,good in (('é'*512,True),('😀'*256,True),('x'*1025,False),('é'*513,False),('\ud800',False), (True,False)):
                    obj=state(screen=screen);obj['visible'][key]=text
                    with self.subTest(screen=screen,key=key,good=good):
                        if good: checked(obj)
                        else: self.reject_both(obj)
        for text in ('x'*1025,'é'*513,'',None,True,'\ud800'):
            obj=state(screen='research_complete');obj['visible']['buttons'][0]['label']=text
            self.reject_both(obj)

    def test_button_subsets_have_atomic_actions_and_canonical_order(self):
        for screen,order in BUTTONS.items():
            original=state(screen=screen)
            for n in range(len(order)+1):
                for subset in itertools.combinations(order,n):
                    obj=copy.deepcopy(original)
                    obj['visible']['buttons']=[b for b in obj['visible']['buttons'] if b['id'] in subset]
                    obj['actions']=[d for d in obj['actions'] if d['id'] not in MAPPING[screen] or MAPPING[screen][d['id']] in subset]
                    if not obj['actions']: obj['capability']=None
                    checked(obj)
            if len(order)>1:
                obj=copy.deepcopy(original);obj['visible']['buttons'].reverse();self.reject_both(obj)
            for buttons in (None,{},[dict(id='private',label='Shown')],[dict(id=order[0],label='Shown',enabled=True)]):
                obj=copy.deepcopy(original);obj['visible']['buttons']=buttons;self.reject_both(obj)
            obj=copy.deepcopy(original);obj['visible']['buttons'].append(copy.deepcopy(obj['visible']['buttons'][0]));self.reject_both(obj)

    def test_button_missing_action_and_action_missing_button(self):
        for screen,mapping in MAPPING.items():
            for action,button in mapping.items():
                obj=state(screen=screen);obj['actions']=[d for d in obj['actions'] if d['id']!=action]
                with self.subTest(screen=screen,action=action),self.assertRaises(Unavailable): checked(obj)
                validate(page([event(obj)]))  # Retained events have no catalog.
                obj=state(screen=screen);obj['visible']['buttons']=[b for b in obj['visible']['buttons'] if b['id']!=button]
                with self.assertRaises(Unavailable): checked(obj)

    def test_descriptor_labels_kinds_sources_and_order(self):
        for screen in FIELDS:
            original=state(screen=screen)
            for i,descriptor in enumerate(original['actions']):
                for key,value in (('label','Guessed'),('label_source','hidden_lookup'),('argument_kind','row'),('private',None)):
                    obj=copy.deepcopy(original);obj['actions'][i][key]=value
                    with self.subTest(screen=screen,key=key),self.assertRaises(Unavailable): checked(obj)
            if len(original['actions'])>1:
                obj=copy.deepcopy(original);obj['actions'].reverse()
                with self.assertRaises(Unavailable): checked(obj)

    def test_mission_visible_actions_and_no_target_arguments(self):
        obj=state(screen='mission_alert')
        self.assertEqual([d['id'] for d in obj['actions']],
                         ['intercept_mission_alert','center_mission_alert','dismiss_mission_alert'])
        for action in MAPPING['mission_alert']:
            a.validate_action_arguments(arguments(action=action,args={}))
            for args in ({'mission_id': 7}, {'longitude': 1}, {'target':'hidden'}):
                with self.assertRaises(Unavailable):
                    a.validate_action_arguments(arguments(action=action,args=args))
        for action in ('intercept','center','intercept_mission','center_mission','center_enemy','open_bases'):
            bad=copy.deepcopy(obj);bad['actions']=[dict(id=action,label='Intercept',label_source='visible_text',argument_kind='none')]
            with self.subTest(action=action),self.assertRaises(Unavailable):checked(bad)

    def test_article_null_empty_and_unknown_line_not_collapsed(self):
        for lines in (None,[],[None],['Shown',None,'Other'],[None]*32):
            obj=state(screen='ufopaedia_article');obj['visible']['body_lines']=lines
            self.assertEqual(checked(obj)['visible']['body_lines'],lines)
            self.assertEqual(validate(page([event(obj)]))['events'][0]['visible']['body_lines'],lines)
        for lines in ('Shown',{},False,[''],[True],[1],[{}],[[]],[None]*33):
            obj=state(screen='ufopaedia_article');obj['visible']['body_lines']=lines;self.reject_both(obj)

    def test_article_line_byte_and_aggregate_bounds(self):
        for lines,good in ((['x'*1024],True),(['é'*512],True),(['😀'*256],True),(['x'*1025],False),(['é'*513],False),(['x'*1024]*8,True),(['x'*1024]*8+['y'],False),(['é'*512]*8,True),(['é'*512]*8+['é'],False)):
            obj=state(screen='ufopaedia_article');obj['visible']['body_lines']=lines
            with self.subTest(good=good,length=len(lines)):
                if good: checked(obj);validate(page([event(obj)]))
                else:self.reject_both(obj)

    def test_article_controls_and_whitespace_are_not_normalized_by_client(self):
        for char in [chr(i) for i in range(32)]+['\x7f']:
            obj=state(screen='ufopaedia_article');obj['visible']['body_lines']=['a'+char+'b'];self.reject_both(obj)
        for line in (' leading','trailing ','two  spaces',' ','\ud800'):
            obj=state(screen='ufopaedia_article');obj['visible']['body_lines']=[line];self.reject_both(obj)
        for line in ('one space','é漢字','A\u00a0B','A\x80B','A\x9fB'):
            obj=state(screen='ufopaedia_article');obj['visible']['body_lines']=[line];self.assertEqual(checked(obj)['visible']['body_lines'],[line])

    def test_article_details_remain_unavailable(self):
        for status in (None,True,{},'presented','complete','known',''):
            obj=state(screen='ufopaedia_article');obj['visible']['details_status']=status;self.reject_both(obj)

    def test_article_scroll_authority_independent_of_body_lines(self):
        for lines in (None,[],[None],['Short']):
            obj=only(state(screen='ufopaedia_article'),'scroll_article_text');obj['visible']['body_lines']=lines
            checked(obj)
            for direction in ('up','down'):
                arg=arguments(obj,action='scroll_article_text',args=dict(direction=direction))
                self.assertTrue(a.canonical_action_request(arg).endswith((' scroll_article_text '+direction).encode()))
        obj=state(screen='ufopaedia_article');obj['actions']=[d for d in obj['actions'] if d['id']!='scroll_article_text'];checked(obj)

    def test_scroll_arguments_strict_existing_kind(self):
        for args in ({},{'direction':True},{'direction':'left'},{'direction':0},{'direction':'down','count':1},{'direction':'up','duration':1},{'row':'r0'},{'direction':None}):
            with self.subTest(args=args),self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='scroll_article_text',args=args))
        for action in set(NEW)-{'scroll_article_text'}:
            a.validate_action_arguments(arguments(action=action,args={}))
            for args in ({'id':'private'},{'direction':'down'},{'confirmed':True},None,[]):
                if args is None: args={'value':None}
                with self.subTest(action=action,args=args),self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=args))

    def test_notice_rows_complete_readonly_and_compact(self):
        for screen in NOTICES:
            obj=state(screen=screen);obj['visible']['rows']=[];checked(obj)
            obj['visible']['rows']=[dict(handle=f'r{i}',cells=['Shown'],selected=False) for i in range(32)];checked(obj)
            for key,value in (('handle','r32'),('handle','r00'),('handle','r1'),('selected',True),('selected',0),('cells',[None]),('cells',[]),('cells',['a','b']),('cells',['x'*1025]),('hidden_id',1)):
                bad=copy.deepcopy(obj);bad['visible']['rows'][0][key]=value
                with self.subTest(screen=screen,key=key):self.reject_both(bad)
            bad=copy.deepcopy(obj);bad['visible']['rows'].append(dict(handle='r32',cells=['Shown'],selected=False));self.reject_both(bad)
            bad=copy.deepcopy(obj);bad['visible']['rows'].reverse();self.reject_both(bad)

    def test_notice_rows_cannot_be_selected_by_old_row_actions(self):
        for screen in NOTICES:
            for action in ('choose_visible_row','select_research_project','select_production_project','select_soldier'):
                obj=state(screen=screen);obj['actions'].append(dict(id=action,label='Shown',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action]))
                with self.assertRaises(Unavailable):checked(obj)

    def test_notice_scroll_flags_and_exact_semantics(self):
        for screen in NOTICES:
            for flags in ({'up':True,'down':False},{'up':False,'down':True},{'up':True,'down':True}):
                obj=state(screen=screen);obj['visible']['scroll']=flags;checked(obj)
            for flags in ({},{'up':True},{'up':1,'down':False},{'up':False,'down':None},{'up':True,'down':False,'more':True}):
                obj=state(screen=screen);obj['visible']['scroll']=flags;self.reject_both(obj)
            obj=state(screen=screen);obj['visible']['scroll']={'up':False,'down':False}
            with self.assertRaises(Unavailable):checked(obj)
            obj['actions']=[d for d in obj['actions'] if d['id']!='scroll_visible_list'];checked(obj)

    def test_state_canonical_ascii_byte_limit_exact(self):
        obj=sized_notice(32768);self.assertEqual(size(obj),32768);checked(obj)
        obj=sized_notice(32769)
        with self.assertRaises(Unavailable):checked(obj)
        obj=state(screen=NOTICES[0]);obj['visible']['rows']=[dict(handle=f'r{i}',cells=['é'*512],selected=False) for i in range(32)]
        self.assertLess(len(json.dumps(obj,ensure_ascii=False,separators=(',',':')).encode('utf-8')),size(obj))
        with self.assertRaises(Unavailable):checked(obj)

    def test_result_remains_dispatch_not_success_and_observation_has_no_grant(self):
        for screen in FIELDS:
            obj=state(None,None,revision=2,event_sequence=2,screen=screen)
            receipt=result(observation=obj,event_sequence=2);a.validate_result(dump(receipt),COMMAND,EPOCH)
            for key,value in (('semantics','success'),('status','completed')):
                bad=copy.deepcopy(receipt);bad[key]=value
                with self.assertRaises(Unavailable):a.validate_result(dump(bad),COMMAND,EPOCH)
            obj['capability']='2'*32
            with self.assertRaises(Unavailable):a.validate_result(dump(receipt),COMMAND,EPOCH)

    def test_events_keep_exact_visible_schema_and_no_authority_or_new_kind(self):
        events=[event(state(screen=s),i+1) for i,s in enumerate(FIELDS)]
        self.assertEqual(validate(page(events))['events'],events)
        for e in events:
            for key,value in (('capability',None),('actions',[]),('research_id',None),('source','research')):
                bad=copy.deepcopy(e);bad.update(seq=1);bad[key]=value
                with self.assertRaises(Unavailable):validate(page([bad]))
            bad=copy.deepcopy(e);bad.update(seq=1,kind=e['state'])
            with self.assertRaises(Unavailable):validate(page([bad]))


class ResearchCompletionClient(unittest.TestCase):
    def fixture(self,**kwargs):
        peer=NativeFixture(**kwargs);self.addCleanup(peer.close);return peer

    def test_every_new_action_through_real_client_and_server(self):
        observed=set()
        for screen in FIELDS:
            for action in MAPPING[screen] | ({'scroll_article_text':None} if screen=='ufopaedia_article' else {}):
                peer=self.fixture(screen=screen);server=adapter.Server(agent_control=peer.client)
                obj=call(server,'read_ui_state')['result']['structuredContent']
                reply=call(server,'execute_ui_action',arguments(obj,action=action))['result']['structuredContent']
                self.assertEqual(reply['status'],'dispatched');self.assertEqual(reply['semantics'],'dispatch_not_success')
                self.assertIsNone(reply['observation']['capability']);observed.add(action)
        self.assertEqual(observed,set(NEW))

    def test_both_article_directions_with_null_lines(self):
        def transform(op,obj):
            if op=='R':obj['visible']['body_lines']=None
            return obj
        for direction in ('up','down'):
            peer=self.fixture(screen='ufopaedia_article',transform=transform);obj=peer.client.observe()
            peer.client.execute(**arguments(obj,action='scroll_article_text',args=dict(direction=direction)))
            self.assertTrue(peer.requests[-1].endswith((' scroll_article_text '+direction).encode()))

    def test_unoffered_article_control_cannot_come_from_lines(self):
        def transform(op,obj):
            if op=='R':obj['actions']=[d for d in obj['actions'] if d['id']!='scroll_article_text']
            return obj
        peer=self.fixture(screen='ufopaedia_article',transform=transform);obj=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='scroll_article_text'))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))

    def test_notice_current_scroll_direction_and_context_cache(self):
        def transform(op,obj):
            if op=='R' and obj['state'] in NOTICES:obj['visible']['scroll']={'up':False,'down':True}
            return obj
        peer=self.fixture(screen=NOTICES[0],transform=transform);obj=peer.client.observe()
        self.assertEqual(peer.client.scroll_directions,frozenset({'down'}))
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='scroll_visible_list',args={'direction':'up'}))
        obj['visible']['scroll']['up']=True
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='scroll_visible_list',args={'direction':'up'}))
        peer.client.execute(**arguments(obj,action='scroll_visible_list'));self.assertFalse(peer.client.scroll_directions)
        peer.screen='ufopaedia_article';peer.client.observe();self.assertFalse(peer.client.scroll_directions)

    def test_stale_grant_context_change_and_deadline_no_send(self):
        peer=self.fixture(screen='ufopaedia_article');old=peer.client.observe();peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='scroll_article_text'))
        peer.screen='mission_alert';peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='next_ufopaedia_article'))
        peer.screen='ufopaedia_article';fresh=peer.client.observe()
        with patch.object(a.time,'monotonic',return_value=peer.client.capability_deadline+1):
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,action='scroll_article_text'))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))

    def test_duplicate_queries_status_and_changed_args_conflict(self):
        peer=self.fixture(screen='ufopaedia_article');obj=peer.client.observe();args=arguments(obj,action='scroll_article_text')
        first=peer.client.execute(**args);self.assertEqual(peer.client.execute(**args),first)
        bad=copy.deepcopy(args);bad['args']['direction']='up'
        with self.assertRaises(Unavailable):peer.client.execute(**bad)
        self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)
        self.assertEqual(sum(x.startswith(b'S ') for x in peer.requests),1)

    def test_unavailable_receipt_terminal_without_retry(self):
        def transform(op,obj):
            if op in ('E','S'):obj.update(status='unavailable',observation=None)
            return obj
        peer=self.fixture(screen='ufopaedia_article',transform=transform);obj=peer.client.observe();args=arguments(obj,action='scroll_article_text')
        self.assertEqual(peer.client.execute(**args)['status'],'unavailable')
        self.assertEqual(peer.client.execute(**args)['status'],'unavailable')
        self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)

    def test_unknown_pending_timeout_and_no_replay(self):
        for mode in ('silent','pending','unknown_after_dispatch'):
            def transform(op,obj):
                if op in ('E','S'):
                    if mode=='silent':return None
                    obj.update(status=mode,observation=None)
                return obj
            peer=self.fixture(screen='ufopaedia_article',transform=transform);obj=peer.client.observe();args=arguments(obj,action='scroll_article_text')
            if mode=='unknown_after_dispatch':
                self.assertEqual(peer.client.execute(**args)['status'],mode)
                self.assertEqual(peer.client.execute(**args)['status'],mode)
            else:
                with self.assertRaises(a.ActionOutcomeUnknown):peer.client.execute(**args)
                with self.assertRaises(Unavailable):peer.client.execute(**args)
            with self.assertRaises(Unavailable):peer.client.execute(**dict(args,command_id='9'*32))
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)

    def test_reconnect_needs_new_read(self):
        old=self.fixture(screen='ufopaedia_article');obj=old.client.observe();old.client.close()
        peer=self.fixture(screen='ufopaedia_article')
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='scroll_article_text'))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        peer.client.execute(**arguments(peer.client.observe(),action='scroll_article_text'))

    def test_retained_pages_preserve_grant_and_replay_bytes_semantics(self):
        for text in ('x'*1024,'漢'*341,'😀'*256):
            peer=self.fixture(screen='ufopaedia_article');obj=peer.client.observe()
            # Retained native records must fit conservative UTF-8 asciiBound,
            # not merely Python's smaller canonical-ASCII serialization.
            article=state(screen='ufopaedia_article');article['visible']['body_lines']=[text]*4
            peer.public_events=[event(article,i+1) for i in range(3)];peer.event_page_limit=1
            before=(peer.client.capability,peer.client.revision,peer.client.available_actions)
            for after in range(3):
                response=peer.client.read_events(EPOCH,after,16)
                self.assertEqual(response['next_cursor'],after+1);self.assertEqual(response['events'],[event(article,after+1)])
            empty=peer.client.read_events(EPOCH,3,16);self.assertEqual(empty['events'],[])
            self.assertEqual(peer.client.read_events(EPOCH,0,16)['events'],[event(article,1)])
            self.assertEqual(before,(peer.client.capability,peer.client.revision,peer.client.available_actions))
            peer.client.execute(**arguments(obj,action='scroll_article_text'))

    def test_changed_retained_article_and_oversized_page_fail_closed(self):
        peer=self.fixture(screen='ufopaedia_article');peer.client.observe();peer.public_events=[event(state(screen='ufopaedia_article'))]
        peer.client.read_events(EPOCH,0,16);peer.public_events[0]['visible']['body_lines']=['Changed']
        with self.assertRaises(Unavailable):peer.client.read_events(EPOCH,0,16)
        self.assertEqual(peer.client.state,'CLOSED')
        article=state(screen='ufopaedia_article');article['visible']['body_lines']=['😀'*256]*8
        events=[event(article,i+1) for i in range(3)]
        self.assertGreater(size(page(events)),a.MAX_PACKET)
        with self.assertRaises(Unavailable):validate(page(events))

    def test_server_transport_uncertainty_remains_nonreceipt(self):
        peer=self.fixture(screen='ufopaedia_article',transform=lambda op,obj:None if op=='E' else obj)
        server=adapter.Server(agent_control=peer.client);obj=call(server,'read_ui_state')['result']['structuredContent']
        reply=call(server,'execute_ui_action',arguments(obj,action='scroll_article_text'))['result']
        self.assertTrue(reply['isError']);self.assertEqual(reply['structuredContent']['status'],'outcome_unknown')
        self.assertIs(reply['structuredContent']['retryable'],False)


if __name__=='__main__':unittest.main()
