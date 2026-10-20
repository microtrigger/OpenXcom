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

SCHEMA = 'openxcom-agent-control/2'
ERROR_SCHEMA = 'openxcom-mcp-agent-control-error/1'
MODE = 'agent-control'
MAX_PACKET = 32768
MAX_COMMANDS = 64
MAX_COUNTER = 2**53 - 1
TIMEOUT, ACTION_TIMEOUT, STARTUP_TIMEOUT = 2.0, 31.0, 31.0
CAPABILITY_TTL = 120.0
ACTIONS = ('next_soldier', 'previous_soldier', 'kneel', 'stand', 'continue_next_turn',
           'new_battle', 'start_battle', 'continue_briefing', 'close_inventory',
           'open_new_game', 'open_options', 'open_load_game', 'open_pause_menu',
           'open_save_game', 'resume_game', 'cancel_menu', 'dismiss_error')
STATE_FIELDS = {
    'next_turn': ('title', 'turn', 'side', 'message'),
    'main_menu': ('new_battle_label',),
    'new_battle': ('mission', 'craft', 'terrain', 'difficulty', 'alien_race'),
    'briefing': ('title', 'target', 'craft', 'briefing'),
    'inventory': ('close_label',),
    'new_game': ('title', 'ironman_note'),
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
    'new_battle': frozenset(('start_battle', 'cancel_menu')),
    'briefing': frozenset(('continue_briefing',)),
    'inventory': frozenset(('close_inventory',)),
    'new_game': frozenset(('cancel_menu',)),
    'options_shell': frozenset(('cancel_menu',)),
    'pause_menu': frozenset(('open_load_game', 'open_save_game', 'open_options', 'resume_game')),
    'load_list': frozenset(('cancel_menu',)),
    'save_list': frozenset(('cancel_menu',)),
    'error_message': frozenset(('dismiss_error',)),
    'unsupported': frozenset(),
}
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
    require((nullable and value is None) or
            (type(value) is str and len(value.encode('utf-8')) <= 4096))


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
        exact(action, 'id label label_source')
        require(type(action['id']) is str and action['id'] in ACTIONS and action['id'] not in seen)
        seen.add(action['id'])
        visible_text(action['label'])
        require(bool(action['label']))
        require(action['label_source'] == ('visible_control_semantics' if obj['state'] in ('battlescape', 'inventory')
                                          else 'visible_text'))
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
    elif obj['state'] in STATE_FIELDS:
        exact(visible, ' '.join(STATE_FIELDS[obj['state']]))
        for text in visible.values():
            visible_text(text, nullable=True)
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
    require(len(json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii')) <= MAX_PACKET)
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
    require(len(json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii')) <= MAX_PACKET)
    return obj


def validate_action_arguments(arguments):
    exact(arguments, 'command_id epoch capability revision action')
    for name in ('command_id', 'epoch', 'capability'):
        token(arguments[name])
    integer(arguments['revision'], 0, MAX_COUNTER)
    require(type(arguments['action']) is str and arguments['action'] in ACTIONS)


def definitions():
    opaque = {'type': 'string', 'pattern': '^[0-9a-f]{32}$'}
    return [
        {'name': 'read_ui_state',
         'description': 'Explicit agent-control protocol /2: read one fresh, bounded semantic UI presentation and the currently exposed visible UI action IDs; listing does not predict action success or hidden eligibility. Includes visible text/numbers, public UI revision and event sequence; no screenshot recognition, actor IDs or hidden world state. Unsupported screens have an empty catalog. Menu shells expose bounded rendered labels and navigation controls; they do not enumerate save files or offer campaign save/load/overwrite, campaign start, or settings edits. This replaces any previous one-use capability. After every action, read again before another action; transport uncertainty permanently closes this channel.',
         'inputSchema': {'type': 'object', 'properties': {}, 'additionalProperties': False},
         'annotations': {'readOnlyHint': True, 'destructiveHint': False, 'idempotentHint': False, 'openWorldHint': False}},
        {'name': 'execute_ui_action',
         'description': 'Directly dispatch one currently exposed semantic UI action in explicitly enabled agent-control mode. This is not human confirmation and does not approve supervised controls. Use the exact action ID, epoch, revision and single-use capability from a fresh read_ui_state, with a new command ID. The capability expires after 120 seconds and any UI revision change invalidates it sooner. Kneel/stand may spend time units and trigger reactions. Menu cancel uses the ordinary handler: New Battle can save its skirmish preset, and Options reloads persisted settings and restores volume/input capture. A receipt means handler dispatch, never success; inspect its separate post-action observation and obtain a fresh read before another action. Exact duplicates only query the receipt; conflicting command IDs fail. Never automatically retry after uncertainty. An adapter_error with status outcome_unknown means a mutating send was attempted but no trustworthy native receipt arrived; stop and reconcile manually.',
         'inputSchema': {'type': 'object', 'properties': {
             'command_id': dict(opaque), 'epoch': dict(opaque), 'capability': dict(opaque),
             'revision': {'type': 'integer', 'minimum': 0, 'maximum': MAX_COUNTER},
             'action': {'type': 'string', 'enum': list(ACTIONS)}},
             'required': ['command_id', 'epoch', 'capability', 'revision', 'action'], 'additionalProperties': False},
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
        self.capability_deadline = 0.0
        self.event_sequence = 0
        self.commands = {}
        self.flight = threading.Lock()

    def close(self):
        channel, self.channel = self.channel, None
        self.state = 'CLOSED'
        self.capability = None
        self.available_actions = frozenset()
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
                obj = validate_state(parse(self.exchange(('R ' + challenge).encode('ascii'))), challenge, self.epoch)
                require(obj['revision'] >= (self.revision if self.revision is not None else 0)
                        and obj['event_sequence'] >= self.event_sequence)
                require(obj['capability'] is None or obj['capability'] != old_capability)
                self.revision, self.event_sequence = obj['revision'], obj['event_sequence']
                self.capability = obj['capability']
                if self.capability is not None:
                    self.last_capability = self.capability
                self.available_actions = frozenset(action['id'] for action in obj['actions'])
                self.capability_deadline = time.monotonic() + CAPABILITY_TTL
                self.state = 'READY' if self.capability is not None else 'NEED_CONTEXT'
                return obj
            except FAILURES:
                self.close()
                raise Unavailable('Unavailable') from None

    def execute(self, command_id, epoch, capability, revision, action):
        arguments = dict(command_id=command_id, epoch=epoch, capability=capability, revision=revision, action=action)
        validate_action_arguments(arguments)
        with self.exclusive():
            previous = self.commands.get(command_id)
            if previous is not None:
                require(previous['arguments'] == arguments)
                return self._read_result(command_id)
            require(self.state == 'READY' and epoch == self.epoch and capability == self.capability
                    and revision == self.revision and action in self.available_actions
                    and time.monotonic() < self.capability_deadline)
            if len(self.commands) >= MAX_COMMANDS:
                self.close()
                raise Unavailable('Unavailable')
            request = f'E {command_id} {epoch} {capability} {revision} {action}'.encode('ascii')
            # Spend the grant and reserve the ID before the only send attempt.
            self.capability = None
            self.available_actions = frozenset()
            self.state = 'PENDING'
            self.commands[command_id] = {'arguments': arguments, 'result': None}
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
