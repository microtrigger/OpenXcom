# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic ordinary inventory Unload authority, transport and launcher checks."""
import hashlib
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
from test_supervised_inventory_close import close_context
from test_supervised_inventory_ground import ground_context
from test_supervised_inventory_click import click_context
from test_supervised_inventory_right_click import RightFixture, right_context


unload_arguments = open_arguments


def context_for(action):
    return dict(D="O", N="O", R="V", L="V", B="V", C="J", G="K", E="H", X="U", F="Q")[action]


def unload_context(challenge=CHALLENGE, capability=CAPABILITY, epoch=EPOCH, source=None):
    obj = tactical_context(challenge, capability, epoch) if source is None else dict(source)
    obj['schema'] = s.INVENTORY_UNLOAD_CONTEXT_SCHEMA
    return obj


def invalid_unload_arguments():
    return invalid_arguments() + [dict(unload_arguments(), **{name: True}) for name in
        ('unload', 'page', 'page_index', 'page_count', 'next_page', 'scroll', 'preflight', 'can_unload',
         'x', 'y', 'button', 'modifiers', 'held', 'held_item', 'ammo', 'ammo_id', 'eligibility',
         'can_reload', 'can_unload', 'outcome', 'cost', 'time_units', 'shift', 'ctrl', 'alt')]


class UnloadFixture(RightFixture):
    def __init__(self, **kwargs):
        kwargs.setdefault('inventory_unload_enabled', True)
        super().__init__(**kwargs)

    def reply(self, raw):
        if self.response is not None: return self.response(raw, self)
        if raw[:1] == b'Q':
            self.counter += 1
            self.latest = unload_context(raw[1:].decode(), f'{self.counter:032x}')
            return dump(self.latest)
        if raw[:1] == b'F': return dump(status(raw[65:97].decode(), self.status))
        return super().reply(raw)

    def observe_unload(self): return self.client.observe_inventory_unload()

    def action(self, kind, command_id=COMMAND, capability=None):
        if kind == 'F':
            common = unload_arguments(self.latest['capability'] if capability is None else capability,
                                      EPOCH, command_id)
            return lambda: self.client.request_inventory_unload(**common)
        return super().action(kind, command_id, capability)

    def observation(self, kind):
        if kind == 'Q': return self.observe_unload()
        return super().observation(kind)


