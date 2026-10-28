# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic semantic-control checks: no GUI, game, assets or model calls."""
import copy
import io
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import threading
import unittest
from unittest.mock import patch
import adapter
import agent_control as a
import launch_presentation
from snapshots import Unavailable

EPOCH, CAPABILITY, COMMAND, CHALLENGE = ('1' * 32, '2' * 32, '3' * 32, '4' * 32)


def dump(obj):
    return json.dumps(obj, separators=(',', ':')).encode('utf-8')


def state(challenge=CHALLENGE, capability=CAPABILITY, revision=1, event_sequence=1, screen='battlescape'):
    visible = dict(selected_name='Jane', time_units=58, energy=62, health=33, morale=100, kneel_indicator='standing', reserve_indicator='none', kneel_reserve_indicator='off')
    visible['input_context'] = dict(mode='normal', selected_action='none', cursor='normal')
    visible['tactical'] = dict(mode='map', viewport=dict(x=0, y=0, width=320, height=140),
        message=None, view_level=None, grid=[dict(handle=f'g{i}',
            polygon=[dict(x=i*20,y=0),dict(x=i*20+16,y=0),dict(x=i*20+16,y=16),dict(x=i*20,y=16)],
            target=dict(x=i*20+8,y=8)) for i in range(2)], fragment_status='unavailable', fragments=[], truncated=False)
    visible['tactical']['map_context'] = dict(scope='viewport', selected_position=dict(x=0,y=0,z=0),
        cells=[dict(handle=f'g{i}', position=dict(x=i,y=0,z=0),
                    parts=dict(floor='floor',west='unknown',north='unknown',object='unknown'),
                    unit='own' if i == 0 else 'unknown') for i in range(2)], truncated=False)
    visible.update(hands=[dict(hand='left',ammo=6),dict(hand='right',ammo=None)],
                   enemy_buttons=[dict(handle='e0',number=1)],focus_target='g0',layers_indicator=None)
    ids = ['next_soldier', 'previous_soldier', 'kneel', 'open_pause_menu', 'end_turn',
           'map_primary', 'map_secondary', 'center_selected_unit', 'center_enemy', 'open_hand_action_menu', 'open_inventory']
    if screen in a.STATE_FIELDS:
        visible = {key: key.title() for key in a.STATE_FIELDS[screen]}; ids = sorted(a.STATE_ACTIONS[screen])
        if screen == 'load_list':
            visible['rows'] = [dict(handle='r0', cells=['MCP Test', '2026-10-24', '14:00'])]
        elif screen == 'save_list':
            visible.update(rows=[], new_save_name='MCP Test')
            ids = [x for x in ids if x != 'select_new_save_slot']
    elif screen == 'inventory':
        visible = dict(close_label='Close inventory', **dict.fromkeys(a.INVENTORY_TEXT_FIELDS), held=None, targets=[])
        ids = ['close_inventory']
    elif screen == 'dogfight':
        visible=dict(status=None,distance='560',ammo1='10',ammo2=None,interception='1',damage=None,buttons=[dict(id=mode,selected=mode=='standoff') for mode in a.DOGFIGHT_MODES])
        ids=['choose_dogfight_mode']
    elif screen == 'minimap':
        visible = dict(level_label='Level 1', viewport=dict(x=0,y=0,width=320,height=160),
                       terrain=[[0,0,4,4]], markers=[], targets=[dict(handle='m0',x=2,y=2)],
                       content_status='bounded', truncated=False)
        ids = sorted(a.STATE_ACTIONS[screen])
    elif screen == 'weapon_action_menu':
        visible = dict(rows=[dict(handle='a0',label='Snap Shot',accuracy='60%',time_units='12'),
                             dict(handle='a1',label='Aimed Shot',accuracy=None,time_units=None)])
        ids = sorted(a.STATE_ACTIONS[screen])
    elif screen == 'prime_grenade':
        visible = dict(title='Set Timer', timers=[dict(handle='t0', label='0'), dict(handle='t1', label='3')])
        ids = sorted(a.STATE_ACTIONS[screen])
    elif screen == 'debriefing':
        visible = dict(title='Mission complete', rating='Rating> Good', stats_label='Stats', table_status='unavailable',tables={k:dict(status='unavailable',rows=[]) for k in ('stats','recovery','total')})
        ids = ['close_debriefing']
    elif screen == 'new_game':
        visible = dict(title='Select Difficulty', ironman_note='One life', ironman=False,
                       difficulty=[dict(value=value, label=value.title(), selected=value == 'beginner') for value in a.DIFFICULTIES])
        ids = sorted(a.STATE_ACTIONS[screen])
    elif screen == 'build_base':
        visible = dict(title='Choose base site', globe=dict(width=256, height=200, sample_step=16,
            samples=[dict(x=64, y=80, terrain='textured'), dict(x=80, y=80, terrain='untextured')]))
        ids = ['place_base_site']
    elif screen == 'geoscape':
        visible = {field: field.title() for field in a.GEOSCAPE_FIELDS}
        visible['buttons'] = [dict(id=value, label=value.title()) for value in a.GEOSCAPE_BUTTONS]
        visible.update(speed=None, speeds=[])
        ids = ['open_funding','open_interception']
    elif screen in ('basescape', 'base_info'):
        visible = dict(base_name='Base',base_slots=[dict(handle='b0',selected=True),dict(handle='b1',selected=False)],
                       buttons=[dict(id=key,label=key.title()) for key in a.BASE_BUTTONS[screen]])
        if screen == 'basescape': visible.update(location='Area',funds='$1,000')
        else: visible.update(personnel_heading='Personnel',space_heading='Space',metrics=[dict(label='Soldiers',value='10:20')])
        ids=sorted(a.STATE_ACTIONS[screen])
    elif screen in ('base_stores', 'base_transfers'):
        visible=dict(title='Stores' if screen=='base_stores' else 'Transfers', headings=['Item','Quantity','Size'],
                     rows=[dict(handle='r0',cells=['Rifle','2','0.2'],selected=False)],scroll=dict(up=True,down=True))
        ids=sorted(a.STATE_ACTIONS[screen])
    elif screen in a.COMPLETION_BUTTONS:
        buttons = [dict(id=key, label={'ok': 'OK', 'go_to_base': 'Go To Base', 'destination': 'Allocate Manufacture', 'intercept': 'Intercept', 'center': 'Center', 'cancel': 'Cancel'}[key]) for key in a.COMPLETION_BUTTONS[screen]]
        if screen == 'items_arriving':
            visible = dict(title='Items Arriving', headings=['Item', 'Quantity', 'Destination'],
                           rows=[dict(handle='r0', cells=['Scientist', '1', 'Base'], selected=False)],
                           scroll=dict(up=True, down=True), buttons=buttons)
        elif screen == 'production_complete':
            visible = dict(message='Production of Medi-Kit is complete at Base', buttons=buttons)
        else:
            visible = dict(title='UFO-1', detected_label='Detected', hyperwave_label=None,
                           details_status='unavailable', buttons=buttons)
        ids = sorted(a.STATE_ACTIONS[screen])
    elif screen in a.RESEARCH_COMPLETION_FIELDS:
        visible = {key: None for key in a.RESEARCH_COMPLETION_FIELDS[screen].split()}
        visible['buttons'] = [dict(id=key, label=key.title()) for key in a.RESEARCH_COMPLETION_BUTTONS[screen]]
        if screen in a.RESEARCH_NOTICE_STATES:
            visible.update(title='New possibilities', rows=[dict(handle='r0',cells=['Shown research'],selected=False)],
                           scroll=dict(up=True,down=True))
        elif screen == 'ufopaedia_article':
            visible.update(title='Pistol',body_lines=['A shown line',None,'Another line'],details_status='unavailable')
        elif screen == 'research_complete': visible.update(title='Research Complete',topic='Pistol')
        elif screen == 'research_required': visible['message']='Research required'
        else: visible.update(title='Mission Alert',city='City')
        ids=list(a.RESEARCH_COMPLETION_ACTION_ORDER[screen])
    elif screen in a.OUTFITTING_FIELDS:
        visible = {key: None for key in a.OUTFITTING_FIELDS[screen].split()}
        visible['buttons'] = [dict(id=key,label=key.title()) for key in a.OUTFITTING_BUTTONS[screen]]
        if screen in a.OUTFITTING_LISTS:
            headings, columns = a.OUTFITTING_LISTS[screen]
            visible.update(title='Outfitting',headings=['Heading']*headings,
                rows=[dict(handle='r0',cells=['Shown']+[None]*(columns-1),selected=False)],scroll=dict(up=True,down=True))
            if screen == 'craft_equipment': visible['rows'][0]['controls']=['increase','decrease']
        elif screen == 'soldier_info':
            visible.update(name='MCP Healthy',rank='Rookie',missions='Missions>0',kills='Kills>0',
                           stats=[dict(label='Time Units',value='60')])
        else:
            visible.update(name='MCP Transport',damage='Damage>0%',fuel='Fuel>100%',
                           weapons=[dict(name=None,ammo=None),dict(name=None,ammo=None)])
        ids=sorted(a.STATE_ACTIONS[screen])
    elif screen in a.PROJECT_BUTTONS:
        buttons=[dict(id=key,label=key.title()) for key in a.PROJECT_BUTTONS[screen]]
        if screen in a.PROJECT_LISTS:
            summary,columns=a.PROJECT_LISTS[screen]
            visible=dict(title='Projects',summary=['Available>50']*summary,headings=['Heading']*columns,
                rows=[dict(handle='r0',cells=['Project']+['1']*(columns-1),selected=False)],scroll=dict(up=True,down=True),buttons=buttons)
        elif screen=='manufacture_start':
            visible=dict(title='Medi-Kit',engineer_hours='Hours>420',cost='$28,000',space='Space>4',materials_heading=None,
                headings=[None]*3,rows=[],scroll=dict(up=False,down=False),buttons=buttons)
        else:
            visible=dict(title='Project',available='Available>50',space='Space>50',allocated='Allocated>0',buttons=buttons)
            if screen=='manufacture_info':visible.update(units='>1',monthly_profit='$0',sell=dict(label='Sell',pressed=False),
                quantity_controls=[dict(control=control,increase=True,decrease=True) for control in a.QUANTITY_CONTROLS])
        ids=sorted(a.STATE_ACTIONS[screen])
        if screen=='manufacture_start':ids.remove('scroll_visible_list')
    elif screen == 'purchase':
        visible=dict(title='Purchase / Hire',funds='$1,000',total='$0',space_used='0.0',headings=['Item','Cost'],
            category=dict(label='All'),rows=[dict(handle='r0',cells=['Rifle','$3,000','0','0'],selected=False,controls=['increase','decrease'])],
            scroll=dict(up=True,down=True),scrollbar=dict(targets=[dict(handle='t0',position='top'),dict(handle='t1',position='middle'),dict(handle='t2',position='bottom')]),
            buttons=[dict(id='accept',label='OK'),dict(id='cancel',label='Cancel')])
        ids=sorted(a.STATE_ACTIONS[screen])
    elif screen in a.INTERCEPTION_BUTTON_ACTIONS:
        visible=dict(buttons=[dict(id=key,label=key.title()) for key in a.INTERCEPTION_BUTTON_ACTIONS[screen].values()])
        if screen == 'monthly_report': visible.update(title='Monthly Report',month='Jan 1999',rating='Rating: OK',income='Income',maintenance='Maintenance',balance='Balance',description='Council report',failure=None)
        elif screen == 'landing_confirmation': visible.update(message='SKYRANGER-1 ready to land near LANDING SITE-1',prompt='Begin Mission?')
        elif screen == 'psi_training': visible.update(title='Psionic Training', bases=['Shown base'],rows=[dict(handle='r0',cells=['Shown base'])])
        elif screen == 'lost_in_service': visible.update(title='Lost in Service',table_status='shown',rows=[dict(cells=['Shown name','','Rookie','','Kills: 0'])])
        elif screen == 'statistics': visible.update(title='Defeat')
        elif screen == 'slideshow': visible.update(caption='Current caption',buttons=[dict(id='advance',label='Advance current slide')])
        elif screen == 'commendations': visible.update(title='Medals', rows=[])
        elif screen == 'promotions': visible.update(title='Promotions',headings=['Name','New Rank','Base'],rows=[])
        elif screen == 'alien_base_discovered': visible.update(message='X-Com agents have located an alien base in Shown Region')
        elif screen == 'ufo_lost': visible.update(message='UFO-1\nTracking lost')
        elif screen == 'craft_error': visible.update(message='Not enough ammunition to rearm craft at base')
        elif screen == 'craft_destination_selection': visible.update(title='SELECT DESTINATION', targets=[dict(row='r0', x=100, y=70, width=4, height=4)])
        elif screen == 'geoscape_craft_navigation': visible.update(title='INTERCEPTOR-1',status='Status: Patrolling',redirect=None)
        elif screen == 'craft_patrol': visible.update(message='INTERCEPTOR-1\nhas reached\nWAY POINT-1',patrolling='Now patrolling')
        elif screen == 'low_fuel': visible.update(title='Low Fuel',message='INTERCEPTOR-1 is returning to base')
        elif screen == 'dogfight_error': visible.update(craft_name='BARRACUDA-1',message='Unable to engage')
        elif screen == 'interception_confirm': visible['target']='UFO-1'
        else: visible.update(title='Select Craft',headings=[None]*4,rows=[dict(handle='r0',cells=['Shown']*4,selected=False)],scroll=dict(up=True,down=True))
        ids=sorted(a.STATE_ACTIONS[screen])
        if screen == 'monthly_report':
            ids=['acknowledge_monthly_report'];visible['buttons']=[dict(id='ok',label='Ok')]
        if screen == 'geoscape_craft_navigation':
            ids=['cancel_craft_navigation','open_craft_destination']
            visible['buttons']=[b for b in visible['buttons'] if b['id'] in ('cancel','target')]
    elif screen in a.TRANSFER_BUTTON_ACTIONS:
        visible = dict(title='Transfer', buttons=[dict(id=key,label=key.title()) for key in a.TRANSFER_BUTTON_ACTIONS[screen].values()])
        if screen == 'transfer_confirm': visible.update(cost='Cost',total='$1,000')
        else:
            destination = screen == 'transfer_destination'
            columns = 2 if destination else 4
            row = dict(handle='r0',cells=['Shown']*columns,selected=False)
            if not destination: row['controls']=['increase','decrease']
            visible.update(headings=[None]*columns,rows=[row],scroll=dict(up=True,down=True))
            if destination: visible['funds']='$1,000'
        ids = sorted(a.STATE_ACTIONS[screen])
    elif screen == 'purchase_category':
        visible=dict(control='category',label='All',rows=[dict(handle='r0',cells=['All'],selected=True)],scroll=dict(up=True,down=True))
        ids=sorted(a.STATE_ACTIONS[screen])
    elif screen == 'new_battle_dropdown':
        visible = dict(control='mission', label='Mission',
                       rows=[dict(handle='r0', cells=['Ground assault'], selected=False),
                             dict(handle='r1', cells=['Base defense'], selected=True)],
                       scroll=dict(up=True, down=True))
        ids = sorted(a.STATE_ACTIONS[screen])
    elif screen == 'funding':
        visible = dict(title='International Relations', headings=['Country', 'Funding', 'Change'],
                       rows=[dict(handle='r0', cells=['Nation', '$1,000', '+$10'], selected=False),
                             dict(handle='r1', cells=['TOTAL', '$1,000', ''], selected=False)],
                       scroll=dict(up=True, down=True))
        ids = sorted(a.STATE_ACTIONS[screen])
    elif screen == 'unsupported':
        visible, ids, capability = {}, [], None
    if not ids:
        capability = None
    def action_label(action):
        if action == 'choose_visible_craft_destination': return 'Select visible target'
        if action == 'open_psi_training_base': return 'Open psi training base'
        if action == 'choose_dogfight_mode': return 'Choose dogfight mode'
        if screen in a.INTERCEPTION_BUTTON_ACTIONS:
            if action == 'choose_visible_row': return 'Choose visible row'
            if action == 'scroll_visible_list': return 'Scroll visible list'
            return next(button['label'] for button in visible['buttons'] if button['id'] == a.INTERCEPTION_BUTTON_ACTIONS[screen][action])
        if screen in a.TRANSFER_BUTTON_ACTIONS:
            if action in a.QUANTITY_SEMANTIC_LABELS: return a.QUANTITY_SEMANTIC_LABELS[action]
            if action == 'choose_visible_row': return 'Choose visible row'
            if action == 'scroll_visible_list': return 'Scroll visible list'
            return next(button['label'] for button in visible['buttons'] if button['id'] == a.TRANSFER_BUTTON_ACTIONS[screen][action])
        if screen in a.RESEARCH_COMPLETION_FIELDS:
            if action == 'scroll_article_text': return 'Scroll article text'
            if action == 'scroll_visible_list': return 'Scroll visible list'
            return next(button['label'] for button in visible['buttons'] if button['id'] == a.RESEARCH_COMPLETION_BUTTON_ACTIONS[screen][action])
        if screen in a.OUTFITTING_FIELDS:
            if action in a.OUTFITTING_SEMANTIC_LABELS: return a.OUTFITTING_SEMANTIC_LABELS[action]
            if action == 'scroll_visible_list': return 'Scroll visible list'
            return next(button['label'] for button in visible['buttons'] if button['id'] == a.OUTFITTING_BUTTON_ACTIONS[screen][action])
        if screen in a.COMPLETION_BUTTONS:
            if action == 'scroll_visible_list': return 'Scroll visible list'
            return next(button['label'] for button in visible['buttons'] if button['id'] == a.COMPLETION_BUTTON_ACTIONS[screen][action])
        if action in a.QUANTITY_SEMANTIC_LABELS:return a.QUANTITY_SEMANTIC_LABELS[action]
        if action=='set_production_sell':return visible['sell']['label']
        if action=='open_purchase_category':return visible['category']['label']
        if action in ('accept_purchase','cancel_purchase'):return next(x['label'] for x in visible['buttons'] if x['id']==action.split('_')[0])
        if screen=='purchase_category' and action in ('choose_visible_row','scroll_visible_list','close_dropdown'):
            return {'choose_visible_row':'Choose visible row','scroll_visible_list':'Scroll visible list','close_dropdown':'Close choices'}[action]
        if action == 'select_base': return 'Select base'
        mapping = ({'open_interception':'intercept','open_funding':'funding','open_bases':'bases','open_pause_menu':'options'} if screen=='geoscape'
                   else a.BASE_BUTTON_ACTIONS.get(screen,a.PROJECT_BUTTON_ACTIONS.get(screen,{})))
        if screen=='new_research' and action=='select_research_project':return 'Select research project'
        if action==a.PROJECT_SELECTION_ACTIONS.get(screen):return visible['rows'][0]['cells'][0]
        if action==a.PROJECT_STAFF_ACTIONS.get(screen):return next(x['label'] for x in visible['buttons'] if x['id'] in a.STAFF_DIRECTIONS)
        if action in mapping: return next(x['label'] for x in visible['buttons'] if x['id']==mapping[action])
        return (visible['rows'][0]['label'] if action == 'choose_weapon_action' else visible['timers'][0]['label'] if action == 'choose_grenade_timer' else 'Cancel timer' if action == 'cancel_grenade_timer' else action.replace('_', ' ').title())
    return dict(schema=a.SCHEMA, mode=a.MODE, type='state', challenge=challenge, epoch=EPOCH,
                revision=revision, capability=capability, state=screen, visible=visible, cursor=None,
                actions=[dict(id=action, label=action_label(action),
                              label_source=a.label_source(screen, action), argument_kind=a.ARGUMENT_KINDS[action]) for action in ids],
                event_sequence=event_sequence, semantics='presented_after_request')


