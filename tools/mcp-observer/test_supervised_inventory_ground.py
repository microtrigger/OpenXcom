# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic ordinary inventory Ground authority, transport and launcher checks."""
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
from test_supervised_stance import CAPABILITY, CHALLENGE, COMMAND, EPOCH, call, dump, status
from test_supervised_turn import (decode_png, decoded_rgb, initialized_requests, noisy_context,
                                  tactical_context, tool_request)
from test_supervised_inventory import invalid_arguments, inventory_view, open_arguments
from test_supervised_inventory_close import CloseFixture, close_context


ground_arguments = open_arguments


def ground_context(challenge=CHALLENGE, capability=CAPABILITY, epoch=EPOCH, source=None):
    obj = tactical_context(challenge, capability, epoch) if source is None else dict(source)
    obj['schema'] = s.INVENTORY_GROUND_CONTEXT_SCHEMA
    return obj


def invalid_ground_arguments():
    return invalid_arguments() + [dict(ground_arguments(), **{name: True}) for name in
        ('ground', 'page', 'page_index', 'page_count', 'next_page', 'scroll', 'preflight', 'can_ground')]


class GroundFixture(CloseFixture):
    def __init__(self, **kwargs):
        kwargs.setdefault('inventory_ground_enabled', True)
        super().__init__(**kwargs)

    def reply(self, raw):
        if self.response is not None: return self.response(raw, self)
        if raw[:1] == b'K':
            self.counter += 1
            self.latest = ground_context(raw[1:].decode(), f'{self.counter:032x}')
            return dump(self.latest)
        if raw[:1] == b'G': return dump(status(raw[65:97].decode(), self.status))
        return super().reply(raw)

    def observe_ground(self): return self.client.observe_inventory_ground()

    def action(self, kind, command_id=COMMAND, capability=None):
        if kind == 'G':
            common = ground_arguments(self.latest['capability'] if capability is None else capability,
                                      EPOCH, command_id)
            return lambda: self.client.request_inventory_ground(**common)
        return super().action(kind, command_id, capability)

    def observation(self, kind):
        if kind == 'K': return self.observe_ground()
        return super().observation(kind)


