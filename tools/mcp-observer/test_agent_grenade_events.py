# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-19.
"""Synthetic /15 grenade-event wire and adapter contracts only.

The socket peer supplies fabricated records and receipts. These tests establish
closed Python validation, replay and authority behavior, not native producer
correctness, renderer admission, raster attribution, causality, game play,
explosion damage, event/video synchronization, or live grenade visibility.
"""
import copy
import json
import unittest
from unittest.mock import patch

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, result, arguments, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate, presentation, audio
from test_agent_projectiles import projectile
from test_agent_reports import message


KINDS = ('thrown_item_presented', 'large_explosion_effect_presented')
NULL_FIELDS = ('source', 'actor', 'faction', 'world_position', 'hit', 'damage')
EVENT_FIELDS = {'seq', 'kind', 'semantics', *NULL_FIELDS}


def grenade_event(kind=KINDS[0], seq=1):
    event = projectile(seq)
    event['kind'] = kind
    return event


class GrenadeEventValidationTests(unittest.TestCase):
    def test_wire15_limits_catalog_and_exact_ten_kind_union(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/41')
        self.assertEqual((len(a.ACTIONS), len(a.STATE_ACTIONS), len(a.definitions())), (227, 97, 7))
        self.assertEqual(a.INVENTORY_TARGET_HANDLES, tuple(f'v{i}' for i in range(96)))
        self.assertEqual((a.MAX_COMMANDS, a.EVENT_CAPACITY, a.MAX_EVENTS), (256, 256, 16))
        self.assertEqual((a.MAX_REQUEST, a.MAX_TEXT_PACKET, a.MAX_PACKET), (512, 32768, 49152))
        self.assertEqual(a.ERROR_SCHEMA, 'openxcom-mcp-agent-control-error/1')
        events = [presentation(1), message(seq=2), audio(3),
                  dict(seq=4, kind='audio_status', status='active', partial_window_discarded=False),
                  dict(seq=5, kind='audio_gap', dropped_windows=1), projectile(6),
                  grenade_event('impact_effect_presented', 7),
                  grenade_event('collapse_animation_presented', 8),
                  grenade_event(KINDS[0], 9), grenade_event(KINDS[1], 10)]
        self.assertEqual(len({event['kind'] for event in events}), 10)
        self.assertEqual(validate(page(events))['events'], events)
        self.assertEqual(validate(page(events[5:], after=5), after=5)['next_cursor'], 10)

    def test_new_kinds_are_exact_nine_key_records_without_audio_dependency(self):
        for kind in KINDS:
            event = grenade_event(kind)
            self.assertEqual(set(event), EVENT_FIELDS)
            for history in ('mixed_output', 'unavailable'):
                with self.subTest(kind=kind, history=history):
                    self.assertEqual(validate(page([event], history=history), history=history)['events'], [event])

    def test_every_new_event_field_is_required(self):
        for kind in KINDS:
            for field in EVENT_FIELDS:
                obj = page([grenade_event(kind)])
                del obj['events'][0][field]
                with self.subTest(kind=kind, missing=field), self.assertRaises(Unavailable):
                    validate(obj)

    def test_extra_source_identity_causality_outcome_and_authority_fields_reject(self):
        extras = ('thrower', 'shooter', 'owner', 'target', 'unit_id', 'actor_id',
                  'item_id', 'projectile_id', 'explosion_id', 'resource_id', 'weapon',
                  'grenade', 'fuse', 'timer', 'radius', 'position', 'coordinates',
                  'cause', 'caused_by', 'parent_seq', 'throw_seq', 'command_id',
                  'episode_id', 'death', 'outcome', 'count', 'timestamp',
                  'actions', 'capability', 'revision')
        for kind in KINDS:
            for field in extras:
                for value in (None, 1, 'private'):
                    event = grenade_event(kind)
                    event[field] = value
                    with self.subTest(kind=kind, extra=field, value=value), self.assertRaises(Unavailable):
                        validate(page([event]))

    def test_all_six_null_fields_reject_every_nonnull_json_type(self):
        for kind in KINDS:
            for field in NULL_FIELDS:
                for value in (False, True, 0, 1, -1, 0.0, '', 'unknown', [], [None], {}, {'x': 0}):
                    event = grenade_event(kind)
                    event[field] = value
                    with self.subTest(kind=kind, field=field, value=value), self.assertRaises(Unavailable):
                        validate(page([event]))

    def test_semantics_kind_and_nearby_unimplemented_aliases_are_closed_literals(self):
        for kind in KINDS:
            cases = (
                ('semantics', (None, False, 0, [], {}, 'visible_grenade', 'throw',
                               'detonation', 'damage', 'ordinary_renderer_visible_contribution ')),
                ('kind', (None, False, 0, [], {}, kind+' ', 'grenade_thrown', 'grenade_presented',
                          'thrown_item_absent', 'large_explosion_presented', 'big_explosion_presented',
                          'grenade_exploded', 'blast_damage', 'melee_impact_presented', 'psi_effect_presented')))
            for field, values in cases:
                for value in values:
                    event = grenade_event(kind)
                    event[field] = value
                    with self.subTest(kind=kind, field=field, value=value), self.assertRaises(Unavailable):
                        validate(page([event]))

    def test_sequence_is_strict_positive_safe_integer_and_contiguous(self):
        for kind in KINDS:
            for value in (None, False, True, 0, -1, 1.0, '1', a.MAX_COUNTER+1):
                obj = page([grenade_event(kind)])
                obj['events'][0]['seq'] = value
                with self.subTest(kind=kind, value=value), self.assertRaises(Unavailable):
                    validate(obj)
            validate(page([grenade_event(kind, a.MAX_COUNTER)], after=a.MAX_COUNTER-1), after=a.MAX_COUNTER-1)
        for second in (1, 3):
            with self.subTest(second=second), self.assertRaises(Unavailable):
                validate(page([grenade_event(KINDS[0], 1), grenade_event(KINDS[1], second)], newest=3))

    def test_independent_reversed_and_repeated_kinds_need_no_causal_pair(self):
        for kinds in ((KINDS[0],), (KINDS[1],), KINDS, tuple(reversed(KINDS)),
                      (KINDS[0], KINDS[0]), (KINDS[1], KINDS[1])):
            events = [grenade_event(kind, seq) for seq, kind in enumerate(kinds, 1)]
            with self.subTest(kinds=kinds):
                self.assertEqual(validate(page(events))['events'], events)

    def test_event_limit_gap_and_partial_page_cursors_remain_exact(self):
        events = [grenade_event(KINDS[i % 2], i+1) for i in range(17)]
        validate(page(events[:16]))
        with self.assertRaises(Unavailable):
            validate(page(events))
        obj = page([grenade_event(KINDS[1], 45)], newest=300)
        self.assertTrue(validate(obj)['gap'])
        self.assertEqual(validate(obj)['next_cursor'], 45)
        for field, value in (('gap', False), ('oldest', 44), ('next_cursor', 300)):
            wrong = copy.deepcopy(obj)
            wrong[field] = value
            with self.subTest(field=field), self.assertRaises(Unavailable):
                validate(wrong)

    def test_duplicate_json_keys_and_packet_overflow_fail_closed(self):
        for kind in KINDS:
            for field in NULL_FIELDS:
                raw = dump(page([grenade_event(kind)]))
                pair = ('"'+field+'":null').encode()
                raw = raw.replace(pair, pair+b','+pair)
                with self.subTest(kind=kind, field=field), self.assertRaises(Unavailable):
                    a.validate_events(raw, CHALLENGE, EPOCH, 0, 16, 'mixed_output')
        with self.assertRaises(Unavailable):
            a.validate_events(b' '*(a.MAX_PACKET+1), CHALLENGE, EPOCH, 0, 16, 'mixed_output')

    def test_all_earlier_wires_including14_reject_in_every_response_envelope(self):
        for version in range(1, 15):
            old = f'openxcom-agent-control/{version}'
            with self.subTest(version=version):
                with self.assertRaises(Unavailable):
                    a.validate_hello(dump(dict(schema=old, mode=a.MODE, epoch=EPOCH,
                                               ledger_capacity=256, command_generation=1, audio_history='mixed_output')))
                with self.assertRaises(Unavailable):
                    a.validate_ready(dump(dict(schema=old, mode=a.MODE, status='ready')))
                obj = state()
                obj['schema'] = old
                with self.assertRaises(Unavailable):
                    a.validate_state(obj, CHALLENGE, EPOCH)
                obj = result()
                obj['schema'] = old
                with self.assertRaises(Unavailable):
                    a.validate_result(dump(obj), COMMAND, EPOCH)
                obj = result(observation=state(challenge=None, capability=None, event_sequence=2))
                obj['observation']['schema'] = old
                with self.assertRaises(Unavailable):
                    a.validate_result(dump(obj), COMMAND, EPOCH)
                for kind in KINDS:
                    obj = page([grenade_event(kind)])
                    obj['schema'] = old
                    with self.assertRaises(Unavailable):
                        validate(obj)

    def test_new_events_cannot_add_authority_or_causality_to_page_envelope(self):
        for kind in KINDS:
            for field, value in (('capability', None), ('actions', []), ('revision', 1),
                                 ('command_id', COMMAND), ('causal_pairs', [])):
                obj = page([grenade_event(kind)])
                obj[field] = value
                with self.subTest(kind=kind, field=field), self.assertRaises(Unavailable):
                    validate(obj)


class GrenadeEventClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs)
        self.addCleanup(native.close)
        return native

    def test_event_read_cannot_initialize_or_accept_other_epoch(self):
        native = self.fixture()
        native.public_events.append(grenade_event())
        with self.assertRaises(Unavailable):
            native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(native.requests, [])
        native.client.observe()
        count = len(native.requests)
        with self.assertRaises(Unavailable):
            native.client.read_events('9'*32, 0, 16)
        self.assertEqual(len(native.requests), count)
        self.assertEqual(native.client.state, 'READY')

    def test_canonical_read_preserves_every_action_context_field(self):
        native = self.fixture()
        observed = native.client.observe()
        native.public_events.extend(grenade_event(kind, seq) for seq, kind in enumerate(KINDS, 1))
        fields = ('state', 'epoch', 'capability', 'revision', 'last_capability', 'capability_deadline',
                  'event_sequence', 'available_actions', 'difficulty_values', 'viewport_bounds',
                  'viewport_samples', 'choice_controls', 'visible_rows', 'scroll_directions',
                  'grid_cells', 'enemy_buttons', 'hands', 'weapon_actions', 'grenade_timers', 'inventory_targets')
        before = {field: getattr(native.client, field) for field in fields}
        with patch('agent_control.secrets.token_hex', return_value=CHALLENGE):
            native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(native.requests[-1], f'V {CHALLENGE} {EPOCH} 0 16'.encode())
        self.assertEqual({field: getattr(native.client, field) for field in fields}, before)
        self.assertEqual(native.client.commands, {})
        self.assertEqual(native.client.execute(**arguments(observed))['status'], 'dispatched')

    def test_event_replay_does_not_refresh_expired_action_grant(self):
        native = self.fixture()
        observed = native.client.observe()
        native.public_events.append(grenade_event(KINDS[1]))
        native.client.capability_deadline = 0.0
        native.client.read_events(EPOCH, 0, 16)
        native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(native.client.capability_deadline, 0.0)
        count = len(native.requests)
        with self.assertRaises(Unavailable):
            native.client.execute(**arguments(observed))
        self.assertEqual(len(native.requests), count)
        self.assertEqual(native.client.commands, {})

    def test_duplicate_command_queries_receipt_only_and_preserves_fresh_grant(self):
        native = self.fixture()
        observed = native.client.observe()
        native.public_events.extend(grenade_event(kind, seq) for seq, kind in enumerate(KINDS, 1))
        native.client.read_events(EPOCH, 0, 16)
        args = arguments(observed)
        receipt = native.client.execute(**args)
        native.client.read_events(EPOCH, 0, 16)
        fresh = native.client.observe()
        self.assertEqual(native.client.execute(**args), receipt)
        self.assertEqual(native.client.capability, fresh['capability'])
        self.assertEqual(native.requests[-1], ('S 1 '+COMMAND).encode())
        self.assertEqual(sum(raw.startswith(b'E ') for raw in native.requests), 1)

    def test_retained_records_detach_and_any_valid_cross_kind_mutation_closes(self):
        for first, later in ((KINDS[0], KINDS[1]), (KINDS[1], KINDS[0]),
                             (KINDS[0], 'projectile_presented'), (KINDS[1], 'impact_effect_presented'),
                             (KINDS[0], 'collapse_animation_presented'), (KINDS[1], 'presentation')):
            with self.subTest(first=first, later=later):
                native = self.fixture()
                native.client.observe()
                native.public_events.append(grenade_event(first))
                returned = native.client.read_events(EPOCH, 0, 16)
                returned['events'][0]['hit'] = True
                returned['events'].append(grenade_event(KINDS[1], 2))
                self.assertEqual(native.client.read_events(EPOCH, 0, 16)['events'], [grenade_event(first)])
                native.public_events[0] = presentation() if later == 'presentation' else grenade_event(later)
                with self.assertRaises(Unavailable):
                    native.client.read_events(EPOCH, 0, 16)
                count = len(native.requests)
                with self.assertRaises(Unavailable):
                    native.client.observe()
                self.assertEqual(native.client.state, 'CLOSED')
                self.assertEqual(len(native.requests), count)

    def test_ring_eviction_partial_pages_and_retained_prefix_replay(self):
        native = self.fixture()
        native.client.observe()
        native.public_events.extend(grenade_event(KINDS[i % 2], i+1) for i in range(300))
        native.event_page_limit = 2
        first = native.client.read_events(EPOCH, 0, 16)
        self.assertTrue(first['gap'])
        self.assertEqual((first['oldest'], first['newest'], first['next_cursor']), (45, 300, 46))
        second = native.client.read_events(EPOCH, first['next_cursor'], 16)
        self.assertFalse(second['gap'])
        self.assertEqual([event['seq'] for event in second['events']], [47, 48])
        self.assertEqual(native.client.read_events(EPOCH, 0, 16)['events'], first['events'])
        native.public_events.append(grenade_event(KINDS[0], 301))
        self.assertEqual(native.client.read_events(EPOCH, 46, 16)['events'], second['events'])
        self.assertLessEqual(len(native.client.public_event_records), 256)

    def test_unsupported_state_event_history_cannot_grant_action_authority(self):
        for kind in KINDS:
            with self.subTest(kind=kind):
                native = self.fixture(screen='unsupported')
                native.public_events.append(grenade_event(kind))
                native.client.observe()
                native.client.read_events(EPOCH, 0, 16)
                self.assertIsNone(native.client.capability)
                self.assertEqual(native.client.commands, {})
                count = len(native.requests)
                with self.assertRaises(Unavailable):
                    native.client.execute(**arguments())
                self.assertEqual(len(native.requests), count)

    def test_malformed_or_lost_event_response_is_terminal_without_action_or_retry(self):
        bad_event = grenade_event(KINDS[1])
        bad_event['source'] = 'grenade'
        malformed = page([bad_event])
        for corrupt in (None, malformed, b'{not-json'):
            with self.subTest(corrupt=corrupt):
                native = self.fixture(transform=lambda op, obj, bad=corrupt: bad if op == 'V' else obj)
                observed = native.client.observe()
                native.public_events.append(grenade_event())
                with patch('agent_control.secrets.token_hex', return_value=CHALLENGE):
                    with self.assertRaises(Unavailable) as caught:
                        native.client.read_events(EPOCH, 0, 16)
                self.assertNotIsInstance(caught.exception, a.ActionOutcomeUnknown)
                count = len(native.requests)
                for operation in (lambda: native.client.execute(**arguments(observed)),
                                  lambda: native.client.read_events(EPOCH, 0, 16), native.client.observe):
                    with self.assertRaises(Unavailable):
                        operation()
                self.assertEqual(native.client.state, 'CLOSED')
                self.assertEqual(len(native.requests), count)
                self.assertFalse(any(raw.startswith(b'E ') for raw in native.requests))

    def test_prior_wire14_hello_is_terminal_before_client_request(self):
        native = self.fixture(hello=dict(schema='openxcom-agent-control/14', mode=a.MODE,
                                        epoch=EPOCH, ledger_capacity=256, command_generation=1, audio_history='mixed_output'))
        with self.assertRaises(Unavailable):
            native.client.observe()
        self.assertEqual(native.requests, [])
        self.assertEqual(native.client.state, 'CLOSED')

    def test_missing_or_malformed_mutating_receipt_is_terminal_unknown_never_replayed(self):
        for corrupt in (None, {'schema': a.SCHEMA, 'kind': KINDS[0]}):
            with self.subTest(corrupt=corrupt):
                native = self.fixture(transform=lambda op, obj, bad=corrupt: bad if op == 'E' else obj)
                observed = native.client.observe()
                native.public_events.extend(grenade_event(kind, seq) for seq, kind in enumerate(KINDS, 1))
                native.client.read_events(EPOCH, 0, 16)
                args = arguments(observed)
                with self.assertRaises(a.ActionOutcomeUnknown) as caught:
                    native.client.execute(**args)
                self.assertEqual(caught.exception.command_id, COMMAND)
                public = caught.exception.public_error()
                self.assertEqual(public['schema'], 'openxcom-mcp-agent-control-error/1')
                self.assertEqual(public['semantics'], 'not_a_native_receipt')
                self.assertFalse(public['retryable'])
                count = len(native.requests)
                for operation in (lambda: native.client.execute(**args), lambda: native.client.read_result(COMMAND),
                                  lambda: native.client.read_events(EPOCH, 0, 16), native.client.observe):
                    with self.assertRaises(Unavailable):
                        operation()
                self.assertEqual(native.client.state, 'CLOSED')
                self.assertEqual(len(native.requests), count)
                self.assertEqual(sum(raw.startswith(b'E ') for raw in native.requests), 1)

    def test_all_four_semantic_tools_remain_text_only_with_new_history(self):
        native = self.fixture()
        server = adapter.Server(agent_control=native.client)
        observed = call(server, 'read_ui_state')['result']
        native.public_events.extend(grenade_event(kind, seq) for seq, kind in enumerate(KINDS, 1))
        history = call(server, 'read_public_events', dict(epoch=EPOCH, after=0, limit=16))['result']
        receipt = call(server, 'execute_ui_action', arguments(observed['structuredContent']))['result']
        status = call(server, 'read_ui_action_result', dict(command_id=COMMAND))['result']
        for reply in (observed, history, receipt, status):
            self.assertFalse(reply['isError'])
            self.assertEqual([item['type'] for item in reply['content']], ['text'])
            self.assertEqual(json.loads(reply['content'][0]['text']), reply['structuredContent'])
        self.assertEqual([event['kind'] for event in history['structuredContent']['events']], list(KINDS))


if __name__ == '__main__':
    unittest.main(verbosity=2)
