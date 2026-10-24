# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic ordinary map left-click controls; no native gameplay or approval."""
import io
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time
import unittest
from unittest.mock import patch

import adapter
import launch_presentation
import supervised_stance as s
from snapshots import Unavailable
from test_supervised_stance import (CAPABILITY, COMMAND, EPOCH, arguments, call,
                                    dump, status)
from test_supervised_selection import selection_arguments
from test_supervised_turn import (TurnFixture, decode_png, decoded_rgb, initialized_requests,
                                  tactical_context, tool_request, turn_arguments)


left_arguments = turn_arguments


def invalid_arguments():
    invalid = [None, [], False, {}, arguments(), selection_arguments()]
    for name in left_arguments():
        obj = left_arguments(); del obj[name]; invalid.append(obj)
    for name in ('approved', 'human_approved', 'force', 'unit_id', 'targets', 'world_x',
                 'world_y', 'preflight', 'cost', 'path', 'button', 'modifiers', 'preview'):
        invalid.append(dict(left_arguments(), **{name: True}))
    for name, below, above in (('x', 15, 304), ('y', 39, 128)):
        for value in (None, True, False, 16.0, 80.0, '80', [], {}, below, above, -1, 2**64):
            invalid.append(dict(left_arguments(), **{name: value}))
    for name in ('capability', 'control_epoch', 'command_id'):
        for value in (None, False, 1, [], {}, 'a' * 31, 'A' * 32, 'z' * 32):
            invalid.append(dict(left_arguments(), **{name: value}))
    return invalid


class LeftClickFixture(TurnFixture):
    def __init__(self, **kwargs):
        kwargs.setdefault('map_left_click_enabled', True)
        super().__init__(**kwargs)

    def reply(self, raw):
        if self.response is not None: return self.response(raw, self)
        if raw[:1] == b'L': return dump(status(raw[65:97].decode(), self.status))
        return super().reply(raw)

    def left_args(self, **kwargs):
        return left_arguments(self.latest['capability'], self.latest['control_epoch'], **kwargs)


class ValidationTests(unittest.TestCase):
    def test_exact_schema_integer_bounds_and_no_oracles_or_approval(self):
        for x in (16, 160, 303):
            for y in (40, 80, 127):
                s.validate_map_left_click_arguments(left_arguments(x=x, y=y))
        for obj in invalid_arguments():
            with self.subTest(obj=obj), self.assertRaises(Unavailable):
                s.validate_map_left_click_arguments(obj)

    def test_opt_in_requires_strict_boolean_and_turn(self):
        for value in (None, 0, 1, '1', 'true', [], {}):
            with self.subTest(value=value), self.assertRaises(Unavailable):
                s.StanceClient(None, turn_enabled=True, map_left_click_enabled=value)
        parent, peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        try:
            with self.assertRaises(Unavailable): s.StanceClient(parent, map_left_click_enabled=True)
            self.assertGreaterEqual(parent.fileno(), 0)
        finally: parent.close(); peer.close()


class ClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = LeftClickFixture(**kwargs); self.addCleanup(native.close); return native

    def assert_closed_no_retry(self, native):
        count = len(native.requests)
        self.assertEqual(native.client.state, 'CLOSED')
        self.assertIsNone(native.client.channel); self.assertIsNone(native.client.capability)
        self.assertIsNone(native.client.capability_kind)
        for operation in (native.observe, native.observe_tactical,
                          lambda: native.client.request_map_left_click(**left_arguments()),
                          lambda: native.client.request_turn(**turn_arguments()),
                          lambda: native.client.request(**arguments()),
                          lambda: native.client.request_selection(**selection_arguments()),
                          lambda: native.client.read_status(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)

    def dispatch(self, native, ident=COMMAND):
        native.observe_tactical(); args = native.left_args(command_id=ident)
        native.client.request_map_left_click(**args)
        native.status = 'dispatched'; native.client.read_status(ident)
        return args

    def test_disabled_left_click_preserves_tactical_grant_and_right_click(self):
        native = self.fixture(map_left_click_enabled=False); native.observe_tactical()
        with self.assertRaises(Unavailable): native.client.request_map_left_click(**native.left_args())
        self.assertEqual([r[:1] for r in native.requests], [b'V'])
        self.assertEqual(native.client.state, 'READY')
        native.client.request_turn(**native.turn_args())
        self.assertEqual([r[:1] for r in native.requests], [b'V', b'R'])

    def test_exact_103_byte_packet_and_boundary_coordinates(self):
        native = self.fixture()
        for index, (x, y) in enumerate(((16, 40), (303, 127), (160, 80))):
            native.observe_tactical(); args = native.left_args(command_id=f'{index:032x}', x=x, y=y)
            result = native.client.request_map_left_click(**args)
            self.assertEqual(result['status'], 'pending_confirmation')
            expected = ('L' + EPOCH + args['capability'] + args['command_id'] + f'{x:03d}{y:03d}').encode()
            self.assertEqual(native.requests[-1], expected); self.assertEqual(len(expected), 103)
            self.assertEqual(native.client.state, 'PENDING'); self.assertIsNone(native.client.capability)
            native.status = 'dispatched'; native.client.read_status(args['command_id'])
            native.status = 'pending_confirmation'

    def test_v_authorizes_either_map_action_only_once(self):
        for first, second in (('request_map_left_click', 'request_turn'), ('request_turn', 'request_map_left_click')):
            native = self.fixture(); native.observe_tactical(); args = native.left_args()
            getattr(native.client, first)(**args)
            with self.assertRaises(Unavailable): getattr(native.client, second)(**dict(args, command_id='a' * 32))
            self.assertEqual(len(native.requests), 2); self.assertEqual(native.client.state, 'PENDING')

    def test_panel_never_authorizes_l_or_r_tactical_never_d_or_n(self):
        native = self.fixture(); native.observe()
        for method in (native.client.request_map_left_click, native.client.request_turn):
            with self.assertRaises(Unavailable): method(**native.left_args())
        native.observe_tactical()
        for method, args in ((native.client.request, native.args()),
                             (native.client.request_selection, selection_arguments(native.latest['capability'], EPOCH))):
            with self.assertRaises(Unavailable): method(**args)
        self.assertEqual([r[:1] for r in native.requests], [b'O', b'V'])
        self.assertEqual(native.client.state, 'READY')

    def test_both_observation_kinds_retire_previous_left_click_grant(self):
        for tactical in (False, True):
            native = self.fixture(); native.observe_tactical(); old = native.left_args()
            (native.observe_tactical if tactical else native.observe)(); count = len(native.requests)
            with self.assertRaises(Unavailable): native.client.request_map_left_click(**old)
            self.assertEqual(len(native.requests), count)

    def test_pending_blocks_all_contexts_and_new_actions(self):
        native = self.fixture(); native.observe_tactical(); args = native.left_args()
        native.client.request_map_left_click(**args); count = len(native.requests)
        for operation in (native.observe, native.observe_tactical,
                          lambda: native.client.request_map_left_click(**dict(args, command_id='a' * 32)),
                          lambda: native.client.request_turn(**dict(args, command_id='a' * 32)),
                          lambda: native.client.request(**native.args(command_id='b' * 32)),
                          lambda: native.client.request_selection(**selection_arguments(command_id='c' * 32))):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'PENDING')

    def test_dispatched_requires_successful_fresh_v_before_any_action(self):
        native = self.fixture(); args = self.dispatch(native); count = len(native.requests)
        self.assertTrue(native.client.tactical_verification_required)
        with self.assertRaises(Unavailable): native.observe()
        with self.assertRaises(Unavailable): native.client.request_map_left_click(**dict(args, command_id='a' * 32))
        self.assertEqual(len(native.requests), count)
        native.observe_tactical(); self.assertFalse(native.client.tactical_verification_required)
        native.observe(); native.status = 'pending_confirmation'
        native.client.request_selection(**selection_arguments(native.latest['capability'], EPOCH, 'a' * 32))
        self.assertEqual([r[:1] for r in native.requests], [b'V', b'L', b'S', b'V', b'O', b'N'])

    def test_cancellation_and_immediate_unavailable_allow_either_fresh_kind(self):
        for immediate in (False, True):
            for tactical in (False, True):
                native = self.fixture(); native.observe_tactical()
                if immediate: native.status = 'unavailable'
                native.client.request_map_left_click(**native.left_args())
                if not immediate:
                    native.status = 'unavailable'; native.client.read_status(COMMAND)
                self.assertFalse(native.client.tactical_verification_required)
                (native.observe_tactical if tactical else native.observe)()
                self.assertEqual(native.client.state, 'READY')

    def test_exact_duplicates_query_s_only_and_preserve_new_grant(self):
        native = self.fixture(); native.observe_tactical(); args = native.left_args()
        native.client.request_map_left_click(**args)
        for value in ('pending_confirmation', 'dispatched', 'dispatched'):
            native.status = value
            self.assertEqual(native.client.request_map_left_click(**args)['status'], value)
            self.assertEqual(native.requests[-1], b'S' + COMMAND.encode() + f'{native.client._query_sequence:032x}'.encode())
        native.observe_tactical(); capability = native.client.capability
        native.client.request_map_left_click(**args)
        self.assertEqual(native.client.capability, capability); self.assertEqual(native.client.state, 'READY')
        self.assertFalse(native.client.tactical_verification_required)
        self.assertEqual(sum(r[:1] == b'L' for r in native.requests), 1)

    def test_old_duplicate_does_not_change_new_pending_command(self):
        native = self.fixture(); old = self.dispatch(native); native.observe_tactical()
        native.status = 'pending_confirmation'
        native.client.request_turn(**native.turn_args(command_id='a' * 32))
        native.status = 'dispatched'; native.client.request_map_left_click(**old)
        self.assertEqual(native.client.active, 'a' * 32); self.assertEqual(native.client.state, 'PENDING')
        self.assertFalse(native.client.tactical_verification_required)

    def test_all_cross_kind_and_changed_argument_conflicts_never_send(self):
        for kind in ('D', 'N', 'R', 'L'):
            native = self.fixture(); (native.observe_tactical if kind in ('R', 'L') else native.observe)()
            args = native.left_args()
            methods = {'D': lambda: native.client.request(**native.args()),
                       'N': lambda: native.client.request_selection(**selection_arguments(native.latest['capability'], EPOCH)),
                       'R': lambda: native.client.request_turn(**args),
                       'L': lambda: native.client.request_map_left_click(**args)}
            methods[kind](); count = len(native.requests)
            for other, operation in methods.items():
                if other != kind:
                    with self.assertRaises(Unavailable): operation()
            if kind == 'L':
                for name, value in (('x', 161), ('y', 81), ('capability', 'a' * 32), ('control_epoch', 'b' * 32)):
                    with self.assertRaises(Unavailable): native.client.request_map_left_click(**dict(args, **{name: value}))
            self.assertEqual(len(native.requests), count)

    def test_stale_or_wrong_epoch_never_sends(self):
        native = self.fixture(); native.observe_tactical(); old = native.left_args()
        native.observe_tactical(); count = len(native.requests)
        for args in (old, dict(native.left_args(), control_epoch='a' * 32)):
            with self.assertRaises(Unavailable): native.client.request_map_left_click(**args)
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'READY')

    def test_shared_non_evicting_held_ledger_and_duplicate_at_capacity(self):
        native = self.fixture(); native.status = 'unavailable'; first = None
        for index in range(s.MAX_COMMANDS):
            ident = f'{index:032x}'
            if index % 4 in (0, 1):
                native.observe_tactical(); args = native.left_args(command_id=ident)
                if index == 0: first = args
                (native.client.request_map_left_click if index % 4 == 0 else native.client.request_turn)(**args)
            else:
                native.observe()
                if index % 4 == 2: native.client.request(**native.args(command_id=ident))
                else: native.client.request_selection(**selection_arguments(native.latest['capability'], EPOCH, ident))
        self.assertEqual(len(native.client.commands), 64); self.assertIn('0' * 32, native.client.commands)
        native.observe_tactical(); grant = native.client.capability
        native.client.request_map_left_click(**first)
        self.assertEqual(native.requests[-1], b'S' + b'0' * 32 + f'{native.client._query_sequence:032x}'.encode())
        self.assertEqual(native.client.capability, grant); self.assertEqual(native.client.state, 'READY')
        with self.assertRaises(Unavailable): native.client.request_map_left_click(**native.left_args(command_id='f' * 32))
        self.assert_closed_no_retry(native)
        self.assertEqual(sum(r[:1] in (b'D', b'N', b'R', b'L') for r in native.requests), 64)

    def test_malformed_initial_dispatched_or_extra_receipt_closes(self):
        for case_ordinal, response in enumerate((b'{}', b'\xff', b'x' * 257, dump(status(value='dispatched')),
                         dump(status(command_id='a' * 32)), dump(dict(status(), busy=True)),
                         [dump(status()), dump(status())]), 1):
            native = self.fixture(); native.observe_tactical()
            native.response = lambda raw, fixture, response=response: response
            with self.assertRaises(Unavailable, msg="receipt_case_" + str(case_ordinal)): native.client.request_map_left_click(**native.left_args())
            self.assert_closed_no_retry(native)
            self.assertEqual(sum(r[:1] == b'L' for r in native.requests), 1)

    def test_disconnect_or_action_timeout_never_resends(self):
        for mode in ('before', 'after', 'timeout'):
            native = self.fixture(); native.observe_tactical()
            if mode == 'before':
                native.peer.shutdown(socket.SHUT_RDWR); native.thread.join(1)
            elif mode == 'after': native.response = lambda raw, fixture: None
            else:
                def delayed(raw, fixture): time.sleep(0.4); return dump(status())
                native.response = delayed
            with self.assertRaises(Unavailable): native.client.request_map_left_click(**native.left_args())
            self.assert_closed_no_retry(native)
            self.assertEqual(sum(r[:1] == b'L' for r in native.requests), 0 if mode == 'before' else 1)

    def test_uncertain_status_or_terminal_regression_closes(self):
        for mode in ('malformed', 'disconnect', 'regression'):
            native = self.fixture(); native.observe_tactical(); native.client.request_map_left_click(**native.left_args())
            if mode == 'malformed': native.response = lambda raw, fixture: b'{}'
            elif mode == 'disconnect': native.response = lambda raw, fixture: None
            else:
                native.status = 'dispatched'; native.client.read_status(COMMAND); native.status = 'unavailable'
            with self.assertRaises(Unavailable): native.client.read_status(COMMAND)
            self.assert_closed_no_retry(native)
            self.assertEqual(sum(r[:1] == b'L' for r in native.requests), 1)

    def test_postdispatch_failed_or_disconnected_v_closes_without_resend(self):
        for response in (b'{}', None):
            native = self.fixture(); self.dispatch(native)
            native.response = lambda raw, fixture, response=response: response
            with self.assertRaises(Unavailable): native.observe_tactical()
            self.assert_closed_no_retry(native)
            self.assertEqual([r[:1] for r in native.requests], [b'V', b'L', b'S', b'V'])

    def test_postdispatch_deadline_timeout_closes_and_never_reobserves(self):
        native = self.fixture(); self.dispatch(native)
        def delayed(raw, fixture):
            time.sleep(0.08); return dump(tactical_context(raw[1:].decode(), 'b' * 32))
        native.response = delayed
        with patch.object(s, 'MAP_LEFT_CLICK_VIEW_TIMEOUT', 0.03), self.assertRaises(Unavailable):
            native.observe_tactical()
        self.assert_closed_no_retry(native)
        self.assertEqual([r[:1] for r in native.requests], [b'V', b'L', b'S', b'V'])

    def test_only_known_dispatched_l_selects_ten_second_budget_then_resets(self):
        native = self.fixture(); deadlines = []
        exchange = native.client.exchange
        def record(request, maximum, timeout=None):
            deadlines.append((request[:1], native.client.timeout if timeout is None else timeout))
            return exchange(request, maximum, timeout=timeout)
        with patch.object(native.client, 'exchange', side_effect=record):
            native.observe_tactical(); args = native.left_args()
            native.client.request_map_left_click(**args); native.client.request_map_left_click(**args)
            native.status = 'dispatched'; native.client.read_status(COMMAND)
            native.client.request_map_left_click(**args)
            native.observe_tactical(); native.client.request_map_left_click(**args)
            native.observe_tactical(); native.observe()
        self.assertEqual(s.TIMEOUT, 2.0); self.assertEqual(s.MAP_LEFT_CLICK_VIEW_TIMEOUT, 10.0)
        self.assertEqual(deadlines, [(b'V', .3), (b'L', .3), (b'S', .3), (b'S', .3),
                                     (b'S', .3), (b'V', 10.0), (b'S', .3), (b'V', .3), (b'O', .3)])

    def test_r_and_cancelled_l_observation_deadlines_stay_short(self):
        for action, result in (('R', 'dispatched'), ('L', 'unavailable')):
            native = self.fixture(); native.observe_tactical()
            method = native.client.request_turn if action == 'R' else native.client.request_map_left_click
            method(**native.left_args()); native.status = result; native.client.read_status(COMMAND)
            with patch.object(native.client, 'exchange', wraps=native.client.exchange) as exchange:
                native.observe_tactical()
            self.assertEqual(exchange.call_args.kwargs, {'timeout': .3})

    def test_old_dispatched_l_cannot_widen_later_r_deadline(self):
        native = self.fixture(); old = self.dispatch(native); native.observe_tactical()
        native.status = 'pending_confirmation'; native.client.request_turn(**native.turn_args(command_id='a' * 32))
        native.status = 'dispatched'; native.client.read_status('a' * 32)
        native.client.request_map_left_click(**old)
        with patch.object(native.client, 'exchange', wraps=native.client.exchange) as exchange:
            native.observe_tactical()
        self.assertEqual(exchange.call_args.kwargs, {'timeout': .3})

    def test_delayed_settled_frame_exceeding_normal_budget_is_accepted(self):
        native = self.fixture(); self.dispatch(native)
        def delayed(raw, fixture):
            time.sleep(0.4); return dump(tactical_context(raw[1:].decode(), 'b' * 32))
        native.response = delayed
        obj, rgb = native.observe_tactical()
        self.assertEqual(rgb, decoded_rgb(obj)); self.assertEqual(native.client.state, 'READY')
        self.assertFalse(native.client.tactical_verification_required)

    def test_concurrent_call_never_unlocks_or_sends(self):
        native = self.fixture(); native.observe_tactical(); native.client.flight.acquire()
        try:
            with self.assertRaises(Unavailable): native.client.request_map_left_click(**native.left_args())
            self.assertTrue(native.client.flight.locked()); self.assertEqual(len(native.requests), 1)
        finally: native.client.flight.release()
        native.client.request_map_left_click(**native.left_args())


