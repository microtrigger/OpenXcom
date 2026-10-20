# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
"""Synthetic /6 displayed-text and ordinary acknowledgment contracts only."""
import json
import unittest

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, result, arguments, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate, presentation


def message(text='Jane\nis under alien control', report='alien_control', seq=1):
    return dict(seq=seq, kind='displayed_message', text=text, report=report, source=None, position=None)


class DisplayedReportValidationTests(unittest.TestCase):
    def test_exact_compiled_protocol_actions_states_and_tool_count(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/17')
        self.assertEqual(len(a.ACTIONS), 66)
        self.assertEqual(len(a.STATE_ACTIONS), 29)
        self.assertEqual(len(a.definitions()), 4)
        self.assertEqual(a.ARGUMENT_KINDS['acknowledge_message'], 'none')
        self.assertEqual(a.STATE_ACTIONS['infobox'], frozenset())
        self.assertEqual(a.STATE_ACTIONS['infobox_ok'], {'acknowledge_message'})

    def test_exact_four_visible_suffixes_and_unicode_prefix(self):
        for suffix, report in [('is under alien control', 'alien_control'),
                               ('is under Molecular Control', 'alien_control'),
                               ('has Panicked', 'panic'), ('has gone Berserk', 'berserk')]:
            for prefix in ('Jane', 'Élodie', 'A\u0301', '名', ' '):
                text = prefix + '\n' + suffix
                with self.subTest(text=text):
                    self.assertEqual(a.displayed_message_report(text), report)
                    validate(page([message(text, report)]))

    def test_unknown_text_is_retained_without_classification(self):
        for text in ('Unknown message', 'has Panicked', '\nhas Panicked',
                     'Jane\nhas Panicked!', 'Jane\nhas panicked', 'Jane\nhas Panicked ',
                     'Jane\nhas  Panicked', 'Jane\r\nhas Panicked',
                     'Jane\nextra\nhas Panicked', 'Jane\nhas Panicked\n',
                     'Jane\nis under molecular control', '\x00unknown\x85message',
                     'Jane\u0085\nhas Panicked'):
            with self.subTest(text=text):
                self.assertIsNone(a.displayed_message_report(text))
                validate(page([message(text, None)]))

    def test_every_c0_c1_prefix_control_disables_only_classification(self):
        for code in list(range(32)) + list(range(127, 160)):
            text = 'Jane' + chr(code) + '\nhas Panicked'
            with self.subTest(code=code):
                validate(page([message(text, None)]))
                with self.assertRaises(Unavailable): validate(page([message(text, 'panic')]))

    def test_text_bound_is_utf8_bytes_and_nonempty(self):
        for text in ('A' * 1024, 'é' * 512, '\U0001f600' * 256):
            validate(page([message(text, None)]))
        for text in ('', 'A' * 1025, 'é' * 513, '\U0001f600' * 257, '\ud800', None, 1, True):
            with self.subTest(text=repr(text)), self.assertRaises(Unavailable):
                validate(page([message(text, None)]))
        suffix = '\nhas Panicked'
        text = 'A' * (1024 - len(suffix)) + suffix
        validate(page([message(text, 'panic')]))
        self.assertIsNone(a.displayed_message_report('A' + text))

    def test_report_must_equal_classifier_including_null(self):
        for text, expected in [('Jane\nis under alien control', 'alien_control'),
                               ('Jane\nhas Panicked', 'panic'),
                               ('Jane\nhas gone Berserk', 'berserk'), ('Unrecognized', None)]:
            for report in (None, 'alien_control', 'panic', 'berserk', 'shot', True, 0, {}, []):
                if report == expected: continue
                with self.subTest(text=text, report=report), self.assertRaises(Unavailable):
                    validate(page([message(text, report)]))

    def test_exact_keys_and_null_attribution(self):
        for key, value in [('source', 'unit-7'), ('position', {'x': 1, 'y': 2}),
                           ('unit_id', 7), ('cause', 'psi'), ('sound', None), ('capability', '2' * 32)]:
            event = message(); event[key] = value
            with self.subTest(key=key), self.assertRaises(Unavailable): validate(page([event]))
        for key in message():
            obj = page([message()]); del obj['events'][0][key]
            with self.subTest(missing=key), self.assertRaises(Unavailable): validate(obj)

    def test_messages_do_not_require_audio_capability(self):
        validate(page([message()], history='unavailable'), history='unavailable')

    def test_infobox_nullable_text_and_no_hidden_fields(self):
        for screen in ('infobox', 'infobox_ok'):
            for text in (None, '', 'Jane\nhas Panicked'):
                obj = state(screen=screen); obj['visible']['message'] = text
                a.validate_state(obj, CHALLENGE, EPOCH)
                event = presentation(screen=screen); event['visible']['message'] = text
                validate(page([event]))
            obj = state(screen=screen); obj['visible']['source'] = None
            with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        obj = state(screen='infobox'); obj['capability'] = '2' * 32
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_acknowledge_only_exposed_ok_state_and_exact_args(self):
        descriptor = state(screen='infobox_ok')['actions'][0]
        for screen in a.STATE_ACTIONS:
            if screen == 'infobox_ok': continue
            obj = state(screen=screen); obj['actions'] = [descriptor]; obj['capability'] = '2' * 32
            with self.subTest(screen=screen), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        for args in ({'value': True}, {'message': 'OK'}, [], None):
            obj = arguments(state(screen='infobox_ok'), action='acknowledge_message'); obj['args'] = args
            with self.subTest(args=args), self.assertRaises(Unavailable): a.validate_action_arguments(obj)
        obj = state(screen='infobox_ok'); obj['actions'][0]['label_source'] = 'visible_control_semantics'
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_earlier_schema_rejected_in_every_response_envelope(self):
        for version in range(1, 10):
            old = f'openxcom-agent-control/{version}'
            hello = dict(schema=old, mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output')
            with self.subTest(version=version), self.assertRaises(Unavailable): a.validate_hello(dump(hello))
            with self.assertRaises(Unavailable): a.validate_ready(dump(dict(schema=old, mode=a.MODE, status='ready')))
            obj = state(screen='infobox'); obj['schema'] = old
            with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
            obj = result(); obj['schema'] = old
            with self.assertRaises(Unavailable): a.validate_result(dump(obj), COMMAND, EPOCH)
            obj = page([message()]); obj['schema'] = old
            with self.assertRaises(Unavailable): validate(obj)


class DisplayedReportClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs); self.addCleanup(native.close); return native

    def test_timed_infobox_event_read_mints_no_action_authority(self):
        native = self.fixture(screen='infobox'); observed = native.client.observe()
        self.assertIsNone(observed['capability']); self.assertEqual(observed['actions'], [])
        native.public_events.append(message()); native.client.read_events(EPOCH, 0, 16)
        self.assertIsNone(native.client.capability); self.assertEqual(native.client.commands, {})
        self.assertEqual(sum(request.startswith(b'E ') for request in native.requests), 0)

    def test_message_history_preserves_grant_and_acknowledge_duplicate_is_status_only(self):
        native = self.fixture(screen='infobox_ok', transitions={('infobox_ok', 'acknowledge_message'): 'battlescape'})
        observed = native.client.observe(); args = arguments(observed, action='acknowledge_message')
        native.public_events.append(message()); native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(native.client.capability, observed['capability'])
        receipt = native.client.execute(**args)
        self.assertEqual(receipt['observation']['state'], 'battlescape')
        fresh = native.client.observe()
        self.assertEqual(native.client.execute(**args), receipt)
        self.assertEqual(native.client.capability, fresh['capability'])
        self.assertEqual(native.requests[-1], ('S ' + COMMAND).encode())
        self.assertEqual(sum(request.startswith(b'E ') for request in native.requests), 1)

    def test_stale_acknowledgment_is_rejected_without_send(self):
        native = self.fixture(screen='infobox_ok'); old = native.client.observe(); native.client.observe()
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(old, action='acknowledge_message'))
        self.assertEqual(len(native.requests), count)

    def test_wrong_report_closes_event_transport_and_prevents_acknowledgment(self):
        native = self.fixture(screen='infobox_ok'); observed = native.client.observe()
        native.public_events.append(message(report='panic'))
        with self.assertRaises(Unavailable): native.client.read_events(EPOCH, 0, 16)
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(observed, action='acknowledge_message'))
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'CLOSED')

    def test_lost_acknowledgment_receipt_has_terminal_unknown_outcome(self):
        native = self.fixture(screen='infobox_ok', transform=lambda op, obj: None if op == 'E' else obj)
        observed = native.client.observe(); args = arguments(observed, action='acknowledge_message')
        with self.assertRaises(a.ActionOutcomeUnknown) as caught: native.client.execute(**args)
        self.assertEqual(caught.exception.command_id, COMMAND)
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**args)
        with self.assertRaises(Unavailable): native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(len(native.requests), count)
        self.assertEqual(sum(request.startswith(b'E ') for request in native.requests), 1)

    def test_mutated_known_message_is_not_silently_reclassified(self):
        native = self.fixture(); native.client.observe(); native.public_events.append(message())
        first = native.client.read_events(EPOCH, 0, 16)
        first['events'][0]['text'] = 'Edited return value'
        self.assertEqual(native.client.read_events(EPOCH, 0, 16)['events'][0]['text'], message()['text'])
        native.public_events[0] = message('Jane\nhas Panicked', 'panic')
        with self.assertRaises(Unavailable): native.client.read_events(EPOCH, 0, 16)

    def test_all_four_tools_return_json_without_images(self):
        native = self.fixture(screen='infobox_ok'); server = adapter.Server(agent_control=native.client)
        observed = call(server, 'read_ui_state')['result']; native.public_events.append(message())
        history = call(server, 'read_public_events', dict(epoch=EPOCH, after=0, limit=16))['result']
        dispatched = call(server, 'execute_ui_action', arguments(observed['structuredContent'], action='acknowledge_message'))['result']
        status = call(server, 'read_ui_action_result', dict(command_id=COMMAND))['result']
        for reply in (observed, history, dispatched, status):
            self.assertFalse(reply['isError']); self.assertEqual([entry['type'] for entry in reply['content']], ['text'])
            self.assertEqual(json.loads(reply['content'][0]['text']), reply['structuredContent'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
