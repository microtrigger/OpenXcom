# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic tactical pixels and shared-channel ordinary-right-click checks."""
import base64
import io
import json
import os
from pathlib import Path
import random
import socket
import struct
import subprocess
import sys
import time
import unittest
import zlib
from unittest.mock import patch

import adapter
import launch_presentation
import supervised_stance as s
from snapshots import Unavailable
from test_supervised_stance import (CAPABILITY, CHALLENGE, COMMAND, EPOCH, NativeFixture,
                                    arguments, call, context, dump, status)
from test_supervised_selection import selection_arguments


def tactical_context(challenge=CHALLENGE, capability=CAPABILITY, epoch=EPOCH,
                     indices=None, palette=None):
    if indices is None: indices = bytes(s.TACTICAL_PIXELS)
    if palette is None: palette = bytes.fromhex('0180ff') + bytes(765)
    return {'schema': s.TACTICAL_CONTEXT_SCHEMA, 'challenge': challenge,
            'control_epoch': epoch, 'capability': capability,
            'semantics': 'presented_after_request',
            'view': {'width': 320, 'height': 200, 'indices_hex': indices.hex(),
                     'palette_hex': palette.hex()}}


def noisy_context():
    rng = random.Random(17)
    colors = [rng.randbytes(3) for _ in range(256)]
    if len(set(colors)) != 256: raise AssertionError('Fixture colors must be distinct')
    indices = bytes(range(256)) + rng.randbytes(s.TACTICAL_PIXELS - 256)
    return tactical_context(indices=indices, palette=b''.join(colors))


def turn_arguments(capability=CAPABILITY, control_epoch=EPOCH, command_id=COMMAND,
                   x=160, y=80):
    return dict(capability=capability, control_epoch=control_epoch,
                command_id=command_id, x=x, y=y)


def decoded_rgb(obj):
    palette = bytes.fromhex(obj['view']['palette_hex'])
    return b''.join(palette[index * 3:index * 3 + 3]
                    for index in bytes.fromhex(obj['view']['indices_hex']))


def decode_png(test, content):
    """Independent decoder for the exact generated PNG subset and RGB oracle."""
    test.assertEqual((content['type'], content['mimeType']), ('image', 'image/png'))
    raw = base64.b64decode(content['data'], validate=True)
    test.assertEqual(raw[:8], b'\x89PNG\r\n\x1a\n')
    chunks, offset = [], 8
    while offset < len(raw):
        length, = struct.unpack('>I', raw[offset:offset + 4])
        kind = raw[offset + 4:offset + 8]
        payload = raw[offset + 8:offset + 8 + length]
        crc, = struct.unpack('>I', raw[offset + 8 + length:offset + 12 + length])
        test.assertEqual(crc, zlib.crc32(kind + payload) & 0xffffffff)
        chunks.append((kind, payload)); offset += length + 12
    test.assertEqual(offset, len(raw))
    test.assertEqual([kind for kind, _ in chunks], [b'IHDR', b'PLTE', b'IDAT', b'IEND'])
    test.assertEqual(struct.unpack('>IIBBBBB', chunks[0][1]), (320, 200, 8, 3, 0, 0, 0))
    palette = chunks[1][1]
    test.assertEqual(len(palette), 768)
    test.assertEqual(chunks[3][1], b'')
    rows = zlib.decompress(chunks[2][1])
    test.assertEqual(len(rows), 64200)
    test.assertTrue(all(rows[y * 321] == 0 for y in range(200)))
    indices = b''.join(rows[y * 321 + 1:(y + 1) * 321] for y in range(200))
    rgb = b''.join(palette[index * 3:index * 3 + 3] for index in indices)
    return raw, rgb, indices, palette


class TurnFixture(NativeFixture):
    def __init__(self, **kwargs):
        kwargs.setdefault('selection_enabled', True)
        kwargs.setdefault('turn_enabled', True)
        super().__init__(**kwargs)

    def reply(self, raw):
        if self.response is not None: return self.response(raw, self)
        if raw[:1] == b'V':
            self.counter += 1
            self.latest = tactical_context(raw[1:].decode(), f'{self.counter:032x}')
            return dump(self.latest)
        if raw[:1] == b'R': return dump(status(raw[65:97].decode(), self.status))
        return super().reply(raw)

    def observe_tactical(self):
        return self.client.observe_tactical()

    def turn_args(self, **kwargs):
        return turn_arguments(self.latest['capability'], self.latest['control_epoch'], **kwargs)


