# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-19.
"""Wire21 semantic quantity validation. Synthetic sockets are not native/game proof."""
import copy
import itertools
import json
import threading
import unittest
import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, result, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate
NEW_ACTIONS = ('change_production_quantity','set_production_sell','open_purchase','change_purchase_quantity',
               'accept_purchase','cancel_purchase','open_purchase_category','scroll_purchase','position_purchase_list')
NEW_STATES = ('purchase','purchase_category')
SCREENS = ('manufacture_info',) + NEW_STATES

def checked(obj):
    return a.validate_state(obj, CHALLENGE, EPOCH)


def catalog(obj, ids=()):
    obj['actions'] = [entry for entry in obj['actions'] if entry['id'] in ids]
    if not obj['actions']: obj['capability'] = None
    return obj


def sized_purchase(size):
    obj = state(screen='purchase')
    obj['visible']['rows'] = [dict(handle=f'r{i}', cells=['', '', '', ''], selected=False, controls=['increase']) for i in range(32)]
    left = size - len(json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii'))
    if left < 0: raise ValueError('size too small')
    for row in obj['visible']['rows']:
        for index in range(4):
            count = min(left, 1024); row['cells'][index] = 'x' * count; left -= count
    if left: raise ValueError('size too large')
    return obj


class QuantityContractTests(unittest.TestCase):
    def test_new_action_authority_rejected_in_every_wrong_state(self):
        for screen in a.STATE_ACTIONS:
            for action in set(NEW_ACTIONS) - a.STATE_ACTIONS[screen]:
                obj = state(screen=screen)
                obj['actions'] = [dict(id=action, label='Shown', label_source=a.label_source(screen, action), argument_kind=a.ARGUMENT_KINDS[action])]
                with self.subTest(screen=screen, action=action), self.assertRaises(Unavailable): checked(obj)


    def test_exact_visible_keys_and_no_hidden_prediction(self):
        for screen in SCREENS:
            obj = state(screen=screen); checked(obj)
            for key in obj['visible']:
                bad = copy.deepcopy(obj); del bad['visible'][key]
                with self.subTest(screen=screen, key=key), self.assertRaises(Unavailable): checked(bad)
            for key in ('max_quantity', 'eligible', 'item_type', 'item_id', 'capacity', 'success', 'arrived', 'timer_count', 'held_ms'):
                bad = copy.deepcopy(obj); bad['visible'][key] = None
                with self.assertRaises(Unavailable): checked(bad)


    def test_nullable_purchase_text_independent_of_arrow_authority(self):
        obj = state(screen='purchase')
        for key in ('title', 'funds', 'total', 'space_used'): obj['visible'][key] = None
        obj['visible']['headings'] = [None, None]
        obj['visible']['rows'][0]['cells'] = [None] * 4
        checked(obj)
        event = dict(seq=1, kind='presentation', state='purchase', visible=obj['visible'])
        self.assertEqual(validate(page([event]))['events'][0], event)
        for cells in ([None]*3, [None]*5, 'null', [True]*4):
            bad = copy.deepcopy(obj); bad['visible']['rows'][0]['cells'] = cells
            with self.assertRaises(Unavailable): checked(bad)


    def test_text_utf8_limits_renderer_tokens_and_no_numeric_inference(self):
        for value in (None, '', 'é'*512, '$-1', '>∞', '\x0150', 'not a number'):
            obj = state(screen='purchase'); obj['visible']['rows'][0]['cells'] = [value]*4; checked(obj)
        for field in ('title', 'funds', 'total', 'space_used'):
            obj = state(screen='purchase'); obj['visible'][field] = 'é'*513
            with self.assertRaises(Unavailable): checked(obj)
        for screen in SCREENS:
            obj = state(screen=screen)
            obj['visible']['title' if screen != 'purchase_category' else 'label'] = '\ud800'
            with self.assertRaises(Unavailable): checked(obj)


    def test_compact_32_rows_selection_and_arrow_subsets(self):
        obj = state(screen='purchase')
        obj['visible']['rows'] = [dict(handle=f'r{i}',cells=[None]*4,selected=i==31,controls=['increase'] if i%2 else ['decrease']) for i in range(32)]
        checked(obj)
        for key, value in (('handle','r32'), ('handle','r00'), ('selected',1), ('controls',[]), ('controls',['decrease','increase']), ('controls',['increase','increase']), ('controls',['max'])):
            bad = copy.deepcopy(obj); bad['visible']['rows'][0][key] = value
            with self.subTest(key=key,value=value), self.assertRaises(Unavailable): checked(bad)
        obj['visible']['rows'][0]['selected'] = True
        with self.assertRaises(Unavailable): checked(obj)
        obj['visible']['rows'][0]['selected'] = False; obj['visible']['rows'].append(dict(handle='r32',cells=[None]*4,selected=False,controls=['increase']))
        with self.assertRaises(Unavailable): checked(obj)


    def test_empty_rows_do_not_authorize_purchase_change(self):
        obj = state(screen='purchase'); obj['visible']['rows'] = []
        with self.assertRaises(Unavailable): checked(obj)
        catalog(obj, [x['id'] for x in obj['actions'] if x['id'] != 'change_purchase_quantity']); checked(obj)


    def test_scroll_flags_and_track_targets_are_fresh_control_projection(self):
        for targets in ([], [dict(handle='t1',position='top')], [dict(handle='t0',position='bottom'),dict(handle='t1',position='top')], [dict(handle='t0',position='top',percent=0)]):
            obj=state(screen='purchase'); obj['visible']['scrollbar']['targets']=targets
            with self.assertRaises(Unavailable):checked(obj)
        obj=state(screen='purchase');obj['visible']['scrollbar']['targets']=[dict(handle='t0',position='middle')];checked(obj)
        obj=state(screen='purchase');obj['visible']['scrollbar']=None
        with self.assertRaises(Unavailable):checked(obj)
        catalog(obj,[x['id'] for x in obj['actions'] if x['id']!='position_purchase_list']);checked(obj)
        obj=state(screen='purchase');obj['visible']['scroll']=dict(up=False,down=False)
        with self.assertRaises(Unavailable):checked(obj)
        catalog(obj,[x['id'] for x in obj['actions'] if x['id'] not in ('scroll_purchase','scroll_visible_list')]);checked(obj)


    def test_category_and_buttons_authority_and_canonical_order(self):
        for key,action in (('category','open_purchase_category'),):
            obj=state(screen='purchase');obj['visible'][key]=None
            with self.assertRaises(Unavailable):checked(obj)
            catalog(obj,[x['id'] for x in obj['actions'] if x['id']!=action]);checked(obj)
        for value in ([],[dict(id='cancel',label='Cancel'),dict(id='accept',label='OK')],[dict(id='accept',label='')]):
            obj=state(screen='purchase');obj['visible']['buttons']=value
            with self.assertRaises(Unavailable):checked(obj)
        for bad in ({'label':''},{'label':None},{'label':'All','index':0}):
            obj=state(screen='purchase');obj['visible']['category']=bad
            with self.assertRaises(Unavailable):checked(obj)


    def test_dropdown_category_only_complete_nonempty_1024_labels(self):
        checked(state(screen='purchase_category'))
        for control in ('mission','hidden_category',None):
            obj=state(screen='purchase_category');obj['visible']['control']=control
            with self.assertRaises(Unavailable):checked(obj)
        for value in (None,'','é'*513):
            obj=state(screen='purchase_category');obj['visible']['rows'][0]['cells']=[value]
            with self.assertRaises(Unavailable):checked(obj)
        obj=state(screen='purchase_category');obj['visible']['label']=None;checked(obj)
        obj=state(screen='new_battle_dropdown');obj['visible']['control']='category'
        with self.assertRaises(Unavailable):checked(obj)


    def test_all_new_descriptors_exact_label_source_and_kind(self):
        for screen in SCREENS+('basescape',):
            obj=state(screen=screen)
            for entry in obj['actions']:
                if entry['id'] not in NEW_ACTIONS+('accept_production','stop_production','choose_visible_row','close_dropdown'):continue
                for key,value in (('label','Unshown'),('label_source','model'),('argument_kind','none' if entry['argument_kind']!='none' else 'purchase_row')):
                    bad=copy.deepcopy(obj);next(x for x in bad['actions'] if x['id']==entry['id'])[key]=value
                    with self.subTest(screen=screen,action=entry['id'],key=key),self.assertRaises(Unavailable):checked(bad)


    def test_canonical_ascii_state_and_event_budget(self):
        checked(sized_purchase(32768))
        with self.assertRaises(Unavailable):checked(sized_purchase(32769))
        obj=sized_purchase(30000)
        event=dict(seq=1,kind='presentation',state='purchase',visible=obj['visible'])
        self.assertEqual(validate(page([event],newest=3))['next_cursor'],1)
        with self.assertRaises(Unavailable):validate(page([event,dict(event,seq=2)]))
        obj['visible']['rows']=[dict(handle=f'r{i}',cells=['\x01'*1024]*4,selected=False,controls=['increase']) for i in range(32)]
        with self.assertRaises(Unavailable):checked(obj)


    def test_presentation_events_validate_same_new_fields_without_authority(self):
        events=[dict(seq=i+1,kind='presentation',state=screen,visible=state(screen=screen)['visible']) for i,screen in enumerate(SCREENS)]
        self.assertEqual(validate(page(events))['events'],events)
        for index in range(len(events)):
            bad=copy.deepcopy(events);bad[index]['visible']['max']=0
            with self.assertRaises(Unavailable):validate(page(bad))
        for kind in ('purchase_committed','manufacture_completed','gesture_repeat'):
            with self.assertRaises(Unavailable):validate(page([dict(seq=1,kind=kind)]))


    def test_wire_counts_no_gesture_catalog_and_old_wires_rejected(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/26')
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS),len(a.definitions())),(176,78,4))
        self.assertEqual((a.MAX_COMMANDS,a.EVENT_CAPACITY,a.MAX_TEXT_PACKET,a.MAX_PACKET,a.MAX_REQUEST),(256,256,32768,49152,512))
        for version in range(1,22):
            obj=state(screen='purchase');obj['schema']=f'openxcom-agent-control/{version}'
            with self.assertRaises(Unavailable):checked(obj)
        for action in ('operate_manufacture_arrow','toggle_production_sell','operate_purchase_row','operate_purchase_scroll','drag_purchase_scrollbar'):
            self.assertNotIn(action,a.ACTIONS)
        schema=json.dumps(a.definitions()[1]['inputSchema'])
        for word in ('gesture','duration','hold_short','hold_long','wheel','input'):self.assertNotIn('"'+word+'"',schema)

    def test_all_quantity_boundaries_and_exact_canonical_suffix(self):
        for action,key,domain in (('change_production_quantity','control',a.QUANTITY_CONTROLS),('change_purchase_quantity','row',a.ROW_HANDLES)):
            for target in domain:
                variants=[dict(operation=op) for op in ('max','clear')]
                variants += [dict(operation='adjust',value=value) for value in (-1000,-1,1,1000)]
                variants += [dict(operation='set',value=value) for value in (0,1,100000)]
                for variant in variants:
                    args={key:target,**variant};request=a.canonical_action_request(arguments(action=action,args=args))
                    suffix=action+' '+target+' '+variant['operation']+(' '+str(variant['value']) if 'value' in variant else '')
                    self.assertTrue(request.endswith(suffix.encode('ascii')))
        for enabled in (False,True):
            self.assertTrue(a.canonical_action_request(arguments(action='set_production_sell',args={'enabled':enabled})).endswith((' true' if enabled else ' false').encode()))
        for direction,amount in itertools.product(a.SCROLL_DIRECTIONS,a.PURCHASE_AMOUNTS):
            self.assertTrue(a.canonical_action_request(arguments(action='scroll_purchase',args=dict(direction=direction,amount=amount))).endswith(f' {direction} {amount}'.encode()))
        for target in a.PURCHASE_SCROLL_HANDLES:
            self.assertTrue(a.canonical_action_request(arguments(action='position_purchase_list',args={'target':target})).endswith((' '+target).encode()))

    def test_numeric_type_range_missing_extra_and_operation_are_closed(self):
        for action,key,target in (('change_production_quantity','control','units'),('change_purchase_quantity','row','r0')):
            for operation,bad_values in (('adjust',(0,-1001,1001)),('set',(-1,100001))):
                for value in bad_values+(True,False,None,'1',1.0,[],{},float('nan'),float('inf')):
                    with self.subTest(action=action,op=operation,value=value),self.assertRaises(Unavailable):
                        a.validate_action_arguments(arguments(action=action,args={key:target,'operation':operation,'value':value}))
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args={key:target,'operation':operation}))
            for operation in ('max','clear'):
                for value in (None,0,1):
                    with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args={key:target,'operation':operation,'value':value}))
            for operation in ('all','infinite','increase','max\n',True,None):
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args={key:target,'operation':operation}))
            for target_bad in ('r00','r32','units\n',0,True,None,[]):
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args={key:target_bad,'operation':'clear'}))
        for action in NEW_ACTIONS+('accept_production','stop_production'):
            for key in ('gesture','duration_ms','input','hold','repeat','item_id','x','y','max_quantity'):
                args=arguments(action=action);args['args'][key]=1
                with self.assertRaises(Unavailable):a.validate_action_arguments(args)
        for enabled in (None,0,1,'true',[],{}):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='set_production_sell',args={'enabled':enabled}))

    def test_quantity_schema_oneof_exact_operation_numeric_bounds(self):
        branches=a.definitions()[1]['inputSchema']['oneOf']
        for action,key in (('change_production_quantity','control'),('change_purchase_quantity','row')):
            entry=next(x for x in branches if action in x['properties']['action']['enum'])['properties']['args']
            self.assertEqual(len(entry['oneOf']),4)
            for variant,operation in zip(entry['oneOf'],a.QUANTITY_OPERATIONS):
                self.assertEqual(variant['properties']['operation']['enum'],[operation]);self.assertFalse(variant['additionalProperties'])
                self.assertEqual(variant['required'],[key,'operation']+(['value'] if operation in ('adjust','set') else []))
                if operation=='adjust':self.assertEqual(variant['properties']['value'],dict(type='integer',minimum=-1000,maximum=1000,**{'not':{'enum':[0]}}))
                if operation=='set':self.assertEqual(variant['properties']['value'],dict(type='integer',minimum=0,maximum=100000))
        for action in ('accept_purchase','cancel_purchase','accept_production','stop_production'):
            self.assertEqual(a.ARGUMENT_KINDS[action],'none')
            self.assertTrue(a.canonical_action_request(arguments(action=action,args={})).endswith(action.encode()))

    def test_production_quantity_projection_exact_canonical_flags_and_atomic_engineers(self):
        for mutate in ('reverse','duplicate','unknown','false_flags','bool_type','extra','missing','legacy_mismatch'):
            obj=catalog(state(screen='manufacture_info'));controls=obj['visible']['quantity_controls']
            if mutate=='reverse':controls.reverse()
            elif mutate=='duplicate':controls.append(copy.deepcopy(controls[0]))
            elif mutate=='unknown':controls[0]['control']='scientists'
            elif mutate=='false_flags':controls[1].update(increase=False,decrease=False)
            elif mutate=='bool_type':controls[1]['increase']=1
            elif mutate=='extra':controls[1]['max']=100
            elif mutate=='missing':del controls[0]['decrease']
            else:controls[0]['increase']=False
            with self.subTest(mutate=mutate),self.assertRaises(Unavailable):checked(obj)
        obj=catalog(state(screen='manufacture_info'),['change_production_quantity'])
        for key in ('title','available','allocated','space','units','monthly_profit'):obj['visible'][key]=None
        checked(obj)
        obj['visible']['quantity_controls']=[];obj['visible']['buttons']=[x for x in obj['visible']['buttons'] if x['id'] not in a.STAFF_DIRECTIONS]
        with self.assertRaises(Unavailable):checked(obj)
        catalog(obj);checked(obj)

    def test_sell_nullable_desired_bool_and_exact_shown_label(self):
        obj=state(screen='manufacture_info');obj['visible']['sell']=None
        with self.assertRaises(Unavailable):checked(obj)
        catalog(obj,[x['id'] for x in obj['actions'] if x['id']!='set_production_sell']);checked(obj)
        for value in ({},dict(label='',pressed=False),dict(label='Sell',pressed=1),dict(label='Else',pressed=False),dict(label='Sell',pressed=False,enabled=True)):
            obj=state(screen='manufacture_info');obj['visible']['sell']=value
            with self.assertRaises(Unavailable):checked(obj)
        obj=state(screen='manufacture_info');obj['visible']['sell']['pressed']=True;checked(obj)

    def test_purchase_old_scroll_action_and_manufacture_old_widget_shape_rejected(self):
        obj=state(screen='purchase');obj['actions'].append(dict(id='scroll_visible_list',label='Scroll visible list',label_source='visible_control_semantics',argument_kind='scroll_direction'))
        with self.assertRaises(Unavailable):checked(obj)
        obj=state(screen='manufacture_info');obj['visible']['controls']=[]
        with self.assertRaises(Unavailable):checked(obj)
        del obj['visible']['quantity_controls']
        with self.assertRaises(Unavailable):checked(obj)


