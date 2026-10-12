# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-11.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic fixed pixels and anonymous sockets; no game, assets or models."""
import array
import base64
import io
import json
import os
import random
import socket
import struct
import sys
import threading
import time
import unittest
import zlib
from unittest.mock import patch

import adapter
import launch_presentation
import supervised_stance as s
from snapshots import Unavailable

EPOCH, CAPABILITY, COMMAND, CHALLENGE = (c * 32 for c in '1234')
RGB = bytes.fromhex('0180ff') * (s.WIDTH * s.HEIGHT)


def dump(obj):
    return json.dumps(obj, separators=(',', ':')).encode()


def context(challenge=CHALLENGE, capability=CAPABILITY, epoch=EPOCH, rgb=RGB):
    return {'schema': s.CONTEXT_SCHEMA, 'challenge': challenge, 'control_epoch': epoch,
            'capability': capability, 'semantics': 'presented_after_request',
            'panel': {'width': s.WIDTH, 'height': s.HEIGHT, 'rgb_hex': rgb.hex()}}


def status(command_id=COMMAND, value='pending_confirmation'):
    return {'schema': s.STATUS_SCHEMA, 'command_id': command_id, 'status': value}


def arguments(capability=CAPABILITY, epoch=EPOCH, command_id=COMMAND,
              observed='stand', desired='kneel'):
    return {'capability': capability, 'control_epoch': epoch, 'command_id': command_id,
            'observed': observed, 'desired': desired}


def call(server, name, args=None):
    server.state = 'ready'
    params = {'name': name}
    if args is not None:
        params['arguments'] = args
    return server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call', 'params': params})


class NativeFixture:
    """One deterministic responder; call records prove absence of automatic sends."""
    def __init__(self, hello=None, response=None, selection_enabled=False):
        parent, self.peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.client = s.StanceClient(parent, timeout=0.3, startup_timeout=0.3,
                                     selection_enabled=selection_enabled)
        self.hello = dump({'schema': s.HELLO_SCHEMA, 'control_epoch': EPOCH}) if hello is None else hello
        self.response = response
        self.requests = []
        self.errors = []
        self.counter = 1
        self.latest = None
        self.status = 'pending_confirmation'
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def reply(self, raw):
        if self.response is not None:
            return self.response(raw, self)
        if raw[:1] == b'O':
            self.counter += 1
            self.latest = context(raw[1:].decode(), f'{self.counter:032x}')
            return dump(self.latest)
        if raw[:1] in (b'D', b'N'):
            return dump(status(raw[65:97].decode(), self.status))
        if raw[:1] == b'S':
            return dump(status(raw[1:].decode(), self.status))
        raise AssertionError('Unexpected request')

    def run(self):
        try:
            self.peer.send(self.hello)
            while True:
                raw = self.peer.recv(4096)
                if not raw:
                    return
                self.requests.append(raw)
                reply = self.reply(raw)
                if reply is None:
                    return
                if isinstance(reply, list):
                    for packet in reply:
                        self.peer.send(packet)
                else:
                    self.peer.send(reply)
        except OSError:
            pass
        except Exception as exc:
            self.errors.append(exc)
        finally:
            self.peer.close()

    def close(self):
        self.client.close()
        self.thread.join(2)
        if self.thread.is_alive():
            self.peer.close()
            raise AssertionError('Fixture responder did not stop')
        if self.errors:
            raise self.errors[0]

    def observe(self):
        obj, rgb = self.client.observe()
        return obj, rgb

    def args(self, command_id=COMMAND, observed='stand', desired='kneel'):
        return arguments(self.latest['capability'], self.latest['control_epoch'], command_id,
                         observed, desired)


