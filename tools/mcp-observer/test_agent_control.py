# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Synthetic semantic-control checks: no GUI, game, assets or model calls."""
import copy
import io
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import threading
import unittest
from unittest.mock import patch
import adapter
import agent_control as a
import launch_presentation
from snapshots import Unavailable

EPOCH, CAPABILITY, COMMAND, CHALLENGE = ('1' * 32, '2' * 32, '3' * 32, '4' * 32)


def dump(obj):
    return json.dumps(obj, separators=(',', ':')).encode('utf-8')


def state(challenge=CHALLENGE, capability=CAPABILITY, revision=1, event_sequence=1, screen='battlescape'):
    visible = dict(selected_name='Jane', time_units=58, energy=62, health=33, morale=100, kneel_indicator='standing')
    ids = ['next_soldier', 'previous_soldier', 'kneel']
    if screen in a.STATE_FIELDS:
        visible = {key: key.title() for key in a.STATE_FIELDS[screen]}; ids = sorted(a.STATE_ACTIONS[screen])
    elif screen == 'unsupported':
        visible, ids, capability = {}, [], None
    return dict(schema=a.SCHEMA, mode=a.MODE, type='state', challenge=challenge, epoch=EPOCH,
                revision=revision, capability=capability, state=screen, visible=visible,
                actions=[dict(id=action, label=action.replace('_', ' ').title(),
                              label_source='visible_control_semantics' if screen in ('battlescape', 'inventory') else 'visible_text') for action in ids],
                event_sequence=event_sequence, semantics='presented_after_request')


def arguments(obj=None, command_id=COMMAND, action='kneel'):
    obj = state() if obj is None else obj
    return {**{k: obj[k] for k in ('epoch', 'capability', 'revision')}, 'command_id': command_id, 'action': action}


def result(command_id=COMMAND, status='dispatched', event_sequence=2, observation=None):
    return dict(schema=a.SCHEMA, mode=a.MODE, type='result', command_id=command_id, status=status,
                event_sequence=event_sequence, observation=observation, semantics='dispatch_not_success')


def call(server, name, args=None):
    server.state = 'ready'
    return server.handle(dict(jsonrpc='2.0', id=1, method='tools/call', params=dict(name=name, arguments={} if args is None else args)))


class NativeFixture:
    def __init__(self, *, hello=None, transform=None, timeout=0.15, screen='battlescape'):
        parent, self.peer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.client = a.AgentControlClient(parent, timeout=timeout, action_timeout=timeout, startup_timeout=timeout)
        self.peer.settimeout(1)
        self.requests, self.results, self.errors = [], {}, []
        self.transform, self.screen = transform, screen
        self.revision, self.sequence, self.counter = 1, 0, 10
        self.latest = None
        self.hello = hello or dict(schema=a.SCHEMA, mode=a.MODE, epoch=EPOCH, ledger_capacity=64)
        self.thread = threading.Thread(target=self.run); self.thread.start()

    def run(self):
        try:
            self.peer.send(dump(self.hello))
            while True:
                raw = self.peer.recv(4096)
                if not raw: return
                self.requests.append(raw); command = raw.decode().split(' ')
                if command == ['H', a.SCHEMA]:
                    obj = dict(schema=a.SCHEMA, mode=a.MODE, status='ready')
                elif command[0] == 'R' and len(command) == 2:
                    self.counter += 1; self.sequence += 1
                    obj = state(command[1], f'{self.counter:032x}', self.revision, self.sequence, self.screen)
                    self.latest = copy.deepcopy(obj)
                elif command[0] == 'E' and len(command) == 6:
                    _, ident, epoch, cap, revision, action = command
                    assert epoch == EPOCH and cap == self.latest['capability']
                    assert int(revision) == self.revision and action in [i['id'] for i in self.latest['actions']]
                    assert ident not in self.results, 'Native action unexpectedly replayed'
                    self.revision += 1; self.sequence += 1
                    obj = result(ident, event_sequence=self.sequence,
                                 observation=state(None, None, self.revision, self.sequence, self.screen))
                    self.results[ident] = copy.deepcopy(obj)
                elif command[0] == 'S' and len(command) == 2:
                    obj = copy.deepcopy(self.results[command[1]])
                else:
                    raise AssertionError('Unexpected native request')
                if self.transform is not None: obj = self.transform(command[0], obj)
                if obj is not None: self.peer.send(obj if isinstance(obj, bytes) else dump(obj))
        except (BrokenPipeError, ConnectionResetError):
            pass
        except BaseException as error:
            self.errors.append(error)

    def close(self):
        self.client.close(); self.thread.join(2); self.peer.close()
        assert not self.thread.is_alive()
        if self.errors: raise self.errors[0]