class ValidationTests(unittest.TestCase):
    def test_exact_context_roundtrip_noise_bound_and_indexed_png(self):
        for obj in (ground_context(), ground_context(source=noisy_context()),
                    ground_context(source=tactical_context(palette=bytes(768)))):
            self.assertEqual(set(obj), {'schema', 'challenge', 'control_epoch', 'capability', 'semantics', 'view'})
            self.assertEqual(s.validate_inventory_ground_context(dump(obj), CHALLENGE), (obj, decoded_rgb(obj)))
            self.assertLessEqual(len(dump(obj)), s.MAX_PACKET)
            self.assertEqual(decode_png(self, s.tactical_image_content(decoded_rgb(obj)))[1], decoded_rgb(obj))

    def test_missing_extra_fields_and_hidden_metadata_rejected(self):
        for inner in (False, True):
            original = ground_context()['view'] if inner else ground_context()
            for name in original:
                obj = ground_context(); del (obj['view'] if inner else obj)[name]
                with self.subTest(name=name, inner=inner), self.assertRaises(Unavailable):
                    s.validate_inventory_ground_context(dump(obj), CHALLENGE)
            for name in ('items', 'item_id', 'unit_id', 'name', 'slot', 'weight', 'held', 'time_units',
                         'busy', 'reason', 'timestamp', 'action', 'parent', 'generation', 'can_ground',
                         'page', 'page_count', 'page_index', 'next_page', 'preflight'):
                obj = ground_context(); (obj['view'] if inner else obj)[name] = 'PRIVATE-SENTINEL'
                with self.subTest(name=name, inner=inner), self.assertRaises(Unavailable):
                    s.validate_inventory_ground_context(dump(obj), CHALLENGE)

    def test_types_tokens_semantics_geometry_and_hex(self):
        for name in ('schema', 'challenge', 'control_epoch', 'capability', 'semantics', 'view'):
            for value in (None, True, 1, [], {}, '', 'a' * 31, 'A' * 32, 'g' * 32):
                obj = ground_context(); obj[name] = value
                with self.subTest(name=name, value=value), self.assertRaises(Unavailable):
                    s.validate_inventory_ground_context(dump(obj), CHALLENGE)
        for name, size in (('width', 320), ('height', 200)):
            for value in (None, True, False, [], {}, size - 1, size + 1, float(size), str(size)):
                obj = ground_context(); obj['view'][name] = value
                with self.assertRaises(Unavailable): s.validate_inventory_ground_context(dump(obj), CHALLENGE)
        for name, size in (('indices_hex', 128000), ('palette_hex', 1536)):
            for value in (None, False, 1, [], {}, '', '0' * (size - 1), '0' * (size + 1),
                          'fF' * (size // 2), ' ' * size, 'z' * size):
                obj = ground_context(); obj['view'][name] = value
                with self.assertRaises(Unavailable): s.validate_inventory_ground_context(dump(obj), CHALLENGE)

    def test_cross_schema_challenge_alias_unused_palette_and_duplicate_rejection(self):
        for obj in (tactical_context(), inventory_view(), close_context(), ground_context(challenge='a' * 32),
                    ground_context(source=tactical_context(indices=b'\1\0' * 32000,
                        palette=bytes.fromhex('ff00010180ff') + bytes(762))),
                    ground_context(source=tactical_context(indices=b'\0\1' * 32000,
                        palette=bytes.fromhex('0180ff0180ff') + bytes(762))),
                    ground_context(source=tactical_context(palette=bytes.fromhex('0180ffff0001') + bytes(762)))):
            with self.assertRaises(Unavailable): s.validate_inventory_ground_context(dump(obj), CHALLENGE)
        for validator in (s.validate_tactical_context, s.validate_inventory_view, s.validate_context,
                          s.validate_inventory_close_context):
            with self.assertRaises(Unavailable): validator(dump(ground_context()), CHALLENGE)
        raw = dump(ground_context())
        for packet in (b'', b'null', b'false', b'[]', b'42', b'\xff', b'{"x":NaN}', b'[' * 2000,
                       raw[:-1], raw + b'\0', raw[:-1] + b',"schema":"duplicate"}',
                       b'x' * (s.MAX_PACKET + 1), None, 'text', bytearray(raw)):
            with self.assertRaises(Unavailable): s.validate_inventory_ground_context(packet, CHALLENGE)
        padded = raw + b' ' * (s.MAX_PACKET - len(raw))
        self.assertEqual(s.validate_inventory_ground_context(padded, CHALLENGE)[0], ground_context())
        with self.assertRaises(Unavailable): s.validate_inventory_ground_context(padded + b' ', CHALLENGE)

    def test_three_tokens_only_no_action_page_preflight_or_model_approval(self):
        s.validate_inventory_ground_arguments(ground_arguments())
        for obj in invalid_ground_arguments():
            with self.subTest(obj=obj), self.assertRaises(Unavailable): s.validate_inventory_ground_arguments(obj)

    def test_strict_opt_in_dependency_without_socket_mutation(self):
        parent, peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        try:
            for value in (None, 0, 1, '1', 'true', [], {}):
                with self.assertRaises(Unavailable):
                    s.StanceClient(parent, turn_enabled=True, inventory_enabled=True,
                                   inventory_close_enabled=True, inventory_ground_enabled=value)
                self.assertTrue(parent.getblocking())
            for turn, inventory, close in itertools.product((False, True), repeat=3):
                if turn and inventory and close: continue
                with self.assertRaises(Unavailable):
                    s.StanceClient(parent, turn_enabled=turn, inventory_enabled=inventory,
                        inventory_close_enabled=close, inventory_ground_enabled=True)
                self.assertGreaterEqual(parent.fileno(), 0)
        finally: parent.close(); peer.close()


class ClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = GroundFixture(**kwargs); self.addCleanup(native.close); return native

    def dispatch(self, native, command_id=COMMAND):
        native.observe_ground(); args = native.open_args(command_id=command_id)
        native.status = 'pending_confirmation'; native.client.request_inventory_ground(**args)
        native.status = 'dispatched'; native.client.read_status(command_id)
        self.assertTrue(native.client.inventory_verification_required)
        self.assertFalse(native.client.tactical_verification_required)
        return args

    def assert_closed_no_retry(self, native):
        count = len(native.requests)
        self.assertEqual(native.client.state, 'CLOSED'); self.assertIsNone(native.client.channel)
        self.assertIsNone(native.client.capability); self.assertIsNone(native.client.capability_kind)
        for operation in (*(lambda k=k: native.observation(k) for k in 'OVIJK'),
                          *(native.action(kind) for kind in 'DNRLBCG'),
                          lambda: native.client.read_status(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)

    def test_default_disabled_no_send_or_grant_change(self):
        native = self.fixture(inventory_ground_enabled=False); native.observe_close(); cap = native.client.capability
        for operation in (native.observe_ground, lambda: native.client.request_inventory_ground(**native.open_args())):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(native.client.capability, cap); native.action('C')()
        self.assertEqual([r[:1] for r in native.requests], [b'J', b'C'])

    def test_exact_k33_g97_wire_and_fresh_capless_i_required(self):
        native = self.fixture()
        with patch.object(s.secrets, 'token_hex', return_value=CHALLENGE):
            obj, rgb = native.observe_ground()
        self.assertEqual(native.requests, [('K' + CHALLENGE).encode()]); self.assertEqual(rgb, decoded_rgb(obj))
        args = native.open_args(); native.client.request_inventory_ground(**args)
        self.assertEqual(native.requests[-1], ('G' + EPOCH + args['capability'] + COMMAND).encode())
        self.assertEqual(len(native.requests[-1]), 97)
        native.status = 'dispatched'; native.client.read_status(COMMAND); count = len(native.requests)
        for operation in (*(lambda k=k: native.observation(k) for k in 'OVJK'),
                          *(native.action(kind, 'a' * 32) for kind in 'DNRLBCG')):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)
        obj, _ = native.observe_inventory(); self.assertFalse(native.client.inventory_verification_required)
        self.assertEqual(set(obj), {'schema', 'challenge', 'semantics', 'view'})
        self.assertIsNone(native.client.capability); self.assertIsNone(native.client.capability_kind)
        self.assertEqual(native.client.state, 'NEED_CONTEXT')
        with self.assertRaises(Unavailable): native.action('G', 'a' * 32)()
        native.observe_ground(); native.status = 'pending_confirmation'; native.action('G', 'a' * 32)()

    def test_full_o_v_i_j_k_seven_action_matrix(self):
        for kind, action in itertools.product('OVIJK', 'DNRLBCG'):
            with self.subTest(kind=kind, action=action):
                native = self.fixture(); native.observe_ground(); old = native.client.capability
                native.observation(kind); before = len(native.requests)
                operation = native.action(action, capability=old if kind == 'I' else None)
                allowed = {'O':'DN', 'V':'RLB', 'I':'', 'J':'C', 'K':'G'}
                if action in allowed[kind]:
                    operation(); self.assertEqual(native.requests[-1][:1], action.encode())
                else:
                    with self.assertRaises(Unavailable): operation()
                    self.assertEqual(len(native.requests), before)

    def test_k_replaces_prior_grants_i_retires_k_and_j_k_are_not_interchangeable(self):
        for kind in 'OVJK':
            native = self.fixture(); native.observation(kind); old = native.open_args(); native.observe_ground()
            with self.assertRaises(Unavailable): native.client.request_inventory_ground(**old)
            native.observe_inventory(); self.assertIsNone(native.client.capability)
            with self.assertRaises(Unavailable): native.action('G')()
            self.assertEqual(native.client.state, 'NEED_CONTEXT')
            native.observe_ground(); native.action('G')()
        native = self.fixture(); native.observe_ground(); old = native.open_args(); native.observe_close()
        with self.assertRaises(Unavailable): native.client.request_inventory_ground(**old)
        with self.assertRaises(Unavailable): native.action('G')()
        native.action('C')()

    def test_pending_ground_blocks_shared_observations_and_new_commands(self):
        native = self.fixture(); native.observe_ground(); native.action('G')(); before = len(native.requests)
        for operation in (*(lambda k=k: native.observation(k) for k in 'OVIJK'),
                          *(native.action(kind, 'a' * 32) for kind in 'DNRLBCG')):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), before); self.assertEqual(native.client.state, 'PENDING')

    def test_k_never_satisfies_post_b_i_or_post_r_l_c_v(self):
        for kind in 'BRLC':
            native = self.fixture(); native.observation('J' if kind == 'C' else 'V'); native.action(kind)()
            native.status = 'dispatched'; native.client.read_status(COMMAND); before = len(native.requests)
            with self.assertRaises(Unavailable): native.observe_ground()
            self.assertEqual(native.client.inventory_verification_required, kind == 'B')
            self.assertEqual(native.client.tactical_verification_required, kind in 'RLC')
            self.assertEqual(len(native.requests), before)
            native.observation('I' if kind == 'B' else 'V'); native.observe_ground()

    def test_cancel_or_immediate_rejection_needs_fresh_context_all_kinds_allowed(self):
        for immediate, kind in itertools.product((False, True), 'OVIJK'):
            native = self.fixture(); native.observe_ground()
            native.status = 'unavailable' if immediate else 'pending_confirmation'; native.action('G')()
            if not immediate: native.status = 'unavailable'; native.client.read_status(COMMAND)
            self.assertFalse(native.client.inventory_verification_required)
            self.assertFalse(native.client.tactical_verification_required)
            self.assertEqual(native.client.state, 'NEED_CONTEXT'); self.assertIsNone(native.client.capability)
            with self.assertRaises(Unavailable): native.action('G', 'a' * 32)()
            native.observation(kind)

    def test_duplicate_never_dispatches_rearms_or_discharges_i_barrier(self):
        native = self.fixture(); args = self.dispatch(native)
        native.client.request_inventory_ground(**args); native.client.read_status(COMMAND)
        self.assertTrue(native.client.inventory_verification_required)
        native.observe_inventory(); native.client.request_inventory_ground(**args)
        self.assertFalse(native.client.inventory_verification_required)
        self.assertEqual(native.client.state, 'NEED_CONTEXT'); self.assertIsNone(native.client.capability)
        native.observe_ground(); cap = native.client.capability
        native.client.request_inventory_ground(**args); native.client.read_status(COMMAND)
        self.assertFalse(native.client.inventory_verification_required)
        self.assertEqual(native.client.capability, cap); self.assertEqual(native.client.state, 'READY')
        self.assertEqual(sum(r[:1] == b'G' for r in native.requests), 1)

    def test_old_g_status_preserves_new_b_r_l_c_g_barriers(self):
        for kind in 'BRLCG':
            native = self.fixture(); old = self.dispatch(native); native.observe_inventory()
            native.observation('K' if kind == 'G' else 'J' if kind == 'C' else 'V')
            native.status = 'pending_confirmation'; native.action(kind, 'a' * 32)()
            native.status = 'dispatched'; native.client.read_status('a' * 32)
            native.client.request_inventory_ground(**old)
            self.assertEqual(native.client.inventory_verification_required, kind in 'BG')
            self.assertEqual(native.client.tactical_verification_required, kind in 'RLC')

    def test_old_c_status_preserves_new_ground_barrier(self):
        native = self.fixture(); native.observe_close(); old = native.open_args(); native.action('C')()
        native.status = 'dispatched'; native.client.read_status(COMMAND); native.observe_tactical()
        self.dispatch(native, 'a' * 32); native.client.request_close_inventory(**old)
        self.assertTrue(native.client.inventory_verification_required)
        self.assertFalse(native.client.tactical_verification_required)

    def test_conflicting_ids_across_seven_actions_and_changed_tokens(self):
        for first, second in itertools.permutations('DNRLBCG', 2):
            native = self.fixture(); native.observation('K' if first == 'G' else 'J' if first == 'C' else 'O' if first in 'DN' else 'V')
            native.action(first)(); before = len(native.requests)
            with self.assertRaises(Unavailable): native.action(second)()
            self.assertEqual(len(native.requests), before)
        native = self.fixture(); native.observe_ground(); args = native.open_args(); native.action('G')()
        for name in ('control_epoch', 'capability'):
            with self.assertRaises(Unavailable): native.client.request_inventory_ground(**dict(args, **{name:'a' * 32}))

    def test_shared_non_evicting_64_ledger_never_sends_65th_command(self):
        native = self.fixture(); native.status = 'unavailable'
        for index in range(s.MAX_COMMANDS):
            kind = 'DNRLBCG'[index % 7]
            native.observation('K' if kind == 'G' else 'J' if kind == 'C' else 'O' if kind in 'DN' else 'V')
            native.action(kind, f'{index:032x}')()
        self.assertEqual(len(native.client.commands), 64)
        native.observe_ground(); before = len(native.requests)
        with self.assertRaises(Unavailable): native.action('G', 'f' * 32)()
        self.assertEqual(len(native.requests), before); self.assert_closed_no_retry(native)

    def test_wrong_epoch_stale_capability_and_local_invalid_arguments_do_not_send(self):
        native = self.fixture(); native.observe_ground(); old = native.open_args(); native.observe_ground()
        for args in (old, dict(native.open_args(), control_epoch='a' * 32)):
            with self.assertRaises(Unavailable): native.client.request_inventory_ground(**args)
        self.assertEqual([r[:1] for r in native.requests], [b'K', b'K'])

    def test_k_failure_is_terminal_without_replay(self):
        for mode in ('malformed', 'extra', 'wrong_kind', 'close_kind', 'capless', 'replay', 'disconnect'):
            native = self.fixture(); native.observe_ground()
            def respond(raw, fixture, mode=mode):
                obj = ground_context(raw[1:].decode(), f'{3:032x}')
                if mode == 'malformed': return b'{}'
                if mode == 'extra': obj['items'] = []
                if mode == 'wrong_kind': obj['schema'] = s.TACTICAL_CONTEXT_SCHEMA
                if mode == 'close_kind': obj['schema'] = s.INVENTORY_CLOSE_CONTEXT_SCHEMA
                if mode == 'capless': obj = inventory_view(raw[1:].decode())
                if mode == 'replay': obj['challenge'] = 'a' * 32
                if mode == 'disconnect': return None
                return dump(obj)
            native.response = respond
            with self.assertRaises(Unavailable): native.observe_ground()
            self.assert_closed_no_retry(native); self.assertEqual([r[:1] for r in native.requests], [b'K', b'K'])

    def test_g_uncertain_or_initial_dispatched_response_is_terminal(self):
        for response in (None, b'{}', dump(status(value='dispatched'))):
            native = self.fixture(); native.observe_ground()
            native.response = lambda raw, fixture, response=response: response
            with self.assertRaises(Unavailable): native.action('G')()
            self.assert_closed_no_retry(native); self.assertEqual([r[:1] for r in native.requests], [b'K', b'G'])

    def test_g_terminal_status_regression_closes(self):
        native = self.fixture(); self.dispatch(native); native.status = 'unavailable'
        with self.assertRaises(Unavailable): native.client.read_status(COMMAND)
        self.assert_closed_no_retry(native)

    def test_failed_post_g_i_preserves_barrier_without_replay(self):
        native = self.fixture(); self.dispatch(native)
        native.response = lambda raw, fixture: dump(ground_context(raw[1:].decode()))
        with self.assertRaises(Unavailable): native.observe_inventory()
        self.assertTrue(native.client.inventory_verification_required); self.assert_closed_no_retry(native)
        self.assertEqual([r[:1] for r in native.requests], [b'K', b'G', b'S', b'I'])

    def test_k_g_and_post_g_i_use_normal_deadlines_duplicate_does_not_extend(self):
        self.assertEqual(s.TIMEOUT, 2.0); native = self.fixture()
        with patch.object(native.client, 'exchange', wraps=native.client.exchange) as exchange:
            native.observe_ground()
        self.assertEqual(exchange.call_args.kwargs, {'timeout': .3})
        args = self.dispatch(native)
        with patch.object(native.client, 'exchange', wraps=native.client.exchange) as exchange:
            native.observe_inventory()
        self.assertEqual(exchange.call_args.kwargs, {'timeout': .3})
        native.client.request_inventory_ground(**args)
        with patch.object(native.client, 'exchange', wraps=native.client.exchange) as exchange:
            native.observe_ground()
        self.assertEqual(exchange.call_args.kwargs, {'timeout': .3})

    def test_k_g_and_post_g_i_timeout_are_terminal(self):
        for kind in 'KGI':
            native = self.fixture(); self.dispatch(native) if kind == 'I' else native.observe_ground()
            native.client.timeout = .03
            def delayed(raw, fixture):
                time.sleep(.08)
                if raw[:1] == b'G': return dump(status())
                return dump(inventory_view(raw[1:].decode()) if kind == 'I' else ground_context(raw[1:].decode(), f'{9:032x}'))
            native.response = delayed
            with self.assertRaises(Unavailable): (native.action('G') if kind == 'G' else lambda: native.observation(kind))()
            self.assert_closed_no_retry(native)

    def test_extra_queued_k_response_and_single_flight_fail_closed(self):
        native = self.fixture(); native.observe_ground(); native.client.flight.acquire(); before = len(native.requests)
        try:
            for operation in (native.observe_ground, native.action('G')):
                with self.assertRaises(Unavailable): operation()
            self.assertTrue(native.client.flight.locked()); self.assertEqual(len(native.requests), before)
        finally: native.client.flight.release()
        queued = threading.Event()
        def respond(raw, fixture):
            reply = dump(ground_context(raw[1:].decode(), f'{3:032x}'))
            fixture.peer.send(reply); fixture.peer.send(reply); queued.set(); return None
        native.response = respond; packet = native.client.packet
        def after_queue(deadline, maximum):
            self.assertTrue(queued.wait(.3)); return packet(deadline, maximum)
        with patch.object(native.client, 'packet', side_effect=after_queue), self.assertRaises(Unavailable):
            native.observe_ground()
        self.assert_closed_no_retry(native)


