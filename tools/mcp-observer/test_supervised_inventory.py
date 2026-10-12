# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic capless inventory pixels and bounded OPEN control checks."""
import io
import itertools
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import threading
import time
import unittest
from unittest.mock import patch

import adapter
import launch_presentation
import supervised_stance as s
from snapshots import Unavailable
from test_supervised_stance import (CAPABILITY, CHALLENGE, COMMAND, EPOCH, arguments,
                                    call, dump, status)
from test_supervised_selection import selection_arguments
from test_supervised_turn import (decode_png, decoded_rgb, initialized_requests,
                                  noisy_context, tactical_context, tool_request, turn_arguments)
from test_supervised_map_left_click import LeftClickFixture


def inventory_view(challenge=CHALLENGE, source=None):
    obj = tactical_context() if source is None else source
    return dict(schema=s.INVENTORY_VIEW_SCHEMA, challenge=challenge,
                semantics='presented_after_request', view=dict(obj['view']))


def open_arguments(capability=CAPABILITY, control_epoch=EPOCH, command_id=COMMAND):
    return dict(capability=capability, control_epoch=control_epoch, command_id=command_id)


def invalid_arguments():
    invalid = [None, [], False, {}, arguments(), selection_arguments(), turn_arguments()]
    for name in open_arguments():
        obj = open_arguments(); del obj[name]; invalid.append(obj)
        for value in (None, True, False, 1, 1.0, [], {}, '', 'a' * 31, 'A' * 32,
                      'g' * 32, '0' * 33, '0' * 31 + '\n'):
            invalid.append(dict(open_arguments(), **{name: value}))
    for name in ('approved', 'human_approved', 'actor', 'actor_id', 'unit', 'unit_id',
                 'item', 'items', 'item_id', 'slot', 'action', 'close', 'desired',
                 'x', 'y', 'world_x', 'target', 'button', 'modifiers', 'force'):
        invalid.append(dict(open_arguments(), **{name: True}))
    return invalid


class InventoryFixture(LeftClickFixture):
    def __init__(self, **kwargs):
        kwargs.setdefault('inventory_enabled', True)
        super().__init__(**kwargs)

    def reply(self, raw):
        if self.response is not None: return self.response(raw, self)
        if raw[:1] == b'I': return dump(inventory_view(raw[1:].decode()))
        if raw[:1] == b'B': return dump(status(raw[65:97].decode(), self.status))
        return super().reply(raw)

    def observe_inventory(self): return self.client.observe_inventory()

    def open_args(self, **kwargs):
        return open_arguments(self.latest['capability'], self.latest['control_epoch'], **kwargs)

    def action(self, kind, command_id=COMMAND, capability=None):
        common = open_arguments(self.latest['capability'] if capability is None else capability,
                                EPOCH, command_id)
        if kind == 'B': return lambda: self.client.request_open_inventory(**common)
        if kind == 'D': return lambda: self.client.request(**common, observed='stand', desired='kneel')
        if kind == 'N': return lambda: self.client.request_selection(**common, direction='next')
        if kind == 'R': return lambda: self.client.request_turn(**common, x=16, y=40)
        if kind == 'L': return lambda: self.client.request_map_left_click(**common, x=16, y=40)
        raise AssertionError('Bad fixture action')


