# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# This file is free software: you may redistribute and/or modify it under
# GNU GPL version 3 or, at your option, any later version.
# Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
"""Strict historical snapshot parsing/encoding behind fixed-name MCP tools.
No function here certifies current state. A held writer lock is insufficient.
"""
import base64
import fcntl
import os
import re
import stat
import struct
import zlib
from adapter import strict_json

MAX_SNAPSHOT = 102400
PROFILE = 'software-single-buffer-8bit-integer-scale-no-bands'
FILES = {'hud': ('snapshot.json', 'observer.lock'), 'cursor': ('cursor.json', 'cursor.lock')}


class Unavailable(ValueError):
    def __init__(self, *args, failure_code=None):
        super().__init__(*args)
        self.failure_code = failure_code


def require(ok):
    if not ok:
        raise Unavailable('Observation unavailable')


def exact(obj, keys):
    require(type(obj) is dict and set(obj) == set(keys.split()))


def integer(n, low, high):
    require(type(n) is int and low <= n <= high)


def validate(raw, kind):
    require(kind in FILES and len(raw) <= MAX_SNAPSHOT)
    try:
        obj = strict_json(raw.decode('utf-8'))
    except (ValueError, UnicodeError, RecursionError):
        raise Unavailable('Observation unavailable') from None
    common = 'schema session semantics freshness public_revision '
    exact(obj, common + ('observation' if kind == 'hud' else 'profile unavailable_or_unsupported crop'))
    require(obj['schema'] == ('experimental-public-hud/0' if kind == 'hud' else 'experimental-presented-cursor/0'))
    require(type(obj['session']) is str and re.fullmatch('[0-9a-f]{32}', obj['session']) is not None)
    require(obj['semantics'] == 'last_observation' and obj['freshness'] == 'unverified')
    integer(obj['public_revision'], 0, 2**64 - 1)
    if kind == 'hud':
        obs = obj['observation']
        require(type(obs) is dict and type(obs.get('sampled')) is bool)
        exact(obs, 'sampled selected_hud' if obs['sampled'] else 'sampled')
        if obs['sampled'] and obs['selected_hud'] is not None:
            exact(obs['selected_hud'], 'time_units energy health morale')
            for n in obs['selected_hud'].values():
                integer(n, 0, 2**32-1)
    else:
        require(obj['profile'] == PROFILE and type(obj['unavailable_or_unsupported']) is bool)
        require(obj['unavailable_or_unsupported'] == (obj['crop'] is None))
        if obj['crop'] is not None:
            c = obj['crop']
            exact(c, 'screen_width screen_height x y width height format pixels')
            for key in ('screen_width', 'screen_height'):
                integer(c[key], 1, 4096)
            for key in ('width', 'height'):
                integer(c[key], 1, 128)
            integer(c['x'], 0, c['screen_width'] - c['width'])
            integer(c['y'], 0, c['screen_height'] - c['height'])
            require(c['format'] == 'rgb24-hex-row-major' and type(c['pixels']) is str)
            require(len(c['pixels']) == c['width'] * c['height'] * 6)
            require(re.fullmatch('[0-9a-f]+', c['pixels']) is not None)
    return obj


def private_file(fd):
    s = os.fstat(fd)
    require(stat.S_ISREG(s.st_mode) and s.st_uid == os.geteuid()
            and stat.S_IMODE(s.st_mode) == 0o600 and s.st_nlink == 1)
    return s


def private_directory(path):
    # Resolve every component without following symlinks, then pin the leaf fd.
    require(type(path) is str and path.startswith('/') and '\x00' not in path)
    components = path.split('/')[1:]
    require(bool(components) and all(p not in ('', '.', '..') for p in components))
    fd = os.open('/', os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC)
    try:
        for component in components:
            nxt = os.open(component, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | os.O_CLOEXEC, dir_fd=fd)
            os.close(fd)
            fd = nxt
        s = os.fstat(fd)
        require(s.st_uid == os.geteuid() and stat.S_IMODE(s.st_mode) == 0o700)
        return fd
    except BaseException:
        os.close(fd)
        raise


def read_candidate(path, kind):
    """Read validated *historical* candidate with conflicting-lock check.
    This cannot verify engine ownership, session binding or frames.
    """
    require(kind in FILES)
    directory = lock = snapshot = None
    try:
        directory = private_directory(path)
        filename, lockname = FILES[kind]
        flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_CLOEXEC | os.O_NONBLOCK
        lock = os.open(lockname, flags, dir_fd=directory)
        ls = private_file(lock)
        try:
            fcntl.flock(lock, fcntl.LOCK_SH | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(lock, fcntl.LOCK_UN)
            raise Unavailable('Observation unavailable')
        snapshot = os.open(filename, flags, dir_fd=directory)
        before = private_file(snapshot)
        require(before.st_size <= MAX_SNAPSHOT)
        raw = bytearray()
        while len(raw) <= MAX_SNAPSHOT:
            chunk = os.read(snapshot, min(8192, MAX_SNAPSHOT + 1 - len(raw)))
            if not chunk:
                break
            raw.extend(chunk)
        after = private_file(snapshot)
        require(len(raw) <= MAX_SNAPSHOT and
                (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) ==
                (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns))
        current_lock = os.stat(lockname, dir_fd=directory, follow_symlinks=False)
        require((ls.st_dev, ls.st_ino) == (current_lock.st_dev, current_lock.st_ino))
        return validate(bytes(raw), kind)
    except (OSError, ValueError, OverflowError):
        raise Unavailable('Observation unavailable') from None
    finally:
        for fd in (snapshot, lock, directory):
            if fd is not None:
                os.close(fd)


def encode_png(snapshot):
    """Metadata-free exact RGB PNG; takes a validated cursor snapshot again."""
    import json
    obj = validate(json.dumps(snapshot, separators=(',', ':')).encode(), 'cursor')
    c = obj['crop']
    require(c is not None)
    pixels = bytes.fromhex(c['pixels'])
    width, height = c['width'], c['height']
    rows = b''.join(b'\0' + pixels[y*width*3:(y+1)*width*3] for y in range(height))
    def chunk(tag, payload):
        return struct.pack('>I', len(payload)) + tag + payload + struct.pack('>I', zlib.crc32(tag + payload))
    png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))
    require(len(png) <= 50000)
    return png


class CursorCache:
    """One image only. Revision regression/conflicts and old-session replay reject.
    Purely internal cache correctness, not freshness or authenticated session binding.
    """
    def __init__(self):
        self.last = None
        self.png = None
        self.sessions = set()

    def convert(self, raw):
        obj = validate(raw, 'cursor')
        key = (obj['session'], obj['public_revision'])
        if self.last is not None:
            oldkey = (self.last['session'], self.last['public_revision'])
            if key[0] == oldkey[0]:
                require(key[1] >= oldkey[1])
                if key[1] == oldkey[1]:
                    require(obj == self.last)
                    return self.png
            else:
                require(key[0] not in self.sessions and len(self.sessions) < 64)
        png = None if obj['crop'] is None else encode_png(obj)
        self.sessions.add(key[0])
        self.last, self.png = obj, png
        return png


def image_content(snapshot):
    """MCP image block containing only the validated historical RGB pixels."""
    return {'type': 'image', 'mimeType': 'image/png',
            'data': base64.b64encode(encode_png(snapshot)).decode('ascii')}