def arguments(obj=None, command_id=COMMAND, action='kneel', args=None):
    obj = state() if obj is None else obj
    if args is None:
        args = {'move_cursor': {'x':0,'y':0}, 'select_difficulty': {'value': 'beginner'}, 'set_ironman': {'value': False},
                'replace_base_name': {'text': 'Alpha'}, 'replace_new_save_name': {'text': 'MCP Test'}, 'load_saved_game': {'row': 'r0'}, 'place_base_site': {'x': 64, 'y': 80},
                'open_new_battle_choice': {'control': 'mission'}, 'choose_visible_row': {'row': 'r0'}, 'open_psi_training_base': {'row': 'r0'}, 'choose_visible_craft_destination': {'row': 'r0'},
                'scroll_visible_list': {'direction': 'down'}, 'scroll_article_text': {'direction': 'down'}, 'map_primary': {'cell':'g0'},
                'map_secondary': {'cell':'g0'}, 'center_enemy': {'button':'e0'},
                'open_hand_action_menu': {'hand':'left'}, 'choose_weapon_action': {'row':'a0'}, 'inventory_hover': {'target':'v0'},
                'inventory_activate': {'target':'v0'}, 'choose_grenade_timer': {'timer':'t0'},
                'camera_pan': {'direction':'left'}, 'minimap_center': {'target':'m0'},
                'choose_dogfight_mode': {'mode':'standoff'}, 'choose_geoscape_speed': {'speed':'s0'}, 'select_base': {'target':'b0'},
                'select_research_project': {'row':'r0'}, 'select_production_project': {'row':'r0'},
                'adjust_scientists': {'direction':'increase'}, 'adjust_engineers': {'direction':'increase'},
                'change_transfer_quantity': {'row':'r0','operation':'adjust','value':1},
                'change_production_quantity': {'control':'engineers','operation':'adjust','value':1},
                'set_production_sell': {'enabled':True},
                'change_purchase_quantity': {'row':'r0','operation':'adjust','value':1},
                'scroll_purchase': {'direction':'down','amount':'step'},
                'position_purchase_list': {'target':'t0'},
                'toggle_psi_training': {'row':'r0'}, 'select_soldier': {'row':'r0'}, 'select_craft': {'row':'r0'},
                'open_craft_soldier_info': {'row':'r0'}, 'select_craft_armor_soldier': {'row':'r0'},
                'select_soldier_armor': {'row':'r0'}, 'set_craft_soldier': {'row':'r0','assigned':True},
                'change_craft_equipment': {'row':'r0','operation':'adjust','value':1}}.get(action, {})
    return {**{k: obj[k] for k in ('epoch', 'capability', 'revision')}, 'command_id': command_id, 'action': action, 'args': args}


def result(command_id=COMMAND, status='dispatched', event_sequence=2, observation=None):
    return dict(schema=a.SCHEMA, mode=a.MODE, type='result', command_id=command_id, status=status,
                event_sequence=event_sequence, observation=observation, semantics='dispatch_not_success')


def call(server, name, args=None):
    server.state = 'ready'
    return server.handle(dict(jsonrpc='2.0', id=1, method='tools/call', params=dict(name=name, arguments={} if args is None else args)))


FIXTURE_REASONS = frozenset((
    'verification_epoch_capability', 'verification_revision_action',
    'verification_duplicate', 'verification_arity', 'unexpected_request',
    'worker_timeout', 'worker_os_error', 'worker_value_error', 'worker_assertion',
    'worker_other', 'peer_close_failure', 'cleanup_worker_alive',
    'cleanup_join_failure', 'cleanup_client_close_failure'))


class FixtureFailure(AssertionError):
    """Fixed synthetic-peer reason, never the original exception or payload."""
    def __init__(self, reason):
        self.reason = reason if type(reason) is str and reason in FIXTURE_REASONS else 'worker_other'
        super().__init__(self.reason)


def fixture_check(condition, reason):
    if not condition:
        raise FixtureFailure(reason)