class ValidationTests(unittest.TestCase):
    def test_inventory_exact_capless_shape_and_noise_bound(self):
        for source in (tactical_context(), noisy_context(), tactical_context(palette=bytes(768))):
            obj = inventory_view(source=source)
            self.assertEqual(set(obj), {'schema', 'challenge', 'semantics', 'view'})
            self.assertEqual(s.validate_inventory_view(dump(obj), CHALLENGE), (obj, decoded_rgb(obj)))
            self.assertLessEqual(len(dump(obj)), s.MAX_PACKET)
            self.assertEqual(decode_png(self, s.tactical_image_content(decoded_rgb(obj)))[1], decoded_rgb(obj))

    def test_inventory_every_missing_extra_field_rejected(self):
        for inner in (False, True):
            original = inventory_view()['view'] if inner else inventory_view()
            for name in original:
                obj = inventory_view(); del (obj['view'] if inner else obj)[name]
                with self.subTest(name=name, inner=inner), self.assertRaises(Unavailable):
                    s.validate_inventory_view(dump(obj), CHALLENGE)
            for name in ('control_epoch', 'capability', 'epoch', 'items', 'item_id', 'unit_id',
                         'name', 'names', 'slot', 'weight', 'time_units', 'busy', 'reason',
                         'timestamp', 'action', 'schema_extra'):
                obj = inventory_view(); (obj['view'] if inner else obj)[name] = 'PRIVATE-SENTINEL'
                with self.subTest(name=name, inner=inner), self.assertRaises(Unavailable):
                    s.validate_inventory_view(dump(obj), CHALLENGE)

    def test_inventory_strict_types_tokens_geometry_and_semantics(self):
        for name in ('schema', 'challenge', 'semantics', 'view'):
            for value in (None, True, 1, [], {}, '', 'a' * 31, 'A' * 32, 'g' * 32):
                obj = inventory_view(); obj[name] = value
                with self.subTest(name=name, value=value), self.assertRaises(Unavailable):
                    s.validate_inventory_view(dump(obj), CHALLENGE)
        for name, size in (('width', 320), ('height', 200)):
            for value in (None, True, False, [], {}, size - 1, size + 1, float(size), str(size)):
                obj = inventory_view(); obj['view'][name] = value
                with self.subTest(name=name, value=value), self.assertRaises(Unavailable):
                    s.validate_inventory_view(dump(obj), CHALLENGE)
        with self.assertRaises(Unavailable): s.validate_inventory_view(dump(inventory_view('a' * 32)), CHALLENGE)
        with self.assertRaises(Unavailable): s.validate_inventory_view(dump(tactical_context()), CHALLENGE)
        with self.assertRaises(Unavailable): s.validate_tactical_context(dump(inventory_view()), CHALLENGE)

    def test_inventory_hex_lengths_and_case(self):
        for name, size in (('indices_hex', 128000), ('palette_hex', 1536)):
            for value in (None, False, 1, [], {}, '', '0' * (size - 1), '0' * (size + 1),
                          'fF' * (size // 2), ' ' * size, 'z' * size):
                obj = inventory_view(); obj['view'][name] = value
                with self.subTest(name=name, value=str(value)[:20]), self.assertRaises(Unavailable):
                    s.validate_inventory_view(dump(obj), CHALLENGE)

    def test_inventory_canonical_rgb_rejects_aliases_and_unused_channels(self):
        original = inventory_view(source=tactical_context(indices=b'\0\1' * 32000,
            palette=bytes.fromhex('0180ffff0001') + bytes(762)))
        swapped = inventory_view(source=tactical_context(indices=b'\1\0' * 32000,
            palette=bytes.fromhex('ff00010180ff') + bytes(762)))
        self.assertEqual(decoded_rgb(original), decoded_rgb(swapped))
        s.validate_inventory_view(dump(original), CHALLENGE)
        invalid = [swapped, inventory_view(source=tactical_context(indices=b'\0\1' * 32000,
            palette=bytes.fromhex('0180ff0180ff') + bytes(762))),
            inventory_view(source=tactical_context(indices=b'\0\2' * 32000,
            palette=bytes.fromhex('0180ff000000ff0001') + bytes(759)))]
        for position in (3, 383, 767):
            palette = bytearray(bytes.fromhex('0180ff') + bytes(765)); palette[position] = 1
            obj = inventory_view(source=tactical_context(palette=palette))
            self.assertEqual(decoded_rgb(obj), decoded_rgb(inventory_view())); invalid.append(obj)
        for obj in invalid:
            with self.assertRaises(Unavailable): s.validate_inventory_view(dump(obj), CHALLENGE)

    def test_inventory_malformed_duplicate_nonfinite_size_bounds(self):
        raw = dump(inventory_view())
        for packet in (b'', b'null', b'false', b'[]', b'42', b'\xff', b'{"x":NaN}',
                       b'[' * 2000, raw[:-1], raw + b'\0', raw[:-1] + b',"schema":"duplicate"}',
                       b'x' * (s.MAX_PACKET + 1), None, 'text', bytearray(raw)):
            with self.subTest(packet=str(packet)[:20]), self.assertRaises(Unavailable):
                s.validate_inventory_view(packet, CHALLENGE)
        padded = raw + b' ' * (s.MAX_PACKET - len(raw))
        self.assertEqual(s.validate_inventory_view(padded, CHALLENGE)[0], inventory_view())
        with self.assertRaises(Unavailable): s.validate_inventory_view(padded + b' ', CHALLENGE)

    def test_open_exact_three_tokens_no_close_payload(self):
        s.validate_open_inventory_arguments(open_arguments())
        for obj in invalid_arguments():
            with self.subTest(obj=obj), self.assertRaises(Unavailable): s.validate_open_inventory_arguments(obj)

    def test_opt_in_strict_boolean_and_turn_dependency(self):
        for value in (None, 0, 1, '1', 'true', [], {}):
            with self.subTest(value=value), self.assertRaises(Unavailable):
                s.StanceClient(None, turn_enabled=True, inventory_enabled=value)
        parent, peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        try:
            with self.assertRaises(Unavailable): s.StanceClient(parent, inventory_enabled=True)
            self.assertGreaterEqual(parent.fileno(), 0)
        finally: parent.close(); peer.close()


class ClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = InventoryFixture(**kwargs); self.addCleanup(native.close); return native

    def dispatch(self, native, command_id=COMMAND):
        native.observe_tactical(); args = native.open_args(command_id=command_id)
        native.status = 'pending_confirmation'
        self.assertEqual(native.client.request_open_inventory(**args)['status'], 'pending_confirmation')
        native.status = 'dispatched'; native.client.read_status(command_id)
        self.assertTrue(native.client.inventory_verification_required)
        return args

    def assert_closed_no_retry(self, native):
        count = len(native.requests)
        self.assertEqual(native.client.state, 'CLOSED'); self.assertIsNone(native.client.channel)
        self.assertIsNone(native.client.capability); self.assertIsNone(native.client.capability_kind)
        for operation in (native.observe, native.observe_tactical, native.observe_inventory,
                          lambda: native.client.request_open_inventory(**open_arguments()),
                          lambda: native.client.request_turn(**turn_arguments()),
                          lambda: native.client.request_map_left_click(**turn_arguments()),
                          lambda: native.client.request(**arguments()),
                          lambda: native.client.request_selection(**selection_arguments()),
                          lambda: native.client.read_status(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)

    def test_disabled_inventory_has_no_send_or_grant_effect(self):
        native = self.fixture(inventory_enabled=False); native.observe_tactical(); cap = native.client.capability
        for operation in (native.observe_inventory, lambda: native.client.request_open_inventory(**native.open_args())):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(native.client.capability, cap); native.action('R')()
        self.assertEqual([r[:1] for r in native.requests], [b'V', b'R'])

    def test_exact_b99_and_i33_wire_pending_then_capless_view(self):
        native = self.fixture(); args = self.dispatch(native)
        self.assertEqual(native.requests[1], ('B' + EPOCH + args['capability'] + COMMAND + 'o0').encode())
        self.assertEqual(len(native.requests[1]), 99)
        with patch.object(s.secrets, 'token_hex', return_value=CHALLENGE):
            obj, rgb = native.observe_inventory()
        self.assertEqual(native.requests[-1], ('I' + CHALLENGE).encode())
        self.assertEqual(obj, inventory_view()); self.assertEqual(rgb, decoded_rgb(obj))
        self.assertEqual(native.client.state, 'NEED_CONTEXT')
        self.assertIsNone(native.client.capability); self.assertIsNone(native.client.capability_kind)
        self.assertFalse(native.client.inventory_verification_required)

    def test_full_o_v_i_capability_action_matrix(self):
        for kind, action in itertools.product('OVI', 'DNRLB'):
            with self.subTest(kind=kind, action=action):
                native = self.fixture(); native.observe_tactical(); old = native.client.capability
                {'O': native.observe, 'V': native.observe_tactical, 'I': native.observe_inventory}[kind]()
                before = len(native.requests)
                operation = native.action(action, capability=old if kind == 'I' else None)
                if (kind == 'O' and action in 'DN') or (kind == 'V' and action in 'RLB'):
                    operation(); self.assertEqual(native.requests[-1][:1], action.encode())
                else:
                    with self.assertRaises(Unavailable): operation()
                    self.assertEqual(len(native.requests), before)

    def test_manual_inventory_initial_or_after_grant_retires_without_authority(self):
        for previous in (None, 'O', 'V'):
            native = self.fixture()
            if previous: {'O': native.observe, 'V': native.observe_tactical}[previous]()
            native.observe_inventory(); self.assertEqual(native.client.state, 'NEED_CONTEXT')
            self.assertIsNone(native.client.capability); self.assertIsNone(native.client.capability_kind)
            native.observe_inventory(); self.assertEqual(native.client.state, 'NEED_CONTEXT')
            native.observe(); native.action('D')()
            self.assertEqual(native.requests[-1][:1], b'D')

    def test_pending_b_blocks_all_shared_observations_and_new_actions(self):
        native = self.fixture(); native.observe_tactical(); native.action('B')(); count = len(native.requests)
        for operation in (native.observe, native.observe_tactical, native.observe_inventory,
                          *(native.action(kind, command_id='a' * 32) for kind in 'DNRLB')):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'PENDING')

    def test_dispatched_b_requires_i_and_i_never_authorizes_action(self):
        native = self.fixture(); self.dispatch(native); count = len(native.requests)
        for operation in (native.observe, native.observe_tactical,
                          *(native.action(kind, command_id='a' * 32) for kind in 'DNRLB')):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)
        native.observe_inventory(); count = len(native.requests)
        for kind in 'DNRLB':
            with self.assertRaises(Unavailable): native.action(kind, command_id='a' * 32)()
        self.assertEqual(len(native.requests), count)
        native.observe_tactical(); native.status = 'pending_confirmation'; native.action('R', 'a' * 32)()

    def test_i_cannot_satisfy_r_or_l_required_v(self):
        for kind in 'RL':
            native = self.fixture(); native.observe_tactical(); native.action(kind)()
            native.status = 'dispatched'; native.client.read_status(COMMAND); count = len(native.requests)
            with self.assertRaises(Unavailable): native.observe_inventory()
            self.assertTrue(native.client.tactical_verification_required); self.assertEqual(len(native.requests), count)
            native.observe_tactical(); native.observe_inventory()
            self.assertEqual(native.client.state, 'NEED_CONTEXT')

    def test_b_unavailable_immediate_or_cancelled_allows_either_context_or_i(self):
        for immediate, next_kind in itertools.product((False, True), 'OVI'):
            native = self.fixture(); native.observe_tactical()
            native.status = 'unavailable' if immediate else 'pending_confirmation'; native.action('B')()
            if not immediate: native.status = 'unavailable'; native.client.read_status(COMMAND)
            self.assertFalse(native.client.inventory_verification_required)
            {'O': native.observe, 'V': native.observe_tactical, 'I': native.observe_inventory}[next_kind]()

    def test_duplicate_and_status_never_clear_rearm_or_replace_i_barrier(self):
        native = self.fixture(); args = self.dispatch(native)
        native.client.request_open_inventory(**args); native.client.read_status(COMMAND)
        self.assertTrue(native.client.inventory_verification_required)
        native.observe_inventory()
        native.client.request_open_inventory(**args); native.client.read_status(COMMAND)
        self.assertFalse(native.client.inventory_verification_required)
        self.assertEqual(native.client.state, 'NEED_CONTEXT'); self.assertIsNone(native.client.capability)
        native.observe_tactical(); cap = native.client.capability
        native.client.request_open_inventory(**args); self.assertEqual(native.client.capability, cap)
        self.assertEqual(native.client.state, 'READY')
        self.assertEqual(sum(r[:1] == b'B' for r in native.requests), 1)

    def test_old_b_status_cannot_clear_new_b_or_r_verification(self):
        for kind in 'BR':
            native = self.fixture(); old = self.dispatch(native); native.observe_inventory()
            native.observe_tactical(); native.status = 'pending_confirmation'; native.action(kind, 'a' * 32)()
            native.status = 'dispatched'; native.client.read_status('a' * 32)
            native.client.request_open_inventory(**old)
            self.assertEqual(native.client.inventory_verification_required, kind == 'B')
            self.assertEqual(native.client.tactical_verification_required, kind == 'R')

    def test_conflicting_ids_rejected_across_all_five_actions(self):
        for first, second in itertools.permutations('DNRLB', 2):
            native = self.fixture(); (native.observe if first in 'DN' else native.observe_tactical)()
            native.action(first)(); count = len(native.requests)
            with self.assertRaises(Unavailable): native.action(second)()
            self.assertEqual(len(native.requests), count)
        native = self.fixture(); native.observe_tactical(); original = native.open_args()
        native.client.request_open_inventory(**original); count = len(native.requests)
        for name in ('capability', 'control_epoch'):
            with self.assertRaises(Unavailable): native.client.request_open_inventory(**dict(original, **{name:'a' * 32}))
        self.assertEqual(len(native.requests), count)

    def test_wrong_epoch_stale_grant_and_invalid_fields_do_not_send(self):
        native = self.fixture(); native.observe_tactical(); old = native.open_args(); native.observe_tactical()
        for args in (old, dict(native.open_args(), control_epoch='a' * 32)):
            with self.assertRaises(Unavailable): native.client.request_open_inventory(**args)
        count = len(native.requests)
        for args in invalid_arguments():
            self.assertEqual(call(adapter.Server(supervised_stance=native.client), 'request_open_inventory', args)['error']['code'], -32602)
        self.assertEqual(len(native.requests), count)

    def test_shared_non_evicting_64_ledger_includes_b(self):
        native = self.fixture(); native.status = 'unavailable'
        for i in range(64):
            kind = 'BDNRL'[i % 5]
            (native.observe if kind in 'DN' else native.observe_tactical)()
            native.action(kind, f'{i + 1:032x}')()
        self.assertEqual(len(native.client.commands), 64)
        native.client.read_status(f'{1:032x}')
        native.observe_tactical(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.action('B', 'f' * 32)()
        self.assertEqual(len(native.requests), count); self.assert_closed_no_retry(native)

    def test_failed_i_missing_extra_wrong_kind_replay_queue_disconnect_closes(self):
        for mode in ('malformed', 'extra', 'wrong_kind', 'replay', 'disconnect'):
            with self.subTest(mode=mode):
                native = self.fixture(); self.dispatch(native)
                def respond(raw, fixture):
                    obj = inventory_view(raw[1:].decode())
                    if mode == 'malformed': return b'{}'
                    if mode == 'extra': obj['capability'] = CAPABILITY
                    if mode == 'wrong_kind': obj = tactical_context(raw[1:].decode())
                    if mode == 'replay': obj['challenge'] = 'a' * 32
                    if mode == 'disconnect': return None
                    return dump(obj)
                native.response = respond
                with self.assertRaises(Unavailable): native.observe_inventory()
                self.assert_closed_no_retry(native)
                self.assertEqual([r[:1] for r in native.requests], [b'V', b'B', b'S', b'I'])

    def test_extra_queued_inventory_response_closes(self):
        native = self.fixture(); self.dispatch(native); queued = threading.Event()
        def respond(raw, fixture):
            obj = inventory_view(raw[1:].decode())
            fixture.peer.send(dump(obj)); fixture.peer.send(dump(obj)); queued.set()
            return None
        native.response = respond
        packet = native.client.packet
        def after_queue(deadline, maximum):
            self.assertTrue(queued.wait(.3))
            return packet(deadline, maximum)
        with patch.object(native.client, 'packet', side_effect=after_queue), self.assertRaises(Unavailable):
            native.observe_inventory()
        self.assert_closed_no_retry(native)

    def test_b_send_uncertainty_and_invalid_initial_dispatched_never_retry(self):
        for response in (None, b'{}', dump(status(value='dispatched'))):
            native = self.fixture(); native.observe_tactical(); args = native.open_args()
            native.response = lambda raw, fixture, response=response: response
            with self.assertRaises(Unavailable): native.client.request_open_inventory(**args)
            self.assert_closed_no_retry(native); self.assertEqual([r[:1] for r in native.requests], [b'V', b'B'])

    def test_b_status_terminal_regression_closes_without_resend(self):
        native = self.fixture(); self.dispatch(native); native.status = 'unavailable'
        with self.assertRaises(Unavailable): native.client.read_status(COMMAND)
        self.assert_closed_no_retry(native); self.assertEqual(sum(r[:1] == b'B' for r in native.requests), 1)

    def test_i_deadline_is_normal_two_seconds_and_never_left_click_budget(self):
        self.assertEqual(s.TIMEOUT, 2.0)
        native = self.fixture(); args = self.dispatch(native)
        with patch.object(native.client, 'exchange', wraps=native.client.exchange) as exchange:
            native.observe_inventory()
        self.assertEqual(exchange.call_args.kwargs, {'timeout': .3})
        native.client.request_open_inventory(**args)
        with patch.object(native.client, 'exchange', wraps=native.client.exchange) as exchange:
            native.observe_inventory()
        self.assertEqual(exchange.call_args.kwargs, {'timeout': .3})

    def test_i_timeout_closes_with_no_resend(self):
        native = self.fixture(); self.dispatch(native); native.client.timeout = .03
        def delayed(raw, fixture):
            time.sleep(.08); return dump(inventory_view(raw[1:].decode()))
        native.response = delayed
        with self.assertRaises(Unavailable): native.observe_inventory()
        self.assert_closed_no_retry(native); self.assertEqual([r[:1] for r in native.requests], [b'V', b'B', b'S', b'I'])

    def test_inventory_single_flight_does_not_send_or_unlock(self):
        native = self.fixture(); native.observe_tactical(); native.client.flight.acquire(); count = len(native.requests)
        try:
            for operation in (native.observe_inventory, lambda: native.client.request_open_inventory(**native.open_args())):
                with self.assertRaises(Unavailable): operation()
            self.assertTrue(native.client.flight.locked()); self.assertEqual(len(native.requests), count)
        finally: native.client.flight.release()
        native.action('B')()


class AdapterTests(unittest.TestCase):
    def test_disabled_definition_matrix_unchanged_and_dependencies_required(self):
        for flags in itertools.product((False, True), repeat=7):
            self.assertEqual(adapter.definitions(*flags), adapter.definitions(*flags, supervised_inventory=False))
        for stance, turn, inventory in itertools.product((False, True), repeat=3):
            tools = adapter.definitions(supervised_stance=stance, supervised_turn=turn, supervised_inventory=inventory)
            for name in ('request_open_inventory', 'read_inventory_view'):
                self.assertEqual(name in {t['name'] for t in tools}, stance and turn and inventory)
        for turn, inventory in ((False, True), (True, False), (True, 1), (True, '1'), (True, None)):
            client = type('Disabled', (), dict(turn_enabled=turn, inventory_enabled=inventory))()
            for name, args in (('request_open_inventory', open_arguments()), ('read_inventory_view', {})):
                self.assertIn('error', call(adapter.Server(supervised_stance=client), name, args))

    def test_exact_tool_schemas_annotations_no_close_and_no_selection_dependency(self):
        tools = adapter.definitions(supervised_stance=True, supervised_turn=True, supervised_inventory=True)
        names = [t['name'] for t in tools]
        self.assertNotIn('request_selection', names); self.assertNotIn('request_map_left_click', names)
        self.assertFalse(any('close' in name for name in names)); self.assertEqual(names.count('read_tactical_context'), 1)
        open_tool = next(t for t in tools if t['name'] == 'request_open_inventory')
        self.assertEqual(set(open_tool['inputSchema']['properties']), set(open_arguments()))
        self.assertEqual(set(open_tool['inputSchema']['required']), set(open_arguments()))
        self.assertFalse(open_tool['inputSchema']['additionalProperties'])
        self.assertEqual(open_tool['annotations'], dict(readOnlyHint=False, destructiveHint=True, idempotentHint=True, openWorldHint=False))
        view_tool = next(t for t in tools if t['name'] == 'read_inventory_view')
        self.assertEqual(view_tool['inputSchema'], dict(type='object', properties={}, additionalProperties=False))
        self.assertTrue(view_tool['annotations']['readOnlyHint']); self.assertFalse(view_tool['annotations']['idempotentHint'])

    def test_invalid_arguments_never_reach_client(self):
        def fail(*args, **kwargs): self.fail('Invalid arguments reached transport')
        client = type('NeverCall', (), dict(turn_enabled=True, inventory_enabled=True,
            request_open_inventory=fail, observe_inventory=fail))()
        for args in invalid_arguments():
            self.assertEqual(call(adapter.Server(supervised_stance=client), 'request_open_inventory', args)['error']['code'], -32602)
        for args in (None, False, [], {'capability': CAPABILITY}, {'close': True}, {'items': []}):
            # None here means omitted arguments in the helper, which is valid for read.
            if args is None: continue
            self.assertEqual(call(adapter.Server(supervised_stance=client), 'read_inventory_view', args)['error']['code'], -32602)

    def test_public_inventory_envelope_exact_pixels_and_no_native_metadata(self):
        obj = inventory_view(source=noisy_context()); rgb = decoded_rgb(obj)
        client = type('Enabled', (), dict(turn_enabled=True, inventory_enabled=True,
            observe_inventory=lambda self: (obj, rgb)))()
        reply = call(adapter.Server(supervised_stance=client), 'read_inventory_view', {})['result']
        expected = dict(schema=s.PUBLIC_INVENTORY_VIEW_SCHEMA, challenge=CHALLENGE,
            semantics='presented_after_request', view=dict(width=320, height=200, format='image/png'))
        self.assertEqual(reply['structuredContent'], expected)
        self.assertEqual(json.loads(reply['content'][0]['text']), expected)
        self.assertEqual(len(reply['content']), 2); self.assertFalse(reply['isError'])
        self.assertEqual(decode_png(self, reply['content'][1])[1], rgb)
        self.assertLess(len(dump(reply)), adapter.MAX_RESPONSE)
        source = io.BytesIO(b''.join(dump(msg) + b'\n' for msg in initialized_requests() +
            [tool_request(2, 'read_inventory_view', {})])); output = io.BytesIO()
        adapter.serve(source, output, supervised_stance=client)
        self.assertTrue(all(len(line) + 1 <= adapter.MAX_RESPONSE for line in output.getvalue().splitlines()))

    def test_shared_status_schema_unchanged_and_failure_generic(self):
        for value in s.STATUSES:
            client = type('Enabled', (), dict(turn_enabled=True, inventory_enabled=True,
                request_open_inventory=lambda self, **args: status(value=value)))()
            reply = call(adapter.Server(supervised_stance=client), 'request_open_inventory', open_arguments())['result']
            self.assertEqual(reply['structuredContent'], dict(schema=s.PUBLIC_STATUS_SCHEMA, command_id=COMMAND,
                status=value, semantics='dispatch_status_not_success'))
        def fail(*args, **kwargs): raise Unavailable('PRIVATE-SENTINEL-item')
        client = type('Failed', (), dict(turn_enabled=True, inventory_enabled=True,
            request_open_inventory=fail, observe_inventory=fail))()
        for name, args in (('request_open_inventory', open_arguments()), ('read_inventory_view', {})):
            self.assertEqual(call(adapter.Server(supervised_stance=client), name, args)['result'],
                dict(content=[dict(type='text', text='{"error":"unavailable"}')], structuredContent={'error':'unavailable'}, isError=True))


class LauncherTests(unittest.TestCase):
    def test_inventory_requires_stance_turn_and_rejects_audio_only(self):
        for flags in (['--supervised-inventory'], ['--supervised-stance', '--supervised-inventory'],
                      ['--supervised-turn', '--supervised-inventory'],
                      ['--audio-only', '--supervised-stance', '--supervised-turn', '--supervised-inventory']):
            with self.subTest(flags=flags), patch.object(sys, 'argv', ['launcher', *flags, '--', 'game']), \
                    patch('launch_presentation.subprocess.Popen') as spawn, patch('sys.stderr', io.StringIO()), \
                    self.assertRaises(SystemExit): launch_presentation.main()
            spawn.assert_not_called()

    def test_ambient_scrubbed_literal_one_no_extra_channel_or_kill(self):
        for flags, enabled, count in (([], False, 1), (['--audio-only'], False, 1),
                (['--supervised-stance'], False, 2), (['--supervised-stance', '--supervised-turn'], False, 2),
                (['--supervised-stance', '--supervised-turn', '--supervised-inventory'], True, 2)):
            for ambient in ('1', 'true', '99'):
                with self.subTest(flags=flags, ambient=ambient), \
                        patch.object(sys, 'argv', ['launcher', *flags, '--', 'synthetic-game']), \
                        patch.dict(os.environ, {'OPENXCOM_INVENTORY_ENABLED': ambient}), \
                        patch('launch_presentation.subprocess.Popen') as spawn, patch('launch_presentation.serve') as serve:
                    launch_presentation.main(); env = spawn.call_args.kwargs['env']
                    self.assertEqual(env.get('OPENXCOM_INVENTORY_ENABLED'), '1' if enabled else None)
                    self.assertEqual(len(set(spawn.call_args.kwargs['pass_fds'])), count)
                    client = serve.call_args.args[6]
                    if client is not None:
                        self.assertEqual(client.inventory_enabled, enabled); self.assertIsNone(client.channel)
                    spawn.return_value.kill.assert_not_called(); spawn.return_value.terminate.assert_not_called()

    def run_launcher(self, calls, mode='normal', enabled=True):
        flags = ['--supervised-stance', '--supervised-turn']
        if enabled: flags.append('--supervised-inventory')
        messages = initialized_requests() + [{'jsonrpc':'2.0', 'id':2, 'method':'tools/list'}]
        messages += [tool_request(i, name, args) for i, (name, args) in enumerate(calls, 3)]
        result = subprocess.run([sys.executable, str(Path(__file__).with_name('launch_presentation.py')),
            *flags, '--', sys.executable, str(Path(__file__).with_name('sdk_supervised_inventory_smoke.py')),
            '--synthetic-producer', mode], input=b''.join(dump(msg) + b'\n' for msg in messages),
            capture_output=True, timeout=15, env=dict(os.environ, OPENXCOM_INVENTORY_ENABLED='1'))
        self.assertEqual(result.returncode, 0); self.assertEqual(result.stderr, b'')
        lines = result.stdout.splitlines(); self.assertTrue(all(len(line) + 1 <= adapter.MAX_RESPONSE for line in lines))
        replies = [json.loads(line) for line in lines]; self.assertEqual(len(replies), len(calls) + 2)
        names = {t['name'] for t in replies[1]['result']['tools']}
        self.assertEqual('request_open_inventory' in names, enabled); self.assertEqual('read_inventory_view' in names, enabled)
        return replies[2:]

    def test_real_launcher_open_i_barrier_capless_and_fresh_v(self):
        args = open_arguments(capability=f'{2:032x}')
        calls = [('read_tactical_context', {}), ('request_open_inventory', dict(args, action='close')),
                 ('request_open_inventory', args), ('read_inventory_view', {}), ('request_open_inventory', args),
                 ('read_stance_context', {}), ('read_tactical_context', {}), ('read_inventory_view', {}),
                 ('request_open_inventory', args), ('request_open_inventory', dict(args, command_id='a' * 32)),
                 ('read_tactical_context', {}), ('request_turn', turn_arguments(capability=f'{3:032x}', command_id='a' * 32, x=16, y=40)),
                 ('read_stance_status', {'command_id':'a' * 32})]
        replies = self.run_launcher(calls)
        self.assertEqual([i for i, r in enumerate(replies) if 'error' in r], [1])
        self.assertEqual([i for i, r in enumerate(replies) if r.get('result', {}).get('isError')], [3, 5, 6, 9])
        self.assertEqual(replies[7]['result']['structuredContent']['schema'], s.PUBLIC_INVENTORY_VIEW_SCHEMA)
        self.assertEqual(replies[12]['result']['structuredContent']['status'], 'unavailable')

    def test_real_launcher_manual_i_and_ambient_disabled(self):
        reply = self.run_launcher([('read_inventory_view', {})])[0]['result']
        self.assertFalse(reply['isError']); self.assertNotIn('capability', reply['structuredContent'])
        replies = self.run_launcher([('read_inventory_view', {}), ('request_open_inventory', open_arguments())], enabled=False)
        self.assertTrue(all(r['error']['code'] == -32602 for r in replies))

    def test_real_launcher_failed_i_modes_close_without_resend(self):
        args = open_arguments(capability=f'{2:032x}')
        for mode in ('malformed', 'extra', 'wrong_kind', 'replay', 'alias', 'unused_palette', 'disconnect', 'delay'):
            with self.subTest(mode=mode):
                replies = self.run_launcher([('read_tactical_context', {}), ('request_open_inventory', args),
                    ('read_stance_status', {'command_id':COMMAND}), ('read_inventory_view', {}),
                    ('request_open_inventory', args), ('read_tactical_context', {}), ('read_inventory_view', {})], mode)
                self.assertEqual(replies[2]['result']['structuredContent']['status'], 'dispatched')
                self.assertTrue(all(r['result']['isError'] for r in replies[3:]))


if __name__ == '__main__': unittest.main()
