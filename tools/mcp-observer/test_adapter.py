# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# This file is free software: you may redistribute and/or modify it under
# GNU GPL version 3 or, at your option, any later version.
# Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
import base64
import copy
import fcntl
import io
import json
import os
import random
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest.mock import patch
import adapter as a
import snapshots as s


def dump(obj):
    return json.dumps(obj, separators=(',', ':')).encode()


def fixture(kind='cursor', w=128, h=128, pixels=None):
    obj = dict(schema='experimental-presented-cursor/0' if kind == 'cursor' else 'experimental-public-hud/0',
               session='a'*32, semantics='last_observation', freshness='unverified', public_revision=1)
    if kind == 'hud':
        obj['observation'] = {'sampled': True, 'selected_hud': dict(time_units=58, energy=62, health=33, morale=100)}
    else:
        obj.update(profile=s.PROFILE, unavailable_or_unsupported=False, crop=dict(screen_width=960,
            screen_height=600, x=0, y=0, width=w, height=h, format='rgb24-hex-row-major',
            pixels=(pixels if pixels is not None else bytes(w*h*3)).hex()))
    return obj


def req(method, params=None, ident=1):
    return dict(jsonrpc='2.0', id=ident, method=method, params=params or {})


def ready(**kwargs):
    server = a.Server(**kwargs)
    server.handle(req('initialize', {'protocolVersion': a.PROTOCOL, 'capabilities': {},
                                    'clientInfo': {'name': 'fixture', 'version': '0'}}))
    server.handle({'jsonrpc':'2.0', 'method':'notifications/initialized'})
    return server


class ProtocolTests(unittest.TestCase):
    def test_handshake_and_lifecycle(self):
        server = a.Server()
        self.assertIn('error', server.handle(req('tools/list')))
        self.assertIn('error', server.handle(req('initialize')))
        result = server.handle(req('initialize', {'protocolVersion': 'future', 'capabilities': {},
                         'clientInfo': {'name':'test','version':'1'}}))
        self.assertEqual(result['result']['protocolVersion'], a.PROTOCOL)
        self.assertIn('error', server.handle(req('tools/list')))
        self.assertIsNone(server.handle({'jsonrpc':'2.0','method':'notifications/initialized'}))
        self.assertEqual(len(server.handle(req('tools/list'))['result']['tools']), 2)
        self.assertIn('error', server.handle(req('initialize')))
        self.assertEqual(server.handle(req('ping'))['result'], {})

    def test_current_is_fail_closed_without_io(self):
        server = ready()
        with patch('snapshots.read_candidate', side_effect=AssertionError('must not read')):
            for name in a.NAMES:
                out = server.handle(req('tools/call', {'name':name,'arguments':{'require_current':True}}))
                self.assertEqual(out['result'], a.UNAVAILABLE)

    def test_unknown_methods_arguments_paths(self):
        server = ready()
        for method in ('resources/read','resources/subscribe','shutdown','game/move','shell'):
            self.assertEqual(server.handle(req(method))['error']['code'], -32601)
        for args in ({'path':'/etc/passwd'}, {'require_current':1}, [], None):
            self.assertIn('error', server.handle(req('tools/call', {'name':a.NAMES[0],'arguments':args})))
        self.assertIn('error', server.handle(req('tools/call', {'name':'unknown'})))
        self.assertIn('error', server.handle(req('tools/list', {'cursor':'junk'})))

    def test_envelopes(self):
        server = ready()
        for msg in ([], {}, {'jsonrpc':'1.0','id':1,'method':'ping'}, req('ping', ident=True),
                    req('ping', ident=None), req('ping', ident=1.5)):
            self.assertIn('error', server.handle(msg))
        self.assertIsNone(server.handle({'jsonrpc':'2.0','method':'bogus'}))

    def test_wire_limits_parse_and_eof(self):
        for raw in (b'{bad}\n', b'\xff\n', b'{"a":1,"a":2}\n', b'NaN\n', b'['*2000+b'\n'):
            out = io.BytesIO(); a.serve(io.BytesIO(raw), out)
            self.assertEqual(json.loads(out.getvalue())['error']['code'], -32700)
        out=io.BytesIO(); a.serve(io.BytesIO(b'x'*(a.MAX_MESSAGE+1)), out)
        self.assertEqual(json.loads(out.getvalue())['error']['message'], 'Message too large')
        out=io.BytesIO(); a.serve(io.BytesIO(b''), out); self.assertEqual(out.getvalue(), b'')
        out=io.BytesIO(); a.serve(io.BytesIO(dump(req('ping'))+b'\n'), out)
        self.assertEqual(json.loads(out.getvalue())['result'], {})

    def test_subprocess_stdio(self):
        wire = dump(req('ping'))+b'\n'
        result = subprocess.run([sys.executable, 'adapter.py'], input=wire, capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 0); self.assertEqual(result.stderr, b'')
        self.assertEqual(json.loads(result.stdout)['result'], {})