class AdapterTests(unittest.TestCase):
    def test_default_tools_unchanged_and_all_dependencies_required(self):
        for kwargs in ({}, {'supervised_stance': True}, {'supervised_map_left_click': True},
                       {'supervised_stance': True, 'supervised_map_left_click': True},
                       {'supervised_turn': True, 'supervised_map_left_click': True},
                       {'supervised_stance': True, 'supervised_turn': True}):
            self.assertNotIn('request_map_left_click', [t['name'] for t in adapter.definitions(**kwargs)])
        self.assertEqual(adapter.definitions(), adapter.definitions(supervised_map_left_click=False))
        self.assertEqual(adapter.definitions(supervised_stance=True, supervised_turn=True),
                         adapter.definitions(supervised_stance=True, supervised_turn=True, supervised_map_left_click=False))
        for turn, left in ((False, True), (True, False), (True, 1), (True, '1'), (True, None)):
            client = type('Disabled', (), {'turn_enabled': turn, 'map_left_click_enabled': left})()
            server = adapter.Server(supervised_stance=client)
            self.assertIn('error', call(server, 'request_map_left_click', left_arguments()))
            listed = server.handle({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'})
            self.assertNotIn('request_map_left_click', [t['name'] for t in listed['result']['tools']])

    def test_exact_schema_and_plain_ordinary_left_click_description(self):
        definitions = adapter.definitions(supervised_stance=True, supervised_turn=True, supervised_map_left_click=True)
        self.assertEqual([t['name'] for t in definitions].count('read_tactical_context'), 1)
        tool = next(t for t in definitions if t['name'] == 'request_map_left_click')
        schema = tool['inputSchema']
        self.assertEqual(set(schema['properties']), set(left_arguments()))
        self.assertEqual(set(schema['required']), set(left_arguments())); self.assertFalse(schema['additionalProperties'])
        for name, minimum, maximum in (('x', 16, 303), ('y', 40, 127)):
            self.assertEqual(schema['properties'][name], {'type': 'integer', 'minimum': minimum, 'maximum': maximum})
        self.assertEqual(tool['annotations'], {'readOnlyHint': False, 'destructiveHint': True,
                                               'idempotentHint': True, 'openWorldHint': False})
        for phrase in ('ordinary left-click', 'may move, select', 'do nothing', 'time-unit costs',
                       'reservations', 'contacts', 'reaction fire', 'path-preview', 'never rewritten',
                       'read_stance_status', 'fresh tactical image'):
            self.assertIn(phrase, tool['description'])

    def test_invalid_arguments_never_reach_transport(self):
        def fail(*args, **kwargs): self.fail('Invalid arguments reached transport')
        client = type('NeverCall', (), {'turn_enabled': True, 'map_left_click_enabled': True,
                                      'request_map_left_click': fail})()
        for args in invalid_arguments():
            with self.subTest(args=args):
                self.assertEqual(call(adapter.Server(supervised_stance=client), 'request_map_left_click', args)['error']['code'], -32602)

    def test_shared_receipt_schema_has_no_timing_queue_or_busy_fields(self):
        for value in s.STATUSES:
            client = type('Enabled', (), {'turn_enabled': True, 'map_left_click_enabled': True,
                'request_map_left_click': lambda self, **args: status(value=value)})()
            reply = call(adapter.Server(supervised_stance=client), 'request_map_left_click', left_arguments())['result']
            expected = {'schema': s.PUBLIC_STATUS_SCHEMA, 'command_id': COMMAND, 'status': value,
                        'semantics': 'dispatch_status_not_success'}
            self.assertEqual(reply, {'content': [{'type': 'text', 'text': json.dumps(expected, separators=(',', ':'))}],
                                     'structuredContent': expected, 'isError': False})

    def test_failure_is_generic_only(self):
        def fail(*args, **kwargs): raise Unavailable('PRIVATE-SENTINEL-target')
        client = type('Failed', (), {'turn_enabled': True, 'map_left_click_enabled': True,
                                    'request_map_left_click': fail})()
        reply = call(adapter.Server(supervised_stance=client), 'request_map_left_click', left_arguments())['result']
        self.assertEqual(reply, {'content': [{'type': 'text', 'text': '{"error":"unavailable"}'}],
                                'structuredContent': {'error': 'unavailable'}, 'isError': True})


class LauncherTests(unittest.TestCase):
    def test_requires_both_flags_and_rejects_audio_only(self):
        for flags in (['--supervised-map-left-click'], ['--supervised-stance', '--supervised-map-left-click'],
                      ['--supervised-turn', '--supervised-map-left-click'],
                      ['--audio-only', '--supervised-stance', '--supervised-turn', '--supervised-map-left-click']):
            with self.subTest(flags=flags), patch.object(sys, 'argv', ['launcher', *flags, '--', 'game']), \
                    patch('launch_presentation.subprocess.Popen') as spawn, patch('sys.stderr', io.StringIO()), \
                    self.assertRaises(SystemExit): launch_presentation.main()
            spawn.assert_not_called()

    def test_ambient_scrubbed_and_explicit_literal_one_no_extra_channel(self):
        for flags, enabled, count in (([], False, 1), (['--audio-only'], False, 1),
                (['--supervised-stance'], False, 2), (['--supervised-stance', '--supervised-turn'], False, 2),
                (['--supervised-stance', '--supervised-turn', '--supervised-map-left-click'], True, 2)):
            for ambient in ('1', 'true', '99'):
                with self.subTest(flags=flags, ambient=ambient), \
                        patch.object(sys, 'argv', ['launcher', *flags, '--', 'synthetic-game']), \
                        patch.dict(os.environ, {'OPENXCOM_MAP_LEFT_CLICK_ENABLED': ambient}), \
                        patch('launch_presentation.subprocess.Popen') as spawn, patch('launch_presentation.serve') as serve:
                    launch_presentation.main(); env = spawn.call_args.kwargs['env']
                    self.assertEqual(env.get('OPENXCOM_MAP_LEFT_CLICK_ENABLED'), '1' if enabled else None)
                    self.assertEqual(len(set(spawn.call_args.kwargs['pass_fds'])), count)
                    client = serve.call_args.args[6]
                    if client is not None:
                        self.assertEqual(client.map_left_click_enabled, enabled); self.assertIsNone(client.channel)
                    spawn.return_value.kill.assert_not_called(); spawn.return_value.terminate.assert_not_called()

    def run_launcher(self, child, calls, enabled=True):
        flags = ['--supervised-stance', '--supervised-turn', '--supervised-selection']
        if enabled: flags.append('--supervised-map-left-click')
        messages = initialized_requests() + [{'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'}]
        messages += [tool_request(i, name, args) for i, (name, args) in enumerate(calls, 3)]
        result = subprocess.run([sys.executable, str(Path(__file__).with_name('launch_presentation.py')),
            *flags, '--', sys.executable, '-c', child], input=b''.join(dump(msg) + b'\n' for msg in messages),
            capture_output=True, timeout=16, env=dict(os.environ, OPENXCOM_MAP_LEFT_CLICK_ENABLED='1'))
        self.assertEqual(result.returncode, 0); self.assertEqual(result.stderr, b'')
        lines = result.stdout.splitlines()
        self.assertTrue(all(len(line) + 1 <= adapter.MAX_RESPONSE for line in lines))
        replies = [json.loads(line) for line in lines]
        self.assertEqual(len(replies), len(calls) + 2)
        self.assertEqual('request_map_left_click' in {t['name'] for t in replies[1]['result']['tools']}, enabled)
        return replies[2:]

    def test_real_launcher_l_r_o_n_d_interop_and_strict_arguments(self):
        child = SYNTHETIC_PRODUCER.replace('MODE_VALUE', repr('interop'))
        left = left_arguments(capability=f'{2:032x}', x=16, y=40)
        right = turn_arguments(capability=f'{3:032x}', command_id='4' * 32, x=16, y=40)
        selection = selection_arguments(capability=f'{4:032x}', command_id='5' * 32, direction='previous')
        stance = arguments(capability=f'{5:032x}', command_id='6' * 32)
        calls = [('read_tactical_context', {}), ('request_map_left_click', dict(left, x=16.0)),
                 ('request_map_left_click', dict(left, y=True)), ('request_map_left_click', dict(left, approved=True)),
                 ('request_map_left_click', left), ('read_tactical_context', {}), ('request_map_left_click', left),
                 ('read_stance_context', {}), ('request_turn', left), ('request_map_left_click', dict(left, x=17)),
                 ('read_tactical_context', {}), ('request_map_left_click', left), ('request_turn', right),
                 ('read_stance_status', {'command_id': '4' * 32}), ('read_stance_context', {}),
                 ('request_map_left_click', dict(left, capability=f'{4:032x}', command_id='5' * 32)),
                 ('request_selection', selection), ('read_stance_status', {'command_id': '5' * 32}),
                 ('read_stance_context', {}), ('request_stance', stance), ('read_stance_status', {'command_id': '6' * 32})]
        replies = self.run_launcher(child, calls)
        self.assertEqual([i for i, r in enumerate(replies) if 'error' in r], [1, 2, 3])
        self.assertEqual([i for i, r in enumerate(replies) if r.get('result', {}).get('isError')], [5, 7, 8, 9, 15])
        for index in (0, 10):
            self.assertEqual(decode_png(self, replies[index]['result']['content'][1])[1], decoded_rgb(tactical_context()))
        for index, value in ((4, 'pending_confirmation'), (6, 'dispatched'), (11, 'dispatched'),
                             (12, 'pending_confirmation'), (13, 'unavailable'), (16, 'pending_confirmation'),
                             (17, 'unavailable'), (19, 'pending_confirmation'), (20, 'unavailable')):
            self.assertEqual(replies[index]['result']['structuredContent']['status'], value)

    def test_real_launcher_left_delay_over_two_seconds_is_accepted(self):
        replies = self.run_launcher(SYNTHETIC_PRODUCER.replace('MODE_VALUE', repr('delay_l')),
            [('read_tactical_context', {}), ('request_map_left_click', left_arguments(capability=f'{2:032x}', x=16, y=40)),
             ('read_stance_status', {'command_id': COMMAND}), ('read_tactical_context', {})])
        self.assertFalse(replies[-1]['result']['isError'])
        self.assertEqual(replies[-1]['result']['structuredContent']['schema'], s.PUBLIC_TACTICAL_CONTEXT_SCHEMA)

    def test_real_launcher_right_delay_still_times_out(self):
        replies = self.run_launcher(SYNTHETIC_PRODUCER.replace('MODE_VALUE', repr('delay_r')),
            [('read_tactical_context', {}), ('request_turn', turn_arguments(capability=f'{2:032x}', x=16, y=40)),
             ('read_stance_status', {'command_id': COMMAND}), ('read_tactical_context', {}),
             ('request_turn', turn_arguments(capability=f'{2:032x}', x=16, y=40))])
        self.assertTrue(replies[-2]['result']['isError']); self.assertTrue(replies[-1]['result']['isError'])

    def test_real_launcher_disconnect_after_dispatch_fails_closed(self):
        left = left_arguments(capability=f'{2:032x}', x=16, y=40)
        replies = self.run_launcher(SYNTHETIC_PRODUCER.replace('MODE_VALUE', repr('disconnect')),
            [('read_tactical_context', {}), ('request_map_left_click', left), ('read_stance_status', {'command_id': COMMAND}),
             ('read_tactical_context', {}), ('request_map_left_click', left), ('read_stance_context', {})])
        self.assertEqual(replies[2]['result']['structuredContent']['status'], 'dispatched')
        self.assertTrue(all(r['result']['isError'] for r in replies[3:]))

    def test_real_launcher_ambient_flag_does_not_expose_tool(self):
        child = "import os; assert 'OPENXCOM_MAP_LEFT_CLICK_ENABLED' not in os.environ"
        replies = self.run_launcher(child, [('request_map_left_click', left_arguments())], enabled=False)
        self.assertEqual(replies[0]['error']['code'], -32602)


# The child validates expected packets using explicit checks even under python -O.
# Its exceptions are discarded by the real launcher; missing replies still fail tests.
SYNTHETIC_PRODUCER = r'''
import json, os, socket, time
mode = MODE_VALUE
def check(value):
    if not value: raise RuntimeError('Synthetic wire mismatch')
for fd in (1, 2): os.write(fd, b'PRIVATE-LEFT-CLICK-DIAGNOSTIC\xff\n')
check(os.environ['OPENXCOM_MAP_LEFT_CLICK_ENABLED'] == '1')
check(os.environ['OPENXCOM_TURN_ENABLED'] == '1')
channel = socket.socket(fileno=int(os.environ['OPENXCOM_STANCE_FD']))
def send(obj): channel.send(json.dumps(obj, separators=(',', ':')).encode())
epoch, counter, known, query_counter = '1' * 32, 1, {}, 0
send(dict(schema='experimental-stance/2', control_epoch=epoch))
expected = iter(['V', 'L', 'S', 'V', 'S', 'R', 'S', 'O', 'N', 'S', 'O', 'D', 'S']
    if mode == 'interop' else ['V', 'R' if mode == 'delay_r' else 'L', 'S', 'V'])
while True:
    raw = channel.recv(4096)
    if not raw: break
    kind = next(expected); check(raw[:1] == kind.encode())
    if kind in ('V', 'O'):
        check(len(raw) == 33)
        counter += 1
        if counter == 3 and mode == 'disconnect': channel.close(); break
        if counter == 3 and mode.startswith('delay'): time.sleep(2.3)
        obj = dict(schema='experimental-tactical-context/1' if kind == 'V' else 'experimental-stance-context/1',
            challenge=raw[1:].decode(), control_epoch=epoch, capability=f'{counter:032x}', semantics='presented_after_request')
        if kind == 'V': obj['view'] = dict(width=320, height=200, indices_hex='00' * 64000, palette_hex='0180ff' + '00' * 765)
        else: obj['panel'] = dict(width=320, height=56, rgb_hex='0180ff' * 320 * 56)
        try: send(obj)
        except BrokenPipeError: break
    else:
        if kind == 'S':
            query_counter += 1
            cid = raw[1:33].decode(); check(cid in known and raw == ('S' + cid + f'{query_counter:032x}').encode())
            value = 'dispatched' if cid == '3' * 32 else 'unavailable'
        else:
            cid = raw[65:97].decode(); check(cid not in known)
            payload = '016040' if kind in ('L', 'R') else ('p0' if kind == 'N' else 'sk')
            check(raw == (kind + epoch + f'{counter:032x}' + cid + payload).encode())
            known[cid] = kind; value = 'pending_confirmation'
        send(dict(schema='experimental-stance-status/2', command_id=cid, status=value, query_nonce=raw[33:65].decode() if kind == 'S' else None))
'''


if __name__ == '__main__': unittest.main()