class AdapterTests(unittest.TestCase):
    def test_default_definition_matrix_unchanged_and_ground_dependencies(self):
        for flags in itertools.product((False, True), repeat=9):
            self.assertEqual(adapter.definitions(*flags), adapter.definitions(*flags, supervised_inventory_ground=False))
        for stance, turn, inventory, close, ground in itertools.product((False, True), repeat=5):
            tools = adapter.definitions(supervised_stance=stance, supervised_turn=turn,
                supervised_inventory=inventory, supervised_inventory_close=close, supervised_inventory_ground=ground)
            for name in ('request_inventory_ground', 'read_inventory_ground_context'):
                self.assertEqual(name in {t['name'] for t in tools}, stance and turn and inventory and close and ground)
        for value in (None, 0, 1, '1', 'true', [], {}):
            tools = adapter.definitions(supervised_stance=True, supervised_turn=True,
                supervised_inventory=True, supervised_inventory_close=True, supervised_inventory_ground=value)
            self.assertNotIn('request_inventory_ground', {tool['name'] for tool in tools})
        for turn, inventory, close, ground in ((False, True, True, True), (True, False, True, True),
                (True, True, False, True), (True, True, True, False), (True, True, True, 1), (True, True, True, '1')):
            client = type('Disabled', (), dict(turn_enabled=turn, inventory_enabled=inventory,
                inventory_close_enabled=close, inventory_ground_enabled=ground))()
            for name, args in (('request_inventory_ground', ground_arguments()), ('read_inventory_ground_context', {})):
                self.assertIn('error', call(adapter.Server(supervised_stance=client), name, args))

    def client(self, **attrs):
        return type('Client', (), dict(turn_enabled=True, inventory_enabled=True,
            inventory_close_enabled=True, inventory_ground_enabled=True, **attrs))()

    def test_exact_tool_schemas_annotations_and_ordinary_ground_wording(self):
        tools = adapter.definitions(supervised_stance=True, supervised_turn=True,
            supervised_inventory=True, supervised_inventory_close=True, supervised_inventory_ground=True)
        names = [t['name'] for t in tools]
        self.assertNotIn('request_selection', names); self.assertNotIn('request_map_left_click', names)
        tool = next(t for t in tools if t['name'] == 'request_inventory_ground')
        self.assertEqual(set(tool['inputSchema']['properties']), set(ground_arguments()))
        self.assertEqual(set(tool['inputSchema']['required']), set(ground_arguments()))
        self.assertFalse(tool['inputSchema']['additionalProperties'])
        self.assertEqual(tool['annotations'], dict(readOnlyHint=False, destructiveHint=True, idempotentHint=True, openWorldHint=False))
        self.assertIn('rearranges ground slots/stacks and cycles the page', tool['description'])
        self.assertNotIn('read-only', tool['description']); self.assertNotIn('scroll', tool['description'])
        view = next(t for t in tools if t['name'] == 'read_inventory_ground_context')
        self.assertEqual(view['inputSchema'], dict(type='object', properties={}, additionalProperties=False))
        self.assertTrue(view['annotations']['readOnlyHint']); self.assertFalse(view['annotations']['idempotentHint'])
        for name in ('read_tactical_context', 'read_inventory_view', 'read_stance_status', 'read_inventory_close_context'):
            self.assertEqual(names.count(name), 1)

    def test_invalid_inputs_never_reach_client(self):
        def fail(*args, **kwargs): self.fail('Invalid arguments reached transport')
        client = self.client(request_inventory_ground=fail, observe_inventory_ground=fail)
        for args in invalid_ground_arguments():
            self.assertEqual(call(adapter.Server(supervised_stance=client), 'request_inventory_ground', args)['error']['code'], -32602)
        for args in (False, [], {'capability':CAPABILITY}, {'ground':True}, {'items':[]}, {'page':1}):
            self.assertEqual(call(adapter.Server(supervised_stance=client), 'read_inventory_ground_context', args)['error']['code'], -32602)

    def test_public_ground_envelope_exact_indexed_pixels_and_bound(self):
        obj = ground_context(source=noisy_context()); rgb = decoded_rgb(obj)
        client = self.client(observe_inventory_ground=lambda self:(obj, rgb))
        reply = call(adapter.Server(supervised_stance=client), 'read_inventory_ground_context', {})['result']
        expected = dict(schema=s.PUBLIC_INVENTORY_GROUND_CONTEXT_SCHEMA, challenge=CHALLENGE,
            control_epoch=EPOCH, capability=CAPABILITY, semantics='presented_after_request',
            view=dict(width=320, height=200, format='image/png'))
        self.assertEqual(reply['structuredContent'], expected); self.assertEqual(json.loads(reply['content'][0]['text']), expected)
        self.assertEqual(len(reply['content']), 2); self.assertFalse(reply['isError'])
        self.assertEqual(decode_png(self, reply['content'][1])[1], rgb); self.assertLess(len(dump(reply)), adapter.MAX_RESPONSE)
        source = io.BytesIO(b''.join(dump(msg) + b'\n' for msg in initialized_requests() +
            [tool_request(2, 'read_inventory_ground_context', {})])); output = io.BytesIO()
        adapter.serve(source, output, supervised_stance=client)
        self.assertTrue(all(len(line) + 1 <= adapter.MAX_RESPONSE for line in output.getvalue().splitlines()))

    def test_status_unchanged_and_errors_generic(self):
        for value in s.STATUSES:
            client = self.client(request_inventory_ground=lambda self, **args:status(value=value))
            reply = call(adapter.Server(supervised_stance=client), 'request_inventory_ground', ground_arguments())['result']
            self.assertEqual(reply['structuredContent'], dict(schema=s.PUBLIC_STATUS_SCHEMA, command_id=COMMAND,
                status=value, semantics='dispatch_status_not_success'))
        def fail(*args, **kwargs): raise Unavailable('PRIVATE-SENTINEL-item')
        client = self.client(request_inventory_ground=fail, observe_inventory_ground=fail)
        for name, args in (('request_inventory_ground', ground_arguments()), ('read_inventory_ground_context', {})):
            self.assertEqual(call(adapter.Server(supervised_stance=client), name, args)['result'],
                dict(content=[dict(type='text', text='{"error":"unavailable"}')], structuredContent={'error':'unavailable'}, isError=True))