class ValidationTests(unittest.TestCase):
    def test_exact_context_roundtrip_noise_bound_and_indexed_png(self):
        for obj in (unload_context(), unload_context(source=noisy_context()),
                    unload_context(source=tactical_context(palette=bytes(768)))):
            self.assertEqual(set(obj), {'schema', 'challenge', 'control_epoch', 'capability', 'semantics', 'view'})
            self.assertEqual(s.validate_inventory_unload_context(dump(obj), CHALLENGE), (obj, decoded_rgb(obj)))
            self.assertLessEqual(len(dump(obj)), s.MAX_PACKET)
            self.assertEqual(decode_png(self, s.tactical_image_content(decoded_rgb(obj)))[1], decoded_rgb(obj))

    def test_missing_extra_fields_and_hidden_metadata_rejected(self):
        for inner in (False, True):
            original = unload_context()['view'] if inner else unload_context()
            for name in original:
                obj = unload_context(); del (obj['view'] if inner else obj)[name]
                with self.subTest(name=name, inner=inner), self.assertRaises(Unavailable):
                    s.validate_inventory_unload_context(dump(obj), CHALLENGE)
            for name in ('items', 'item_id', 'unit_id', 'name', 'slot', 'weight', 'held', 'time_units',
                         'busy', 'reason', 'timestamp', 'action', 'parent', 'generation', 'can_unload',
                         'page', 'page_count', 'page_index', 'next_page', 'preflight', 'ammo', 'ammo_id',
                         'held_item', 'eligibility', 'outcome', 'cost', 'approved', 'button', 'modifiers'):
                obj = unload_context(); (obj['view'] if inner else obj)[name] = 'PRIVATE-SENTINEL'
                with self.subTest(name=name, inner=inner), self.assertRaises(Unavailable):
                    s.validate_inventory_unload_context(dump(obj), CHALLENGE)

    def test_types_tokens_semantics_geometry_and_hex(self):
        for name in ('schema', 'challenge', 'control_epoch', 'capability', 'semantics', 'view'):
            for value in (None, True, 1, [], {}, '', 'a' * 31, 'A' * 32, 'g' * 32):
                obj = unload_context(); obj[name] = value
                with self.subTest(name=name, value=value), self.assertRaises(Unavailable):
                    s.validate_inventory_unload_context(dump(obj), CHALLENGE)
        for name, size in (('width', 320), ('height', 200)):
            for value in (None, True, False, [], {}, size - 1, size + 1, float(size), str(size)):
                obj = unload_context(); obj['view'][name] = value
                with self.assertRaises(Unavailable): s.validate_inventory_unload_context(dump(obj), CHALLENGE)
        for name, size in (('indices_hex', 128000), ('palette_hex', 1536)):
            for value in (None, False, 1, [], {}, '', '0' * (size - 1), '0' * (size + 1),
                          'fF' * (size // 2), ' ' * size, 'z' * size):
                obj = unload_context(); obj['view'][name] = value
                with self.assertRaises(Unavailable): s.validate_inventory_unload_context(dump(obj), CHALLENGE)

    def test_cross_schema_challenge_alias_unused_palette_and_duplicate_rejection(self):
        for obj in (tactical_context(), inventory_view(), close_context(), ground_context(), click_context(), right_context(), unload_context(challenge='a' * 32),
                    unload_context(source=tactical_context(indices=b'\1\0' * 32000,
                        palette=bytes.fromhex('ff00010180ff') + bytes(762))),
                    unload_context(source=tactical_context(indices=b'\0\1' * 32000,
                        palette=bytes.fromhex('0180ff0180ff') + bytes(762))),
                    unload_context(source=tactical_context(palette=bytes.fromhex('0180ffff0001') + bytes(762)))):
            with self.assertRaises(Unavailable): s.validate_inventory_unload_context(dump(obj), CHALLENGE)
        for validator in (s.validate_tactical_context, s.validate_inventory_view, s.validate_context,
                          s.validate_inventory_close_context, s.validate_inventory_ground_context,
                          s.validate_inventory_click_context, s.validate_inventory_right_click_context):
            with self.assertRaises(Unavailable): validator(dump(unload_context()), CHALLENGE)
        raw = dump(unload_context())
        for packet in (b'', b'null', b'false', b'[]', b'42', b'\xff', b'{"x":NaN}', b'[' * 2000,
                       raw[:-1], raw + b'\0', raw[:-1] + b',"schema":"duplicate"}',
                       b'x' * (s.MAX_PACKET + 1), None, 'text', bytearray(raw)):
            with self.assertRaises(Unavailable): s.validate_inventory_unload_context(packet, CHALLENGE)
        padded = raw + b' ' * (s.MAX_PACKET - len(raw))
        self.assertEqual(s.validate_inventory_unload_context(padded, CHALLENGE)[0], unload_context())
        with self.assertRaises(Unavailable): s.validate_inventory_unload_context(padded + b' ', CHALLENGE)

    def test_three_tokens_only_no_action_page_preflight_or_model_approval(self):
        s.validate_inventory_unload_arguments(unload_arguments())
        for obj in invalid_unload_arguments():
            with self.subTest(obj=obj), self.assertRaises(Unavailable): s.validate_inventory_unload_arguments(obj)

    def test_strict_opt_in_dependency_without_socket_mutation(self):
        parent, peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        try:
            for value in (None, 0, 1, '1', 'true', [], {}):
                with self.assertRaises(Unavailable):
                    s.StanceClient(parent, turn_enabled=True, inventory_enabled=True,
                                   inventory_close_enabled=True, inventory_unload_enabled=value)
                self.assertTrue(parent.getblocking())
            for turn, inventory, close in itertools.product((False, True), repeat=3):
                if turn and inventory and close: continue
                with self.assertRaises(Unavailable):
                    s.StanceClient(parent, turn_enabled=turn, inventory_enabled=inventory,
                        inventory_close_enabled=close, inventory_unload_enabled=True)
                self.assertGreaterEqual(parent.fileno(), 0)
        finally: parent.close(); peer.close()


class ClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = UnloadFixture(**kwargs); self.addCleanup(native.close); return native

    def dispatch(self, native, command_id=COMMAND):
        native.observe_unload(); args = native.open_args(command_id=command_id)
        native.status = 'pending_confirmation'; native.client.request_inventory_unload(**args)
        native.status = 'dispatched'; native.client.read_status(command_id)
        self.assertTrue(native.client.inventory_verification_required)
        self.assertFalse(native.client.tactical_verification_required)
        return args

    def assert_closed_no_retry(self, native):
        count = len(native.requests)
        self.assertEqual(native.client.state, 'CLOSED'); self.assertIsNone(native.client.channel)
        self.assertIsNone(native.client.capability); self.assertIsNone(native.client.capability_kind)
        for operation in (*(lambda k=k: native.observation(k) for k in 'OVIJKHUQ'),
                          *(native.action(kind) for kind in 'DNRLBCGEXF'),
                          lambda: native.client.read_status(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)

    def test_default_disabled_no_send_or_grant_change(self):
        native = self.fixture(inventory_unload_enabled=False); native.observe_close(); cap = native.client.capability
        for operation in (native.observe_unload, lambda: native.client.request_inventory_unload(**native.open_args())):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(native.client.capability, cap); native.action('C')()
        self.assertEqual([r[:1] for r in native.requests], [b'J', b'C'])

    def test_exact_q33_f97_wire_and_fresh_capless_i_required(self):
        native = self.fixture()
        with patch.object(s.secrets, 'token_hex', return_value=CHALLENGE):
            obj, rgb = native.observe_unload()
        self.assertEqual(native.requests, [('Q' + CHALLENGE).encode()]); self.assertEqual(rgb, decoded_rgb(obj))
        args = native.open_args(); native.client.request_inventory_unload(**args)
        self.assertEqual(native.requests[-1], ('F' + EPOCH + args['capability'] + COMMAND).encode())
        self.assertEqual(len(native.requests[-1]), 97)
        native.status = 'dispatched'; native.client.read_status(COMMAND); count = len(native.requests)
        for operation in (*(lambda k=k: native.observation(k) for k in 'OVJKHUQ'),
                          *(native.action(kind, 'a' * 32) for kind in 'DNRLBCGEXF')):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)
        obj, _ = native.observe_inventory(); self.assertFalse(native.client.inventory_verification_required)
        self.assertEqual(set(obj), {'schema', 'challenge', 'semantics', 'view'})
        self.assertIsNone(native.client.capability); self.assertIsNone(native.client.capability_kind)
        self.assertEqual(native.client.state, 'NEED_CONTEXT')
        with self.assertRaises(Unavailable): native.action('F', 'a' * 32)()
        native.observe_unload(); native.status = 'pending_confirmation'; native.action('F', 'a' * 32)()

    def test_full_eight_context_ten_action_matrix(self):
        for kind, action in itertools.product('OVIJKHUQ', 'DNRLBCGEXF'):
            with self.subTest(kind=kind, action=action):
                native = self.fixture(); native.observe_unload(); old = native.client.capability
                native.observation(kind); before = len(native.requests)
                operation = native.action(action, capability=old if kind == 'I' else None)
                allowed = {'O':'DN', 'V':'RLB', 'I':'', 'J':'C', 'K':'G', 'H':'E', 'U':'X', 'Q':'F'}
                if action in allowed[kind]:
                    operation(); self.assertEqual(native.requests[-1][:1], action.encode())
                else:
                    with self.assertRaises(Unavailable): operation()
                    self.assertEqual(len(native.requests), before)

    def test_q_replaces_prior_grants_i_retires_q_and_j_q_are_not_interchangeable(self):
        for kind in 'OVJKHUQ':
            native = self.fixture(); native.observation(kind); old = native.open_args(); native.observe_unload()
            with self.assertRaises(Unavailable): native.client.request_inventory_unload(**old)
            native.observe_inventory(); self.assertIsNone(native.client.capability)
            with self.assertRaises(Unavailable): native.action('F')()
            self.assertEqual(native.client.state, 'NEED_CONTEXT')
            native.observe_unload(); native.action('F')()
        native = self.fixture(); native.observe_unload(); old = native.open_args(); native.observe_close()
        with self.assertRaises(Unavailable): native.client.request_inventory_unload(**old)
        with self.assertRaises(Unavailable): native.action('F')()
        native.action('C')()

    def test_pending_unload_blocks_shared_observations_and_new_commands(self):
        native = self.fixture(); native.observe_unload(); native.action('F')(); before = len(native.requests)
        for operation in (*(lambda k=k: native.observation(k) for k in 'OVIJKHUQ'),
                          *(native.action(kind, 'a' * 32) for kind in 'DNRLBCGEXF')):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), before); self.assertEqual(native.client.state, 'PENDING')

    def test_q_never_satisfies_post_b_i_or_post_r_l_c_v(self):
        for kind in 'BRLC':
            native = self.fixture(); native.observation('J' if kind == 'C' else 'V'); native.action(kind)()
            native.status = 'dispatched'; native.client.read_status(COMMAND); before = len(native.requests)
            with self.assertRaises(Unavailable): native.observe_unload()
            self.assertEqual(native.client.inventory_verification_required, kind == 'B')
            self.assertEqual(native.client.tactical_verification_required, kind in 'RLC')
            self.assertEqual(len(native.requests), before)
            native.observation('I' if kind == 'B' else 'V'); native.observe_unload()

    def test_cancel_or_immediate_rejection_needs_fresh_context_all_kinds_allowed(self):
        for immediate, kind in itertools.product((False, True), 'OVIJKHUQ'):
            native = self.fixture(); native.observe_unload()
            native.status = 'unavailable' if immediate else 'pending_confirmation'; native.action('F')()
            if not immediate: native.status = 'unavailable'; native.client.read_status(COMMAND)
            self.assertFalse(native.client.inventory_verification_required)
            self.assertFalse(native.client.tactical_verification_required)
            self.assertEqual(native.client.state, 'NEED_CONTEXT'); self.assertIsNone(native.client.capability)
            with self.assertRaises(Unavailable): native.action('F', 'a' * 32)()
            native.observation(kind)

    def test_duplicate_never_dispatches_rearms_or_discharges_i_barrier(self):
        native = self.fixture(); args = self.dispatch(native)
        native.client.request_inventory_unload(**args); native.client.read_status(COMMAND)
        self.assertTrue(native.client.inventory_verification_required)
        native.observe_inventory(); native.client.request_inventory_unload(**args)
        self.assertFalse(native.client.inventory_verification_required)
        self.assertEqual(native.client.state, 'NEED_CONTEXT'); self.assertIsNone(native.client.capability)
        native.observe_unload(); cap = native.client.capability
        native.client.request_inventory_unload(**args); native.client.read_status(COMMAND)
        self.assertFalse(native.client.inventory_verification_required)
        self.assertEqual(native.client.capability, cap); self.assertEqual(native.client.state, 'READY')
        self.assertEqual(sum(r[:1] == b'F' for r in native.requests), 1)

    def test_old_f_status_preserves_new_b_r_l_c_f_barriers(self):
        for kind in 'BRLCF':
            native = self.fixture(); old = self.dispatch(native); native.observe_inventory()
            native.observation('Q' if kind == 'F' else 'J' if kind == 'C' else 'V')
            native.status = 'pending_confirmation'; native.action(kind, 'a' * 32)()
            native.status = 'dispatched'; native.client.read_status('a' * 32)
            native.client.request_inventory_unload(**old)
            self.assertEqual(native.client.inventory_verification_required, kind in 'BF')
            self.assertEqual(native.client.tactical_verification_required, kind in 'RLC')

    def test_old_c_status_preserves_new_unload_barrier(self):
        native = self.fixture(); native.observe_close(); old = native.open_args(); native.action('C')()
        native.status = 'dispatched'; native.client.read_status(COMMAND); native.observe_tactical()
        self.dispatch(native, 'a' * 32); native.client.request_close_inventory(**old)
        self.assertTrue(native.client.inventory_verification_required)
        self.assertFalse(native.client.tactical_verification_required)

    def test_conflicting_ids_across_ten_actions_and_changed_tokens(self):
        for first, second in itertools.permutations('DNRLBCGEXF', 2):
            native = self.fixture(); native.observation(context_for(first))
            native.action(first)(); before = len(native.requests)
            with self.assertRaises(Unavailable): native.action(second)()
            self.assertEqual(len(native.requests), before)
        native = self.fixture(); native.observe_unload(); args = native.open_args(); native.action('F')()
        for name in ('control_epoch', 'capability'):
            with self.assertRaises(Unavailable): native.client.request_inventory_unload(**dict(args, **{name:'a' * 32}))

    def test_shared_non_evicting_64_ledger_never_sends_65th_command(self):
        native = self.fixture(); native.status = 'unavailable'
        for index in range(s.MAX_COMMANDS):
            kind = 'DNRLBCGEXF'[index % 10]
            native.observation(context_for(kind))
            native.action(kind, f'{index:032x}')()
        self.assertEqual(len(native.client.commands), 64)
        native.observe_unload(); before = len(native.requests)
        with self.assertRaises(Unavailable): native.action('F', 'f' * 32)()
        self.assertEqual(len(native.requests), before); self.assert_closed_no_retry(native)

    def test_wrong_epoch_stale_capability_and_local_invalid_arguments_do_not_send(self):
        native = self.fixture(); native.observe_unload(); old = native.open_args(); native.observe_unload()
        for args in (old, dict(native.open_args(), control_epoch='a' * 32)):
            with self.assertRaises(Unavailable): native.client.request_inventory_unload(**args)
        self.assertEqual([r[:1] for r in native.requests], [b'Q', b'Q'])

    def test_q_failure_is_terminal_without_replay(self):
        for mode in ('malformed', 'extra', 'wrong_kind', 'close_kind', 'capless', 'replay', 'disconnect'):
            native = self.fixture(); native.observe_unload()
            def respond(raw, fixture, mode=mode):
                obj = unload_context(raw[1:].decode(), f'{3:032x}')
                if mode == 'malformed': return b'{}'
                if mode == 'extra': obj['items'] = []
                if mode == 'wrong_kind': obj['schema'] = s.TACTICAL_CONTEXT_SCHEMA
                if mode == 'close_kind': obj['schema'] = s.INVENTORY_CLOSE_CONTEXT_SCHEMA
                if mode == 'capless': obj = inventory_view(raw[1:].decode())
                if mode == 'replay': obj['challenge'] = 'a' * 32
                if mode == 'disconnect': return None
                return dump(obj)
            native.response = respond
            with self.assertRaises(Unavailable): native.observe_unload()
            self.assert_closed_no_retry(native); self.assertEqual([r[:1] for r in native.requests], [b'Q', b'Q'])

    def test_f_uncertain_or_initial_dispatched_response_is_terminal(self):
        for response in (None, b'{}', dump(status(value='dispatched'))):
            native = self.fixture(); native.observe_unload()
            native.response = lambda raw, fixture, response=response: response
            with self.assertRaises(Unavailable): native.action('F')()
            self.assert_closed_no_retry(native); self.assertEqual([r[:1] for r in native.requests], [b'Q', b'F'])

    def test_f_terminal_status_regression_closes(self):
        native = self.fixture(); self.dispatch(native); native.status = 'unavailable'
        with self.assertRaises(Unavailable): native.client.read_status(COMMAND)
        self.assert_closed_no_retry(native)

    def test_failed_post_f_i_preserves_barrier_without_replay(self):
        native = self.fixture(); self.dispatch(native)
        native.response = lambda raw, fixture: dump(unload_context(raw[1:].decode()))
        with self.assertRaises(Unavailable): native.observe_inventory()
        self.assertTrue(native.client.inventory_verification_required); self.assert_closed_no_retry(native)
        self.assertEqual([r[:1] for r in native.requests], [b'Q', b'F', b'S', b'I'])

    def test_q_f_and_post_f_i_use_normal_deadlines_duplicate_does_not_extend(self):
        self.assertEqual(s.TIMEOUT, 2.0); native = self.fixture()
        with patch.object(native.client, 'exchange', wraps=native.client.exchange) as exchange:
            native.observe_unload()
        self.assertEqual(exchange.call_args.kwargs, {'timeout': .3})
        args = self.dispatch(native)
        with patch.object(native.client, 'exchange', wraps=native.client.exchange) as exchange:
            native.observe_inventory()
        self.assertEqual(exchange.call_args.kwargs, {'timeout': .3})
        native.client.request_inventory_unload(**args)
        with patch.object(native.client, 'exchange', wraps=native.client.exchange) as exchange:
            native.observe_unload()
        self.assertEqual(exchange.call_args.kwargs, {'timeout': .3})

    def test_q_f_and_post_f_i_timeout_are_terminal(self):
        for kind in 'QFI':
            native = self.fixture(); self.dispatch(native) if kind == 'I' else native.observe_unload()
            native.client.timeout = .03
            def delayed(raw, fixture):
                time.sleep(.08)
                if raw[:1] == b'F': return dump(status())
                return dump(inventory_view(raw[1:].decode()) if kind == 'I' else unload_context(raw[1:].decode(), f'{9:032x}'))
            native.response = delayed
            with self.assertRaises(Unavailable): (native.action('F') if kind == 'F' else lambda: native.observation(kind))()
            self.assert_closed_no_retry(native)

    def test_extra_queued_q_response_and_single_flight_fail_closed(self):
        native = self.fixture(); native.observe_unload(); native.client.flight.acquire(); before = len(native.requests)
        try:
            for operation in (native.observe_unload, native.action('F')):
                with self.assertRaises(Unavailable): operation()
            self.assertTrue(native.client.flight.locked()); self.assertEqual(len(native.requests), before)
        finally: native.client.flight.release()
        queued = threading.Event()
        def respond(raw, fixture):
            reply = dump(unload_context(raw[1:].decode(), f'{3:032x}'))
            fixture.peer.send(reply); fixture.peer.send(reply); queued.set(); return None
        native.response = respond; packet = native.client.packet
        def after_queue(deadline, maximum):
            self.assertTrue(queued.wait(.3)); return packet(deadline, maximum)
        with patch.object(native.client, 'packet', side_effect=after_queue), self.assertRaises(Unavailable):
            native.observe_unload()
        self.assert_closed_no_retry(native)


    def test_independent_unload_without_ground_left_or_right(self):
        native = self.fixture(inventory_ground_enabled=False, inventory_click_enabled=False,
                              inventory_right_click_enabled=False)
        self.dispatch(native); native.observe_inventory(); native.observe_unload()
        self.assertEqual(native.client.capability_kind, 'Q')

    def test_q_cannot_satisfy_existing_ground_left_or_either_right_barrier(self):
        for action, destination in (('G', 'I'), ('E', 'I'), ('X', 'I'), ('X', 'V')):
            native = self.fixture(destination=destination)
            native.observation(context_for(action)); native.action(action)()
            native.status = 'dispatched'; native.client.read_status(COMMAND)
            count = len(native.requests)
            with self.assertRaises(Unavailable): native.observe_unload()
            self.assertEqual(len(native.requests), count)
            native.observation(destination); native.observe_unload()

    def test_old_f_preserves_ground_left_or_unresolved_right_barrier(self):
        for action, destination in (('G', 'I'), ('E', 'I'), ('X', 'I'), ('X', 'V')):
            native = self.fixture(destination=destination); old = self.dispatch(native)
            native.observe_inventory(); native.observation(context_for(action))
            native.status = 'pending_confirmation'; native.action(action, 'a' * 32)()
            native.status = 'dispatched'; native.client.read_status('a' * 32)
            native.client.request_inventory_unload(**old)
            self.assertEqual(native.client.inventory_verification_required, action in 'GE')
            self.assertEqual(native.client.inventory_right_verification_required, action == 'X')
            self.assertFalse(native.client.tactical_verification_required)
            count = len(native.requests)
            with self.assertRaises(Unavailable): native.observe_unload()
            self.assertEqual(len(native.requests), count)
            native.observation(destination); native.observe_unload()

    def test_failed_required_i_send_or_disconnect_keeps_barrier(self):
        for mode in ('send', 'disconnect', 'malformed', 'timeout'):
            native = self.fixture(); self.dispatch(native)
            if mode == 'send':
                with patch.object(native.client, 'exchange', side_effect=OSError), self.assertRaises(Unavailable):
                    native.observe_inventory()
            else:
                native.client.timeout = .03
                def reply(raw, fixture, mode=mode):
                    if mode == 'timeout': time.sleep(.08)
                    return None if mode == 'disconnect' else b'{}'
                native.response = reply
                with self.assertRaises(Unavailable): native.observe_inventory()
            self.assertTrue(native.client.inventory_verification_required)
            self.assert_closed_no_retry(native)

    def test_status_extras_and_predicted_success_are_terminal(self):
        for extra in ('outcome', 'held', 'ammo', 'items', 'eligible', 'reason', 'approved'):
            native = self.fixture(); native.observe_unload(); native.action('F')()
            native.response = lambda raw, fixture, extra=extra: dump(dict(status(value='dispatched'), **{extra: True}))
            with self.assertRaises(Unavailable): native.client.read_status(COMMAND)
            self.assert_closed_no_retry(native)
        for value in ('success', 'unloaded', 'no_op', 'refused'):
            native = self.fixture(); native.observe_unload(); native.action('F')()
            native.response = lambda raw, fixture, value=value: dump(status(value=value))
            with self.assertRaises(Unavailable): native.client.read_status(COMMAND)
            self.assert_closed_no_retry(native)