class NativeFixture:
    def __init__(self, *, hello=None, transform=None, timeout=0.15, screen='battlescape', transitions=None):
        parent, self.peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.client = a.AgentControlClient(parent, timeout=timeout, action_timeout=timeout, startup_timeout=timeout)
        self.peer.settimeout(1)
        self.requests, self.results, self.errors = [], {}, []
        self.worker_phase = 'hello_send'
        self.worker_failure_phase = None
        self.worker_reason = None
        self.worker_close = 'not_attempted'
        self.worker_finished = False
        self.cleanup_peer_close = 'not_attempted'
        self.transform, self.screen = transform, screen
        self.transitions = transitions or {}
        self.revision, self.sequence, self.counter = 1, 0, 10
        self.difficulty, self.ironman, self.base_name = 'beginner', False, ''
        self.control = 'mission'
        self.latest = None
        self.public_events = []
        self.event_page_limit = a.MAX_EVENTS
        self.audio_status = 'active'
        self.hello = hello or dict(schema=a.SCHEMA, mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output')
        self.thread = threading.Thread(target=self.run); self.thread.start()

    def observation(self, challenge, capability):
        obj = state(challenge, capability, self.revision, self.sequence, self.screen)
        if self.screen == 'new_game':
            obj['visible']['ironman'] = self.ironman
            for choice in obj['visible']['difficulty']:
                choice['selected'] = choice['value'] == self.difficulty
        if self.screen == 'new_battle_dropdown':
            obj['visible']['control'] = self.control
            obj['visible']['label'] = self.control.title()
        if self.screen == 'base_name':
            obj['visible']['name'] = self.base_name
            if not self.base_name:
                obj['actions'] = [item for item in obj['actions'] if item['id'] != 'confirm_base_name']
        return obj

    def _record_failure(self, reason):
        if not self.errors:
            # Create but do not raise here: no original traceback/context retained.
            failure = FixtureFailure(reason)
            self.errors.append(failure)
            self.worker_reason = failure.reason
            self.worker_failure_phase = self.worker_phase

    def run(self):
        try:
            self.worker_phase = 'hello_send'
            self.peer.send(dump(self.hello))
            while True:
                self.worker_phase = 'receive'
                raw = self.peer.recv(4096)
                if not raw:
                    self.worker_reason = 'eof'
                    return
                self.worker_phase = 'decode'
                self.requests.append(raw); command = raw.decode().split(' ')
                if command == ['H', a.SCHEMA]:
                    self.worker_phase = 'build_reply'
                    obj = dict(schema=a.SCHEMA, mode=a.MODE, status='ready')
                elif command[0] == 'R' and len(command) == 2:
                    self.worker_phase = 'build_reply'
                    self.counter += 1; self.sequence += 1
                    obj = self.observation(command[1], f'{self.counter:032x}')
                    self.latest = copy.deepcopy(obj)
                elif command[0] == 'E' and len(command) >= 6:
                    _, ident, epoch, cap, revision, action = command[:6]
                    self.worker_phase = 'verify'
                    fixture_check(epoch == EPOCH and cap == self.latest['capability'], 'verification_epoch_capability')
                    fixture_check(int(revision) == self.revision and action in [i['id'] for i in self.latest['actions']], 'verification_revision_action')
                    fixture_check(ident not in self.results, 'verification_duplicate')
                    kind = a.ARGUMENT_KINDS[action]
                    expected_count = 6 if kind == 'none' else 8 if kind in ('point', 'cursor_position', 'purchase_scroll', 'crew_assignment') else 7
                    if kind in ('production_quantity','purchase_quantity','equipment_quantity','transfer_quantity'):
                        expected_count = 9 if command[7] in ('adjust','set') else 8
                    fixture_check(len(command) == expected_count, 'verification_arity')
                    self.worker_phase = 'build_reply'
                    if action == 'select_difficulty': self.difficulty = command[6]
                    elif action == 'set_ironman': self.ironman = command[6] == '1'
                    elif action == 'open_new_battle_choice': self.control = command[6]
                    elif action == 'replace_base_name': self.base_name = '' if command[6] == '-' else bytes.fromhex(command[6]).decode('utf-8')
                    self.revision += 1; self.sequence += 1
                    self.screen = self.transitions.get((self.screen, action), self.screen)
                    obj = result(ident, event_sequence=self.sequence,
                                 observation=self.observation(None, None))
                    self.results[ident] = copy.deepcopy(obj)
                elif command[0] == 'V' and len(command) == 5:
                    self.worker_phase = 'build_reply'
                    _, challenge, epoch, after, limit = command
                    after, limit = int(after), int(limit)
                    newest = len(self.public_events); oldest = max(1, newest-a.EVENT_CAPACITY+1)
                    if epoch != EPOCH or after > newest:
                        obj = dict(schema=a.SCHEMA, mode=a.MODE, type='error', status='unavailable')
                    else:
                        events = copy.deepcopy(self.public_events[max(after, oldest-1):][:min(limit, self.event_page_limit)])
                        obj = dict(schema=a.SCHEMA, mode=a.MODE, type='events', challenge=challenge, epoch=epoch,
                            audio_history=self.hello['audio_history'],
                            audio_status=self.audio_status if self.hello['audio_history']=='mixed_output' else 'unavailable',
                            sound_playback=getattr(self,'sound_playback',dict(playback_epoch=0,status='inactive')),
                            after=after, oldest=oldest, newest=newest,
                            next_cursor=events[-1]['seq'] if events else after, gap=after+1<oldest, events=events)
                elif command[0] == 'S' and len(command) == 2:
                    self.worker_phase = 'build_reply'
                    obj = copy.deepcopy(self.results[command[1]])
                else:
                    raise FixtureFailure('unexpected_request')
                if self.transform is not None:
                    self.worker_phase = 'transform'
                    obj = self.transform(command[0], obj)
                if obj is not None:
                    self.worker_phase = 'send_reply'
                    self.peer.send(obj if isinstance(obj, bytes) else dump(obj))
        except (BrokenPipeError, ConnectionResetError):
            # Exception class only, not a claim about the peer's causal state.
            self.worker_reason = 'broken_pipe_or_reset'
            self.worker_failure_phase = self.worker_phase
        except BaseException as error:
            reason = (error.reason if isinstance(error, FixtureFailure) else
                      'worker_timeout' if isinstance(error, TimeoutError) else
                      'worker_os_error' if isinstance(error, OSError) else
                      'worker_value_error' if isinstance(error, ValueError) else
                      'worker_assertion' if isinstance(error, AssertionError) else 'worker_other')
            self._record_failure(reason)
        finally:
            try:
                self.peer.close()
                self.worker_close = 'closed'
            except BaseException:
                self.worker_close = 'failed'
                self._record_failure('peer_close_failure')
            self.worker_phase = 'exit'
            self.worker_finished = True

    def close(self):
        cleanup_reason = None
        alive = False
        try:
            try:
                self.client.close()
            except BaseException:
                cleanup_reason = 'cleanup_client_close_failure'
            if cleanup_reason is None:
                try:
                    self.thread.join(2)
                    alive = self.thread.is_alive()
                except BaseException:
                    cleanup_reason = 'cleanup_join_failure'
        finally:
            try:
                self.peer.close() # Idempotent owned-endpoint fallback, no second wait.
                self.cleanup_peer_close = 'closed'
            except BaseException:
                self.cleanup_peer_close = 'failed'
                if cleanup_reason is None:
                    cleanup_reason = 'peer_close_failure'
        if cleanup_reason is not None:
            raise FixtureFailure(cleanup_reason) from None
        fixture_check(not alive, 'cleanup_worker_alive')
        if self.errors:
            raise FixtureFailure(self.errors[0].reason) from None


class ValidationTests(unittest.TestCase):
    def test_exact_hello_and_version_identity(self):
        hello = dict(schema=a.SCHEMA, mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output')
        self.assertEqual(a.validate_hello(dump(hello)), EPOCH)
        for key, value in (('schema', 'experimental-stance/1'), ('schema', 'experimental-stance/2'), ('mode', 'supervised'), ('epoch', 'A' * 32),
                           ('ledger_capacity', 256.0), ('ledger_capacity', True), ('extra', 'hidden')):
            with self.subTest(key=key, value=value), self.assertRaises(Unavailable):
                a.validate_hello(dump(dict(hello, **{key: value})))

    def test_duplicate_keys_nonfinite_nonjson_and_oversize(self):
        for raw in (b'{"schema":1,"schema":2}', b'{"a":NaN}', b'\xff', b'', b'x' * (a.MAX_PACKET + 1)):
            with self.subTest(raw=raw[:30]), self.assertRaises(Unavailable): a.parse(raw)

    def test_all_supported_shapes_and_nullable_visible_text(self):
        for screen in a.STATE_ACTIONS:
            obj = state(screen=screen)
            if screen in a.STATE_FIELDS:
                obj['visible'] = dict.fromkeys(a.STATE_FIELDS[screen])
                if screen in ('save_list', 'load_list'):
                    obj['visible']['rows'] = []
                    if screen == 'save_list': obj['visible']['new_save_name'] = None
                    obj['actions'] = [x for x in obj['actions'] if x['id'] == 'cancel_menu']
                if screen == 'new_battle':
                    obj['actions'] = [item for item in obj['actions'] if item['id'] != 'open_new_battle_choice']
            a.validate_state(obj, CHALLENGE, EPOCH)
        obj = state()
        for field in ('selected_name', 'time_units', 'energy', 'health', 'morale', 'kneel_indicator'):
            obj['visible'][field] = None
        a.validate_state(obj, CHALLENGE, EPOCH)

    def test_state_rejects_unknown_hidden_or_malformed_fields(self):
        cases = []
        for key, value in (('schema', 'other'), ('mode', 'supervised'), ('challenge', '5' * 32), ('epoch', '5' * 32),
                           ('state', 'hidden_world'), ('revision', True), ('revision', -1), ('revision', 2**53),
                           ('event_sequence', 1.0), ('capability', 'BAD'), ('hidden_unit_id', 123), ('semantics', 'world_state')):
            obj = state(); obj[key] = value; cases.append(obj)
        for key, value in (('hidden_unit_id', 123), ('time_units', True), ('health', -1), ('morale', 100.0),
                           ('selected_name', 'x' * 4097), ('kneel_indicator', 'unknown')):
            obj = state(); obj['visible'][key] = value; cases.append(obj)
        for key, value in (('target', 123), ('id', 'attack_any_enemy'), ('label_source', 'hidden_rule')):
            obj = state(); obj['actions'][0][key] = value; cases.append(obj)
        obj = state(); obj['actions'].append(copy.deepcopy(obj['actions'][0])); cases.append(obj)
        obj = state(); obj['actions'].append(dict(id='stand', label='Stand', label_source='visible_control_semantics')); cases.append(obj)
        obj = state(screen='unsupported'); obj['capability'] = CAPABILITY; cases.append(obj)
        obj = state(screen='next_turn'); obj['actions'][0]['id'] = 'kneel'; cases.append(obj)
        obj = state(); obj['actions'] = []; cases.append(obj)
        obj = state(); del obj['visible']; cases.append(obj)
        for obj in cases:
            with self.subTest(obj=obj), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_exact_action_args_and_allowlist(self):
        for action in a.ACTIONS: a.validate_action_arguments(arguments(action=action))
        cases = [None, [], {}, False]
        for name in arguments():
            obj = arguments(); del obj[name]; cases.append(obj)
        for key, value in (('approved', True), ('desired', 'kneel'), ('x', 12), ('unit_id', 4),
                           ('action', 'toggle_kneel'), ('action', 'attack'), ('action', True), ('epoch', 'A' * 32),
                           ('capability', 'z' * 32), ('revision', True), ('revision', 1.0), ('revision', -1), ('command_id', 'short')):
            cases.append(dict(arguments(), **{key: value}))
        for args in cases:
            with self.subTest(args=args), self.assertRaises(Unavailable): a.validate_action_arguments(args)

    def test_inventory_action_is_visible_icon_semantics(self):
        obj = state(screen='inventory')
        self.assertEqual(obj['actions'][0]['label_source'], 'visible_control_semantics')
        a.validate_state(obj, CHALLENGE, EPOCH)
        obj['actions'][0]['label_source'] = 'visible_text'
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_unicode_expansion_is_bounded_for_mcp_output(self):
        obj = state(screen='new_battle')
        for name in obj['visible']: obj['visible'][name] = '\U0001f600' * 1000
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_protocol_v1_v2_v3_v4_hello_ready_state_and_result_are_rejected(self):
        for schema in ('openxcom-agent-control/1', 'openxcom-agent-control/2', 'openxcom-agent-control/3', 'openxcom-agent-control/4'):
            with self.subTest(schema=schema):
                hello = dict(schema=schema, mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output')
                with self.assertRaises(Unavailable): a.validate_hello(dump(hello))
                with self.assertRaises(Unavailable):
                    a.validate_ready(dump(dict(schema=schema, mode=a.MODE, status='ready')))
                obj = state(); obj['schema'] = schema
                with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
                obj = result(); obj['schema'] = schema
                with self.assertRaises(Unavailable): a.validate_result(dump(obj), COMMAND, EPOCH)

    def test_exact_menu_state_action_matrix(self):
        expected = {
            'main_menu': {'new_battle', 'open_new_game', 'open_options', 'open_load_game'},
            'new_battle': {'start_battle', 'cancel_menu', 'open_new_battle_choice'},
            'new_game': {'cancel_menu', 'select_difficulty', 'set_ironman', 'start_new_game'}, 'options_shell': {'cancel_menu'},
            'pause_menu': {'open_load_game', 'open_save_game', 'open_options', 'resume_game'},
            'load_list': {'cancel_menu', 'load_saved_game'},
            'save_list': {'cancel_menu', 'select_new_save_slot', 'replace_new_save_name', 'save_new_game'},
            'save_in_progress': set(), 'load_in_progress': set(),
            'error_message': {'dismiss_error'},
        }
        self.assertEqual(len(a.ACTIONS), 188); self.assertEqual(len(a.STATE_ACTIONS), 81)
        for screen, actions in expected.items():
            self.assertEqual(a.STATE_ACTIONS[screen], actions)
            obj = state(screen=screen); a.validate_state(obj, CHALLENGE, EPOCH)
            for bad in set(a.ACTIONS) - actions:
                candidate = copy.deepcopy(obj)
                candidate['actions'] = [dict(id=bad, label='Visible', label_source='visible_text')]
                with self.subTest(screen=screen, bad=bad), self.assertRaises(Unavailable):
                    a.validate_state(candidate, CHALLENGE, EPOCH)

    def test_menu_shell_has_no_save_file_or_campaign_mutation_api(self):
        for action in ('save_game', 'load_game', 'start_campaign', 'abandon_game', 'delete_save',
                       'confirm_load', 'overwrite_save', 'apply_options', 'select_save_row'):
            with self.subTest(action=action), self.assertRaises(Unavailable):
                a.validate_action_arguments(arguments(action=action))
        for field in ('filename', 'path', 'rows', 'save_id', 'selected_campaign', 'hidden_options'):
            obj = state(screen='load_list'); obj['visible'][field] = 'PRIVATE'
            with self.subTest(field=field), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_menu_fields_are_exact_nullable_rendered_strings(self):
        fields = {
            'base_name': ('title', 'name'), 'options_shell': ('category', 'tooltip'),
            'pause_menu': ('title',), 'load_list': ('title', 'name_heading', 'date_heading', 'details'),
            'save_list': ('title', 'name_heading', 'date_heading', 'details'), 'error_message': ('message', 'heading'),
        }
        for screen, names in fields.items():
            self.assertEqual(a.STATE_FIELDS[screen], names)
            obj = state(screen=screen); obj['visible'] = dict.fromkeys(names)
            if screen in ('save_list', 'load_list'):
                obj['visible']['rows'] = []
                if screen == 'save_list': obj['visible']['new_save_name'] = None
                obj['actions'] = [x for x in obj['actions'] if x['id'] == 'cancel_menu']
            a.validate_state(obj, CHALLENGE, EPOCH)
            for value in (123, True, [], {}):
                candidate = copy.deepcopy(obj); candidate['visible'][names[0]] = value
                with self.subTest(screen=screen, value=value), self.assertRaises(Unavailable):
                    a.validate_state(candidate, CHALLENGE, EPOCH)

    def test_result_separates_dispatch_from_observation(self):
        for status in a.STATUSES: a.validate_result(dump(result(status=status)), COMMAND, EPOCH)
        observation = state(None, None, revision=2, event_sequence=2)
        a.validate_result(dump(result(observation=observation)), COMMAND, EPOCH)
        for change in (dict(semantics='success'), dict(command_id='7' * 32), dict(status='succeeded'), dict(approved=True),
                       dict(observation=state(None, CAPABILITY, revision=2, event_sequence=2)),
                       dict(observation=observation, status='unknown_after_dispatch')):
            with self.subTest(change=change), self.assertRaises(Unavailable):
                a.validate_result(dump(dict(result(), **change)), COMMAND, EPOCH)


class TypedValidationTests(unittest.TestCase):
    def test_exact_action_kind_catalog_not_peer_defined(self):
        self.assertEqual(len(a.ACTIONS), 188)
        self.assertEqual({k: v for k, v in a.ARGUMENT_KINDS.items() if v != 'none'}, {
            'move_cursor': 'cursor_position', 'select_difficulty': 'difficulty', 'set_ironman': 'boolean',
            'replace_base_name': 'text', 'replace_new_save_name': 'text', 'load_saved_game': 'visible_row', 'place_base_site': 'point',
            'open_new_battle_choice': 'choice_control', 'choose_visible_row': 'visible_row', 'open_psi_training_base': 'visible_row', 'choose_visible_craft_destination': 'visible_row',
            'scroll_visible_list': 'scroll_direction', 'scroll_article_text': 'scroll_direction', 'map_primary': 'grid_cell', 'map_secondary': 'grid_cell',
            'center_enemy': 'enemy_button', 'open_hand_action_menu': 'hand', 'choose_weapon_action': 'weapon_action',
            'inventory_hover': 'inventory_target', 'inventory_activate': 'inventory_target', 'choose_grenade_timer': 'grenade_timer',
            'camera_pan': 'camera_direction', 'minimap_center': 'minimap_target',
            'choose_dogfight_mode': 'dogfight_mode', 'choose_geoscape_speed': 'geoscape_speed', 'select_base': 'base_target',
            'select_research_project': 'project_row', 'select_production_project': 'project_row',
            'adjust_scientists': 'staff_direction', 'adjust_engineers': 'staff_direction',
            'change_production_quantity':'production_quantity','set_production_sell':'boolean_value',
            'change_transfer_quantity':'transfer_quantity','change_purchase_quantity':'purchase_quantity','scroll_purchase':'purchase_scroll','position_purchase_list':'purchase_position',
            'toggle_psi_training':'outfitting_row','select_soldier':'outfitting_row','select_craft':'outfitting_row',
            'open_craft_soldier_info':'outfitting_row','select_craft_armor_soldier':'outfitting_row',
            'select_soldier_armor':'outfitting_row','set_craft_soldier':'crew_assignment',
            'change_craft_equipment':'equipment_quantity'})
        for screen in a.STATE_ACTIONS:
            obj = state(screen=screen)
            for descriptor in obj['actions']:
                for bad in ('arbitrary', 'none' if descriptor['argument_kind'] != 'none' else 'point', {}, None):
                    changed = copy.deepcopy(obj)
                    next(x for x in changed['actions'] if x['id'] == descriptor['id'])['argument_kind'] = bad
                    with self.subTest(screen=screen, action=descriptor['id'], bad=bad), self.assertRaises(Unavailable):
                        a.validate_state(changed, CHALLENGE, EPOCH)
        obj = state(); del obj['actions'][0]['argument_kind']
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_typed_args_required_and_exact(self):
        invalid = {
            'kneel': ({'value': False}, None, [], False),
            'select_difficulty': ({}, {'value': 'hard'}, {'value': 0}, {'value': True}, {'value': 'beginner', 'index': 0}),
            'set_ironman': ({}, {'value': 0}, {'value': 1}, {'value': 'false'}, {'value': None}, {'value': False, 'toggle': True}),
            'replace_base_name': ({}, {'text': None}, {'text': b'Alpha'}, {'text': 123}, {'text': 'Alpha', 'confirm': True}),
            'place_base_site': ({}, {'x': 1}, {'x': True, 'y': 0}, {'x': 0, 'y': 1.0}, {'x': -1, 'y': 0},
                                {'x': 4096, 'y': 0}, {'x': 0, 'y': 4096}, {'x': 0, 'y': 0, 'world': True}),
        }
        for action, cases in invalid.items():
            for args in cases:
                obj = arguments(action=action); obj['args'] = args
                with self.subTest(action=action, args=args), self.assertRaises(Unavailable): a.validate_action_arguments(obj)
        obj = arguments(); del obj['args']
        with self.assertRaises(Unavailable): a.validate_action_arguments(obj)
        for action in ('generic_click', 'send_keys', 'set_base', 'advance_time', 'save_game'):
            with self.subTest(action=action), self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action=action))

    def test_utf8_byte_limit_controls_and_surrogates(self):
        for text in ('', 'A' * 128, 'é' * 64, '\U0001f600' * 32, 'e\u0301', ' A '):
            a.validate_action_arguments(arguments(action='replace_base_name', args={'text': text}))
        invalid = ['A' * 129, 'é' * 65, '\U0001f600' * 33, '\ud800', '\udfff']
        invalid += ['A' + chr(c) + 'B' for c in [*range(32), *range(127, 160)]]
        for text in invalid:
            with self.subTest(text=repr(text)), self.assertRaises(Unavailable):
                a.validate_action_arguments(arguments(action='replace_base_name', args={'text': text}))

    def test_canonical_wire_suffixes_and_no_normalization(self):
        cases = [('start_new_game', {}, ''), ('confirm_base_name', {}, ''),
                 ('select_difficulty', {'value': 'superhuman'}, ' superhuman'),
                 ('set_ironman', {'value': False}, ' 0'), ('set_ironman', {'value': True}, ' 1'),
                 ('replace_base_name', {'text': ''}, ' -'), ('replace_base_name', {'text': 'é'}, ' c3a9'),
                 ('replace_base_name', {'text': 'e\u0301'}, ' 65cc81'),
                 ('place_base_site', {'y': 199, 'x': 0}, ' 0 199')]
        for action, args, suffix in cases:
            request = a.canonical_action_request(arguments(action=action, args=args))
            self.assertEqual(request, f'E {COMMAND} {EPOCH} {CAPABILITY} 1 {action}{suffix}'.encode('ascii'))
        largest = a.canonical_action_request(arguments(action='replace_base_name', args={'text': 'é' * 64}))
        self.assertLessEqual(len(largest), a.MAX_REQUEST)
        self.assertEqual(a.canonical_action_request(arguments(action='place_base_site', args={'x': 1, 'y': 2})),
                         a.canonical_action_request(arguments(action='place_base_site', args={'y': 2, 'x': 1})))

    def test_new_game_visible_choice_schema_and_selection(self):
        obj = state(screen='new_game'); a.validate_state(obj, CHALLENGE, EPOCH)
        cases = []
        for key, value in (('ironman', 1), ('difficulty', None), ('hidden_campaign_seed', 5)):
            changed = copy.deepcopy(obj); changed['visible'][key] = value; cases.append(changed)
        for key, value in (('value', 'hard'), ('label', None), ('selected', 1), ('hidden_index', 3)):
            changed = copy.deepcopy(obj); changed['visible']['difficulty'][0][key] = value; cases.append(changed)
        changed = copy.deepcopy(obj); changed['visible']['difficulty'][1]['selected'] = True; cases.append(changed)
        changed = copy.deepcopy(obj); changed['visible']['difficulty'][1]['value'] = 'beginner'; cases.append(changed)
        changed = copy.deepcopy(obj); changed['visible']['difficulty'] = []; cases.append(changed)
        changed = copy.deepcopy(obj); changed['visible']['ironman'] = None; cases.append(changed)
        for changed in cases:
            with self.subTest(changed=changed), self.assertRaises(Unavailable): a.validate_state(changed, CHALLENGE, EPOCH)
        obj['actions'] = [x for x in obj['actions'] if x['id'] in ('cancel_menu', 'start_new_game')]
        obj['visible'].update(difficulty=[], ironman=None, title=None, ironman_note=None)
        a.validate_state(obj, CHALLENGE, EPOCH)

    def test_globe_projection_strict_bounded_draw_only(self):
        obj = state(screen='build_base'); a.validate_state(obj, CHALLENGE, EPOCH)
        cases = []
        for key, value in (('width', 0), ('width', 513), ('height', 401), ('height', True), ('sample_step', 16.0),
                           ('sample_step', 8), ('samples', {}), ('world_longitude', 1.0), ('is_land', True)):
            changed = copy.deepcopy(obj); changed['visible']['globe'][key] = value; cases.append(changed)
        for key, value in (('x', -1), ('x', 256), ('x', 1.0), ('y', True), ('y', 200), ('terrain', 'ocean'),
                           ('terrain', 'land'), ('legal_base_site', True), ('polygon_id', 42)):
            changed = copy.deepcopy(obj); changed['visible']['globe']['samples'][0][key] = value; cases.append(changed)
        changed = copy.deepcopy(obj); changed['visible']['globe']['samples'].append(changed['visible']['globe']['samples'][0]); cases.append(changed)
        changed = copy.deepcopy(obj); changed['visible']['globe']['samples'] *= 401; cases.append(changed)
        for changed in cases:
            with self.subTest(changed=changed), self.assertRaises(Unavailable): a.validate_state(changed, CHALLENGE, EPOCH)

    def test_no_globe_or_empty_samples_cannot_offer_site_action(self):
        for globe in (None, dict(width=256, height=200, sample_step=16, samples=[])):
            obj = state(screen='build_base'); obj['visible']['globe'] = globe
            with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
            obj.update(actions=[], capability=None); a.validate_state(obj, CHALLENGE, EPOCH)

    def test_maximum_globe_frame_and_receipt_fit_mcp_response(self):
        obj = state(screen='build_base', challenge=None, capability=None, event_sequence=2)
        obj['visible']['title'] = '\\' * 4096
        obj['visible']['globe'] = dict(width=512, height=400, sample_step=16,
            samples=[dict(x=x, y=y, terrain='untextured') for y in range(8, 400, 16) for x in range(8, 512, 16)])
        self.assertEqual(len(obj['visible']['globe']['samples']), 800)
        receipt = result(observation=obj)
        a.validate_result(dump(receipt), COMMAND, EPOCH)
        response = dict(jsonrpc='2.0', id='i' * (adapter.MAX_MESSAGE - 256), result=dict(
            content=[dict(type='text', text=json.dumps(receipt, separators=(',', ':')))], structuredContent=receipt, isError=False))
        self.assertLess(len(json.dumps(response, ensure_ascii=True, separators=(',', ':')).encode()), adapter.MAX_RESPONSE)

    def test_base_name_descriptor_sources_and_exact_fields(self):
        obj = state(screen='base_name'); a.validate_state(obj, CHALLENGE, EPOCH)
        sources = {item['id']: item['label_source'] for item in obj['actions']}
        self.assertEqual(sources, dict(replace_base_name='visible_control_semantics', confirm_base_name='visible_text'))
        for field in ('base_id', 'longitude', 'latitude', 'country', 'cost'):
            changed = copy.deepcopy(obj); changed['visible'][field] = 1
            with self.subTest(field=field), self.assertRaises(Unavailable): a.validate_state(changed, CHALLENGE, EPOCH)

    def test_geoscape_funding_only_visible_labels_no_hidden_model(self):
        obj = state(screen='geoscape'); a.validate_state(obj, CHALLENGE, EPOCH)
        self.assertEqual([item['id'] for item in obj['actions']], ['open_funding','open_interception'])
        cases = []
        for key, value in (('hour', 3), ('seconds_elapsed', 99), ('ufos', []), ('bases', []), ('time_paused', True)):
            changed = copy.deepcopy(obj); changed['visible'][key] = value; cases.append(changed)
        changed = copy.deepcopy(obj); changed['visible']['buttons'] = []; cases.append(changed)
        changed = copy.deepcopy(obj); changed['visible']['buttons'][0]['id'] = 'launch_mission'; cases.append(changed)
        changed = copy.deepcopy(obj); changed['visible']['buttons'][0]['enabled'] = True; cases.append(changed)
        changed = copy.deepcopy(obj); changed['visible']['buttons'].append(changed['visible']['buttons'][0]); cases.append(changed)
        changed = copy.deepcopy(obj); changed['actions'] = state()['actions']; cases.append(changed)
        for changed in cases:
            with self.subTest(changed=changed), self.assertRaises(Unavailable): a.validate_state(changed, CHALLENGE, EPOCH)
        obj['visible'] = dict.fromkeys(a.GEOSCAPE_FIELDS); obj['visible'].update(buttons=[],speeds=[])
        obj.update(actions=[], capability=None)
        a.validate_state(obj, CHALLENGE, EPOCH)

    def test_action_specific_schema_is_closed_and_requires_args(self):
        schema = next(d for d in a.definitions() if d['name'] == 'execute_ui_action')['inputSchema']
        self.assertFalse(schema['additionalProperties']); self.assertIn('args', schema['required'])
        covered = []
        for branch in schema['oneOf']:
            covered.extend(branch['properties']['action']['enum'])
            args = branch['properties']['args']
            for variant in args.get('oneOf', [args]): self.assertFalse(variant['additionalProperties'])
        self.assertEqual(sorted(covered), sorted(a.ACTIONS)); self.assertEqual(len(covered), len(set(covered)))


class ClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs); self.addCleanup(native.close); return native

    def test_only_anonymous_seqpacket_allowed(self):
        for kind in (socket.SOCK_STREAM, socket.SOCK_DGRAM):
            left, right = socket.socketpair(socket.AF_UNIX, kind)
            with left, right, self.assertRaises(Unavailable): a.AgentControlClient(left)
        # The sandbox permits anonymous socketpair, not opening a listener.
        # Simulate a named endpoint on a real socket without creating a service.
        class NamedSocket(socket.socket):
            def getsockname(self): return '/not-an-anonymous-endpoint'
        left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        named = NamedSocket(fileno=left.detach())
        with named, right, self.assertRaises(Unavailable): a.AgentControlClient(named)

    def test_timeout_configuration_strict(self):
        for value in (True, False, 0, -1, float('inf'), float('nan'), 10, '1'):
            with self.subTest(value=value), self.assertRaises(Unavailable): a.AgentControlClient(None, timeout=value)

    def test_explicit_handshake_then_challenge_bound_state(self):
        native = self.fixture(); obj = native.client.observe()
        self.assertEqual(native.requests[0], b'H openxcom-agent-control/30')
        self.assertEqual(native.requests[1], ('R ' + obj['challenge']).encode())
        self.assertEqual(native.client.state, 'READY'); self.assertEqual(obj['visible']['selected_name'], 'Jane')

    def test_mismatched_native_identity_closes_before_any_request(self):
        for key, value in (('schema', 'experimental-stance/1'), ('schema', 'experimental-stance/2'), ('mode', 'supervised')):
            hello = dict(schema=a.SCHEMA, mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output'); hello[key] = value
            native = self.fixture(hello=hello)
            with self.assertRaises(Unavailable): native.client.observe()
            self.assertEqual(native.client.state, 'CLOSED'); self.assertEqual(native.requests, [])

    def test_mismatched_ready_closes_before_read(self):
        native = self.fixture(transform=lambda op, obj: dict(obj, mode='supervised') if op == 'H' else obj)
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.requests, [b'H openxcom-agent-control/30']); self.assertEqual(native.client.state, 'CLOSED')

    def test_new_read_invalidates_old_capability(self):
        native = self.fixture(); old = native.client.observe(); fresh = native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(old))
        self.assertEqual(len(native.requests), count); self.assertNotEqual(old['capability'], fresh['capability'])
        native.client.execute(**arguments(fresh))

    def test_capability_allows_reasoning_time_but_remains_bounded(self):
        native = self.fixture(); obj = native.client.observe()
        remaining = native.client.capability_deadline - a.time.monotonic()
        self.assertGreater(remaining, 119.0)
        self.assertLessEqual(remaining, 120.0)
        # Ten seconds of elapsed reasoning must not recreate the old five-second
        # limit. Native still validates its own TTL and exact displayed binding.
        native.client.capability_deadline -= 10.0
        native.client.execute(**arguments(obj))

    def test_wrong_epoch_revision_action_or_expired_grant_never_sends(self):
        native = self.fixture(); args = arguments(native.client.observe()); count = len(native.requests)
        for changes in (dict(epoch='f' * 32), dict(revision=0), dict(capability='f' * 32), dict(action='stand'), dict(action='continue_next_turn')):
            with self.subTest(changes=changes), self.assertRaises(Unavailable): native.client.execute(**dict(args, **changes))
        native.client.capability_deadline = 0
        with self.assertRaises(Unavailable): native.client.execute(**args)
        self.assertEqual(len(native.requests), count)

    def test_unsupported_state_is_explicit_and_never_grants_control(self):
        native = self.fixture(screen='unsupported'); obj = native.client.observe()
        self.assertEqual(obj['state'], 'unsupported'); self.assertEqual(obj['actions'], [])
        self.assertIsNone(obj['capability']); self.assertEqual(native.client.state, 'NEED_CONTEXT')
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments())
        self.assertEqual(len(native.requests), count)
        native.screen = 'next_turn'; obj = native.client.observe(); native.client.execute(**arguments(obj, action='continue_next_turn'))

    def test_exact_wire_and_post_action_state_never_rearms(self):
        native = self.fixture(); obj = native.client.observe(); args = arguments(obj); receipt = native.client.execute(**args)
        self.assertEqual(native.requests[-1], f'E {COMMAND} {EPOCH} {obj["capability"]} 1 kneel'.encode())
        self.assertEqual(receipt['status'], 'dispatched'); self.assertEqual(receipt['semantics'], 'dispatch_not_success')
        self.assertIsNone(receipt['observation']['capability']); self.assertEqual(native.client.state, 'NEED_CONTEXT')
        self.assertIsNone(native.client.capability); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**dict(args, command_id='a' * 32))
        self.assertEqual(len(native.requests), count)

    def test_duplicate_queries_receipt_and_preserves_newer_grant(self):
        native = self.fixture(); args = arguments(native.client.observe()); first = native.client.execute(**args)
        self.assertEqual(native.client.execute(**args), first); self.assertEqual(native.requests[-1], ('S ' + COMMAND).encode())
        fresh = native.client.observe(); self.assertEqual(native.client.execute(**args), first)
        self.assertEqual(native.client.capability, fresh['capability']); self.assertEqual(native.client.state, 'READY')
        self.assertEqual(len([req for req in native.requests if req.startswith(b'E ')]), 1)

    def test_cross_action_and_changed_id_reuse_never_sends(self):
        native = self.fixture(); args = arguments(native.client.observe()); native.client.execute(**args); count = len(native.requests)
        for changes in (dict(action='next_soldier'), dict(epoch='f' * 32), dict(capability='f' * 32), dict(revision=3)):
            with self.subTest(changes=changes), self.assertRaises(Unavailable): native.client.execute(**dict(args, **changes))
        self.assertEqual(len(native.requests), count)

    def test_unknown_command_status_never_sends(self):
        native = self.fixture(); native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.read_result(COMMAND)
        self.assertEqual(len(native.requests), count)

    def test_shared_non_evicting_256_entry_ledger(self):
        native = self.fixture()
        for index in range(a.MAX_COMMANDS):
            native.client.execute(**arguments(native.client.observe(), command_id=f'{index:032x}', action='next_soldier'))
        self.assertEqual(len(native.client.commands), a.MAX_COMMANDS); obj = native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(obj))
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'CLOSED'); self.assertIn('0' * 32, native.client.commands)

    def test_dispatch_timeout_terminal_no_retry_or_status_poll(self):
        native = self.fixture(transform=lambda op, obj: None if op == 'E' else obj); args = arguments(native.client.observe())
        with self.assertRaises(Unavailable): native.client.execute(**args)
        count = len(native.requests); self.assertIn(COMMAND, native.client.commands)
        for operation in (native.client.observe, lambda: native.client.execute(**args), lambda: native.client.read_result(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(native.client.state, 'CLOSED'); self.assertEqual(len(native.requests), count)
        self.assertEqual(len([r for r in native.requests if r.startswith(b'E ')]), 1)

    def test_unsolicited_packet_before_mutating_send_is_definitely_unsent(self):
        native = self.fixture(); args = arguments(native.client.observe())
        native.peer.send(b'{}')
        with self.assertRaises(Unavailable) as caught: native.client.execute(**args)
        self.assertNotIsInstance(caught.exception, a.ActionOutcomeUnknown)
        self.assertFalse(any(r.startswith(b'E ') for r in native.requests))
        self.assertEqual(native.client.state, 'CLOSED')

    def test_malformed_reply_after_mutating_send_is_outcome_unknown(self):
        native = self.fixture(transform=lambda op, obj: b'{"hidden_reason":"PRIVATE"}' if op == 'E' else obj)
        args = arguments(native.client.observe())
        with self.assertRaises(a.ActionOutcomeUnknown) as caught: native.client.execute(**args)
        self.assertEqual(caught.exception.command_id, COMMAND)
        self.assertNotIn('PRIVATE', dump(caught.exception.public_error()).decode())
        self.assertEqual(native.client.state, 'CLOSED')
        self.assertEqual(len([r for r in native.requests if r.startswith(b'E ')]), 1)

    def test_malformed_or_wrong_bound_observation_closes(self):
        for changes in (dict(challenge='f' * 32), dict(epoch='f' * 32), dict(hidden=1), dict(revision=True)):
            native = self.fixture(transform=lambda op, obj, c=changes: dict(obj, **c) if op == 'R' else obj)
            with self.subTest(changes=changes), self.assertRaises(Unavailable): native.client.observe()
            self.assertEqual(native.client.state, 'CLOSED')

    def test_v1_peer_fails_before_handshake_or_action_send(self):
        native = self.fixture(hello=dict(schema='openxcom-agent-control/1', mode=a.MODE,
                                         epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output'))
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.requests, []); self.assertEqual(native.client.state, 'CLOSED')

    def test_main_menu_shell_round_trips_need_fresh_context(self):
        pairs = [('open_new_game', 'new_game'), ('open_options', 'options_shell'),
                 ('open_load_game', 'load_list'), ('new_battle', 'new_battle')]
        for opener, screen in pairs:
            with self.subTest(screen=screen):
                native = self.fixture(screen='main_menu', transitions={
                    ('main_menu', opener): screen, (screen, 'cancel_menu'): 'main_menu'})
                initial = native.client.observe()
                opened = native.client.execute(**arguments(initial, action=opener))
                self.assertEqual(opened['observation']['state'], screen)
                self.assertIsNone(opened['observation']['capability'])
                fresh = native.client.observe()
                closed = native.client.execute(**arguments(fresh, command_id='a' * 32, action='cancel_menu'))
                self.assertEqual(closed['observation']['state'], 'main_menu')
                self.assertEqual(native.client.state, 'NEED_CONTEXT')

    def test_pause_save_load_options_shell_round_trips(self):
        transitions = {('battlescape', 'open_pause_menu'): 'pause_menu', ('pause_menu', 'resume_game'): 'battlescape'}
        for action, screen in (('open_load_game', 'load_list'), ('open_save_game', 'save_list'), ('open_options', 'options_shell')):
            transitions[('pause_menu', action)] = screen; transitions[(screen, 'cancel_menu')] = 'pause_menu'
        native = self.fixture(transitions=transitions)
        sequence = [('open_pause_menu', 'pause_menu'), ('open_load_game', 'load_list'), ('cancel_menu', 'pause_menu'),
                    ('open_save_game', 'save_list'), ('cancel_menu', 'pause_menu'), ('open_options', 'options_shell'),
                    ('cancel_menu', 'pause_menu'), ('resume_game', 'battlescape')]
        for index, (action, destination) in enumerate(sequence):
            receipt = native.client.execute(**arguments(native.client.observe(), command_id=f'{index:032x}', action=action))
            self.assertEqual(receipt['observation']['state'], destination)
        self.assertEqual(len([r for r in native.requests if r.startswith(b'E ')]), len(sequence))

    def test_duplicate_cancel_is_receipt_only_and_cannot_pop_parent(self):
        native = self.fixture(screen='load_list', transitions={('load_list', 'cancel_menu'): 'pause_menu'})
        args = arguments(native.client.observe(), action='cancel_menu')
        first = native.client.execute(**args)
        parent = native.client.observe()
        self.assertEqual(native.client.execute(**args), first)
        self.assertEqual(native.screen, 'pause_menu'); self.assertEqual(native.client.capability, parent['capability'])
        self.assertEqual(len([r for r in native.requests if r.startswith(b'E ')]), 1)
        self.assertEqual(native.requests[-1], ('S ' + COMMAND).encode())

    def test_error_modal_has_own_grant_and_no_parent_actions(self):
        native = self.fixture(screen='pause_menu', transitions={('error_message', 'dismiss_error'): 'pause_menu'})
        parent = native.client.observe(); native.screen = 'error_message'; modal = native.client.observe()
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(parent, action='resume_game'))
        with self.assertRaises(Unavailable): native.client.execute(**arguments(modal, action='cancel_menu'))
        self.assertEqual(len(native.requests), count)
        receipt = native.client.execute(**arguments(modal, action='dismiss_error'))
        self.assertEqual(receipt['observation']['state'], 'pause_menu')

    def test_hidden_cancel_leaves_no_capability_and_no_synthetic_back(self):
        native = self.fixture(screen='save_list', transform=lambda op, obj: dict(obj, actions=[], capability=None) if op == 'R' else obj)
        observed = native.client.observe(); self.assertEqual(observed['actions'], []); self.assertIsNone(observed['capability'])
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(action='cancel_menu'))
        self.assertEqual(len(native.requests), count)

    def test_generic_error_closes_without_hidden_details(self):
        native = self.fixture(transform=lambda op, obj: dict(schema=a.SCHEMA, mode=a.MODE, type='error', status='unavailable') if op == 'R' else obj)
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.client.state, 'CLOSED')

    def test_unknown_after_dispatch_not_success_and_needs_new_read(self):
        native = self.fixture(transform=lambda op, obj: dict(obj, status='unknown_after_dispatch', observation=None) if op == 'E' else obj)
        receipt = native.client.execute(**arguments(native.client.observe()))
        self.assertEqual(receipt['status'], 'unknown_after_dispatch'); self.assertIsNone(receipt['observation'])
        self.assertEqual(native.client.state, 'NEED_CONTEXT')

    def test_terminal_receipt_cannot_change(self):
        native = self.fixture(transform=lambda op, obj: dict(obj, status='unavailable', observation=None) if op == 'S' else obj)
        args = arguments(native.client.observe()); native.client.execute(**args)
        with self.assertRaises(Unavailable): native.client.read_result(COMMAND)
        self.assertEqual(native.client.state, 'CLOSED')

    def test_reused_capability_after_dispatch_rejected(self):
        native = self.fixture(); obj = native.client.observe(); native.client.execute(**arguments(obj))
        native.transform = lambda op, reply: dict(reply, capability=obj['capability']) if op == 'R' else reply
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.client.state, 'CLOSED')

    def test_counters_never_regress_on_new_read(self):
        native = self.fixture(); native.client.observe(); native.transform = lambda op, obj: dict(obj, revision=0) if op == 'R' else obj
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.client.state, 'CLOSED')


class TypedClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs); self.addCleanup(native.close); return native

    def test_v2_peer_closes_before_any_request(self):
        native = self.fixture(hello=dict(schema='openxcom-agent-control/2', mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output'))
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.requests, []); self.assertEqual(native.client.state, 'CLOSED')

    def test_each_typed_action_canonical_duplicate_and_changed_args(self):
        cases = [('new_game', 'select_difficulty', {'value': 'genius'}, {'value': 'veteran'}),
                 ('new_game', 'set_ironman', {'value': True}, {'value': False}),
                 ('base_name', 'replace_base_name', {'text': 'é'}, {'text': 'e\u0301'}),
                 ('build_base', 'place_base_site', {'x': 64, 'y': 80}, {'x': 65, 'y': 80})]
        for screen, action, values, changed in cases:
            with self.subTest(action=action):
                native = self.fixture(screen=screen)
                request = arguments(native.client.observe(), action=action, args=values)
                expected_wire = a.canonical_action_request(request)
                receipt = native.client.execute(**request)
                self.assertEqual(native.requests[-1], expected_wire)
                self.assertEqual(native.client.commands[COMMAND]['request'], expected_wire)
                fresh = native.client.observe()
                duplicate = dict(reversed(list(request.items()))); duplicate['args'] = dict(reversed(list(values.items())))
                self.assertEqual(native.client.execute(**duplicate), receipt)
                self.assertEqual(native.requests[-1], ('S ' + COMMAND).encode())
                self.assertEqual(native.client.capability, fresh['capability'])
                count = len(native.requests)
                with self.assertRaises(Unavailable): native.client.execute(**dict(request, args=changed))
                self.assertEqual(len(native.requests), count)
                self.assertEqual(sum(req.startswith(b'E ') for req in native.requests), 1)

    def test_mutating_caller_args_cannot_change_reserved_identity(self):
        native = self.fixture(screen='base_name')
        request = arguments(native.client.observe(), action='replace_base_name', args={'text': 'Original'})
        receipt = native.client.execute(**request); request['args']['text'] = 'Changed'
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**request)
        self.assertEqual(len(native.requests), count)
        request['args']['text'] = 'Original'
        self.assertEqual(native.client.execute(**request), receipt)
        self.assertEqual(native.requests[-1], ('S ' + COMMAND).encode())

    def test_unexposed_difficulty_value_is_rejected_before_send(self):
        def limited(op, obj):
            if op == 'R': obj['visible']['difficulty'] = [x for x in obj['visible']['difficulty'] if x['value'] == 'genius']
            return obj
        native = self.fixture(screen='new_game', transform=limited); obj = native.client.observe()
        count = len(native.requests)
        # Caller cannot expand the cached visible choice set by mutating its DTO.
        obj['visible']['difficulty'].append(dict(value='beginner', label='Forged', selected=False))
        with self.assertRaises(Unavailable): native.client.execute(**arguments(obj, action='select_difficulty', args={'value': 'beginner'}))
        self.assertEqual(len(native.requests), count)
        receipt = native.client.execute(**arguments(obj, action='select_difficulty', args={'value': 'genius'}))
        self.assertTrue(next(x for x in receipt['observation']['visible']['difficulty'] if x['value'] == 'genius')['selected'])

    def test_point_uses_fresh_sample_membership_without_terrain_preflight(self):
        native = self.fixture(screen='build_base'); obj = native.client.observe(); count = len(native.requests)
        obj['visible']['globe']['width'] = 4096
        obj['visible']['globe']['samples'].append(dict(x=1, y=1, terrain='textured'))
        for point in ({'x': 256, 'y': 0}, {'x': 0, 'y': 200}, {'x': 4095, 'y': 4095}, {'x': 1, 'y': 1}, {'x': 64, 'y': 81}):
            with self.subTest(point=point), self.assertRaises(Unavailable):
                native.client.execute(**arguments(obj, action='place_base_site', args=point))
        self.assertEqual(len(native.requests), count)
        # Advertised untextured coverage is not a land/eligibility prediction.
        receipt = native.client.execute(**arguments(obj, action='place_base_site', args={'x': 80, 'y': 80}))
        self.assertEqual(receipt['status'], 'dispatched')
        self.assertTrue(native.requests[-1].endswith(b'place_base_site 80 80'))

    def test_desired_boolean_does_not_toggle_and_text_clear_does_not_confirm(self):
        native = self.fixture(screen='new_game')
        for index in range(2):
            receipt = native.client.execute(**arguments(native.client.observe(), command_id=f'{index:032x}',
                                                        action='set_ironman', args={'value': False}))
            self.assertIs(receipt['observation']['visible']['ironman'], False)
        names = self.fixture(screen='base_name')
        for index, text in enumerate(('Alpha', '')):
            receipt = names.client.execute(**arguments(names.client.observe(), command_id=f'{index:032x}',
                                                       action='replace_base_name', args={'text': text}))
            self.assertEqual(receipt['observation']['visible']['name'], text)
            self.assertEqual(receipt['observation']['state'], 'base_name')
            self.assertIsNone(receipt['observation']['capability'])
        fresh = names.client.observe(); count = len(names.requests)
        with self.assertRaises(Unavailable): names.client.execute(**arguments(fresh, action='confirm_base_name'))
        self.assertEqual(len(names.requests), count); self.assertNotIn('confirm_base_name', [x['id'] for x in fresh['actions']])

    def test_typed_start_site_name_confirm_flow_to_funding_only_geoscape(self):
        native = self.fixture(screen='new_game', transitions={('new_game', 'start_new_game'): 'build_base',
            ('build_base', 'place_base_site'): 'base_name', ('base_name', 'confirm_base_name'): 'geoscape'})
        sequence = [('select_difficulty', {'value': 'veteran'}, 'new_game'),
                    ('set_ironman', {'value': False}, 'new_game'), ('start_new_game', {}, 'build_base'),
                    ('place_base_site', {'x': 64, 'y': 80}, 'base_name'),
                    ('replace_base_name', {'text': 'Example'}, 'base_name'), ('confirm_base_name', {}, 'geoscape')]
        for index, (action, args, destination) in enumerate(sequence):
            receipt = native.client.execute(**arguments(native.client.observe(), command_id=f'{index:032x}', action=action, args=args))
            self.assertEqual(receipt['observation']['state'], destination)
        final = native.client.observe(); self.assertEqual([item['id'] for item in final['actions']], ['open_funding','open_interception'])
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(action='open_pause_menu'))
        self.assertEqual(len(native.requests), count)

    def test_typed_send_uncertainty_is_terminal_for_every_kind(self):
        for screen, action in (('new_game', 'select_difficulty'), ('new_game', 'set_ironman'),
                               ('base_name', 'replace_base_name'), ('build_base', 'place_base_site')):
            with self.subTest(action=action):
                native = self.fixture(screen=screen, transform=lambda op, obj: None if op == 'E' else obj)
                request = arguments(native.client.observe(), action=action)
                with self.assertRaises(a.ActionOutcomeUnknown) as failure: native.client.execute(**request)
                self.assertEqual(failure.exception.command_id, COMMAND)
                count = len(native.requests)
                for operation in (lambda: native.client.execute(**request), native.client.observe, lambda: native.client.read_result(COMMAND)):
                    with self.assertRaises(Unavailable): operation()
                self.assertEqual(len(native.requests), count)
                self.assertEqual(sum(req.startswith(b'E ') for req in native.requests), 1)

    def test_v2_receipt_after_typed_send_is_adapter_unknown(self):
        native = self.fixture(screen='new_game', transform=lambda op, obj: dict(obj, schema='openxcom-agent-control/2') if op == 'E' else obj)
        request = arguments(native.client.observe(), action='set_ironman')
        with self.assertRaises(a.ActionOutcomeUnknown): native.client.execute(**request)
        self.assertEqual(native.client.state, 'CLOSED')

    def test_all_strategy_tool_outputs_are_text_only(self):
        for screen, action in (('new_game', 'select_difficulty'), ('new_game', 'set_ironman'), ('new_game', 'start_new_game'),
                               ('build_base', 'place_base_site'), ('base_name', 'replace_base_name')):
            with self.subTest(screen=screen, action=action):
                native = self.fixture(screen=screen); server = adapter.Server(agent_control=native.client)
                observation = call(server, 'read_ui_state')['result']
                execution = call(server, 'execute_ui_action', arguments(observation['structuredContent'], action=action))['result']
                status = call(server, 'read_ui_action_result', {'command_id': COMMAND})['result']
                for output in (observation, execution, status):
                    self.assertFalse(output['isError'])
                    self.assertEqual([item['type'] for item in output['content']], ['text'])
                    self.assertEqual(json.loads(output['content'][0]['text']), output['structuredContent'])
        native = self.fixture(screen='geoscape'); server = adapter.Server(agent_control=native.client)
        output = call(server, 'read_ui_state')['result']
        self.assertEqual([item['type'] for item in output['content']], ['text'])
        self.assertEqual([item['id'] for item in output['structuredContent']['actions']], ['open_funding','open_interception'])


