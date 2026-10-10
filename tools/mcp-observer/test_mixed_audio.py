# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic bytes and anonymous IPC only; no game, devices, assets or models."""
import base64
import io
import json
import os
import socket
import struct
import sys
import threading
import time
import unittest
import wave
from unittest.mock import patch

import adapter
import launch_presentation
import mixed_audio as a
from snapshots import Unavailable

EPOCH, ADMISSION, CHALLENGE, NEXT = (c * 32 for c in '1234')
PCM = struct.pack('<hh', -32768, 32767) * a.FRAMES


def fixture(pcm=PCM, challenge=CHALLENGE, next_token=NEXT):
    return {'schema': a.SCHEMA, 'epoch': EPOCH, 'challenge': challenge,
            'next_token': next_token, 'semantics': a.SEMANTICS, 'profile': a.PROFILE,
            'duration_ms': 500, 'sample_rate': 22050, 'channels': 2,
            'sample_format': 's16le', 'frames': 11025, 'pcm_hex': pcm.hex()}


def dump(obj):
    return json.dumps(obj, separators=(',', ':')).encode()


def call(server, arguments=None):
    server.state = 'ready'
    params = {'name': 'read_mixed_audio'}
    if arguments is not None:
        params['arguments'] = arguments
    return server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call', 'params': params})


