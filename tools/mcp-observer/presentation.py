# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""One-flight, transient presentation acknowledgements; never current world state."""
import json
import secrets
import select
import socket
import time
from adapter import strict_json
from snapshots import Unavailable, require, exact, validate, PROFILE, MAX_SNAPSHOT


def token(value):
    import re
    require(type(value) is str and re.fullmatch('[0-9a-f]{32}', value) is not None)


class PresentationClient:
    def __init__(self, channel, timeout=1.0):
        require(type(timeout) in (int, float) and 0 < timeout <= 2.0)
        require(channel.family == socket.AF_UNIX and channel.type == socket.SOCK_SEQPACKET)
        channel.getpeername()
        channel.setblocking(False)
        self.channel = channel
        self.timeout = timeout
        self.epoch = self.admission = None
        self.busy = False
        self.next_allowed = 0.0

    def close(self):
        if self.channel is not None:
            self.channel.close()
        self.channel = None

    def packet(self, deadline, maximum):
        require(self.channel is not None)
        remaining = deadline - time.monotonic()
        require(remaining > 0)
        ready, _, _ = select.select([self.channel], [], [], remaining)
        require(bool(ready) and time.monotonic() < deadline)
        raw, _, flags, _ = self.channel.recvmsg(maximum + 1)
        require(bool(raw) and len(raw) <= maximum and not flags & socket.MSG_TRUNC)
        return raw

    def observe(self):
        try:
            require(not self.busy and self.channel is not None)
            self.busy = True
            # Fixed maximum four requests/second; rejected calls do not produce frames.
            require(time.monotonic() >= self.next_allowed)
            deadline = time.monotonic() + self.timeout
            if self.epoch is None:
                hello = self.packet(deadline, 88).decode('ascii')
                require(hello.startswith('OPENXCOM-PRESENTATION/1 ') and len(hello) == 88)
                self.epoch, self.admission = hello[-64:-32], hello[-32:]
                token(self.epoch); token(self.admission)
            # No unsolicited or leftover reply is admissible.
            require(not select.select([self.channel], [], [], 0)[0])
            challenge = secrets.token_hex(16)
            request = (self.epoch + self.admission + challenge).encode('ascii')
            require(time.monotonic() < deadline and self.channel.send(request) == 96)
            raw = self.packet(deadline, MAX_SNAPSHOT)
            obj = strict_json(raw.decode('utf-8'))
            exact(obj, 'schema epoch challenge next_token semantics profile crop')
            require(obj['schema'] == 'experimental-presentation/1' and obj['epoch'] == self.epoch
                    and obj['challenge'] == challenge and obj['semantics'] == 'presented_after_request'
                    and obj['profile'] == PROFILE)
            token(obj['next_token']); require(obj['next_token'] != self.admission)
            # Reuse strict public geometry/pixel validation, never its historical metadata.
            candidate = {'schema': 'experimental-presented-cursor/0', 'session': self.epoch,
                         'semantics': 'last_observation', 'freshness': 'unverified',
                         'profile': PROFILE, 'unavailable_or_unsupported': False,
                         'public_revision': 0, 'crop': obj['crop']}
            validate(json.dumps(candidate, separators=(',', ':')).encode(), 'cursor')
            require(time.monotonic() < deadline)
            self.admission = obj['next_token']
            self.next_allowed = time.monotonic() + 0.25
            return obj, candidate
        except (OSError, ValueError, UnicodeError, RecursionError, TypeError):
            self.close()
            raise Unavailable('Observation unavailable') from None
        finally:
            self.busy = False
