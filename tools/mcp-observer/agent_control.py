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

SCHEMA = 'openxcom-agent-control/4'
ERROR_SCHEMA = 'openxcom-mcp-agent-control-error/1'
MODE = 'agent-control'
MAX_PACKET = 49152
MAX_TEXT_PACKET = 32768
MAX_REQUEST = 512
MAX_NAME_BYTES = 128
MAX_COMMANDS = 64
MAX_COUNTER = 2**53 - 1
TIMEOUT, ACTION_TIMEOUT, STARTUP_TIMEOUT = 2.0, 31.0, 31.0
CAPABILITY_TTL = 120.0
ACTIONS = ('next_soldier', 'previous_soldier', 'kneel', 'stand', 'continue_next_turn',
           'new_battle', 'start_battle', 'continue_briefing', 'close_inventory',
           'open_new_game', 'open_options', 'open_load_game', 'open_pause_menu',
           'open_save_game', 'resume_game', 'cancel_menu', 'dismiss_error',
           'select_difficulty', 'set_ironman', 'start_new_game', 'place_base_site',
           'replace_base_name', 'confirm_base_name', 'open_new_battle_choice',
           'choose_visible_row', 'scroll_visible_list', 'close_dropdown',
           'open_funding', 'close_funding')
DIFFICULTIES = ('beginner', 'experienced', 'veteran', 'genius', 'superhuman')
CHOICE_CONTROLS = ('mission', 'craft', 'terrain', 'difficulty', 'alien_race')
ROW_HANDLES = tuple(f'r{index}' for index in range(32))
SCROLL_DIRECTIONS = ('up', 'down')
ARGUMENT_KINDS = dict.fromkeys(ACTIONS, 'none')
ARGUMENT_KINDS.update(select_difficulty='difficulty', set_ironman='boolean',
                      replace_base_name='text', place_base_site='point',
                      open_new_battle_choice='choice_control', choose_visible_row='visible_row',
                      scroll_visible_list='scroll_direction')
STATE_FIELDS = {
    'next_turn': ('title', 'turn', 'side', 'message'),
    'main_menu': ('new_battle_label',),
    'new_battle': ('mission', 'craft', 'terrain', 'difficulty', 'alien_race'),
    'briefing': ('title', 'target', 'craft', 'briefing'),
    'inventory': ('close_label',),
    'base_name': ('title', 'name'),
    'options_shell': ('category', 'tooltip'),
    'pause_menu': ('title',),
    'load_list': ('title', 'name_heading', 'date_heading', 'details'),
    'save_list': ('title', 'name_heading', 'date_heading', 'details'),
    'error_message': ('message',),
}
STATE_ACTIONS = {
    'battlescape': frozenset(('next_soldier', 'previous_soldier', 'kneel', 'stand', 'open_pause_menu')),
    'next_turn': frozenset(('continue_next_turn',)),
    'main_menu': frozenset(('new_battle', 'open_new_game', 'open_options', 'open_load_game')),
    'new_battle': frozenset(('start_battle', 'cancel_menu', 'open_new_battle_choice')),
    'new_battle_dropdown': frozenset(('choose_visible_row', 'scroll_visible_list', 'close_dropdown')),
    'funding': frozenset(('scroll_visible_list', 'close_funding')),
    'briefing': frozenset(('continue_briefing',)),
    'inventory': frozenset(('close_inventory',)),
    'new_game': frozenset(('cancel_menu', 'select_difficulty', 'set_ironman', 'start_new_game')),
    'base_name': frozenset(('replace_base_name', 'confirm_base_name')),
    'build_base': frozenset(('place_base_site',)),
    'geoscape': frozenset(('open_funding',)),
    'options_shell': frozenset(('cancel_menu',)),
    'pause_menu': frozenset(('open_load_game', 'open_save_game', 'open_options', 'resume_game')),
    'load_list': frozenset(('cancel_menu',)),
    'save_list': frozenset(('cancel_menu',)),
    'error_message': frozenset(('dismiss_error',)),
    'unsupported': frozenset(),
}
GEOSCAPE_FIELDS = ('hour', 'minute', 'second', 'weekday', 'day', 'month', 'year', 'funds', 'speed')
GEOSCAPE_BUTTONS = ('intercept', 'bases', 'graphs', 'ufopaedia', 'options', 'funding')
STATUSES = ('pending', 'dispatched', 'unavailable', 'unknown_after_dispatch', 'unknown')
TERMINAL_STATUSES = ('dispatched', 'unavailable', 'unknown_after_dispatch')
TOOL_NAMES = ('read_ui_state', 'execute_ui_action', 'read_ui_action_result')
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
    exact(obj, 'schema mode epoch ledger_capacity')
    identity(obj)
    token(obj['epoch'])
    integer(obj['ledger_capacity'], MAX_COMMANDS, MAX_COMMANDS)
    return obj['epoch']


def validate_ready(raw):
    obj = parse(raw)
    exact(obj, 'schema mode status')
    identity(obj)
    require(obj['status'] == 'ready')