class EncodingTests(unittest.TestCase):
    def test_exact_wav_header_and_interleaved_channel_order(self):
        raw = a.encode_wav(PCM)
        self.assertEqual(len(raw), 44144)
        self.assertEqual(raw[:44], bytes.fromhex(
            '5249464668ac000057415645666d74201000000001000200225600008858010004001000'
            '6461746144ac0000'))
        self.assertEqual(raw[44:], PCM)
        with wave.open(io.BytesIO(raw), 'rb') as wav:
            self.assertEqual((wav.getnchannels(), wav.getsampwidth(), wav.getframerate(),
                              wav.getnframes(), wav.getcomptype()), (2, 2, 22050, 11025, 'NONE'))
            self.assertEqual(wav.readframes(11026), PCM)
        self.assertEqual(struct.unpack('<hh', raw[44:48]), (-32768, 32767))

    def test_varying_signed_samples_no_transform(self):
        pcm = b''.join(struct.pack('<hh', (i * 17) % 65536 - 32768,
                                  (i * 19) % 65536 - 32768) for i in range(a.FRAMES))
        self.assertEqual(a.encode_wav(pcm)[44:], pcm)

    def test_silence_is_full_clip_not_trimmed_or_rejected(self):
        pcm = bytes(a.PCM_BYTES)
        obj, decoded = a.validate(dump(fixture(pcm)), EPOCH, CHALLENGE, ADMISSION)
        self.assertEqual(decoded, pcm)
        content = a.audio_content(decoded)
        self.assertEqual(content['type'], 'audio')
        self.assertEqual(content['mimeType'], 'audio/wav')
        self.assertEqual(len(content['data']), 58860)
        self.assertEqual(base64.b64decode(content['data'], validate=True), a.encode_wav(pcm))
        self.assertEqual(a.encode_wav(pcm)[44:], pcm)
        self.assertNotIn('silent', obj)

    def test_identical_pcm_identical_wav_base64_and_normalized_response(self):
        one = fixture()
        two = fixture(challenge='a' * 32, next_token='b' * 32)
        results = []
        for obj in (one, two):
            client = type('SyntheticClient', (), {'observe': lambda self, obj=obj: (obj, PCM)})()
            result = call(adapter.Server(mixed_audio=client))
            meta = json.loads(result['result']['content'][0]['text'])
            meta['challenge'] = CHALLENGE
            result['result']['content'][0]['text'] = json.dumps(meta, separators=(',', ':'))
            results.append(dump(result))
        self.assertEqual(*results)
        self.assertEqual(a.audio_content(PCM), a.audio_content(PCM))

    def test_encode_rejects_wrong_size_type(self):
        for pcm in (b'', PCM[:-1], PCM + b'\0', bytearray(PCM), None, '00'):
            with self.subTest(size=len(pcm) if pcm is not None else None), self.assertRaises(Unavailable):
                a.encode_wav(pcm)

    def test_pcm_native_wav_and_mcp_bounds(self):
        self.assertEqual(len(PCM), a.PCM_BYTES)
        self.assertEqual(len(fixture()['pcm_hex']), 88200)
        self.assertLess(len(dump(fixture())), a.MAX_PACKET)
        client = type('SyntheticClient', (), {'observe': lambda self: (fixture(), PCM)})()
        result = call(adapter.Server(mixed_audio=client))
        raw = dump(result) + b'\n'
        self.assertLess(len(raw), adapter.MAX_RESPONSE)
        self.assertEqual(len(result['result']['content'][1]['data']), a.BASE64_CHARACTERS)
        self.assertEqual(a.WAV_BYTES, 44144)

    def test_response_bound_with_maximal_utf8_request_id(self):
        client = type('SyntheticClient', (), {'observe': lambda self: (fixture(), PCM)})()
        request = {'jsonrpc': '2.0', 'id': '', 'method': 'tools/call',
                   'params': {'name': 'read_mixed_audio', 'arguments': {}}}
        room = adapter.MAX_MESSAGE - len(dump(request)) - 1
        request['id'] = '\U0001f600' * (room // 4)
        encoded = json.dumps(request, separators=(',', ':'), ensure_ascii=False).encode() + b'\n'
        self.assertLessEqual(len(encoded), adapter.MAX_MESSAGE)
        initialize = {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
                      'params': {'protocolVersion': adapter.PROTOCOL, 'capabilities': {},
                                 'clientInfo': {'name': 'synthetic', 'version': '1'}}}
        started = {'jsonrpc': '2.0', 'method': 'notifications/initialized'}
        source = io.BytesIO(dump(initialize) + b'\n' + dump(started) + b'\n' + encoded)
        sink = io.BytesIO()
        adapter.serve(source, sink, mixed_audio=client)
        lines = sink.getvalue().splitlines(keepends=True)
        self.assertEqual(len(lines), 2)
        self.assertLessEqual(len(lines[-1]), adapter.MAX_RESPONSE)
        self.assertEqual(json.loads(lines[-1])['id'], request['id'])
        self.assertFalse(json.loads(lines[-1])['result']['isError'])


class SchemaTests(unittest.TestCase):
    def test_valid(self):
        obj, pcm = a.validate(dump(fixture()), EPOCH, CHALLENGE, ADMISSION)
        self.assertEqual(obj, fixture())
        self.assertEqual(pcm, PCM)

    def test_every_unknown_field_is_rejected(self):
        for field in ('actor', 'source_id', 'position', 'ticks', 'timestamp', 'hud', 'reason', 'silent'):
            with self.subTest(field=field), self.assertRaises(Unavailable):
                a.validate(dump(dict(fixture(), **{field: 'hidden'})), EPOCH, CHALLENGE, ADMISSION)

    def test_every_missing_field_is_rejected(self):
        for field in fixture():
            obj = fixture(); del obj[field]
            with self.subTest(field=field), self.assertRaises(Unavailable):
                a.validate(dump(obj), EPOCH, CHALLENGE, ADMISSION)

    def test_numeric_fields_are_exact_strict_integers(self):
        for field in ('duration_ms', 'sample_rate', 'channels', 'frames'):
            for value in (-1, 0, True, False, 1.0, '2', None, {}, [], 2**64):
                with self.subTest(field=field, value=value), self.assertRaises(Unavailable):
                    a.validate(dump(dict(fixture(), **{field: value})), EPOCH, CHALLENGE, ADMISSION)

    def test_malformed_packets(self):
        for raw in (b'', b'{}', b'null', b'[]', b'false', b'\xff', b'{"x":NaN}',
                    b'[' * 2000, b'x' * (a.MAX_PACKET + 1),
                    dump(fixture())[:-1], dump(fixture()) + b'\x00',
                    dump(fixture())[:-1] + b',"frames":11025}'):
            with self.subTest(size=len(raw)), self.assertRaises(Unavailable):
                a.validate(raw, EPOCH, CHALLENGE, ADMISSION)


def invalid_field(field, value):
    def test(self):
        obj = fixture(); obj[field] = value
        with self.assertRaisesRegex(Unavailable, '^Observation unavailable$'):
            a.validate(dump(obj), EPOCH, CHALLENGE, ADMISSION)
    return test


for label, field, value in (
        ('schema', 'schema', 'experimental-presentation/1'),
        ('semantics', 'semantics', 'current'), ('profile', 'profile', 'other'),
        ('format', 'sample_format', 's16be'), ('epoch', 'epoch', '5' * 32),
        ('challenge', 'challenge', '5' * 32), ('token_replay', 'next_token', ADMISSION),
        ('token_short', 'next_token', 'a' * 31), ('token_case', 'next_token', 'A' * 32),
        ('token_null', 'next_token', None), ('short_pcm', 'pcm_hex', PCM.hex()[:-2]),
        ('long_pcm', 'pcm_hex', PCM.hex() + '00'), ('odd_pcm', 'pcm_hex', PCM.hex()[:-1]),
        ('uppercase_pcm', 'pcm_hex', PCM.hex().upper()),
        ('whitespace_pcm', 'pcm_hex', ' ' * (a.PCM_BYTES * 2)),
        ('nonhex_pcm', 'pcm_hex', 'g' * (a.PCM_BYTES * 2)),
        ('label_pcm', 'pcm_hex', 'actor:' + '0' * (a.PCM_BYTES * 2 - 6)),
        ('pcm_null', 'pcm_hex', None), ('pcm_array', 'pcm_hex', [])):
    setattr(SchemaTests, f'test_{label}_rejected', invalid_field(field, value))


class ChannelTests(unittest.TestCase):
    def setUp(self):
        channel, self.peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.client = a.MixedAudioClient(channel, timeout=0.1, startup_timeout=0.2)
        self.peer.settimeout(1)
        self.peer.send(a.HELLO + (EPOCH + ADMISSION).encode())
        self.threads = []
        self.errors = []

    def tearDown(self):
        self.client.close()
        for thread in self.threads:
            thread.join(1)
        self.peer.close()
        self.assertFalse(any(t.is_alive() for t in self.threads))
        if self.errors:
            raise self.errors[0]

    def producer(self, change=None, raw=None, delay=0, next_token=NEXT):
        def work():
            try:
                request = self.peer.recv(97).decode('ascii')
                self.assertEqual(len(request), 96)
                self.assertEqual(request[:64], EPOCH + (self.client.admission or ADMISSION))
                obj = fixture(challenge=request[64:], next_token=next_token)
                if change:
                    change(obj)
                if delay:
                    time.sleep(delay)
                packet = raw(obj) if callable(raw) else raw
                try:
                    self.peer.send(dump(obj) if packet is None else packet)
                except BrokenPipeError:
                    pass
            except Exception as error:
                self.errors.append(error)
        thread = threading.Thread(target=work)
        self.threads.append(thread); thread.start()

    def unavailable(self):
        with self.assertRaisesRegex(Unavailable, '^Observation unavailable$'):
            self.client.observe()
        self.assertIsNone(self.client.channel)
        with self.assertRaises(Unavailable):
            self.client.observe()

    def test_success_and_rotation(self):
        self.producer()
        obj, pcm = self.client.observe()
        self.assertEqual(pcm, PCM)
        self.assertEqual(obj['epoch'], EPOCH)
        self.assertEqual(self.client.admission, NEXT)
        self.assertRegex(obj['challenge'], '^[0-9a-f]{32}$')

    def test_timeout(self): self.unavailable()
    def test_late(self): self.producer(delay=0.15); self.unavailable()
    def test_eof(self): self.peer.close(); self.unavailable()
    def test_wrong_epoch(self): self.producer(lambda o: o.update(epoch='5' * 32)); self.unavailable()
    def test_wrong_challenge(self): self.producer(lambda o: o.update(challenge='5' * 32)); self.unavailable()
    def test_replayed_admission(self): self.producer(lambda o: o.update(next_token=ADMISSION)); self.unavailable()
    def test_malformed(self): self.producer(raw=b'{}'); self.unavailable()
    def test_oversized(self): self.producer(raw=b'x' * 110000); self.unavailable()
    def test_truncated_json(self): self.producer(raw=lambda o: dump(o)[:-1]); self.unavailable()
    def test_hidden_labels(self): self.producer(lambda o: o.update(source_id='secret')); self.unavailable()
    def test_unsolicited(self): self.peer.send(b'{}'); self.unavailable()

    def test_ancillary_descriptor_is_rejected(self):
        self.client.channel.recv(100)
        read_fd, write_fd = os.pipe()
        try:
            self.peer.sendmsg([a.HELLO + (EPOCH + ADMISSION).encode()],
                              [(socket.SOL_SOCKET, socket.SCM_RIGHTS, struct.pack('i', read_fd))])
            self.unavailable()
        finally:
            os.close(read_fd); os.close(write_fd)

    def test_deadline_includes_reply_validation(self):
        self.producer()
        validate = a.validate
        def slow_validation(*args):
            result = validate(*args)
            time.sleep(0.12)
            return result
        with patch('mixed_audio.validate', side_effect=slow_validation):
            self.unavailable()

    def test_replayed_reply(self):
        self.producer(); obj, _ = self.client.observe()
        self.client.next_allowed = 0
        self.producer(raw=dump(obj)); self.unavailable()

    def test_rate_limit_permanently_closes(self):
        self.producer(); self.client.observe()
        self.assertGreater(self.client.next_allowed, time.monotonic() + 0.9)
        self.unavailable()

    def test_second_permitted_request_rotates_binding(self):
        self.producer(); first, _ = self.client.observe()
        self.client.next_allowed = 0  # Unit clock shortcut only, never public API.
        self.producer(next_token='6' * 32); second, _ = self.client.observe()
        self.assertNotEqual(first['challenge'], second['challenge'])
        self.assertEqual(first['epoch'], second['epoch'])
        self.assertEqual(self.client.admission, '6' * 32)

    def test_concurrent_request_closes_without_second_send(self):
        self.client.flight.acquire()
        try: self.unavailable()
        finally: self.client.flight.release()

    def test_concurrent_close_between_select_and_recv_is_generic(self):
        def close_after_select(read, write, exceptional, timeout):
            self.client.close()
            return read, [], []
        with patch('mixed_audio.select.select', side_effect=close_after_select):
            self.unavailable()

    def test_second_thread_closes_waiting_first_request(self):
        outcomes = []
        def waiting():
            try: self.client.observe()
            except Exception as error: outcomes.append(error)
        thread = threading.Thread(target=waiting); self.threads.append(thread); thread.start()
        self.assertEqual(len(self.peer.recv(100)), 96)
        self.unavailable()
        thread.join(1)
        self.assertEqual(len(outcomes), 1)
        self.assertIsInstance(outcomes[0], Unavailable)
        self.assertEqual(str(outcomes[0]), 'Observation unavailable')
        self.assertEqual(self.peer.recv(100), b'')

    def test_bad_hello(self):
        self.client.channel.recv(100)
        self.peer.send(b'OPENXCOM-PRESENTATION/1 ' + (EPOCH + ADMISSION).encode())
        self.unavailable()

    def test_startup_timeout_is_separate_from_capture(self):
        self.client.channel.recv(100)
        def later():
            time.sleep(0.12)
            self.peer.send(a.HELLO + (EPOCH + ADMISSION).encode())
        thread = threading.Thread(target=later); self.threads.append(thread); thread.start()
        self.producer(delay=0.03)
        self.assertEqual(self.client.observe()[1], PCM)

    def test_startup_timeout_closes(self):
        self.client.channel.recv(100)
        self.client.startup_timeout = 0.01
        self.unavailable()

    def test_queued_second_hello_not_new_session(self):
        self.producer(); self.client.observe()
        self.client.next_allowed = 0
        self.peer.send(a.HELLO + ('5' * 64).encode())
        self.unavailable()

    def test_tool_success_native_audio_and_closed_metadata(self):
        self.producer()
        result = call(adapter.Server(mixed_audio=self.client), {})['result']
        self.assertFalse(result['isError'])
        self.assertEqual(len(result['content']), 2)
        meta = json.loads(result['content'][0]['text'])
        self.assertEqual(set(meta), set(a.KEYS.split()) - {'next_token', 'pcm_hex'})
        self.assertEqual(meta['schema'], a.PUBLIC_SCHEMA)
        self.assertEqual(meta['semantics'], a.SEMANTICS)
        self.assertEqual(result['content'][1], a.audio_content(PCM))

    def test_failure_is_one_generic_text_only_result(self):
        self.producer(lambda o: o.update(challenge='5' * 32))
        result = call(adapter.Server(mixed_audio=self.client))['result']
        self.assertEqual(result, {'isError': True, 'content': [{'type': 'text', 'text':
            '{"available":false,"reason":"observation_unavailable"}'}]})

    def test_no_historical_or_visual_fallback(self):
        self.producer()
        with patch('snapshots.read_candidate', side_effect=AssertionError('historical read')):
            result = call(adapter.Server('unused', 'unused', mixed_audio=self.client))
        self.assertFalse(result['result']['isError'])


class InterfaceTests(unittest.TestCase):
    def test_audio_is_absent_by_default(self):
        self.assertNotIn('read_mixed_audio', [t['name'] for t in adapter.definitions()])
        self.assertIn('error', call(adapter.Server()))

    def test_audio_tool_takes_no_arguments(self):
        client = type('NeverRead', (), {'observe': lambda self: self.fail()})()
        server = adapter.Server(mixed_audio=client)
        for args in (None, [], True, 0, 'x', {'require_current': False}, {'duration_ms': 500},
                     {'source_id': 'x'}, {'path': 'x'}, {'volume': 0}):
            server.state = 'ready'
            result = server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
                                    'params': {'name': 'read_mixed_audio', 'arguments': args}})
            self.assertEqual(result['error']['code'], -32602)
        definition = adapter.definitions(mixed_audio=True)[-1]
        self.assertEqual(definition['inputSchema'], {'type': 'object', 'properties': {},
                                                   'additionalProperties': False})
        self.assertTrue(definition['annotations']['readOnlyHint'])
        self.assertFalse(definition['annotations']['idempotentHint'])

    def test_no_cross_stream_calls_or_failure_cascade(self):
        audio = type('SyntheticAudio', (), {'observe': lambda self: (fixture(), PCM)})()
        class FailedVisual:
            version = 2
            def observe(self): raise Unavailable('Observation unavailable')
        server = adapter.Server(presentation=FailedVisual(), mixed_audio=audio)
        self.assertFalse(call(server)['result']['isError'])
        failed = server.handle({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/call',
                                'params': {'name': 'read_presented_hud', 'arguments': {}}})
        self.assertTrue(failed['result']['isError'])
        self.assertFalse(call(server)['result']['isError'])

    def test_timeout_configuration_rejected_before_socket(self):
        for value in (0, -1, 2.1, True, None, '1', float('inf'), float('nan')):
            with self.subTest(value=value), self.assertRaises(Unavailable):
                a.MixedAudioClient(None, timeout=value)
        for value in (0, 31.1, True, float('nan')):
            with self.subTest(startup=value), self.assertRaises(Unavailable):
                a.MixedAudioClient(None, startup_timeout=value)

    def test_stream_socket_rejected(self):
        one, two = socket.socketpair()
        try:
            with self.assertRaises(Unavailable): a.MixedAudioClient(one)
        finally: one.close(); two.close()

    def test_launcher_explicit_channels_and_scrubbed_ambient_fds(self):
        for flags, visual, audio, version in (([], True, False, 1),
                (['--mixed-audio'], True, True, 1),
                (['--mixed-audio', '--presented-hud'], True, True, 2),
                (['--audio-only'], False, True, None)):
            with self.subTest(flags=flags), \
                    patch.object(sys, 'argv', ['launcher', *flags, '--', 'synthetic-game']), \
                    patch.dict(os.environ, {'OPENXCOM_PRESENTATION_VERSION': '2',
                                            'OPENXCOM_PRESENTATION_FD': '123',
                                            'OPENXCOM_MIXED_AUDIO_FD': '456'}), \
                    patch('launch_presentation.subprocess.Popen') as spawn, \
                    patch('launch_presentation.serve') as serve:
                launch_presentation.main()
                env = spawn.call_args.kwargs['env']
                self.assertEqual('OPENXCOM_PRESENTATION_FD' in env, visual)
                self.assertEqual('OPENXCOM_MIXED_AUDIO_FD' in env, audio)
                self.assertEqual(env.get('OPENXCOM_PRESENTATION_VERSION'), '2' if version == 2 else None)
                fds = spawn.call_args.kwargs['pass_fds']
                self.assertEqual(len(set(fds)), int(visual) + int(audio))
                self.assertEqual(serve.call_args.args[4] is not None, visual)
                self.assertEqual(serve.call_args.args[5] is not None, audio)
                for client in serve.call_args.args[4:]:
                    if client is not None: self.assertIsNone(client.channel)
                spawn.return_value.kill.assert_not_called()
                spawn.return_value.terminate.assert_not_called()

    def test_invalid_launcher_combination_does_not_spawn(self):
        with patch.object(sys, 'argv', ['launcher', '--audio-only', '--presented-hud', '--', 'game']), \
                patch('launch_presentation.subprocess.Popen') as spawn, \
                patch('sys.stderr', io.StringIO()), self.assertRaises(SystemExit):
            launch_presentation.main()
        spawn.assert_not_called()


if __name__ == '__main__':
    unittest.main()