class ValidationTests(unittest.TestCase):
    def test_exact_hello_and_version_identity(self):
        hello = dict(schema=a.SCHEMA, mode=a.MODE, epoch=EPOCH, ledger_capacity=64)
        self.assertEqual(a.validate_hello(dump(hello)), EPOCH)
        for key, value in (('schema', 'experimental-stance/1'), ('mode', 'supervised'), ('epoch', 'A' * 32),
                           ('ledger_capacity', 64.0), ('ledger_capacity', True), ('extra', 'hidden')):
            with self.subTest(key=key, value=value), self.assertRaises(Unavailable):
                a.validate_hello(dump(dict(hello, **{key: value})))

    def test_duplicate_keys_nonfinite_nonjson_and_oversize(self):
        for raw in (b'{"schema":1,"schema":2}', b'{"a":NaN}', b'\xff', b'', b'x' * (a.MAX_PACKET + 1)):
            with self.subTest(raw=raw[:30]), self.assertRaises(Unavailable): a.parse(raw)

    def test_all_supported_shapes_and_nullable_visible_text(self):
        for screen in a.STATE_ACTIONS:
            obj = state(screen=screen)
            if screen in a.STATE_FIELDS: obj['visible'] = dict.fromkeys(a.STATE_FIELDS[screen])
            a.validate_state(obj, CHALLENGE, EPOCH)
        obj = state(); obj['visible'] = dict.fromkeys(obj['visible']); a.validate_state(obj, CHALLENGE, EPOCH)

    def test_state_rejects_unknown_hidden_or_malformed_fields(self):
        cases = []
        for key, value in (('schema', 'other'), ('mode', 'supervised'), ('challenge', '5' * 32), ('epoch', '5' * 32),
                           ('state', 'hidden_world'), ('revision', True), ('revision', -1), ('revision', 2**53),
                           ('event_sequence', 1.0), ('capability', 'BAD'), ('hidden_unit_id', 123), ('semantics', 'world_state')):
            obj = state(); obj[key] = value; cases.append(obj)
        for key, value in (('hidden_unit_id', 123), ('time_units', True), ('health', -1), ('morale', 100.0),
                           ('selected_name', 'x' * 4097), ('kneel_indicator', 'unknown')):
            obj = state(); obj['visible'][key] = value; cases.append(obj)
        for key, value in (('target', 123), ('id', 'attack_any_enemy'), ('label_source', 'hidden_rule')):
            obj = state(); obj['actions'][0][key] = value; cases.append(obj)
        obj = state(); obj['actions'].append(copy.deepcopy(obj['actions'][0])); cases.append(obj)
        obj = state(); obj['actions'].append(dict(id='stand', label='Stand', label_source='visible_control_semantics')); cases.append(obj)
        obj = state(screen='unsupported'); obj['capability'] = CAPABILITY; cases.append(obj)
        obj = state(screen='next_turn'); obj['actions'][0]['id'] = 'kneel'; cases.append(obj)
        obj = state(); obj['actions'] = []; cases.append(obj)
        obj = state(); del obj['visible']; cases.append(obj)
        for obj in cases:
            with self.subTest(obj=obj), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_exact_action_args_and_allowlist(self):
        for action in a.ACTIONS: a.validate_action_arguments(arguments(action=action))
        cases = [None, [], {}, False]
        for name in arguments():
            obj = arguments(); del obj[name]; cases.append(obj)
        for key, value in (('approved', True), ('desired', 'kneel'), ('x', 12), ('unit_id', 4),
                           ('action', 'toggle_kneel'), ('action', 'attack'), ('action', True), ('epoch', 'A' * 32),
                           ('capability', 'z' * 32), ('revision', True), ('revision', 1.0), ('revision', -1), ('command_id', 'short')):
            cases.append(dict(arguments(), **{key: value}))
        for args in cases:
            with self.subTest(args=args), self.assertRaises(Unavailable): a.validate_action_arguments(args)

    def test_inventory_action_is_visible_icon_semantics(self):
        obj = state(screen='inventory')
        self.assertEqual(obj['actions'][0]['label_source'], 'visible_control_semantics')
        a.validate_state(obj, CHALLENGE, EPOCH)
        obj['actions'][0]['label_source'] = 'visible_text'
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_unicode_expansion_is_bounded_for_mcp_output(self):
        obj = state(screen='new_battle')
        for name in obj['visible']: obj['visible'][name] = '\U0001f600' * 1000
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_result_separates_dispatch_from_observation(self):
        for status in a.STATUSES: a.validate_result(dump(result(status=status)), COMMAND, EPOCH)
        observation = state(None, None, revision=2, event_sequence=2)
        a.validate_result(dump(result(observation=observation)), COMMAND, EPOCH)
        for change in (dict(semantics='success'), dict(command_id='7' * 32), dict(status='succeeded'), dict(approved=True),
                       dict(observation=state(None, CAPABILITY, revision=2, event_sequence=2)),
                       dict(observation=observation, status='unknown_after_dispatch')):
            with self.subTest(change=change), self.assertRaises(Unavailable):
                a.validate_result(dump(dict(result(), **change)), COMMAND, EPOCH)


class ClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs); self.addCleanup(native.close); return native

    def test_only_anonymous_seqpacket_allowed(self):
        for kind in (socket.SOCK_STREAM, socket.SOCK_DGRAM):
            left, right = socket.socketpair(socket.AF_UNIX, kind)
            with left, right, self.assertRaises(Unavailable): a.AgentControlClient(left)
        # The sandbox permits anonymous socketpair, not opening a listener.
        # Simulate a named endpoint on a real socket without creating a service.
        class NamedSocket(socket.socket):
            def getsockname(self): return '/not-an-anonymous-endpoint'
        left, right = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        named = NamedSocket(fileno=left.detach())
        with named, right, self.assertRaises(Unavailable): a.AgentControlClient(named)

    def test_timeout_configuration_strict(self):
        for value in (True, False, 0, -1, float('inf'), float('nan'), 10, '1'):
            with self.subTest(value=value), self.assertRaises(Unavailable): a.AgentControlClient(None, timeout=value)

    def test_explicit_handshake_then_challenge_bound_state(self):
        native = self.fixture(); obj = native.client.observe()
        self.assertEqual(native.requests[0], b'H openxcom-agent-control/1')
        self.assertEqual(native.requests[1], ('R ' + obj['challenge']).encode())
        self.assertEqual(native.client.state, 'READY'); self.assertEqual(obj['visible']['selected_name'], 'Jane')

    def test_mismatched_native_identity_closes_before_any_request(self):
        for key, value in (('schema', 'experimental-stance/1'), ('mode', 'supervised')):
            hello = dict(schema=a.SCHEMA, mode=a.MODE, epoch=EPOCH, ledger_capacity=64); hello[key] = value
            native = self.fixture(hello=hello)
            with self.assertRaises(Unavailable): native.client.observe()
            self.assertEqual(native.client.state, 'CLOSED'); self.assertEqual(native.requests, [])

    def test_mismatched_ready_closes_before_read(self):
        native = self.fixture(transform=lambda op, obj: dict(obj, mode='supervised') if op == 'H' else obj)
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.requests, [b'H openxcom-agent-control/1']); self.assertEqual(native.client.state, 'CLOSED')

    def test_new_read_invalidates_old_capability(self):
        native = self.fixture(); old = native.client.observe(); fresh = native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(old))
        self.assertEqual(len(native.requests), count); self.assertNotEqual(old['capability'], fresh['capability'])
        native.client.execute(**arguments(fresh))

    def test_capability_allows_reasoning_time_but_remains_bounded(self):
        native = self.fixture(); obj = native.client.observe()
        remaining = native.client.capability_deadline - a.time.monotonic()
        self.assertGreater(remaining, 119.0)
        self.assertLessEqual(remaining, 120.0)
        # Ten seconds of elapsed reasoning must not recreate the old five-second
        # limit. Native still validates its own TTL and exact displayed binding.
        native.client.capability_deadline -= 10.0
        native.client.execute(**arguments(obj))

    def test_wrong_epoch_revision_action_or_expired_grant_never_sends(self):
        native = self.fixture(); args = arguments(native.client.observe()); count = len(native.requests)
        for changes in (dict(epoch='f' * 32), dict(revision=0), dict(capability='f' * 32), dict(action='stand'), dict(action='continue_next_turn')):
            with self.subTest(changes=changes), self.assertRaises(Unavailable): native.client.execute(**dict(args, **changes))
        native.client.capability_deadline = 0
        with self.assertRaises(Unavailable): native.client.execute(**args)
        self.assertEqual(len(native.requests), count)

    def test_unsupported_state_is_explicit_and_never_grants_control(self):
        native = self.fixture(screen='unsupported'); obj = native.client.observe()
        self.assertEqual(obj['state'], 'unsupported'); self.assertEqual(obj['actions'], [])
        self.assertIsNone(obj['capability']); self.assertEqual(native.client.state, 'NEED_CONTEXT')
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments())
        self.assertEqual(len(native.requests), count)
        native.screen = 'next_turn'; obj = native.client.observe(); native.client.execute(**arguments(obj, action='continue_next_turn'))

    def test_exact_wire_and_post_action_state_never_rearms(self):
        native = self.fixture(); obj = native.client.observe(); args = arguments(obj); receipt = native.client.execute(**args)
        self.assertEqual(native.requests[-1], f'E {COMMAND} {EPOCH} {obj["capability"]} 1 kneel'.encode())
        self.assertEqual(receipt['status'], 'dispatched'); self.assertEqual(receipt['semantics'], 'dispatch_not_success')
        self.assertIsNone(receipt['observation']['capability']); self.assertEqual(native.client.state, 'NEED_CONTEXT')
        self.assertIsNone(native.client.capability); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**dict(args, command_id='a' * 32))
        self.assertEqual(len(native.requests), count)

    def test_duplicate_queries_receipt_and_preserves_newer_grant(self):
        native = self.fixture(); args = arguments(native.client.observe()); first = native.client.execute(**args)
        self.assertEqual(native.client.execute(**args), first); self.assertEqual(native.requests[-1], ('S ' + COMMAND).encode())
        fresh = native.client.observe(); self.assertEqual(native.client.execute(**args), first)
        self.assertEqual(native.client.capability, fresh['capability']); self.assertEqual(native.client.state, 'READY')
        self.assertEqual(len([req for req in native.requests if req.startswith(b'E ')]), 1)

    def test_cross_action_and_changed_id_reuse_never_sends(self):
        native = self.fixture(); args = arguments(native.client.observe()); native.client.execute(**args); count = len(native.requests)
        for changes in (dict(action='next_soldier'), dict(epoch='f' * 32), dict(capability='f' * 32), dict(revision=3)):
            with self.subTest(changes=changes), self.assertRaises(Unavailable): native.client.execute(**dict(args, **changes))
        self.assertEqual(len(native.requests), count)

    def test_unknown_command_status_never_sends(self):
        native = self.fixture(); native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.read_result(COMMAND)
        self.assertEqual(len(native.requests), count)

    def test_shared_non_evicting_64_entry_ledger(self):
        native = self.fixture()
        for index in range(64):
            native.client.execute(**arguments(native.client.observe(), command_id=f'{index:032x}', action='next_soldier'))
        self.assertEqual(len(native.client.commands), 64); obj = native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(obj))
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'CLOSED'); self.assertIn('0' * 32, native.client.commands)

    def test_dispatch_timeout_terminal_no_retry_or_status_poll(self):
        native = self.fixture(transform=lambda op, obj: None if op == 'E' else obj); args = arguments(native.client.observe())
        with self.assertRaises(Unavailable): native.client.execute(**args)
        count = len(native.requests); self.assertIn(COMMAND, native.client.commands)
        for operation in (native.client.observe, lambda: native.client.execute(**args), lambda: native.client.read_result(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(native.client.state, 'CLOSED'); self.assertEqual(len(native.requests), count)
        self.assertEqual(len([r for r in native.requests if r.startswith(b'E ')]), 1)

    def test_unsolicited_packet_before_mutating_send_is_definitely_unsent(self):
        native = self.fixture(); args = arguments(native.client.observe())
        native.peer.send(b'{}')
        with self.assertRaises(Unavailable) as caught: native.client.execute(**args)
        self.assertNotIsInstance(caught.exception, a.ActionOutcomeUnknown)
        self.assertFalse(any(r.startswith(b'E ') for r in native.requests))
        self.assertEqual(native.client.state, 'CLOSED')

    def test_malformed_reply_after_mutating_send_is_outcome_unknown(self):
        native = self.fixture(transform=lambda op, obj: b'{"hidden_reason":"PRIVATE"}' if op == 'E' else obj)
        args = arguments(native.client.observe())
        with self.assertRaises(a.ActionOutcomeUnknown) as caught: native.client.execute(**args)
        self.assertEqual(caught.exception.command_id, COMMAND)
        self.assertNotIn('PRIVATE', dump(caught.exception.public_error()).decode())
        self.assertEqual(native.client.state, 'CLOSED')
        self.assertEqual(len([r for r in native.requests if r.startswith(b'E ')]), 1)

    def test_malformed_or_wrong_bound_observation_closes(self):
        for changes in (dict(challenge='f' * 32), dict(epoch='f' * 32), dict(hidden=1), dict(revision=True)):
            native = self.fixture(transform=lambda op, obj, c=changes: dict(obj, **c) if op == 'R' else obj)
            with self.subTest(changes=changes), self.assertRaises(Unavailable): native.client.observe()
            self.assertEqual(native.client.state, 'CLOSED')

    def test_generic_error_closes_without_hidden_details(self):
        native = self.fixture(transform=lambda op, obj: dict(schema=a.SCHEMA, mode=a.MODE, type='error', status='unavailable') if op == 'R' else obj)
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.client.state, 'CLOSED')

    def test_unknown_after_dispatch_not_success_and_needs_new_read(self):
        native = self.fixture(transform=lambda op, obj: dict(obj, status='unknown_after_dispatch', observation=None) if op == 'E' else obj)
        receipt = native.client.execute(**arguments(native.client.observe()))
        self.assertEqual(receipt['status'], 'unknown_after_dispatch'); self.assertIsNone(receipt['observation'])
        self.assertEqual(native.client.state, 'NEED_CONTEXT')

    def test_terminal_receipt_cannot_change(self):
        native = self.fixture(transform=lambda op, obj: dict(obj, status='unavailable', observation=None) if op == 'S' else obj)
        args = arguments(native.client.observe()); native.client.execute(**args)
        with self.assertRaises(Unavailable): native.client.read_result(COMMAND)
        self.assertEqual(native.client.state, 'CLOSED')

    def test_reused_capability_after_dispatch_rejected(self):
        native = self.fixture(); obj = native.client.observe(); native.client.execute(**arguments(obj))
        native.transform = lambda op, reply: dict(reply, capability=obj['capability']) if op == 'R' else reply
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.client.state, 'CLOSED')

    def test_counters_never_regress_on_new_read(self):
        native = self.fixture(); native.client.observe(); native.transform = lambda op, obj: dict(obj, revision=0) if op == 'R' else obj
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.client.state, 'CLOSED')