class ValidationTests(unittest.TestCase):
    def test_exact_valid_schemas_and_rotated_context_epoch(self):
        self.assertEqual(s.validate_hello(dump({'schema': s.HELLO_SCHEMA, 'control_epoch': EPOCH})), EPOCH)
        obj = context(epoch='a' * 32)
        self.assertEqual(s.validate_context(dump(obj), CHALLENGE), (obj, RGB))
        for value in s.STATUSES:
            self.assertEqual(s.validate_status(dump(status(value=value)), COMMAND), status(value=value))
        s.validate_arguments(arguments())

    def test_hello_shapes_and_values(self):
        valid = {'schema': s.HELLO_SCHEMA, 'control_epoch': EPOCH}
        invalid = [{}, [], None, False, dict(valid, hidden='secret'),
                   dict(valid, schema='experimental-presentation/1')]
        for value in ('a' * 31, 'A' * 32, 'g' * 32, None, 123, [], {}):
            invalid.append(dict(valid, control_epoch=value))
        for obj in invalid:
            with self.subTest(obj=obj), self.assertRaises(Unavailable):
                s.validate_hello(dump(obj))

    def test_context_rejects_every_missing_and_unknown_field(self):
        for key in context():
            obj = context(); del obj[key]
            with self.subTest(key=key), self.assertRaises(Unavailable):
                s.validate_context(dump(obj), CHALLENGE)
        for key in ('unit_id', 'hidden_stats', 'stance', 'time_units', 'reserve', 'timestamp', 'reason'):
            obj = context(); obj[key] = 'PRIVATE-SENTINEL'
            with self.subTest(key=key), self.assertRaises(Unavailable):
                s.validate_context(dump(obj), CHALLENGE)
        for key in ('width', 'height', 'rgb_hex'):
            obj = context(); del obj['panel'][key]
            with self.subTest(key=key), self.assertRaises(Unavailable):
                s.validate_context(dump(obj), CHALLENGE)
        obj = context(); obj['panel']['unit_id'] = 99
        with self.assertRaises(Unavailable): s.validate_context(dump(obj), CHALLENGE)

    def test_context_rejects_invalid_geometry_tokens_pixels_and_semantics(self):
        mutations = [('schema', 'experimental-presentation/2'), ('challenge', '5' * 32),
                     ('semantics', 'current'), ('panel', None), ('panel', []), ('panel', True)]
        for key in ('challenge', 'control_epoch', 'capability'):
            mutations.extend((key, v) for v in ('a' * 31, 'A' * 32, 'z' * 32, 1, None, [], {}))
        for key, value in mutations:
            obj = context(); obj[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(Unavailable):
                s.validate_context(dump(obj), CHALLENGE)
        for key in ('width', 'height'):
            for value in (0, -1, True, False, 1.0, '320', None, [], {}, 2**32):
                obj = context(); obj['panel'][key] = value
                with self.subTest(key=key, value=value), self.assertRaises(Unavailable):
                    s.validate_context(dump(obj), CHALLENGE)
        for value in ('', RGB.hex()[:-2], RGB.hex() + '00', RGB.hex().upper(),
                      ' ' * (s.RGB_BYTES * 2), 'z' * (s.RGB_BYTES * 2), None, [], 3):
            obj = context(); obj['panel']['rgb_hex'] = value
            with self.subTest(value=str(value)[:20]), self.assertRaises(Unavailable):
                s.validate_context(dump(obj), CHALLENGE)

    def test_malformed_json_wrong_types_duplicates_and_oversize(self):
        packets = (b'', b'[]', b'null', b'false', b'42', b'\xff', b'{"x":NaN}',
                   b'[' * 2000, b'x' * (s.MAX_PACKET + 1), dump(context())[:-1],
                   dump(context()) + b'\0', dump(context())[:-1] + b',"schema":"duplicate"}',
                   None, 'hello', bytearray(dump(context())))
        for raw in packets:
            with self.subTest(raw=str(raw)[:30]), self.assertRaises(Unavailable):
                s.validate_context(raw, CHALLENGE)

    def test_status_exact_keys_identity_and_values(self):
        invalid = [{}, [], None, dict(status(), success=True),
                   dict(status(), schema='wrong'), dict(status(), command_id='5' * 32)]
        for key in status():
            obj = status(); del obj[key]; invalid.append(obj)
        for value in (True, False, None, [], {}, 'success', 'approved', 'unknown_outcome'):
            invalid.append(dict(status(), status=value))
        for obj in invalid:
            with self.subTest(obj=obj), self.assertRaises(Unavailable):
                s.validate_status(dump(obj), COMMAND)
        with self.assertRaises(Unavailable):
            s.validate_status(dump(status()) + b' ' * 257, COMMAND)

    def test_no_model_approval_boolean_or_extra_fields(self):
        for name in ('approved', 'human_approved', 'force', 'unit_id', 'ignore_tu'):
            with self.subTest(name=name), self.assertRaises(Unavailable):
                s.validate_arguments(dict(arguments(), **{name: True}))
        for name in arguments():
            obj = arguments(); del obj[name]
            with self.subTest(name=name), self.assertRaises(Unavailable): s.validate_arguments(obj)
        for name in ('observed', 'desired'):
            for value in (True, False, None, 'k', 's', 'toggle', [], {}):
                obj = arguments(); obj[name] = value
                with self.subTest(name=name, value=value), self.assertRaises(Unavailable):
                    s.validate_arguments(obj)


class ImageTests(unittest.TestCase):
    def test_png_losslessly_preserves_exact_panel(self):
        for rgb in (RGB, bytes(s.RGB_BYTES), random.Random(13).randbytes(s.RGB_BYTES)):
            obj = s.image_content(rgb)
            self.assertEqual((obj['type'], obj['mimeType']), ('image', 'image/png'))
            raw = base64.b64decode(obj['data'], validate=True)
            self.assertEqual(raw[:8], b'\x89PNG\r\n\x1a\n')
            chunks = []
            offset = 8
            while offset < len(raw):
                length, = struct.unpack('>I', raw[offset:offset + 4])
                kind = raw[offset + 4:offset + 8]
                payload = raw[offset + 8:offset + 8 + length]
                crc, = struct.unpack('>I', raw[offset + 8 + length:offset + 12 + length])
                self.assertEqual(crc, zlib.crc32(kind + payload) & 0xffffffff)
                chunks.append((kind, payload)); offset += length + 12
            self.assertEqual([kind for kind, _ in chunks], [b'IHDR', b'IDAT', b'IEND'])
            self.assertEqual(struct.unpack('>IIBBBBB', chunks[0][1]), (320, 56, 8, 2, 0, 0, 0))
            rows = zlib.decompress(chunks[1][1])
            self.assertEqual(len(rows), s.RGB_BYTES + s.HEIGHT)
            decoded = b''.join(rows[y * 961 + 1:(y + 1) * 961] for y in range(s.HEIGHT))
            self.assertEqual(decoded, rgb)
            self.assertTrue(all(rows[y * 961] == 0 for y in range(s.HEIGHT)))
            self.assertLessEqual(len(raw), s.RGB_BYTES + s.HEIGHT + 256)

    def test_rejects_inexact_pixel_type_or_length(self):
        for rgb in (None, '', bytearray(RGB), b'', RGB[:-1], RGB + b'\0'):
            with self.subTest(value=str(rgb)[:20]), self.assertRaises(Unavailable): s.image_content(rgb)


class ClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs)
        self.addCleanup(native.close)
        return native

    def assert_closed_no_retry(self, native, args=None):
        count = len(native.requests)
        self.assertIsNone(native.client.channel)
        self.assertEqual(native.client.state, 'CLOSED')
        for operation in (native.client.observe,
                          lambda: native.client.request(**(args or arguments())),
                          lambda: native.client.read_status(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)

    def test_wire_lengths_and_fresh_context_after_terminal(self):
        native = self.fixture()
        obj, rgb = native.observe()
        self.assertEqual(rgb, RGB)
        self.assertEqual(native.client.state, 'READY')
        args = native.args()
        receipt = native.client.request(**args)
        self.assertEqual(receipt['status'], 'pending_confirmation')
        self.assertEqual(native.client.state, 'PENDING')
        self.assertEqual(native.requests[0][:1], b'O')
        self.assertEqual(len(native.requests[0]), 33)
        self.assertEqual(native.requests[1], ('D' + EPOCH + obj['capability'] + COMMAND + 'sk').encode())
        self.assertEqual(len(native.requests[1]), 99)
        for _ in range(2):
            self.assertEqual(native.client.read_status(COMMAND)['status'], 'pending_confirmation')
        self.assertEqual(native.client.state, 'PENDING')
        native.status = 'dispatched'
        self.assertEqual(native.client.read_status(COMMAND)['status'], 'dispatched')
        self.assertEqual(native.client.state, 'NEED_CONTEXT')
        with self.assertRaises(Unavailable): native.client.request(**dict(args, command_id='a' * 32))
        new, _ = native.observe()
        self.assertNotEqual(new['capability'], obj['capability'])
        self.assertEqual(native.client.state, 'READY')
        native.status = 'pending_confirmation'
        native.client.request(**native.args(command_id='a' * 32, observed='kneel', desired='stand'))
        self.assertEqual(native.requests[-1][-2:], b'ks')
        self.assertEqual(sum(r[:1] == b'D' for r in native.requests), 2)

    def test_pending_blocks_observation_and_new_command_without_send(self):
        native = self.fixture(); native.observe(); args = native.args()
        native.client.request(**args)
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.observe()
        with self.assertRaises(Unavailable): native.client.request(**dict(args, command_id='a' * 32))
        self.assertEqual(len(native.requests), count)
        self.assertEqual(native.client.state, 'PENDING')
        self.assertIsNotNone(native.client.channel)

    def test_duplicate_queries_s_and_conflicting_id_never_sends(self):
        native = self.fixture(); native.observe(); args = native.args()
        native.client.request(**args)
        native.status = 'dispatched'
        self.assertEqual(native.client.request(**args)['status'], 'dispatched')
        self.assertEqual(native.requests[-1], b'S' + COMMAND.encode())
        self.assertEqual(sum(r[:1] == b'D' for r in native.requests), 1)
        count = len(native.requests)
        for key, value in (('desired', 'stand'), ('observed', 'kneel'),
                           ('control_epoch', 'a' * 32), ('capability', 'b' * 32)):
            with self.assertRaises(Unavailable): native.client.request(**dict(args, **{key: value}))
        self.assertEqual(len(native.requests), count)
        native.observe()
        with self.assertRaises(Unavailable): native.client.request(**native.args())
        self.assertEqual(sum(r[:1] == b'D' for r in native.requests), 1)

    def test_new_context_invalidates_previous_capability(self):
        native = self.fixture(); native.observe(); old = native.args()
        native.observe()
        with self.assertRaises(Unavailable): native.client.request(**old)
        self.assertEqual([r[:1] for r in native.requests], [b'O', b'O'])
        native.client.request(**native.args())

    def test_failed_replacement_context_invalidates_previous_ready_capability(self):
        native = self.fixture(); native.observe(); old = native.args()
        native.response = lambda raw, fixture: b'{"error":"unavailable"}'
        with self.assertRaises(Unavailable): native.observe()
        self.assert_closed_no_retry(native, old)
        self.assertEqual([r[:1] for r in native.requests], [b'O', b'O'])

    def test_same_stance_is_explicit_not_a_toggle(self):
        native = self.fixture(); native.observe()
        native.client.request(**native.args(observed='stand', desired='stand'))
        self.assertEqual(native.requests[-1][-2:], b'ss')

    def test_context_epoch_can_rotate_from_handshake_and_previous_context(self):
        def response(raw, native):
            native.counter += 1
            native.latest = context(raw[1:].decode(), f'{native.counter:032x}', epoch='b' * 32)
            return dump(native.latest)
        native = self.fixture(response=response)
        obj, _ = native.observe()
        self.assertEqual(native.client.control_epoch, 'b' * 32)
        self.assertEqual(obj['control_epoch'], 'b' * 32)
        with self.assertRaises(Unavailable): native.client.request(**dict(native.args(), control_epoch=EPOCH))
        self.assertEqual(len(native.requests), 1)

    def test_native_unavailable_is_terminal_not_success(self):
        native = self.fixture(); native.observe(); native.status = 'unavailable'
        self.assertEqual(native.client.request(**native.args())['status'], 'unavailable')
        self.assertEqual(native.client.state, 'NEED_CONTEXT')
        with self.assertRaises(Unavailable):
            native.client.request(**native.args(command_id='a' * 32))
        native.observe()
        self.assertEqual(native.client.state, 'READY')

    def test_initial_dispatched_is_not_accepted_as_local_confirmation(self):
        native = self.fixture(); native.observe(); native.status = 'dispatched'
        with self.assertRaises(Unavailable): native.client.request(**native.args())
        self.assert_closed_no_retry(native, native.args())

    def test_terminal_status_cannot_regress_or_change(self):
        for terminal, next_status in (('dispatched', 'pending_confirmation'),
                                      ('dispatched', 'unavailable'), ('unavailable', 'dispatched')):
            with self.subTest(terminal=terminal, next_status=next_status):
                native = self.fixture(); native.observe(); native.client.request(**native.args())
                native.status = terminal; native.client.read_status(COMMAND)
                native.status = next_status
                with self.assertRaises(Unavailable): native.client.read_status(COMMAND)
                self.assert_closed_no_retry(native)

    def test_unknown_status_id_does_not_touch_transport(self):
        native = self.fixture(); native.observe()
        with self.assertRaises(Unavailable): native.client.read_status('a' * 32)
        self.assertEqual(len(native.requests), 1)
        self.assertEqual(native.client.state, 'READY')

    def test_bad_hello_never_sends_observation(self):
        native = self.fixture(hello=b'{"schema":"wrong","control_epoch":"' + EPOCH.encode() + b'"}')
        with self.assertRaises(Unavailable): native.observe()
        self.assertEqual(native.requests, [])
        self.assert_closed_no_retry(native)

    def test_failed_context_and_oversize_close_permanently(self):
        for reply in (b'{"error":"unavailable"}', b'\xff', b'{}',
                      b'x' * (s.MAX_PACKET + 1), dump(context(challenge='5' * 32)),
                      dump(dict(context(), hidden_stats={'enemy': 1}))):
            with self.subTest(reply=reply[:30]):
                native = self.fixture(response=lambda raw, fixture, reply=reply: reply)
                with self.assertRaises(Unavailable): native.observe()
                self.assertEqual([r[:1] for r in native.requests], [b'O'])
                self.assert_closed_no_retry(native)

    def test_failed_post_dispatch_context_blocks_future_commands(self):
        native = self.fixture(); native.observe(); args = native.args()
        native.client.request(**args); native.status = 'dispatched'; native.client.read_status(COMMAND)
        native.response = lambda raw, fixture: b'{"error":"unavailable"}'
        with self.assertRaises(Unavailable): native.observe()
        self.assert_closed_no_retry(native, args)
        self.assertEqual(sum(r[:1] == b'D' for r in native.requests), 1)

    def test_disconnect_before_and_after_command_send_never_retries(self):
        native = self.fixture(); native.observe(); args = native.args()
        native.peer.shutdown(socket.SHUT_RDWR)
        native.thread.join(1)
        with self.assertRaises(Unavailable): native.client.request(**args)
        self.assertEqual(sum(r[:1] == b'D' for r in native.requests), 0)
        self.assert_closed_no_retry(native, args)
        native = self.fixture(); native.observe(); args = native.args()
        native.response = lambda raw, fixture: None
        with self.assertRaises(Unavailable): native.client.request(**args)
        self.assertEqual(sum(r[:1] == b'D' for r in native.requests), 1)
        self.assert_closed_no_retry(native, args)

    def test_pending_status_disconnect_never_replays_command(self):
        native = self.fixture(); native.observe(); args = native.args(); native.client.request(**args)
        native.response = lambda raw, fixture: None
        with self.assertRaises(Unavailable): native.client.read_status(COMMAND)
        self.assertEqual([r[:1] for r in native.requests], [b'O', b'D', b'S'])
        self.assert_closed_no_retry(native, args)

    def test_malformed_status_closes_without_d_replay(self):
        for reply in (b'{"error":"unavailable"}', b'{}', b'x' * 257,
                      dump(status(command_id='a' * 32)), dump(dict(status(), approved=True))):
            with self.subTest(reply=reply[:30]):
                native = self.fixture(); native.observe(); args = native.args()
                native.response = lambda raw, fixture, reply=reply: reply
                with self.assertRaises(Unavailable): native.client.request(**args)
                self.assertEqual(sum(r[:1] == b'D' for r in native.requests), 1)
                self.assert_closed_no_retry(native, args)

    def test_timeout_after_send_never_retries(self):
        native = self.fixture(); native.observe(); args = native.args()
        def timeout(raw, fixture):
            time.sleep(0.45)
            return dump(status())
        native.response = timeout
        with self.assertRaises(Unavailable): native.client.request(**args)
        self.assert_closed_no_retry(native, args)
        self.assertEqual(sum(r[:1] == b'D' for r in native.requests), 1)

    def test_unsolicited_packet_not_accepted_as_context_or_receipt(self):
        native = self.fixture(); native.observe(); args = native.args()
        native.peer.send(dump(status()))
        with self.assertRaises(Unavailable): native.client.request(**args)
        self.assertEqual(sum(r[:1] == b'D' for r in native.requests), 0)
        self.assert_closed_no_retry(native, args)

    def test_ancillary_descriptors_are_rejected_without_observation(self):
        parent, peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        client = s.StanceClient(parent, timeout=0.1, startup_timeout=0.1)
        read_fd, write_fd = os.pipe()
        try:
            hello = dump({'schema': s.HELLO_SCHEMA, 'control_epoch': EPOCH})
            peer.sendmsg([hello], [(socket.SOL_SOCKET, socket.SCM_RIGHTS, array.array('i', [read_fd]))])
            with self.assertRaises(Unavailable): client.observe()
            self.assertIsNone(client.channel)
            self.assertEqual(peer.recv(4096), b'')
        finally:
            client.close(); peer.close(); os.close(read_fd); os.close(write_fd)

    def test_bootstrap_disconnect_is_closed_without_automatic_reopen(self):
        parent, peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        client = s.StanceClient(parent, timeout=0.1, startup_timeout=0.1)
        peer.close()
        try:
            with self.assertRaises(Unavailable): client.observe()
            self.assertIsNone(client.channel)
            with self.assertRaises(Unavailable): client.observe()
        finally:
            client.close()

    def test_bounded_non_evicting_command_ledger(self):
        native = self.fixture()
        for number in range(s.MAX_COMMANDS):
            native.observe(); native.status = 'unavailable'
            native.client.request(**native.args(command_id=f'{number:032x}'))
        self.assertEqual(len(native.client.commands), s.MAX_COMMANDS)
        self.assertIn('0' * 32, native.client.commands)
        native.observe()
        with self.assertRaises(Unavailable): native.client.request(**native.args(command_id='f' * 32))
        self.assert_closed_no_retry(native)
        self.assertEqual(sum(r[:1] == b'D' for r in native.requests), s.MAX_COMMANDS)

    def test_concurrent_calls_do_not_send_or_unlock_other_flight(self):
        native = self.fixture(); native.observe()
        native.client.flight.acquire()
        try:
            with self.assertRaises(Unavailable): native.observe()
            with self.assertRaises(Unavailable): native.client.request(**native.args())
            self.assertEqual(len(native.requests), 1)
            self.assertTrue(native.client.flight.locked())
        finally:
            native.client.flight.release()
        native.client.request(**native.args())


class AdapterTests(unittest.TestCase):
    def test_tools_are_absent_by_default_and_require_explicit_client(self):
        self.assertFalse(any('stance' in tool['name'] for tool in adapter.definitions()))
        for name in ('read_stance_context', 'request_stance', 'read_stance_status'):
            self.assertIn('error', call(adapter.Server(), name))
        definitions = {tool['name']: tool for tool in adapter.definitions(supervised_stance=True)}
        self.assertEqual(set(definitions) - set(adapter.NAMES),
                         {'read_stance_context', 'request_stance', 'read_stance_status'})
        request = definitions['request_stance']
        self.assertFalse(request['annotations']['readOnlyHint'])
        self.assertTrue(request['annotations']['destructiveHint'])
        self.assertFalse(request['inputSchema']['additionalProperties'])
        self.assertEqual(set(request['inputSchema']['required']), set(arguments()))
        self.assertNotIn('approved', request['inputSchema']['properties'])

    def test_context_exposes_only_public_metadata_and_png(self):
        client = type('SyntheticClient', (), {'observe': lambda self: (context(), RGB)})()
        reply = call(adapter.Server(supervised_stance=client), 'read_stance_context')['result']
        self.assertFalse(reply['isError'])
        self.assertEqual(reply['structuredContent'], json.loads(reply['content'][0]['text']))
        self.assertEqual(reply['content'][1], s.image_content(RGB))
        meta = reply['structuredContent']
        self.assertEqual(set(meta), {'schema', 'challenge', 'control_epoch', 'capability', 'semantics', 'panel'})
        self.assertEqual(meta['schema'], s.PUBLIC_CONTEXT_SCHEMA)
        self.assertEqual(meta['panel'], {'width': 320, 'height': 56, 'format': 'image/png'})
        self.assertNotIn('rgb_hex', dump(reply).decode())
        self.assertNotIn('time_units', dump(reply).decode())

    def test_pending_dispatched_and_unavailable_receipts_are_not_success(self):
        for value in s.STATUSES:
            client = type('SyntheticClient', (), {
                'request': lambda self, **args: status(value=value),
                'read_status': lambda self, **args: status(value=value)})()
            for name, args in (('request_stance', arguments()), ('read_stance_status', {'command_id': COMMAND})):
                reply = call(adapter.Server(supervised_stance=client), name, args)['result']
                self.assertFalse(reply['isError'])
                self.assertEqual(reply['structuredContent']['status'], value)
                self.assertEqual(reply['structuredContent']['semantics'], 'dispatch_status_not_success')
                self.assertNotIn('success', reply['structuredContent'])

    def test_failures_are_generic_and_do_not_leak_detail(self):
        def fail(*args, **kwargs): raise Unavailable('PRIVATE-SENTINEL-path-or-unit')
        client = type('SyntheticClient', (), {'observe': fail, 'request': fail, 'read_status': fail})()
        server = adapter.Server(supervised_stance=client)
        for name, args in (('read_stance_context', {}), ('request_stance', arguments()),
                           ('read_stance_status', {'command_id': COMMAND})):
            reply = call(server, name, args)['result']
            self.assertTrue(reply['isError'])
            self.assertEqual(reply['content'], [{'type': 'text', 'text': '{"error":"unavailable"}'}])
            self.assertEqual(reply['structuredContent'], {'error': 'unavailable'})
            self.assertNotIn('PRIVATE', dump(reply).decode())

    def test_wrong_mcp_arguments_never_reach_client(self):
        def fail(*args, **kwargs): self.fail('Invalid MCP arguments reached transport')
        client = type('NeverCall', (), {'observe': fail, 'request': fail, 'read_status': fail})()
        server = adapter.Server(supervised_stance=client)
        server.state = 'ready'
        for name, invalid in (
                ('read_stance_context', [None, [], False, {'force': True}]),
                ('request_stance', [None, [], False, {}, dict(arguments(), approved=True),
                                    dict(arguments(), observed=True), dict(arguments(), command_id='bad')]),
                ('read_stance_status', [None, [], False, {}, {'command_id': COMMAND, 'retry': True},
                                       {'command_id': True}])):
            for args in invalid:
                params = {'name': name, 'arguments': args}
                result = server.handle({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call', 'params': params})
                self.assertEqual(result['error']['code'], -32602)

    def test_stance_and_observation_failures_are_independent(self):
        def fail(*args, **kwargs): raise Unavailable('Unavailable')
        stance = type('SyntheticStance', (), {'observe': lambda self: (context(), RGB)})()
        presentation = type('FailedPresentation', (), {'observe': fail})()
        server = adapter.Server(presentation=presentation, supervised_stance=stance)
        self.assertTrue(call(server, 'read_presented_cursor')['result']['isError'])
        self.assertFalse(call(server, 'read_stance_context')['result']['isError'])
        server.supervised_stance = type('FailedStance', (), {'observe': fail})()
        with patch.object(server, 'historical', side_effect=AssertionError('No fallback read')):
            self.assertTrue(call(server, 'read_stance_context')['result']['isError'])

    def test_largest_png_and_unicode_request_id_fit_mcp_response_bound(self):
        rgb = random.Random(11).randbytes(s.RGB_BYTES)
        client = type('SyntheticClient', (), {'observe': lambda self: (context(rgb=rgb), rgb)})()
        request = {'jsonrpc': '2.0', 'id': '', 'method': 'tools/call',
                   'params': {'name': 'read_stance_context', 'arguments': {}}}
        room = adapter.MAX_MESSAGE - len(dump(request)) - 1
        request['id'] = '\U0001f600' * (room // 4)
        encoded = json.dumps(request, separators=(',', ':'), ensure_ascii=False).encode() + b'\n'
        initialize = {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
                      'params': {'protocolVersion': adapter.PROTOCOL, 'capabilities': {},
                                 'clientInfo': {'name': 'synthetic', 'version': '1'}}}
        ready = {'jsonrpc': '2.0', 'method': 'notifications/initialized'}
        self.assertLessEqual(len(encoded), adapter.MAX_MESSAGE)
        source = io.BytesIO(dump(initialize) + b'\n' + dump(ready) + b'\n' + encoded)
        sink = io.BytesIO()
        adapter.serve(source, sink, supervised_stance=client)
        replies = sink.getvalue().splitlines(keepends=True)
        self.assertEqual(len(replies), 2)
        self.assertLessEqual(len(replies[-1]), adapter.MAX_RESPONSE)
        result = json.loads(replies[-1])
        self.assertEqual(result['id'], request['id'])
        self.assertFalse(result['result']['isError'])


class LauncherTests(unittest.TestCase):
    def test_explicit_opt_in_and_independent_fds_scrub_ambient_stance(self):
        for flags, enabled, count in (([], False, 1), (['--audio-only'], False, 1),
                                     (['--supervised-stance'], True, 2),
                                     (['--supervised-stance', '--presented-hud', '--mixed-audio'], True, 3)):
            with self.subTest(flags=flags), \
                    patch.object(sys, 'argv', ['launcher', *flags, '--', 'synthetic-game']), \
                    patch.dict(os.environ, {'OPENXCOM_STANCE_FD': '789'}), \
                    patch('launch_presentation.subprocess.Popen') as spawn, \
                    patch('launch_presentation.serve') as serve:
                launch_presentation.main()
                env = spawn.call_args.kwargs['env']
                self.assertEqual('OPENXCOM_STANCE_FD' in env, enabled)
                fds = spawn.call_args.kwargs['pass_fds']
                self.assertEqual(len(set(fds)), count)
                self.assertEqual(serve.call_args.args[6] is not None, enabled)
                if enabled:
                    self.assertIn(int(env['OPENXCOM_STANCE_FD']), fds)
                    self.assertNotEqual(env['OPENXCOM_STANCE_FD'], env['OPENXCOM_PRESENTATION_FD'])
                    self.assertIsNone(serve.call_args.args[6].channel)
                spawn.return_value.kill.assert_not_called()
                spawn.return_value.terminate.assert_not_called()

    def test_audio_only_rejects_stance_without_spawning(self):
        with patch.object(sys, 'argv', ['launcher', '--audio-only', '--supervised-stance', '--', 'game']), \
                patch('launch_presentation.subprocess.Popen') as spawn, \
                patch('sys.stderr', io.StringIO()), self.assertRaises(SystemExit):
            launch_presentation.main()
        spawn.assert_not_called()

    def test_wrong_socket_kind_and_timeouts_are_rejected(self):
        one, two = socket.socketpair()
        try:
            with self.assertRaises(Unavailable): s.StanceClient(one)
        finally:
            one.close(); two.close()
        for value in (0, -1, True, float('nan'), float('inf'), 2.1):
            with self.subTest(timeout=value), self.assertRaises(Unavailable): s.StanceClient(None, timeout=value)
        for value in (0, -1, True, float('nan'), float('inf'), 31.1):
            with self.subTest(startup_timeout=value), self.assertRaises(Unavailable):
                s.StanceClient(None, startup_timeout=value)


if __name__ == '__main__':
    unittest.main()
