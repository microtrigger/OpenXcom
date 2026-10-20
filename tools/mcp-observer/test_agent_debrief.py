# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
"""Synthetic protocol /8 debrief contracts; no game outcome or table proof."""
import copy
import unittest
import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, result, arguments, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, presentation, validate


class DebriefProjectionTests(unittest.TestCase):
    def test_identity_counts_and_kinds(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/13')
        self.assertEqual((len(a.ACTIONS), len(a.STATE_ACTIONS), len(a.definitions())), (47, 24, 4))
        self.assertEqual(a.STATE_ACTIONS['debriefing'], {'close_debriefing'})
        self.assertEqual(a.ARGUMENT_KINDS['close_debriefing'], 'none')

    def test_rendered_nullable_strings(self):
        for field in ('title', 'rating', 'stats_label'):
            for value in (None, '', 'Mission complete', 'x' * 1024, 'é' * 512):
                obj = state(screen='debriefing'); obj['visible'][field] = value
                a.validate_state(obj, CHALLENGE, EPOCH)

    def test_missing_extra_hidden_fields(self):
        for key in state(screen='debriefing')['visible']:
            obj = state(screen='debriefing'); del obj['visible'][key]
            with self.subTest(key=key), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        for key in ('rows', 'score', 'casualties', 'recovered_items', 'mission_id', 'victory', 'base', 'stats'):
            obj = state(screen='debriefing'); obj['visible'][key] = None
            with self.subTest(key=key), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_bad_text_types_lengths_and_surrogates(self):
        for field in ('title', 'rating', 'stats_label'):
            for value in (True, 0, [], {}, 'x' * 1025, 'é' * 513, '\ud800'):
                obj = state(screen='debriefing'); obj['visible'][field] = value
                with self.subTest(field=field, value=repr(value)[:25]), self.assertRaises(Unavailable):
                    a.validate_state(obj, CHALLENGE, EPOCH)

    def test_table_always_unavailable(self):
        for value in ('presented', 'supported', '', None, True, 0, [], {}):
            obj = state(screen='debriefing'); obj['visible']['table_status'] = value
            with self.subTest(value=value), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_close_canonical_empty_args_and_visible_text(self):
        obj = state(screen='debriefing'); request = arguments(obj, action='close_debriefing')
        self.assertEqual(a.canonical_action_request(request),
                         f'E {COMMAND} {EPOCH} {obj["capability"]} 1 close_debriefing'.encode())
        for args in (None, [], False, {'confirm': True}, {'stats': False}, {'destination': 'main_menu'}):
            with self.subTest(args=args), self.assertRaises(Unavailable): a.validate_action_arguments(dict(request, args=args))
        for key, value in [('label_source', 'visible_control_semantics'), ('argument_kind', 'boolean')]:
            altered = copy.deepcopy(obj); altered['actions'][0][key] = value
            with self.assertRaises(Unavailable): a.validate_state(altered, CHALLENGE, EPOCH)

    def test_close_only_debrief_no_stats_authority(self):
        close = state(screen='debriefing')['actions'][0]
        for screen in a.STATE_ACTIONS:
            if screen == 'debriefing': continue
            obj = state(screen=screen); obj.update(actions=[close], capability='2' * 32)
            with self.subTest(screen=screen), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        for action in ('open_stats', 'read_stats'):
            with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action=action))
        obj = state(screen='debriefing'); obj['actions'][0]['id'] = 'continue_next_turn'
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_unoffered_close_has_no_grant(self):
        obj = state(screen='debriefing'); obj.update(actions=[], capability=None)
        a.validate_state(obj, CHALLENGE, EPOCH)
        obj['capability'] = '2' * 32
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_history_same_projection_no_authority(self):
        event = presentation(screen='debriefing'); validate(page([event]))
        for key, value in [('actions', []), ('capability', '2' * 32), ('score', 10)]:
            altered = copy.deepcopy(event); altered[key] = value
            with self.assertRaises(Unavailable): validate(page([altered]))
        event['visible']['table_status'] = 'presented'
        with self.assertRaises(Unavailable): validate(page([event]))

    def test_post_action_never_grants_authority(self):
        obj = result(observation=state(None, None, event_sequence=2, screen='debriefing'))
        a.validate_result(dump(obj), COMMAND, EPOCH)
        obj['observation']['capability'] = '2' * 32
        with self.assertRaises(Unavailable): a.validate_result(dump(obj), COMMAND, EPOCH)

    def test_every_prior_identity_rejected(self):
        for version in range(1, 10):
            old = f'openxcom-agent-control/{version}'
            with self.subTest(version=version):
                with self.assertRaises(Unavailable): a.validate_hello(dump(dict(schema=old, mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output')))
                with self.assertRaises(Unavailable): a.validate_ready(dump(dict(schema=old, mode=a.MODE, status='ready')))
                obj = state(screen='debriefing'); obj['schema'] = old
                with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
                obj = result(); obj['schema'] = old
                with self.assertRaises(Unavailable): a.validate_result(dump(obj), COMMAND, EPOCH)
                obj = page([presentation(screen='debriefing')]); obj['schema'] = old
                with self.assertRaises(Unavailable): validate(obj)


class DebriefTransportTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(screen='debriefing', **kwargs); self.addCleanup(native.close); return native

    def test_close_receipt_and_separate_main_menu_read(self):
        native = self.fixture(transitions={('debriefing', 'close_debriefing'): 'main_menu'})
        observed = native.client.observe(); receipt = native.client.execute(**arguments(observed, action='close_debriefing'))
        self.assertEqual(receipt['status'], 'dispatched'); self.assertEqual(receipt['semantics'], 'dispatch_not_success')
        self.assertEqual(receipt['observation']['state'], 'main_menu')
        self.assertEqual(native.client.read_result(COMMAND), receipt)
        self.assertEqual(native.client.observe()['state'], 'main_menu')
        self.assertEqual(sum(req.startswith(b'E ') for req in native.requests), 1)

    def test_dispatch_does_not_predict_destination(self):
        native = self.fixture(); observed = native.client.observe()
        receipt = native.client.execute(**arguments(observed, action='close_debriefing'))
        self.assertEqual(receipt['observation']['state'], 'debriefing')
        self.assertEqual(native.client.observe()['state'], 'debriefing')

    def test_events_preserve_grant_duplicate_status_only(self):
        native = self.fixture(transitions={('debriefing', 'close_debriefing'): 'main_menu'})
        observed = native.client.observe(); request = arguments(observed, action='close_debriefing')
        native.public_events = [presentation(screen='debriefing')]; native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(native.client.capability, observed['capability'])
        receipt = native.client.execute(**request); fresh = native.client.observe()
        self.assertEqual(native.client.execute(**request), receipt)
        self.assertEqual(native.requests[-1], f'S {COMMAND}'.encode())
        self.assertEqual(native.client.capability, fresh['capability'])
        self.assertEqual(sum(req.startswith(b'E ') for req in native.requests), 1)

    def test_stale_and_conflicting_close_never_send(self):
        native = self.fixture(); old = native.client.observe(); fresh = native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(old, action='close_debriefing'))
        self.assertEqual(len(native.requests), count)
        request = arguments(fresh, action='close_debriefing'); native.client.execute(**request); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**dict(request, action='cancel_menu'))
        self.assertEqual(len(native.requests), count)

    def test_unoffered_close_rejected_without_send(self):
        def transform(op, obj):
            if op == 'R': obj.update(actions=[], capability=None)
            return obj
        native = self.fixture(transform=transform); observed = native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(observed, action='close_debriefing'))
        self.assertEqual(len(native.requests), count)

    def test_v7_peer_rejected_before_request(self):
        native = self.fixture(hello=dict(schema='openxcom-agent-control/7', mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output'))
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.requests, []); self.assertEqual(native.client.state, 'CLOSED')

    def test_postsend_bad_debrief_unknown_terminal_no_replay(self):
        def transform(op, obj):
            if op == 'E': obj['observation']['visible']['score'] = 999
            return obj
        native = self.fixture(transform=transform); observed = native.client.observe(); request = arguments(observed, action='close_debriefing')
        with self.assertRaises(a.ActionOutcomeUnknown): native.client.execute(**request)
        count = len(native.requests)
        for operation in (lambda: native.client.execute(**request), native.client.observe, lambda: native.client.read_result(COMMAND)):
            with self.assertRaises(Unavailable): operation()
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'CLOSED')

    def test_four_semantic_tools_have_no_image_output(self):
        native = self.fixture(transitions={('debriefing', 'close_debriefing'): 'main_menu'}); server = adapter.Server(agent_control=native.client)
        observed = call(server, 'read_ui_state')['result']; obj = observed['structuredContent']
        native.public_events = [presentation(screen='debriefing')]
        history = call(server, 'read_public_events', dict(epoch=EPOCH, after=0, limit=16))['result']
        receipt = call(server, 'execute_ui_action', arguments(obj, action='close_debriefing'))['result']
        status = call(server, 'read_ui_action_result', dict(command_id=COMMAND))['result']
        for reply in (observed, history, receipt, status):
            self.assertFalse(reply['isError'])
            self.assertEqual(reply['content'], [dict(type='text', text=dump(reply['structuredContent']).decode())])


if __name__ == '__main__': unittest.main()