class ListValidationTests(unittest.TestCase):
    def test_closed_list_action_matrix_kinds_and_sources(self):
        expected = {
            'new_battle': {'start_battle', 'cancel_menu', 'open_new_battle_choice'},
            'new_battle_dropdown': {'choose_visible_row', 'scroll_visible_list', 'close_dropdown'},
            'funding': {'scroll_visible_list', 'close_funding'}, 'geoscape': {'open_funding', 'open_bases', 'open_pause_menu', 'choose_geoscape_speed'},
        }
        for screen, actions in expected.items():
            self.assertEqual(a.STATE_ACTIONS[screen], actions)
            obj = state(screen=screen); a.validate_state(obj, CHALLENGE, EPOCH)
            for action in set(a.ACTIONS) - actions:
                bad = copy.deepcopy(obj)
                bad['actions'] = [dict(id=action, label='Visible', label_source=a.label_source(screen, action),
                                       argument_kind=a.ARGUMENT_KINDS[action])]
                with self.subTest(screen=screen, action=action), self.assertRaises(Unavailable):
                    a.validate_state(bad, CHALLENGE, EPOCH)
        self.assertEqual(a.label_source('funding', 'close_funding'), 'visible_text')
        self.assertEqual(a.label_source('geoscape', 'open_funding'), 'visible_text')
        for screen, action in (('new_battle', 'open_new_battle_choice'), ('new_battle_dropdown', 'choose_visible_row'),
                               ('new_battle_dropdown', 'close_dropdown'), ('funding', 'scroll_visible_list')):
            self.assertEqual(a.label_source(screen, action), 'visible_control_semantics')

    def test_closed_argument_enums_reject_indices_coercion_and_extra_keys(self):
        cases = {
            'open_new_battle_choice': ({}, {'control': 'soldier'}, {'control': 0}, {'control': True},
                                       {'control': 'Mission'}, {'control': 'mission', 'id': 4}),
            'choose_visible_row': ({}, {'row': 0}, {'row': True}, {'row': '0'}, {'row': 'r-1'}, {'row': 'r32'},
                                   {'row': 'r00'}, {'row': 'r01'}, {'row': 'r0\n'}, {'row': 'r0', 'control': 'craft'},
                                   {'row': 'r0', 'absolute_index': 0}, {'row': 'STR_PRIVATE_RULE'}),
            'scroll_visible_list': ({}, {'direction': 'page_down'}, {'direction': 'Down'}, {'direction': 1},
                                      {'direction': 'down', 'count': 2}, {'direction': 'down', 'list_id': 'private'}),
            'close_dropdown': ({'control': 'mission'}, {'row': 'r0'}),
            'open_funding': ({'country': 'hidden'},), 'close_funding': ({'confirm': True},),
        }
        for action, values in cases.items():
            for value in values:
                with self.subTest(action=action, value=value), self.assertRaises(Unavailable):
                    a.validate_action_arguments(arguments(action=action, args=value))
        for action, field, values in (('open_new_battle_choice', 'control', a.CHOICE_CONTROLS),
                                       ('choose_visible_row', 'row', a.ROW_HANDLES),
                                       ('scroll_visible_list', 'direction', a.SCROLL_DIRECTIONS)):
            for value in values:
                wire = a.canonical_action_request(arguments(action=action, args={field: value}))
                self.assertEqual(wire, f'E {COMMAND} {EPOCH} {CAPABILITY} 1 {action} {value}'.encode())

    def test_rows_are_bounded_sequential_local_handles_not_model_indices(self):
        for screen in ('new_battle_dropdown', 'funding'):
            obj = state(screen=screen)
            for count in (0, 1, 32):
                good = copy.deepcopy(obj)
                good['visible']['rows'] = [dict(handle=f'r{i}', cells=['Row'] * (3 if screen == 'funding' else 1), selected=False) for i in range(count)]
                if not count:
                    good['actions'] = [x for x in good['actions'] if x['id'] != 'choose_visible_row']
                a.validate_state(good, CHALLENGE, EPOCH)
            cases = []
            for key, value in (('handle', 'r31'), ('handle', 0), ('handle', 'r00'), ('selected', 1),
                               ('absolute_index', 5), ('rule_id', 'STR_HIDDEN'), ('offscreen', False)):
                bad = copy.deepcopy(obj); bad['visible']['rows'][0][key] = value; cases.append(bad)
            bad = copy.deepcopy(obj); bad['visible']['rows'][1]['handle'] = 'r0'; cases.append(bad)
            bad = copy.deepcopy(obj); bad['visible']['rows'].reverse(); cases.append(bad)
            bad = copy.deepcopy(obj); bad['visible']['rows'] *= 17; cases.append(bad)
            for value in ({}, None, 'rows'):
                bad = copy.deepcopy(obj); bad['visible']['rows'] = value; cases.append(bad)
            for bad in cases:
                with self.subTest(screen=screen, bad=bad), self.assertRaises(Unavailable): a.validate_state(bad, CHALLENGE, EPOCH)

    def test_exact_dropdown_projection_and_hover_only_selection(self):
        obj = state(screen='new_battle_dropdown'); cases = []
        for key, value in (('control', 'hidden_control'), ('control', 0), ('label', 3), ('total_rows', 400),
                           ('selected_index', 200), ('scroll_offset', 99), ('list_id', 'model')):
            bad = copy.deepcopy(obj); bad['visible'][key] = value; cases.append(bad)
        for cells in ([], ['one', 'two'], [''], [None], [1], 'text'):
            bad = copy.deepcopy(obj); bad['visible']['rows'][0]['cells'] = cells; cases.append(bad)
        bad = copy.deepcopy(obj); bad['visible']['rows'][0]['selected'] = True; cases.append(bad)
        bad = copy.deepcopy(obj); bad['visible']['rows'] = []; cases.append(bad)
        for bad in cases:
            with self.subTest(bad=bad), self.assertRaises(Unavailable): a.validate_state(bad, CHALLENGE, EPOCH)
        obj['visible']['label'] = None
        for row in obj['visible']['rows']: row['selected'] = False
        a.validate_state(obj, CHALLENGE, EPOCH)
        # Equal visible labels are allowed. They do not establish row identity.
        obj['visible']['rows'][1]['cells'] = obj['visible']['rows'][0]['cells'][:]
        a.validate_state(obj, CHALLENGE, EPOCH)

    def test_exact_funding_projection_empty_cells_and_no_row_selection(self):
        obj = state(screen='funding'); cases = []
        for key, value in (('headings', []), ('headings', [None] * 4), ('headings', [None, 1, None]),
                           ('title', 1), ('total_rows', 32), ('country_ids', ['PRIVATE']), ('funds_raw', 123)):
            bad = copy.deepcopy(obj); bad['visible'][key] = value; cases.append(bad)
        for cells in ([], ['one'], ['one', 'two', 'three', 'four'], ['one', None, 'three']):
            bad = copy.deepcopy(obj); bad['visible']['rows'][0]['cells'] = cells; cases.append(bad)
        bad = copy.deepcopy(obj); bad['visible']['rows'][0]['selected'] = True; cases.append(bad)
        for bad in cases:
            with self.subTest(bad=bad), self.assertRaises(Unavailable): a.validate_state(bad, CHALLENGE, EPOCH)
        obj['visible'].update(title=None, headings=[None, None, None])
        obj['visible']['rows'][0]['cells'] = ['', '', '']
        a.validate_state(obj, CHALLENGE, EPOCH)

    def test_scroll_flags_are_strict_visible_controls_not_hidden_eligibility(self):
        for screen in ('new_battle_dropdown', 'funding'):
            obj = state(screen=screen)
            for scroll in ({}, {'up': True}, {'up': 1, 'down': False}, {'up': True, 'down': 'false'},
                           {'up': True, 'down': False, 'can_scroll_more': False}, {'up': False, 'down': False}):
                bad = copy.deepcopy(obj); bad['visible']['scroll'] = scroll
                with self.subTest(screen=screen, scroll=scroll), self.assertRaises(Unavailable): a.validate_state(bad, CHALLENGE, EPOCH)
            obj['visible']['scroll'] = dict(up=False, down=False)
            obj['actions'] = [x for x in obj['actions'] if x['id'] != 'scroll_visible_list']
            a.validate_state(obj, CHALLENGE, EPOCH)

    def test_missing_new_battle_controls_and_funding_button_cannot_authorize(self):
        obj = state(screen='new_battle'); obj['visible'] = dict.fromkeys(a.CHOICE_CONTROLS)
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        obj['actions'] = [x for x in obj['actions'] if x['id'] != 'open_new_battle_choice']
        a.validate_state(obj, CHALLENGE, EPOCH)
        obj = state(screen='geoscape'); obj['visible']['buttons'] = [x for x in obj['visible']['buttons'] if x['id'] != 'funding']
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        obj.update(actions=[], capability=None); a.validate_state(obj, CHALLENGE, EPOCH)

    def test_text_heavy_list_envelope_retains_tighter_escape_bound(self):
        for screen in ('new_battle_dropdown', 'funding'):
            obj = state(screen=screen)
            obj['visible']['rows'] = [dict(handle=f'r{i}', cells=['\U0001f600' * 1000] * (3 if screen == 'funding' else 1), selected=False) for i in range(32)]
            with self.subTest(screen=screen), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)


class ListClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs); self.addCleanup(native.close); return native

    def test_v3_peer_closes_before_any_request(self):
        native = self.fixture(hello=dict(schema='openxcom-agent-control/3', mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output'))
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.requests, []); self.assertEqual(native.client.state, 'CLOSED')

    def test_only_fresh_visible_new_battle_control_opens(self):
        def hide(op, obj):
            if op == 'R': obj['visible']['craft'] = None
            return obj
        native = self.fixture(screen='new_battle', transform=hide)
        obj = native.client.observe(); count = len(native.requests)
        obj['visible']['craft'] = 'Forged visible craft'
        with self.assertRaises(Unavailable): native.client.execute(**arguments(obj, action='open_new_battle_choice', args={'control': 'craft'}))
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.commands, {})
        native.client.execute(**arguments(obj, action='open_new_battle_choice', args={'control': 'mission'}))

    def test_forged_and_offscreen_rows_cannot_expand_cached_grant(self):
        native = self.fixture(screen='new_battle_dropdown'); obj = native.client.observe(); count = len(native.requests)
        obj['visible']['rows'].append(dict(handle='r2', cells=['Forged'], selected=False))
        for row in ('r2', 'r31'):
            with self.subTest(row=row), self.assertRaises(Unavailable):
                native.client.execute(**arguments(obj, action='choose_visible_row', args={'row': row}))
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.commands, {})
        native.client.execute(**arguments(obj, action='choose_visible_row', args={'row': 'r1'}))
        self.assertTrue(native.requests[-1].endswith(b'choose_visible_row r1'))

    def test_fresh_scroll_direction_required_and_returned_dto_cannot_authorize(self):
        for screen in ('new_battle_dropdown', 'funding'):
            with self.subTest(screen=screen):
                native = self.fixture(screen=screen, transform=lambda op, obj: (obj['visible']['scroll'].update(up=False) or obj) if op == 'R' else obj)
                obj = native.client.observe(); count = len(native.requests)
                obj['visible']['scroll']['up'] = True
                with self.assertRaises(Unavailable): native.client.execute(**arguments(obj, action='scroll_visible_list', args={'direction': 'up'}))
                self.assertEqual(len(native.requests), count); self.assertEqual(native.client.commands, {})
                native.client.execute(**arguments(obj, action='scroll_visible_list', args={'direction': 'down'}))

    def test_stale_cross_list_and_same_label_aba_grants_never_send(self):
        native = self.fixture(screen='new_battle_dropdown'); old = native.client.observe()
        native.control = 'craft'; native.revision += 1
        fresh = native.client.observe(); count = len(native.requests)
        self.assertEqual(old['visible']['rows'], fresh['visible']['rows'])
        self.assertNotEqual(old['visible']['control'], fresh['visible']['control'])
        with self.assertRaises(Unavailable): native.client.execute(**arguments(old, action='choose_visible_row'))
        # Returning to the same named list and same labels does not revive a grant.
        native.control = 'mission'; native.revision += 1; newest = native.client.observe(); count += 1
        with self.assertRaises(Unavailable): native.client.execute(**arguments(old, action='choose_visible_row'))
        self.assertEqual(len(native.requests), count)
        native.client.execute(**arguments(newest, action='choose_visible_row'))

    def test_funding_rows_are_never_selection_authority(self):
        native = self.fixture(screen='new_battle_dropdown'); old = native.client.observe()
        native.screen = 'funding'; native.revision += 1; current = native.client.observe(); count = len(native.requests)
        self.assertEqual(native.client.visible_rows, frozenset())
        for obj in (old, current):
            with self.assertRaises(Unavailable): native.client.execute(**arguments(obj, action='choose_visible_row'))
        self.assertEqual(len(native.requests), count)
        native.client.execute(**arguments(current, action='close_funding'))

    def test_each_new_typed_kind_has_canonical_duplicate_and_conflict(self):
        cases = [('new_battle', 'open_new_battle_choice', {'control': 'mission'}, {'control': 'craft'}),
                 ('new_battle_dropdown', 'choose_visible_row', {'row': 'r0'}, {'row': 'r1'}),
                 ('funding', 'scroll_visible_list', {'direction': 'down'}, {'direction': 'up'})]
        for screen, action, values, changed in cases:
            with self.subTest(action=action):
                native = self.fixture(screen=screen); request = arguments(native.client.observe(), action=action, args=values)
                receipt = native.client.execute(**request); fresh = native.client.observe()
                self.assertEqual(native.client.execute(**request), receipt)
                self.assertEqual(native.requests[-1], ('S ' + COMMAND).encode())
                self.assertEqual(native.client.capability, fresh['capability'])
                count = len(native.requests)
                with self.assertRaises(Unavailable): native.client.execute(**dict(request, args=changed))
                self.assertEqual(len(native.requests), count)
                self.assertEqual(sum(req.startswith(b'E ') for req in native.requests), 1)

    def test_five_dropdown_round_trips_then_funding_are_semantic_only(self):
        transitions = {('new_battle', 'open_new_battle_choice'): 'new_battle_dropdown',
                       ('new_battle_dropdown', 'choose_visible_row'): 'new_battle',
                       ('new_battle_dropdown', 'close_dropdown'): 'new_battle',
                       ('new_battle', 'cancel_menu'): 'geoscape', ('geoscape', 'open_funding'): 'funding',
                       ('funding', 'close_funding'): 'geoscape'}
        native = self.fixture(screen='new_battle', transitions=transitions); number = 0
        def execute(action, args, destination):
            nonlocal number
            number += 1
            receipt = native.client.execute(**arguments(native.client.observe(), command_id=f'{number:032x}', action=action, args=args))
            self.assertEqual(receipt['observation']['state'], destination)
            self.assertIsNone(receipt['observation']['capability'])
            return receipt
        for control in a.CHOICE_CONTROLS:
            opened = execute('open_new_battle_choice', {'control': control}, 'new_battle_dropdown')
            self.assertEqual(opened['observation']['visible']['control'], control)
            execute('scroll_visible_list', {'direction': 'down'}, 'new_battle_dropdown')
            execute('choose_visible_row', {'row': 'r0'}, 'new_battle')
            execute('open_new_battle_choice', {'control': control}, 'new_battle_dropdown')
            execute('close_dropdown', {}, 'new_battle')
        execute('cancel_menu', {}, 'geoscape')
        opened = execute('open_funding', {}, 'funding')
        scrolled = execute('scroll_visible_list', {'direction': 'up'}, 'funding')
        # A visible boundary arrow may dispatch and leave identical visible rows.
        self.assertEqual(opened['observation']['visible'], scrolled['observation']['visible'])
        execute('close_funding', {}, 'geoscape')
        self.assertEqual(sum(req.startswith(b'E ') for req in native.requests), 29)

    def test_all_list_actions_share_existing_non_evicting_ledger(self):
        native = self.fixture(screen='funding')
        for index in range(a.MAX_COMMANDS):
            if index == 63: native.screen = 'new_battle_dropdown'
            action = 'choose_visible_row' if index == 63 else 'scroll_visible_list'
            native.client.execute(**arguments(native.client.observe(), command_id=f'{index:032x}', action=action))
        self.assertEqual(len(native.client.commands), a.MAX_COMMANDS)
        fresh = native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(fresh, action='close_dropdown'))
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'CLOSED')
        self.assertIn('0' * 32, native.client.commands)

    def test_all_new_actions_have_terminal_uncertainty_without_replay(self):
        cases = [('new_battle', 'open_new_battle_choice'), ('new_battle_dropdown', 'choose_visible_row'),
                 ('new_battle_dropdown', 'scroll_visible_list'), ('new_battle_dropdown', 'close_dropdown'),
                 ('geoscape', 'open_funding'), ('funding', 'close_funding')]
        for screen, action in cases:
            with self.subTest(action=action):
                native = self.fixture(screen=screen, transform=lambda op, obj: dict(obj, schema='openxcom-agent-control/3') if op == 'E' else obj)
                request = arguments(native.client.observe(), action=action)
                with self.assertRaises(a.ActionOutcomeUnknown) as error: native.client.execute(**request)
                self.assertEqual(error.exception.command_id, COMMAND); count = len(native.requests)
                for operation in (native.client.observe, lambda: native.client.execute(**request), lambda: native.client.read_result(COMMAND)):
                    with self.assertRaises(Unavailable): operation()
                self.assertEqual(native.client.state, 'CLOSED'); self.assertEqual(len(native.requests), count)
                self.assertEqual(sum(req.startswith(b'E ') for req in native.requests), 1)

    def test_all_list_tool_outputs_remain_text_only(self):
        for screen, action in (('new_battle', 'open_new_battle_choice'), ('new_battle_dropdown', 'choose_visible_row'),
                              ('new_battle_dropdown', 'close_dropdown'), ('funding', 'scroll_visible_list'),
                              ('geoscape', 'open_funding'), ('funding', 'close_funding')):
            with self.subTest(action=action):
                native = self.fixture(screen=screen); server = adapter.Server(agent_control=native.client)
                observed = call(server, 'read_ui_state')['result']
                receipt = call(server, 'execute_ui_action', arguments(observed['structuredContent'], action=action))['result']
                status = call(server, 'read_ui_action_result', {'command_id': COMMAND})['result']
                for reply in (observed, receipt, status):
                    self.assertFalse(reply['isError']); self.assertEqual([item['type'] for item in reply['content']], ['text'])
                    self.assertEqual(json.loads(reply['content'][0]['text']), reply['structuredContent'])