class AdapterTests(unittest.TestCase):
    def test_default_definition_matrix_unchanged_and_unload_dependencies(self):
        # Exact compact JSON catalogue digest measured from the actual 12-flag
        # adapter at 641c639e9087cc76770a6e9efa9b33baef1e72a9, before this pilot.
        digest = hashlib.sha256()
        for flags in itertools.product((False, True), repeat=12):
            tools = adapter.definitions(*flags)
            self.assertEqual(tools, adapter.definitions(*flags, supervised_inventory_unload=False))
            digest.update(json.dumps(tools, separators=(',', ':')).encode() + b'\n')
        self.assertEqual(digest.hexdigest(), 'ecc709248b2378607f88aa9f59bfd5ea359ae8a5499145746e334200ba096d8a')
        for stance, turn, inventory, close, unload in itertools.product((False, True), repeat=5):
            tools = adapter.definitions(supervised_stance=stance, supervised_turn=turn,
                supervised_inventory=inventory, supervised_inventory_close=close, supervised_inventory_unload=unload)
            for name in ('request_inventory_unload', 'read_inventory_unload_context'):
                self.assertEqual(name in {t['name'] for t in tools}, stance and turn and inventory and close and unload)
        for value in (None, 0, 1, '1', 'true', [], {}):
            tools = adapter.definitions(supervised_stance=True, supervised_turn=True,
                supervised_inventory=True, supervised_inventory_close=True, supervised_inventory_unload=value)
            self.assertNotIn('request_inventory_unload', {tool['name'] for tool in tools})
        for turn, inventory, close, unload in ((False, True, True, True), (True, False, True, True),
                (True, True, False, True), (True, True, True, False), (True, True, True, 1), (True, True, True, '1')):
            client = type('Disabled', (), dict(turn_enabled=turn, inventory_enabled=inventory,
                inventory_close_enabled=close, inventory_unload_enabled=unload))()
            for name, args in (('request_inventory_unload', unload_arguments()), ('read_inventory_unload_context', {})):
                self.assertIn('error', call(adapter.Server(supervised_stance=client), name, args))

    def client(self, **attrs):
        return type('Client', (), dict(turn_enabled=True, inventory_enabled=True,
            inventory_close_enabled=True, inventory_unload_enabled=True, **attrs))()

    def test_exact_tool_schemas_annotations_and_ordinary_unload_wording(self):
        tools = adapter.definitions(supervised_stance=True, supervised_turn=True,
            supervised_inventory=True, supervised_inventory_close=True, supervised_inventory_unload=True)
        names = [t['name'] for t in tools]
        self.assertNotIn('request_selection', names); self.assertNotIn('request_map_left_click', names)
        tool = next(t for t in tools if t['name'] == 'request_inventory_unload')
        self.assertEqual(set(tool['inputSchema']['properties']), set(unload_arguments()))
        self.assertEqual(set(tool['inputSchema']['required']), set(unload_arguments()))
        self.assertFalse(tool['inputSchema']['additionalProperties'])
        self.assertEqual(tool['annotations'], dict(readOnlyHint=False, destructiveHint=True, idempotentHint=True, openWorldHint=False))
        for phrase in ('ordinary Unload button attempt', 'do nothing or show a warning',
                       'successful unload spends 8 TU', 'right hand', 'ammunition in the left',
                       'every dispatched attempt', 'read_tactical_context cannot replace',
                       'does not cancel held selection', 'never successful unloading'):
            self.assertIn(phrase, tool['description'])
        self.assertNotIn('read-only', tool['description']); self.assertNotIn('scroll', tool['description'])
        view = next(t for t in tools if t['name'] == 'read_inventory_unload_context')
        self.assertEqual(view['inputSchema'], dict(type='object', properties={}, additionalProperties=False))
        self.assertTrue(view['annotations']['readOnlyHint']); self.assertFalse(view['annotations']['idempotentHint'])
        for name in ('read_tactical_context', 'read_inventory_view', 'read_stance_status', 'read_inventory_close_context'):
            self.assertEqual(names.count(name), 1)

    def test_invalid_inputs_never_reach_client(self):
        def fail(*args, **kwargs): self.fail('Invalid arguments reached transport')
        client = self.client(request_inventory_unload=fail, observe_inventory_unload=fail)
        for args in invalid_unload_arguments():
            self.assertEqual(call(adapter.Server(supervised_stance=client), 'request_inventory_unload', args)['error']['code'], -32602)
        for args in (False, [], {'capability':CAPABILITY}, {'unload':True}, {'items':[]}, {'page':1}):
            self.assertEqual(call(adapter.Server(supervised_stance=client), 'read_inventory_unload_context', args)['error']['code'], -32602)

    def test_public_unload_envelope_exact_indexed_pixels_and_bound(self):
        obj = unload_context(source=noisy_context()); rgb = decoded_rgb(obj)
        client = self.client(observe_inventory_unload=lambda self:(obj, rgb))
        reply = call(adapter.Server(supervised_stance=client), 'read_inventory_unload_context', {})['result']
        expected = dict(schema=s.PUBLIC_INVENTORY_UNLOAD_CONTEXT_SCHEMA, challenge=CHALLENGE,
            control_epoch=EPOCH, capability=CAPABILITY, semantics='presented_after_request',
            view=dict(width=320, height=200, format='image/png'))
        self.assertEqual(reply['structuredContent'], expected); self.assertEqual(json.loads(reply['content'][0]['text']), expected)
        self.assertEqual(len(reply['content']), 2); self.assertFalse(reply['isError'])
        self.assertEqual(decode_png(self, reply['content'][1])[1], rgb); self.assertLess(len(dump(reply)), adapter.MAX_RESPONSE)
        source = io.BytesIO(b''.join(dump(msg) + b'\n' for msg in initialized_requests() +
            [tool_request(2, 'read_inventory_unload_context', {})])); output = io.BytesIO()
        adapter.serve(source, output, supervised_stance=client)
        self.assertTrue(all(len(line) + 1 <= adapter.MAX_RESPONSE for line in output.getvalue().splitlines()))

    def test_status_unchanged_and_errors_generic(self):
        for value in s.STATUSES:
            client = self.client(request_inventory_unload=lambda self, **args:status(value=value))
            reply = call(adapter.Server(supervised_stance=client), 'request_inventory_unload', unload_arguments())['result']
            self.assertEqual(reply['structuredContent'], dict(schema=s.PUBLIC_STATUS_SCHEMA, command_id=COMMAND,
                status=value, semantics='dispatch_status_not_success'))
        def fail(*args, **kwargs): raise Unavailable('PRIVATE-SENTINEL-item')
        client = self.client(request_inventory_unload=fail, observe_inventory_unload=fail)
        for name, args in (('request_inventory_unload', unload_arguments()), ('read_inventory_unload_context', {})):
            self.assertEqual(call(adapter.Server(supervised_stance=client), name, args)['result'],
                dict(content=[dict(type='text', text='{"error":"unavailable"}')], structuredContent={'error':'unavailable'}, isError=True))