def label_source(screen, action):
    return ('visible_control_semantics' if screen in ('battlescape', 'inventory') or
            action in ('replace_base_name', 'place_base_site', 'open_new_battle_choice',
                       'choose_visible_row', 'scroll_visible_list', 'close_dropdown') else 'visible_text')


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
    visible = obj['visible']
    if obj['state'] == 'battlescape':
        exact(visible, 'selected_name time_units energy health morale kneel_indicator')
        visible_text(visible['selected_name'], nullable=True)
        for name in ('time_units', 'energy', 'health', 'morale'):
            if visible[name] is not None:
                integer(visible[name], 0, 2**32 - 1)
        require(visible['kneel_indicator'] is None or
                (type(visible['kneel_indicator']) is str and visible['kneel_indicator'] in ('standing', 'kneeling')))
        require('continue_next_turn' not in seen)
    elif obj['state'] == 'new_game':
        validate_new_game(visible, seen)
    elif obj['state'] == 'geoscape':
        validate_geoscape(visible, seen)
    elif obj['state'] in ('new_battle_dropdown', 'funding'):
        validate_visible_list(visible, seen, funding=obj['state'] == 'funding')
    elif obj['state'] == 'build_base':
        validate_build_base(visible, seen)
    elif obj['state'] in STATE_FIELDS:
        exact(visible, ' '.join(STATE_FIELDS[obj['state']]))
        for text in visible.values():
            visible_text(text, nullable=True)
        if obj['state'] == 'new_battle':
            require('open_new_battle_choice' not in seen or any(value is not None for value in visible.values()))
    else:
        exact(visible, '')
        require(not seen)
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
    elif kind in ('choice_control', 'visible_row', 'scroll_direction'):
        suffix = ' ' + args[{'choice_control': 'control', 'visible_row': 'row', 'scroll_direction': 'direction'}[kind]]
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
         'description': 'Explicit agent-control protocol /4: read one fresh, bounded semantic UI presentation and the currently exposed visible UI action IDs; listing does not predict action success or hidden eligibility. Includes visible text/numbers, public UI revision and event sequence; no screenshot recognition, actor IDs or hidden world state. Campaign setup exposes visible difficulty choices and Ironman state, clipped drawn Globe geometry in its viewport, and the base-name field. New Battle exposes five bounded dropdowns with only fully rendered local rows; Funding exposes a read-only table and ordinary scroll/OK controls. Geoscape exposes only opening Funding. Unsupported screens have an empty catalog. No campaign save/load/overwrite, file rows, arbitrary settings or general strategy actions are exposed. This replaces any previous one-use capability. After every action, read again before another action; transport uncertainty permanently closes this channel.',
         'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
         'annotations': {'readOnlyHint': True, 'destructiveHint': False, 'idempotentHint': False, 'openWorldHint': False}},
        {'name': 'execute_ui_action',
         'description': 'Directly dispatch one currently exposed semantic UI action in explicitly enabled agent-control mode. This is not human confirmation and does not approve supervised controls. Use the exact action ID, epoch, revision and single-use capability from a fresh read_ui_state, with a new command ID and the action-specific args object ({} for no-argument actions). New Battle control names, dropdown row handles and one-step scroll directions must come from the fresh visible projection; handles are local to that list and grant, never global indices or model IDs. Scroll flags describe visible arrows, so boundary no-ops remain possible. Funding rows cannot be selected. Difficulty must be a currently exposed choice; Ironman takes a desired boolean, not a toggle. Base-name text is strict UTF-8, at most 128 bytes, without control characters; replacement does not confirm the name. Base-site x/y must exactly match a point in the freshly advertised Globe samples; unrestricted viewport points are deferred. Coordinates are viewport-local integers, never world coordinates or a general click. Start and name confirmation are separate explicit actions. The capability expires after 120 seconds and any UI revision change invalidates it sooner. Kneel/stand may spend time units and trigger reactions. Menu cancel uses the ordinary handler: New Battle can save its skirmish preset, and Options reloads persisted settings and restores volume/input capture. A receipt means handler dispatch, never success; inspect its separate post-action observation and obtain a fresh read before another action. Exact duplicates only query the receipt; conflicting command IDs fail. Never automatically retry after uncertainty. An adapter_error with status outcome_unknown means a mutating send was attempted but no trustworthy native receipt arrived; stop and reconcile manually.',
         'inputSchema': execute_schema,
         'annotations': {'readOnlyHint': False, 'destructiveHint': True, 'idempotentHint': True, 'openWorldHint': False}},
        {'name': 'read_ui_action_result',
         'description': 'Read a known command receipt without replaying it. The separate observation describes visible UI after dispatch; dispatched is not gameplay success. Public event_sequence is not a simulation counter. This tool never reconnects or retries an uncertain action.',
         'inputSchema': {'type': 'object', 'properties': {'command_id': dict(opaque)},
                         'required': ['command_id'], 'additionalProperties': False},
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
        self.capability_deadline = 0.0
        self.event_sequence = 0
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
                    self.epoch = validate_hello(self.packet(time.monotonic() + self.startup_timeout))
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
                self.capability_deadline = time.monotonic() + CAPABILITY_TTL
                self.state = 'READY' if self.capability is not None else 'NEED_CONTEXT'
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
        if previous is not None and previous['status'] in TERMINAL_STATUSES:
            require(obj == previous)
        if previous is None:
            require(obj['event_sequence'] >= self.event_sequence)
            observation = obj['observation']
            if observation is not None:
                require(observation['revision'] >= self.revision)
                self.revision = observation['revision']
            self.event_sequence = obj['event_sequence']
        self.commands[obj['command_id']]['result'] = obj
        if self.state == 'PENDING' and obj['status'] in TERMINAL_STATUSES:
            self.state = 'NEED_CONTEXT'
        return obj
