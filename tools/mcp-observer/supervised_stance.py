# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-11.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Experimental local-UI-confirmed controls; never an approval API.

Only ordinary presented panel or tactical pixels are observed. A dispatch receipt is not
success. After each terminal command, a new context must be displayed before
another request. No transport failure, timeout or malformed response is retried.
"""
import base64
from contextlib import contextmanager
import math
import re
import secrets
import select
import socket
import struct
import threading
import time
import zlib

from adapter import strict_json
from snapshots import Unavailable, exact, integer, require

HELLO_SCHEMA = 'experimental-stance/1'
CONTEXT_SCHEMA = 'experimental-stance-context/1'
TACTICAL_CONTEXT_SCHEMA = 'experimental-tactical-context/1'
INVENTORY_VIEW_SCHEMA = 'experimental-inventory-view/1'
INVENTORY_CLOSE_CONTEXT_SCHEMA = 'experimental-inventory-close-context/1'
INVENTORY_GROUND_CONTEXT_SCHEMA = 'experimental-inventory-ground-context/1'
STATUS_SCHEMA = 'experimental-stance-status/1'
PUBLIC_CONTEXT_SCHEMA = 'openxcom-mcp-stance-context/1'
PUBLIC_TACTICAL_CONTEXT_SCHEMA = 'openxcom-mcp-tactical-context/1'
PUBLIC_INVENTORY_VIEW_SCHEMA = 'openxcom-mcp-inventory-view/1'
PUBLIC_INVENTORY_CLOSE_CONTEXT_SCHEMA = 'openxcom-mcp-inventory-close-context/1'
PUBLIC_INVENTORY_GROUND_CONTEXT_SCHEMA = 'openxcom-mcp-inventory-ground-context/1'
PUBLIC_STATUS_SCHEMA = 'openxcom-mcp-stance-status/1'
MAX_PACKET = 128 * 1024
MAX_COMMANDS = 64
WIDTH, HEIGHT = 320, 56
RGB_BYTES = WIDTH * HEIGHT * 3
TACTICAL_WIDTH, TACTICAL_HEIGHT = 320, 200
TACTICAL_PIXELS = TACTICAL_WIDTH * TACTICAL_HEIGHT
TACTICAL_RGB_BYTES = TACTICAL_PIXELS * 3
PALETTE_BYTES = 256 * 3
TIMEOUT, STARTUP_TIMEOUT = 2.0, 31.0
# Only a verified dispatched left-click can select this observation deadline.
# The native producer permits at most eight seconds of ordinary settling.
MAP_LEFT_CLICK_VIEW_TIMEOUT = 10.0
STANCES = {'kneel': 'k', 'stand': 's'}
DIRECTIONS = {'next': 'n', 'previous': 'p'}
STATUSES = ('pending_confirmation', 'dispatched', 'unavailable')
FAILURES = (OSError, ValueError, UnicodeError, RecursionError, TypeError)


def token(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{32}', value) is not None)


def parse(raw, maximum=MAX_PACKET):
    require(type(raw) is bytes and 0 < len(raw) <= maximum)
    try:
        return strict_json(raw.decode('utf-8'))
    except (ValueError, UnicodeError, RecursionError, TypeError):
        raise Unavailable('Unavailable') from None


def validate_hello(raw):
    obj = parse(raw, 256)
    exact(obj, 'schema control_epoch')
    require(obj['schema'] == HELLO_SCHEMA)
    token(obj['control_epoch'])
    return obj['control_epoch']


def validate_context(raw, challenge):
    obj = parse(raw)
    exact(obj, 'schema challenge control_epoch capability semantics panel')
    require(obj['schema'] == CONTEXT_SCHEMA and obj['challenge'] == challenge
            and obj['semantics'] == 'presented_after_request')
    for name in ('challenge', 'control_epoch', 'capability'):
        token(obj[name])
    exact(obj['panel'], 'width height rgb_hex')
    integer(obj['panel']['width'], WIDTH, WIDTH)
    integer(obj['panel']['height'], HEIGHT, HEIGHT)
    pixels = obj['panel']['rgb_hex']
    require(type(pixels) is str and len(pixels) == RGB_BYTES * 2
            and re.fullmatch('[0-9a-f]+', pixels) is not None)
    return obj, bytes.fromhex(pixels)


def validate_tactical_context(raw, challenge):
    """Accept only canonical first-seen indexing of the actual displayed RGB.

    Index aliases and unused palette entries cannot carry an extra data channel.
    The transport palette is discarded after lossless reconstruction of RGB24.
    """
    obj = parse(raw)
    exact(obj, 'schema challenge control_epoch capability semantics view')
    require(obj['schema'] == TACTICAL_CONTEXT_SCHEMA and obj['challenge'] == challenge
            and obj['semantics'] == 'presented_after_request')
    for name in ('challenge', 'control_epoch', 'capability'):
        token(obj[name])
    return obj, validate_indexed_view(obj['view'])


def validate_inventory_close_context(raw, challenge):
    obj = parse(raw)
    exact(obj, 'schema challenge control_epoch capability semantics view')
    require(obj['schema'] == INVENTORY_CLOSE_CONTEXT_SCHEMA and obj['challenge'] == challenge
            and obj['semantics'] == 'presented_after_request')
    for name in ('challenge', 'control_epoch', 'capability'):
        token(obj[name])
    return obj, validate_indexed_view(obj['view'])


def validate_inventory_ground_context(raw, challenge):
    obj = parse(raw)
    exact(obj, 'schema challenge control_epoch capability semantics view')
    require(obj['schema'] == INVENTORY_GROUND_CONTEXT_SCHEMA and obj['challenge'] == challenge
            and obj['semantics'] == 'presented_after_request')
    for name in ('challenge', 'control_epoch', 'capability'):
        token(obj[name])
    return obj, validate_indexed_view(obj['view'])


def validate_inventory_view(raw, challenge):
    obj = parse(raw)
    exact(obj, 'schema challenge semantics view')
    require(obj['schema'] == INVENTORY_VIEW_SCHEMA and obj['challenge'] == challenge
            and obj['semantics'] == 'presented_after_request')
    token(obj['challenge'])
    return obj, validate_indexed_view(obj['view'])


def validate_indexed_view(view):
    # Shared canonical visible-RGB rules. No palette aliases or hidden metadata.
    exact(view, 'width height indices_hex palette_hex')
    integer(view['width'], TACTICAL_WIDTH, TACTICAL_WIDTH)
    integer(view['height'], TACTICAL_HEIGHT, TACTICAL_HEIGHT)
    for name, length in (('indices_hex', TACTICAL_PIXELS * 2),
                         ('palette_hex', PALETTE_BYTES * 2)):
        require(type(view[name]) is str and len(view[name]) == length
                and re.fullmatch('[0-9a-f]+', view[name]) is not None)
    indices = bytes.fromhex(view['indices_hex'])
    palette = bytes.fromhex(view['palette_hex'])
    colors = tuple(palette[i:i + 3] for i in range(0, PALETTE_BYTES, 3))
    used, seen = 0, set()
    for index in indices:
        if index == used:
            require(colors[index] not in seen)
            seen.add(colors[index])
            used += 1
        else:
            require(index < used)
    require(palette[used * 3:] == bytes(PALETTE_BYTES - used * 3))
    rgb = b''.join(colors[index] for index in indices)
    require(len(rgb) == TACTICAL_RGB_BYTES)
    return rgb


def validate_status(raw, command_id):
    obj = parse(raw, 256)
    exact(obj, 'schema command_id status')
    require(obj['schema'] == STATUS_SCHEMA and obj['command_id'] == command_id
            and type(obj['status']) is str and obj['status'] in STATUSES)
    token(obj['command_id'])
    return obj


def validate_arguments(arguments):
    exact(arguments, 'capability control_epoch command_id observed desired')
    for name in ('capability', 'control_epoch', 'command_id'):
        token(arguments[name])
    for name in ('observed', 'desired'):
        require(type(arguments[name]) is str and arguments[name] in STANCES)


def validate_selection_arguments(arguments):
    exact(arguments, 'capability control_epoch command_id direction')
    for name in ('capability', 'control_epoch', 'command_id'):
        token(arguments[name])
    require(type(arguments['direction']) is str and arguments['direction'] in DIRECTIONS)


def validate_turn_arguments(arguments):
    exact(arguments, 'capability control_epoch command_id x y')
    for name in ('capability', 'control_epoch', 'command_id'):
        token(arguments[name])
    integer(arguments['x'], 16, 303)
    integer(arguments['y'], 40, 127)


def validate_map_left_click_arguments(arguments):
    validate_turn_arguments(arguments)


def validate_open_inventory_arguments(arguments):
    exact(arguments, 'capability control_epoch command_id')
    for name in ('capability', 'control_epoch', 'command_id'):
        token(arguments[name])


def validate_close_inventory_arguments(arguments):
    exact(arguments, 'capability control_epoch command_id')
    for name in ('capability', 'control_epoch', 'command_id'):
        token(arguments[name])


def validate_inventory_ground_arguments(arguments):
    exact(arguments, 'capability control_epoch command_id')
    for name in ('capability', 'control_epoch', 'command_id'):
        token(arguments[name])


def image_content(rgb):
    """Losslessly encode only the fixed displayed RGB panel, with no metadata."""
    require(type(rgb) is bytes and len(rgb) == RGB_BYTES)
    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload
                + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))
    rows = b''.join(b'\0' + rgb[i:i + WIDTH * 3]
                    for i in range(0, RGB_BYTES, WIDTH * 3))
    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
    require(len(png) <= RGB_BYTES + HEIGHT + 256)
    return {'type': 'image', 'mimeType': 'image/png',
            'data': base64.b64encode(png).decode('ascii')}


def tactical_image_content(rgb):
    """Encode only visible RGB as a canonical lossless indexed PNG.

    Rebuild the palette from validated RGB, never from an SDL palette. This
    bounds even index noise below the existing MCP response limit. Only fixed
    IHDR/PLTE/IDAT/IEND chunks are emitted, without ancillary metadata.
    """
    require(type(rgb) is bytes and len(rgb) == TACTICAL_RGB_BYTES)
    palette, indices, lookup = bytearray(), bytearray(), {}
    for offset in range(0, len(rgb), 3):
        color = rgb[offset:offset + 3]
        if color not in lookup:
            require(len(lookup) < 256)
            lookup[color] = len(lookup)
            palette.extend(color)
        indices.append(lookup[color])
    palette.extend(bytes(PALETTE_BYTES - len(palette)))
    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload
                + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))
    rows = b''.join(b'\0' + indices[i:i + TACTICAL_WIDTH]
                    for i in range(0, TACTICAL_PIXELS, TACTICAL_WIDTH))
    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', TACTICAL_WIDTH, TACTICAL_HEIGHT, 8, 3, 0, 0, 0))
           + chunk(b'PLTE', bytes(palette))
           + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
    require(len(png) <= TACTICAL_PIXELS + TACTICAL_HEIGHT + PALETTE_BYTES + 256)
    return {'type': 'image', 'mimeType': 'image/png',
            'data': base64.b64encode(png).decode('ascii')}


class StanceClient:
    """Finite fail-closed pilot. Command IDs have a bounded, non-evicting ledger.

    NEW -> READY -> PENDING -> NEED_CONTEXT -> READY. An immediate native
    rejection also needs a fresh context. Any failed observation or uncertain
    exchange enters CLOSED; only an explicit new launcher can create a client.
    Stance and selection use panel capabilities; map clicks use tactical
    capabilities. Both observations replace the previous grant. All commands
    share one ledger. Inventory I is capless and retires any previous grant.
    A dispatched inventory open requires I before any other shared observation
    or new action. Separate close context J authorizes only C; dispatched C
    requires a fresh V. Separate ground context K authorizes only G; dispatched G
    requires capless I. Duplicate calls query S, never replay D, N, R, L, B, C or G.
    """
    def __init__(self, channel, timeout=TIMEOUT, startup_timeout=STARTUP_TIMEOUT,
                 selection_enabled=False, turn_enabled=False, map_left_click_enabled=False,
                 inventory_enabled=False, inventory_close_enabled=False, inventory_ground_enabled=False):
        require(type(selection_enabled) is bool and type(turn_enabled) is bool
                and type(map_left_click_enabled) is bool and type(inventory_enabled) is bool
                and type(inventory_close_enabled) is bool and type(inventory_ground_enabled) is bool)
        require(not map_left_click_enabled or turn_enabled)
        require(not inventory_enabled or turn_enabled)
        require(not inventory_close_enabled or inventory_enabled)
        require(not inventory_ground_enabled or inventory_close_enabled)
        for value, maximum in ((timeout, TIMEOUT), (startup_timeout, STARTUP_TIMEOUT)):
            require(type(value) in (int, float) and math.isfinite(value) and 0 < value <= maximum)
        require(isinstance(channel, socket.socket) and channel.family == socket.AF_UNIX
                and channel.type == socket.SOCK_SEQPACKET)
        require(channel.getsockname() in ('', b'') and channel.getpeername() in ('', b''))
        channel.setblocking(False)
        self.channel = channel
        self.timeout, self.startup_timeout = timeout, startup_timeout
        self.selection_enabled = selection_enabled
        self.turn_enabled = turn_enabled
        self.map_left_click_enabled = map_left_click_enabled
        self.inventory_enabled = inventory_enabled
        self.inventory_close_enabled = inventory_close_enabled
        self.inventory_ground_enabled = inventory_ground_enabled
        self.state = 'NEW'
        self.control_epoch = self.capability = self.active = None
        self.capability_kind = None
        self.tactical_verification_required = False
        self.inventory_verification_required = False
        self.commands = {}
        self.flight = threading.Lock()

    def close(self):
        channel, self.channel = self.channel, None
        self.state = 'CLOSED'
        self.capability = None
        self.capability_kind = None
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

    def packet(self, deadline, maximum):
        channel = self.channel
        require(channel is not None)
        remaining = deadline - time.monotonic()
        require(remaining > 0)
        ready, _, _ = select.select([channel], [], [], remaining)
        require(bool(ready) and time.monotonic() < deadline)
        raw, ancillary, flags, _ = channel.recvmsg(maximum + 1)
        require(bool(raw) and len(raw) <= maximum and not ancillary
                and not flags & (socket.MSG_TRUNC | socket.MSG_CTRUNC))
        return raw

    def exchange(self, request, maximum, timeout=None):
        # Caller closes the channel on ANY failure; send is attempted once only.
        require(self.channel is not None)
        require(not select.select([self.channel], [], [], 0)[0])
        deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
        require(time.monotonic() < deadline and self.channel.send(request) == len(request))
        raw = self.packet(deadline, maximum)
        require(time.monotonic() < deadline)
        require(not select.select([self.channel], [], [], 0)[0])
        return raw

    def observe(self):
        return self._observe('O', validate_context)

    def observe_tactical(self):
        require(self.turn_enabled)
        return self._observe('V', validate_tactical_context)

    def observe_inventory(self):
        require(self.inventory_enabled)
        return self._observe('I', validate_inventory_view)

    def observe_inventory_close(self):
        require(self.inventory_close_enabled)
        return self._observe('J', validate_inventory_close_context)

    def observe_inventory_ground(self):
        require(self.inventory_ground_enabled)
        return self._observe('K', validate_inventory_ground_context)

    def _observe(self, action, validate):
        with self.exclusive():
            # Do not replace context while the human confirmation is pending.
            require(self.state in ('NEW', 'READY', 'NEED_CONTEXT'))
            require(action == 'V' or not self.tactical_verification_required)
            require(action == 'I' or not self.inventory_verification_required)
            try:
                if self.state == 'NEW':
                    self.control_epoch = validate_hello(
                        self.packet(time.monotonic() + self.startup_timeout, 256))
                challenge = secrets.token_hex(16)
                old_capability = self.capability
                self.capability = None
                self.capability_kind = None
                # Longer waiting never applies to initial views, actions, status,
                # R, cancellation, or an old duplicate dispatched receipt.
                timeout = (MAP_LEFT_CLICK_VIEW_TIMEOUT
                           if action == 'V' and self.tactical_verification_required
                           and self.commands[self.active]['action'] == 'L' else self.timeout)
                obj, rgb = validate(self.exchange((action + challenge).encode('ascii'), MAX_PACKET,
                                                 timeout=timeout),
                                    challenge)
                if action == 'I':
                    # Read-only inventory pixels never authorize any control action.
                    self.inventory_verification_required = False
                    self.state = 'NEED_CONTEXT'
                    return obj, rgb
                # Each O/V/J/K observation grants a new one-use, action-bound capability.
                require(obj['capability'] != old_capability)
                # The game may have changed control since the startup handshake.
                self.control_epoch = obj['control_epoch']
                self.capability = obj['capability']
                self.capability_kind = action
                self.tactical_verification_required = False
                self.state = 'READY'
                return obj, rgb
            except FAILURES:
                self.close()
                raise Unavailable('Unavailable') from None

    def request(self, capability, control_epoch, command_id, observed, desired):
        arguments = dict(capability=capability, control_epoch=control_epoch,
                         command_id=command_id, observed=observed, desired=desired)
        validate_arguments(arguments)
        return self._request('D', arguments, STANCES[observed] + STANCES[desired])

    def request_selection(self, capability, control_epoch, command_id, direction):
        require(self.selection_enabled)
        arguments = dict(capability=capability, control_epoch=control_epoch,
                         command_id=command_id, direction=direction)
        validate_selection_arguments(arguments)
        return self._request('N', arguments, DIRECTIONS[direction] + '0')

    def request_turn(self, capability, control_epoch, command_id, x, y):
        require(self.turn_enabled)
        arguments = dict(capability=capability, control_epoch=control_epoch,
                         command_id=command_id, x=x, y=y)
        validate_turn_arguments(arguments)
        return self._request('R', arguments, f'{x:03d}{y:03d}')

    def request_map_left_click(self, capability, control_epoch, command_id, x, y):
        require(self.map_left_click_enabled)
        arguments = dict(capability=capability, control_epoch=control_epoch,
                         command_id=command_id, x=x, y=y)
        validate_map_left_click_arguments(arguments)
        return self._request('L', arguments, f'{x:03d}{y:03d}')

    def request_open_inventory(self, capability, control_epoch, command_id):
        require(self.inventory_enabled)
        arguments = dict(capability=capability, control_epoch=control_epoch, command_id=command_id)
        validate_open_inventory_arguments(arguments)
        return self._request('B', arguments, 'o0')

    def request_close_inventory(self, capability, control_epoch, command_id):
        require(self.inventory_close_enabled)
        arguments = dict(capability=capability, control_epoch=control_epoch, command_id=command_id)
        validate_close_inventory_arguments(arguments)
        return self._request('C', arguments, '')

    def request_inventory_ground(self, capability, control_epoch, command_id):
        require(self.inventory_ground_enabled)
        arguments = dict(capability=capability, control_epoch=control_epoch, command_id=command_id)
        validate_inventory_ground_arguments(arguments)
        return self._request('G', arguments, '')

    def _request(self, action, arguments, payload):
        capability, control_epoch, command_id = (arguments[name] for name in
                                                ('capability', 'control_epoch', 'command_id'))
        with self.exclusive():
            previous = self.commands.get(command_id)
            if previous is not None:
                require(previous['action'] == action and previous['arguments'] == arguments)
                return self._status(command_id)
            require(self.state == 'READY' and capability == self.capability
                    and control_epoch == self.control_epoch
                    and not self.inventory_verification_required and not self.tactical_verification_required
                    and self.capability_kind == ('K' if action == 'G' else 'J' if action == 'C' else
                                                 'V' if action in ('R', 'L', 'B') else 'O'))
            if len(self.commands) >= MAX_COMMANDS:
                self.close()
                raise Unavailable('Unavailable')
            request = (action + control_epoch + capability + command_id + payload).encode('ascii')
            require(len(request) == (97 if action in ('C', 'G') else 103 if action in ('R', 'L') else 99))
            # Consume before the send. No exception can make it eligible again.
            self.capability = None
            self.capability_kind = None
            self.active = command_id
            self.state = 'PENDING'
            self.commands[command_id] = {'action': action, 'arguments': arguments, 'status': None}
            try:
                obj = validate_status(self.exchange(request, 256), command_id)
                require(obj['status'] != 'dispatched')
                return self._accept_status(obj)
            except FAILURES:
                self.close()
                raise Unavailable('Unavailable') from None

    def read_status(self, command_id):
        token(command_id)
        with self.exclusive():
            require(command_id in self.commands)
            return self._status(command_id)

    def _status(self, command_id):
        try:
            obj = validate_status(self.exchange(b'S' + command_id.encode('ascii'), 256), command_id)
            return self._accept_status(obj)
        except FAILURES:
            self.close()
            raise Unavailable('Unavailable') from None

    def _accept_status(self, obj):
        command_id, status = obj['command_id'], obj['status']
        previous = self.commands[command_id]['status']
        require(previous in (None, 'pending_confirmation') or previous == status)
        self.commands[command_id]['status'] = status
        if command_id == self.active and self.state == 'PENDING' and status != 'pending_confirmation':
            self.state = 'NEED_CONTEXT'
            self.inventory_verification_required = (status == 'dispatched'
                                                    and self.commands[command_id]['action'] in ('B', 'G'))
            self.tactical_verification_required = (status == 'dispatched'
                                                   and self.commands[command_id]['action'] in ('R', 'L', 'C'))
        return obj
