# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Explicit semantic agent control over one inherited anonymous local socket.

This is a distinct mode, never a way to approve the supervised human pilot.
Only the bounded visible UI schema below is accepted. Receipts describe dispatch,
not gameplay success; every new command needs a fresh read and one-use capability.
Uncertain transport is terminal and never automatically retried.
"""
from contextlib import contextmanager
import json
import math
import re
import secrets
import select
import socket
import threading
import time

from adapter import strict_json
from snapshots import Unavailable, exact, integer, require

SCHEMA = 'openxcom-agent-control/25'
ERROR_SCHEMA = 'openxcom-mcp-agent-control-error/1'
MODE = 'agent-control'
MAX_PACKET = 49152
MAX_TEXT_PACKET = 32768
MAX_REQUEST = 512
MAX_NAME_BYTES = 128
MAX_COMMANDS = 256
MAX_COUNTER = 2**53 - 1
EVENT_CAPACITY, MAX_EVENTS = 256, 16
AUDIO_HISTORY = ('mixed_output', 'unavailable')
AUDIO_STATUSES = ('pending', 'active', 'unavailable')
TIMEOUT, ACTION_TIMEOUT, STARTUP_TIMEOUT = 2.0, 31.0, 31.0
CAPABILITY_TTL = 120.0
ACTIONS = ('next_soldier', 'previous_soldier', 'kneel', 'stand', 'continue_next_turn',
           'new_battle', 'start_battle', 'continue_briefing', 'close_inventory',
           'open_new_game', 'open_options', 'open_load_game', 'open_pause_menu',
           'open_save_game', 'resume_game', 'cancel_menu', 'dismiss_error',
           'select_difficulty', 'set_ironman', 'start_new_game', 'place_base_site',
           'replace_base_name', 'confirm_base_name', 'open_new_battle_choice',
           'choose_visible_row', 'scroll_visible_list', 'close_dropdown',
           'open_funding', 'close_funding', 'end_turn', 'acknowledge_message',
           'map_primary', 'map_secondary', 'center_selected_unit', 'center_enemy',
           'open_hand_action_menu', 'choose_weapon_action', 'cancel_weapon_action',
           'close_debriefing', 'open_inventory', 'inventory_hover', 'inventory_activate',
           'cancel_inventory_hold', 'unload_inventory', 'next_inventory_ground_page',
           'choose_grenade_timer', 'cancel_grenade_timer',
           'view_level_up', 'view_level_down', 'toggle_layers', 'open_minimap',
           'close_minimap', 'minimap_center', 'camera_pan', 'unit_up', 'unit_down',
           'choose_geoscape_speed', 'open_bases', 'select_base', 'open_base_info',
           'close_base_info', 'return_geoscape', 'open_base_stores', 'close_base_stores',
           'open_base_transfers', 'close_base_transfers',
           'open_research', 'open_new_research', 'select_research_project', 'adjust_scientists',
           'accept_research', 'cancel_research_project', 'close_research_list',
           'open_manufacture', 'open_new_production', 'select_production_project', 'start_production',
           'cancel_production_start', 'adjust_engineers', 'accept_production', 'stop_production', 'close_production_list',
           'open_transfer', 'cancel_transfer_destination', 'change_transfer_quantity', 'review_transfer',
           'cancel_transfer_items', 'confirm_transfer', 'cancel_transfer_confirmation',
           'change_production_quantity', 'set_production_sell', 'open_purchase', 'change_purchase_quantity',
           'accept_purchase', 'cancel_purchase', 'open_purchase_category', 'scroll_purchase', 'position_purchase_list',
           'dismiss_items_arriving', 'open_arrival_base', 'dismiss_production_complete',
           'open_production_destination', 'dismiss_ufo_contact',
           'open_soldiers', 'select_soldier', 'close_soldiers', 'previous_personnel',
           'next_personnel', 'open_soldier_armor', 'close_soldier_info', 'open_crafts',
           'select_craft', 'close_crafts', 'open_craft_crew', 'open_craft_equipment',
           'open_craft_armor', 'close_craft_info', 'set_craft_soldier', 'open_craft_soldier_info',
           'close_craft_crew', 'change_craft_equipment', 'close_craft_equipment', 'select_craft_armor_soldier',
           'close_craft_armor', 'select_soldier_armor', 'cancel_soldier_armor',
           'dismiss_research_complete', 'view_research_reports', 'dismiss_new_research', 'open_research_allocation', 'dismiss_new_manufacture', 'open_manufacture_allocation', 'dismiss_research_required', 'close_ufopaedia_article', 'previous_ufopaedia_article', 'next_ufopaedia_article', 'scroll_article_text', 'dismiss_mission_alert')
DIFFICULTIES = ('beginner', 'experienced', 'veteran', 'genius', 'superhuman')
CHOICE_CONTROLS = ('mission', 'craft', 'terrain', 'difficulty', 'alien_race')
ROW_HANDLES = tuple(f'r{index}' for index in range(32))
SCROLL_DIRECTIONS = ('up', 'down')
GRID_HANDLES = tuple(f'g{index}' for index in range(128))
ENEMY_BUTTON_HANDLES = tuple(f'e{index}' for index in range(10))
WEAPON_ACTION_HANDLES = tuple(f'a{index}' for index in range(6))
GRENADE_TIMER_HANDLES = tuple(f't{index}' for index in range(24))
HANDS = ('left', 'right')
CAMERA_DIRECTIONS = ('left', 'right', 'up', 'down')
MINIMAP_TARGET_HANDLES = tuple(f'm{index}' for index in range(160))
GEOSCAPE_SPEED_HANDLES = tuple(f's{index}' for index in range(6))
BASE_TARGET_HANDLES = tuple(f'b{index}' for index in range(8))
BASE_BUTTONS = {'basescape': ('base_info', 'research', 'manufacture', 'purchase', 'transfer', 'soldiers', 'crafts', 'geoscape'), 'base_info': ('ok', 'stores', 'transfers')}
BASE_BUTTON_ACTIONS = {'basescape': {'open_base_info': 'base_info', 'open_research': 'research', 'open_manufacture': 'manufacture', 'open_purchase': 'purchase', 'open_transfer': 'transfer', 'open_soldiers': 'soldiers', 'open_crafts': 'crafts', 'return_geoscape': 'geoscape'},
                       'base_info': {'close_base_info': 'ok', 'open_base_stores': 'stores', 'open_base_transfers': 'transfers'}}
STAFF_DIRECTIONS = ('increase', 'decrease')
PROJECT_LISTS = {'research': (3, 3), 'new_research': (0, 1), 'manufacture': (4, 5), 'new_manufacture': (0, 2)}
PROJECT_BUTTONS = {
    'research': ('new', 'close'), 'new_research': ('close',),
    'manufacture': ('new', 'close'), 'new_manufacture': ('close',),
    'research_info': ('increase', 'decrease', 'accept', 'cancel'),
    'manufacture_start': ('start', 'cancel'),
    'manufacture_info': ('increase', 'decrease', 'accept', 'stop')}
PROJECT_BUTTON_ACTIONS = {
    'research': {'open_new_research': 'new', 'close_research_list': 'close'},
    'new_research': {'close_research_list': 'close'},
    'manufacture': {'open_new_production': 'new', 'close_production_list': 'close'},
    'new_manufacture': {'close_production_list': 'close'},
    'research_info': {'accept_research': 'accept', 'cancel_research_project': 'cancel'},
    'manufacture_start': {'start_production': 'start', 'cancel_production_start': 'cancel'},
    'manufacture_info': {'accept_production': 'accept', 'stop_production': 'stop'}}
PROJECT_SELECTION_ACTIONS = {'research': 'select_research_project', 'new_research': 'select_research_project',
                             'manufacture': 'select_production_project', 'new_manufacture': 'select_production_project'}
PROJECT_STAFF_ACTIONS = {'research_info': 'adjust_scientists', 'manufacture_info': 'adjust_engineers'}
QUANTITY_CONTROLS = ('engineers', 'units')
QUANTITY_OPERATIONS = ('adjust', 'set', 'max', 'clear')
PURCHASE_SCROLL_HANDLES = tuple(f't{index}' for index in range(3))
PURCHASE_POSITIONS = ('top', 'middle', 'bottom')
PURCHASE_AMOUNTS = ('step', 'page', 'end')
QUANTITY_SEMANTIC_LABELS = {'change_production_quantity': 'Change production quantity',
    'change_transfer_quantity': 'Change transfer quantity',
    'change_purchase_quantity': 'Change purchase quantity', 'scroll_purchase': 'Scroll purchase list',
    'position_purchase_list': 'Position purchase list'}
COMPLETION_BUTTONS = {'items_arriving': ('ok', 'go_to_base'),
    'production_complete': ('ok', 'destination'), 'ufo_contact': ('intercept', 'center', 'cancel')}
COMPLETION_BUTTON_ACTIONS = {
    'items_arriving': {'dismiss_items_arriving': 'ok', 'open_arrival_base': 'go_to_base'},
    'production_complete': {'dismiss_production_complete': 'ok', 'open_production_destination': 'destination'},
    'ufo_contact': {'dismiss_ufo_contact': 'cancel'}}
OUTFITTING_FIELDS = {
    'soldiers': 'title headings rows scroll buttons',
    'soldier_info': 'name rank missions kills craft recovery training stats buttons',
    'crafts': 'title base headings rows scroll buttons',
    'craft_info': 'name damage fuel weapons buttons',
    'craft_soldiers': 'title headings available used rows scroll buttons',
    'craft_equipment': 'title headings available used crew rows scroll buttons',
    'craft_armor': 'title headings rows scroll buttons',
    'soldier_armor': 'title headings rows scroll buttons',
}
OUTFITTING_LISTS = {'soldiers': (3, 3), 'crafts': (5, 5), 'craft_soldiers': (3, 3), 'craft_equipment': (2, 3), 'craft_armor': (3, 3), 'soldier_armor': (2, 2)}
OUTFITTING_BUTTONS = {
    'soldiers': ('close',),
    'soldier_info': ('previous', 'next', 'armor', 'close'),
    'crafts': ('close',),
    'craft_info': ('crew', 'equipment', 'armor', 'close'),
    'craft_soldiers': ('close',),
    'craft_equipment': ('close',),
    'craft_armor': ('close',),
    'soldier_armor': ('cancel',),
}
OUTFITTING_BUTTON_ACTIONS = {
    'soldiers': {'close_soldiers': 'close'},
    'soldier_info': {'previous_personnel': 'previous', 'next_personnel': 'next', 'open_soldier_armor': 'armor', 'close_soldier_info': 'close'},
    'crafts': {'close_crafts': 'close'},
    'craft_info': {'open_craft_crew': 'crew', 'open_craft_equipment': 'equipment', 'open_craft_armor': 'armor', 'close_craft_info': 'close'},
    'craft_soldiers': {'close_craft_crew': 'close'},
    'craft_equipment': {'close_craft_equipment': 'close'},
    'craft_armor': {'close_craft_armor': 'close'},
    'soldier_armor': {'cancel_soldier_armor': 'cancel'},
}
OUTFITTING_ROW_ACTIONS = {
    'soldiers': ('select_soldier',),
    'soldier_info': (),
    'crafts': ('select_craft',),
    'craft_info': (),
    'craft_soldiers': ('set_craft_soldier', 'open_craft_soldier_info'),
    'craft_equipment': ('change_craft_equipment',),
    'craft_armor': ('select_craft_armor_soldier',),
    'soldier_armor': ('select_soldier_armor',),
}
OUTFITTING_SEMANTIC_LABELS = {'select_soldier': 'Select soldier', 'select_craft': 'Select craft', 'set_craft_soldier': 'Set craft assignment', 'open_craft_soldier_info': 'Inspect soldier', 'change_craft_equipment': 'Change craft equipment', 'select_craft_armor_soldier': 'Select soldier armor', 'select_soldier_armor': 'Select armor'}
RESEARCH_NOTICE_STATES = ('new_possible_research', 'new_possible_manufacture')
RESEARCH_COMPLETION_FIELDS = {'research_complete': 'title topic buttons',
 'new_possible_research': 'title rows scroll buttons',
 'new_possible_manufacture': 'title rows scroll buttons',
 'research_required': 'message buttons',
 'ufopaedia_article': 'title body_lines buttons details_status',
 'mission_alert': 'title city buttons'}
RESEARCH_COMPLETION_BUTTONS = {'research_complete': ('ok', 'reports'),
 'new_possible_research': ('ok', 'allocate'),
 'new_possible_manufacture': ('ok', 'allocate'),
 'research_required': ('ok',),
 'ufopaedia_article': ('ok', 'previous', 'next'),
 'mission_alert': ('intercept', 'center', 'cancel')}
RESEARCH_COMPLETION_BUTTON_ACTIONS = {'research_complete': {'dismiss_research_complete': 'ok', 'view_research_reports': 'reports'},
 'new_possible_research': {'dismiss_new_research': 'ok', 'open_research_allocation': 'allocate'},
 'new_possible_manufacture': {'dismiss_new_manufacture': 'ok', 'open_manufacture_allocation': 'allocate'},
 'research_required': {'dismiss_research_required': 'ok'},
 'ufopaedia_article': {'close_ufopaedia_article': 'ok',
                       'previous_ufopaedia_article': 'previous',
                       'next_ufopaedia_article': 'next'},
 'mission_alert': {'dismiss_mission_alert': 'cancel'}}
RESEARCH_COMPLETION_ACTION_ORDER = {'research_complete': ('dismiss_research_complete', 'view_research_reports'),
 'new_possible_research': ('dismiss_new_research', 'open_research_allocation', 'scroll_visible_list'),
 'new_possible_manufacture': ('dismiss_new_manufacture',
                              'open_manufacture_allocation',
                              'scroll_visible_list'),
 'research_required': ('dismiss_research_required',),
 'ufopaedia_article': ('close_ufopaedia_article',
                       'previous_ufopaedia_article',
                       'next_ufopaedia_article',
                       'scroll_article_text'),
 'mission_alert': ('dismiss_mission_alert',)}
INVENTORY_TARGET_HANDLES = tuple(f'v{index}' for index in range(96))
INVENTORY_TEXT_FIELDS = ('selected_name', 'time_units', 'weight', 'item_name', 'ammo', 'warning')
ARGUMENT_KINDS = dict.fromkeys(ACTIONS, 'none')
ARGUMENT_KINDS.update(select_difficulty='difficulty', set_ironman='boolean',
                      replace_base_name='text', place_base_site='point',
                      open_new_battle_choice='choice_control', choose_visible_row='visible_row',
                      scroll_visible_list='scroll_direction', scroll_article_text='scroll_direction', map_primary='grid_cell', map_secondary='grid_cell',
                      center_enemy='enemy_button', open_hand_action_menu='hand', choose_weapon_action='weapon_action',
                      inventory_hover='inventory_target', inventory_activate='inventory_target',
                      choose_grenade_timer='grenade_timer', camera_pan='camera_direction',
                      minimap_center='minimap_target', choose_geoscape_speed='geoscape_speed',
                      select_base='base_target', select_research_project='project_row', select_production_project='project_row',
                      adjust_scientists='staff_direction', adjust_engineers='staff_direction',
                      change_production_quantity='production_quantity', set_production_sell='boolean_value',
                      change_transfer_quantity='transfer_quantity', change_purchase_quantity='purchase_quantity', scroll_purchase='purchase_scroll',
                      position_purchase_list='purchase_position',
                      select_soldier='outfitting_row', select_craft='outfitting_row', set_craft_soldier='crew_assignment', open_craft_soldier_info='outfitting_row', change_craft_equipment='equipment_quantity', select_craft_armor_soldier='outfitting_row', select_soldier_armor='outfitting_row')
STATE_FIELDS = {
    'next_turn': ('title', 'turn', 'side', 'message'),
    'main_menu': ('new_battle_label',),
    'new_battle': ('mission', 'craft', 'terrain', 'difficulty', 'alien_race'),
    'briefing': ('title', 'target', 'craft', 'briefing'),
    'base_name': ('title', 'name'),
    'options_shell': ('category', 'tooltip'),
    'pause_menu': ('title',),
    'load_list': ('title', 'name_heading', 'date_heading', 'details'),
    'save_list': ('title', 'name_heading', 'date_heading', 'details'),
    'error_message': ('message', 'heading'),
    'infobox': ('message',),
    'infobox_ok': ('message',),
    'battlescape_warning': ('message',),
}
STATE_ACTIONS = {
    'battlescape': frozenset(('next_soldier', 'previous_soldier', 'kneel', 'stand', 'open_pause_menu', 'end_turn',
                              'map_primary', 'map_secondary', 'center_selected_unit', 'center_enemy',
                              'open_hand_action_menu', 'open_inventory', 'view_level_up', 'view_level_down',
                              'toggle_layers', 'open_minimap', 'camera_pan', 'unit_up', 'unit_down')),
    'minimap': frozenset(('view_level_up', 'view_level_down', 'close_minimap', 'minimap_center')),
    'next_turn': frozenset(('continue_next_turn',)),
    'main_menu': frozenset(('new_battle', 'open_new_game', 'open_options', 'open_load_game')),
    'new_battle': frozenset(('start_battle', 'cancel_menu', 'open_new_battle_choice')),
    'new_battle_dropdown': frozenset(('choose_visible_row', 'scroll_visible_list', 'close_dropdown')),
    'funding': frozenset(('scroll_visible_list', 'close_funding')),
    'briefing': frozenset(('continue_briefing',)),
    'inventory': frozenset(('close_inventory', 'inventory_hover', 'inventory_activate',
                              'cancel_inventory_hold', 'unload_inventory', 'next_inventory_ground_page')),
    'new_game': frozenset(('cancel_menu', 'select_difficulty', 'set_ironman', 'start_new_game')),
    'base_name': frozenset(('replace_base_name', 'confirm_base_name')),
    'build_base': frozenset(('place_base_site',)),
    'geoscape': frozenset(('open_funding', 'choose_geoscape_speed', 'open_bases', 'open_pause_menu')),
    'basescape': frozenset(('select_base', 'open_base_info', 'open_research', 'open_manufacture', 'open_purchase', 'open_transfer', 'open_soldiers', 'open_crafts', 'return_geoscape')),
    'base_info': frozenset(('select_base', 'close_base_info', 'open_base_stores', 'open_base_transfers')),
    'base_stores': frozenset(('scroll_visible_list', 'close_base_stores')),
    'base_transfers': frozenset(('scroll_visible_list', 'close_base_transfers')),
    'research': frozenset(('open_new_research', 'select_research_project', 'scroll_visible_list', 'close_research_list')),
    'new_research': frozenset(('select_research_project', 'scroll_visible_list', 'close_research_list')),
    'research_info': frozenset(('adjust_scientists', 'accept_research', 'cancel_research_project')),
    'manufacture': frozenset(('open_new_production', 'select_production_project', 'scroll_visible_list', 'close_production_list')),
    'new_manufacture': frozenset(('select_production_project', 'scroll_visible_list', 'close_production_list')),
    'manufacture_start': frozenset(('start_production', 'cancel_production_start', 'scroll_visible_list')),
    'manufacture_info': frozenset(('adjust_engineers', 'accept_production', 'stop_production', 'change_production_quantity', 'set_production_sell')),
    'transfer_destination': frozenset(('choose_visible_row', 'scroll_visible_list', 'cancel_transfer_destination')),
    'transfer_items': frozenset(('change_transfer_quantity', 'scroll_visible_list', 'review_transfer', 'cancel_transfer_items')),
    'transfer_confirm': frozenset(('confirm_transfer', 'cancel_transfer_confirmation')),
    'purchase': frozenset(('change_purchase_quantity', 'accept_purchase', 'cancel_purchase',
                           'open_purchase_category', 'scroll_purchase', 'position_purchase_list')),
    'purchase_category': frozenset(('choose_visible_row', 'scroll_visible_list', 'close_dropdown')),
    'items_arriving': frozenset(('dismiss_items_arriving', 'open_arrival_base', 'scroll_visible_list')),
    'production_complete': frozenset(('dismiss_production_complete', 'open_production_destination')),
    'ufo_contact': frozenset(('dismiss_ufo_contact',)),
    'soldiers': frozenset(('select_soldier', 'close_soldiers', 'scroll_visible_list')),
    'soldier_info': frozenset(('previous_personnel', 'next_personnel', 'open_soldier_armor', 'close_soldier_info')),
    'crafts': frozenset(('select_craft', 'close_crafts', 'scroll_visible_list')),
    'craft_info': frozenset(('open_craft_crew', 'open_craft_equipment', 'open_craft_armor', 'close_craft_info')),
    'craft_soldiers': frozenset(('set_craft_soldier', 'open_craft_soldier_info', 'close_craft_crew', 'scroll_visible_list')),
    'craft_equipment': frozenset(('change_craft_equipment', 'close_craft_equipment', 'scroll_visible_list')),
    'craft_armor': frozenset(('select_craft_armor_soldier', 'close_craft_armor', 'scroll_visible_list')),
    'soldier_armor': frozenset(('select_soldier_armor', 'cancel_soldier_armor', 'scroll_visible_list')),
    'options_shell': frozenset(('cancel_menu',)),
    'pause_menu': frozenset(('open_load_game', 'open_save_game', 'open_options', 'resume_game')),
    'load_list': frozenset(('cancel_menu',)),
    'save_list': frozenset(('cancel_menu',)),
    'error_message': frozenset(('dismiss_error',)),
    'infobox': frozenset(),
    'infobox_ok': frozenset(('acknowledge_message',)),
    'battlescape_warning': frozenset(),
    'weapon_action_menu': frozenset(('choose_weapon_action', 'cancel_weapon_action')),
    'prime_grenade': frozenset(('choose_grenade_timer', 'cancel_grenade_timer')),
    'debriefing': frozenset(('close_debriefing',)),
    'research_complete': frozenset(('dismiss_research_complete', 'view_research_reports')),
    'new_possible_research': frozenset(('dismiss_new_research', 'open_research_allocation', 'scroll_visible_list')),
    'new_possible_manufacture': frozenset(('dismiss_new_manufacture', 'open_manufacture_allocation', 'scroll_visible_list')),
    'research_required': frozenset(('dismiss_research_required',)),
    'ufopaedia_article': frozenset(('close_ufopaedia_article', 'previous_ufopaedia_article', 'next_ufopaedia_article', 'scroll_article_text')),
    'mission_alert': frozenset(('dismiss_mission_alert',)),
    'unsupported': frozenset(),
}
GEOSCAPE_FIELDS = ('hour', 'minute', 'second', 'weekday', 'day', 'month', 'year', 'funds', 'speed')
GEOSCAPE_BUTTONS = ('intercept', 'bases', 'graphs', 'ufopaedia', 'options', 'funding')
STATUSES = ('pending', 'dispatched', 'unavailable', 'unknown_after_dispatch', 'unknown')
TERMINAL_STATUSES = ('dispatched', 'unavailable', 'unknown_after_dispatch')
TOOL_NAMES = ('read_ui_state', 'execute_ui_action', 'read_ui_action_result', 'read_public_events')
FAILURES = (OSError, ValueError, UnicodeError, RecursionError, TypeError)


class ActionOutcomeUnknown(Unavailable):
    """The adapter attempted a mutating send but has no trustworthy receipt.

    This is local transport uncertainty, never a native dispatch/outcome report.
    The client has already closed and cannot reconnect or replay the command.
    """
    def __init__(self, command_id):
        super().__init__('Action outcome unknown')
        self.command_id = command_id

    def public_error(self):
        return dict(schema=ERROR_SCHEMA, mode=MODE, type='adapter_error', source='adapter',
                    command_id=self.command_id, status='outcome_unknown', retryable=False,
                    semantics='not_a_native_receipt')


def token(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{32}', value) is not None)


def visible_text(value, nullable=False):
    if nullable and value is None:
        return
    require(type(value) is str)
    try:
        require(len(value.encode('utf-8')) <= 4096)
    except UnicodeError:
        raise Unavailable('Unavailable') from None


def parse(raw):
    require(type(raw) is bytes and 0 < len(raw) <= MAX_PACKET)
    try:
        return strict_json(raw.decode('utf-8'))
    except (ValueError, UnicodeError, RecursionError, TypeError):
        raise Unavailable('Unavailable') from None


def identity(obj):
    require(obj['schema'] == SCHEMA and obj['mode'] == MODE)


def validate_hello(raw):
    obj = parse(raw)
    exact(obj, 'schema mode epoch ledger_capacity audio_history')
    identity(obj)
    token(obj['epoch'])
    integer(obj['ledger_capacity'], MAX_COMMANDS, MAX_COMMANDS)
    require(type(obj['audio_history']) is str and obj['audio_history'] in AUDIO_HISTORY)
    return obj['epoch']


def validate_ready(raw):
    obj = parse(raw)
    exact(obj, 'schema mode status')
    identity(obj)
    require(obj['status'] == 'ready')


def label_source(screen, action):
    if action in QUANTITY_SEMANTIC_LABELS or action in OUTFITTING_SEMANTIC_LABELS or (screen == 'new_research' and action == 'select_research_project'):
        return 'visible_control_semantics'
    return ('visible_control_semantics' if screen in ('battlescape', 'inventory', 'minimap') or
            action in ('replace_base_name', 'place_base_site', 'open_new_battle_choice',
                       'choose_visible_row', 'scroll_visible_list', 'scroll_article_text', 'close_dropdown', 'cancel_weapon_action',
                       'cancel_grenade_timer', 'select_base') else 'visible_text')


def validate_new_game(visible, actions):
    exact(visible, 'title ironman_note difficulty ironman')
    visible_text(visible['title'], nullable=True)
    visible_text(visible['ironman_note'], nullable=True)
    require(visible['ironman'] is None or type(visible['ironman']) is bool)
    choices = visible['difficulty']
    require(type(choices) is list and len(choices) <= len(DIFFICULTIES))
    seen, selected = set(), 0
    for choice in choices:
        exact(choice, 'value label selected')
        require(type(choice['value']) is str and choice['value'] in DIFFICULTIES and choice['value'] not in seen)
        seen.add(choice['value'])
        visible_text(choice['label']); require(bool(choice['label']))
        require(type(choice['selected']) is bool)
        selected += choice['selected']
    require(selected <= 1)
    require('select_difficulty' not in actions or (bool(seen) and visible['title'] is not None))
    require('set_ironman' not in actions or visible['ironman'] is not None)


def strategy_text(value, nullable=False):
    visible_text(value, nullable=nullable)
    require(value is None or len(value.encode('utf-8')) <= 1024)


def validate_geoscape(visible, actions):
    exact(visible, ' '.join(GEOSCAPE_FIELDS) + ' buttons speeds')
    for field in GEOSCAPE_FIELDS:
        visible_text(visible[field], nullable=True)
    require(type(visible['buttons']) is list and len(visible['buttons']) <= len(GEOSCAPE_BUTTONS))
    seen = set()
    for button in visible['buttons']:
        exact(button, 'id label')
        require(type(button['id']) is str and button['id'] in GEOSCAPE_BUTTONS and button['id'] not in seen)
        seen.add(button['id'])
        visible_text(button['label']); require(bool(button['label']))
    for action, button in (('open_funding', 'funding'), ('open_bases', 'bases'), ('open_pause_menu', 'options')):
        require(action not in actions or button in seen)
    speeds = visible['speeds']
    require(type(speeds) is list and len(speeds) <= len(GEOSCAPE_SPEED_HANDLES))
    selected = []
    for index, speed in enumerate(speeds):
        exact(speed, 'handle label selected')
        require(speed['handle'] == GEOSCAPE_SPEED_HANDLES[index])
        strategy_text(speed['label']); require(bool(speed['label']))
        require(type(speed['selected']) is bool)
        if speed['selected']: selected.append(speed['label'])
    require(len(selected) <= 1 and visible['speed'] == (selected[0] if selected else None))
    require('choose_geoscape_speed' not in actions or bool(speeds))


def validate_base_navigation(screen, visible, actions):
    fields = ('base_name location funds base_slots buttons' if screen == 'basescape' else
              'base_name base_slots personnel_heading space_heading metrics buttons')
    exact(visible, fields)
    for name in (('base_name', 'location', 'funds') if screen == 'basescape' else
                 ('base_name', 'personnel_heading', 'space_heading')):
        strategy_text(visible[name], nullable=True)
    slots = visible['base_slots']
    require(type(slots) is list and len(slots) <= len(BASE_TARGET_HANDLES))
    selected = 0
    for index, slot in enumerate(slots):
        exact(slot, 'handle selected')
        require(slot['handle'] == BASE_TARGET_HANDLES[index] and type(slot['selected']) is bool)
        selected += slot['selected']
    require(selected <= 1 and ('select_base' not in actions or bool(slots)))
    buttons = visible['buttons']
    require(type(buttons) is list and len(buttons) <= len(BASE_BUTTONS[screen]))
    previous, seen = -1, set()
    for button in buttons:
        exact(button, 'id label')
        require(type(button['id']) is str and button['id'] in BASE_BUTTONS[screen])
        index = BASE_BUTTONS[screen].index(button['id'])
        require(index > previous); previous = index; seen.add(button['id'])
        strategy_text(button['label']); require(bool(button['label']))
    for action, button in BASE_BUTTON_ACTIONS[screen].items():
        require(action not in actions or button in seen)
    if screen == 'base_info':
        metrics = visible['metrics']
        require(type(metrics) is list and len(metrics) <= 12)
        for metric in metrics:
            exact(metric, 'label value')
            strategy_text(metric['label']); require(bool(metric['label']))
            strategy_text(metric['value'], nullable=True)


def validate_projects(screen, visible, actions):
    if screen in PROJECT_LISTS:
        exact(visible, 'title summary headings rows scroll buttons')
        summary_count, columns = PROJECT_LISTS[screen]
        require(type(visible['summary']) is list and len(visible['summary']) == summary_count)
        for value in visible['summary']: strategy_text(value, nullable=True)
        fields = ('title',)
    elif screen == 'research_info':
        exact(visible, 'title available space allocated buttons')
        fields = ('title', 'available', 'space', 'allocated')
    elif screen == 'manufacture_start':
        exact(visible, 'title engineer_hours cost space materials_heading headings rows scroll buttons')
        fields = ('title', 'engineer_hours', 'cost', 'space', 'materials_heading')
        columns = 3
    else:
        exact(visible, 'title available space allocated units monthly_profit buttons sell quantity_controls')
        fields = ('title', 'available', 'space', 'allocated', 'units', 'monthly_profit')
    for field in fields: strategy_text(visible[field], nullable=True)
    if screen in PROJECT_LISTS or screen == 'manufacture_start':
        require(type(visible['headings']) is list and len(visible['headings']) == columns)
        for value in visible['headings']: strategy_text(value, nullable=True)
        rows = visible['rows']; require(type(rows) is list and len(rows) <= len(ROW_HANDLES))
        selected = 0
        for index, row in enumerate(rows):
            exact(row, 'handle cells selected')
            require(row['handle'] == ROW_HANDLES[index])
            require(type(row['cells']) is list and len(row['cells']) == columns)
            for value in row['cells']: strategy_text(value, nullable=screen == 'new_research')
            require(type(row['selected']) is bool); selected += row['selected']
        require(selected == 0 if screen == 'manufacture_start' else selected <= 1)
        exact(visible['scroll'], 'up down')
        for value in visible['scroll'].values(): require(type(value) is bool)
        require('scroll_visible_list' not in actions or any(visible['scroll'].values()))
        selection = PROJECT_SELECTION_ACTIONS.get(screen)
        require(selection not in actions or (visible['title'] is not None and bool(rows) and
                all((screen == 'new_research' and row['cells'][0] is None) or row['cells'][0] for row in rows)))
    buttons = visible['buttons']; require(type(buttons) is list and len(buttons) <= len(PROJECT_BUTTONS[screen]))
    previous, seen = -1, set()
    for button in buttons:
        exact(button, 'id label')
        require(type(button['id']) is str and button['id'] in PROJECT_BUTTONS[screen])
        index = PROJECT_BUTTONS[screen].index(button['id']); require(index > previous); previous = index
        strategy_text(button['label']); require(bool(button['label'])); seen.add(button['id'])
    for action, button in PROJECT_BUTTON_ACTIONS[screen].items(): require(action not in actions or button in seen)
    if screen == 'manufacture_info': validate_quantity_controls(visible, actions)
    staff = PROJECT_STAFF_ACTIONS.get(screen)
    require(staff not in actions or (visible['title'] is not None and visible['allocated'] is not None and bool(seen.intersection(STAFF_DIRECTIONS))))


def ordered_subset(values, allowed, *, nonempty=False):
    require(type(values) is list and len(values) <= len(allowed) and (bool(values) or not nonempty))
    previous = -1
    for value in values:
        require(type(value) is str and value in allowed)
        index = allowed.index(value); require(index > previous); previous = index


def validate_quantity_controls(visible, actions):
    controls = visible['quantity_controls']
    require(type(controls) is list and len(controls) <= len(QUANTITY_CONTROLS))
    ordered_subset([entry.get('control') if type(entry) is dict else None for entry in controls], QUANTITY_CONTROLS)
    by_control = {}
    for entry in controls:
        exact(entry, 'control increase decrease')
        require(type(entry['increase']) is bool and type(entry['decrease']) is bool)
        require(entry['increase'] or entry['decrease'])
        by_control[entry['control']] = entry
    buttons = {entry['id'] for entry in visible['buttons']}
    engineers = by_control.get('engineers', {})
    for direction in STAFF_DIRECTIONS: require(engineers.get(direction, False) == (direction in buttons))
    sell = visible['sell']
    if sell is not None:
        exact(sell, 'label pressed'); strategy_text(sell['label']); require(bool(sell['label']))
        require(type(sell['pressed']) is bool)
    require('change_production_quantity' not in actions or bool(controls))
    require('set_production_sell' not in actions or sell is not None)


TRANSFER_BUTTON_ACTIONS = {
    'transfer_destination': {'cancel_transfer_destination': 'cancel'},
    'transfer_items': {'review_transfer': 'review', 'cancel_transfer_items': 'cancel'},
    'transfer_confirm': {'confirm_transfer': 'confirm', 'cancel_transfer_confirmation': 'cancel'}}


def validate_transfer(screen, visible, actions):
    mapping = TRANSFER_BUTTON_ACTIONS[screen]
    if screen == 'transfer_confirm':
        exact(visible, 'title cost total buttons')
        for key in ('title', 'cost', 'total'): strategy_text(visible[key], nullable=True)
    else:
        destination = screen == 'transfer_destination'
        exact(visible, 'title funds headings rows scroll buttons' if destination else 'title headings rows scroll buttons')
        strategy_text(visible['title'], nullable=True)
        if destination: strategy_text(visible['funds'], nullable=True)
        columns = 2 if destination else 4
        require(type(visible['headings']) is list and len(visible['headings']) == columns)
        for value in visible['headings']: strategy_text(value, nullable=True)
        rows = visible['rows']; require(type(rows) is list and len(rows) <= len(ROW_HANDLES))
        selected = 0
        for index, row in enumerate(rows):
            exact(row, 'handle cells selected' if destination else 'handle cells selected controls')
            require(row['handle'] == ROW_HANDLES[index])
            require(type(row['cells']) is list and len(row['cells']) == columns)
            for value in row['cells']: strategy_text(value, nullable=True)
            require(type(row['selected']) is bool); selected += row['selected']
            if not destination: ordered_subset(row['controls'], STAFF_DIRECTIONS, nonempty=True)
        require(selected <= 1)
        exact(visible['scroll'], 'up down')
        for value in visible['scroll'].values(): require(type(value) is bool)
        require('scroll_visible_list' not in actions or any(visible['scroll'].values()))
        require('choose_visible_row' not in actions or bool(rows))
        require('change_transfer_quantity' not in actions or bool(rows))
    buttons = visible['buttons']; require(type(buttons) is list and len(buttons) <= len(mapping))
    ordered_subset([entry.get('id') if type(entry) is dict else None for entry in buttons], tuple(mapping.values()))
    for button in buttons:
        exact(button, 'id label'); strategy_text(button['label']); require(bool(button['label']))
    shown = {button['id'] for button in buttons}
    for action, button in mapping.items(): require(action not in actions or button in shown)


def validate_purchase(visible, actions):
    exact(visible, 'title funds total space_used headings category rows scroll scrollbar buttons')
    for key in ('title', 'funds', 'total', 'space_used'): strategy_text(visible[key], nullable=True)
    require(type(visible['headings']) is list and len(visible['headings']) == 2)
    for value in visible['headings']: strategy_text(value, nullable=True)
    if visible['category'] is not None:
        exact(visible['category'], 'label'); strategy_text(visible['category']['label']); require(bool(visible['category']['label']))
    rows = visible['rows']; require(type(rows) is list and len(rows) <= len(ROW_HANDLES))
    selected = 0
    for index, row in enumerate(rows):
        exact(row, 'handle cells selected controls'); require(row['handle'] == ROW_HANDLES[index])
        require(type(row['cells']) is list and len(row['cells']) == 4)
        for value in row['cells']: strategy_text(value, nullable=True)
        require(type(row['selected']) is bool); selected += row['selected']
        ordered_subset(row['controls'], STAFF_DIRECTIONS, nonempty=True)
    require(selected <= 1)
    exact(visible['scroll'], 'up down')
    for value in visible['scroll'].values(): require(type(value) is bool)
    scrollbar = visible['scrollbar']
    if scrollbar is not None:
        exact(scrollbar, 'targets')
        targets = scrollbar['targets']; require(type(targets) is list and 0 < len(targets) <= 3)
        positions = []
        for index, target in enumerate(targets):
            exact(target, 'handle position'); require(target['handle'] == PURCHASE_SCROLL_HANDLES[index])
            positions.append(target['position'])
        ordered_subset(positions, PURCHASE_POSITIONS, nonempty=True)
    buttons = visible['buttons']; require(type(buttons) is list and len(buttons) <= 2)
    ordered_subset([entry.get('id') if type(entry) is dict else None for entry in buttons], ('accept', 'cancel'))
    for button in buttons:
        exact(button, 'id label'); strategy_text(button['label']); require(bool(button['label']))
    shown = {button['id'] for button in buttons}
    require('change_purchase_quantity' not in actions or bool(rows))
    require('open_purchase_category' not in actions or visible['category'] is not None)
    require('position_purchase_list' not in actions or scrollbar is not None)
    require('scroll_purchase' not in actions or any(visible['scroll'].values()))
    for action, button in (('accept_purchase', 'accept'), ('cancel_purchase', 'cancel')):
        require(action not in actions or button in shown)



def validate_outfitting(screen, visible, actions):
    """Explicit personnel/control opt-ins; unknown text never supplies identity."""
    exact(visible, OUTFITTING_FIELDS[screen])
    for field in OUTFITTING_FIELDS[screen].split():
        if field not in ('headings', 'rows', 'scroll', 'buttons', 'stats', 'weapons'):
            strategy_text(visible[field], nullable=True)
    if screen in OUTFITTING_LISTS:
        headings, columns = OUTFITTING_LISTS[screen]
        require(type(visible['headings']) is list and len(visible['headings']) == headings)
        for text in visible['headings']: strategy_text(text, nullable=True)
        rows = visible['rows']; require(type(rows) is list and len(rows) <= len(ROW_HANDLES))
        selected = 0
        for index, row in enumerate(rows):
            exact(row, 'handle cells selected controls' if screen == 'craft_equipment' else 'handle cells selected')
            require(row['handle'] == ROW_HANDLES[index])
            require(type(row['cells']) is list and len(row['cells']) == columns)
            for text in row['cells']: strategy_text(text, nullable=True)
            require(type(row['selected']) is bool); selected += row['selected']
            if screen == 'craft_equipment': ordered_subset(row['controls'], STAFF_DIRECTIONS, nonempty=True)
        require(selected <= 1)
        exact(visible['scroll'], 'up down')
        for flag in visible['scroll'].values(): require(type(flag) is bool)
        require('scroll_visible_list' not in actions or any(visible['scroll'].values()))
        require(not set(OUTFITTING_ROW_ACTIONS[screen]).intersection(actions) or bool(rows))
    elif screen == 'soldier_info':
        stats = visible['stats']; require(type(stats) is list and len(stats) <= 11)
        for stat in stats:
            exact(stat, 'label value')
            strategy_text(stat['label'], nullable=True); strategy_text(stat['value'], nullable=True)
            require(stat['label'] is not None or stat['value'] is not None)
    else:
        weapons = visible['weapons']; require(type(weapons) is list and len(weapons) == 2)
        for weapon in weapons:
            exact(weapon, 'name ammo')
            strategy_text(weapon['name'], nullable=True); strategy_text(weapon['ammo'], nullable=True)
    buttons = visible['buttons']
    require(type(buttons) is list and len(buttons) <= len(OUTFITTING_BUTTONS[screen]))
    ordered_subset([button.get('id') if type(button) is dict else None for button in buttons], OUTFITTING_BUTTONS[screen])
    for button in buttons:
        exact(button, 'id label'); strategy_text(button['label']); require(bool(button['label']))
    shown = {button['id'] for button in buttons}
    for action, button in OUTFITTING_BUTTON_ACTIONS[screen].items():
        require(action not in actions or button in shown)


def validate_research_completion(screen, visible, actions):
    """Shown completion controls and separately proved article lines only."""
    exact(visible, RESEARCH_COMPLETION_FIELDS[screen])
    for field in RESEARCH_COMPLETION_FIELDS[screen].split():
        if field not in ('rows', 'scroll', 'buttons', 'body_lines', 'details_status'):
            strategy_text(visible[field], nullable=True)
    if screen in RESEARCH_NOTICE_STATES:
        rows = visible['rows']; require(type(rows) is list and len(rows) <= len(ROW_HANDLES))
        for index, row in enumerate(rows):
            exact(row, 'handle cells selected')
            require(row['handle'] == ROW_HANDLES[index])
            require(type(row['cells']) is list and len(row['cells']) == 1)
            strategy_text(row['cells'][0]); require(row['selected'] is False)
        exact(visible['scroll'], 'up down')
        for flag in visible['scroll'].values(): require(type(flag) is bool)
        require('scroll_visible_list' not in actions or any(visible['scroll'].values()))
    elif screen == 'ufopaedia_article':
        require(visible['details_status'] == 'unavailable')
        lines = visible['body_lines']
        if lines is not None:
            require(type(lines) is list and len(lines) <= 32)
            total = 0
            for line in lines:
                if line is None: continue
                strategy_text(line); require(bool(line))
                require(all(ord(char) >= 32 and ord(char) != 127 for char in line))
                require(not line.startswith(' ') and not line.endswith(' ') and '  ' not in line)
                total += len(line.encode('utf-8'))
            require(total <= 8192)
    buttons = visible['buttons']
    require(type(buttons) is list and len(buttons) <= len(RESEARCH_COMPLETION_BUTTONS[screen]))
    ordered_subset([button.get('id') if type(button) is dict else None for button in buttons], RESEARCH_COMPLETION_BUTTONS[screen])
    for button in buttons:
        exact(button, 'id label'); strategy_text(button['label']); require(bool(button['label']))
    shown = {button['id'] for button in buttons}
    for action, button in RESEARCH_COMPLETION_BUTTON_ACTIONS[screen].items():
        require(action not in actions or button in shown)


def validate_arrival_table(visible, actions):
    # Only Items Arriving opts into independent unknown cells. Other table
    # schemas retain complete strings; null never supplies text or identity.
    strategy_text(visible['title'], nullable=True)
    require(type(visible['headings']) is list and len(visible['headings']) == 3)
    for value in visible['headings']: strategy_text(value, nullable=True)
    rows = visible['rows']
    require(type(rows) is list and len(rows) <= len(ROW_HANDLES))
    for index, row in enumerate(rows):
        exact(row, 'handle cells selected')
        require(row['handle'] == ROW_HANDLES[index])
        require(type(row['cells']) is list and len(row['cells']) == 3)
        for value in row['cells']: strategy_text(value, nullable=True)
        require(any(type(value) is str and bool(value) for value in row['cells']))
        require(row['selected'] is False)
    exact(visible['scroll'], 'up down')
    for value in visible['scroll'].values(): require(type(value) is bool)
    require('scroll_visible_list' not in actions or any(visible['scroll'].values()))


def validate_completion(screen, visible, actions):
    if screen == 'items_arriving':
        exact(visible, 'title headings rows scroll buttons')
        validate_arrival_table(visible, actions)
    elif screen == 'production_complete':
        exact(visible, 'message buttons')
        strategy_text(visible['message'], nullable=True)
    else:
        exact(visible, 'title detected_label hyperwave_label details_status buttons')
        for key in ('title', 'detected_label', 'hyperwave_label'):
            strategy_text(visible[key], nullable=True)
        require(visible['details_status'] == 'unavailable')
    buttons = visible['buttons']
    require(type(buttons) is list and len(buttons) <= len(COMPLETION_BUTTONS[screen]))
    ordered_subset([button.get('id') if type(button) is dict else None for button in buttons], COMPLETION_BUTTONS[screen])
    for button in buttons:
        exact(button, 'id label')
        strategy_text(button['label']); require(bool(button['label']))
    shown = {button['id'] for button in buttons}
    for action, button in COMPLETION_BUTTON_ACTIONS[screen].items():
        require(action not in actions or button in shown)


def validate_base_table(visible, actions):
    # Read-only use of the existing rendered multi-column projection. An empty
    # admitted slice is not a claim that the underlying list is empty.
    validate_visible_list(visible, actions, funding=True)
    strategy_text(visible['title'], nullable=True)
    for value in visible['headings']: strategy_text(value, nullable=True)
    for row in visible['rows']:
        for value in row['cells']: strategy_text(value)


def validate_visible_list(visible, actions, *, funding=False, purchase_category=False):
    """Only fully rendered, grant-local rows; no backing indices or totals."""
    exact(visible, 'title headings rows scroll' if funding else 'control label rows scroll')
    if funding:
        visible_text(visible['title'], nullable=True)
        require(type(visible['headings']) is list and len(visible['headings']) == 3)
        for heading in visible['headings']:
            visible_text(heading, nullable=True)
    else:
        require(type(visible['control']) is str and visible['control'] in (('category',) if purchase_category else CHOICE_CONTROLS))
        (strategy_text if purchase_category else visible_text)(visible['label'], nullable=True)
    rows = visible['rows']
    require(type(rows) is list and len(rows) <= len(ROW_HANDLES))
    selected = 0
    for index, row in enumerate(rows):
        exact(row, 'handle cells selected')
        require(row['handle'] == ROW_HANDLES[index])
        require(type(row['cells']) is list and len(row['cells']) == (3 if funding else 1))
        for cell in row['cells']:
            (strategy_text if purchase_category else visible_text)(cell)
            require(funding or bool(cell))
        require(type(row['selected']) is bool)
        selected += row['selected']
    require(selected == 0 if funding else selected <= 1)
    exact(visible['scroll'], 'up down')
    for value in visible['scroll'].values():
        require(type(value) is bool)
    require('choose_visible_row' not in actions or bool(rows))
    require('scroll_visible_list' not in actions or any(visible['scroll'].values()))


def validate_build_base(visible, actions):
    exact(visible, 'title globe')
    visible_text(visible['title'], nullable=True)
    globe = visible['globe']
    if globe is None:
        require('place_base_site' not in actions)
        return
    exact(globe, 'width height sample_step samples')
    integer(globe['width'], 1, 512)
    integer(globe['height'], 1, 400)
    integer(globe['sample_step'], 16, 16)
    require(type(globe['samples']) is list and len(globe['samples']) <= 800)
    seen = set()
    for sample in globe['samples']:
        exact(sample, 'x y terrain')
        integer(sample['x'], 0, globe['width'] - 1)
        integer(sample['y'], 0, globe['height'] - 1)
        coordinate = (sample['x'], sample['y'])
        require(coordinate not in seen)
        seen.add(coordinate)
        require(type(sample['terrain']) is str and sample['terrain'] in ('textured', 'untextured'))
    require('place_base_site' not in actions or bool(seen))


def validate_tactical(tactical):
    """Bounded public drawing geometry, never map indices or actor identities."""
    exact(tactical, 'mode viewport message view_level grid fragment_status fragments truncated')
    require(type(tactical['mode']) is str and tactical['mode'] in ('map', 'hidden_movement', 'unavailable'))
    require(type(tactical['fragment_status']) is str and tactical['fragment_status'] in ('presented', 'unavailable'))
    require(tactical['view_level'] is None and type(tactical['truncated']) is bool)
    visible_text(tactical['message'], nullable=True)
    viewport = tactical['viewport']
    if viewport is not None:
        exact(viewport, 'x y width height')
        for axis in ('x', 'y'): integer(viewport[axis], 0, 4095)
        integer(viewport['width'], 1, 640); integer(viewport['height'], 1, 400)
    grid, fragments = tactical['grid'], tactical['fragments']
    require(type(grid) is list and len(grid) <= 128 and type(fragments) is list and len(fragments) <= 128)
    if tactical['mode'] != 'map':
        require(not grid and not fragments)
        if tactical['mode'] == 'unavailable':
            require(viewport is None and tactical['message'] is None)
        return
    require(viewport is not None and tactical['message'] is None)
    def point(value):
        exact(value, 'x y')
        integer(value['x'], 0, viewport['width'] - 1)
        integer(value['y'], 0, viewport['height'] - 1)
    for index, cell in enumerate(grid):
        exact(cell, 'handle polygon target')
        require(cell['handle'] == f'g{index}' and type(cell['polygon']) is list and len(cell['polygon']) == 4)
        for vertex in cell['polygon']: point(vertex)
        point(cell['target'])
    require(tactical['fragment_status'] != 'unavailable' or not fragments)
    count = 0
    for index, fragment in enumerate(fragments):
        exact(fragment, 'handle kind classification runs')
        require(fragment['handle'] == f'p{index}' and fragment['kind'] == 'painted_fragment'
                and fragment['classification'] is None)
        require(type(fragment['runs']) is list and 0 < len(fragment['runs']) <= 512)
        count += len(fragment['runs']); require(count <= 512)
        for run in fragment['runs']:
            exact(run, 'x y width')
            integer(run['x'], 0, viewport['width'] - 1)
            integer(run['y'], 0, viewport['height'] - 1)
            integer(run['width'], 1, viewport['width'] - run['x'])


def validate_battlescape_controls(visible, actions):
    if visible['layers_indicator'] is not None:
        integer(visible['layers_indicator'], 1, 2)
    hands = visible['hands']
    require(type(hands) is list and len(hands) <= len(HANDS))
    seen_hands = set()
    for hand in hands:
        exact(hand, 'hand ammo')
        require(type(hand['hand']) is str and hand['hand'] in HANDS and hand['hand'] not in seen_hands)
        seen_hands.add(hand['hand'])
        if hand['ammo'] is not None:
            integer(hand['ammo'], 0, 2**32 - 1)
    buttons = visible['enemy_buttons']
    require(type(buttons) is list and len(buttons) <= len(ENEMY_BUTTON_HANDLES))
    numbers = set()
    for index, button in enumerate(buttons):
        exact(button, 'handle number')
        require(button['handle'] == ENEMY_BUTTON_HANDLES[index])
        integer(button['number'], 1, 10)
        require(button['number'] not in numbers)
        numbers.add(button['number'])
    tactical = visible['tactical']
    cells = {cell['handle'] for cell in tactical['grid']}
    focus = visible['focus_target']
    require(focus is None or (type(focus) is str and tactical['mode'] == 'map' and focus in cells))
    require(not {'map_primary', 'map_secondary', 'camera_pan'}.intersection(actions) or (tactical['mode'] == 'map' and bool(cells)))
    require('center_enemy' not in actions or bool(buttons))
    require('open_hand_action_menu' not in actions or bool(hands))


def validate_minimap(visible, actions):
    """Painted local geometry only; no map/model identity or legal-target claim."""
    exact(visible, 'level_label viewport terrain markers targets content_status truncated')
    visible_text(visible['level_label'], nullable=True)
    require(visible['level_label'] is None or len(visible['level_label'].encode('utf-8')) <= 1024)
    require(type(visible['content_status']) is str and visible['content_status'] in ('bounded', 'unavailable'))
    require(type(visible['truncated']) is bool)
    viewport = visible['viewport']
    if viewport is not None:
        exact(viewport, 'x y width height')
        integer(viewport['x'], 0, 639); integer(viewport['y'], 0, 399)
        integer(viewport['width'], 1, 640 - viewport['x'])
        integer(viewport['height'], 1, 400 - viewport['y'])
    terrain, markers, targets = visible['terrain'], visible['markers'], visible['targets']
    require(type(terrain) is list and len(terrain) <= 512)
    require(type(markers) is list and len(markers) <= 128)
    require(type(targets) is list and len(targets) <= len(MINIMAP_TARGET_HANDLES))
    if visible['content_status'] == 'unavailable':
        require(not terrain and not markers and not targets and not visible['truncated'])
    else:
        require(viewport is not None and bool(terrain or markers))
    def rectangle(x, y, width, height):
        require(viewport is not None)
        integer(x, 0, viewport['width'] - 1); integer(y, 0, viewport['height'] - 1)
        integer(width, 1, viewport['width'] - x); integer(height, 1, viewport['height'] - y)
    previous, active = None, []
    for rect in terrain:
        require(type(rect) is list and len(rect) == 4)
        x, y, width, height = rect
        rectangle(x, y, width, height)
        key = (y, x, width, height)
        require(previous is None or previous < key)
        previous = key
        # Canonical y order makes expired rectangles irrelevant. Different
        # terrain rectangles may touch, but must not overlap even one pixel.
        active = [prior for prior in active if prior[3] > y]
        require(all(x >= right or x + width <= left for left, _, right, _ in active))
        active.append((x, y, x + width, y + height))
    previous = None
    for marker in markers:
        exact(marker, 'kind x y width height')
        require(type(marker['kind']) is str and marker['kind'] in ('unit_marker', 'item_marker'))
        rectangle(marker['x'], marker['y'], marker['width'], marker['height'])
        integer(marker['width'], 4, 4); integer(marker['height'], 4, 4)
        key = (marker['y'], marker['x'], marker['kind'])
        require(previous is None or previous < key)
        previous = key
    points = set()
    for index, target in enumerate(targets):
        exact(target, 'handle x y')
        require(target['handle'] == MINIMAP_TARGET_HANDLES[index] and viewport is not None)
        integer(target['x'], 0, viewport['width'] - 1)
        integer(target['y'], 0, viewport['height'] - 1)
        point = (target['x'], target['y'])
        require(point not in points); points.add(point)
    require('minimap_center' not in actions or (visible['content_status'] == 'bounded' and bool(targets)))


def validate_weapon_action_menu(visible, actions):
    exact(visible, 'rows')
    rows = visible['rows']
    require(type(rows) is list and len(rows) <= len(WEAPON_ACTION_HANDLES))
    for index, row in enumerate(rows):
        exact(row, 'handle label accuracy time_units')
        require(row['handle'] == WEAPON_ACTION_HANDLES[index])
        visible_text(row['label']); require(bool(row['label']))
        visible_text(row['accuracy'], nullable=True)
        visible_text(row['time_units'], nullable=True)
    require('choose_weapon_action' not in actions or bool(rows))


def validate_prime_grenade(visible, actions):
    exact(visible, 'title timers')
    visible_text(visible['title'], nullable=True)
    require(visible['title'] is None or len(visible['title'].encode('utf-8')) <= 1024)
    timers = visible['timers']
    require(type(timers) is list and len(timers) <= len(GRENADE_TIMER_HANDLES))
    for index, timer in enumerate(timers):
        exact(timer, 'handle label')
        require(timer['handle'] == GRENADE_TIMER_HANDLES[index])
        visible_text(timer['label'])
        require(bool(timer['label']) and len(timer['label'].encode('utf-8')) <= 1024)
    require('choose_grenade_timer' not in actions or bool(timers))


def validate_inventory(visible, actions):
    exact(visible, 'close_label ' + ' '.join(INVENTORY_TEXT_FIELDS) + ' held targets')
    require(visible['close_label'] == 'Close inventory')
    def text(value):
        visible_text(value, nullable=True)
        require(value is None or len(value.encode('utf-8')) <= 1024)
    for field in INVENTORY_TEXT_FIELDS: text(visible[field])
    def rectangle(value):
        integer(value['x'], 0, 639); integer(value['y'], 0, 399)
        integer(value['width'], 1, 640 - value['x'])
        integer(value['height'], 1, 400 - value['y'])
    held = visible['held']
    if held is not None:
        exact(held, 'status x y width height')
        require(held['status'] == 'presented')
        rectangle(held)
    targets = visible['targets']
    require(type(targets) is list and len(targets) <= len(INVENTORY_TARGET_HANDLES))
    for index, target in enumerate(targets):
        exact(target, 'handle area label x y width height point item')
        require(target['handle'] == INVENTORY_TARGET_HANDLES[index])
        require(type(target['area']) is str and target['area'] in ('hand', 'ground', 'body'))
        text(target['label']); rectangle(target)
        exact(target['point'], 'x y')
        integer(target['point']['x'], target['x'], target['x'] + target['width'] - 1)
        integer(target['point']['y'], target['y'], target['y'] + target['height'] - 1)
        require(type(target['item']) is str and target['item'] in ('presented', 'unknown'))
    require(not {'inventory_hover', 'inventory_activate'}.intersection(actions) or bool(targets))
    require('cancel_inventory_hold' not in actions or held is not None)


def validate_visible(screen, visible, seen):
    require(type(screen) is str and screen in STATE_ACTIONS)
    if screen == 'battlescape':
        exact(visible, 'selected_name time_units energy health morale kneel_indicator tactical hands enemy_buttons focus_target layers_indicator')
        visible_text(visible['selected_name'], nullable=True)
        for name in ('time_units', 'energy', 'health', 'morale'):
            if visible[name] is not None:
                integer(visible[name], 0, 2**32 - 1)
        require(visible['kneel_indicator'] is None or
                (type(visible['kneel_indicator']) is str and visible['kneel_indicator'] in ('standing', 'kneeling')))
        validate_tactical(visible['tactical'])
        validate_battlescape_controls(visible, seen)
    elif screen == 'minimap':
        validate_minimap(visible, seen)
    elif screen == 'inventory':
        validate_inventory(visible, seen)
    elif screen == 'weapon_action_menu':
        validate_weapon_action_menu(visible, seen)
    elif screen == 'prime_grenade':
        validate_prime_grenade(visible, seen)
    elif screen == 'debriefing':
        exact(visible, 'title rating stats_label table_status')
        for key in ('title', 'rating', 'stats_label'):
            visible_text(visible[key], nullable=True)
            require(visible[key] is None or len(visible[key].encode('utf-8')) <= 1024)
        require(visible['table_status'] == 'unavailable')
    elif screen == 'new_game':
        validate_new_game(visible, seen)
    elif screen == 'geoscape':
        validate_geoscape(visible, seen)
    elif screen in ('basescape', 'base_info'):
        validate_base_navigation(screen, visible, seen)
    elif screen in PROJECT_BUTTONS:
        validate_projects(screen, visible, seen)
    elif screen in OUTFITTING_FIELDS:
        validate_outfitting(screen, visible, seen)
    elif screen in RESEARCH_COMPLETION_FIELDS:
        validate_research_completion(screen, visible, seen)
    elif screen in ('base_stores', 'base_transfers'):
        validate_base_table(visible, seen)
    elif screen in TRANSFER_BUTTON_ACTIONS:
        validate_transfer(screen, visible, seen)
    elif screen == 'purchase':
        validate_purchase(visible, seen)
    elif screen in COMPLETION_BUTTONS:
        validate_completion(screen, visible, seen)
    elif screen in ('new_battle_dropdown', 'funding', 'purchase_category'):
        validate_visible_list(visible, seen, funding=screen == 'funding', purchase_category=screen == 'purchase_category')
    elif screen == 'build_base':
        validate_build_base(visible, seen)
    elif screen in STATE_FIELDS:
        exact(visible, ' '.join(STATE_FIELDS[screen]))
        for text in visible.values(): visible_text(text, nullable=True)
        if screen == 'error_message':
            strategy_text(visible['heading'], nullable=True)
            require(visible['heading'] is None or (bool(visible['heading']) and '\x02' not in visible['heading']))
        if screen == 'new_battle':
            require('open_new_battle_choice' not in seen or any(value is not None for value in visible.values()))
    else:
        exact(visible, ''); require(not seen)


def validate_state(obj, challenge, epoch, *, post_action=False):
    exact(obj, 'schema mode type challenge epoch revision capability state visible actions event_sequence semantics')
    identity(obj)
    require(obj['type'] == 'state' and obj['semantics'] == 'presented_after_request')
    token(obj['epoch'])
    require(obj['epoch'] == epoch and obj['challenge'] == challenge)
    if challenge is not None:
        token(obj['challenge'])
    integer(obj['revision'], 0, MAX_COUNTER)
    integer(obj['event_sequence'], 0, MAX_COUNTER)
    require(type(obj['state']) is str and obj['state'] in STATE_ACTIONS)
    if obj['capability'] is not None:
        token(obj['capability'])
    if post_action or obj['state'] == 'unsupported':
        require(obj['capability'] is None)
    require(type(obj['actions']) is list and len(obj['actions']) <= len(ACTIONS))
    seen = set()
    for action in obj['actions']:
        exact(action, 'id label label_source argument_kind')
        require(type(action['id']) is str and action['id'] in ACTIONS and action['id'] not in seen)
        seen.add(action['id'])
        visible_text(action['label'])
        require(bool(action['label']))
        require(action['label_source'] == label_source(obj['state'], action['id']))
        require(action['argument_kind'] == ARGUMENT_KINDS[action['id']])
    require(not {'kneel', 'stand'}.issubset(seen) and seen <= STATE_ACTIONS[obj['state']])
    validate_visible(obj['state'], obj['visible'], seen)
    if obj['state'] == 'weapon_action_menu' and 'choose_weapon_action' in seen:
        descriptor = next(action for action in obj['actions'] if action['id'] == 'choose_weapon_action')
        require(descriptor['label'] == obj['visible']['rows'][0]['label'])
    if obj['state'] == 'prime_grenade':
        for descriptor in obj['actions']:
            expected = (obj['visible']['timers'][0]['label'] if descriptor['id'] == 'choose_grenade_timer'
                        else 'Cancel timer')
            require(descriptor['label'] == expected)
    if obj['state'] in ('geoscape', 'basescape', 'base_info', 'base_stores', 'base_transfers'):
        buttons = {button['id']: button['label'] for button in obj['visible'].get('buttons', [])}
        mapping = ({'open_funding': 'funding', 'open_bases': 'bases', 'open_pause_menu': 'options'}
                   if obj['state'] == 'geoscape' else BASE_BUTTON_ACTIONS.get(obj['state'], {}))
        for descriptor in obj['actions']:
            action = descriptor['id']
            strategy_text(descriptor['label'])
            if action in mapping: require(descriptor['label'] == buttons[mapping[action]])
            elif action == 'choose_geoscape_speed':
                require(descriptor['label'] == obj['visible']['speeds'][0]['label'])
            elif action == 'select_base': require(descriptor['label'] == 'Select base')
    if obj['state'] in PROJECT_BUTTONS:
        buttons = {button['id']: button['label'] for button in obj['visible']['buttons']}
        for descriptor in obj['actions']:
            action = descriptor['id']; strategy_text(descriptor['label'])
            if action in PROJECT_BUTTON_ACTIONS[obj['state']]:
                require(descriptor['label'] == buttons[PROJECT_BUTTON_ACTIONS[obj['state']][action]])
            elif action == PROJECT_SELECTION_ACTIONS.get(obj['state']):
                require(descriptor['label'] == ('Select research project' if obj['state'] == 'new_research'
                                                else obj['visible']['rows'][0]['cells'][0]))
            elif action == PROJECT_STAFF_ACTIONS.get(obj['state']):
                require(descriptor['label'] == next(buttons[key] for key in STAFF_DIRECTIONS if key in buttons))
    if obj['state'] in ('purchase', 'purchase_category', 'manufacture_info'):
        visible = obj['visible']
        buttons = {button['id']: button['label'] for button in visible.get('buttons', [])}
        for descriptor in obj['actions']:
            action = descriptor['id']; strategy_text(descriptor['label'])
            expected = QUANTITY_SEMANTIC_LABELS.get(action)
            if action == 'set_production_sell': expected = visible['sell']['label']
            elif action == 'open_purchase_category': expected = visible['category']['label']
            elif action in ('accept_purchase', 'cancel_purchase'): expected = buttons[action.split('_')[0]]
            elif action in ('choose_visible_row', 'scroll_visible_list', 'close_dropdown'):
                expected = {'choose_visible_row': 'Choose visible row', 'scroll_visible_list': 'Scroll visible list', 'close_dropdown': 'Close choices'}[action]
            if expected is not None: require(descriptor['label'] == expected)
    if obj['state'] in TRANSFER_BUTTON_ACTIONS:
        buttons = {button['id']: button['label'] for button in obj['visible']['buttons']}
        mapping = TRANSFER_BUTTON_ACTIONS[obj['state']]
        for descriptor in obj['actions']:
            action = descriptor['id']; strategy_text(descriptor['label'])
            expected = (QUANTITY_SEMANTIC_LABELS[action] if action in QUANTITY_SEMANTIC_LABELS else
                        'Choose visible row' if action == 'choose_visible_row' else
                        'Scroll visible list' if action == 'scroll_visible_list' else buttons[mapping[action]])
            require(descriptor['label'] == expected)
    if obj['state'] in COMPLETION_BUTTONS:
        labels = {button['id']: button['label'] for button in obj['visible']['buttons']}
        for descriptor in obj['actions']:
            strategy_text(descriptor['label'])
            expected = ('Scroll visible list' if descriptor['id'] == 'scroll_visible_list' else
                        labels[COMPLETION_BUTTON_ACTIONS[obj['state']][descriptor['id']]])
            require(descriptor['label'] == expected)
    if obj['state'] in OUTFITTING_FIELDS:
        buttons = {button['id']: button['label'] for button in obj['visible']['buttons']}
        mapping = OUTFITTING_BUTTON_ACTIONS[obj['state']]
        # A state/receipt carries the complete admitted catalog. Retained V
        # presentation validates visible only and does not mint this authority.
        for action, button in mapping.items(): require((action in seen) == (button in buttons))
        for descriptor in obj['actions']:
            action = descriptor['id']; strategy_text(descriptor['label'])
            expected = ('Scroll visible list' if action == 'scroll_visible_list' else
                        OUTFITTING_SEMANTIC_LABELS[action] if action in OUTFITTING_SEMANTIC_LABELS else buttons[mapping[action]])
            require(descriptor['label'] == expected)
    if obj['state'] in RESEARCH_COMPLETION_FIELDS:
        buttons = {button['id']: button['label'] for button in obj['visible']['buttons']}
        mapping = RESEARCH_COMPLETION_BUTTON_ACTIONS[obj['state']]
        for action, button in mapping.items(): require((action in seen) == (button in buttons))
        ordered_subset([descriptor['id'] for descriptor in obj['actions']], RESEARCH_COMPLETION_ACTION_ORDER[obj['state']])
        for descriptor in obj['actions']:
            action = descriptor['id']; strategy_text(descriptor['label'])
            expected = ('Scroll article text' if action == 'scroll_article_text' else
                        'Scroll visible list' if action == 'scroll_visible_list' else buttons[mapping[action]])
            require(descriptor['label'] == expected)
    # An empty/unsupported catalog never authorizes an action.
    if not seen:
        require(obj['capability'] is None)
    elif not post_action:
        require(obj['capability'] is not None)
    # The MCP output includes both text and structured JSON. Bound canonical
    # ASCII too, so Unicode/escape expansion cannot exceed the outer limit.
    limit = MAX_PACKET if obj['state'] == 'build_base' else MAX_TEXT_PACKET
    require(len(json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii')) <= limit)
    return obj


def validate_result(raw, command_id, epoch):
    obj = parse(raw)
    exact(obj, 'schema mode type command_id status event_sequence observation semantics')
    identity(obj)
    require(obj['type'] == 'result' and obj['semantics'] == 'dispatch_not_success')
    token(obj['command_id'])
    require(obj['command_id'] == command_id and type(obj['status']) is str and obj['status'] in STATUSES)
    integer(obj['event_sequence'], 0, MAX_COUNTER)
    if obj['observation'] is not None:
        validate_state(obj['observation'], None, epoch, post_action=True)
        require(obj['observation']['event_sequence'] == obj['event_sequence'])
        require(obj['status'] == 'dispatched')
    # Only the bounded point-sample projection needs the larger envelope.
    # Text-heavy states retain the tighter limit against escape expansion.
    limit = MAX_PACKET if obj['observation'] is not None and obj['observation']['state'] == 'build_base' else MAX_TEXT_PACKET
    require(len(json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii')) <= limit)
    return obj


def validate_event_arguments(arguments):
    exact(arguments, 'epoch after limit')
    token(arguments['epoch'])
    integer(arguments['after'], 0, MAX_COUNTER)
    integer(arguments['limit'], 1, MAX_EVENTS)


def displayed_message_report(text):
    """Classify only the exact bounded displayed phrase, never its game cause."""
    if len(text.encode('utf-8')) > 1024 or text.count('\n') != 1:
        return None
    prefix, suffix = text.split('\n')
    if not prefix or any(ord(c) < 32 or 127 <= ord(c) <= 159 for c in prefix):
        return None
    return {'is under alien control': 'alien_control',
            'is under Molecular Control': 'alien_control',
            'has Panicked': 'panic', 'has gone Berserk': 'berserk'}.get(suffix)


def validate_events(raw, challenge, epoch, after, limit, audio_history):
    obj = parse(raw)
    exact(obj, 'schema mode type challenge epoch audio_history audio_status after oldest newest next_cursor gap events')
    identity(obj)
    require(obj['type'] == 'events' and obj['challenge'] == challenge and obj['epoch'] == epoch)
    token(obj['challenge']); token(obj['epoch'])
    require(obj['audio_history'] == audio_history and audio_history in AUDIO_HISTORY)
    require(type(obj['audio_status']) is str and obj['audio_status'] in AUDIO_STATUSES)
    require(audio_history != 'unavailable' or obj['audio_status'] == 'unavailable')
    for key in ('after', 'newest', 'next_cursor'): integer(obj[key], 0, MAX_COUNTER)
    integer(obj['oldest'], 1, MAX_COUNTER)
    require(obj['after'] == after and after <= obj['newest'])
    require(obj['oldest'] == max(1, obj['newest'] - EVENT_CAPACITY + 1))
    require(type(obj['gap']) is bool and obj['gap'] == (after + 1 < obj['oldest']))
    events = obj['events']
    require(type(events) is list and len(events) <= limit)
    expected = max(after + 1, obj['oldest'])
    require(bool(events) == (expected <= obj['newest']))
    for event in events:
        require(type(event) is dict and type(event.get('kind')) is str)
        integer(event.get('seq'), 1, MAX_COUNTER)
        require(event['seq'] == expected and expected <= obj['newest']); expected += 1
        if event['kind'] == 'presentation':
            exact(event, 'seq kind state visible')
            validate_visible(event['state'], event['visible'], frozenset())
        elif event['kind'] == 'displayed_message':
            exact(event, 'seq kind text report source position')
            visible_text(event['text'])
            require(bool(event['text']) and len(event['text'].encode('utf-8')) <= 1024)
            require(event['source'] is None and event['position'] is None)
            expected_report = displayed_message_report(event['text'])
            require(event['report'] is None if expected_report is None else
                    type(event['report']) is str and event['report'] == expected_report)
        elif event['kind'] in ('projectile_presented', 'impact_effect_presented', 'collapse_animation_presented',
                              'thrown_item_presented', 'large_explosion_effect_presented'):
            exact(event, 'seq kind semantics source actor faction world_position hit damage')
            require(event['semantics'] == 'ordinary_renderer_visible_contribution')
            require(all(event[key] is None for key in
                        ('source', 'actor', 'faction', 'world_position', 'hit', 'damage')))
        elif event['kind'] == 'audio_output':
            exact(event, 'seq kind duration_ms nonzero source sound position')
            require(audio_history == 'mixed_output')
            integer(event['duration_ms'], 100, 100)
            require(type(event['nonzero']) is bool and event['source'] is None
                    and event['sound'] is None and event['position'] is None)
        elif event['kind'] == 'audio_status':
            exact(event, 'seq kind status partial_window_discarded')
            require(audio_history == 'mixed_output' and type(event['status']) is str
                    and event['status'] in AUDIO_STATUSES and type(event['partial_window_discarded']) is bool)
            require(event['status'] == 'unavailable' or not event['partial_window_discarded'])
        else:
            exact(event, 'seq kind dropped_windows')
            require(event['kind'] == 'audio_gap' and audio_history == 'mixed_output')
            integer(event['dropped_windows'], 1, MAX_COUNTER)
        require(len(json.dumps(event, ensure_ascii=True, separators=(',', ':')).encode('ascii')) <= MAX_TEXT_PACKET)
    require(obj['next_cursor'] == (events[-1]['seq'] if events else after))
    require(len(json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii')) <= MAX_PACKET)
    return obj


def validate_action_arguments(arguments):
    exact(arguments, 'command_id epoch capability revision action args')
    for name in ('command_id', 'epoch', 'capability'):
        token(arguments[name])
    integer(arguments['revision'], 0, MAX_COUNTER)
    require(type(arguments['action']) is str and arguments['action'] in ACTIONS)
    args = arguments['args']
    kind = ARGUMENT_KINDS[arguments['action']]
    if kind == 'none':
        exact(args, '')
    elif kind == 'difficulty':
        exact(args, 'value')
        require(type(args['value']) is str and args['value'] in DIFFICULTIES)
    elif kind == 'boolean':
        exact(args, 'value')
        require(type(args['value']) is bool)
    elif kind == 'boolean_value':
        exact(args, 'enabled'); require(type(args['enabled']) is bool)
    elif kind == 'crew_assignment':
        exact(args, 'row assigned')
        require(type(args['row']) is str and args['row'] in ROW_HANDLES)
        require(type(args['assigned']) is bool)
    elif kind in ('production_quantity', 'purchase_quantity', 'equipment_quantity', 'transfer_quantity'):
        key, domain = ('control', QUANTITY_CONTROLS) if kind == 'production_quantity' else ('row', ROW_HANDLES)
        require(type(args) is dict and type(args.get('operation')) is str and args['operation'] in QUANTITY_OPERATIONS)
        operation = args['operation']
        exact(args, key + ' operation' + (' value' if operation in ('adjust', 'set') else ''))
        require(type(args[key]) is str and args[key] in domain)
        if operation == 'adjust': integer(args['value'], -1000, 1000); require(args['value'] != 0)
        elif operation == 'set': integer(args['value'], 0, 100000)
    elif kind == 'purchase_scroll':
        exact(args, 'direction amount')
        require(type(args['direction']) is str and args['direction'] in SCROLL_DIRECTIONS)
        require(type(args['amount']) is str and args['amount'] in PURCHASE_AMOUNTS)
    elif kind == 'purchase_position':
        exact(args, 'target'); require(type(args['target']) is str and args['target'] in PURCHASE_SCROLL_HANDLES)
    elif kind == 'text':
        exact(args, 'text')
        require(type(args['text']) is str)
        try:
            require(len(args['text'].encode('utf-8')) <= MAX_NAME_BYTES)
        except UnicodeError:
            raise Unavailable('Unavailable') from None
        require(all(ord(c) >= 32 and not 127 <= ord(c) <= 159 for c in args['text']))
    elif kind == 'point':
        exact(args, 'x y')
        for coordinate in args.values():
            integer(coordinate, 0, 4095)
    else:
        field, values = {
            'choice_control': ('control', CHOICE_CONTROLS),
            'visible_row': ('row', ROW_HANDLES),
            'scroll_direction': ('direction', SCROLL_DIRECTIONS),
            'grid_cell': ('cell', GRID_HANDLES),
            'enemy_button': ('button', ENEMY_BUTTON_HANDLES),
            'hand': ('hand', HANDS),
            'weapon_action': ('row', WEAPON_ACTION_HANDLES),
            'grenade_timer': ('timer', GRENADE_TIMER_HANDLES),
            'inventory_target': ('target', INVENTORY_TARGET_HANDLES),
            'camera_direction': ('direction', CAMERA_DIRECTIONS),
            'minimap_target': ('target', MINIMAP_TARGET_HANDLES),
            'geoscape_speed': ('speed', GEOSCAPE_SPEED_HANDLES),
            'base_target': ('target', BASE_TARGET_HANDLES),
            'project_row': ('row', ROW_HANDLES),
            'outfitting_row': ('row', ROW_HANDLES),
            'staff_direction': ('direction', STAFF_DIRECTIONS),
        }[kind]
        exact(args, field)
        require(type(args[field]) is str and args[field] in values)


def canonical_action_request(arguments):
    """Compile the closed typed action into immutable, canonical ledger bytes."""
    validate_action_arguments(arguments)
    action, args = arguments['action'], arguments['args']
    kind = ARGUMENT_KINDS[action]
    suffix = ''
    if kind == 'difficulty':
        suffix = ' ' + args['value']
    elif kind == 'boolean':
        suffix = ' 1' if args['value'] else ' 0'
    elif kind == 'boolean_value':
        suffix = ' true' if args['enabled'] else ' false'
    elif kind == 'crew_assignment':
        suffix = ' ' + args['row'] + (' true' if args['assigned'] else ' false')
    elif kind in ('production_quantity', 'purchase_quantity', 'equipment_quantity', 'transfer_quantity'):
        suffix = ' ' + args['control' if kind == 'production_quantity' else 'row'] + ' ' + args['operation']
        if args['operation'] in ('adjust', 'set'): suffix += ' ' + str(args['value'])
    elif kind == 'purchase_scroll':
        suffix = ' ' + args['direction'] + ' ' + args['amount']
    elif kind == 'purchase_position':
        suffix = ' ' + args['target']
    elif kind == 'text':
        suffix = ' ' + (args['text'].encode('utf-8').hex() or '-')
    elif kind == 'point':
        suffix = f' {args["x"]} {args["y"]}'
    elif kind in ('choice_control', 'visible_row', 'scroll_direction', 'grid_cell', 'enemy_button', 'hand', 'weapon_action', 'grenade_timer', 'inventory_target', 'camera_direction', 'minimap_target', 'geoscape_speed', 'base_target', 'project_row', 'outfitting_row', 'staff_direction'):
        suffix = ' ' + args[{'choice_control': 'control', 'visible_row': 'row', 'scroll_direction': 'direction',
                                 'grid_cell': 'cell', 'enemy_button': 'button', 'hand': 'hand', 'weapon_action': 'row', 'grenade_timer': 'timer', 'inventory_target': 'target', 'camera_direction': 'direction', 'minimap_target': 'target', 'geoscape_speed': 'speed', 'base_target': 'target', 'project_row': 'row', 'outfitting_row': 'row', 'staff_direction': 'direction'}[kind]]
    request = ('E {command_id} {epoch} {capability} {revision} {action}'.format(**arguments) + suffix).encode('ascii')
    require(len(request) <= MAX_REQUEST)
    return request


def definitions():
    opaque = {'type': 'string', 'pattern': '^[0-9a-f]{32}$'}
    def arg_schema(properties):
        return {'type': 'object', 'properties': properties, 'required': list(properties), 'additionalProperties': False}
    argument_schemas = {
        'none': arg_schema({}),
        'difficulty': arg_schema({'value': {'type': 'string', 'enum': list(DIFFICULTIES)}}),
        'boolean': arg_schema({'value': {'type': 'boolean'}}),
        'text': arg_schema({'text': {'type': 'string', 'maxLength': MAX_NAME_BYTES,
                                     'not': {'pattern': '[\\u0000-\\u001f\\u007f-\\u009f]'}}}),
        'point': arg_schema({key: {'type': 'integer', 'minimum': 0, 'maximum': 4095} for key in ('x', 'y')}),
        'choice_control': arg_schema({'control': {'type': 'string', 'enum': list(CHOICE_CONTROLS)}}),
        'visible_row': arg_schema({'row': {'type': 'string', 'enum': list(ROW_HANDLES)}}),
        'scroll_direction': arg_schema({'direction': {'type': 'string', 'enum': list(SCROLL_DIRECTIONS)}}),
        'grid_cell': arg_schema({'cell': {'type': 'string', 'enum': list(GRID_HANDLES)}}),
        'enemy_button': arg_schema({'button': {'type': 'string', 'enum': list(ENEMY_BUTTON_HANDLES)}}),
        'hand': arg_schema({'hand': {'type': 'string', 'enum': list(HANDS)}}),
        'weapon_action': arg_schema({'row': {'type': 'string', 'enum': list(WEAPON_ACTION_HANDLES)}}),
        'grenade_timer': arg_schema({'timer': {'type': 'string', 'enum': list(GRENADE_TIMER_HANDLES)}}),
        'inventory_target': arg_schema({'target': {'type': 'string', 'enum': list(INVENTORY_TARGET_HANDLES)}}),
        'camera_direction': arg_schema({'direction': {'type': 'string', 'enum': list(CAMERA_DIRECTIONS)}}),
        'minimap_target': arg_schema({'target': {'type': 'string', 'enum': list(MINIMAP_TARGET_HANDLES)}}),
        'geoscape_speed': arg_schema({'speed': {'type': 'string', 'enum': list(GEOSCAPE_SPEED_HANDLES)}}),
        'base_target': arg_schema({'target': {'type': 'string', 'enum': list(BASE_TARGET_HANDLES)}}),
        'project_row': arg_schema({'row': {'type': 'string', 'enum': list(ROW_HANDLES)}}),
        'outfitting_row': arg_schema({'row': {'type': 'string', 'enum': list(ROW_HANDLES)}}),
        'crew_assignment': arg_schema({'row': {'type': 'string', 'enum': list(ROW_HANDLES)}, 'assigned': {'type': 'boolean'}}),
        'staff_direction': arg_schema({'direction': {'type': 'string', 'enum': list(STAFF_DIRECTIONS)}}),
    }
    for kind, key, domain in (('production_quantity', 'control', QUANTITY_CONTROLS), ('purchase_quantity', 'row', ROW_HANDLES), ('equipment_quantity', 'row', ROW_HANDLES), ('transfer_quantity', 'row', ROW_HANDLES)):
        branches = []
        for operation in QUANTITY_OPERATIONS:
            properties = {key: {'type': 'string', 'enum': list(domain)}, 'operation': {'type': 'string', 'enum': [operation]}}
            if operation == 'adjust': properties['value'] = {'type': 'integer', 'minimum': -1000, 'maximum': 1000, 'not': {'enum': [0]}}
            elif operation == 'set': properties['value'] = {'type': 'integer', 'minimum': 0, 'maximum': 100000}
            branches.append(arg_schema(properties))
        argument_schemas[kind] = {'oneOf': branches}
    argument_schemas.update(
        boolean_value=arg_schema({'enabled': {'type': 'boolean'}}),
        purchase_scroll=arg_schema({'direction': {'type': 'string', 'enum': list(SCROLL_DIRECTIONS)}, 'amount': {'type': 'string', 'enum': list(PURCHASE_AMOUNTS)}}),
        purchase_position=arg_schema({'target': {'type': 'string', 'enum': list(PURCHASE_SCROLL_HANDLES)}}))
    execute_schema = {
        'type': 'object', 'properties': {
            'command_id': dict(opaque), 'epoch': dict(opaque), 'capability': dict(opaque),
            'revision': {'type': 'integer', 'minimum': 0, 'maximum': MAX_COUNTER},
            'action': {'type': 'string', 'enum': list(ACTIONS)}, 'args': {'type': 'object'}},
        'required': ['command_id', 'epoch', 'capability', 'revision', 'action', 'args'], 'additionalProperties': False,
        'oneOf': [{'properties': {'action': {'enum': [action for action in ACTIONS if ARGUMENT_KINDS[action] == kind]},
                                  'args': schema}} for kind, schema in argument_schemas.items()]}
    return [
        {'name': 'read_ui_state',
         'description': 'Explicit agent-control protocol /24: read one fresh, bounded semantic UI presentation and the currently exposed visible UI action IDs; listing does not predict action success or hidden eligibility. Includes visible text/numbers, public UI revision and lifecycle event sequence; the read_public_events cursor is separate. Battlescape adds a camera-derived grid and bounded unclassified presented fragments only when available; HiddenMovement is explicit and suppresses geometry. Grid geometry is not known terrain, visibility or legal movement; no screenshot recognition, actor IDs or hidden world state. Campaign setup exposes visible difficulty choices and Ironman state, clipped drawn Globe geometry in its viewport, and the base-name field. New Battle exposes five bounded dropdowns with only fully rendered local rows; Funding exposes a read-only table and ordinary scroll/OK controls. Geoscape exposes shown time-speed controls, Funding, Bases and Pause. Base screens expose painted local selection slots, displayed metrics and read-only Stores/Transfers projections. Battlescape additionally exposes only current painted hand/ammo controls, numbered enemy HUD buttons and a camera-geometry focus grid handle. Weapon action menus expose bounded rendered Fire/Prime/Throw rows through choose_weapon_action; the tactical grenade timer exposes only fully shown button labels and fresh local tN handles; actual warning text has a separate read-only state and retained displayed-message event. These are not target identities or success predictions. Timed Infobox and Infobox OK expose only the actual displayed message; only a visible OK button can offer acknowledge_message. Debriefing exposes only complete shown title/rating/Stats label, an unavailable table, and the visible ordinary OK control. Inventory exposes the two painted hands, shown stock body cells and current painted ground page through local vN targets, actual shown text and held-sprite evidence; unknown item/held projection never asserts emptiness or eligibility. Standalone hover obtains ordinary rendered inspection text. Tactical viewport controls add ordinary view-level, layer-mode, camera pan and vertical-unit commands; layers_indicator1/2 is the painted display mode, never a world level. The minimap exposes its actual level label, bounded draw-derived terrain rectangles and generic unit/item markers with fresh local mN center targets; these reveal no identity, faction, world position, legal destination or item contents. Research and Manufacture expose bounded current/new project lists, actual displayed allocation/details and explicit ordinary accept/cancel/stop controls. New Research rows are in top-to-bottom admitted ordinary click-band order. A null first cell is an admitted row control with unavailable complete text, never a project name; select_research_project has the semantic label Select research project there. Other project tables retain complete string cells. Project labels and grant-local rows reveal no stable identity or hidden recipe; rendered counts remain text, without computed eligibility. Unsupported screens have an empty catalog. Manufacture additionally exposes shown quantity direction controls and Sell state. Purchase exposes bounded nullable displayed cells independently of shown quantity arrows, a category dropdown and local scroll-position targets. Empty or unknown text never establishes item identity, stock or eligibility. Strategic arrivals expose a painted read-only table with exactly three independently nullable cells per row and ordinary OK/Go To Base controls. A row retains at least one completely shown nonempty cell; null supplies no missing name, quantity or destination. Button authority is independent of full row text and grants no row selection or destination identity. Production completion exposes the shown message and ordinary destination label, without a hidden completion enum. UFO contact exposes nullable shown labels and independently admitted buttons; only Cancel is actionable and the contact detail table stays unavailable. Error messages add an independently proved nullable heading, never reconstruction of a clipped message. Personnel and transport outfitting expose only painted names, nullable stat/text cells and grant-local row controls. Unknown labels never establish soldier, craft, armor or item identity. Desired crew assignment and bounded equipment quantity commands use shared ordinary rules; listed controls do not predict capacity, wounds or stock. Equipment changes are immediate, armor choice applies immediately, and close only returns. No campaign save/load/overwrite, file rows, arbitrary settings or unlisted strategy actions are exposed. This replaces any previous one-use capability. After every action, read again before another action; transport uncertainty permanently closes this channel.',
         'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
         'annotations': {'readOnlyHint': True, 'destructiveHint': False, 'idempotentHint': False, 'openWorldHint': False}},
        {'name': 'execute_ui_action',
         'description': 'Directly dispatch one currently exposed semantic UI action in explicitly enabled agent-control mode. This is not human confirmation and does not approve supervised controls. Use the exact action ID, epoch, revision and single-use capability from a fresh read_ui_state, with a new command ID and the action-specific args object ({} for no-argument actions). New Battle control names, dropdown row handles and one-step scroll directions must come from the fresh visible projection; handles are local to that list and grant, never global indices or model IDs. Grid, minimap, inventory, enemy-button, hand, weapon-action and grenade-timer handles must come from the same fresh grant. Timer selection dispatch does not establish activation; require the later actual rendered warning. Throw target dispatch does not establish flight, landing, detonation or a hit. Timer cancel follows the ordinary popup cancel handler. Map primary/secondary and hand actions preserve normal context-sensitive behavior and can refuse or cancel instead of moving/firing/opening; read the resulting state and visible warning. Scroll flags describe visible arrows, so boundary no-ops remain possible. View-level controls may clamp at a boundary; vertical-unit controls may silently refuse. Camera pan is an ordinary directional step, and minimap_center uses only a fresh listed local point. Dispatch never establishes camera motion, level change, movement eligibility or unit movement. Funding, Stores and Transfers rows cannot be selected. Geoscape speed and base-slot handles are fresh local painted choices; they expose no base existence, persistent identity or eligibility. Ordinary speed controls advance real game time; opening Bases can reset the speed through its normal handler. Shown base labels and metric strings are not recomputed from hidden state. Empty admitted tables do not prove empty stores or transfers. Difficulty must be a currently exposed choice; Ironman takes a desired boolean, not a toggle. Base-name text is strict UTF-8, at most 128 bytes, without control characters; replacement does not confirm the name. Base-site x/y must exactly match a point in the freshly advertised Globe samples; unrestricted viewport points are deferred. Coordinates are viewport-local integers, never world coordinates or a general click. Start and name confirmation are separate explicit actions. The capability expires after 120 seconds and any UI revision change invalidates it sooner. Kneel/stand may spend time units and trigger reactions. end_turn dispatches the ordinary End Turn control and may start enemy activity; its receipt is not an enemy-turn success claim. Menu cancel uses the ordinary handler: New Battle can save its skirmish preset, and Options reloads persisted settings and restores volume/input capture. Inventory hover inspects through the ordinary shared hover path. Inventory activation atomically applies ordinary hover/held-anchor and primary activation on a fresh listed vN target. Shown item/held sprite evidence never predicts emptiness, compatibility or eligibility; null and unknown stay unknown. Cancel requires a shown held sprite; unload, page and close follow ordinary controls and may refuse or have no transition. Inspect the fresh rendered item/ammo/warning and held/target projection to establish postconditions. Select New Research using a freshly offered row handle, including an admitted unknown-label row; never infer its name from the handle or array order. Read the later complete detail title before claiming a shown project name. Selecting a new Research row immediately creates a project and can consume an item and ordinary RNG. accept_research retains the project/assignments; cancel_research_project removes it and follows ordinary unfinished-item/staff refund rules. start_production opens allocation and creates a Production. accept_production charges the first unit/materials for a new production, while existing-project acceptance does not charge again. stop_production removes it and releases assigned staff without refunding already spent costs. Only cancel_production_start is the ordinary side-effect-free detail return; there is no generic allocation cancel. adjust_scientists/adjust_engineers make one ordinary increase/decrease step from a currently shown direction, with ordinary clamping or refusal. Direct change_production_quantity and change_purchase_quantity apply bounded adjust, finite set, max or clear through shared ordinary semantic helpers. Positive adjust/max needs a shown increase control; negative adjust/clear needs decrease; set needs both. Ordinary clamping, resource limits and warnings still apply. max for units follows ordinary craft/noncraft behavior; clear means the ordinary minimum, which need not be zero. set_production_sell takes the desired boolean. Transfer changes remain a draft until confirm_transfer commits through the ordinary confirmation handler; review_transfer only opens confirmation, cancel_transfer_confirmation returns to the draft, and cancel_transfer_items discards it. Destination capacity refusals are ordinary game rules: a dispatch receipt does not establish shipment. change_transfer_quantity uses the same bounded operations and shown-direction requirements. Purchase changes remain a draft until accept_purchase commits ordinary costs/transfers; cancel_purchase discards the draft. scroll_purchase makes one semantic step/page/end and position_purchase_list uses a fresh advertised local target. None of these commands accepts pointer gestures, durations, holds or repeat counts. Fresh displayed rows/text are required to establish changes or postconditions. A receipt means handler dispatch, never success; inspect its separate post-action observation and obtain a fresh read before another action. Exact duplicates only query the receipt; conflicting command IDs fail. Never automatically retry after uncertainty. An adapter_error with status outcome_unknown means a mutating send was attempted but no trustworthy native receipt arrived; stop and reconcile manually.',
         'inputSchema': execute_schema,
         'annotations': {'readOnlyHint': False, 'destructiveHint': True, 'idempotentHint': True, 'openWorldHint': False}},
        {'name': 'read_ui_action_result',
         'description': 'Read a known command receipt without replaying it. The separate observation describes visible UI after dispatch; dispatched is not gameplay success. Public event_sequence is not a simulation counter. This tool never reconnects or retries an uncertain action.',
         'inputSchema': {'type': 'object', 'properties': {'command_id': dict(opaque)},
                         'required': ['command_id'], 'additionalProperties': False},
         'annotations': {'readOnlyHint': True, 'destructiveHint': False, 'idempotentHint': True, 'openWorldHint': False}},
        {'name': 'read_public_events',
         'description': 'Read retained public presentation, exact displayed-message reports, renderer projectile, impact-effect, collapse-animation, thrown-item and large-explosion-effect presence episodes and digital mixed-output history after a cursor, without consuming it or an action grant. First read_ui_state supplies the session epoch. The independent native ring retains256 public events; after is exclusive, limit is1..16, next_cursor advances through returned events and gap explicitly reports expired history. A byte-bounded page may contain fewer than limit. Identical cursor reads may include newer events; retained records are immutable. Presentation events contain only the same bounded visible DTO, no action authority. displayed_message contains complete shown text and an exact compiled phrase report or null; source and position stay null, and no hidden cause or actor identity is inferred. projectile_presented, impact_effect_presented, collapse_animation_presented, thrown_item_presented and large_explosion_effect_presented mean the corresponding ordinary renderer contribution survived an admitted presentation; source, actor, faction, world_position, hit and damage remain null. Impact excludes big explosions and melee/psi branches; collapse is humanoid drawRoutine0 only and never a death or damage claim. Thrown-item and large-explosion-effect events are independent presence episodes: no item identity, grenade causality, timer expiry, landing or detonation outcome is inferred from their order. Episodes may coalesce and absent events prove no absence of firing, impacts, collapse, thrown items or large effects; no attack counter or RGB-only equivalence is claimed. audio_history is compiled mixed_output capability or unavailable; current audio_status is pending, active, or unavailable. Ordered audio_status events also disclose discarded partial windows on terminal reset/failure. Current status may differ from older paged history. audio_output records an actual100ms digital final-mix window; nonzero does not identify an enemy, shot, source, direction, or physical audibility. Those unknown attribution fields stay null. audio_gap reports lost completed callback windows separately from public-ring gap. Sequence gives order only; completed PCM windows have no shared video clock, and trace receive time is not event occurrence time. Event-to-video synchronization is unproved. A grid or unclassified fragment is not enemy semantics or movement legality. This read does not advance simulation, refresh a stale grant, reconnect, or retry uncertain actions.',
         'inputSchema': arg_schema({'epoch': dict(opaque),
             'after': {'type': 'integer', 'minimum': 0, 'maximum': MAX_COUNTER},
             'limit': {'type': 'integer', 'minimum': 1, 'maximum': MAX_EVENTS}}),
         'annotations': {'readOnlyHint': True, 'destructiveHint': False, 'idempotentHint': True, 'openWorldHint': False}}]


class AgentControlClient:
    def __init__(self, channel, timeout=TIMEOUT, action_timeout=ACTION_TIMEOUT, startup_timeout=STARTUP_TIMEOUT):
        for value, maximum in ((timeout, TIMEOUT), (action_timeout, ACTION_TIMEOUT),
                               (startup_timeout, STARTUP_TIMEOUT)):
            require(type(value) in (int, float) and math.isfinite(value) and 0 < value <= maximum)
        require(isinstance(channel, socket.socket) and channel.family == socket.AF_UNIX
                and channel.type == socket.SOCK_SEQPACKET)
        require(channel.getsockname() in ('', b'') and channel.getpeername() in ('', b''))
        channel.setblocking(False)
        self.channel = channel
        self.timeout, self.action_timeout, self.startup_timeout = timeout, action_timeout, startup_timeout
        self.state = 'NEW'
        self.epoch = self.capability = self.revision = self.last_capability = None
        self.available_actions = frozenset()
        self.difficulty_values = frozenset()
        self.viewport_bounds = None
        self.viewport_samples = frozenset()
        self.choice_controls = frozenset()
        self.visible_rows = frozenset()
        self.scroll_directions = frozenset()
        self.grid_cells = frozenset()
        self.enemy_buttons = frozenset()
        self.hands = frozenset()
        self.weapon_actions = frozenset()
        self.grenade_timers = frozenset()
        self.inventory_targets = frozenset()
        self.minimap_targets = frozenset()
        self.geoscape_speeds = frozenset()
        self.base_targets = frozenset()
        self.project_rows = frozenset()
        self.staff_directions = frozenset()
        self.quantity_directions = frozenset()
        self.purchase_rows = frozenset()
        self.transfer_rows = frozenset()
        self.purchase_targets = frozenset()
        self.outfitting_rows = frozenset()
        self.equipment_rows = frozenset()
        self.capability_deadline = 0.0
        self.event_sequence = 0
        self.audio_history = None
        self.audio_status = None
        self.public_event_oldest, self.public_event_newest = 1, 0
        self.public_event_records = {}
        self.commands = {}
        self.flight = threading.Lock()

    def close(self):
        channel, self.channel = self.channel, None
        self.state = 'CLOSED'
        self.capability = None
        self.available_actions = frozenset()
        self.difficulty_values = frozenset()
        self.viewport_bounds = None
        self.viewport_samples = frozenset()
        self.choice_controls = frozenset()
        self.visible_rows = frozenset()
        self.scroll_directions = frozenset()
        self.grid_cells = frozenset()
        self.enemy_buttons = frozenset()
        self.hands = frozenset()
        self.weapon_actions = frozenset()
        self.grenade_timers = frozenset()
        self.inventory_targets = frozenset()
        self.minimap_targets = frozenset()
        self.geoscape_speeds = frozenset()
        self.base_targets = frozenset()
        self.project_rows = frozenset()
        self.staff_directions = frozenset()
        self.quantity_directions = frozenset()
        self.purchase_rows = frozenset()
        self.transfer_rows = frozenset()
        self.purchase_targets = frozenset()
        self.outfitting_rows = frozenset()
        self.equipment_rows = frozenset()
        self.public_event_records = {}
        if channel is not None:
            channel.close()

    @contextmanager
    def exclusive(self):
        acquired = self.flight.acquire(blocking=False)
        require(acquired)
        try:
            require(self.channel is not None and self.state != 'CLOSED')
            yield
        finally:
            self.flight.release()

    def packet(self, deadline):
        require(self.channel is not None)
        remaining = deadline - time.monotonic()
        require(remaining > 0)
        ready, _, _ = select.select([self.channel], [], [], remaining)
        require(bool(ready) and time.monotonic() < deadline)
        raw, ancillary, flags, _ = self.channel.recvmsg(MAX_PACKET + 1)
        require(bool(raw) and len(raw) <= MAX_PACKET and not ancillary
                and not flags & (socket.MSG_TRUNC | socket.MSG_CTRUNC))
        return raw

    def exchange(self, request, timeout=None, *, before_send=None):
        require(self.channel is not None and not select.select([self.channel], [], [], 0)[0])
        deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
        require(time.monotonic() < deadline)
        if before_send is not None:
            before_send()
        require(self.channel.send(request) == len(request))
        raw = self.packet(deadline)
        require(time.monotonic() < deadline and not select.select([self.channel], [], [], 0)[0])
        return raw

    def observe(self):
        with self.exclusive():
            require(self.state in ('NEW', 'READY', 'NEED_CONTEXT'))
            try:
                if self.state == 'NEW':
                    hello = self.packet(time.monotonic() + self.startup_timeout)
                    self.epoch = validate_hello(hello)
                    self.audio_history = parse(hello)['audio_history']
                    validate_ready(self.exchange(('H ' + SCHEMA).encode('ascii')))
                challenge = secrets.token_hex(16)
                old_capability = self.last_capability
                self.capability = None
                self.available_actions = frozenset()
                self.difficulty_values = frozenset()
                self.viewport_bounds = None
                self.viewport_samples = frozenset()
                self.choice_controls = frozenset()
                self.visible_rows = frozenset()
                self.scroll_directions = frozenset()
                self.grid_cells = frozenset()
                self.enemy_buttons = frozenset()
                self.hands = frozenset()
                self.weapon_actions = frozenset()
                self.grenade_timers = frozenset()
                self.inventory_targets = frozenset()
                self.minimap_targets = frozenset()
                self.geoscape_speeds = frozenset()
                self.base_targets = frozenset()
                self.project_rows = frozenset()
                self.staff_directions = frozenset()
                self.quantity_directions = frozenset()
                self.purchase_rows = frozenset()
                self.transfer_rows = frozenset()
                self.purchase_targets = frozenset()
                self.outfitting_rows = frozenset()
                self.equipment_rows = frozenset()
                obj = validate_state(parse(self.exchange(('R ' + challenge).encode('ascii'))), challenge, self.epoch)
                require(obj['revision'] >= (self.revision if self.revision is not None else 0)
                        and obj['event_sequence'] >= self.event_sequence)
                require(obj['capability'] is None or obj['capability'] != old_capability)
                self.revision, self.event_sequence = obj['revision'], obj['event_sequence']
                self.capability = obj['capability']
                if self.capability is not None:
                    self.last_capability = self.capability
                self.available_actions = frozenset(action['id'] for action in obj['actions'])
                if obj['state'] == 'new_battle':
                    self.choice_controls = frozenset(key for key, value in obj['visible'].items() if value is not None)
                if obj['state'] in ('new_battle_dropdown', 'purchase_category', 'transfer_destination'):
                    self.visible_rows = frozenset(row['handle'] for row in obj['visible']['rows'])
                if obj['state'] in ('new_battle_dropdown', 'purchase_category', 'purchase', 'transfer_destination', 'transfer_items', 'funding', 'base_stores', 'base_transfers', 'manufacture_start', 'items_arriving') or obj['state'] in PROJECT_LISTS or obj['state'] in OUTFITTING_LISTS or obj['state'] in RESEARCH_NOTICE_STATES:
                    self.scroll_directions = frozenset(key for key, value in obj['visible']['scroll'].items() if value)
                if obj['state'] == 'geoscape':
                    self.geoscape_speeds = frozenset(speed['handle'] for speed in obj['visible']['speeds'])
                if obj['state'] in ('basescape', 'base_info'):
                    self.base_targets = frozenset(slot['handle'] for slot in obj['visible']['base_slots'])
                if obj['state'] in PROJECT_LISTS:
                    self.project_rows = frozenset(row['handle'] for row in obj['visible']['rows'])
                if obj['state'] in PROJECT_STAFF_ACTIONS:
                    self.staff_directions = frozenset(button['id'] for button in obj['visible']['buttons'] if button['id'] in STAFF_DIRECTIONS)
                if obj['state'] == 'manufacture_info':
                    self.quantity_directions = frozenset((entry['control'], direction) for entry in obj['visible']['quantity_controls'] for direction in STAFF_DIRECTIONS if entry[direction])
                if obj['state'] == 'transfer_items':
                    self.transfer_rows = frozenset((entry['handle'], direction) for entry in obj['visible']['rows'] for direction in entry['controls'])
                if obj['state'] == 'purchase':
                    self.purchase_rows = frozenset((entry['handle'], direction) for entry in obj['visible']['rows'] for direction in entry['controls'])
                    bar = obj['visible']['scrollbar']
                    self.purchase_targets = frozenset(entry['handle'] for entry in bar['targets']) if bar is not None else frozenset()
                if obj['state'] in OUTFITTING_LISTS:
                    self.outfitting_rows = frozenset(row['handle'] for row in obj['visible']['rows'])
                if obj['state'] == 'craft_equipment':
                    self.equipment_rows = frozenset((row['handle'], direction) for row in obj['visible']['rows'] for direction in row['controls'])
                if obj['state'] == 'new_game':
                    self.difficulty_values = frozenset(choice['value'] for choice in obj['visible']['difficulty'])
                if obj['state'] == 'build_base' and obj['visible']['globe'] is not None:
                    globe = obj['visible']['globe']
                    self.viewport_bounds = (globe['width'], globe['height'])
                    self.viewport_samples = frozenset((sample['x'], sample['y']) for sample in globe['samples'])
                if obj['state'] == 'battlescape':
                    self.grid_cells = frozenset(cell['handle'] for cell in obj['visible']['tactical']['grid'])
                    self.enemy_buttons = frozenset(button['handle'] for button in obj['visible']['enemy_buttons'])
                    self.hands = frozenset(hand['hand'] for hand in obj['visible']['hands'])
                elif obj['state'] == 'minimap':
                    self.minimap_targets = frozenset(target['handle'] for target in obj['visible']['targets'])
                elif obj['state'] == 'inventory':
                    self.inventory_targets = frozenset(target['handle'] for target in obj['visible']['targets'])
                elif obj['state'] == 'weapon_action_menu':
                    self.weapon_actions = frozenset(row['handle'] for row in obj['visible']['rows'])
                elif obj['state'] == 'prime_grenade':
                    self.grenade_timers = frozenset(timer['handle'] for timer in obj['visible']['timers'])
                self.capability_deadline = time.monotonic() + CAPABILITY_TTL
                self.state = 'READY' if self.capability is not None else 'NEED_CONTEXT'
                return obj
            except FAILURES:
                self.close()
                raise Unavailable('Unavailable') from None

    def read_events(self, epoch, after, limit):
        validate_event_arguments(dict(epoch=epoch, after=after, limit=limit))
        with self.exclusive():
            require(self.state in ('READY', 'NEED_CONTEXT') and epoch == self.epoch)
            try:
                challenge = secrets.token_hex(16)
                request = f'V {challenge} {epoch} {after} {limit}'.encode('ascii')
                obj = validate_events(self.exchange(request), challenge, epoch, after, limit, self.audio_history)
                require(obj['oldest'] >= self.public_event_oldest and obj['newest'] >= self.public_event_newest)
                records = {seq: value for seq, value in self.public_event_records.items() if seq >= obj['oldest']}
                for event in obj['events']:
                    canonical = json.dumps(event, sort_keys=True, ensure_ascii=True, separators=(',', ':')).encode('ascii')
                    require(event['seq'] not in records or records[event['seq']] == canonical)
                    records[event['seq']] = canonical
                require(len(records) <= EVENT_CAPACITY)
                self.public_event_records = records
                self.public_event_oldest, self.public_event_newest = obj['oldest'], obj['newest']
                self.audio_status = obj['audio_status']
                # No command-ledger admission, UI revision update or grant refresh/consumption.
                return obj
            except FAILURES:
                self.close()
                raise Unavailable('Unavailable') from None

    def execute(self, command_id, epoch, capability, revision, action, args):
        arguments = dict(command_id=command_id, epoch=epoch, capability=capability, revision=revision, action=action, args=args)
        validate_action_arguments(arguments)
        # Values are primitives. Copy the caller's mutable object before
        # compiling, and retain only immutable canonical bytes in the ledger.
        args = dict(args)
        arguments['args'] = args
        request = canonical_action_request(arguments)
        with self.exclusive():
            previous = self.commands.get(command_id)
            if previous is not None:
                require(previous['request'] == request)
                return self._read_result(command_id)
            require(self.state == 'READY' and epoch == self.epoch and capability == self.capability
                    and revision == self.revision and action in self.available_actions
                    and time.monotonic() < self.capability_deadline)
            if action == 'select_difficulty':
                require(args['value'] in self.difficulty_values)
            elif action == 'open_new_battle_choice':
                require(args['control'] in self.choice_controls)
            elif action == 'choose_visible_row':
                require(args['row'] in self.visible_rows)
            elif action == 'scroll_visible_list':
                require(args['direction'] in self.scroll_directions)
            elif action in ('change_production_quantity', 'change_purchase_quantity', 'change_craft_equipment', 'change_transfer_quantity'):
                key = args['control'] if action == 'change_production_quantity' else args['row']
                shown = (self.quantity_directions if action == 'change_production_quantity' else
                         self.equipment_rows if action == 'change_craft_equipment' else
                         self.transfer_rows if action == 'change_transfer_quantity' else self.purchase_rows)
                operation = args['operation']
                required = (STAFF_DIRECTIONS if operation == 'set' else
                            ('increase',) if operation == 'max' or (operation == 'adjust' and args['value'] > 0) else ('decrease',))
                require(all((key, direction) in shown for direction in required))
            elif ARGUMENT_KINDS[action] in ('outfitting_row', 'crew_assignment'):
                require(args['row'] in self.outfitting_rows)
            elif action == 'scroll_purchase':
                require(args['direction'] in self.scroll_directions)
            elif action == 'position_purchase_list':
                require(args['target'] in self.purchase_targets)
            elif action == 'place_base_site':
                require(self.viewport_bounds is not None)
                require(args['x'] < self.viewport_bounds[0] and args['y'] < self.viewport_bounds[1]
                        and (args['x'], args['y']) in self.viewport_samples)
            elif action in ('map_primary', 'map_secondary'):
                require(args['cell'] in self.grid_cells)
            elif action == 'center_enemy':
                require(args['button'] in self.enemy_buttons)
            elif action == 'open_hand_action_menu':
                require(args['hand'] in self.hands)
            elif action == 'choose_weapon_action':
                require(args['row'] in self.weapon_actions)
            elif action == 'choose_grenade_timer':
                require(args['timer'] in self.grenade_timers)
            elif action == 'minimap_center':
                require(args['target'] in self.minimap_targets)
            elif action == 'choose_geoscape_speed':
                require(args['speed'] in self.geoscape_speeds)
            elif action == 'select_base':
                require(args['target'] in self.base_targets)
            elif action in ('select_research_project', 'select_production_project'):
                require(args['row'] in self.project_rows)
            elif action in ('adjust_scientists', 'adjust_engineers'):
                require(args['direction'] in self.staff_directions)
            elif action in ('inventory_hover', 'inventory_activate'):
                require(args['target'] in self.inventory_targets)
            if len(self.commands) >= MAX_COMMANDS:
                self.close()
                raise Unavailable('Unavailable')
            # Spend the grant and reserve the ID before the only send attempt.
            self.capability = None
            self.available_actions = frozenset()
            self.difficulty_values = frozenset()
            self.viewport_bounds = None
            self.viewport_samples = frozenset()
            self.choice_controls = frozenset()
            self.visible_rows = frozenset()
            self.scroll_directions = frozenset()
            self.grid_cells = frozenset()
            self.enemy_buttons = frozenset()
            self.hands = frozenset()
            self.weapon_actions = frozenset()
            self.grenade_timers = frozenset()
            self.inventory_targets = frozenset()
            self.minimap_targets = frozenset()
            self.geoscape_speeds = frozenset()
            self.base_targets = frozenset()
            self.project_rows = frozenset()
            self.staff_directions = frozenset()
            self.quantity_directions = frozenset()
            self.purchase_rows = frozenset()
            self.transfer_rows = frozenset()
            self.purchase_targets = frozenset()
            self.outfitting_rows = frozenset()
            self.equipment_rows = frozenset()
            self.state = 'PENDING'
            self.commands[command_id] = {'request': request, 'result': None}
            attempted = False
            def mark_send_attempted():
                nonlocal attempted
                attempted = True
            try:
                obj = validate_result(self.exchange(request, self.action_timeout, before_send=mark_send_attempted),
                                      command_id, self.epoch)
                require(obj['status'] in TERMINAL_STATUSES)
                return self._accept_result(obj)
            except FAILURES:
                self.close()
                if attempted:
                    raise ActionOutcomeUnknown(command_id) from None
                raise Unavailable('Unavailable') from None

    def read_result(self, command_id):
        token(command_id)
        with self.exclusive():
            require(command_id in self.commands)
            return self._read_result(command_id)

    def _read_result(self, command_id):
        try:
            obj = validate_result(self.exchange(('S ' + command_id).encode('ascii')), command_id, self.epoch)
            return self._accept_result(obj)
        except FAILURES:
            self.close()
            raise Unavailable('Unavailable') from None

    def _accept_result(self, obj):
        previous = self.commands[obj['command_id']]['result']
        canonical = json.dumps(obj, sort_keys=True, ensure_ascii=True, separators=(',', ':')).encode('ascii')
        if previous is not None and strict_json(previous.decode('ascii'))['status'] in TERMINAL_STATUSES:
            require(canonical == previous)
        if previous is None:
            require(obj['event_sequence'] >= self.event_sequence)
            observation = obj['observation']
            if observation is not None:
                require(observation['revision'] >= self.revision)
                self.revision = observation['revision']
            self.event_sequence = obj['event_sequence']
        # Never retain a caller-mutable receipt or nested observation as the
        # terminal identity baseline. Every output object is detached from bytes.
        self.commands[obj['command_id']]['result'] = canonical
        if self.state == 'PENDING' and obj['status'] in TERMINAL_STATUSES:
            self.state = 'NEED_CONTEXT'
        return obj
