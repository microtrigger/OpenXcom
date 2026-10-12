# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-11.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Experimental local-UI-confirmed stance/selection requests; never an approval API.

Only ordinary presented panel pixels are observed. A dispatch receipt is not
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
STATUS_SCHEMA = 'experimental-stance-status/1'
PUBLIC_CONTEXT_SCHEMA = 'openxcom-mcp-stance-context/1'
PUBLIC_STATUS_SCHEMA = 'openxcom-mcp-stance-status/1'
MAX_PACKET = 128 * 1024
MAX_COMMANDS = 64
WIDTH, HEIGHT = 320, 56
RGB_BYTES = WIDTH * HEIGHT * 3
TIMEOUT, STARTUP_TIMEOUT = 2.0, 31.0
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


class StanceClient:
    """Finite fail-closed pilot. Command IDs have a bounded, non-evicting ledger.

    NEW -> READY -> PENDING -> NEED_CONTEXT -> READY. An immediate native
    rejection also needs a fresh context. Any failed observation or uncertain
    exchange enters CLOSED; only an explicit new launcher can create a client.
    Stance and selection share one capability and command ledger. Duplicate
    explicit request calls query S, never replay D or N.
    """
    def __init__(self, channel, timeout=TIMEOUT, startup_timeout=STARTUP_TIMEOUT,
                 selection_enabled=False):
        require(type(selection_enabled) is bool)
        for value, maximum in ((timeout, TIMEOUT), (startup_timeout, STARTUP_TIMEOUT)):
            require(type(value) in (int, float) and math.isfinite(value) and 0 < value <= maximum)
        require(isinstance(channel, socket.socket) and channel.family == socket.AF_UNIX
                and channel.type == socket.SOCK_SEQPACKET)
        require(channel.getsockname() in ('', b'') and channel.getpeername() in ('', b''))
        channel.setblocking(False)
        self.channel = channel
        self.timeout, self.startup_timeout = timeout, startup_timeout
        self.selection_enabled = selection_enabled
        self.state = 'NEW'
        self.control_epoch = self.capability = self.active = None
        self.commands = {}
        self.flight = threading.Lock()

    def close(self):
        channel, self.channel = self.channel, None
        self.state = 'CLOSED'
        self.capability = None
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

    def exchange(self, request, maximum):
        # Caller closes the channel on ANY failure; send is attempted once only.
        require(self.channel is not None)
        require(not select.select([self.channel], [], [], 0)[0])
        deadline = time.monotonic() + self.timeout
        require(time.monotonic() < deadline and self.channel.send(request) == len(request))
        raw = self.packet(deadline, maximum)
        require(time.monotonic() < deadline)
        require(not select.select([self.channel], [], [], 0)[0])
        return raw

    def observe(self):
        with self.exclusive():
            # Do not replace context while the human confirmation is pending.
            require(self.state in ('NEW', 'READY', 'NEED_CONTEXT'))
            try:
                if self.state == 'NEW':
                    self.control_epoch = validate_hello(
                        self.packet(time.monotonic() + self.startup_timeout, 256))
                challenge = secrets.token_hex(16)
                old_capability = self.capability
                self.capability = None
                obj, rgb = validate_context(self.exchange(b'O' + challenge.encode('ascii'), MAX_PACKET),
                                            challenge)
                # O is a new one-use capability even when the displayed pixels match.
                require(obj['capability'] != old_capability)
                # The game may have changed control since the startup handshake.
                self.control_epoch = obj['control_epoch']
                self.capability = obj['capability']
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

    def _request(self, action, arguments, payload):
        capability, control_epoch, command_id = (arguments[name] for name in
                                                ('capability', 'control_epoch', 'command_id'))
        with self.exclusive():
            previous = self.commands.get(command_id)
            if previous is not None:
                require(previous['action'] == action and previous['arguments'] == arguments)
                return self._status(command_id)
            require(self.state == 'READY' and capability == self.capability
                    and control_epoch == self.control_epoch)
            if len(self.commands) >= MAX_COMMANDS:
                self.close()
                raise Unavailable('Unavailable')
            request = (action + control_epoch + capability + command_id + payload).encode('ascii')
            require(len(request) == 99)
            # Consume before the send. No exception can make it eligible again.
            self.capability = None
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
        return obj