class McpTests(unittest.TestCase):
    def test_tools_explicit_and_distinct_from_supervised(self):
        self.assertFalse({d['name'] for d in adapter.definitions()} & set(a.TOOL_NAMES))
        enabled = {d['name']: d for d in adapter.definitions(agent_control=True)}
        self.assertTrue(set(a.TOOL_NAMES) <= set(enabled)); self.assertNotIn('request_stance', enabled)
        self.assertFalse(enabled['execute_ui_action']['annotations']['readOnlyHint'])
        self.assertIn('not human confirmation', enabled['execute_ui_action']['description'])
        with self.assertRaises(ValueError): adapter.Server(agent_control=object(), supervised_stance=object())
        for name in a.TOOL_NAMES: self.assertEqual(call(adapter.Server(), name)['error']['code'], -32602)

    def test_initialize_has_distinct_identity(self):
        reply = adapter.Server(agent_control=object()).handle(dict(jsonrpc='2.0', id=1, method='initialize',
            params=dict(protocolVersion=adapter.PROTOCOL, capabilities={}, clientInfo=dict(name='test', version='1'))))
        self.assertEqual(reply['result']['serverInfo']['name'], 'openxcom-semantic-agent-control')

    def test_malformed_tool_args_never_call_client(self):
        class Never:
            def __getattr__(self, key): raise AssertionError('Client invoked')
        server = adapter.Server(agent_control=Never())
        cases = [('execute_ui_action', {}), ('execute_ui_action', dict(arguments(), approved=True)),
                 ('execute_ui_action', arguments(action='toggle_kneel')), ('read_ui_state', {'x': 1}),
                 ('read_ui_action_result', {'command_id': 'bad'}), ('read_ui_action_result', {})]
        for name, args in cases:
            with self.subTest(name=name, args=args): self.assertEqual(call(server, name, args)['error']['code'], -32602)
        for args in (None, [], False):
            reply = server.handle(dict(jsonrpc='2.0', id=1, method='tools/call', params=dict(name='read_ui_state', arguments=args)))
            self.assertEqual(reply['error']['code'], -32602)

    def test_structured_output_contains_no_image(self):
        native = NativeFixture(); self.addCleanup(native.close); server = adapter.Server(agent_control=native.client)
        reply = call(server, 'read_ui_state')['result']; self.assertFalse(reply['isError']); obj = reply['structuredContent']
        self.assertEqual(reply['content'], [dict(type='text', text=dump(obj).decode())])
        receipt = call(server, 'execute_ui_action', arguments(obj))['result']
        self.assertEqual(receipt['structuredContent']['status'], 'dispatched')
        self.assertEqual(call(server, 'read_ui_action_result', dict(command_id=COMMAND))['result'], receipt)
        self.assertFalse(any(item['type'] == 'image' for item in receipt['content']))

    def test_lost_action_reply_exposes_adapter_uncertainty_not_native_receipt(self):
        native = NativeFixture(transform=lambda op, obj: None if op == 'E' else obj)
        self.addCleanup(native.close); server = adapter.Server(agent_control=native.client)
        obj = call(server, 'read_ui_state')['result']['structuredContent']
        reply = call(server, 'execute_ui_action', arguments(obj))['result']
        expected = dict(schema=a.ERROR_SCHEMA, mode=a.MODE, type='adapter_error', source='adapter',
                        command_id=COMMAND, status='outcome_unknown', retryable=False, semantics='not_a_native_receipt')
        self.assertTrue(reply['isError']); self.assertEqual(reply['structuredContent'], expected)
        self.assertEqual(reply['content'], [dict(type='text', text=dump(expected).decode())])
        self.assertNotIn('observation', expected)
        count = len(native.requests)
        self.assertTrue(call(server, 'execute_ui_action', arguments(obj))['result']['isError'])
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'CLOSED')

    def test_failure_bounded_no_hidden_reason(self):
        class Failed:
            def observe(self): raise Unavailable('PRIVATE-SENTINEL')
        reply = call(adapter.Server(agent_control=Failed()), 'read_ui_state')['result']
        self.assertEqual(reply, dict(content=[dict(type='text', text='{"error":"unavailable"}')],
                                    structuredContent={'error': 'unavailable'}, isError=True))
        self.assertNotIn('PRIVATE', dump(reply).decode())


