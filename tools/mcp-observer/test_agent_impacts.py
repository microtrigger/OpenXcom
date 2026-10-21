# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
"""Synthetic /10 wire/transport tests; no native raster or live effect proof."""
import copy
import json
import unittest

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, arguments, call, dump, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate, presentation, audio
from test_agent_projectiles import projectile
from test_agent_reports import message

KINDS = ('impact_effect_presented', 'collapse_animation_presented')
NULL_FIELDS = ('source', 'actor', 'faction', 'world_position', 'hit', 'damage')


def effect(kind, seq=1):
    event = projectile(seq)
    event['kind'] = kind
    return event


class ImpactValidationTests(unittest.TestCase):
    def test_protocol_catalog_and_all_eight_kinds(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/25')
        self.assertIn('protocol /24:', a.definitions()[0]['description'])
        self.assertEqual((len(a.ACTIONS), len(a.STATE_ACTIONS), len(a.definitions())), (131, 55, 4))
        self.assertEqual((a.MAX_COMMANDS, a.EVENT_CAPACITY, a.MAX_EVENTS), (256, 256, 16))
        self.assertEqual((a.MAX_REQUEST, a.MAX_TEXT_PACKET, a.MAX_PACKET), (512, 32768, 49152))
        events = [presentation(1), message(seq=2), audio(3),
                  dict(seq=4, kind='audio_status', status='active', partial_window_discarded=False),
                  dict(seq=5, kind='audio_gap', dropped_windows=1), projectile(6),
                  effect(KINDS[0], 7), effect(KINDS[1], 8)]
        self.assertEqual(validate(page(events))['events'], events)
        self.assertEqual(validate(page(events[5:], after=5), after=5)['next_cursor'], 8)

    def test_effects_do_not_require_audio_capability(self):
        for kind in KINDS:
            for history in ('mixed_output', 'unavailable'):
                event = effect(kind)
                with self.subTest(kind=kind, history=history):
                    self.assertEqual(validate(page([event], history=history), history=history)['events'], [event])

    def test_each_nine_key_field_is_required(self):
        for kind in KINDS:
            self.assertEqual(len(effect(kind)), 9)
            for key in effect(kind):
                obj = page([effect(kind)])
                del obj['events'][0][key]
                with self.subTest(kind=kind, missing=key), self.assertRaises(Unavailable): validate(obj)

    def test_extra_attribution_outcome_and_authority_fields_reject(self):
        for kind in KINDS:
            for key, value in (('death', None), ('outcome', None), ('status', 'presented'),
                               ('location', None), ('type', 'humanoid'), ('target', None),
                               ('resource_id', 1), ('count', 1), ('timestamp', 0),
                               ('episode_id', 'p0'), ('actions', []), ('capability', '2'*32)):
                event = effect(kind); event[key] = value
                with self.subTest(kind=kind, extra=key), self.assertRaises(Unavailable): validate(page([event]))

    def test_six_nulls_reject_all_nonnull_types(self):
        for kind in KINDS:
            for key in NULL_FIELDS:
                for value in (False, True, 0, 1, 0.0, '', 'unknown', [], {}, {'x': 0}):
                    event = effect(kind); event[key] = value
                    with self.subTest(kind=kind, key=key, value=value), self.assertRaises(Unavailable):
                        validate(page([event]))

    def test_kind_and_semantics_are_literal_closed_values(self):
        for kind in KINDS:
            for key, values in (
                ('kind', (None, False, 0, [], kind+' ', 'impact', 'death', 'collapse',
                          'big_explosion_presented', 'melee_impact_presented', 'psi_effect_presented')),
                ('semantics', (None, False, 0, {}, 'visible_effect',
                               'ordinary_renderer_visible_contribution ', 'hit', 'death', 'damage'))):
                for value in values:
                    event = effect(kind); event[key] = value
                    with self.subTest(kind=kind, key=key, value=value), self.assertRaises(Unavailable):
                        validate(page([event]))

    def test_sequence_strict_safe_integer_and_contiguity(self):
        for kind in KINDS:
            for value in (None, False, True, 0, -1, 1.0, '1', a.MAX_COUNTER+1):
                obj = page([effect(kind)]); obj['events'][0]['seq'] = value
                with self.subTest(kind=kind, value=value), self.assertRaises(Unavailable): validate(obj)
            validate(page([effect(kind, a.MAX_COUNTER)], after=a.MAX_COUNTER-1), after=a.MAX_COUNTER-1)
        for second in (1, 3):
            with self.assertRaises(Unavailable):
                validate(page([effect(KINDS[0], 1), effect(KINDS[1], second)], newest=3))

    def test_order_allows_independent_roles_without_inventing_causal_pair(self):
        for kinds in (KINDS, tuple(reversed(KINDS)), (KINDS[0], KINDS[0]), (KINDS[1], KINDS[1])):
            events = [effect(kind, i+1) for i, kind in enumerate(kinds)]
            self.assertEqual(validate(page(events))['events'], events)

    def test_retention_gap_partial_pages_and_limit_remain_closed(self):
        events = [effect(KINDS[i % 2], i+1) for i in range(17)]
        validate(page(events[:16]))
        with self.assertRaises(Unavailable): validate(page(events))
        obj = page([effect(KINDS[0], 45)], newest=300)
        self.assertTrue(validate(obj)['gap']); self.assertEqual(validate(obj)['next_cursor'], 45)
        for key, value in (('gap', False), ('oldest', 44), ('next_cursor', 300), ('capability', None)):
            wrong = copy.deepcopy(obj); wrong[key] = value
            with self.subTest(key=key), self.assertRaises(Unavailable): validate(wrong)

    def test_duplicate_json_keys_and_packet_budget_fail_closed(self):
        for kind in KINDS:
            raw = dump(page([effect(kind)])).replace(b'"damage":null', b'"damage":null,"damage":null')
            with self.assertRaises(Unavailable): a.validate_events(raw, CHALLENGE, EPOCH, 0, 16, 'mixed_output')
            with self.assertRaises(Unavailable): a.validate_events(b' '*(a.MAX_PACKET+1), CHALLENGE, EPOCH, 0, 16, 'mixed_output')

    def test_current_wire_never_accepts_v9_envelope_with_new_kind(self):
        for kind in KINDS:
            for version in range(1, 10):
                obj = page([effect(kind)]); obj['schema'] = f'openxcom-agent-control/{version}'
                with self.subTest(kind=kind, version=version), self.assertRaises(Unavailable): validate(obj)

    def test_prose_does_not_claim_hits_deaths_or_source(self):
        prose = next(x for x in a.definitions() if x['name'] == 'read_public_events')['description']
        for text in (*KINDS, 'remain null', 'never a death or damage claim', 'no attack counter',
                     'big explosions and melee/psi', 'no shared video clock'):
            self.assertIn(text, prose)


class ImpactClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs); self.addCleanup(native.close); return native

    def test_v_and_replays_preserve_grant_and_exact_duplicate_uses_status_only(self):
        native = self.fixture(); observed = native.client.observe()
        native.public_events.extend(effect(kind, i+1) for i, kind in enumerate(KINDS))
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
        self.assertEqual(native.requests[-1], ('S '+COMMAND).encode())
        self.assertEqual(sum(raw.startswith(b'E ') for raw in native.requests), 1)

    def test_returned_records_detached_and_cross_kind_mutation_terminal(self):
        for first, later in ((KINDS[0], KINDS[1]), (KINDS[1], KINDS[0]),
                             (KINDS[0], 'projectile_presented'), (KINDS[1], 'presentation')):
            with self.subTest(first=first, later=later):
                native = self.fixture(); native.client.observe(); native.public_events.append(effect(first))
                returned = native.client.read_events(EPOCH, 0, 16); returned['events'][0]['hit'] = True
                self.assertEqual(native.client.read_events(EPOCH, 0, 16)['events'], [effect(first)])
                native.public_events[0] = presentation() if later == 'presentation' else effect(later)
                with self.assertRaises(Unavailable): native.client.read_events(EPOCH, 0, 16)
                count = len(native.requests)
                with self.assertRaises(Unavailable): native.client.observe()
                self.assertEqual(native.client.state, 'CLOSED'); self.assertEqual(len(native.requests), count)

    def test_unsupported_state_and_event_cannot_grant_authority(self):
        for kind in KINDS:
            native = self.fixture(screen='unsupported'); native.public_events.append(effect(kind))
            native.client.observe(); native.client.read_events(EPOCH, 0, 16)
            self.assertIsNone(native.client.capability); self.assertEqual(native.client.commands, {})
            count = len(native.requests)
            with self.assertRaises(Unavailable): native.client.execute(**arguments())
            self.assertEqual(len(native.requests), count)

    def test_malformed_effect_closes_read_without_sending_action(self):
        for kind in KINDS:
            native = self.fixture(); observed = native.client.observe()
            event = effect(kind); event['damage'] = 0; native.public_events.append(event)
            with self.assertRaises(Unavailable) as caught: native.client.read_events(EPOCH, 0, 16)
            self.assertNotIsInstance(caught.exception, a.ActionOutcomeUnknown)
            count = len(native.requests)
            with self.assertRaises(Unavailable): native.client.execute(**arguments(observed))
            self.assertEqual(len(native.requests), count); self.assertEqual(native.client.state, 'CLOSED')
            self.assertFalse(any(raw.startswith(b'E ') for raw in native.requests))

    def test_prior_v9_hello_closes_before_client_request(self):
        native = self.fixture(hello=dict(schema='openxcom-agent-control/9', mode=a.MODE,
            epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output'))
        with self.assertRaises(Unavailable): native.client.observe()
        self.assertEqual(native.requests, []); self.assertEqual(native.client.state, 'CLOSED')

    def test_missing_or_malformed_mutating_receipt_is_unknown_never_replayed(self):
        for corrupt in (None, {'schema': a.SCHEMA, 'kind': KINDS[0]}):
            native = self.fixture(transform=lambda op, obj, bad=corrupt: bad if op == 'E' else obj)
            observed = native.client.observe(); native.public_events.append(effect(KINDS[1]))
            native.client.read_events(EPOCH, 0, 16); args = arguments(observed)
            with self.assertRaises(a.ActionOutcomeUnknown) as caught: native.client.execute(**args)
            self.assertEqual(caught.exception.command_id, COMMAND)
            public = caught.exception.public_error()
            self.assertEqual(public['schema'], 'openxcom-mcp-agent-control-error/1')
            self.assertEqual(public['semantics'], 'not_a_native_receipt'); self.assertFalse(public['retryable'])
            count = len(native.requests)
            for operation in (lambda: native.client.execute(**args), lambda: native.client.read_result(COMMAND),
                              lambda: native.client.read_events(EPOCH, 0, 16), native.client.observe):
                with self.assertRaises(Unavailable): operation()
            self.assertEqual(len(native.requests), count)
            self.assertEqual(sum(raw.startswith(b'E ') for raw in native.requests), 1)

    def test_all_semantic_tools_stay_text_only(self):
        native = self.fixture(); server = adapter.Server(agent_control=native.client)
        observed = call(server, 'read_ui_state')['result']
        native.public_events.extend(effect(kind, i+1) for i, kind in enumerate(KINDS))
        history = call(server, 'read_public_events', dict(epoch=EPOCH, after=0, limit=16))['result']
        receipt = call(server, 'execute_ui_action', arguments(observed['structuredContent']))['result']
        status = call(server, 'read_ui_action_result', dict(command_id=COMMAND))['result']
        for reply in (observed, history, receipt, status):
            self.assertFalse(reply['isError']); self.assertEqual([item['type'] for item in reply['content']], ['text'])
            self.assertEqual(json.loads(reply['content'][0]['text']), reply['structuredContent'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
