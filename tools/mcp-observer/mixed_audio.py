# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Bounded application postmix windows. No physical-audibility or video claim."""
import base64
import math
import re
import secrets
import select
import socket
import struct
import threading
import time

from adapter import strict_json
from snapshots import Unavailable, exact, integer, require

HELLO = b'OPENXCOM-MIXED-AUDIO/1 '
SCHEMA = 'experimental-mixed-audio/1'
PUBLIC_SCHEMA = 'openxcom-mcp-mixed-audio/1'
SEMANTICS = 'post_admission_mixed_output'
PROFILE = 'sdl-mixer-postmix-s16le-stereo-22050'
SAMPLE_RATE = 22050
CHANNELS = 2
FRAMES = 11025
PCM_BYTES = FRAMES * CHANNELS * 2
WAV_BYTES = PCM_BYTES + 44
BASE64_CHARACTERS = ((WAV_BYTES + 2) // 3) * 4
MAX_PACKET = 102400
CAPTURE_TIMEOUT = 2.0
STARTUP_TIMEOUT = 31.0
KEYS = ('schema epoch challenge next_token semantics profile duration_ms '
        'sample_rate channels sample_format frames pcm_hex')


def token(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{32}', value) is not None)


def validate(raw, epoch, challenge, admission):
    """Reject all unrecognized fields, profiles and representations before decode."""
    require(type(raw) is bytes and 0 < len(raw) <= MAX_PACKET)
    try:
        obj = strict_json(raw.decode('utf-8'))
        exact(obj, KEYS)
        require(obj['schema'] == SCHEMA and obj['epoch'] == epoch
                and obj['challenge'] == challenge and obj['semantics'] == SEMANTICS
                and obj['profile'] == PROFILE and obj['sample_format'] == 's16le')
        token(obj['epoch']); token(obj['challenge']); token(obj['next_token'])
        require(obj['next_token'] != admission)
        for name, value in (('duration_ms', 500), ('sample_rate', SAMPLE_RATE),
                            ('channels', CHANNELS), ('frames', FRAMES)):
            integer(obj[name], value, value)
        text = obj['pcm_hex']
        require(type(text) is str and len(text) == PCM_BYTES * 2
                and re.fullmatch('[0-9a-f]+', text) is not None)
        pcm = bytes.fromhex(text)
        require(len(pcm) == PCM_BYTES)
        return obj, pcm
    except (ValueError, UnicodeError, RecursionError, TypeError):
        raise Unavailable('Observation unavailable') from None


def encode_wav(pcm):
    """Canonical 44-byte PCM WAV header followed by unchanged interleaved L/R."""
    require(type(pcm) is bytes and len(pcm) == PCM_BYTES)
    header = struct.pack('<4sI4s4sIHHIIHH4sI',
                         b'RIFF', WAV_BYTES - 8, b'WAVE', b'fmt ', 16, 1,
                         CHANNELS, SAMPLE_RATE, SAMPLE_RATE * CHANNELS * 2,
                         CHANNELS * 2, 16, b'data', PCM_BYTES)
    return header + pcm


def audio_content(pcm):
    return {'type': 'audio', 'mimeType': 'audio/wav',
            'data': base64.b64encode(encode_wav(pcm)).decode('ascii')}


class MixedAudioClient:
    def __init__(self, channel, timeout=CAPTURE_TIMEOUT, startup_timeout=STARTUP_TIMEOUT):
        for value, maximum in ((timeout, CAPTURE_TIMEOUT), (startup_timeout, STARTUP_TIMEOUT)):
            require(type(value) in (int, float) and math.isfinite(value) and 0 < value <= maximum)
        require(isinstance(channel, socket.socket) and channel.family == socket.AF_UNIX
                and channel.type == socket.SOCK_SEQPACKET)
        # Only an inherited anonymous connected socketpair; no named endpoint.
        require(channel.getsockname() in ('', b'') and channel.getpeername() in ('', b''))
        channel.setblocking(False)
        self.channel = channel
        self.timeout = timeout
        self.startup_timeout = startup_timeout
        self.epoch = self.admission = None
        self.next_allowed = 0.0
        self.flight = threading.Lock()

    def close(self):
        channel, self.channel = self.channel, None
        if channel is not None:
            channel.close()

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

    def observe(self):
        acquired = self.flight.acquire(blocking=False)
        try:
            channel = self.channel
            require(acquired and channel is not None)
            require(time.monotonic() >= self.next_allowed)
            if self.epoch is None:
                # Resource loading gets a separate bounded readiness allowance.
                # Native BOOTSTRAP is capped at 30s from its own construction.
                hello = self.packet(time.monotonic() + self.startup_timeout, len(HELLO) + 64)
                require(hello.startswith(HELLO) and len(hello) == len(HELLO) + 64)
                self.epoch = hello[-64:-32].decode('ascii')
                self.admission = hello[-32:].decode('ascii')
                token(self.epoch); token(self.admission)
            require(not select.select([channel], [], [], 0)[0])
            challenge = secrets.token_hex(16)
            request = (self.epoch + self.admission + challenge).encode('ascii')
            deadline = time.monotonic() + self.timeout
            require(time.monotonic() < deadline and channel.send(request) == 96)
            raw = self.packet(deadline, MAX_PACKET)
            obj, pcm = validate(raw, self.epoch, challenge, self.admission)
            require(time.monotonic() < deadline)
            # A queued extra packet (including a second hello) is never history.
            require(not select.select([channel], [], [], 0)[0])
            self.admission = obj['next_token']
            # Conservatively wait a full second after accepting a result. This
            # also enforces the native minimum of one second between admissions.
            self.next_allowed = time.monotonic() + 1.0
            return obj, pcm
        except (OSError, ValueError, UnicodeError, RecursionError, TypeError):
            self.close()
            raise Unavailable('Observation unavailable') from None
        finally:
            if acquired:
                self.flight.release()