class LauncherTests(unittest.TestCase):
    def test_requires_close_and_existing_chain_and_rejects_audio_only(self):
        chain = ['--supervised-stance', '--supervised-turn', '--supervised-inventory', '--supervised-inventory-close']
        for flags in [[], *[chain[:i] + chain[i + 1:] for i in range(len(chain))], ['--audio-only', *chain]]:
            with self.subTest(flags=flags), patch.object(sys, 'argv', ['launcher', *flags, '--supervised-inventory-ground', '--', 'game']), \
                    patch('launch_presentation.subprocess.Popen') as spawn, patch('sys.stderr', io.StringIO()), \
                    self.assertRaises(SystemExit): launch_presentation.main()
            spawn.assert_not_called()

    def test_ambient_scrubbed_literal_one_no_extra_channel_or_kill(self):
        chain = ['--supervised-stance', '--supervised-turn', '--supervised-inventory', '--supervised-inventory-close']
        for flags, enabled, count in (([], False, 1), (['--audio-only'], False, 1),
                (['--supervised-stance'], False, 2), (chain, False, 2),
                (chain + ['--supervised-inventory-ground'], True, 2)):
            for ambient in ('1', 'true', '99'):
                with self.subTest(flags=flags, ambient=ambient), \
                        patch.object(sys, 'argv', ['launcher', *flags, '--', 'synthetic-game']), \
                        patch.dict(os.environ, {'OPENXCOM_INVENTORY_GROUND_ENABLED':ambient}), \
                        patch('launch_presentation.subprocess.Popen') as spawn, patch('launch_presentation.serve') as serve:
                    launch_presentation.main(); env = spawn.call_args.kwargs['env']
                    self.assertEqual(env.get('OPENXCOM_INVENTORY_GROUND_ENABLED'), '1' if enabled else None)
                    self.assertEqual(len(set(spawn.call_args.kwargs['pass_fds'])), count)
                    client = serve.call_args.args[6]
                    if client is not None:
                        self.assertEqual(client.inventory_ground_enabled, enabled); self.assertIsNone(client.channel)
                    spawn.return_value.kill.assert_not_called(); spawn.return_value.terminate.assert_not_called()

    def run_launcher(self, calls, mode='normal', enabled=True, optimized=False):
        flags = ['--supervised-stance', '--supervised-turn', '--supervised-inventory', '--supervised-inventory-close']
        if enabled: flags.append('--supervised-inventory-ground')
        messages = initialized_requests() + [{'jsonrpc':'2.0', 'id':2, 'method':'tools/list'}]
        messages += [tool_request(i, name, args) for i, (name, args) in enumerate(calls, 3)]
        result = subprocess.run([sys.executable, *(['-O'] if optimized else []),
            str(Path(__file__).with_name('launch_presentation.py')), *flags, '--', sys.executable,
            str(Path(__file__).with_name('sdk_supervised_inventory_ground_smoke.py')),
            '--synthetic-producer', mode], input=b''.join(dump(msg) + b'\n' for msg in messages),
            capture_output=True, timeout=15, env=dict(os.environ, OPENXCOM_INVENTORY_GROUND_ENABLED='1'))
        self.assertEqual(result.returncode, 0); self.assertEqual(result.stderr, b'')
        lines = result.stdout.splitlines(); self.assertTrue(all(len(line) + 1 <= adapter.MAX_RESPONSE for line in lines))
        replies = [json.loads(line) for line in lines]; self.assertEqual(len(replies), len(calls) + 2)
        names = {t['name'] for t in replies[1]['result']['tools']}
        self.assertEqual('request_inventory_ground' in names, enabled)
        self.assertEqual('read_inventory_ground_context' in names, enabled)
        return replies[2:]

    def test_actual_launcher_ground_i_barrier_exact_duplicate_and_fresh_k(self):
        args = ground_arguments(capability=f'{2:032x}')
        calls = [('read_inventory_ground_context', {}), ('request_inventory_ground', dict(args, approved=True)),
                 ('request_inventory_ground', args), ('read_inventory_view', {}), ('request_inventory_ground', args),
                 ('read_inventory_ground_context', {}), ('read_inventory_close_context', {}), ('read_tactical_context', {}),
                 ('read_stance_context', {}), ('request_inventory_ground', dict(args, command_id='a' * 32)),
                 ('read_inventory_view', {}), ('request_inventory_ground', args), ('read_inventory_ground_context', {}),
                 ('request_inventory_ground', ground_arguments(capability=f'{3:032x}', command_id='a' * 32)),
                 ('read_stance_status', {'command_id':'a' * 32}), ('read_inventory_view', {})]
        for optimized in (False, True):
            replies = self.run_launcher(calls, optimized=optimized)
            self.assertEqual([i for i, r in enumerate(replies) if 'error' in r], [1])
            self.assertEqual([i for i, r in enumerate(replies) if r.get('result', {}).get('isError')], [3, 5, 6, 7, 8, 9])
            self.assertEqual(replies[0]['result']['structuredContent']['schema'], s.PUBLIC_INVENTORY_GROUND_CONTEXT_SCHEMA)
            self.assertEqual(replies[14]['result']['structuredContent']['status'], 'dispatched')
            self.assertNotIn('capability', replies[15]['result']['structuredContent'])

    def test_close_only_ambient_does_not_enable_ground(self):
        replies = self.run_launcher([('read_inventory_ground_context', {}), ('request_inventory_ground', ground_arguments())], enabled=False)
        self.assertTrue(all(r['error']['code'] == -32602 for r in replies))

    def test_actual_launcher_bad_k_or_post_g_i_closes_without_resend(self):
        args = ground_arguments(capability=f'{2:032x}')
        for mode in ('malformed', 'extra', 'wrong_kind', 'close_kind', 'capless', 'replay', 'alias', 'unused_palette', 'disconnect', 'delay', 'bad_post_i'):
            with self.subTest(mode=mode):
                initial = [('read_inventory_ground_context', {})]
                if mode == 'bad_post_i':
                    initial += [('request_inventory_ground', args), ('read_stance_status', {'command_id':COMMAND}), ('read_inventory_view', {})]
                replies = self.run_launcher(initial + [('request_inventory_ground', args), ('read_inventory_view', {}), ('read_inventory_ground_context', {})], mode)
                failed_at = 3 if mode == 'bad_post_i' else 0
                self.assertTrue(all(r['result']['isError'] for r in replies[failed_at:]))


if __name__ == '__main__': unittest.main()
