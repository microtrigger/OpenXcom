# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
import json
import socket
import threading
import time
import unittest
from unittest.mock import patch
from adapter import Server
from presentation import PresentationClient
from snapshots import Unavailable, PROFILE, encode_png

EPOCH = '1' * 32
ADMISSION = '2' * 32
CROP = {'screen_width':320,'screen_height':200,'x':0,'y':0,'width':1,'height':1,
        'format':'rgb24-hex-row-major','pixels':'010203'}

class PresentationTests(unittest.TestCase):
    def setUp(self):
        a, self.peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.client = PresentationClient(a, timeout=0.08)
        self.peer.settimeout(1)
        self.peer.send(('OPENXCOM-PRESENTATION/1 '+EPOCH+ADMISSION).encode())
        self.threads = []
    def tearDown(self):
        self.client.close()
        for t in self.threads: t.join(1)
        self.peer.close()
    def producer(self, change=None, delay=0, raw=None):
        def work():
            request = self.peer.recv(97).decode()
            self.assertEqual(len(request), 96)
            obj = {'schema':'experimental-presentation/1','epoch':EPOCH,'challenge':request[64:],
                   'next_token':'3'*32,'semantics':'presented_after_request','profile':PROFILE,'crop':dict(CROP)}
            if change: change(obj)
            if delay: time.sleep(delay)
            try: self.peer.send(raw if raw is not None else json.dumps(obj).encode())
            except BrokenPipeError: pass
        t=threading.Thread(target=work);self.threads.append(t);t.start()
    def unavailable(self):
        with self.assertRaisesRegex(Unavailable, '^Observation unavailable$'): self.client.observe()
        self.assertIsNone(self.client.channel)
    def test_exact_pixels_and_envelope(self):
        self.producer();obj,candidate=self.client.observe()
        self.assertEqual(obj['crop'],CROP);self.assertEqual(obj['semantics'],'presented_after_request')
        self.assertTrue(encode_png(candidate).startswith(b'\x89PNG'))
        self.assertEqual(self.client.admission,'3'*32)
    def test_timeout_no_frame(self): self.unavailable()
    def test_late(self): self.producer(delay=0.12);self.unavailable()
    def test_wrong_epoch(self): self.producer(lambda o:o.update(epoch='4'*32));self.unavailable()
    def test_wrong_challenge(self): self.producer(lambda o:o.update(challenge='4'*32));self.unavailable()
    def test_replayed_token(self): self.producer(lambda o:o.update(next_token=ADMISSION));self.unavailable()
    def test_extra_key(self): self.producer(lambda o:o.update(ticks=1));self.unavailable()
    def test_invalid_crop(self): self.producer(lambda o:o['crop'].update(width=129));self.unavailable()
    def test_null_crop(self): self.producer(lambda o:o.update(crop=None));self.unavailable()
    def test_oversize(self): self.producer(raw=b'x'*110000);self.unavailable()
    def test_malformed(self): self.producer(raw=b'{}');self.unavailable()
    def test_nonfinite(self): self.producer(raw=b'{"x":NaN}');self.unavailable()
    def test_unsolicited(self): self.peer.send(b'{}');self.unavailable()
    def test_closed(self): self.peer.close();self.unavailable()
    def test_rate_bound(self):
        self.producer();self.client.observe();self.unavailable()
    def test_replay_response(self):
        self.producer();obj,_=self.client.observe();self.client.next_allowed=0
        self.producer(raw=json.dumps(obj).encode());self.unavailable()
    def test_stale_hello_after_initialization(self):
        self.client.epoch=EPOCH;self.client.admission=ADMISSION
        # A startup packet remaining after initialization is not a new response.
        self.unavailable()
    def test_deadline_precedes_send(self):
        self.client.channel.recv(100)
        self.client.epoch=EPOCH;self.client.admission=ADMISSION
        with patch('presentation.time.monotonic', side_effect=[0.0, 0.0, 1.0]):
            self.unavailable()
        self.assertEqual(self.peer.recv(100), b'')

    def test_optional_tool_and_hud_still_denied(self):
        s=Server(presentation=self.client);s.state='ready'
        reply=s.handle({'jsonrpc':'2.0','id':1,'method':'tools/list'})
        self.assertEqual(len(reply['result']['tools']),3)
        reply=s.handle({'jsonrpc':'2.0','id':2,'method':'tools/call','params':{'name':'read_last_hud','arguments':{'require_current':True}}})
        self.assertTrue(reply['result']['isError'])
        for args in ({'x':1}, [], None, {'require_current':True}):
            reply=s.handle({'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'read_presented_cursor','arguments':args}})
            self.assertIn('error',reply)
    def test_fresh_tool_image_output(self):
        self.producer();s=Server(presentation=self.client);s.state='ready'
        reply=s.handle({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'read_presented_cursor','arguments':{}}})
        self.assertFalse(reply['result']['isError']);content=reply['result']['content']
        meta=json.loads(content[0]['text']);self.assertNotIn('next_token',meta)
        self.assertNotIn('public_revision',meta);self.assertNotIn('pixels',meta['crop'])
        self.assertEqual(content[1]['type'],'image')

if __name__=='__main__': unittest.main()
