# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-19.
"""Wire24 closed outfitting contract. Synthetic sockets do not claim gameplay."""
import copy
import itertools
import json
import unittest
from unittest.mock import patch

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, result, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate

STATES = ('allocate_psi_training', 'soldiers', 'soldier_info', 'crafts', 'craft_info', 'craft_soldiers',
          'craft_equipment', 'craft_armor', 'soldier_armor')
NEW_ACTIONS = ('open_soldiers', 'select_soldier', 'close_soldiers', 'previous_personnel',
               'next_personnel', 'open_soldier_armor', 'close_soldier_info', 'open_crafts',
               'select_craft', 'close_crafts', 'open_craft_crew', 'open_craft_equipment',
               'open_craft_armor', 'close_craft_info', 'set_craft_soldier',
               'open_craft_soldier_info', 'close_craft_crew', 'change_craft_equipment',
               'close_craft_equipment', 'select_craft_armor_soldier', 'close_craft_armor',
               'select_soldier_armor', 'cancel_soldier_armor')
ROWS = {'allocate_psi_training': (4, 4), 'soldiers': (3, 3), 'crafts': (5, 5), 'craft_soldiers': (3, 3),
        'craft_equipment': (2, 3), 'craft_armor': (3, 3), 'soldier_armor': (2, 2)}


def checked(obj):
    return a.validate_state(obj, CHALLENGE, EPOCH)


def event(obj):
    return dict(seq=1, kind='presentation', state=obj['state'], visible=copy.deepcopy(obj['visible']))


def only_control(obj, action):
    """Keep the exact atomic button/action pairs; omit unrelated controls."""
    obj['actions'] = [entry for entry in obj['actions'] if entry['id'] == action]
    button = a.OUTFITTING_BUTTON_ACTIONS[obj['state']].get(action)
    obj['visible']['buttons'] = [entry for entry in obj['visible']['buttons'] if entry['id'] == button]
    if not obj['actions']: obj['capability'] = None
    return obj


def all_unknown(obj):
    v = obj['visible']
    for key, value in list(v.items()):
        if key not in ('headings', 'rows', 'scroll', 'buttons', 'stats', 'weapons'): v[key] = None
    if 'headings' in v: v['headings'] = [None] * len(v['headings'])
    for row in v.get('rows', []): row['cells'] = [None] * len(row['cells'])
    if 'stats' in v: v['stats'] = []
    if 'weapons' in v: v['weapons'] = [dict(name=None, ammo=None), dict(name=None, ammo=None)]
    return obj