class ValidationTests(unittest.TestCase):
    def test_good_and_unavailable_schemas(self):
        for kind in ('hud','cursor'):
            self.assertEqual(s.validate(dump(fixture(kind)), kind), fixture(kind))
        obj=fixture('hud'); obj['observation']={'sampled':False}; s.validate(dump(obj),'hud')
        obj=fixture(); obj.update(crop=None, unavailable_or_unsupported=True); s.validate(dump(obj),'cursor')

    def test_bad_fields(self):
        mutations = [('schema','other'), ('session','b'*31), ('public_revision', True),
                     ('public_revision', -1), ('public_revision', 2**64), ('freshness','verified'),
                     ('profile','other'), ('unavailable_or_unsupported',True), ('hidden_units',[])]
        for k,v in mutations:
            obj=fixture(); obj[k]=v
            with self.subTest(k=k,v=v), self.assertRaises(s.Unavailable): s.validate(dump(obj),'cursor')
        obj=fixture('hud'); obj['observation']['selected_hud']['health']=True
        with self.assertRaises(s.Unavailable): s.validate(dump(obj),'hud')

    def test_hud_unsigned_boundaries(self):
        for n in (0, 2**31, 2**32-1):
            obj=fixture('hud'); obj['observation']['selected_hud']['health']=n
            self.assertEqual(s.validate(dump(obj),'hud'),obj)
        for n in (-1, -(2**31), 2**32, True, 1.5):
            obj=fixture('hud'); obj['observation']['selected_hud']['health']=n
            with self.subTest(n=n), self.assertRaises(s.Unavailable):s.validate(dump(obj),'hud')

    def test_bad_crop(self):
        for k,v in [('width',129),('height',True),('screen_width',4097),('x',-1),('y',599),
                    ('pixels','00'),('pixels','gg'*49152),('pixels',' '*98304),('format','png')]:
            obj=fixture(); obj['crop'][k]=v
            with self.subTest(k=k), self.assertRaises(s.Unavailable): s.validate(dump(obj),'cursor')
        with self.assertRaises(s.Unavailable): s.validate(b' '*102401,'hud')

    def test_exact_png_roundtrip_and_bounds(self):
        rng=random.Random(83)
        for w,h,pixels in [(1,1,b'\0\xff\x80'),(128,128,bytes(49152)),
                            (128,128,rng.randbytes(49152))]:
            obj=fixture(w=w,h=h,pixels=pixels)
            png=s.encode_png(obj); self.assertEqual(png,s.encode_png(obj)); self.assertLessEqual(len(png),50000)
            self.assertEqual(png[:8],b'\x89PNG\r\n\x1a\n')
            offset=8; chunks={}
            while offset<len(png):
                n=struct.unpack('>I',png[offset:offset+4])[0]; tag=png[offset+4:offset+8]
                data=png[offset+8:offset+8+n]; crc=struct.unpack('>I',png[offset+8+n:offset+12+n])[0]
                self.assertEqual(crc,zlib.crc32(tag+data)); chunks[tag]=data; offset+=12+n
            self.assertEqual(set(chunks),{b'IHDR',b'IDAT',b'IEND'})
            rows=zlib.decompress(chunks[b'IDAT']); stride=w*3+1
            self.assertTrue(all(rows[y*stride]==0 for y in range(h)))
            self.assertEqual(b''.join(rows[y*stride+1:(y+1)*stride] for y in range(h)),pixels)
            block=s.image_content(obj); self.assertEqual(base64.b64decode(block['data']),png)

    def test_revision_cache_and_session_reset(self):
        cache=s.CursorCache(); obj=fixture(); png=cache.convert(dump(obj))
        self.assertIs(cache.convert(dump(obj)),png)
        changed=copy.deepcopy(obj); changed['crop']['x']=1
        with self.assertRaises(s.Unavailable):cache.convert(dump(changed))
        changed['public_revision']=2; cache.convert(dump(changed))
        with self.assertRaises(s.Unavailable):cache.convert(dump(obj))
        new=copy.deepcopy(obj); new['session']='b'*32; new['public_revision']=0
        cache.convert(dump(new))
        with self.assertRaises(s.Unavailable):cache.convert(dump(obj))


class FileTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.path=Path(self.temp.name)
        self.lock=self.path/'cursor.lock'; self.file=self.path/'cursor.json'
        self.lock.write_bytes(b''); self.lock.chmod(0o600)
        self.file.write_bytes(dump(fixture())); self.file.chmod(0o600)
        self.fd=os.open(self.lock,os.O_RDWR); fcntl.flock(self.fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
    def tearDown(self):
        os.close(self.fd); self.temp.cleanup()
    def read(self): return s.read_candidate(str(self.path),'cursor')
    def test_private_live_lock_historical_only(self):
        self.assertEqual(self.read(),fixture())
        result=ready(cursor_dir=str(self.path)).handle(req('tools/call',{'name':a.NAMES[1]}))['result']
        self.assertFalse(result['isError']); self.assertEqual(result['content'][1]['type'],'image')
        metadata=json.loads(result['content'][0]['text']); self.assertEqual(metadata['freshness'],'unverified')
        self.assertNotIn('pixels',metadata['crop'])
        self.assertEqual(metadata['schema'],'openxcom-mcp-last-cursor/0')
        self.assertEqual(metadata['source_schema'],'experimental-presented-cursor/0')
    def test_wrong_owner_rejected(self):
        with patch('snapshots.os.geteuid', return_value=os.geteuid()+1), self.assertRaises(s.Unavailable):
            self.read()
    def test_stale_file_never_promoted_to_current(self):
        os.utime(self.file, (1,1))
        # Old bytes may be read only as explicitly historical, even with a lock.
        self.assertEqual(self.read()['freshness'], 'unverified')
        server=ready(cursor_dir=str(self.path))
        out=server.handle(req('tools/call', {'name':a.NAMES[1], 'arguments':{'require_current':True}}))
        self.assertEqual(out['result'], a.UNAVAILABLE)
    def test_atomic_replacement_returns_pinned_historical_bytes(self):
        real=os.read; changed=False
        newer=fixture(); newer['session']='b'*32
        def race(fd,n):
            nonlocal changed
            data=real(fd,n)
            if not changed:
                changed=True
                replacement=self.path/'replacement'; replacement.write_bytes(dump(newer)); replacement.chmod(0o600)
                os.replace(replacement,self.file)
            return data
        # The old opened file becomes unlinked; conservative single-link check rejects it.
        with patch('snapshots.os.read', side_effect=race), self.assertRaises(s.Unavailable):self.read()
        self.assertEqual(self.read()['session'], 'b'*32)
    def test_no_lock_crash_and_replaced_lock(self):
        fcntl.flock(self.fd,fcntl.LOCK_UN)
        with self.assertRaises(s.Unavailable):self.read()
        self.lock.unlink()
        with self.assertRaises(s.Unavailable):self.read()
        self.lock.write_bytes(b''); self.lock.chmod(0o600)
        with self.assertRaises(s.Unavailable):self.read()
    def test_modes_symlinks_hardlinks_and_fifo(self):
        self.file.chmod(0o644)
        with self.assertRaises(s.Unavailable):self.read()
        self.file.chmod(0o600); self.path.chmod(0o755)
        with self.assertRaises(s.Unavailable):self.read()
        self.path.chmod(0o700); os.link(self.file,self.path/'link')
        with self.assertRaises(s.Unavailable):self.read()
        (self.path/'link').unlink(); self.file.unlink(); self.file.symlink_to('/etc/passwd')
        with self.assertRaises(s.Unavailable):self.read()
        self.file.unlink(); os.mkfifo(self.file,0o600)
        with self.assertRaises(s.Unavailable):self.read()
    def test_directory_and_lock_symlinks(self):
        with tempfile.TemporaryDirectory() as other:
            alias=Path(other)/'alias'; alias.symlink_to(self.path,target_is_directory=True)
            with self.assertRaises(s.Unavailable):s.read_candidate(str(alias),'cursor')
        self.lock.unlink(); self.lock.symlink_to(self.file)
        with self.assertRaises(s.Unavailable):self.read()
    def test_oversize_and_unknown_kind(self):
        self.file.write_bytes(b'x'*(s.MAX_SNAPSHOT+1))
        with self.assertRaises(s.Unavailable):self.read()
        with self.assertRaises(s.Unavailable):s.read_candidate(str(self.path),'../../secret')
    def test_snapshot_mutation_race_fails(self):
        real=os.read; changed=False
        def race(fd,n):
            nonlocal changed
            data=real(fd,n)
            if not changed:
                changed=True
                with self.file.open('ab') as f:f.write(b' ')
            return data
        with patch('snapshots.os.read',side_effect=race),self.assertRaises(s.Unavailable):self.read()
    def test_lock_replacement_race_fails(self):
        real=os.read; changed=False
        def race(fd,n):
            nonlocal changed
            data=real(fd,n)
            if not changed:
                changed=True; self.lock.unlink(); self.lock.write_bytes(b''); self.lock.chmod(0o600)
            return data
        with patch('snapshots.os.read',side_effect=race),self.assertRaises(s.Unavailable):self.read()
    def test_hud_separate_session(self):
        hp=self.path/'snapshot.json'; hp.write_bytes(dump(fixture('hud'))); hp.chmod(0o600)
        lp=self.path/'observer.lock'; lp.write_bytes(b''); lp.chmod(0o600)
        fd=os.open(lp,os.O_RDWR)
        try:
            fcntl.flock(fd,fcntl.LOCK_EX)
            result=ready(hud_dir=str(self.path)).handle(req('tools/call',{'name':a.NAMES[0]}))['result']
            self.assertFalse(result['isError'])
        finally:os.close(fd)

if __name__=='__main__':unittest.main()