class ValidationTests(unittest.TestCase):
    def test_valid_full_palette_packet_bound_and_rgb_reconstruction(self):
        for obj in (tactical_context(), noisy_context(), tactical_context(epoch='a' * 32),
                    tactical_context(palette=bytes(768))):
            self.assertEqual(s.validate_tactical_context(dump(obj), CHALLENGE), (obj, decoded_rgb(obj)))
            self.assertEqual(len(decoded_rgb(obj)), 192000)
            self.assertEqual(len(obj['view']['indices_hex']), 128000)
            self.assertEqual(len(obj['view']['palette_hex']), 1536)
            self.assertLessEqual(len(dump(obj)), s.MAX_PACKET)
        self.assertEqual(len(dump(noisy_context())), 129832)

    def test_exact_keys_reject_missing_and_hidden_alternatives(self):
        for inner in (False, True):
            original = tactical_context()['view'] if inner else tactical_context()
            for name in original:
                obj = tactical_context(); del (obj['view'] if inner else obj)[name]
                with self.subTest(name=name, inner=inner), self.assertRaises(Unavailable):
                    s.validate_tactical_context(dump(obj), CHALLENGE)
            for name in ('unit_id', 'world_x', 'direction', 'cost', 'busy', 'timestamp', 'panel'):
                obj = tactical_context(); (obj['view'] if inner else obj)[name] = 'sentinel'
                with self.subTest(name=name, inner=inner), self.assertRaises(Unavailable):
                    s.validate_tactical_context(dump(obj), CHALLENGE)

    def test_wrong_schema_tokens_semantics_and_geometry(self):
        for name in ('schema', 'semantics', 'challenge', 'control_epoch', 'capability', 'view'):
            for value in (None, False, 1, [], {}, '', 'a' * 31, 'A' * 32, 'g' * 32):
                obj = tactical_context(); obj[name] = value
                with self.subTest(name=name, value=value), self.assertRaises(Unavailable):
                    s.validate_tactical_context(dump(obj), CHALLENGE)
        for name, limit in (('width', 320), ('height', 200)):
            for value in (limit - 1, limit + 1, True, False, float(limit), str(limit), None, [], {}):
                obj = tactical_context(); obj['view'][name] = value
                with self.subTest(name=name, value=value), self.assertRaises(Unavailable):
                    s.validate_tactical_context(dump(obj), CHALLENGE)
        with self.assertRaises(Unavailable):
            s.validate_tactical_context(dump(tactical_context(challenge='a' * 32)), CHALLENGE)

    def test_exact_lowercase_hex_types_and_lengths(self):
        for name, size in (('indices_hex', 128000), ('palette_hex', 1536)):
            for value in (None, True, 3, [], {}, '', '00' * (size // 2 - 1),
                          '00' * (size // 2 + 1), 'fF' * (size // 2), ' ' * size, 'z' * size):
                obj = tactical_context(); obj['view'][name] = value
                with self.subTest(name=name, value=str(value)[:20]), self.assertRaises(Unavailable):
                    s.validate_tactical_context(dump(obj), CHALLENGE)

    def test_aliases_permutations_and_nonzero_unused_palette_rejected(self):
        valid = tactical_context(indices=b'\0\1' * 32000,
                                 palette=bytes.fromhex('0180ffff0001') + bytes(762))
        s.validate_tactical_context(dump(valid), CHALLENGE)
        swapped = tactical_context(indices=b'\1\0' * 32000,
                                   palette=bytes.fromhex('ff00010180ff') + bytes(762))
        self.assertEqual(decoded_rgb(valid), decoded_rgb(swapped))
        duplicate = tactical_context(indices=b'\0\1' * 32000,
                                     palette=bytes.fromhex('0180ff0180ff') + bytes(762))
        self.assertEqual(decoded_rgb(duplicate), decoded_rgb(tactical_context()))
        invalid = [swapped, duplicate, tactical_context(indices=b'\0\2' * 32000,
                   palette=bytes.fromhex('0180ff000000ff0001') + bytes(759))]
        for position in (3, 383, 767):
            palette = bytearray(bytes.fromhex('0180ff') + bytes(765)); palette[position] = 1
            obj = tactical_context(palette=palette)
            self.assertEqual(decoded_rgb(obj), decoded_rgb(tactical_context())); invalid.append(obj)
        for obj in invalid:
            with self.subTest(prefix=obj['view']['indices_hex'][:8]), self.assertRaises(Unavailable):
                s.validate_tactical_context(dump(obj), CHALLENGE)

    def test_zero_color_may_be_used_but_not_aliased(self):
        obj = tactical_context(indices=b'\0\1' * 32000,
                               palette=bytes.fromhex('0000000180ff') + bytes(762))
        self.assertEqual(s.validate_tactical_context(dump(obj), CHALLENGE)[1], decoded_rgb(obj))
        obj['view']['palette_hex'] = bytes(768).hex()
        with self.assertRaises(Unavailable): s.validate_tactical_context(dump(obj), CHALLENGE)

    def test_malformed_duplicate_nonfinite_and_max_packet_boundary(self):
        raw = dump(tactical_context())
        for packet in (b'', b'[]', b'null', b'false', b'42', b'\xff', b'{"x":NaN}',
                       b'[' * 2000, raw[:-1], raw + b'\0', raw[:-1] + b',"schema":"duplicate"}',
                       b'x' * (s.MAX_PACKET + 1), None, 'text', bytearray(raw)):
            with self.subTest(packet=str(packet)[:20]), self.assertRaises(Unavailable):
                s.validate_tactical_context(packet, CHALLENGE)
        padded = raw + b' ' * (s.MAX_PACKET - len(raw))
        self.assertEqual(s.validate_tactical_context(padded, CHALLENGE)[0], tactical_context())
        with self.assertRaises(Unavailable): s.validate_tactical_context(padded + b' ', CHALLENGE)

    def test_arguments_exact_integer_bounds_tokens_and_no_approval(self):
        for x in (16, 160, 303):
            for y in (40, 80, 127): s.validate_turn_arguments(turn_arguments(x=x, y=y))
        invalid = [None, [], False, {}, arguments(), selection_arguments()]
        for name in turn_arguments():
            obj = turn_arguments(); del obj[name]; invalid.append(obj)
        for name in ('approved', 'human_approved', 'force', 'unit_id', 'target', 'direction',
                     'cost', 'world_x', 'world_y', 'button', 'modifiers'):
            invalid.append(dict(turn_arguments(), **{name: True}))
        for name, below, above in (('x', 15, 304), ('y', 39, 128)):
            for value in (None, True, False, 16.0, 80.0, '80', [], {}, below, above, -1, 2**64):
                invalid.append(dict(turn_arguments(), **{name: value}))
        for name in ('capability', 'control_epoch', 'command_id'):
            for value in (None, False, 1, [], {}, 'a' * 31, 'A' * 32, 'z' * 32):
                invalid.append(dict(turn_arguments(), **{name: value}))
        for obj in invalid:
            with self.subTest(obj=obj), self.assertRaises(Unavailable): s.validate_turn_arguments(obj)

    def test_turn_opt_in_is_strict_boolean(self):
        for value in (None, 0, 1, '1', 'true', [], {}):
            with self.subTest(value=value), self.assertRaises(Unavailable):
                s.StanceClient(None, turn_enabled=value)


class ImageTests(unittest.TestCase):
    def test_canonical_indexed_png_roundtrip_and_noise_response_bound(self):
        for obj in (tactical_context(), noisy_context(), tactical_context(palette=bytes(768))):
            rgb = decoded_rgb(obj); content = s.tactical_image_content(rgb)
            png, actual, indices, palette = decode_png(self, content)
            self.assertEqual(actual, rgb)
            self.assertEqual(indices.hex(), obj['view']['indices_hex'])
            self.assertEqual(palette.hex(), obj['view']['palette_hex'])
            self.assertLessEqual(len(png), s.TACTICAL_PIXELS + s.TACTICAL_HEIGHT + s.PALETTE_BYTES + 256)
            self.assertLess(len(dump({'content': [content]})), adapter.MAX_RESPONSE)
            self.assertEqual(content, s.tactical_image_content(rgb))

    def test_rejects_invalid_rgb_type_length_or_more_than_256_colors(self):
        rgb = decoded_rgb(tactical_context())
        for value in (None, '', bytearray(rgb), b'', rgb[:-1], rgb + b'\0',
                      b''.join(i.to_bytes(3, 'big') for i in range(257)) + rgb[257 * 3:]):
            with self.subTest(value=str(value)[:20]), self.assertRaises(Unavailable):
                s.tactical_image_content(value)


class ClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = TurnFixture(**kwargs); self.addCleanup(native.close); return native

    def assert_closed_no_retry(self, native):
        count = len(native.requests)
        self.assertEqual(native.client.state, 'CLOSED')
        self.assertIsNone(native.client.channel)
        self.assertIsNone(native.client.capability)
        self.assertIsNone(native.client.capability_kind)
        for operation in (native.observe, native.observe_tactical,
                          lambda: native.client.request_turn(**turn_arguments()),
                          lambda: native.client.request(**arguments()),
                          lambda: native.client.request_selection(**selection_arguments()),
                          lambda: native.client.read_status(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)

    def test_disabled_turn_never_sends_or_retires_panel_capability(self):
        native = self.fixture(turn_enabled=False); native.observe()
        for operation in (native.observe_tactical, lambda: native.client.request_turn(**native.turn_args())):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual([r[:1] for r in native.requests], [b'O'])
        self.assertEqual(native.client.state, 'READY')
        native.client.request(**native.args())
        self.assertEqual([r[:1] for r in native.requests], [b'O', b'D'])

    def test_exact_33_and_103_byte_wire_with_coordinate_boundaries(self):
        native = self.fixture()
        for index, (x, y) in enumerate(((16, 40), (303, 127), (160, 80))):
            obj, rgb = native.observe_tactical()
            self.assertEqual(native.requests[-1], b'V' + obj['challenge'].encode())
            self.assertEqual(len(native.requests[-1]), 33)
            self.assertEqual(rgb, decoded_rgb(obj))
            args = native.turn_args(command_id=f'{index:032x}', x=x, y=y)
            self.assertEqual(native.client.request_turn(**args)['status'], 'pending_confirmation')
            expected = ('R' + EPOCH + args['capability'] + args['command_id'] + f'{x:03d}{y:03d}').encode()
            self.assertEqual(native.requests[-1], expected); self.assertEqual(len(expected), 103)
            self.assertEqual(native.client.state, 'PENDING'); self.assertIsNone(native.client.capability)
            self.assertIsNone(native.client.capability_kind)
            native.status = 'dispatched'; native.client.read_status(args['command_id'])
            self.assertEqual(native.client.state, 'NEED_CONTEXT'); native.status = 'pending_confirmation'

    def test_type_bound_capabilities_all_directions(self):
        native = self.fixture(); native.observe_tactical()
        for operation in (lambda: native.client.request(**native.args()),
                          lambda: native.client.request_selection(**selection_arguments(native.latest['capability'], EPOCH))):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual([r[:1] for r in native.requests], [b'V'])
        self.assertEqual(native.client.capability_kind, 'V'); native.observe()
        with self.assertRaises(Unavailable): native.client.request_turn(**native.turn_args())
        self.assertEqual([r[:1] for r in native.requests], [b'V', b'O'])
        self.assertEqual(native.client.capability_kind, 'O')
        native.client.request_selection(**selection_arguments(native.latest['capability'], EPOCH))
        self.assertEqual(native.requests[-1][:1], b'N')

    def test_either_observation_retires_previous_grant(self):
        for tactical in (True, False):
            native = self.fixture()
            first = native.observe_tactical if tactical else native.observe
            second = native.observe if tactical else native.observe_tactical
            first(); old_turn, old_stance = native.turn_args(), native.args(); second()
            count = len(native.requests)
            with self.assertRaises(Unavailable):
                (native.client.request_turn(**old_turn) if tactical else native.client.request(**old_stance))
            self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'READY')

    def test_pending_modal_blocks_both_contexts_and_new_actions(self):
        native = self.fixture(); native.observe_tactical(); args = native.turn_args()
        native.client.request_turn(**args); count = len(native.requests)
        for operation in (native.observe, native.observe_tactical,
                          lambda: native.client.request_turn(**dict(args, command_id='a' * 32)),
                          lambda: native.client.request(**native.args(command_id='b' * 32)),
                          lambda: native.client.request_selection(**selection_arguments(command_id='c' * 32))):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'PENDING')

    def test_dispatched_requires_tactical_before_panel_or_another_action(self):
        native = self.fixture(); native.observe_tactical(); args = native.turn_args()
        native.client.request_turn(**args); native.status = 'dispatched'; native.client.read_status(COMMAND)
        self.assertTrue(native.client.tactical_verification_required); count = len(native.requests)
        with self.assertRaises(Unavailable): native.observe()
        with self.assertRaises(Unavailable): native.client.request_turn(**dict(args, command_id='a' * 32))
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'NEED_CONTEXT')
        native.observe_tactical(); self.assertFalse(native.client.tactical_verification_required)
        native.observe(); native.status = 'pending_confirmation'
        native.client.request(**native.args(command_id='a' * 32))
        self.assertEqual([r[:1] for r in native.requests], [b'V', b'R', b'S', b'V', b'O', b'D'])

    def test_immediate_unavailable_or_cancellation_permits_either_fresh_kind(self):
        for immediate in (True, False):
            for tactical in (True, False):
                native = self.fixture(); native.observe_tactical()
                if immediate: native.status = 'unavailable'
                native.client.request_turn(**native.turn_args())
                if not immediate:
                    native.status = 'unavailable'; native.client.read_status(COMMAND)
                self.assertFalse(native.client.tactical_verification_required)
                self.assertEqual(native.client.state, 'NEED_CONTEXT')
                (native.observe_tactical if tactical else native.observe)()
                self.assertEqual(native.client.state, 'READY')

    def test_duplicate_only_queries_status_leaving_new_grant_intact(self):
        native = self.fixture(); native.observe_tactical(); args = native.turn_args()
        native.client.request_turn(**args)
        for value in ('pending_confirmation', 'dispatched', 'dispatched'):
            native.status = value; self.assertEqual(native.client.request_turn(**args)['status'], value)
            self.assertEqual(native.requests[-1], b'S' + COMMAND.encode())
        native.observe_tactical(); native.observe(); capability = native.client.capability
        native.client.request_turn(**args)
        self.assertEqual(native.requests[-1], b'S' + COMMAND.encode())
        self.assertEqual(native.client.state, 'READY'); self.assertEqual(native.client.capability, capability)
        self.assertFalse(native.client.tactical_verification_required)
        self.assertEqual(sum(r[:1] == b'R' for r in native.requests), 1)

    def test_coordinates_and_cross_action_command_id_conflicts_never_send(self):
        for action in ('D', 'N', 'R'):
            native = self.fixture(); (native.observe_tactical if action == 'R' else native.observe)()
            stance_args, turn_args = native.args(), native.turn_args()
            selection_args = selection_arguments(native.latest['capability'], EPOCH)
            methods = {'D': lambda: native.client.request(**stance_args),
                       'N': lambda: native.client.request_selection(**selection_args),
                       'R': lambda: native.client.request_turn(**turn_args)}
            methods[action](); count = len(native.requests)
            for other, operation in methods.items():
                if other != action:
                    with self.assertRaises(Unavailable): operation()
            if action == 'R':
                for name, value in (('x', 161), ('y', 81), ('capability', 'a' * 32), ('control_epoch', 'b' * 32)):
                    with self.assertRaises(Unavailable):
                        native.client.request_turn(**dict(turn_args, **{name: value}))
            self.assertEqual(len(native.requests), count)

    def test_wrong_epoch_and_stale_capability_never_send(self):
        native = self.fixture(); native.observe_tactical(); old = native.turn_args()
        native.observe_tactical(); count = len(native.requests)
        for args in (old, dict(native.turn_args(), control_epoch='a' * 32)):
            with self.assertRaises(Unavailable): native.client.request_turn(**args)
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'READY')

    def test_shared_non_evicting_64_command_ledger(self):
        native = self.fixture(); native.status = 'unavailable'
        for index in range(s.MAX_COMMANDS):
            ident = f'{index:032x}'
            if index % 3 == 0:
                native.observe_tactical(); native.client.request_turn(**native.turn_args(command_id=ident))
            else:
                native.observe()
                if index % 3 == 1: native.client.request(**native.args(command_id=ident))
                else: native.client.request_selection(**selection_arguments(native.latest['capability'], EPOCH, ident))
        self.assertEqual(len(native.client.commands), 64); self.assertIn('0' * 32, native.client.commands)
        native.observe_tactical()
        with self.assertRaises(Unavailable): native.client.request_turn(**native.turn_args(command_id='f' * 32))
        self.assert_closed_no_retry(native)
        self.assertEqual(sum(r[:1] in (b'D', b'N', b'R') for r in native.requests), 64)

    def test_wrong_kind_malformed_replayed_and_overfull_observation_closes(self):
        responses = [lambda raw, native: dump(context(raw[1:].decode())),
                     lambda raw, native: dump(tactical_context()), lambda raw, native: b'{}',
                     lambda raw, native: dump(tactical_context(raw[1:].decode())) + b'x' * s.MAX_PACKET,
                     lambda raw, native: dump(tactical_context(raw[1:].decode(), palette=bytes(767) + b'\1'))]
        for response in responses:
            native = self.fixture(response=response)
            with self.assertRaises(Unavailable): native.observe_tactical()
            self.assert_closed_no_retry(native); self.assertEqual([r[:1] for r in native.requests], [b'V'])
        native = self.fixture(); native.observe_tactical(); old = native.latest
        native.response = lambda raw, fixture: dump(old)
        with self.assertRaises(Unavailable): native.observe_tactical()
        self.assert_closed_no_retry(native)

    def test_reused_capability_across_context_kind_closes(self):
        native = self.fixture(); native.observe(); old = native.latest['capability']
        native.response = lambda raw, fixture: dump(tactical_context(raw[1:].decode(), old))
        with self.assertRaises(Unavailable): native.observe_tactical()
        self.assert_closed_no_retry(native)

    def test_malformed_or_initial_dispatched_receipt_closes(self):
        for response in (b'{}', b'\xff', b'x' * 257, dump(status(value='dispatched')),
                         dump(status(command_id='a' * 32)), dump(dict(status(), approved=True))):
            native = self.fixture(); native.observe_tactical()
            native.response = lambda raw, fixture, response=response: response
            with self.assertRaises(Unavailable): native.client.request_turn(**native.turn_args())
            self.assert_closed_no_retry(native); self.assertEqual(sum(r[:1] == b'R' for r in native.requests), 1)

    def test_terminal_status_regression_closes(self):
        for initial, changed in (('dispatched', 'unavailable'), ('unavailable', 'dispatched'),
                                 ('dispatched', 'pending_confirmation')):
            native = self.fixture(); native.observe_tactical(); native.client.request_turn(**native.turn_args())
            native.status = initial; native.client.read_status(COMMAND); native.status = changed
            with self.assertRaises(Unavailable): native.client.read_status(COMMAND)
            self.assert_closed_no_retry(native)

    def test_disconnect_or_timeout_after_send_never_retries(self):
        for mode in ('before', 'after', 'timeout'):
            native = self.fixture(); native.observe_tactical()
            if mode == 'before':
                native.peer.shutdown(socket.SHUT_RDWR); native.thread.join(1)
            elif mode == 'after': native.response = lambda raw, fixture: None
            else:
                def timeout(raw, fixture):
                    time.sleep(0.45); return dump(status())
                native.response = timeout
            with self.assertRaises(Unavailable): native.client.request_turn(**native.turn_args())
            self.assert_closed_no_retry(native)
            self.assertEqual(sum(r[:1] == b'R' for r in native.requests), 0 if mode == 'before' else 1)

    def test_failed_postdispatch_observation_closes_without_resend(self):
        native = self.fixture(); native.observe_tactical(); native.client.request_turn(**native.turn_args())
        native.status = 'dispatched'; native.client.read_status(COMMAND)
        native.response = lambda raw, fixture: b'{}'
        with self.assertRaises(Unavailable): native.observe_tactical()
        self.assert_closed_no_retry(native)
        self.assertEqual([r[:1] for r in native.requests], [b'V', b'R', b'S', b'V'])

    def test_naturally_delayed_postdispatch_frame_is_accepted(self):
        native = self.fixture(); native.observe_tactical(); native.client.request_turn(**native.turn_args())
        native.status = 'dispatched'; native.client.read_status(COMMAND)
        def delayed(raw, fixture):
            time.sleep(0.05); return dump(tactical_context(raw[1:].decode(), 'b' * 32))
        native.response = delayed; obj, rgb = native.observe_tactical()
        self.assertEqual(obj['capability'], 'b' * 32); self.assertEqual(rgb, decoded_rgb(obj))
        self.assertFalse(native.client.tactical_verification_required)

    def test_concurrent_call_does_not_send_or_unlock_other_call(self):
        native = self.fixture(); native.observe_tactical(); native.client.flight.acquire()
        try:
            for operation in (native.observe_tactical, lambda: native.client.request_turn(**native.turn_args())):
                with self.assertRaises(Unavailable): operation()
            self.assertEqual([r[:1] for r in native.requests], [b'V']); self.assertTrue(native.client.flight.locked())
        finally: native.client.flight.release()
        native.client.request_turn(**native.turn_args())


