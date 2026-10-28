# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
"""Synthetic /9 wire/transport tests, not raster witness or live combat proof."""
import copy
import json
import unittest

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, result, arguments, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate, presentation, audio
from test_agent_reports import message


def projectile(seq=1):
    return dict(seq=seq, kind='projectile_presented',
                semantics='ordinary_renderer_visible_contribution', source=None,
                actor=None, faction=None, world_position=None, hit=None, damage=None)


class ProjectileValidationTests(unittest.TestCase):
    def test_exact_protocol_catalog_counts_and_limits_unchanged(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/45')
        self.assertEqual((len(a.ACTIONS), len(a.STATE_ACTIONS), len(a.definitions())), (233, 100, 7))
        self.assertEqual((a.MAX_COMMANDS, a.EVENT_CAPACITY, a.MAX_EVENTS), (256, 256, 16))
        self.assertEqual((a.MAX_REQUEST, a.MAX_TEXT_PACKET, a.MAX_PACKET), (512, 32768, 49152))
        self.assertEqual(a.ERROR_SCHEMA, 'openxcom-mcp-agent-control-error/1')

    def test_exact_event_independent_of_audio_capability(self):
        for history in ('mixed_output', 'unavailable'):
            self.assertEqual(validate(page([projectile()], history=history), history=history)['events'], [projectile()])

    def test_all_six_event_kinds_share_one_ordered_ring(self):
        events = [presentation(1), projectile(2), message(seq=3), audio(4),
                  dict(seq=5, kind='audio_status', status='active', partial_window_discarded=False),
                  dict(seq=6, kind='audio_gap', dropped_windows=1)]
        self.assertEqual(validate(page(events))['events'], events)
        self.assertEqual(validate(page(events[1:4], after=1, newest=6), after=1)['next_cursor'], 4)

    def test_every_required_field_and_extra_field_is_closed(self):
        for key in projectile():
            obj = page([projectile()]); del obj['events'][0][key]
            with self.subTest(missing=key), self.assertRaises(Unavailable): validate(obj)
        for key, value in (('shooter', None), ('target', None), ('resource_id', 1), ('position', None),
                           ('coordinates', []), ('projectile_id', 'p0'), ('count', 1), ('weapon', None),
                           ('action', 'fire'), ('capability', '2' * 32), ('hit_points', None),
                           ('timestamp', 1), ('episode_id', 'local')):
            event = projectile(); event[key] = value
            with self.subTest(extra=key), self.assertRaises(Unavailable): validate(page([event]))

    def test_all_six_attribution_fields_are_null_not_false_or_zero(self):
        for key in ('source', 'actor', 'faction', 'world_position', 'hit', 'damage'):
            for value in (False, True, 0, 1, 0.0, '', 'unknown', [], {}, {'x': 0}):
                event = projectile(); event[key] = value
                with self.subTest(key=key, value=value), self.assertRaises(Unavailable): validate(page([event]))

    def test_semantics_and_kind_are_exact_literals(self):
        for key, values in (
            ('semantics', (None, False, 0, {}, 'visible_projectile', 'ordinary_renderer_visible_contribution ',
                           'enemy_fired', 'attack', 'hit')),
            ('kind', (None, False, 0, [], 'projectile', 'projectile_absent', 'projectile_presented '))):
            for value in values:
                event = projectile(); event[key] = value
                with self.subTest(key=key, value=value), self.assertRaises(Unavailable): validate(page([event]))

    def test_sequence_is_positive_safe_integer_and_contiguous(self):
        for value in (None, False, True, 0, -1, 1.0, '1', a.MAX_COUNTER + 1):
            obj = page([projectile()]); obj['events'][0]['seq'] = value
            with self.subTest(value=value), self.assertRaises(Unavailable): validate(obj)
        for seq in (1, 3):
            with self.subTest(second=seq), self.assertRaises(Unavailable):
                validate(page([projectile(1), projectile(seq)], newest=3))
        validate(page([projectile(a.MAX_COUNTER)], after=a.MAX_COUNTER-1), after=a.MAX_COUNTER-1)

    def test_event_limit_retention_gap_and_partial_pages_remain_exact(self):
        validate(page([projectile(i) for i in range(1, 17)]))
        with self.assertRaises(Unavailable): validate(page([projectile(i) for i in range(1, 18)]))
        obj = page([projectile(45)], newest=300)
        self.assertTrue(validate(obj)['gap'])
        self.assertEqual(validate(obj)['next_cursor'], 45)
        for key, value in (('gap', False), ('oldest', 44), ('next_cursor', 300)):
            wrong = copy.deepcopy(obj); wrong[key] = value
            with self.subTest(key=key), self.assertRaises(Unavailable): validate(wrong)

    def test_duplicate_json_key_is_not_silently_accepted(self):
        raw = dump(page([projectile()])).replace(b'"source":null', b'"source":null,"source":null')
        with self.assertRaises(Unavailable): a.validate_events(raw, CHALLENGE, EPOCH, 0, 16, 'mixed_output')

    def test_all_earlier_response_identities_reject(self):
        for version in range(1, 10):
            schema = f'openxcom-agent-control/{version}'
            hello = dict(schema=schema, mode=a.MODE, epoch=EPOCH, ledger_capacity=256, command_generation=1, audio_history='mixed_output')
            with self.subTest(version=version), self.assertRaises(Unavailable): a.validate_hello(dump(hello))
            with self.assertRaises(Unavailable): a.validate_ready(dump(dict(schema=schema, mode=a.MODE, status='ready')))
            obj = state(); obj['schema'] = schema
            with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
            obj = result(); obj['schema'] = schema
            with self.assertRaises(Unavailable): a.validate_result(dump(obj), COMMAND, EPOCH)
            obj = page([projectile()]); obj['schema'] = schema
            with self.assertRaises(Unavailable): validate(obj)

    def test_tool_prose_retains_episode_and_unknown_attribution_limit(self):
        description = next(x for x in a.definitions() if x['name'] == 'read_public_events')['description']
        for phrase in ('projectile_presented', 'remain null', 'Episodes may coalesce', 'no attack counter'):
            self.assertIn(phrase, description)


class ProjectileClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs); self.addCleanup(native.close); return native

    def test_delayed_history_and_exact_replay_do_not_consume_grant_or_admit_command(self):
        native = self.fixture(); observed = native.client.observe()
        native.public_events.extend(projectile(i) for i in range(1, 4))
        before = (native.client.capability, native.client.capability_deadline,
                  native.client.revision, native.client.event_sequence)
        first = native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(native.client.read_events(EPOCH, 0, 16)['events'], first['events'])
        self.assertEqual((native.client.capability, native.client.capability_deadline,
                          native.client.revision, native.client.event_sequence), before)
        self.assertEqual(native.client.commands, {})
        args = arguments(observed); receipt = native.client.execute(**args)
        native.client.read_events(EPOCH, 0, 16)
        fresh = native.client.observe()
        self.assertEqual(native.client.execute(**args), receipt)
        self.assertEqual(native.client.capability, fresh['capability'])
        self.assertEqual(native.requests[-1], ('S 1 ' + COMMAND).encode())
        self.assertEqual(sum(raw.startswith(b'E ') for raw in native.requests), 1)

    def test_no_prior_read_and_unsupported_read_cannot_gain_action_authority(self):
        native = self.fixture(screen='unsupported'); native.public_events.append(projectile())
        with self.assertRaises(Unavailable): native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(native.requests, [])
        native.client.observe(); native.client.read_events(EPOCH, 0, 16)
        self.assertIsNone(native.client.capability); self.assertEqual(native.client.commands, {})
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments())
        self.assertEqual(len(native.requests), count)

    def test_old_cursor_reports_real_ring_gap_and_pages_without_invented_events(self):
        native = self.fixture(); native.client.observe()
        native.public_events.extend(projectile(i) for i in range(1, 301))
        native.event_page_limit = 2
        first = native.client.read_events(EPOCH, 0, 16)
        self.assertTrue(first['gap']); self.assertEqual(first['oldest'], 45)
        self.assertEqual([e['seq'] for e in first['events']], [45, 46])
        second = native.client.read_events(EPOCH, first['next_cursor'], 16)
        self.assertFalse(second['gap']); self.assertEqual([e['seq'] for e in second['events']], [47, 48])
        self.assertEqual(native.client.read_events(EPOCH, 0, 16)['events'], first['events'])

    def test_returned_event_is_detached_and_changed_retained_kind_closes(self):
        native = self.fixture(); native.client.observe(); native.public_events.append(projectile())
        returned = native.client.read_events(EPOCH, 0, 16); returned['events'][0]['hit'] = True
        self.assertEqual(native.client.read_events(EPOCH, 0, 16)['events'], [projectile()])
        native.public_events[0] = presentation()
        with self.assertRaises(Unavailable): native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(native.client.state, 'CLOSED')

    def test_malformed_projectile_read_closes_without_action_or_retry(self):
        native = self.fixture(); observed = native.client.observe()
        event = projectile(); event['faction'] = 'enemy'; native.public_events.append(event)
        with self.assertRaises(Unavailable) as caught: native.client.read_events(EPOCH, 0, 16)
        self.assertNotIsInstance(caught.exception, a.ActionOutcomeUnknown)
        count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(observed))
        with self.assertRaises(Unavailable): native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'CLOSED')
        self.assertFalse(any(raw.startswith(b'E ') for raw in native.requests))

    def test_prior_v8_hello_rejects_before_any_client_request(self):
        native = self.fixture(hello=dict(schema='openxcom-agent-control/8', mode=a.MODE,
            epoch=EPOCH, ledger_capacity=256, command_generation=1, audio_history='mixed_output'))
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.requests, []); self.assertEqual(native.client.state, 'CLOSED')

    def test_lost_action_receipt_after_projectile_history_is_terminal_unknown(self):
        native = self.fixture(transform=lambda op, obj: None if op == 'E' else obj)
        observed = native.client.observe(); native.public_events.append(projectile())
        native.client.read_events(EPOCH, 0, 16); args = arguments(observed)
        with self.assertRaises(a.ActionOutcomeUnknown) as caught: native.client.execute(**args)
        self.assertEqual(caught.exception.command_id, COMMAND)
        count = len(native.requests)
        for action in (lambda: native.client.execute(**args), lambda: native.client.read_result(COMMAND),
                       lambda: native.client.read_events(EPOCH, 0, 16), native.client.observe):
            with self.assertRaises(Unavailable): action()
        self.assertEqual(len(native.requests), count)
        self.assertEqual(sum(raw.startswith(b'E ') for raw in native.requests), 1)

    def test_all_semantic_tools_remain_text_only_with_projectile_history(self):
        native = self.fixture(); server = adapter.Server(agent_control=native.client)
        observed = call(server, 'read_ui_state')['result']; native.public_events.append(projectile())
        history = call(server, 'read_public_events', dict(epoch=EPOCH, after=0, limit=16))['result']
        dispatched = call(server, 'execute_ui_action', arguments(observed['structuredContent']))['result']
        status = call(server, 'read_ui_action_result', dict(command_id=COMMAND))['result']
        for reply in (observed, history, dispatched, status):
            self.assertFalse(reply['isError']); self.assertEqual([e['type'] for e in reply['content']], ['text'])
            self.assertEqual(json.loads(reply['content'][0]['text']), reply['structuredContent'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