def sized_state(size):
    obj = state(screen='crafts')
    obj['visible']['rows'] = [dict(handle=f'r{i}', cells=[''] * 5, selected=False) for i in range(32)]
    remaining = size - len(json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii'))
    if remaining < 0: raise ValueError('Below minimum size')
    for row in obj['visible']['rows']:
        for i in range(5):
            n = min(remaining, 1024); row['cells'][i] = 'x' * n; remaining -= n
    if remaining: raise ValueError('Above field capacity')
    return obj


class OutfittingContractTests(unittest.TestCase):
    def reject_both(self, obj):
        with self.assertRaises(Unavailable): checked(obj)
        with self.assertRaises(Unavailable): validate(page([event(obj)]))

    def test_closed_wire_catalog_and_four_tools(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/33')
        self.assertEqual((len(a.ACTIONS), len(a.STATE_ACTIONS), len(a.definitions())), (188, 81, 5))
        self.assertEqual(set(a.OUTFITTING_FIELDS), set(STATES))
        self.assertEqual(a.OUTFITTING_LISTS, ROWS)
        self.assertEqual(len(NEW_ACTIONS), 23)
        self.assertTrue(set(NEW_ACTIONS) <= set(a.ACTIONS))
        self.assertEqual({x['name'] for x in a.definitions()},
                         {'read_ui_state', 'execute_ui_action', 'read_ui_action_result', 'read_public_events', 'read_public_audio'})
        for version in range(1, 26):
            obj = state(screen='soldiers'); obj['schema'] = f'openxcom-agent-control/{version}'
            with self.subTest(version=version), self.assertRaises(Unavailable): checked(obj)

    def test_all_admitted_exact_shapes_and_all_actions_validate(self):
        for screen in STATES:
            obj = state(screen=screen); expected = copy.deepcopy(obj)
            self.assertEqual(checked(obj), expected)
            self.assertEqual(validate(page([event(obj)]))['events'][0]['visible'], expected['visible'])
            for entry in obj['actions']: a.validate_action_arguments(arguments(obj, action=entry['id']))
        for action in ('open_soldiers', 'open_crafts'):
            a.validate_action_arguments(arguments(state(screen='basescape'), action=action))

    def test_new_actions_rejected_in_every_wrong_state(self):
        for screen, action in itertools.product(a.STATE_ACTIONS, NEW_ACTIONS):
            if action in a.STATE_ACTIONS[screen]: continue
            obj = state(screen=screen)
            obj['actions'].append(dict(id=action, label='Shown', label_source=a.label_source(screen, action),
                                       argument_kind=a.ARGUMENT_KINDS[action]))
            with self.subTest(screen=screen, action=action), self.assertRaises(Unavailable): checked(obj)

    def test_each_required_field_and_no_hidden_values(self):
        for screen in STATES:
            original = state(screen=screen)
            for key in original['visible']:
                obj = copy.deepcopy(original); del obj['visible'][key]
                with self.subTest(screen=screen, missing=key): self.reject_both(obj)
            for key in ('id', 'soldier_id', 'craft_id', 'armor_id', 'item_id', 'eligible', 'max_quantity',
                        'capacity', 'wound_days', 'psi_strength', 'world', 'logical_row', 'total_rows'):
                obj = copy.deepcopy(original); obj['visible'][key] = None
                with self.subTest(screen=screen, extra=key): self.reject_both(obj)

    def test_unknown_all_text_retains_independent_controls(self):
        for screen in STATES:
            obj = all_unknown(state(screen=screen)); expected = copy.deepcopy(obj)
            self.assertEqual(checked(obj), expected)
            self.assertEqual(validate(page([event(obj)]))['events'][0]['visible'], expected['visible'])
        obj = all_unknown(state(screen='craft_equipment'))
        self.assertIn('change_craft_equipment', {x['id'] for x in checked(obj)['actions']})

    def test_null_empty_and_control_markers_remain_distinct(self):
        for screen, (_, columns) in ROWS.items():
            for value in (None, '', '\x01Unknown', 'Not a number', '∞'):
                obj = state(screen=screen); obj['visible']['rows'][0]['cells'] = [value] * columns
                self.assertEqual(checked(obj)['visible']['rows'][0]['cells'], [value] * columns)
        obj = state(screen='soldier_armor'); obj['visible']['rows'][0]['cells'] = ['Personal Armor', None]
        self.assertIsNone(checked(obj)['visible']['rows'][0]['cells'][1])
        obj['visible']['rows'][0]['cells'][1] = ''
        self.assertEqual(checked(obj)['visible']['rows'][0]['cells'][1], '')

    def test_exact_heading_and_row_columns(self):
        for screen, (headings, columns) in ROWS.items():
            for key, values in (('headings', (None, {}, (), [None] * (headings-1), [None] * (headings+1))),
                                ('cells', (None, {}, (), [None] * (columns-1), [None] * (columns+1)))):
                for value in values:
                    obj = state(screen=screen)
                    (obj['visible'] if key == 'headings' else obj['visible']['rows'][0])[key] = value
                    with self.subTest(screen=screen, key=key, value=value): self.reject_both(obj)

    def test_only_strings_or_null_in_every_text_cell(self):
        for screen, (_, columns) in ROWS.items():
            for index, value in itertools.product(range(columns), (0, 1, True, False, 1.5, [], {}, {'text': None})):
                obj = state(screen=screen); obj['visible']['rows'][0]['cells'][index] = value
                with self.subTest(screen=screen, index=index, value=value): self.reject_both(obj)

    def test_32_compact_rows_and_single_painted_selection(self):
        for screen, (_, columns) in ROWS.items():
            obj = state(screen=screen); template = obj['visible']['rows'][0]
            obj['visible']['rows'] = [dict(copy.deepcopy(template), handle=f'r{i}', selected=i==31) for i in range(32)]
            checked(obj)
            for key, value in (('handle', 'r32'), ('handle', 'r00'), ('handle', 'r1'), ('selected', 1), ('selected', None)):
                bad = copy.deepcopy(obj); bad['visible']['rows'][0][key] = value
                with self.subTest(screen=screen, key=key, value=value): self.reject_both(bad)
            bad = copy.deepcopy(obj); bad['visible']['rows'][0]['selected'] = True; self.reject_both(bad)
            obj['visible']['rows'].append(dict(copy.deepcopy(template), handle='r32')); self.reject_both(obj)

    def test_empty_rows_remove_row_authority_only(self):
        for screen in ROWS:
            obj = state(screen=screen); obj['visible']['rows'] = []
            with self.assertRaises(Unavailable): checked(obj)
            obj['actions'] = [x for x in obj['actions'] if x['id'] not in a.OUTFITTING_ROW_ACTIONS[screen]]
            checked(obj)
            self.assertEqual(validate(page([event(obj)]))['events'][0]['visible']['rows'], [])

    def test_row_exact_keys_and_equipment_control_order(self):
        for screen in ROWS:
            for key in ('index', 'bounds', 'name', 'eligible', 'identity', 'amount'):
                obj = state(screen=screen); obj['visible']['rows'][0][key] = None; self.reject_both(obj)
            for key in state(screen=screen)['visible']['rows'][0]:
                obj = state(screen=screen); del obj['visible']['rows'][0][key]; self.reject_both(obj)
        for controls in ([], None, (), {}, ['decrease', 'increase'], ['increase', 'increase'], ['max'], [True]):
            obj = state(screen='craft_equipment'); obj['visible']['rows'][0]['controls'] = controls; self.reject_both(obj)
        for controls in (['increase'], ['decrease'], ['increase', 'decrease']):
            obj = state(screen='craft_equipment'); obj['visible']['rows'][0]['controls'] = controls; checked(obj)

    def test_stats_bounded_nullable_pairs_without_machine_keys(self):
        obj = state(screen='soldier_info'); obj['visible']['stats'] = []; checked(obj)
        obj['visible']['stats'] = [dict(label=None, value='50')] * 11; checked(obj)
        obj['visible']['stats'] = [dict(label='Time Units', value=None)] * 11; checked(obj)
        obj['visible']['stats'] = [dict(label='Repeated', value='0')] * 11; checked(obj)
        obj['visible']['stats'].append(dict(label=None, value=None)); self.reject_both(obj)
        for stat in ({'label': None, 'value': None}, {'label': 'TU'}, {'value': '50'}, {'label': 'TU', 'value': '50', 'id': 'tu'}, None, {},
                     {'label': True, 'value': None}, {'label': None, 'value': 50}):
            obj = state(screen='soldier_info'); obj['visible']['stats'] = [stat]; self.reject_both(obj)

    def test_exact_two_nullable_weapon_pairs(self):
        for weapons in (None, [], [{}], [dict(name=None, ammo=None)] * 3,
                        [dict(name=None, ammo=None, type='weapon'), dict(name=None, ammo=None)]):
            obj = state(screen='craft_info'); obj['visible']['weapons'] = weapons; self.reject_both(obj)
        obj = all_unknown(state(screen='craft_info')); checked(obj)

    def test_text_limits_utf8_bytes_and_surrogates(self):
        for screen in STATES:
            field = 'name' if screen.endswith('_info') else 'title'
            for value in ('x'*1024, 'é'*512, '😀'*256):
                obj = state(screen=screen); obj['visible'][field] = value; checked(obj)
            for value in ('x'*1025, 'é'*513, '😀'*257, '\ud800'):
                obj = state(screen=screen); obj['visible'][field] = value; self.reject_both(obj)
        for location in ('cell', 'heading', 'stat', 'weapon', 'button'):
            obj = state(screen='soldier_info' if location=='stat' else 'craft_info' if location=='weapon' else 'soldiers')
            if location=='cell': obj['visible']['rows'][0]['cells'][0]='é'*513
            elif location=='heading': obj['visible']['headings'][0]='é'*513
            elif location=='stat': obj['visible']['stats'][0]['value']='é'*513
            elif location=='weapon': obj['visible']['weapons'][0]['ammo']='é'*513
            else: obj['visible']['buttons'][0]['label']='é'*513
            self.reject_both(obj)

    def test_button_canonical_order_nonempty_labels_and_exact_keys(self):
        for screen in STATES:
            for value in ('', None, 0, False):
                obj = state(screen=screen); obj['visible']['buttons'][0]['label'] = value; self.reject_both(obj)
            obj = state(screen=screen); obj['visible']['buttons'].append(copy.deepcopy(obj['visible']['buttons'][0])); self.reject_both(obj)
            obj = state(screen=screen); obj['visible']['buttons'][0]['hidden'] = None; self.reject_both(obj)
        for screen in ('soldier_info', 'craft_info'):
            obj = state(screen=screen); obj['visible']['buttons'].reverse(); self.reject_both(obj)

    def test_state_atomic_button_action_pairs_and_event_without_catalog(self):
        for screen in STATES:
            for action, button in a.OUTFITTING_BUTTON_ACTIONS[screen].items():
                obj = state(screen=screen); obj['actions'] = [x for x in obj['actions'] if x['id'] != action]
                with self.assertRaises(Unavailable): checked(obj)
                validate(page([event(obj)]))
                obj = state(screen=screen); obj['visible']['buttons'] = [x for x in obj['visible']['buttons'] if x['id'] != button]
                with self.assertRaises(Unavailable): checked(obj)
                obj['actions'] = [x for x in obj['actions'] if x['id'] != action]
                if not obj['actions']: obj['capability'] = None
                checked(obj)

    def test_exact_descriptor_labels_kinds_and_sources(self):
        for screen in STATES:
            for i in range(len(state(screen=screen)['actions'])):
                for key, value in (('label', 'Invented'), ('label_source', 'hidden'), ('argument_kind', 'none-or-any')):
                    obj = state(screen=screen); obj['actions'][i][key] = value
                    with self.subTest(screen=screen, index=i, key=key), self.assertRaises(Unavailable): checked(obj)
        obj = all_unknown(state(screen='soldiers'))
        row_action = next(x for x in obj['actions'] if x['id'] == 'select_soldier')
        self.assertEqual((row_action['label'], row_action['label_source']), ('Select soldier', 'visible_control_semantics'))

    def test_scroll_is_shown_direction_only(self):
        for screen in ROWS:
            obj = state(screen=screen); obj['visible']['scroll'] = dict(up=False, down=False)
            with self.assertRaises(Unavailable): checked(obj)
            obj['actions'] = [x for x in obj['actions'] if x['id'] != 'scroll_visible_list']; checked(obj)
            for value in (1, None, 'true'):
                bad = copy.deepcopy(obj); bad['visible']['scroll']['up'] = value; self.reject_both(bad)

    def test_state_byte_limit_and_escape_expansion(self):
        obj = sized_state(a.MAX_TEXT_PACKET); checked(obj)
        self.assertEqual(len(json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii')), 32768)
        with self.assertRaises(Unavailable): checked(sized_state(32769))
        obj = state(screen='crafts')
        obj['visible']['rows'] = [dict(handle=f'r{i}', cells=['\x01'*1024]*5, selected=False) for i in range(32)]
        self.reject_both(obj)

    def test_retained_presentation_has_no_action_capability_or_identity(self):
        for screen in STATES:
            obj = all_unknown(state(screen=screen)); ev = event(obj)
            self.assertEqual(validate(page([ev]))['events'][0], ev)
            for key in ('actions', 'capability', 'identity', 'source'):
                bad = dict(ev, **{key: None})
                with self.assertRaises(Unavailable): validate(page([bad]))

    def test_other_table_contracts_do_not_accept_new_unknown_row_optin(self):
        for screen in ('base_stores', 'base_transfers', 'research', 'manufacture', 'new_manufacture'):
            obj = state(screen=screen); obj['visible']['rows'][0]['cells'][0] = None
            with self.subTest(screen=screen), self.assertRaises(Unavailable): checked(obj)


class OutfittingArgumentsTests(unittest.TestCase):
    def test_row_handles_and_desired_assignment_boolean(self):
        for row, assigned in itertools.product(('r0', 'r31'), (False, True)):
            args = arguments(state(screen='craft_soldiers'), action='set_craft_soldier', args=dict(row=row, assigned=assigned))
            a.validate_action_arguments(args)
            self.assertTrue(a.canonical_action_request(args).endswith(f' {row} {str(assigned).lower()}'.encode()))
        for row in ('r32', 'r00', 0, None, 'r-1'):
            with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action='select_soldier', args=dict(row=row)))
        for assigned in (0, 1, 'true', 'false', None, [], {}):
            with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action='set_craft_soldier', args=dict(row='r0', assigned=assigned)))

    def test_equipment_closed_quantity_bounds(self):
        for operation, values in (('adjust', (-1000, -1, 1, 1000)), ('set', (0, 1, 100000))):
            for value in values:
                args = arguments(action='change_craft_equipment', args=dict(row='r31', operation=operation, value=value))
                a.validate_action_arguments(args)
                self.assertTrue(a.canonical_action_request(args).endswith(f' r31 {operation} {value}'.encode()))
        for operation in ('max', 'clear'):
            args = arguments(action='change_craft_equipment', args=dict(row='r0', operation=operation))
            a.validate_action_arguments(args); self.assertTrue(a.canonical_action_request(args).endswith(f' r0 {operation}'.encode()))
        for operation, values in (('adjust', (-1001, 0, 1001, True, False, 1.0, None)), ('set', (-1, 100001, True, 0.0, None))):
            for value in values:
                with self.subTest(operation=operation, value=value), self.assertRaises(Unavailable):
                    a.validate_action_arguments(arguments(action='change_craft_equipment', args=dict(row='r0', operation=operation, value=value)))

    def test_no_gesture_hidden_values_or_extra_quantity_keys(self):
        samples = [('select_soldier', dict(row='r0')), ('set_craft_soldier', dict(row='r0', assigned=True)),
                   ('change_craft_equipment', dict(row='r0', operation='max')), ('close_craft_info', {})]
        for action, args in samples:
            for key in ('id', 'name', 'duration', 'gesture', 'input', 'eligible', 'max_quantity', 'value' if action=='change_craft_equipment' else 'unknown'):
                with self.subTest(action=action, key=key), self.assertRaises(Unavailable):
                    a.validate_action_arguments(arguments(action=action, args=dict(args, **{key: 1})))
        for operation in ('hold', 'increase', 'decrease', 'toggle', 'all', None, True):
            with self.assertRaises(Unavailable):
                a.validate_action_arguments(arguments(action='change_craft_equipment', args=dict(row='r0', operation=operation)))

    def test_tool_schema_closed_oneof_matches_arguments(self):
        execute = next(x for x in a.definitions() if x['name'] == 'execute_ui_action')['inputSchema']
        encoded = json.dumps(execute)
        self.assertIn('crew_assignment' if 'crew_assignment' in encoded else 'assigned', encoded)
        for action in NEW_ACTIONS: self.assertIn('"'+action+'"', encoded)
        for forbidden in ('"duration"', '"gesture"', '"item_id"'): self.assertNotIn(forbidden, encoded)
        self.assertFalse(execute['additionalProperties'])


class OutfittingClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        peer = NativeFixture(**kwargs); self.addCleanup(peer.close); return peer

    def test_all23_actions_actual_client_server_path(self):
        for action in NEW_ACTIONS:
            screen = next(screen for screen in ('basescape',) + STATES if action in a.STATE_ACTIONS[screen])
            peer = self.fixture(screen=screen); server = adapter.Server(agent_control=peer.client)
            obj = call(server, 'read_ui_state')['result']['structuredContent']
            reply = call(server, 'execute_ui_action', arguments(obj, action=action))['result']['structuredContent']
            self.assertEqual((reply['status'], reply['semantics']), ('dispatched', 'dispatch_not_success'))
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests), 1)
            self.assertFalse(peer.client.outfitting_rows or peer.client.equipment_rows)

    def test_unknown_rows_execute_without_name_or_numeric_inference(self):
        for screen, action in (('soldiers', 'select_soldier'), ('craft_soldiers', 'set_craft_soldier'),
                               ('craft_equipment', 'change_craft_equipment'), ('soldier_armor', 'select_soldier_armor')):
            peer = self.fixture(screen=screen, transform=lambda op,obj: all_unknown(obj) if op=='R' else obj)
            obj = peer.client.observe(); reply = peer.client.execute(**arguments(obj, action=action))
            self.assertEqual(reply['status'], 'dispatched')
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests), 1)

    def test_equipment_operations_use_only_fresh_direction_authority(self):
        variants = [(dict(operation='adjust', value=1), {'increase'}), (dict(operation='adjust', value=-1), {'decrease'}),
                    (dict(operation='max'), {'increase'}), (dict(operation='clear'), {'decrease'}),
                    (dict(operation='set', value=0), {'increase', 'decrease'})]
        for directions, (variant, required) in itertools.product((['increase'], ['decrease'], ['increase','decrease']), variants):
            def transform(op, obj):
                if op=='R': obj['visible']['rows'][0].update(cells=[None]*3, controls=directions)
                return obj
            peer = self.fixture(screen='craft_equipment', transform=transform); obj = peer.client.observe()
            args = arguments(obj, action='change_craft_equipment', args=dict(row='r0', **variant))
            if required <= set(directions): self.assertEqual(peer.client.execute(**args)['status'], 'dispatched')
            else:
                with self.assertRaises(Unavailable): peer.client.execute(**args)
                self.assertEqual(peer.client.capability, obj['capability'])
                self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))

    def test_crew_desired_false_and_true_are_explicit_wire_values(self):
        for assigned in (False, True):
            peer = self.fixture(screen='craft_soldiers'); obj = peer.client.observe()
            peer.client.execute(**arguments(obj, action='set_craft_soldier', args=dict(row='r0', assigned=assigned)))
            request = next(x for x in peer.requests if x.startswith(b'E '))
            self.assertTrue(request.endswith(f' r0 {str(assigned).lower()}'.encode()))

    def test_unoffered_row_old_grant_and_deadline_fail_before_send(self):
        for screen, action in (('soldiers','select_soldier'), ('craft_soldiers','set_craft_soldier'), ('craft_equipment','change_craft_equipment')):
            peer = self.fixture(screen=screen); old = peer.client.observe(); fresh = peer.client.observe()
            with self.assertRaises(Unavailable): peer.client.execute(**arguments(old, action=action))
            args = arguments(fresh, action=action); args['args']['row'] = 'r31'
            with self.assertRaises(Unavailable): peer.client.execute(**args)
            peer.client.capability_deadline = 0
            with self.assertRaises(Unavailable): peer.client.execute(**arguments(fresh, action=action))
            self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
            self.assertFalse(peer.client.commands)

    def test_cache_immutable_and_cleared_across_contexts(self):
        peer = self.fixture(screen='craft_equipment'); obj = peer.client.observe()
        self.assertIsInstance(peer.client.outfitting_rows, frozenset); self.assertIsInstance(peer.client.equipment_rows, frozenset)
        obj['visible']['rows'][0]['controls'].clear(); obj['visible']['rows'][0]['handle'] = 'r31'
        self.assertEqual(peer.client.outfitting_rows, frozenset({'r0'})); self.assertIn(('r0','increase'), peer.client.equipment_rows)
        peer.screen = 'craft_soldiers'; peer.client.observe(); self.assertFalse(peer.client.equipment_rows)
        peer.screen = 'unsupported'; peer.client.observe(); self.assertFalse(peer.client.outfitting_rows)
        peer.screen = 'craft_equipment'; peer.client.observe(); peer.client.close()
        self.assertFalse(peer.client.outfitting_rows or peer.client.equipment_rows)

    def test_duplicate_status_only_and_changed_args_conflict(self):
        for screen, action in (('craft_soldiers','set_craft_soldier'), ('craft_equipment','change_craft_equipment')):
            peer = self.fixture(screen=screen); obj = peer.client.observe(); args = arguments(obj, action=action)
            first = peer.client.execute(**args); self.assertEqual(peer.client.execute(**args), first)
            bad = copy.deepcopy(args)
            if action=='set_craft_soldier': bad['args']['assigned'] = False
            else: bad['args']['value'] = 2
            with self.assertRaises(Unavailable): peer.client.execute(**bad)
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests), 1)
            self.assertEqual(sum(x.startswith(b'S ') for x in peer.requests), 1)

    def test_events_are_grant_neutral_and_cannot_change_cached_rows(self):
        peer = self.fixture(screen='craft_soldiers'); obj = peer.client.observe()
        peer.public_events = [event(all_unknown(state(screen='craft_equipment')))]
        before = (peer.client.capability, peer.client.revision, peer.client.outfitting_rows, peer.client.equipment_rows)
        first = peer.client.read_events(EPOCH,0,16); self.assertEqual(peer.client.read_events(EPOCH,0,16)['events'], first['events'])
        self.assertEqual(before, (peer.client.capability, peer.client.revision, peer.client.outfitting_rows, peer.client.equipment_rows))
        peer.public_events[0]['visible']['rows'][0]['cells'][0] = 'Changed'
        with self.assertRaises(Unavailable): peer.client.read_events(EPOCH,0,16)
        self.assertEqual(peer.client.state, 'CLOSED')

    def test_known_unavailable_is_terminal_and_does_not_replay(self):
        # Native stale contexts are reported as unavailable, never a new status.
        for status in ('unavailable',):
            def transform(op,obj):
                if op in ('E','S'): obj.update(status=status, observation=None)
                return obj
            peer = self.fixture(screen='craft_equipment', transform=transform); obj = peer.client.observe()
            args = arguments(obj, action='change_craft_equipment')
            self.assertEqual(peer.client.execute(**args)['status'], status)
            self.assertEqual(peer.client.execute(**args)['status'], status)
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests), 1)

    def test_unknown_transport_and_pending_fail_closed(self):
        for mode in ('silent','pending','unknown_after_dispatch'):
            def transform(op,obj):
                if op in ('E','S'):
                    if mode=='silent': return None
                    obj.update(status=mode, observation=None)
                return obj
            peer = self.fixture(screen='craft_soldiers', transform=transform); obj = peer.client.observe()
            args = arguments(obj, action='set_craft_soldier')
            if mode=='unknown_after_dispatch':
                self.assertEqual(peer.client.execute(**args)['status'],mode)
                self.assertEqual(peer.client.execute(**args)['status'],mode)
            else:
                with self.assertRaises(a.ActionOutcomeUnknown): peer.client.execute(**args)
                with self.assertRaises(Unavailable): peer.client.execute(**args)
            with self.assertRaises(Unavailable): peer.client.execute(**dict(args,command_id='9'*32))
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)

    def test_reconnect_requires_new_observation_not_old_handles(self):
        old = self.fixture(screen='craft_equipment'); obj = old.client.observe(); old.client.close()
        new = self.fixture(screen='craft_equipment')
        with self.assertRaises(Unavailable): new.client.execute(**arguments(obj,action='change_craft_equipment'))
        self.assertFalse(any(x.startswith(b'E ') for x in new.requests))
        fresh = new.client.observe(); new.client.execute(**arguments(fresh,action='change_craft_equipment'))
        self.assertEqual(sum(x.startswith(b'E ') for x in new.requests),1)

    def test_actual_server_uncertainty_remains_nonreceipt(self):
        peer = self.fixture(screen='craft_equipment', transform=lambda op,obj: None if op=='E' else obj)
        server = adapter.Server(agent_control=peer.client); obj = call(server,'read_ui_state')['result']['structuredContent']
        reply = call(server,'execute_ui_action',arguments(obj,action='change_craft_equipment'))['result']
        self.assertTrue(reply['isError'])
        self.assertEqual((reply['structuredContent']['status'],reply['structuredContent']['retryable']),('outcome_unknown',False))


if __name__ == '__main__': unittest.main()