class AdapterTests(unittest.TestCase):
    def test_hidden_without_explicit_turn_client_and_legacy_definitions_unchanged(self):
        for kwargs in ({}, {'supervised_stance': True}, {'supervised_turn': True},
                       {'supervised_stance': True, 'supervised_selection': True}):
            self.assertFalse({'read_tactical_context', 'request_turn'} &
                             {t['name'] for t in adapter.definitions(**kwargs)})
        self.assertEqual(adapter.definitions(supervised_stance=True),
                         adapter.definitions(supervised_stance=True, supervised_turn=False))
        for enabled in (None, False, 1, '1'):
            server = adapter.Server(supervised_stance=type('Disabled', (), {'turn_enabled': enabled})())
            for name, args in (('request_turn', turn_arguments()), ('read_tactical_context', {})):
                self.assertIn('error', call(server, name, args))
            listed = server.handle({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'})
            self.assertNotIn('request_turn', [t['name'] for t in listed['result']['tools']])

    def test_exact_schema_and_normal_right_click_description(self):
        request = next(t for t in adapter.definitions(supervised_stance=True, supervised_turn=True)
                       if t['name'] == 'request_turn')
        self.assertEqual(set(request['inputSchema']['properties']), set(turn_arguments()))
        self.assertEqual(set(request['inputSchema']['required']), set(turn_arguments()))
        self.assertFalse(request['inputSchema']['additionalProperties'])
        for name, minimum, maximum in (('x', 16, 303), ('y', 40, 127)):
            self.assertEqual(request['inputSchema']['properties'][name],
                             {'type': 'integer', 'minimum': minimum, 'maximum': maximum})
        self.assertFalse(request['annotations']['readOnlyHint'])
        self.assertTrue(request['annotations']['destructiveHint']); self.assertTrue(request['annotations']['idempotentHint'])
        for phrase in ('ordinary right-click', 'open a door', 'normal game rules', 'read_stance_status', 'fresh tactical image'):
            self.assertIn(phrase, request['description'])

    def test_public_metadata_has_no_native_palette_or_indices(self):
        obj = noisy_context(); rgb = decoded_rgb(obj)
        client = type('Enabled', (), {'turn_enabled': True, 'observe_tactical': lambda self: (obj, rgb)})()
        reply = call(adapter.Server(supervised_stance=client), 'read_tactical_context', {})['result']
        self.assertFalse(reply['isError']); meta = reply['structuredContent']
        self.assertEqual(meta, {'schema': s.PUBLIC_TACTICAL_CONTEXT_SCHEMA,
            **{name: obj[name] for name in ('challenge', 'control_epoch', 'capability', 'semantics')},
            'view': {'width': 320, 'height': 200, 'format': 'image/png'}})
        self.assertEqual(json.loads(reply['content'][0]['text']), meta)
        self.assertEqual(decode_png(self, reply['content'][1])[1], rgb)
        self.assertNotIn('indices_hex', dump(reply).decode()); self.assertNotIn('palette_hex', dump(reply).decode())
        self.assertLess(len(dump(reply)), adapter.MAX_RESPONSE)

    def test_receipts_preserve_shared_public_schema(self):
        for value in s.STATUSES:
            client = type('Enabled', (), {'turn_enabled': True,
                'request_turn': lambda self, **args: status(value=value),
                'read_status': lambda self, **args: status(value=value)})()
            for name, args in (('request_turn', turn_arguments()), ('read_stance_status', {'command_id': COMMAND})):
                reply = call(adapter.Server(supervised_stance=client), name, args)['result']
                self.assertEqual(reply['structuredContent'], {'schema': s.PUBLIC_STATUS_SCHEMA,
                    'command_id': COMMAND, 'status': value, 'semantics': 'dispatch_status_not_success'})

    def test_invalid_arguments_never_reach_client(self):
        def fail(*args, **kwargs): self.fail('Invalid arguments reached transport')
        client = type('NeverCall', (), {'turn_enabled': True, 'request_turn': fail, 'observe_tactical': fail})()
        for args in (None, [], False, {}, arguments(), dict(turn_arguments(), approved=True),
                     turn_arguments(x=16.0), turn_arguments(y=True), turn_arguments(x=15)):
            self.assertEqual(call(adapter.Server(supervised_stance=client), 'request_turn', args)['error']['code'], -32602)
        for args in ([], False, {'x': 16}, {'require_current': True}):
            self.assertEqual(call(adapter.Server(supervised_stance=client), 'read_tactical_context', args)['error']['code'], -32602)

    def test_failure_has_only_generic_unavailable(self):
        def fail(*args, **kwargs): raise Unavailable('PRIVATE-SENTINEL-target')
        client = type('Failed', (), {'turn_enabled': True, 'request_turn': fail, 'observe_tactical': fail})()
        for name, args in (('request_turn', turn_arguments()), ('read_tactical_context', {})):
            reply = call(adapter.Server(supervised_stance=client), name, args)['result']
            self.assertEqual(reply, {'content': [{'type': 'text', 'text': '{"error":"unavailable"}'}],
                                    'structuredContent': {'error': 'unavailable'}, 'isError': True})

    def test_noise_response_fits_real_serve_bound(self):
        obj = noisy_context(); rgb = decoded_rgb(obj)
        client = type('Enabled', (), {'turn_enabled': True, 'observe_tactical': lambda self: (obj, rgb)})()
        messages = initialized_requests() + [tool_request(2, 'read_tactical_context', {})]; sink = io.BytesIO()
        adapter.serve(io.BytesIO(b''.join(dump(msg) + b'\n' for msg in messages)), sink, supervised_stance=client)
        lines = sink.getvalue().splitlines(); self.assertEqual(len(lines), 2)
        self.assertLessEqual(len(lines[1]) + 1, adapter.MAX_RESPONSE)
        self.assertEqual(decode_png(self, json.loads(lines[1])['result']['content'][1])[1], rgb)


def initialized_requests():
    return [{'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {
        'protocolVersion': adapter.PROTOCOL, 'capabilities': {}, 'clientInfo': {'name': 'synthetic', 'version': '1'}}},
        {'jsonrpc': '2.0', 'method': 'notifications/initialized'}]


def tool_request(ident, name, arguments):
    return {'jsonrpc': '2.0', 'id': ident, 'method': 'tools/call', 'params': {'name': name, 'arguments': arguments}}


class LauncherTests(unittest.TestCase):
    def test_requires_stance_and_rejects_audio_only(self):
        for flags in (['--supervised-turn'], ['--audio-only', '--supervised-turn'],
                      ['--audio-only', '--supervised-stance', '--supervised-turn']):
            with self.subTest(flags=flags), patch.object(sys, 'argv', ['launcher', *flags, '--', 'game']), \
                    patch('launch_presentation.subprocess.Popen') as spawn, patch('sys.stderr', io.StringIO()), \
                    self.assertRaises(SystemExit): launch_presentation.main()
            spawn.assert_not_called()

    def test_env_scrubbed_and_only_explicitly_enabled_on_shared_channel(self):
        cases = (([], False, False, False, 1), (['--audio-only'], False, False, False, 1),
            (['--supervised-stance'], True, False, False, 2),
            (['--supervised-stance', '--supervised-selection'], True, True, False, 2),
            (['--supervised-stance', '--supervised-turn'], True, False, True, 2),
            (['--supervised-stance', '--supervised-selection', '--supervised-turn', '--presented-hud', '--mixed-audio'],
             True, True, True, 3))
        for flags, stance, selection, turn, count in cases:
            with self.subTest(flags=flags), patch.object(sys, 'argv', ['launcher', *flags, '--', 'synthetic-game']), \
                    patch.dict(os.environ, {'OPENXCOM_STANCE_FD': '789', 'OPENXCOM_SELECTION_ENABLED': '1',
                                            'OPENXCOM_TURN_ENABLED': '1'}), \
                    patch('launch_presentation.subprocess.Popen') as spawn, patch('launch_presentation.serve') as serve:
                launch_presentation.main(); env = spawn.call_args.kwargs['env']
                self.assertEqual('OPENXCOM_STANCE_FD' in env, stance)
                self.assertEqual('OPENXCOM_SELECTION_ENABLED' in env, selection)
                self.assertEqual('OPENXCOM_TURN_ENABLED' in env, turn)
                if turn: self.assertEqual(env['OPENXCOM_TURN_ENABLED'], '1')
                self.assertEqual(len(set(spawn.call_args.kwargs['pass_fds'])), count)
                client = serve.call_args.args[6]
                if stance:
                    self.assertEqual(client.turn_enabled, turn); self.assertEqual(client.selection_enabled, selection)
                    self.assertIsNone(client.channel)
                else: self.assertIsNone(client)
                spawn.return_value.kill.assert_not_called(); spawn.return_value.terminate.assert_not_called()

    def run_launcher(self, child, messages, extra_flags=()):
        result = subprocess.run([sys.executable, str(Path(__file__).with_name('launch_presentation.py')),
            '--supervised-stance', '--supervised-turn', *extra_flags, '--', sys.executable, '-c', child],
            input=b''.join(dump(msg) + b'\n' for msg in messages), capture_output=True, timeout=8)
        self.assertEqual(result.returncode, 0); self.assertEqual(result.stderr, b'')
        lines = result.stdout.splitlines()
        self.assertTrue(all(len(line) + 1 <= adapter.MAX_RESPONSE for line in lines))
        return [json.loads(line) for line in lines]

    def test_real_launcher_noise_png_modal_duplicate_and_postdispatch_flow(self):
        child = r'''
import json, os, random, socket
assert os.environ['OPENXCOM_TURN_ENABLED'] == '1'
assert os.environ['OPENXCOM_SELECTION_ENABLED'] == '1'
channel = socket.socket(fileno=int(os.environ['OPENXCOM_STANCE_FD']))
def send(obj): channel.send(json.dumps(obj, separators=(',', ':')).encode())
epoch, command, counter = '1' * 32, '3' * 32, 1
rng = random.Random(17)
palette = b''.join(rng.randbytes(3) for _ in range(256))
indices = bytes(range(256)) + rng.randbytes(64000 - 256)
send(dict(schema='experimental-stance/1', control_epoch=epoch))
expected = iter(['V', 'R', 'S', 'V', 'S', 'R', 'O', 'N', 'S'])
while True:
    raw = channel.recv(4096)
    if not raw: break
    kind = next(expected)
    assert raw[:1] == kind.encode()
    if kind in ('V', 'O'):
        assert len(raw) == 33
        counter += 1
        obj = dict(schema='experimental-tactical-context/1' if kind == 'V' else 'experimental-stance-context/1',
            challenge=raw[1:].decode(), control_epoch=epoch, capability=f'{counter:032x}', semantics='presented_after_request')
        if kind == 'V': obj['view'] = dict(width=320, height=200, indices_hex=indices.hex(), palette_hex=palette.hex())
        else: obj['panel'] = dict(width=320, height=56, rgb_hex='0180ff' * 320 * 56)
        send(obj)
    else:
        if kind == 'R':
            cid = raw[65:97].decode()
            assert raw == ('R' + epoch + f'{counter:032x}' + cid + '016040').encode()
            value = 'pending_confirmation' if cid == command else 'unavailable'
        elif kind == 'N':
            cid = raw[65:97].decode()
            assert raw == ('N' + epoch + f'{counter:032x}' + cid + 'p0').encode()
            value = 'pending_confirmation'
        else:
            cid = raw[1:].decode(); assert len(raw) == 33; value = 'dispatched'
        send(dict(schema='experimental-stance-status/1', command_id=cid, status=value))
'''
        first = turn_arguments(capability=f'{2:032x}', x=16, y=40)
        calls = [('read_tactical_context', {}), ('request_turn', first), ('read_tactical_context', {}),
            ('request_turn', first), ('read_stance_context', {}), ('read_tactical_context', {}),
            ('request_turn', first),
            ('request_turn', turn_arguments(capability=f'{3:032x}', command_id='4' * 32, x=16, y=40)),
            ('read_stance_context', {}), ('request_turn', turn_arguments(capability=f'{4:032x}', command_id='5' * 32)),
            ('request_selection', selection_arguments(capability=f'{4:032x}', command_id='5' * 32, direction='previous')),
            ('read_stance_status', {'command_id': '5' * 32})]
        messages = initialized_requests() + [{'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'}]
        messages += [tool_request(i, name, args) for i, (name, args) in enumerate(calls, 3)]
        replies = self.run_launcher(child, messages, ('--supervised-selection',)); self.assertEqual(len(replies), 14)
        self.assertTrue({'request_turn', 'request_selection', 'read_tactical_context'} <=
                        {t['name'] for t in replies[1]['result']['tools']})
        results = [reply['result'] for reply in replies[2:]]
        self.assertEqual([i for i, result in enumerate(results) if result['isError']], [2, 4, 9])
        for index in (0, 5): self.assertEqual(decode_png(self, results[index]['content'][1])[1], decoded_rgb(noisy_context()))
        for index, expected in ((1, 'pending_confirmation'), (3, 'dispatched'), (7, 'unavailable'),
                                (10, 'pending_confirmation'), (11, 'dispatched')):
            self.assertEqual(results[index]['structuredContent']['status'], expected)
        self.assertEqual(results[8]['structuredContent']['schema'], s.PUBLIC_CONTEXT_SCHEMA)

    def test_real_launcher_rejects_alias_unused_palette_wrong_kind_and_replay(self):
        child = r'''
import json, os, socket
channel = socket.socket(fileno=int(os.environ['OPENXCOM_STANCE_FD']))
def send(obj): channel.send(json.dumps(obj, separators=(',', ':')).encode())
send(dict(schema='experimental-stance/1', control_epoch='1' * 32))
raw = channel.recv(4096)
assert raw[:1] == b'V'
obj = dict(schema='experimental-tactical-context/1', challenge=raw[1:].decode(), control_epoch='1' * 32,
    capability='2' * 32, semantics='presented_after_request',
    view=dict(width=320, height=200, indices_hex='00' * 64000, palette_hex='0180ff' + '00' * 765))
MODE = %r
if MODE == 'alias':
    obj['view']['indices_hex'] = '0001' * 32000; obj['view']['palette_hex'] = '0180ff0180ff' + '00' * 762
elif MODE == 'unused': obj['view']['palette_hex'] = '0180ff' + '00' * 764 + '01'
elif MODE == 'wrongkind':
    obj['schema'] = 'experimental-stance-context/1'
    obj['panel'] = dict(width=320, height=56, rgb_hex='0180ff' * 320 * 56); del obj['view']
elif MODE == 'replay': obj['challenge'] = '0' * 32
send(obj)
assert channel.recv(4096) == b''
'''
        for mode in ('alias', 'unused', 'wrongkind', 'replay'):
            with self.subTest(mode=mode):
                messages = initialized_requests() + [tool_request(2, 'read_tactical_context', {}),
                    tool_request(3, 'read_tactical_context', {}), tool_request(4, 'request_turn', turn_arguments())]
                replies = self.run_launcher(child % mode, messages); self.assertEqual(len(replies), 4)
                for reply in replies[1:]:
                    self.assertEqual(reply['result'], {'content': [{'type': 'text', 'text': '{"error":"unavailable"}'}],
                        'structuredContent': {'error': 'unavailable'}, 'isError': True})


if __name__ == '__main__':
    unittest.main()
