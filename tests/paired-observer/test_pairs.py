# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
"""Real native UI/extraction + native transport/Python adapter integration.
No game assets, saves, displayed windows, simulation, or gameplay input are used.
"""
import copy
import json
import os
from pathlib import Path
import re
import select
import socket
import subprocess
import sys
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mcp-observer'))
from adapter import Server, strict_json
from presentation import PresentationClient

BINARY = sys.argv.pop(1)
MODE = sys.argv.pop(1) if len(sys.argv) > 1 and sys.argv[1].startswith('--expect-') else None


class Native:
    def __init__(self):
        self.socket, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        env = os.environ.copy()
        env['OPENXCOM_PRESENTATION_VERSION'] = '2'
        self.process = subprocess.Popen([BINARY, str(child.fileno())], pass_fds=(child.fileno(),),
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, text=True, env=env)
        child.close()

    def close(self):
        self.socket.close()
        if self.process.poll() is None:
            self.process.stdin.write('quit\n')
            self.process.stdin.flush()
        _, error = self.process.communicate(timeout=3)
        if self.process.returncode:
            raise AssertionError(f'native fixture exited {self.process.returncode}: {error}')
        if error:
            raise AssertionError(f'unexpected native diagnostics: {error}')

    def __enter__(self):
        return self

    def __exit__(self, *unused):
        self.close()

    def command(self, command):
        self.process.stdin.write(command + '\n')
        self.process.stdin.flush()
        if not select.select([self.process.stdout], [], [], 3)[0]:
            raise AssertionError('native control barrier timed out: ' + command)
        line = self.process.stdout.readline()
        if not line:
            raise AssertionError('native control channel closed: ' + command)
        return strict_json(line)

    def receive(self):
        if not select.select([self.socket], [], [], 2)[0]:
            raise AssertionError('native packet timeout')
        raw, _, flags, _ = self.socket.recvmsg(102401)
        assert not flags & socket.MSG_TRUNC
        return raw

    def empty(self):
        return not select.select([self.socket], [], [], 0)[0]

    def hello(self):
        hello = self.receive()
        assert hello.startswith(b'OPENXCOM-PRESENTATION/2 ') and len(hello) == 88
        return hello, hello[-64:-32], hello[-32:]


class Tokens:
    """Field-only bijection preserves token reuse and width, not pixel/secret bytes."""
    def __init__(self):
        self.mapping = {}

    def one(self, raw):
        assert re.fullmatch(b'[0-9a-f]{32}', raw)
        if raw not in self.mapping:
            self.mapping[raw] = f'{len(self.mapping) + 1:032x}'.encode()
        return self.mapping[raw]

    def packet(self, raw):
        if raw.startswith(b'OPENXCOM-PRESENTATION/2 '):
            result = raw[:-64] + self.one(raw[-64:-32]) + self.one(raw[-32:])
        elif len(raw) == 96 and re.fullmatch(b'[0-9a-f]{96}', raw):
            result = b''.join(self.one(raw[i:i+32]) for i in (0, 32, 64))
        else:
            obj = strict_json(raw.decode())
            fields = {key: obj[key] for key in ('epoch', 'challenge', 'next_token') if key in obj}
            def replace(match):
                key, value = match.group(1).decode(), match.group(2)
                assert fields[key] == value.decode()
                return b'"' + key.encode() + b'":"' + self.one(value) + b'"'
            result = re.sub(rb'"(epoch|challenge|next_token)":"([0-9a-f]{32})"', replace, raw)
        assert len(result) == len(raw)
        return result

    def public(self, result):
        result = copy.deepcopy(result)
        for content in result['result']['content']:
            if content['type'] == 'text':
                content['text'] = self.packet(content['text'].encode()).decode()
        return json.dumps(result, separators=(',', ':')).encode()


def raw_observation(scenario):
    with Native() as native:
        hello, epoch, admission = native.hello()
        request = epoch + admission + b'a' * 32
        native.socket.send(request)
        assert native.command('begin') == {'enabled': True, 'pending': True}
        composed = native.command('compose ' + scenario)
        native.command('extract')
        reply = native.receive()
        assert native.empty(), 'unexpected extra observer packet'
        tokens = Tokens()
        transcript = [(len(raw), tokens.packet(raw)) for raw in (hello, request, reply)]
        return composed, transcript


def assert_pair(test, left, right, expected):
    a, a_trace = raw_observation(left)
    b, b_trace = raw_observation(right)
    test.assertEqual(a['crop'], b['crop'], 'fixture precondition: complete final RGB must match')
    test.assertEqual(a_trace, b_trace, 'same final RGB must give byte-equivalent native transcript')
    test.assertEqual(a['hud'], expected)
    test.assertEqual(b['hud'], expected)


def request_call(server, name='read_presented_hud'):
    return server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
                          'params': {'name': name, 'arguments': {}}})