class LauncherTests(unittest.TestCase):
    def test_explicit_only_env_and_private_descriptor(self):
        for flags, enabled, count in (([], False, 1), (['--audio-only'], False, 1), (['--supervised-stance'], False, 2),
                                      (['--agent-control'], True, 2), (['--agent-control', '--mixed-audio'], True, 3)):
            with self.subTest(flags=flags), patch.object(sys, 'argv', ['launcher', *flags, '--', 'synthetic-game']), \
                    patch.dict(os.environ, {'OPENXCOM_AGENT_CONTROL': '1', 'OPENXCOM_AGENT_CONTROL_FD': '999'}), \
                    patch('launch_presentation.subprocess.Popen') as spawn, patch('launch_presentation.serve') as serve:
                launch_presentation.main(); env = spawn.call_args.kwargs['env']
                self.assertEqual('OPENXCOM_AGENT_CONTROL' in env, enabled); self.assertEqual('OPENXCOM_AGENT_CONTROL_FD' in env, enabled)
                if enabled:
                    self.assertEqual(env['OPENXCOM_AGENT_CONTROL'], '1'); self.assertIn(int(env['OPENXCOM_AGENT_CONTROL_FD']), spawn.call_args.kwargs['pass_fds'])
                self.assertEqual(len(set(spawn.call_args.kwargs['pass_fds'])), count); self.assertEqual(serve.call_args.args[7] is not None, enabled)
                self.assertEqual(spawn.call_args.kwargs['stdout'], subprocess.DEVNULL); self.assertEqual(spawn.call_args.kwargs['stderr'], subprocess.DEVNULL)
                spawn.return_value.kill.assert_not_called(); spawn.return_value.terminate.assert_not_called()

    def test_cannot_mix_agent_with_any_supervised_or_audio_only(self):
        flags = ['--supervised-stance', '--supervised-selection', '--supervised-turn', '--supervised-map-left-click',
                 '--supervised-inventory', '--supervised-inventory-close', '--supervised-inventory-ground', '--supervised-inventory-click',
                 '--supervised-inventory-right-click', '--supervised-inventory-unload', '--audio-only']
        for flag in flags:
            with self.subTest(flag=flag), patch.object(sys, 'argv', ['launcher', '--agent-control', flag, '--', 'game']), \
                    patch('launch_presentation.subprocess.Popen') as spawn, patch('sys.stderr', io.StringIO()), self.assertRaises(SystemExit):
                launch_presentation.main()
            spawn.assert_not_called()

    def test_real_stdio_launcher_synthetic_peer(self):
        child = r'''
import json, os, socket, sys
assert os.environ['OPENXCOM_AGENT_CONTROL'] == '1' and 'OPENXCOM_STANCE_FD' not in os.environ
channel = socket.socket(fileno=int(os.environ['OPENXCOM_AGENT_CONTROL_FD']))
assert channel.family == socket.AF_UNIX and channel.type == socket.SOCK_SEQPACKET
assert channel.getsockname() in ('', b'') and channel.getpeername() in ('', b'')
print('PRIVATE CHILD STDOUT'); print('PRIVATE CHILD STDERR', file=sys.stderr)
def send(obj): channel.send(json.dumps(obj, separators=(',', ':')).encode())
schema, mode, epoch = 'openxcom-agent-control/1', 'agent-control', '1' * 32
send(dict(schema=schema,mode=mode,epoch=epoch,ledger_capacity=64))
assert channel.recv(256) == b'H openxcom-agent-control/1'; send(dict(schema=schema,mode=mode,status='ready'))
raw=channel.recv(256).decode(); assert raw.startswith('R ')
observation=dict(schema=schema,mode=mode,type='state',challenge=raw[2:],epoch=epoch,revision=1,capability='2'*32,
 state='next_turn',visible=dict(title='UFO',turn='Turn 1',side='XCOM',message='Continue'),
 actions=[dict(id='continue_next_turn',label='Continue',label_source='visible_text')],event_sequence=1,semantics='presented_after_request')
send(observation)
assert channel.recv(256) == ('E '+'3'*32+' '+epoch+' '+'2'*32+' 1 continue_next_turn').encode()
observation.update(challenge=None,capability=None,revision=2,event_sequence=2)
receipt=dict(schema=schema,mode=mode,type='result',command_id='3'*32,status='dispatched',event_sequence=2,
 observation=observation,semantics='dispatch_not_success'); send(receipt)
assert channel.recv(256) == ('S '+'3'*32).encode(); send(receipt)
assert channel.recv(256) == b''
'''
        messages = [dict(jsonrpc='2.0', id=1, method='initialize', params=dict(protocolVersion=adapter.PROTOCOL,
                    capabilities={}, clientInfo=dict(name='synthetic', version='1'))),
                    dict(jsonrpc='2.0', method='notifications/initialized'), dict(jsonrpc='2.0', id=2, method='tools/list')]
        for ident, (name, args) in enumerate((('read_ui_state', {}), ('execute_ui_action', arguments(action='continue_next_turn')),
                                            ('execute_ui_action', arguments(action='continue_next_turn'))), 3):
            messages.append(dict(jsonrpc='2.0', id=ident, method='tools/call', params=dict(name=name, arguments=args)))
        completed = subprocess.run([sys.executable, str(Path(__file__).with_name('launch_presentation.py')), '--agent-control', '--', sys.executable, '-c', child],
                                   input=b''.join(dump(message)+b'\n' for message in messages), capture_output=True, timeout=8)
        self.assertEqual(completed.returncode, 0); self.assertEqual(completed.stderr, b'')
        replies = [json.loads(line) for line in completed.stdout.splitlines()]; self.assertEqual(len(replies), 5)
        self.assertEqual(replies[0]['result']['serverInfo']['name'], 'openxcom-semantic-agent-control')
        self.assertTrue(set(a.TOOL_NAMES) <= {t['name'] for t in replies[1]['result']['tools']})
        self.assertTrue(all(not response['result']['isError'] for response in replies[2:])); self.assertNotIn(b'PRIVATE', completed.stdout)
        self.assertEqual(replies[3]['result'], replies[4]['result'])


if __name__ == '__main__': unittest.main()