class LauncherTests(unittest.TestCase):
    def test_requires_close_and_existing_chain_and_rejects_audio_only(self):
        chain = ['--supervised-stance', '--supervised-turn', '--supervised-inventory', '--supervised-inventory-close']
        for flags in [[], *[chain[:i] + chain[i + 1:] for i in range(len(chain))], ['--audio-only', *chain]]:
            with self.subTest(flags=flags), patch.object(sys, 'argv', ['launcher', *flags, '--supervised-inventory-unload', '--', 'game']), \
                    patch('launch_presentation.subprocess.Popen') as spawn, patch('sys.stderr', io.StringIO()), \
                    self.assertRaises(SystemExit): launch_presentation.main()
            spawn.assert_not_called()

    def test_ambient_scrubbed_literal_one_no_extra_channel_or_kill(self):
        chain = ['--supervised-stance', '--supervised-turn', '--supervised-inventory', '--supervised-inventory-close']
        for flags, enabled, count in (([], False, 1), (['--audio-only'], False, 1),
                (['--supervised-stance'], False, 2), (chain, False, 2),
                (chain + ['--supervised-inventory-unload'], True, 2)):
            for ambient in ('1', 'true', '99'):
                with self.subTest(flags=flags, ambient=ambient), \
                        patch.object(sys, 'argv', ['launcher', *flags, '--', 'synthetic-game']), \
                        patch.dict(os.environ, {'OPENXCOM_INVENTORY_UNLOAD_ENABLED':ambient}), \
                        patch('launch_presentation.subprocess.Popen') as spawn, patch('launch_presentation.serve') as serve:
                    launch_presentation.main(); env = spawn.call_args.kwargs['env']
                    self.assertEqual(env.get('OPENXCOM_INVENTORY_UNLOAD_ENABLED'), '1' if enabled else None)
                    self.assertEqual(len(set(spawn.call_args.kwargs['pass_fds'])), count)
                    client = serve.call_args.args[6]
                    if client is not None:
                        self.assertEqual(client.inventory_unload_enabled, enabled); self.assertIsNone(client.channel)
                    spawn.return_value.kill.assert_not_called(); spawn.return_value.terminate.assert_not_called()

    def run_launcher(self, calls, mode='normal', enabled=True, optimized=False):
        flags = ['--supervised-stance', '--supervised-turn', '--supervised-inventory', '--supervised-inventory-close']
        if enabled: flags.append('--supervised-inventory-unload')
        messages = initialized_requests() + [{'jsonrpc':'2.0', 'id':2, 'method':'tools/list'}]
        messages += [tool_request(i, name, args) for i, (name, args) in enumerate(calls, 3)]
        result = subprocess.run([sys.executable, *(['-O'] if optimized else []),
            str(Path(__file__).with_name('launch_presentation.py')), *flags, '--', sys.executable,
            str(Path(__file__).with_name('sdk_supervised_inventory_unload_smoke.py')),
            '--synthetic-producer', mode], input=b''.join(dump(msg) + b'\n' for msg in messages),
            capture_output=True, timeout=15, env=dict(os.environ, OPENXCOM_INVENTORY_UNLOAD_ENABLED='1'))
        self.assertEqual(result.returncode, 0); self.assertEqual(result.stderr, b'')
        lines = result.stdout.splitlines(); self.assertTrue(all(len(line) + 1 <= adapter.MAX_RESPONSE for line in lines))
        replies = [json.loads(line) for line in lines]; self.assertEqual(len(replies), len(calls) + 2)
        names = {t['name'] for t in replies[1]['result']['tools']}
        self.assertEqual('request_inventory_unload' in names, enabled)
        self.assertEqual('read_inventory_unload_context' in names, enabled)
        return replies[2:]

    def test_actual_launcher_unload_i_barrier_exact_duplicate_and_fresh_q(self):
        args = unload_arguments(capability=f'{2:032x}')
        calls = [('read_inventory_unload_context', {}), ('request_inventory_unload', dict(args, approved=True)),
                 ('request_inventory_unload', args), ('read_inventory_view', {}), ('request_inventory_unload', args),
                 ('read_inventory_unload_context', {}), ('read_inventory_close_context', {}), ('read_tactical_context', {}),
                 ('read_stance_context', {}), ('request_inventory_unload', dict(args, command_id='a' * 32)),
                 ('read_inventory_view', {}), ('request_inventory_unload', args), ('read_inventory_unload_context', {}),
                 ('request_inventory_unload', unload_arguments(capability=f'{3:032x}', command_id='a' * 32)),
                 ('read_stance_status', {'command_id':'a' * 32}), ('read_inventory_view', {})]
        for optimized in (False, True):
            replies = self.run_launcher(calls, optimized=optimized)
            self.assertEqual([i for i, r in enumerate(replies) if 'error' in r], [1])
            self.assertEqual([i for i, r in enumerate(replies) if r.get('result', {}).get('isError')], [3, 5, 6, 7, 8, 9])
            self.assertEqual(replies[0]['result']['structuredContent']['schema'], s.PUBLIC_INVENTORY_UNLOAD_CONTEXT_SCHEMA)
            self.assertEqual(replies[14]['result']['structuredContent']['status'], 'dispatched')
            self.assertNotIn('capability', replies[15]['result']['structuredContent'])

    def test_close_only_ambient_does_not_enable_unload(self):
        replies = self.run_launcher([('read_inventory_unload_context', {}), ('request_inventory_unload', unload_arguments())], enabled=False)
        self.assertTrue(all(r['error']['code'] == -32602 for r in replies))

    def test_actual_launcher_bad_q_or_post_f_i_closes_without_resend(self):
        args = unload_arguments(capability=f'{2:032x}')
        for mode in ('malformed', 'extra', 'wrong_kind', 'close_kind', 'capless', 'replay', 'alias', 'unused_palette', 'disconnect', 'delay', 'bad_post_i'):
            with self.subTest(mode=mode):
                initial = [('read_inventory_unload_context', {})]
                if mode == 'bad_post_i':
                    initial += [('request_inventory_unload', args), ('read_stance_status', {'command_id':COMMAND}), ('read_inventory_view', {})]
                replies = self.run_launcher(initial + [('request_inventory_unload', args), ('read_inventory_view', {}), ('read_inventory_unload_context', {})], mode)
                failed_at = 3 if mode == 'bad_post_i' else 0
                self.assertTrue(all(r['result']['isError'] for r in replies[failed_at:]))


if __name__ == '__main__': unittest.main()