class Gate(unittest.TestCase):
    def test_real_numbertext_all_supported_values(self):
        with Native() as native:
            native.hello()
            for value in range(1000):
                obj = native.command(f'compose normal:{value}')
                self.assertEqual(obj['hud'], dict(time_units=value, energy=100, health=100, morale=100))
            for value in (1000, 9999, 99990, 4294967295):
                self.assertIsNone(native.command(f'compose normal:{value}')['hud'])

    def test_clipped_long_values_pair(self):
        assert_pair(self, 'normal:9999', 'normal:99990', None)

    def test_composed_pixels_override_source_values(self):
        assert_pair(self, 'overlaid:58', 'overlaid:59',
                    dict(time_units=58, energy=100, health=100, morale=100))

    def test_actual_hidden_and_occluded_pairs(self):
        for mode in ('hidden', 'temporary', 'offscreen', 'crop', 'clip', 'covered'):
            with self.subTest(mode=mode):
                assert_pair(self, mode + ':58', mode + ':59', None)
        assert_pair(self, 'bordered:9999', 'bordered:99990', None)

    def test_equal_rgb_palette_indices(self):
        assert_pair(self, 'normal:58', 'alias:58',
                    dict(time_units=58, energy=100, health=100, morale=100))

    def test_decoder_does_not_cache_prior_hud(self):
        with Native() as native:
            _, epoch, admission = native.hello()
            for index, scenario in enumerate(('normal:58', 'hidden:59', 'normal:59', 'missing:59')):
                native.socket.send(epoch + admission + f'{index:032x}'.encode())
                native.command('begin')
                composed = native.command('compose ' + scenario)
                native.command('extract')
                response = strict_json(native.receive().decode())
                self.assertEqual(response['hud'], composed['hud'])
                if scenario.startswith(('hidden', 'missing')):
                    self.assertIsNone(response['hud'])
                self.assertNotEqual(response['next_token'].encode(), admission)
                admission = response['next_token'].encode()

    def test_native_replay_admission_rejected(self):
        with Native() as native:
            _, epoch, admission = native.hello()
            request = epoch + admission + b'a' * 32
            native.socket.send(request)
            native.command('begin'); native.command('compose normal:58'); native.command('extract')
            response = strict_json(native.receive().decode())
            self.assertNotEqual(response['next_token'].encode(), admission)
            native.socket.send(request)
            self.assertFalse(native.command('begin')['enabled'],
                             'replayed admission must close native channel')
            self.assertEqual(native.receive(), b'')

    def test_new_producer_rejects_old_epoch(self):
        with Native() as old:
            _, old_epoch, old_admission = old.hello()
        with Native() as new:
            _, epoch, admission = new.hello()
            self.assertNotEqual(old_epoch, epoch)  # Check before normalization.
            self.assertNotEqual(old_admission, admission)
            new.socket.send(old_epoch + admission + b'a' * 32)
            self.assertFalse(new.command('begin')['enabled'])
            self.assertEqual(new.receive(), b'')

    def test_idle_and_queued_resets_keep_stream_epoch(self):
        for queued in (False, True):
            with self.subTest(queued=queued), Native() as native:
                _, epoch, admission = native.hello()
                request = epoch + admission + b'a' * 32
                if queued: native.socket.send(request)
                self.assertTrue(native.command('reset')['enabled'])
                if not queued: native.socket.send(request)
                native.command('begin'); native.command('compose normal:58'); native.command('extract')
                response = strict_json(native.receive().decode())
                self.assertEqual(response['epoch'], epoch.decode())
                self.assertEqual(response['challenge'], 'a' * 32)

    def test_consumed_reset_or_interrupted_frame_never_replies(self):
        for interrupt in ('reset', 'begin', 'close'):
            with self.subTest(interrupt=interrupt), Native() as native:
                _, epoch, admission = native.hello()
                native.socket.send(epoch + admission + b'a' * 32)
                native.command('begin'); native.command('compose normal:58')
                self.assertFalse(native.command(interrupt)['enabled'])
                native.command('extract')
                self.assertEqual(native.receive(), b'')

    def test_during_composition_and_after_extraction_await_next_begin(self):
        for phase in ('during', 'after'):
            with self.subTest(phase=phase), Native() as native:
                _, epoch, admission = native.hello()
                native.command('begin'); native.command('compose normal:58')
                if phase == 'after': native.command('extract')
                native.socket.send(epoch + admission + b'a' * 32)
                native.command('wait-request')
                native.command('extract')
                self.assertTrue(native.empty())
                native.command('begin'); native.command('compose normal:59'); native.command('extract')
                self.assertEqual(strict_json(native.receive().decode())['hud']['time_units'], 59)

    def client_run(self, scenario, name='read_presented_hud', interrupt=None):
        with Native() as native:
            client = PresentationClient(native.socket, timeout=0.3, version=2)
            server = Server(hud_dir='must-not-read', cursor_dir='must-not-read', presentation=client)
            server.state = 'ready'
            results, errors = [], []
            def call():
                try: results.append(request_call(server, name))
                except BaseException as error: errors.append(error)
            with patch('snapshots.read_candidate', side_effect=AssertionError('historical fallback')):
                thread = threading.Thread(target=call); thread.start()
                native.command('wait-request')
                native.command('begin')
                composed = native.command('compose ' + scenario)
                if interrupt == 'timeout':
                    thread.join(2)  # Deliberately no response; wait for the real deadline.
                    self.assertFalse(thread.is_alive())
                    self.assertIsNone(client.channel)
                    self.assertFalse(native.command('extract')['enabled'])
                else:
                    if interrupt: native.command(interrupt)
                    native.command('extract'); thread.join(2)
                self.assertFalse(thread.is_alive())
                self.assertEqual(errors, [])
                self.assertEqual(len(results), 1)
                if interrupt:
                    self.assertIsNone(client.channel)
                    self.assertEqual(request_call(server, name), results[0], 'no stale channel revival')
                client.close()
                return composed, results[0]

    def test_real_python_adapter_paired_hud_and_cursor(self):
        for name in ('read_presented_hud', 'read_presented_cursor'):
            for left, right in (('overlaid:58', 'overlaid:59'), ('hidden:58', 'hidden:59'),
                                ('normal:9999', 'normal:99990')):
                with self.subTest(name=name, left=left):
                    a, ar = self.client_run(left, name)
                    b, br = self.client_run(right, name)
                    self.assertEqual(a['crop'], b['crop'])
                    self.assertFalse(ar['result']['isError'])
                    self.assertEqual(Tokens().public(ar), Tokens().public(br))

    def test_real_python_adapter_timeout_and_reset_are_generic(self):
        for interrupt in ('reset', 'timeout'):
            with self.subTest(interrupt=interrupt):
                a, ar = self.client_run('overlaid:58', interrupt=interrupt)
                b, br = self.client_run('overlaid:59', interrupt=interrupt)
                self.assertEqual(a['crop'], b['crop'])
                self.assertEqual(ar, br)
                self.assertEqual(ar['result'], {'content': [{'type': 'text', 'text':
                    '{"available":false,"reason":"observation_unavailable"}'}], 'isError': True})

    def test_adapter_rejects_replayed_native_response(self):
        # Fault-injection relay replays genuine native bytes, not a mock reply.
        with Native() as native:
            endpoint, relay = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
            relay.settimeout(2)
            client = PresentationClient(endpoint, timeout=0.5, version=2)
            server = Server(presentation=client); server.state = 'ready'
            try:
                relay.send(native.receive())
                previous = None
                for replay in (False, True):
                    # Respect the real production rate limit; framing uses pipe barriers.
                    remaining = client.next_allowed - time.monotonic()
                    if remaining > 0: select.select([], [], [], remaining + 0.001)
                    results = []
                    thread = threading.Thread(target=lambda: results.append(request_call(server)))
                    thread.start()
                    request = relay.recv(97)
                    self.assertEqual(len(request), 96)
                    native.socket.send(request)
                    native.command('begin'); native.command('compose normal:58'); native.command('extract')
                    fresh = native.receive()
                    if replay:
                        self.assertNotEqual(strict_json(previous.decode())['challenge'],
                                            strict_json(fresh.decode())['challenge'])
                    relay.send(previous if replay else fresh)
                    thread.join(2)
                    self.assertFalse(thread.is_alive())
                    self.assertEqual(len(results), 1)
                    self.assertEqual(results[0]['result']['isError'], replay)
                    if replay:
                        self.assertIsNone(client.channel)
                        self.assertEqual(request_call(server), results[0])
                    previous = fresh
            finally:
                client.close(); relay.close()

    def test_normalization_retains_nonrandom_differences_and_reuse(self):
        common = {'epoch': 'a' * 32, 'challenge': 'b' * 32, 'next_token': 'c' * 32,
                  'hud': {'time_units': 58}, 'pixels': 'a' * 32, 'public_revision': 1}
        encode = lambda obj: json.dumps(obj, separators=(',', ':')).encode()
        for changed in (dict(common, pixels='b' * 32), dict(common, hud={'time_units': 59}),
                        dict(common, public_revision=2), dict(common, next_token='a' * 32)):
            self.assertNotEqual(Tokens().packet(encode(common)), Tokens().packet(encode(changed)))


if __name__ == '__main__':
    if MODE:
        selected = {'--expect-source-leak': 'test_composed_pixels_override_source_values',
                    '--expect-replay-leak': 'test_native_replay_admission_rejected'}[MODE]
        result = unittest.TestResult()
        Gate(selected).run(result)
        expected = ('same final RGB must give byte-equivalent native transcript' if MODE == '--expect-source-leak'
                    else 'replayed admission must close native channel')
        if result.errors or len(result.failures) != 1 or expected not in result.failures[0][1]:
            raise SystemExit(f'Expected exactly one assertion failure, got: {result.errors!r} {result.failures!r}')
        print('PASS: deliberately mutated production source rejected by ' + selected)
    else:
        unittest.main(verbosity=2)
