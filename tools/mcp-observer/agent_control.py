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

SCHEMA = 'openxcom-agent-control/11'
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
           'open_hand_action_menu', 'choose_fire_mode', 'cancel_weapon_action',
           'close_debriefing', 'open_inventory', 'inventory_hover', 'inventory_activate',
           'cancel_inventory_hold', 'unload_inventory', 'next_inventory_ground_page')
DIFFICULTIES = ('beginner', 'experienced', 'veteran', 'genius', 'superhuman')
CHOICE_CONTROLS = ('mission', 'craft', 'terrain', 'difficulty', 'alien_race')
ROW_HANDLES = tuple(f'r{index}' for index in range(32))
SCROLL_DIRECTIONS = ('up', 'down')
GRID_HANDLES = tuple(f'g{index}' for index in range(128))
ENEMY_BUTTON_HANDLES = tuple(f'e{index}' for index in range(10))
FIRE_MODE_HANDLES = tuple(f'a{index}' for index in range(6))
HANDS = ('left', 'right')
INVENTORY_TARGET_HANDLES = tuple(f'v{index}' for index in range(64))
INVENTORY_TEXT_FIELDS = ('selected_name', 'time_units', 'weight', 'item_name', 'ammo', 'warning')
ARGUMENT_KINDS = dict.fromkeys(ACTIONS, 'none')
ARGUMENT_KINDS.update(select_difficulty='difficulty', set_ironman='boolean',
                      replace_base_name='text', place_base_site='point',
                      open_new_battle_choice='choice_control', choose_visible_row='visible_row',
                      scroll_visible_list='scroll_direction', map_primary='grid_cell', map_secondary='grid_cell',
                      center_enemy='enemy_button', open_hand_action_menu='hand', choose_fire_mode='fire_mode',
                      inventory_hover='inventory_target', inventory_activate='inventory_target')
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
    'error_message': ('message',),
    'infobox': ('message',),
    'infobox_ok': ('message',),
    'battlescape_warning': ('message',),
}
STATE_ACTIONS = {
    'battlescape': frozenset(('next_soldier', 'previous_soldier', 'kneel', 'stand', 'open_pause_menu', 'end_turn',
                              'map_primary', 'map_secondary', 'center_selected_unit', 'center_enemy',
                              'open_hand_action_menu', 'open_inventory')),
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
    'geoscape': frozenset(('open_funding',)),
    'options_shell': frozenset(('cancel_menu',)),
    'pause_menu': frozenset(('open_load_game', 'open_save_game', 'open_options', 'resume_game')),
    'load_list': frozenset(('cancel_menu',)),
    'save_list': frozenset(('cancel_menu',)),
    'error_message': frozenset(('dismiss_error',)),
    'infobox': frozenset(),
    'infobox_ok': frozenset(('acknowledge_message',)),
    'battlescape_warning': frozenset(),
    'weapon_action_menu': frozenset(('choose_fire_mode', 'cancel_weapon_action')),
    'debriefing': frozenset(('close_debriefing',)),
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
    return ('visible_control_semantics' if screen in ('battlescape', 'inventory') or
            action in ('replace_base_name', 'place_base_site', 'open_new_battle_choice',
                       'choose_visible_row', 'scroll_visible_list', 'close_dropdown', 'cancel_weapon_action') else 'visible_text')


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


def validate_geoscape(visible, actions):
    exact(visible, ' '.join(GEOSCAPE_FIELDS) + ' buttons')
    for field in GEOSCAPE_FIELDS:
        visible_text(visible[field], nullable=True)
    require(type(visible['buttons']) is list and len(visible['buttons']) <= len(GEOSCAPE_BUTTONS))
    seen = set()
    for button in visible['buttons']:
        exact(button, 'id label')
        require(type(button['id']) is str and button['id'] in GEOSCAPE_BUTTONS and button['id'] not in seen)
        seen.add(button['id'])
        visible_text(button['label']); require(bool(button['label']))
    require('open_funding' not in actions or 'funding' in seen)


def validate_visible_list(visible, actions, *, funding=False):
    """Only fully rendered, grant-local rows; no backing indices or totals."""
    exact(visible, 'title headings rows scroll' if funding else 'control label rows scroll')
    if funding:
        visible_text(visible['title'], nullable=True)
        require(type(visible['headings']) is list and len(visible['headings']) == 3)
        for heading in visible['headings']:
            visible_text(heading, nullable=True)
    else:
        require(type(visible['control']) is str and visible['control'] in CHOICE_CONTROLS)
        visible_text(visible['label'], nullable=True)
    rows = visible['rows']
    require(type(rows) is list and len(rows) <= len(ROW_HANDLES))
    selected = 0
    for index, row in enumerate(rows):
        exact(row, 'handle cells selected')
        require(row['handle'] == ROW_HANDLES[index])
        require(type(row['cells']) is list and len(row['cells']) == (3 if funding else 1))
        for cell in row['cells']:
            visible_text(cell)
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
    require(not {'map_primary', 'map_secondary'}.intersection(actions) or (tactical['mode'] == 'map' and bool(cells)))
    require('center_enemy' not in actions or bool(buttons))
    require('open_hand_action_menu' not in actions or bool(hands))


def validate_weapon_action_menu(visible, actions):
    exact(visible, 'rows')
    rows = visible['rows']
    require(type(rows) is list and len(rows) <= len(FIRE_MODE_HANDLES))
    for index, row in enumerate(rows):
        exact(row, 'handle label accuracy time_units')
        require(row['handle'] == FIRE_MODE_HANDLES[index])
        visible_text(row['label']); require(bool(row['label']))
        visible_text(row['accuracy'], nullable=True)
        visible_text(row['time_units'], nullable=True)
    require('choose_fire_mode' not in actions or bool(rows))


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
        require(type(target['area']) is str and target['area'] in ('hand', 'ground'))
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
        exact(visible, 'selected_name time_units energy health morale kneel_indicator tactical hands enemy_buttons focus_target')
        visible_text(visible['selected_name'], nullable=True)
        for name in ('time_units', 'energy', 'health', 'morale'):
            if visible[name] is not None:
                integer(visible[name], 0, 2**32 - 1)
        require(visible['kneel_indicator'] is None or
                (type(visible['kneel_indicator']) is str and visible['kneel_indicator'] in ('standing', 'kneeling')))
        validate_tactical(visible['tactical'])
        validate_battlescape_controls(visible, seen)
    elif screen == 'inventory':
        validate_inventory(visible, seen)
    elif screen == 'weapon_action_menu':
        validate_weapon_action_menu(visible, seen)
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
    elif screen in ('new_battle_dropdown', 'funding'):
        validate_visible_list(visible, seen, funding=screen == 'funding')
    elif screen == 'build_base':
        validate_build_base(visible, seen)
    elif screen in STATE_FIELDS:
        exact(visible, ' '.join(STATE_FIELDS[screen]))
        for text in visible.values(): visible_text(text, nullable=True)
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
    if obj['state'] == 'weapon_action_menu' and 'choose_fire_mode' in seen:
        descriptor = next(action for action in obj['actions'] if action['id'] == 'choose_fire_mode')
        require(descriptor['label'] == obj['visible']['rows'][0]['label'])
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
        elif event['kind'] in ('projectile_presented', 'impact_effect_presented', 'collapse_animation_presented'):
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
            'fire_mode': ('row', FIRE_MODE_HANDLES),
            'inventory_target': ('target', INVENTORY_TARGET_HANDLES),
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
    elif kind == 'text':
        suffix = ' ' + (args['text'].encode('utf-8').hex() or '-')
    elif kind == 'point':
        suffix = f' {args["x"]} {args["y"]}'
    elif kind in ('choice_control', 'visible_row', 'scroll_direction', 'grid_cell', 'enemy_button', 'hand', 'fire_mode', 'inventory_target'):
        suffix = ' ' + args[{'choice_control': 'control', 'visible_row': 'row', 'scroll_direction': 'direction',
                                 'grid_cell': 'cell', 'enemy_button': 'button', 'hand': 'hand', 'fire_mode': 'row', 'inventory_target': 'target'}[kind]]
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
        'fire_mode': arg_schema({'row': {'type': 'string', 'enum': list(FIRE_MODE_HANDLES)}}),
        'inventory_target': arg_schema({'target': {'type': 'string', 'enum': list(INVENTORY_TARGET_HANDLES)}}),
    }
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
         'description': 'Explicit agent-control protocol /11: read one fresh, bounded semantic UI presentation and the currently exposed visible UI action IDs; listing does not predict action success or hidden eligibility. Includes visible text/numbers, public UI revision and lifecycle event sequence; the read_public_events cursor is separate. Battlescape adds a camera-derived grid and bounded unclassified presented fragments only when available; HiddenMovement is explicit and suppresses geometry. Grid geometry is not known terrain, visibility or legal movement; no screenshot recognition, actor IDs or hidden world state. Campaign setup exposes visible difficulty choices and Ironman state, clipped drawn Globe geometry in its viewport, and the base-name field. New Battle exposes five bounded dropdowns with only fully rendered local rows; Funding exposes a read-only table and ordinary scroll/OK controls. Geoscape exposes only opening Funding. Battlescape additionally exposes only current painted hand/ammo controls, numbered enemy HUD buttons and a camera-geometry focus grid handle. Weapon action menus expose bounded rendered fire-mode rows; actual warning text has a separate read-only state and retained displayed-message event. These are not target identities or success predictions. Timed Infobox and Infobox OK expose only the actual displayed message; only a visible OK button can offer acknowledge_message. Debriefing exposes only complete shown title/rating/Stats label, an unavailable table, and the visible ordinary OK control. Inventory exposes only two painted hands and the current painted ground page through local vN targets, actual shown text and held-sprite evidence; unknown item/held projection never asserts emptiness or eligibility. Standalone hover obtains ordinary rendered inspection text. Unsupported screens have an empty catalog. No campaign save/load/overwrite, file rows, arbitrary settings or general strategy actions are exposed. This replaces any previous one-use capability. After every action, read again before another action; transport uncertainty permanently closes this channel.',
         'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
         'annotations': {'readOnlyHint': True, 'destructiveHint': False, 'idempotentHint': False, 'openWorldHint': False}},
        {'name': 'execute_ui_action',
         'description': 'Directly dispatch one currently exposed semantic UI action in explicitly enabled agent-control mode. This is not human confirmation and does not approve supervised controls. Use the exact action ID, epoch, revision and single-use capability from a fresh read_ui_state, with a new command ID and the action-specific args object ({} for no-argument actions). New Battle control names, dropdown row handles and one-step scroll directions must come from the fresh visible projection; handles are local to that list and grant, never global indices or model IDs. Grid, enemy-button, hand and fire-mode handles must come from the same fresh grant. Map primary/secondary and hand actions preserve normal context-sensitive behavior and can refuse or cancel instead of moving/firing/opening; read the resulting state and visible warning. Scroll flags describe visible arrows, so boundary no-ops remain possible. Funding rows cannot be selected. Difficulty must be a currently exposed choice; Ironman takes a desired boolean, not a toggle. Base-name text is strict UTF-8, at most 128 bytes, without control characters; replacement does not confirm the name. Base-site x/y must exactly match a point in the freshly advertised Globe samples; unrestricted viewport points are deferred. Coordinates are viewport-local integers, never world coordinates or a general click. Start and name confirmation are separate explicit actions. The capability expires after 120 seconds and any UI revision change invalidates it sooner. Kneel/stand may spend time units and trigger reactions. end_turn dispatches the ordinary End Turn control and may start enemy activity; its receipt is not an enemy-turn success claim. Menu cancel uses the ordinary handler: New Battle can save its skirmish preset, and Options reloads persisted settings and restores volume/input capture. Inventory hover inspects through the ordinary shared hover path. Inventory activation atomically applies ordinary hover/held-anchor and primary activation on a fresh listed vN target. Shown item/held sprite evidence never predicts emptiness, compatibility or eligibility; null and unknown stay unknown. Cancel requires a shown held sprite; unload, page and close follow ordinary controls and may refuse or have no transition. Inspect the fresh rendered item/ammo/warning and held/target projection to establish postconditions. A receipt means handler dispatch, never success; inspect its separate post-action observation and obtain a fresh read before another action. Exact duplicates only query the receipt; conflicting command IDs fail. Never automatically retry after uncertainty. An adapter_error with status outcome_unknown means a mutating send was attempted but no trustworthy native receipt arrived; stop and reconcile manually.',
         'inputSchema': execute_schema,
         'annotations': {'readOnlyHint': False, 'destructiveHint': True, 'idempotentHint': True, 'openWorldHint': False}},
        {'name': 'read_ui_action_result',
         'description': 'Read a known command receipt without replaying it. The separate observation describes visible UI after dispatch; dispatched is not gameplay success. Public event_sequence is not a simulation counter. This tool never reconnects or retries an uncertain action.',
         'inputSchema': {'type': 'object', 'properties': {'command_id': dict(opaque)},
                         'required': ['command_id'], 'additionalProperties': False},
         'annotations': {'readOnlyHint': True, 'destructiveHint': False, 'idempotentHint': True, 'openWorldHint': False}},
        {'name': 'read_public_events',
         'description': 'Read retained public presentation, exact displayed-message reports, renderer projectile, impact-effect and collapse-animation presence episodes and digital mixed-output history after a cursor, without consuming it or an action grant. First read_ui_state supplies the session epoch. The independent native ring retains256 public events; after is exclusive, limit is1..16, next_cursor advances through returned events and gap explicitly reports expired history. A byte-bounded page may contain fewer than limit. Identical cursor reads may include newer events; retained records are immutable. Presentation events contain only the same bounded visible DTO, no action authority. displayed_message contains complete shown text and an exact compiled phrase report or null; source and position stay null, and no hidden cause or actor identity is inferred. projectile_presented, impact_effect_presented and collapse_animation_presented mean the corresponding ordinary renderer contribution survived an admitted presentation; source, actor, faction, world_position, hit and damage remain null. Impact excludes big explosions and melee/psi branches; collapse is humanoid drawRoutine0 only and never a death or damage claim. Episodes may coalesce and absent events prove no absence of firing, impacts or collapse; no attack counter or RGB-only equivalence is claimed. audio_history is compiled mixed_output capability or unavailable; current audio_status is pending, active, or unavailable. Ordered audio_status events also disclose discarded partial windows on terminal reset/failure. Current status may differ from older paged history. audio_output records an actual100ms digital final-mix window; nonzero does not identify an enemy, shot, source, direction, or physical audibility. Those unknown attribution fields stay null. audio_gap reports lost completed callback windows separately from public-ring gap. Sequence gives order only; completed PCM windows have no shared video clock, and trace receive time is not event occurrence time. Event-to-video synchronization is unproved. A grid or unclassified fragment is not enemy semantics or movement legality. This read does not advance simulation, refresh a stale grant, reconnect, or retry uncertain actions.',
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
        self.fire_modes = frozenset()
        self.inventory_targets = frozenset()
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
        self.fire_modes = frozenset()
        self.inventory_targets = frozenset()
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
                self.fire_modes = frozenset()
                self.inventory_targets = frozenset()
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
                if obj['state'] == 'new_battle_dropdown':
                    self.visible_rows = frozenset(row['handle'] for row in obj['visible']['rows'])
                if obj['state'] in ('new_battle_dropdown', 'funding'):
                    self.scroll_directions = frozenset(key for key, value in obj['visible']['scroll'].items() if value)
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
                elif obj['state'] == 'inventory':
                    self.inventory_targets = frozenset(target['handle'] for target in obj['visible']['targets'])
                elif obj['state'] == 'weapon_action_menu':
                    self.fire_modes = frozenset(row['handle'] for row in obj['visible']['rows'])
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
            elif action == 'choose_fire_mode':
                require(args['row'] in self.fire_modes)
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
            self.fire_modes = frozenset()
            self.inventory_targets = frozenset()
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
