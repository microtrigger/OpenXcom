# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
"""Synthetic /7 HUD/grid/weapon contracts; no actual movement/fire proof."""
import copy
import json
import unittest

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate
from test_agent_reports import message

NEW_KINDS = {'map_primary': 'grid_cell', 'map_secondary': 'grid_cell',
             'center_selected_unit': 'none', 'center_enemy': 'enemy_button',
             'open_hand_action_menu': 'hand', 'choose_fire_mode': 'fire_mode',
             'cancel_weapon_action': 'none'}


class GameplayValidationTests(unittest.TestCase):
    def test_closed_counts_kinds_and_state_action_matrix(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/7')
        self.assertEqual(len(a.ACTIONS), 38); self.assertEqual(len(a.STATE_ACTIONS), 22)
        self.assertEqual({key: a.ARGUMENT_KINDS[key] for key in NEW_KINDS}, NEW_KINDS)
        self.assertEqual(a.STATE_ACTIONS['weapon_action_menu'], {'choose_fire_mode', 'cancel_weapon_action'})
        self.assertEqual(a.STATE_ACTIONS['battlescape_warning'], frozenset())
        self.assertNotIn('cancel_weapon_action', a.STATE_ACTIONS['battlescape'])
        for screen in ('battlescape', 'weapon_action_menu', 'battlescape_warning'):
            a.validate_state(state(screen=screen), CHALLENGE, EPOCH)

    def test_exact_canonical_suffixes_no_pixel_or_model_arguments(self):
        cases = [('map_primary', {'cell': 'g127'}, ' g127'), ('map_secondary', {'cell': 'g0'}, ' g0'),
                 ('center_enemy', {'button': 'e9'}, ' e9'), ('center_selected_unit', {}, ''),
                 ('open_hand_action_menu', {'hand': 'right'}, ' right'),
                 ('choose_fire_mode', {'row': 'a5'}, ' a5'), ('cancel_weapon_action', {}, '')]
        for action, args, suffix in cases:
            obj = arguments(action=action, args=args)
            self.assertEqual(a.canonical_action_request(obj),
                             f'E {COMMAND} {EPOCH} {obj["capability"]} 1 {action}{suffix}'.encode())
            for extra in ('x', 'y', 'world_id', 'unit_id', 'mode', 'cost', 'target'):
                with self.subTest(action=action, extra=extra), self.assertRaises(Unavailable):
                    a.validate_action_arguments(dict(obj, args=dict(args, **{extra: 1})))

    def test_strict_local_handle_and_hand_enums(self):
        for action, key, bad_values in [('map_primary', 'cell', ('g128', 'g-1', 'g00', 'r0', 'G0', 0, True, None)),
                ('map_secondary', 'cell', ('e0', 'g1 ', 'g0\n')),
                ('center_enemy', 'button', ('e10', 'e00', 'g0', 1, True)),
                ('choose_fire_mode', 'row', ('a6', 'a00', 'r0', 0, False)),
                ('open_hand_action_menu', 'hand', ('LEFT', 'primary', '', 0, True))]:
            for value in bad_values:
                with self.subTest(action=action, value=value), self.assertRaises(Unavailable):
                    a.validate_action_arguments(arguments(action=action, args={key: value}))

    def test_visible_hands_unique_and_uint_ammo_without_item_metadata(self):
        for value in (None, 0, 2**32-1):
            obj = state(); obj['visible']['hands'][0]['ammo'] = value
            a.validate_state(obj, CHALLENGE, EPOCH)
        for value in (-1, 2**32, True, 3.0, '6', {}):
            obj = state(); obj['visible']['hands'][0]['ammo'] = value
            with self.subTest(value=value), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        for change in ('duplicate', 'extra', 'unknown'):
            obj = state()
            if change == 'duplicate': obj['visible']['hands'][1]['hand'] = 'left'
            elif change == 'extra': obj['visible']['hands'][0]['item_id'] = 'STR_RIFLE'
            else: obj['visible']['hands'][0]['hand'] = 'belt'
            with self.subTest(change=change), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_painted_enemy_buttons_sequential_unique_numbers_not_actor_ids(self):
        for button in (dict(handle='e1', number=1), dict(handle='e0', number=0),
                       dict(handle='e0', number=11), dict(handle='e0', number=True),
                       dict(handle='e0', number=1, unit_id=3)):
            obj = state(); obj['visible']['enemy_buttons'] = [button]
            with self.subTest(button=button), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        obj = state(); obj['visible']['enemy_buttons'] += [dict(handle='e1', number=1)]
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        obj['visible']['enemy_buttons'][1]['number'] = 10; a.validate_state(obj, CHALLENGE, EPOCH)

    def test_focus_is_only_a_current_grid_handle_and_never_target_identity(self):
        for value in ('g2', 'e0', {'x': 1, 'y': 1}, 0, True):
            obj = state(); obj['visible']['focus_target'] = value
            with self.subTest(value=value), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        obj = state(); obj['visible']['focus_target'] = None; a.validate_state(obj, CHALLENGE, EPOCH)
        obj = state(); obj['visible']['target_unit'] = 'hidden'
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_action_catalog_requires_the_corresponding_visible_set(self):
        for field, actions in [('hands', {'open_hand_action_menu'}),
                               ('enemy_buttons', {'center_enemy'}), ('grid', {'map_primary', 'map_secondary'})]:
            obj = state()
            if field == 'grid': obj['visible']['tactical']['grid'] = []; obj['visible']['focus_target'] = None
            else: obj['visible'][field] = []
            with self.subTest(field=field), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
            obj['actions'] = [item for item in obj['actions'] if item['id'] not in actions]
            a.validate_state(obj, CHALLENGE, EPOCH)

    def test_weapon_rows_have_exact_visible_nullable_text_fields(self):
        obj = state(screen='weapon_action_menu')
        a.validate_state(obj, CHALLENGE, EPOCH)
        for key, value in [('handle', 'a1'), ('label', ''), ('accuracy', 60),
                           ('time_units', 12), ('ammo_cost', 3), ('mode', 'aimed')]:
            bad = copy.deepcopy(obj); bad['visible']['rows'][0][key] = value
            with self.subTest(key=key), self.assertRaises(Unavailable): a.validate_state(bad, CHALLENGE, EPOCH)
        obj['visible']['rows'] = []; obj['actions'] = [x for x in obj['actions'] if x['id'] == 'cancel_weapon_action']
        a.validate_state(obj, CHALLENGE, EPOCH)

    def test_weapon_descriptor_is_first_actual_row_text_with_no_peer_defined_kind(self):
        for key, value in [('label', 'Generic fire'), ('label_source', 'visible_control_semantics'), ('argument_kind', 'point')]:
            obj = state(screen='weapon_action_menu')
            next(item for item in obj['actions'] if item['id'] == 'choose_fire_mode')[key] = value
            with self.subTest(key=key), self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        obj = state(screen='weapon_action_menu'); obj['actions'][0]['id'] = 'map_primary'
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)

    def test_warning_is_visible_text_without_action_authority_or_hidden_reason(self):
        for text in (None, 'Not enough time units!', 'No ammunition loaded!'):
            obj = state(screen='battlescape_warning'); obj['visible']['message'] = text
            self.assertIsNone(obj['capability']); self.assertEqual(obj['actions'], [])
            a.validate_state(obj, CHALLENGE, EPOCH)
            if text is not None: validate(page([message(text, None)]))
        obj = state(screen='battlescape_warning'); obj['visible']['reason'] = 'insufficient_tu'
        with self.assertRaises(Unavailable): a.validate_state(obj, CHALLENGE, EPOCH)
        # Identical visible text is classified identically regardless of private cause.
        validate(page([message('Jane\nhas Panicked', 'panic')]))


class GameplayClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = NativeFixture(**kwargs); self.addCleanup(native.close); return native

    def test_unseen_handles_rejected_before_grant_or_ledger_consumption(self):
        for screen, action, bad_args in [('battlescape', 'map_primary', {'cell': 'g127'}),
                ('battlescape', 'map_secondary', {'cell': 'g127'}),
                ('battlescape', 'center_enemy', {'button': 'e9'}),
                ('weapon_action_menu', 'choose_fire_mode', {'row': 'a5'})]:
            native = self.fixture(screen=screen); observed = native.client.observe(); count = len(native.requests)
            with self.subTest(action=action), self.assertRaises(Unavailable):
                native.client.execute(**arguments(observed, action=action, args=bad_args))
            self.assertEqual(len(native.requests), count); self.assertEqual(native.client.commands, {})
            self.assertEqual(native.client.capability, observed['capability'])

    def test_mutating_returned_dto_cannot_add_handle_authority(self):
        for screen, action, field, entry, bad_args in [
                ('battlescape', 'center_enemy', 'enemy_buttons', dict(handle='e1', number=2), {'button': 'e1'}),
                ('weapon_action_menu', 'choose_fire_mode', 'rows', dict(handle='a2', label='Fake', accuracy=None, time_units=None), {'row': 'a2'})]:
            native = self.fixture(screen=screen); observed = native.client.observe(); observed['visible'][field].append(entry)
            count = len(native.requests)
            with self.assertRaises(Unavailable): native.client.execute(**arguments(observed, action=action, args=bad_args))
            self.assertEqual(len(native.requests), count)
        native = self.fixture(); observed = native.client.observe()
        observed['visible']['tactical']['grid'].append(dict(handle='g2'))
        with self.assertRaises(Unavailable): native.client.execute(**arguments(observed, action='map_primary', args={'cell':'g2'}))

    def test_hidden_hand_and_event_only_handles_cannot_broaden_action_grant(self):
        def transform(operation, value):
            if operation == 'R':
                value['visible']['hands'] = [dict(hand='right', ammo=None)]
            return value
        native = self.fixture(transform=transform); observed = native.client.observe()
        observed['visible']['hands'].append(dict(hand='left', ammo=99))
        count = len(native.requests)
        with self.assertRaises(Unavailable):
            native.client.execute(**arguments(observed, action='open_hand_action_menu', args={'hand':'left'}))
        self.assertEqual(len(native.requests), count)
        projection = state()['visible']; extra = copy.deepcopy(projection['tactical']['grid'][0]); extra['handle'] = 'g2'
        projection['tactical']['grid'].append(extra)
        native.public_events.append(dict(seq=1, kind='presentation', state='battlescape', visible=projection))
        native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(native.client.hands, {'right'}); self.assertEqual(native.client.grid_cells, {'g0','g1'})
        with self.assertRaises(Unavailable):
            native.client.execute(**arguments(observed, action='map_primary', args={'cell':'g2'}))
        self.assertEqual(native.client.commands, {})

    def test_stale_same_local_handle_and_cross_state_caches_do_not_authorize(self):
        native = self.fixture(); old = native.client.observe(); fresh = native.client.observe(); count = len(native.requests)
        with self.assertRaises(Unavailable): native.client.execute(**arguments(old, action='map_primary'))
        self.assertEqual(len(native.requests), count)
        native.screen = 'weapon_action_menu'; menu = native.client.observe()
        self.assertEqual(native.client.grid_cells, frozenset()); self.assertEqual(native.client.enemy_buttons, frozenset())
        self.assertEqual(native.client.hands, frozenset()); self.assertEqual(native.client.fire_modes, {'a0', 'a1'})
        with self.assertRaises(Unavailable): native.client.execute(**arguments(menu, action='map_primary'))
        native.screen = 'battlescape_warning'; native.client.observe()
        self.assertEqual(native.client.fire_modes, frozenset()); self.assertIsNone(native.client.capability)

    def test_all_new_actions_canonical_dispatch_and_duplicate_lookup_only(self):
        for index, (action, _) in enumerate(NEW_KINDS.items()):
            screen = 'weapon_action_menu' if action in ('choose_fire_mode', 'cancel_weapon_action') else 'battlescape'
            native = self.fixture(screen=screen); observed = native.client.observe()
            args = arguments(observed, action=action, command_id=f'{index+1:032x}')
            receipt = native.client.execute(**args)
            self.assertEqual(native.requests[-1], a.canonical_action_request(args))
            self.assertEqual(native.client.grid_cells, frozenset()); self.assertEqual(native.client.fire_modes, frozenset())
            fresh = native.client.observe(); self.assertEqual(native.client.execute(**args), receipt)
            self.assertEqual(native.requests[-1], ('S ' + args['command_id']).encode())
            self.assertEqual(native.client.capability, fresh['capability'])
            self.assertEqual(sum(packet.startswith(b'E ') for packet in native.requests), 1)

    def test_changed_tactical_args_conflict_without_replay(self):
        for action, changed in [('map_primary', {'cell':'g1'}), ('map_secondary', {'cell':'g1'}),
                ('center_enemy', {'button':'e1'}), ('open_hand_action_menu', {'hand':'right'}),
                ('choose_fire_mode', {'row':'a1'})]:
            screen = 'weapon_action_menu' if action == 'choose_fire_mode' else 'battlescape'
            native = self.fixture(screen=screen); args = arguments(native.client.observe(), action=action)
            native.client.execute(**args); count = len(native.requests)
            with self.subTest(action=action), self.assertRaises(Unavailable): native.client.execute(**dict(args, args=changed))
            self.assertEqual(len(native.requests), count)

    def test_ordinary_weapon_flow_warning_is_observation_not_success_or_retry(self):
        transitions = {('battlescape','open_hand_action_menu'):'weapon_action_menu',
                       ('weapon_action_menu','choose_fire_mode'):'battlescape',
                       ('battlescape','map_primary'):'battlescape_warning'}
        native = self.fixture(transitions=transitions); server = adapter.Server(agent_control=native.client)
        for action in ('open_hand_action_menu', 'choose_fire_mode', 'map_primary'):
            observed = call(server, 'read_ui_state')['result']['structuredContent']
            receipt = call(server, 'execute_ui_action', arguments(observed, action=action,
                           command_id=f'{len(native.results)+1:032x}'))['result']
            self.assertFalse(receipt['isError']); self.assertEqual([x['type'] for x in receipt['content']], ['text'])
            self.assertEqual(receipt['structuredContent']['status'], 'dispatched')
        warning = call(server, 'read_ui_state')['result']['structuredContent']
        self.assertEqual(warning['state'], 'battlescape_warning'); self.assertEqual(warning['actions'], [])
        self.assertIsNone(warning['capability']); self.assertNotIn('success', receipt['structuredContent'])
        native.public_events.append(message('Not enough time units!', None))
        history = call(server, 'read_public_events', dict(epoch=EPOCH, after=0, limit=16))['result']
        self.assertFalse(history['isError']); self.assertIsNone(history['structuredContent']['events'][0]['report'])
        self.assertEqual(sum(packet.startswith(b'E ') for packet in native.requests), 3)

    def test_lost_map_or_fire_mode_receipt_is_terminal_no_action_or_event_replay(self):
        for action, screen in [('map_primary','battlescape'), ('map_secondary','battlescape'),
                               ('choose_fire_mode','weapon_action_menu')]:
            native = self.fixture(screen=screen, transform=lambda op,value:None if op=='E' else value)
            args = arguments(native.client.observe(), action=action)
            with self.subTest(action=action), self.assertRaises(a.ActionOutcomeUnknown): native.client.execute(**args)
            count = len(native.requests)
            with self.assertRaises(Unavailable): native.client.execute(**args)
            with self.assertRaises(Unavailable): native.client.read_events(EPOCH, 0, 1)
            self.assertEqual(len(native.requests), count)
            self.assertEqual(sum(packet.startswith(b'E ') for packet in native.requests), 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