class QuantityClientTests(unittest.TestCase):
    def fixture(self,**kwargs):
        peer=NativeFixture(**kwargs);self.addCleanup(peer.close);return peer

    def test_every_new_action_uses_fresh_authority_and_clears_quantity_caches(self):
        screens={'open_purchase':'basescape','change_production_quantity':'manufacture_info','set_production_sell':'manufacture_info'}
        for action in NEW_ACTIONS:
            peer=self.fixture(screen=screens.get(action,'purchase'));obj=peer.client.observe()
            self.assertEqual(peer.client.execute(**arguments(obj,action=action))['status'],'dispatched')
            self.assertIsNone(peer.client.capability)
            self.assertFalse(peer.client.quantity_directions or peer.client.purchase_rows or peer.client.purchase_targets)

    def test_direction_membership_for_every_operation_and_both_quantity_families(self):
        for screen,action,key,target in (('manufacture_info','change_production_quantity','control','units'),('purchase','change_purchase_quantity','row','r0')):
            for directions in (['increase'],['decrease'],['increase','decrease']):
                def transform(op,obj):
                    if op=='R':
                        if screen=='manufacture_info':obj['visible']['quantity_controls'][1].update(increase='increase' in directions,decrease='decrease' in directions)
                        else:obj['visible']['rows'][0].update(cells=[None]*4,controls=directions)
                    return obj
                for variant,required in ((dict(operation='adjust',value=1),['increase']),(dict(operation='adjust',value=-1),['decrease']),
                    (dict(operation='max'),['increase']),(dict(operation='clear'),['decrease']),(dict(operation='set',value=0),['increase','decrease'])):
                    peer=self.fixture(screen=screen,transform=transform);obj=peer.client.observe();args=arguments(obj,action=action,args={key:target,**variant})
                    if set(required)<=set(directions):self.assertEqual(peer.client.execute(**args)['status'],'dispatched')
                    else:
                        with self.assertRaises(Unavailable):peer.client.execute(**args)
                        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests));self.assertEqual(peer.client.capability,obj['capability'])

    def test_set_does_not_infer_direction_or_identity_from_displayed_numbers(self):
        def transform(op,obj):
            if op=='R':obj['visible']['rows'][0].update(cells=[None,None,None,'999999'],controls=['increase'])
            return obj
        peer=self.fixture(screen='purchase',transform=transform);obj=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='change_purchase_quantity',args=dict(row='r0',operation='set',value=100000)))
        self.assertFalse(peer.client.commands)
        self.assertEqual(peer.client.execute(**arguments(obj,action='change_purchase_quantity',args=dict(row='r0',operation='max')))['status'],'dispatched')

    def test_fresh_rows_controls_targets_and_old_grants_fail_before_send(self):
        peer=self.fixture(screen='purchase');old=peer.client.observe();fresh=peer.client.observe()
        for obj,action,args in ((old,'change_purchase_quantity',dict(row='r0',operation='clear')),(fresh,'change_purchase_quantity',dict(row='r31',operation='max')),
                               (fresh,'position_purchase_list',dict(target='t2'))):
            if action=='position_purchase_list':peer.client.purchase_targets=frozenset({'t0'})
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action=action,args=args))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        self.assertEqual(peer.client.execute(**arguments(fresh,action='change_purchase_quantity',args=dict(row='r0',operation='set',value=0)))['status'],'dispatched')

    def test_scroll_amounts_all_use_shown_direction_not_model_extent(self):
        def transform(op,obj):
            if op=='R':obj['visible']['scroll']=dict(up=False,down=True)
            return obj
        for amount in a.PURCHASE_AMOUNTS:
            peer=self.fixture(screen='purchase',transform=transform);obj=peer.client.observe()
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='scroll_purchase',args=dict(direction='up',amount=amount)))
            self.assertEqual(peer.client.execute(**arguments(obj,action='scroll_purchase',args=dict(direction='down',amount=amount)))['status'],'dispatched')

    def test_sell_desired_value_is_not_toggle_and_old_commit_paths_stay_none(self):
        for enabled in (False,True):
            peer=self.fixture(screen='manufacture_info');obj=peer.client.observe()
            self.assertEqual(peer.client.execute(**arguments(obj,action='set_production_sell',args={'enabled':enabled}))['status'],'dispatched')
            self.assertTrue(next(x for x in peer.requests if x.startswith(b'E ')).endswith(b' true' if enabled else b' false'))
        for action in ('accept_production','stop_production'):
            peer=self.fixture(screen='manufacture_info');obj=peer.client.observe()
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action=action,args={'input':'left_click'}))
            self.assertEqual(peer.client.execute(**arguments(obj,action=action,args={}))['status'],'dispatched')

    def test_immutable_cache_clear_state_change_and_unsupported(self):
        peer=self.fixture(screen='purchase');obj=peer.client.observe();cached=peer.client.purchase_rows
        obj['visible']['rows'][0]['controls'].clear();obj['visible']['rows'].append(dict(handle='r1',cells=[None]*4,selected=False,controls=['increase']))
        self.assertEqual(peer.client.purchase_rows,cached);self.assertNotIn(('r1','increase'),cached)
        peer.screen='manufacture_info';obj=peer.client.observe();self.assertFalse(peer.client.purchase_rows)
        obj['visible']['quantity_controls'][0]['increase']=False
        self.assertIn(('engineers','increase'),peer.client.quantity_directions)
        peer.screen='unsupported';peer.client.observe();self.assertFalse(peer.client.quantity_directions or peer.client.purchase_targets)
        peer.client.close();self.assertFalse(peer.client.purchase_rows)

    def test_duplicate_is_status_only_and_changed_operation_value_conflicts(self):
        peer=self.fixture(screen='manufacture_info');obj=peer.client.observe()
        args=arguments(obj,action='change_production_quantity',args=dict(control='units',operation='set',value=5))
        receipt=peer.client.execute(**args);self.assertEqual(peer.client.execute(**args),receipt)
        for replacement in (dict(control='units',operation='set',value=6),dict(control='units',operation='adjust',value=5),dict(control='units',operation='max')):
            bad=copy.deepcopy(args);bad['args']=replacement
            with self.assertRaises(Unavailable):peer.client.execute(**bad)
        self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1);self.assertEqual(sum(x.startswith(b'S ') for x in peer.requests),1)

    def test_events_grant_neutral_and_replayed_records_immutable(self):
        peer=self.fixture(screen='purchase');obj=peer.client.observe()
        peer.public_events=[dict(seq=1,kind='presentation',state='purchase',visible=obj['visible'])]
        before=(peer.client.capability,peer.client.revision,peer.client.quantity_directions,peer.client.purchase_rows,peer.client.purchase_targets)
        first=peer.client.read_events(EPOCH,0,16);self.assertEqual(peer.client.read_events(EPOCH,0,16)['events'],first['events'])
        self.assertEqual(before,(peer.client.capability,peer.client.revision,peer.client.quantity_directions,peer.client.purchase_rows,peer.client.purchase_targets))
        peer.public_events[0]['visible']['total']='Changed'
        with self.assertRaises(Unavailable):peer.client.read_events(EPOCH,0,16)
        self.assertEqual(peer.client.state,'CLOSED')

    def test_single_flight_until_terminal_receipt(self):
        entered,release=threading.Event(),threading.Event()
        def transform(op,obj):
            if op=='E':entered.set();release.wait(1)
            return obj
        peer=self.fixture(screen='manufacture_info',transform=transform,timeout=1)
        obj=peer.client.observe();args=arguments(obj,action='change_production_quantity');out=[]
        worker=threading.Thread(target=lambda:out.append(peer.client.execute(**args)));worker.start();self.assertTrue(entered.wait(1))
        try:
            self.assertEqual(peer.client.state,'PENDING');self.assertIsNone(peer.client.capability)
            with self.assertRaises(Unavailable):peer.client.execute(**args)
            with self.assertRaises(Unavailable):peer.client.observe()
        finally:release.set();worker.join(2)
        self.assertFalse(worker.is_alive());self.assertEqual(out[0]['status'],'dispatched')
        self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)

    def test_transport_unknown_or_initial_pending_never_replays(self):
        for mode in ('pending','silent','unknown'):
            def transform(op,obj):
                if op=='E' or (op=='S' and mode=='unknown'):
                    if mode=='silent':return None
                    obj.update(status='pending' if mode=='pending' else 'unknown_after_dispatch',observation=None)
                return obj
            peer=self.fixture(screen='purchase',transform=transform);obj=peer.client.observe();args=arguments(obj,action='change_purchase_quantity')
            if mode=='unknown':
                self.assertEqual(peer.client.execute(**args)['status'],'unknown_after_dispatch')
                self.assertEqual(peer.client.execute(**args)['status'],'unknown_after_dispatch')
                with self.assertRaises(Unavailable):peer.client.execute(**dict(args,command_id='9'*32))
            else:
                with self.assertRaises(a.ActionOutcomeUnknown):peer.client.execute(**args)
                self.assertEqual(peer.client.state,'CLOSED')
                with self.assertRaises(Unavailable):peer.client.execute(**args)
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)

    def test_real_server_returns_structured_uncertainty_not_native_success(self):
        peer=self.fixture(screen='purchase',transform=lambda op,obj:None if op=='E' else obj)
        server=adapter.Server(agent_control=peer.client);obj=call(server,'read_ui_state')['result']['structuredContent']
        reply=call(server,'execute_ui_action',arguments(obj,action='change_purchase_quantity'))['result']
        self.assertTrue(reply['isError']);error=reply['structuredContent']
        self.assertEqual((error['status'],error['semantics'],error['retryable']),('outcome_unknown','not_a_native_receipt',False))

if __name__=='__main__':unittest.main()