class McpTests(unittest.TestCase):
    def test_tools_explicit_and_distinct_from_supervised(self):
        self.assertFalse({d['name'] for d in adapter.definitions()} & set(a.TOOL_NAMES))
        enabled = {d['name']: d for d in adapter.definitions(agent_control=True)}
        self.assertTrue(set(a.TOOL_NAMES) <= set(enabled)); self.assertNotIn('request_stance', enabled)
        self.assertFalse(enabled['execute_ui_action']['annotations']['readOnlyHint'])
        self.assertIn('not human confirmation', enabled['execute_ui_action']['description'])
        with self.assertRaises(ValueError): adapter.Server(agent_control=object(), supervised_stance=object())
        for name in a.TOOL_NAMES: self.assertEqual(call(adapter.Server(), name)['error']['code'], -32602)

    def test_initialize_has_distinct_identity(self):
        reply = adapter.Server(agent_control=object()).handle(dict(jsonrpc='2.0', id=1, method='initialize',
            params=dict(protocolVersion=adapter.PROTOCOL, capabilities={}, clientInfo=dict(name='test', version='1'))))
        self.assertEqual(reply['result']['serverInfo']['name'], 'openxcom-semantic-agent-control')
        self.assertEqual(reply['result']['serverInfo']['version'], '0.25.0')

    def test_malformed_tool_args_never_call_client(self):
        class Never:
            def __getattr__(self, key): raise AssertionError('Client invoked')
        server = adapter.Server(agent_control=Never())
        cases = [('execute_ui_action', {}), ('execute_ui_action', dict(arguments(), approved=True)),
                 ('execute_ui_action', arguments(action='toggle_kneel')), ('read_ui_state', {'x': 1}),
                 ('read_ui_action_result', {'command_id': 'bad'}), ('read_ui_action_result', {})]
        for name, args in cases:
            with self.subTest(name=name, args=args): self.assertEqual(call(server, name, args)['error']['code'], -32602)
        for args in (None, [], False):
            reply = server.handle(dict(jsonrpc='2.0', id=1, method='tools/call', params=dict(name='read_ui_state', arguments=args)))
            self.assertEqual(reply['error']['code'], -32602)

    def test_structured_output_contains_no_image(self):
        native = NativeFixture(); self.addCleanup(native.close); server = adapter.Server(agent_control=native.client)
        reply = call(server, 'read_ui_state')['result']; self.assertFalse(reply['isError']); obj = reply['structuredContent']
        self.assertEqual(reply['content'], [dict(type='text', text=dump(obj).decode())])
        receipt = call(server, 'execute_ui_action', arguments(obj))['result']
        self.assertEqual(receipt['structuredContent']['status'], 'dispatched')
        self.assertEqual(call(server, 'read_ui_action_result', dict(command_id=COMMAND))['result'], receipt)
        self.assertFalse(any(item['type'] == 'image' for item in receipt['content']))

    def test_lost_action_reply_exposes_adapter_uncertainty_not_native_receipt(self):
        native = NativeFixture(transform=lambda op, obj: None if op == 'E' else obj)
        self.addCleanup(native.close); server = adapter.Server(agent_control=native.client)
        obj = call(server, 'read_ui_state')['result']['structuredContent']
        reply = call(server, 'execute_ui_action', arguments(obj))['result']
        expected = dict(schema=a.ERROR_SCHEMA, mode=a.MODE, type='adapter_error', source='adapter',
                        command_id=COMMAND, status='outcome_unknown', retryable=False, semantics='not_a_native_receipt',
                        failure=dict(code='transport_timeout',category='transport',severity='critical',
                                     phase='send_or_receipt',action_outcome='unknown',recovery='reconcile'))
        self.assertTrue(reply['isError']); self.assertEqual(reply['structuredContent'], expected)
        self.assertEqual(reply['content'], [dict(type='text', text=dump(expected).decode())])
        self.assertNotIn('observation', expected)
        count = len(native.requests)
        self.assertTrue(call(server, 'execute_ui_action', arguments(obj))['result']['isError'])
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'CLOSED')

    def test_failure_bounded_no_hidden_reason(self):
        class Failed:
            def observe(self): raise Unavailable('PRIVATE-SENTINEL')
        reply = call(adapter.Server(agent_control=Failed()), 'read_ui_state')['result']
        expected={'error':'unavailable','failure':dict(code='legacy_unavailable',category='unknown',
                  severity='error',phase='read',action_outcome='not_applicable',recovery='stop')}
        self.assertEqual(reply, dict(content=[dict(type='text',text=dump(expected).decode())],
                                    structuredContent=expected,isError=True))
        self.assertNotIn('PRIVATE', dump(reply).decode())


