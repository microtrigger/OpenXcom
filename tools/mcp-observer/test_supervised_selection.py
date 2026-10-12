# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-12.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic shared-channel selection checks; no game, assets or model calls."""
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
from test_supervised_stance import (CAPABILITY, COMMAND, EPOCH, NativeFixture,
                                    arguments, call, dump, status)


def selection_arguments(capability=CAPABILITY, control_epoch=EPOCH,
                        command_id=COMMAND, direction='next'):
    return dict(capability=capability, control_epoch=control_epoch,
                command_id=command_id, direction=direction)


class ValidationTests(unittest.TestCase):
    def test_only_exact_next_previous_arguments(self):
        for direction in ('next', 'previous'):
            s.validate_selection_arguments(selection_arguments(direction=direction))
        invalid = [None, [], False, {}, arguments()]
        for name in selection_arguments():
            obj = selection_arguments(); del obj[name]; invalid.append(obj)
        for name in ('approved', 'human_approved', 'force', 'unit_id', 'target', 'x', 'y', 'desired'):
            invalid.append(dict(selection_arguments(), **{name: True}))
        for value in (None, True, False, 1, [], {}, 'n', 'p', 'NEXT', 'prev', 'nearest', ''):
            invalid.append(selection_arguments(direction=value))
        for name in ('capability', 'control_epoch', 'command_id'):
            for value in (None, True, 1, [], {}, 'a' * 31, 'A' * 32, 'z' * 32):
                invalid.append(dict(selection_arguments(), **{name: value}))
        for obj in invalid:
            with self.subTest(obj=obj), self.assertRaises(Unavailable):
                s.validate_selection_arguments(obj)

    def test_selection_opt_in_is_strict_boolean(self):
        for value in (None, 0, 1, '1', 'true', [], {}):
            with self.subTest(value=value), self.assertRaises(Unavailable):
                s.StanceClient(None, selection_enabled=value)


class ClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(selection_enabled=kwargs.pop('selection_enabled', True), **kwargs)
        self.addCleanup(native.close)
        return native

    def args(self, native, command_id=COMMAND, direction='next'):
        return selection_arguments(native.latest['capability'], native.latest['control_epoch'],
                                   command_id, direction)

    def assert_closed_without_retry(self, native, args):
        self.assertEqual(native.client.state, 'CLOSED')
        self.assertIsNone(native.client.channel)
        self.assertIsNone(native.client.capability)
        count = len(native.requests)
        for operation in (native.observe, lambda: native.client.request_selection(**args),
                          lambda: native.client.request(**arguments()),
                          lambda: native.client.read_status(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count)

    def test_disabled_selection_never_sends_or_consumes_context(self):
        native = self.fixture(selection_enabled=False); native.observe()
        with self.assertRaises(Unavailable): native.client.request_selection(**self.args(native))
        self.assertEqual([r[:1] for r in native.requests], [b'O'])
        self.assertEqual(native.client.state, 'READY')
        native.client.request(**native.args())
        self.assertEqual([r[:1] for r in native.requests], [b'O', b'D'])

    def test_exact_99_byte_wire_for_both_directions_and_shared_status(self):
        native = self.fixture()
        for index, direction in enumerate(('next', 'previous')):
            native.observe(); args = self.args(native, command_id=f'{index:032x}', direction=direction)
            self.assertEqual(native.client.request_selection(**args)['status'], 'pending_confirmation')
            expected = ('N' + EPOCH + args['capability'] + args['command_id']
                        + s.DIRECTIONS[direction] + '0').encode()
            self.assertEqual(native.requests[-1], expected)
            self.assertEqual(len(expected), 99)
            self.assertIsNone(native.client.capability)
            self.assertEqual(native.client.state, 'PENDING')
            native.status = 'dispatched'
            self.assertEqual(native.client.read_status(args['command_id'])['status'], 'dispatched')
            self.assertEqual(native.requests[-1], b'S' + args['command_id'].encode())
            self.assertEqual(native.client.state, 'NEED_CONTEXT')
            native.status = 'pending_confirmation'

    def test_exact_duplicate_queries_status_never_replays_selection(self):
        native = self.fixture(); native.observe(); args = self.args(native)
        native.client.request_selection(**args)
        for value in ('pending_confirmation', 'dispatched', 'dispatched'):
            native.status = value
            self.assertEqual(native.client.request_selection(**args)['status'], value)
            self.assertEqual(native.requests[-1], b'S' + COMMAND.encode())
        self.assertEqual([r[:1] for r in native.requests], [b'O', b'N', b'S', b'S', b'S'])
        native.observe()
        capability = native.client.capability
        self.assertEqual(native.client.request_selection(**args)['status'], 'dispatched')
        self.assertEqual(native.client.state, 'READY')
        self.assertEqual(native.client.capability, capability)

    def test_conflicting_id_fields_never_query_or_send(self):
        native = self.fixture(); native.observe(); args = self.args(native)
        native.client.request_selection(**args)
        count = len(native.requests)
        for name, value in (('direction', 'previous'), ('capability', 'a' * 32),
                            ('control_epoch', 'b' * 32)):
            with self.subTest(name=name), self.assertRaises(Unavailable):
                native.client.request_selection(**dict(args, **{name: value}))
        self.assertEqual(len(native.requests), count)
        self.assertEqual(native.client.state, 'PENDING')

    def test_cross_action_command_id_collision_in_both_orders(self):
        for first in ('stance', 'selection'):
            with self.subTest(first=first):
                native = self.fixture(); native.observe()
                stance_args, selection_args = native.args(), self.args(native)
                if first == 'stance':
                    native.client.request(**stance_args)
                    conflicting = lambda: native.client.request_selection(**selection_args)
                else:
                    native.client.request_selection(**selection_args)
                    conflicting = lambda: native.client.request(**stance_args)
                count = len(native.requests)
                with self.assertRaises(Unavailable): conflicting()
                self.assertEqual(len(native.requests), count)
                native.status = 'dispatched'; native.client.read_status(COMMAND); native.observe()
                count = len(native.requests)
                with self.assertRaises(Unavailable): conflicting()
                self.assertEqual(len(native.requests), count)
                self.assertEqual(native.client.state, 'READY')

    def test_shared_one_use_context_blocks_other_action_pending_and_terminal(self):
        for first in ('stance', 'selection'):
            with self.subTest(first=first):
                native = self.fixture(); native.observe()
                if first == 'stance':
                    native.client.request(**native.args())
                    next_args = self.args(native, command_id='a' * 32)
                    next_action = native.client.request_selection
                else:
                    native.client.request_selection(**self.args(native))
                    next_args = native.args(command_id='a' * 32)
                    next_action = native.client.request
                count = len(native.requests)
                with self.assertRaises(Unavailable): native.observe()
                with self.assertRaises(Unavailable): next_action(**next_args)
                self.assertEqual(len(native.requests), count)
                native.status = 'dispatched'; native.client.read_status(COMMAND)
                count = len(native.requests)
                with self.assertRaises(Unavailable): next_action(**next_args)
                self.assertEqual(len(native.requests), count)
                native.observe()
                count = len(native.requests)
                with self.assertRaises(Unavailable): next_action(**next_args)
                self.assertEqual(len(native.requests), count)
                next_args['capability'] = native.latest['capability']
                native.status = 'pending_confirmation'
                next_action(**next_args)
                self.assertEqual([r[:1] for r in native.requests],
                                 [b'O', b'D' if first == 'stance' else b'N', b'S', b'O',
                                  b'N' if first == 'stance' else b'D'])

    def test_replaced_context_and_wrong_epoch_fail_without_send(self):
        native = self.fixture(); native.observe(); old = self.args(native); native.observe()
        count = len(native.requests)
        for args in (old, dict(self.args(native), control_epoch='f' * 32)):
            with self.assertRaises(Unavailable): native.client.request_selection(**args)
        self.assertEqual(len(native.requests), count)
        self.assertEqual(native.client.state, 'READY')

    def test_immediate_unavailable_needs_new_context(self):
        native = self.fixture(); native.observe(); args = self.args(native)
        native.status = 'unavailable'
        self.assertEqual(native.client.request_selection(**args)['status'], 'unavailable')
        self.assertEqual(native.client.state, 'NEED_CONTEXT')
        self.assertEqual(native.client.request_selection(**args)['status'], 'unavailable')
        self.assertEqual(sum(r[:1] == b'N' for r in native.requests), 1)
        with self.assertRaises(Unavailable):
            native.client.request_selection(**dict(args, command_id='a' * 32))

    def test_shared_bounded_non_evicting_ledger(self):
        native = self.fixture()
        for index in range(s.MAX_COMMANDS):
            native.observe(); native.status = 'unavailable'
            if index % 2:
                native.client.request(**native.args(command_id=f'{index:032x}'))
            else:
                native.client.request_selection(**self.args(native, command_id=f'{index:032x}'))
        self.assertEqual(len(native.client.commands), s.MAX_COMMANDS)
        self.assertIn('0' * 32, native.client.commands)
        native.observe(); args = self.args(native, command_id='f' * 32)
        with self.assertRaises(Unavailable): native.client.request_selection(**args)
        self.assert_closed_without_retry(native, args)
        self.assertEqual(sum(r[:1] in (b'D', b'N') for r in native.requests), s.MAX_COMMANDS)

    def test_malformed_or_initial_dispatched_closes_both_actions(self):
        replies = (b'{}', b'\xff', b'x' * 257, dump(status(value='dispatched')),
                   dump(status(command_id='a' * 32)), dump(dict(status(), approved=True)))
        for reply in replies:
            with self.subTest(reply=reply[:30]):
                native = self.fixture(); native.observe(); args = self.args(native)
                native.response = lambda raw, fixture, reply=reply: reply
                with self.assertRaises(Unavailable): native.client.request_selection(**args)
                self.assert_closed_without_retry(native, args)
                self.assertEqual([r[:1] for r in native.requests], [b'O', b'N'])

    def test_disconnect_before_or_after_send_never_retries(self):
        for before in (True, False):
            with self.subTest(before=before):
                native = self.fixture(); native.observe(); args = self.args(native)
                if before:
                    native.peer.shutdown(socket.SHUT_RDWR); native.thread.join(1)
                else:
                    native.response = lambda raw, fixture: None
                with self.assertRaises(Unavailable): native.client.request_selection(**args)
                self.assert_closed_without_retry(native, args)
                self.assertEqual(sum(r[:1] == b'N' for r in native.requests), 0 if before else 1)

    def test_timeout_after_send_consumes_capability_and_never_retries(self):
        native = self.fixture(); native.observe(); args = self.args(native)
        def timeout(raw, fixture):
            time.sleep(0.45)
            return dump(status())
        native.response = timeout
        with self.assertRaises(Unavailable): native.client.request_selection(**args)
        self.assert_closed_without_retry(native, args)
        self.assertEqual(sum(r[:1] == b'N' for r in native.requests), 1)

    def test_uncertain_shared_status_never_replays_selection(self):
        native = self.fixture(); native.observe(); args = self.args(native)
        native.client.request_selection(**args)
        native.response = lambda raw, fixture: None
        with self.assertRaises(Unavailable): native.client.read_status(COMMAND)
        self.assert_closed_without_retry(native, args)
        self.assertEqual([r[:1] for r in native.requests], [b'O', b'N', b'S'])

    def test_concurrent_selection_does_not_send_or_unlock_other_call(self):
        native = self.fixture(); native.observe()
        native.client.flight.acquire()
        try:
            with self.assertRaises(Unavailable): native.client.request_selection(**self.args(native))
            self.assertEqual([r[:1] for r in native.requests], [b'O'])
            self.assertTrue(native.client.flight.locked())
        finally:
            native.client.flight.release()
        native.client.request_selection(**self.args(native))


class AdapterTests(unittest.TestCase):
    def test_selection_hidden_without_explicit_enabled_client(self):
        for kwargs in ({}, {'supervised_stance': True}, {'supervised_selection': True}):
            self.assertNotIn('request_selection', [t['name'] for t in adapter.definitions(**kwargs)])
        for enabled in (None, False, 1, '1'):
            client = type('DisabledClient', (), {'selection_enabled': enabled})()
            server = adapter.Server(supervised_stance=client)
            self.assertIn('error', call(server, 'request_selection', selection_arguments()))
            result = server.handle({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'})
            self.assertNotIn('request_selection', [t['name'] for t in result['result']['tools']])
        self.assertIn('error', call(adapter.Server(), 'request_selection', selection_arguments()))

    def test_selection_definition_has_no_approval_or_target_parameters(self):
        definitions = adapter.definitions(supervised_stance=True, supervised_selection=True)
        names = {t['name'] for t in definitions}
        self.assertEqual(names - set(adapter.NAMES),
                         {'read_stance_context', 'request_stance', 'read_stance_status', 'request_selection'})
        request = next(t for t in definitions if t['name'] == 'request_selection')
        self.assertFalse(request['annotations']['readOnlyHint'])
        self.assertTrue(request['annotations']['destructiveHint'])
        self.assertTrue(request['annotations']['idempotentHint'])
        self.assertFalse(request['inputSchema']['additionalProperties'])
        self.assertEqual(set(request['inputSchema']['required']), set(selection_arguments()))
        self.assertEqual(set(request['inputSchema']['properties']), set(selection_arguments()))
        self.assertEqual(request['inputSchema']['properties']['direction']['enum'], ['next', 'previous'])
        self.assertIn('read_stance_status', request['description'])

    def test_selection_and_shared_status_keep_unchanged_public_receipt(self):
        for value in s.STATUSES:
            client = type('EnabledClient', (), {
                'selection_enabled': True,
                'request_selection': lambda self, **args: status(value=value),
                'read_status': lambda self, **args: status(value=value)})()
            server = adapter.Server(supervised_stance=client)
            for name, args in (('request_selection', selection_arguments()),
                               ('read_stance_status', {'command_id': COMMAND})):
                reply = call(server, name, args)['result']
                self.assertFalse(reply['isError'])
                self.assertEqual(reply['structuredContent'], {
                    'schema': s.PUBLIC_STATUS_SCHEMA, 'command_id': COMMAND, 'status': value,
                    'semantics': 'dispatch_status_not_success'})
                self.assertEqual(json.loads(reply['content'][0]['text']), reply['structuredContent'])

    def test_invalid_arguments_never_reach_client(self):
        def fail(*args, **kwargs): self.fail('Invalid arguments reached transport')
        client = type('NeverCall', (), {'selection_enabled': True, 'request_selection': fail})()
        server = adapter.Server(supervised_stance=client)
        invalid = (None, [], False, {}, arguments(), dict(selection_arguments(), approved=True),
                   selection_arguments(direction='p'), selection_arguments(command_id='bad'))
        for args in invalid:
            with self.subTest(args=args):
                self.assertEqual(call(server, 'request_selection', args)['error']['code'], -32602)

    def test_transport_failure_has_only_generic_unavailable(self):
        def fail(*args, **kwargs): raise Unavailable('PRIVATE-SENTINEL-target')
        client = type('FailedClient', (), {'selection_enabled': True, 'request_selection': fail})()
        reply = call(adapter.Server(supervised_stance=client), 'request_selection', selection_arguments())['result']
        self.assertTrue(reply['isError'])
        self.assertEqual(reply['structuredContent'], {'error': 'unavailable'})
        self.assertEqual(reply['content'], [{'type': 'text', 'text': '{"error":"unavailable"}'}])
        self.assertNotIn('PRIVATE', dump(reply).decode())


class LauncherTests(unittest.TestCase):
    def test_selection_flag_requires_stance_and_rejects_audio_only(self):
        for flags in (['--supervised-selection'], ['--audio-only', '--supervised-selection'],
                      ['--audio-only', '--supervised-stance', '--supervised-selection']):
            with self.subTest(flags=flags), \
                    patch.object(sys, 'argv', ['launcher', *flags, '--', 'game']), \
                    patch('launch_presentation.subprocess.Popen') as spawn, \
                    patch('sys.stderr', io.StringIO()), self.assertRaises(SystemExit):
                launch_presentation.main()
            spawn.assert_not_called()

    def test_selection_only_explicitly_sets_env_and_uses_shared_descriptor(self):
        cases = (([], False, False, 1), (['--audio-only'], False, False, 1),
                 (['--supervised-stance'], True, False, 2),
                 (['--supervised-stance', '--supervised-selection'], True, True, 2),
                 (['--supervised-stance', '--supervised-selection', '--presented-hud', '--mixed-audio'],
                  True, True, 3))
        for flags, stance, selection, count in cases:
            with self.subTest(flags=flags), \
                    patch.object(sys, 'argv', ['launcher', *flags, '--', 'synthetic-game']), \
                    patch.dict(os.environ, {'OPENXCOM_STANCE_FD': '789', 'OPENXCOM_SELECTION_ENABLED': '1'}), \
                    patch('launch_presentation.subprocess.Popen') as spawn, \
                    patch('launch_presentation.serve') as serve:
                launch_presentation.main()
                env = spawn.call_args.kwargs['env']
                self.assertEqual('OPENXCOM_STANCE_FD' in env, stance)
                self.assertEqual('OPENXCOM_SELECTION_ENABLED' in env, selection)
                if selection: self.assertEqual(env['OPENXCOM_SELECTION_ENABLED'], '1')
                self.assertEqual(len(set(spawn.call_args.kwargs['pass_fds'])), count)
                client = serve.call_args.args[6]
                self.assertEqual(client is not None, stance)
                if stance:
                    self.assertEqual(client.selection_enabled, selection)
                    self.assertIsNone(client.channel)
                spawn.return_value.kill.assert_not_called()
                spawn.return_value.terminate.assert_not_called()

    def test_real_launcher_shared_channel_selection_and_duplicate_receipts(self):
        child = r'''
import json, os, socket
assert os.environ['OPENXCOM_SELECTION_ENABLED'] == '1'
channel = socket.socket(fileno=int(os.environ['OPENXCOM_STANCE_FD']))
def send(obj): channel.send(json.dumps(obj, separators=(',', ':')).encode())
epoch, command, counter, selections = '1' * 32, '3' * 32, 1, 0
send(dict(schema='experimental-stance/1', control_epoch=epoch))
while True:
    raw = channel.recv(4096)
    if not raw: break
    if raw[:1] == b'O':
        counter += 1
        send(dict(schema='experimental-stance-context/1', challenge=raw[1:].decode(),
                  control_epoch=epoch, capability=f'{counter:032x}', semantics='presented_after_request',
                  panel=dict(width=320, height=56, rgb_hex='0180ff' * 320 * 56)))
    elif raw[:1] == b'N':
        assert raw == ('N' + epoch + f'{2:032x}' + command + 'p0').encode()
        selections += 1
        assert selections == 1
        send(dict(schema='experimental-stance-status/1', command_id=command, status='pending_confirmation'))
    else:
        assert raw == ('S' + command).encode() and selections == 1
        send(dict(schema='experimental-stance-status/1', command_id=command, status='dispatched'))
'''
        initialize = {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {
            'protocolVersion': adapter.PROTOCOL, 'capabilities': {},
            'clientInfo': {'name': 'synthetic', 'version': '1'}}}
        requests = [initialize, {'jsonrpc': '2.0', 'method': 'notifications/initialized'},
                    {'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'}]
        calls = [('read_stance_context', {}),
                 ('request_selection', selection_arguments(capability=f'{2:032x}', direction='previous')),
                 ('request_selection', selection_arguments(capability=f'{2:032x}', direction='previous')),
                 ('read_stance_status', {'command_id': COMMAND}), ('read_stance_context', {})]
        for ident, (name, args) in enumerate(calls, 3):
            requests.append({'jsonrpc': '2.0', 'id': ident, 'method': 'tools/call',
                             'params': {'name': name, 'arguments': args}})
        result = subprocess.run([sys.executable, str(Path(__file__).with_name('launch_presentation.py')),
                                 '--supervised-stance', '--supervised-selection', '--',
                                 sys.executable, '-c', child],
                                input=b''.join(dump(request) + b'\n' for request in requests),
                                capture_output=True, timeout=8)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stderr, b'')
        replies = [json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual(len(replies), 7)
        self.assertIn('request_selection', [t['name'] for t in replies[1]['result']['tools']])
        self.assertTrue(all(not reply['result']['isError'] for reply in replies[2:]))
        self.assertEqual([reply['result']['structuredContent']['status'] for reply in replies[3:6]],
                         ['pending_confirmation', 'dispatched', 'dispatched'])
        self.assertNotEqual(replies[2]['result']['structuredContent']['capability'],
                            replies[6]['result']['structuredContent']['capability'])


if __name__ == '__main__':
    unittest.main()