class LauncherTests(unittest.TestCase):
    def test_explicit_only_env_and_private_descriptor(self):
        for flags, enabled, count in (([], False, 1), (['--audio-only'], False, 1), (['--supervised-stance'], False, 2),
                                      (['--agent-control'], True, 2), (['--agent-control', '--mixed-audio'], True, 3)):
            with self.subTest(flags=flags), patch.object(sys, 'argv', ['launcher', *flags, '--', 'synthetic-game']), \
                    patch.dict(os.environ, {'OPENXCOM_AGENT_CONTROL': '1', 'OPENXCOM_AGENT_CONTROL_FD': '999'}), \
                    patch('launch_presentation.subprocess.Popen') as spawn, patch('launch_presentation.serve') as serve:
                launch_presentation.main(); env = spawn.call_args.kwargs['env']
                self.assertEqual('OPENXCOM_AGENT_CONTROL' in env, enabled); self.assertEqual('OPENXCOM_AGENT_CONTROL_FD' in env, enabled)
                if enabled:
                    self.assertEqual(env['OPENXCOM_AGENT_CONTROL'], '1'); self.assertIn(int(env['OPENXCOM_AGENT_CONTROL_FD']), spawn.call_args.kwargs['pass_fds'])
                self.assertEqual(len(set(spawn.call_args.kwargs['pass_fds'])), count); self.assertEqual(serve.call_args.args[7] is not None, enabled)
                self.assertEqual(spawn.call_args.kwargs['stdout'], subprocess.DEVNULL); self.assertEqual(spawn.call_args.kwargs['stderr'], subprocess.DEVNULL)
                spawn.return_value.kill.assert_not_called(); spawn.return_value.terminate.assert_not_called()

    def test_cannot_mix_agent_with_any_supervised_or_audio_only(self):
        flags = ['--supervised-stance', '--supervised-selection', '--supervised-turn', '--supervised-map-left-click',
                 '--supervised-inventory', '--supervised-inventory-close', '--supervised-inventory-ground', '--supervised-inventory-click',
                 '--supervised-inventory-right-click', '--supervised-inventory-unload', '--audio-only']
        for flag in flags:
            with self.subTest(flag=flag), patch.object(sys, 'argv', ['launcher', '--agent-control', flag, '--', 'game']), \
                    patch('launch_presentation.subprocess.Popen') as spawn, patch('sys.stderr', io.StringIO()), self.assertRaises(SystemExit):
                launch_presentation.main()
            spawn.assert_not_called()

    def test_real_stdio_launcher_typed_strategy_flow(self):
        steps = [('new_game', 'select_difficulty', {'value': 'veteran'}, ' veteran', 'new_game'),
                 ('new_game', 'set_ironman', {'value': False}, ' 0', 'new_game'),
                 ('new_game', 'start_new_game', {}, '', 'build_base'),
                 ('build_base', 'place_base_site', {'x': 64, 'y': 80}, ' 64 80', 'base_name'),
                 ('base_name', 'replace_base_name', {'text': 'Café'}, ' 436166c3a9', 'base_name'),
                 ('base_name', 'confirm_base_name', {}, '', 'geoscape')]
        fixtures, messages = [], [dict(jsonrpc='2.0', id=1, method='initialize', params=dict(
            protocolVersion=adapter.PROTOCOL, capabilities={}, clientInfo=dict(name='typed-synthetic', version='1'))),
            dict(jsonrpc='2.0', method='notifications/initialized')]
        for index, (screen, action, values, suffix, after) in enumerate(steps):
            cap, command = f'{index + 30:032x}', f'{index + 60:032x}'
            observed = state(capability=cap, revision=index + 1, event_sequence=index * 2 + 1, screen=screen)
            post = state(None, None, revision=index + 2, event_sequence=index * 2 + 2, screen=after)
            if index >= 4: observed['visible']['name'] = '' if index == 4 else 'Café'
            if index == 4: post['visible']['name'] = 'Café'
            wire = f'E {command} {EPOCH} {cap} {index + 1} {action}{suffix}'
            fixtures.append([observed, wire, result(command, event_sequence=index * 2 + 2, observation=post)])
            args = arguments(observed, command_id=command, action=action, args=values)
            for name, values in (('read_ui_state', {}), ('execute_ui_action', args), ('read_ui_action_result', {'command_id': command})):
                messages.append(dict(jsonrpc='2.0', id=len(messages), method='tools/call', params=dict(name=name, arguments=values)))
        child = r'''
import json, os, socket
channel = socket.socket(fileno=int(os.environ['OPENXCOM_AGENT_CONTROL_FD']))
def send(obj): channel.send(json.dumps(obj, separators=(',', ':')).encode())
send(dict(schema='openxcom-agent-control/30',mode='agent-control',epoch='1'*32,ledger_capacity=256, audio_history='mixed_output'))
assert channel.recv(512) == b'H openxcom-agent-control/30'
send(dict(schema='openxcom-agent-control/30',mode='agent-control',status='ready'))
for observed, wire, receipt in FIXTURES:
    request = channel.recv(512).decode(); assert request.startswith('R ')
    observed['challenge'] = request[2:]; send(observed)
    assert channel.recv(512).decode() == wire; send(receipt)
    assert channel.recv(512).decode() == 'S ' + receipt['command_id']; send(receipt)
assert channel.recv(512) == b''
'''
        child = 'FIXTURES = ' + repr(fixtures) + '\n' + child
        completed = subprocess.run([sys.executable, str(Path(__file__).with_name('launch_presentation.py')), '--agent-control',
            '--', sys.executable, '-c', child], input=b''.join(dump(message) + b'\n' for message in messages), capture_output=True, timeout=8)
        self.assertEqual(completed.returncode, 0); self.assertEqual(completed.stderr, b'')
        replies = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual(len(replies), 19)
        for reply in replies[1:]:
            self.assertFalse(reply['result']['isError'])
            self.assertEqual([item['type'] for item in reply['result']['content']], ['text'])
        self.assertEqual(replies[-1]['result']['structuredContent']['observation']['state'], 'geoscape')

    def test_real_stdio_launcher_typed_list_flow(self):
        steps = [('new_battle', 'open_new_battle_choice', {'control': 'mission'}, ' mission', 'new_battle_dropdown'),
                 ('new_battle_dropdown', 'scroll_visible_list', {'direction': 'down'}, ' down', 'new_battle_dropdown'),
                 ('new_battle_dropdown', 'choose_visible_row', {'row': 'r1'}, ' r1', 'new_battle'),
                 ('new_battle', 'open_new_battle_choice', {'control': 'mission'}, ' mission', 'new_battle_dropdown'),
                 ('new_battle_dropdown', 'close_dropdown', {}, '', 'new_battle'),
                 ('new_battle', 'cancel_menu', {}, '', 'geoscape'),
                 ('geoscape', 'open_funding', {}, '', 'funding'),
                 ('funding', 'scroll_visible_list', {'direction': 'up'}, ' up', 'funding'),
                 ('funding', 'close_funding', {}, '', 'geoscape')]
        fixtures, messages = [], [dict(jsonrpc='2.0', id=1, method='initialize', params=dict(
            protocolVersion=adapter.PROTOCOL, capabilities={}, clientInfo=dict(name='list-synthetic', version='1'))),
            dict(jsonrpc='2.0', method='notifications/initialized')]
        for index, (screen, action, values, suffix, after) in enumerate(steps):
            cap, command = f'{index + 30:032x}', f'{index + 60:032x}'
            observed = state(capability=cap, revision=index + 1, event_sequence=index * 2 + 1, screen=screen)
            post = state(None, None, revision=index + 2, event_sequence=index * 2 + 2, screen=after)
            fixtures.append([observed, f'E {command} {EPOCH} {cap} {index + 1} {action}{suffix}',
                             result(command, event_sequence=index * 2 + 2, observation=post)])
            values = arguments(observed, command_id=command, action=action, args=values)
            for name, args in (('read_ui_state', {}), ('execute_ui_action', values), ('execute_ui_action', values)):
                messages.append(dict(jsonrpc='2.0', id=len(messages), method='tools/call', params=dict(name=name, arguments=args)))
        child = r'''
import json, os, socket
channel = socket.socket(fileno=int(os.environ['OPENXCOM_AGENT_CONTROL_FD']))
def send(obj): channel.send(json.dumps(obj, separators=(',', ':')).encode())
send(dict(schema='openxcom-agent-control/30',mode='agent-control',epoch='1'*32,ledger_capacity=256, audio_history='mixed_output'))
assert channel.recv(512) == b'H openxcom-agent-control/30'
send(dict(schema='openxcom-agent-control/30',mode='agent-control',status='ready'))
for observed, wire, receipt in FIXTURES:
    request = channel.recv(512).decode(); assert request.startswith('R ')
    observed['challenge'] = request[2:]; send(observed)
    assert channel.recv(512).decode() == wire; send(receipt)
    assert channel.recv(512).decode() == 'S ' + receipt['command_id']; send(receipt)
assert channel.recv(512) == b''
'''
        child = 'FIXTURES = ' + repr(fixtures) + '\n' + child
        completed = subprocess.run([sys.executable, str(Path(__file__).with_name('launch_presentation.py')), '--agent-control',
            '--', sys.executable, '-c', child], input=b''.join(dump(message) + b'\n' for message in messages), capture_output=True, timeout=8)
        self.assertEqual(completed.returncode, 0); self.assertEqual(completed.stderr, b'')
        replies = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual(len(replies), 28)
        for index in range(len(steps)):
            triple = replies[index * 3 + 1:index * 3 + 4]
            self.assertEqual(triple[1]['result'], triple[2]['result'])
            for reply in triple:
                self.assertFalse(reply['result']['isError'])
                self.assertEqual([item['type'] for item in reply['result']['content']], ['text'])
        self.assertEqual(replies[-1]['result']['structuredContent']['observation']['state'], 'geoscape')

    def test_real_stdio_launcher_synthetic_peer(self):
        child = r'''
import json, os, socket, sys
assert os.environ['OPENXCOM_AGENT_CONTROL'] == '1' and 'OPENXCOM_STANCE_FD' not in os.environ
channel = socket.socket(fileno=int(os.environ['OPENXCOM_AGENT_CONTROL_FD']))
assert channel.family == socket.AF_UNIX and channel.type == socket.SOCK_SEQPACKET
assert channel.getsockname() in ('', b'') and channel.getpeername() in ('', b'')
print('PRIVATE CHILD STDOUT'); print('PRIVATE CHILD STDERR', file=sys.stderr)
def send(obj): channel.send(json.dumps(obj, separators=(',', ':')).encode())
schema, mode, epoch = 'openxcom-agent-control/30', 'agent-control', '1' * 32
send(dict(schema=schema,mode=mode,epoch=epoch,ledger_capacity=256, audio_history='mixed_output'))
assert channel.recv(256) == b'H openxcom-agent-control/30'; send(dict(schema=schema,mode=mode,status='ready'))
raw=channel.recv(256).decode(); assert raw.startswith('R ')
observation=dict(schema=schema,mode=mode,type='state',challenge=raw[2:],epoch=epoch,revision=1,capability='2'*32,
 cursor=None,state='next_turn',visible=dict(title='UFO',turn='Turn 1',side='XCOM',message='Continue'),
 actions=[dict(id='continue_next_turn',label='Continue',label_source='visible_text',argument_kind='none')],event_sequence=1,semantics='presented_after_request')
send(observation)
assert channel.recv(256) == ('E '+'3'*32+' '+epoch+' '+'2'*32+' 1 continue_next_turn').encode()
observation.update(challenge=None,capability=None,revision=2,event_sequence=2)
receipt=dict(schema=schema,mode=mode,type='result',command_id='3'*32,status='dispatched',event_sequence=2,
 observation=observation,semantics='dispatch_not_success'); send(receipt)
assert channel.recv(256) == ('S '+'3'*32).encode(); send(receipt)
assert channel.recv(256) == b''
'''
        messages = [dict(jsonrpc='2.0', id=1, method='initialize', params=dict(protocolVersion=adapter.PROTOCOL,
                    capabilities={}, clientInfo=dict(name='synthetic', version='1'))),
                    dict(jsonrpc='2.0', method='notifications/initialized'), dict(jsonrpc='2.0', id=2, method='tools/list')]
        for ident, (name, args) in enumerate((('read_ui_state', {}), ('execute_ui_action', arguments(action='continue_next_turn')),
                                            ('execute_ui_action', arguments(action='continue_next_turn'))), 3):
            messages.append(dict(jsonrpc='2.0', id=ident, method='tools/call', params=dict(name=name, arguments=args)))
        completed = subprocess.run([sys.executable, str(Path(__file__).with_name('launch_presentation.py')), '--agent-control', '--', sys.executable, '-c', child],
                                   input=b''.join(dump(message)+b'\n' for message in messages), capture_output=True, timeout=8)
        self.assertEqual(completed.returncode, 0); self.assertEqual(completed.stderr, b'')
        replies = [json.loads(line) for line in completed.stdout.splitlines()]; self.assertEqual(len(replies), 5)
        self.assertEqual(replies[0]['result']['serverInfo']['name'], 'openxcom-semantic-agent-control')
        self.assertTrue(set(a.TOOL_NAMES) <= {t['name'] for t in replies[1]['result']['tools']})
        self.assertTrue(all(not response['result']['isError'] for response in replies[2:])); self.assertNotIn(b'PRIVATE', completed.stdout)
        self.assertEqual(replies[3]['result'], replies[4]['result'])


if __name__ == '__main__': unittest.main()
