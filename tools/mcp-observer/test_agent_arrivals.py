# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-19.
"""Wire23 nullable arrivals contracts; socket fixtures are synthetic, not native/live proof."""
import copy
import itertools
import json
import unittest

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, result, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate


ARRIVAL_ACTIONS = frozenset(('dismiss_items_arriving', 'open_arrival_base', 'scroll_visible_list'))
VISIBLE_KEYS = {'title', 'headings', 'rows', 'scroll', 'buttons'}
ROW_KEYS = {'handle', 'cells', 'selected'}
PARTIAL_ROWS = [[None, '1', None], ['Scientist', None, ''], ['', None, 'Base'],
                [None, '', 'Base'], ['Scientist', '1', None], [None, '1', 'Base']]
PRESENCE_KINDS = ('projectile_presented', 'impact_effect_presented', 'collapse_animation_presented',
                  'thrown_item_presented', 'large_explosion_effect_presented')


def arrival(cells=None):
    obj = state(screen='items_arriving')
    obj['visible'].update(title=None, headings=[None, '', None])
    obj['visible']['rows'] = [dict(handle='r0', cells=copy.deepcopy(cells if cells is not None else [None, '1', '']),
                                  selected=False)]
    return obj


def checked(obj):
    return a.validate_state(obj, CHALLENGE, EPOCH)


def presentation(obj=None, seq=1):
    obj = arrival() if obj is None else obj
    return dict(seq=seq, kind='presentation', state=obj['state'], visible=copy.deepcopy(obj['visible']))


def canonical_size(obj):
    return len(json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii'))


def sized_arrival(size, event=False):
    """Exact ASCII boundary with a nonempty proved cell in every retained row."""
    obj = presentation() if event else arrival()
    obj['visible']['rows'] = [dict(handle=f'r{i}', cells=['x', None, None], selected=False) for i in range(32)]
    remaining = size - canonical_size(obj)
    if remaining < 0:
        raise ValueError('Requested envelope is below the retained-row minimum')
    for row in obj['visible']['rows']:
        count = min(remaining, 1023)
        row['cells'][0] += 'x' * count
        remaining -= count
    if remaining:
        raise ValueError('Requested envelope exceeds bounded partial-column capacity')
    return obj


def retained_events():
    """The same ten retained event kinds, including one nullable presentation."""
    events = [presentation(), dict(seq=2, kind='displayed_message', text='Shown', report=None, source=None, position=None)]
    for kind in PRESENCE_KINDS:
        events.append(dict(seq=len(events) + 1, kind=kind, semantics='ordinary_renderer_visible_contribution',
                           source=None, actor=None, faction=None, world_position=None, hit=None, damage=None))
    events.extend([dict(seq=8, kind='audio_output', duration_ms=100, nonzero=True, source=None, sound=None, position=None),
                   dict(seq=9, kind='audio_status', status='active', partial_window_discarded=False),
                   dict(seq=10, kind='audio_gap', dropped_windows=1)])
    return events


def partial_transform(op, obj):
    """Inject synthetic partial DTOs into the existing real Client socket path."""
    observed = obj if obj.get('type') == 'state' else obj.get('observation')
    if observed is not None and observed['state'] == 'items_arriving':
        observed['visible'].update(title=None, headings=[None, None, None],
            rows=[dict(handle=f'r{i}', cells=copy.deepcopy(cells), selected=False)
                  for i, cells in enumerate(PARTIAL_ROWS)])
    return obj


class ArrivalContractTests(unittest.TestCase):
    def reject_state_and_event(self, obj):
        with self.assertRaises(Unavailable):
            checked(obj)
        with self.assertRaises(Unavailable):
            validate(page([presentation(obj)]))

    def test_wire23_only_preserves_action_state_tool_and_event_catalogs(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/26')
        self.assertEqual((len(a.ACTIONS), len(a.STATE_ACTIONS), len(a.definitions())), (147, 62, 4))
        self.assertEqual(a.STATE_ACTIONS['items_arriving'], ARRIVAL_ACTIONS)
        self.assertEqual({tool['name'] for tool in a.definitions()},
                         {'read_ui_state', 'execute_ui_action', 'read_ui_action_result', 'read_public_events'})
        self.assertEqual((a.MAX_COMMANDS, a.EVENT_CAPACITY, a.MAX_EVENTS, a.MAX_TEXT_PACKET, a.MAX_PACKET, a.MAX_REQUEST),
                         (256, 256, 16, 32768, 49152, 512))
        self.assertEqual(len({event['kind'] for event in retained_events()}), 10)
        self.assertEqual(validate(page(retained_events()))['events'], retained_events())
        for version in range(1, 26):
            obj = arrival(); obj['schema'] = f'openxcom-agent-control/{version}'
            with self.subTest(version=version), self.assertRaises(Unavailable):
                checked(obj)
        for kind in ('items_arriving', 'arrival_cell', 'items_arrived', 'arrival_unknown', 'row_selected'):
            with self.subTest(kind=kind), self.assertRaises(Unavailable):
                validate(page([dict(seq=1, kind=kind)]))

    def test_all_independent_null_empty_and_known_column_combinations(self):
        accepted = 0
        for cells in itertools.product((None, '', 'Known'), repeat=3):
            if 'Known' not in cells:
                continue
            obj = arrival(list(cells)); expected = copy.deepcopy(obj)
            with self.subTest(cells=cells):
                self.assertEqual(checked(obj), expected)
                self.assertEqual(checked(a.parse(dump(obj))), expected)
                self.assertEqual(validate(page([presentation(obj)]))['events'][0]['visible'], expected['visible'])
                self.assertEqual(obj, expected)
            accepted += 1
        self.assertEqual(accepted, 19)

    def test_null_is_preserved_distinct_from_empty_in_state_event_and_receipt(self):
        obj = arrival([None, '', 'Known'])
        outputs = [checked(a.parse(dump(obj)))['visible'],
                   validate(page([presentation(obj)]))['events'][0]['visible']]
        post = copy.deepcopy(obj); post.update(challenge=None, capability=None)
        receipt = result(observation=post, event_sequence=post['event_sequence'])
        outputs.append(a.validate_result(dump(receipt), COMMAND, EPOCH)['observation']['visible'])
        for visible in outputs:
            cells = visible['rows'][0]['cells']
            self.assertIsNone(cells[0]); self.assertEqual(cells[1], '')
            self.assertEqual(cells[2], 'Known')
            self.assertEqual(set(visible), VISIBLE_KEYS)
            self.assertEqual(set(visible['rows'][0]), ROW_KEYS)

    def test_all_null_all_empty_and_mixed_unknown_empty_rows_are_rejected(self):
        for cells in itertools.product((None, ''), repeat=3):
            with self.subTest(cells=cells):
                self.reject_state_and_event(arrival(list(cells)))
        obj = arrival(); obj['visible']['rows'].append(dict(handle='r1', cells=[None, '', None], selected=False))
        self.reject_state_and_event(obj)

    def test_null_requires_present_slots_and_exact_visible_and_row_keys(self):
        original = arrival([None, 'Known', None])
        for key in VISIBLE_KEYS:
            obj = copy.deepcopy(original); del obj['visible'][key]
            with self.subTest(missing_visible=key): self.reject_state_and_event(obj)
        for key in ROW_KEYS:
            obj = copy.deepcopy(original); del obj['visible']['rows'][0][key]
            with self.subTest(missing_row=key): self.reject_state_and_event(obj)
        for cells in (None, [], ['Known'], [None, 'Known'], [None, 'Known', None, None], {}, 'Known'):
            obj = arrival(); obj['visible']['rows'][0]['cells'] = cells
            with self.subTest(cells=cells): self.reject_state_and_event(obj)
        obj = arrival(); obj['visible']['rows'][0]['cells'] = ('Known', None, None)
        with self.assertRaises(Unavailable): checked(obj)

    def test_every_cell_rejects_nonstring_nonnull_values(self):
        for index, value in itertools.product(range(3), (False, True, 0, 1, 1.5, [], {}, {'text': 'Known'})):
            cells = ['Known'] * 3; cells[index] = value
            with self.subTest(column=index, value=value): self.reject_state_and_event(arrival(cells))

    def test_no_additional_raw_text_identity_source_or_knowledge_fields(self):
        extras = ('text', 'raw_text', 'full_text', 'source', 'text_source', 'identity', 'item_id', 'base_id',
                  'destination_id', 'quantity', 'backing_index', 'total_rows', 'known', 'status', 'cells_status',
                  'selected_row', 'controls', 'available', 'hidden')
        for key, location in itertools.product(extras, ('visible', 'row')):
            obj = arrival(); target = obj['visible'] if location == 'visible' else obj['visible']['rows'][0]
            target[key] = None
            with self.subTest(extra=key, location=location): self.reject_state_and_event(obj)
        for value in ({'text': None}, {'text': 'Known', 'source': None}, {'value': None, 'known': False}):
            with self.subTest(wrapped_cell=value): self.reject_state_and_event(arrival([value, 'Known', None]))

    def test_each_column_uses_1024_utf8_bytes_not_characters(self):
        for index in range(3):
            for text in ('x' * 1024, 'é' * 512, '😀' * 256):
                cells = [None] * 3; cells[index] = text
                obj = arrival(cells)
                with self.subTest(column=index, boundary=text[:4]):
                    self.assertEqual(checked(obj), obj)
                    self.assertEqual(validate(page([presentation(obj)]))['events'][0]['visible'], obj['visible'])
            for text in ('x' * 1025, 'é' * 513, '😀' * 257, '\ud800', '\udfff'):
                cells = [None] * 3; cells[index] = text
                with self.subTest(column=index, excessive=repr(text[:4])):
                    self.reject_state_and_event(arrival(cells))

    def test_empty_projection_and_32_compact_read_only_partial_rows(self):
        obj = arrival(); obj['visible']['rows'] = []
        self.assertEqual(checked(obj), obj)
        self.assertEqual(validate(page([presentation(obj)]))['events'][0]['visible']['rows'], [])
        obj['visible']['rows'] = [dict(handle=f'r{i}', cells=[None, str(i), None], selected=False) for i in range(32)]
        self.assertEqual(checked(obj), obj)
        self.assertEqual(validate(page([presentation(obj)]))['events'][0]['visible']['rows'][-1]['handle'], 'r31')
        bad = copy.deepcopy(obj); bad['visible']['rows'].append(dict(handle='r32', cells=[None, '32', None], selected=False))
        self.reject_state_and_event(bad)
        for index, handle in ((0, 'r1'), (0, 'r00'), (0, '0'), (0, None), (0, 0), (1, 'r0'), (31, 'r32')):
            bad = copy.deepcopy(obj); bad['visible']['rows'][index]['handle'] = handle
            with self.subTest(index=index, handle=handle): self.reject_state_and_event(bad)
        bad = copy.deepcopy(obj); bad['visible']['rows'].reverse(); self.reject_state_and_event(bad)
        for value in (True, 0, 1, None, '', 'false'):
            bad = arrival(); bad['visible']['rows'][0]['selected'] = value
            with self.subTest(selected=value): self.reject_state_and_event(bad)

    def test_go_to_base_authority_depends_on_button_not_known_destination_text(self):
        for cells in ([None, '1', None], ['Item', None, None], ['', None, 'Base']):
            obj = arrival(cells); obj['visible'].update(title=None, headings=[None] * 3)
            self.assertEqual({entry['id'] for entry in checked(obj)['actions']}, ARRIVAL_ACTIONS)
        obj = arrival(); obj['visible'].update(rows=[], scroll=dict(up=False, down=False))
        obj['actions'] = [entry for entry in obj['actions'] if entry['id'] == 'open_arrival_base']
        self.assertEqual(checked(obj)['actions'][0], dict(id='open_arrival_base', label='Go To Base',
                         label_source='visible_text', argument_kind='none'))
        obj['visible']['buttons'] = [entry for entry in obj['visible']['buttons'] if entry['id'] != 'go_to_base']
        with self.assertRaises(Unavailable): checked(obj)
        obj.update(actions=[], capability=None); checked(obj)

    def test_arrival_rows_never_offer_selection_or_change_button_arguments(self):
        for action in ('choose_visible_row', 'select_base', 'select_research_project', 'select_production_project'):
            obj = arrival()
            obj['actions'].append(dict(id=action, label='Select', label_source=a.label_source('items_arriving', action),
                                       argument_kind=a.ARGUMENT_KINDS[action]))
            with self.subTest(action=action), self.assertRaises(Unavailable): checked(obj)
        for action, extras in itertools.product(('open_arrival_base', 'dismiss_items_arriving'),
                                               ({'row': 'r0'}, {'base_id': None}, {'destination': 'Base'}, {'cells': [None, '1', None]})):
            with self.subTest(action=action, extras=extras), self.assertRaises(Unavailable):
                a.validate_action_arguments(arguments(arrival(), action=action, args=extras))

    def test_other_nonnullable_tables_keep_their_existing_cell_contracts(self):
        screens = ('funding', 'base_stores', 'base_transfers', 'new_battle_dropdown', 'purchase_category',
                   'research', 'manufacture', 'new_manufacture', 'manufacture_start')
        for screen in screens:
            original = state(screen=screen)
            if screen == 'manufacture_start':
                original['visible']['rows'] = [dict(handle='r0', cells=['Material', '1', '2'], selected=False)]
            checked(original)
            for index in range(len(original['visible']['rows'][0]['cells'])):
                obj = copy.deepcopy(original); obj['visible']['rows'][0]['cells'][index] = None
                with self.subTest(screen=screen, column=index): self.reject_state_and_event(obj)
        # These two older contracts already permit unknown cells; this patch must not tighten them.
        for screen in ('purchase', 'new_research'):
            obj = state(screen=screen)
            obj['visible']['rows'][0]['cells'] = [None] * len(obj['visible']['rows'][0]['cells'])
            if screen == 'new_research':
                obj.update(actions=[], capability=None)
            with self.subTest(retained_nullable=screen):
                self.assertEqual(checked(obj), obj)

    def test_state_and_presentation_canonical_ascii_boundary_with_partial_rows(self):
        for event in (False, True):
            obj = sized_arrival(32768, event=event)
            self.assertEqual(canonical_size(obj), 32768)
            self.assertTrue(all(row['cells'][0] and row['cells'][1:] == [None, None] for row in obj['visible']['rows']))
            if event:
                self.assertEqual(validate(page([obj]))['events'][0], obj)
            else:
                checked(obj)
            bad = sized_arrival(32769, event=event)
            with self.subTest(event=event), self.assertRaises(Unavailable):
                validate(page([bad])) if event else checked(bad)
        obj = arrival()
        obj['visible']['rows'] = [dict(handle=f'r{i}', cells=['\x01' * 1024, None, None], selected=False) for i in range(32)]
        self.reject_state_and_event(obj)

    def test_page_canonical_ascii_boundary_includes_nullable_presentation(self):
        events = [sized_arrival(24000, event=True), sized_arrival(24000, event=True)]
        events[1]['seq'] = 2
        final_size = 24000 + a.MAX_PACKET - canonical_size(page(events))
        events[1] = sized_arrival(final_size, event=True); events[1]['seq'] = 2
        self.assertEqual(canonical_size(page(events)), 49152)
        self.assertEqual(validate(page(events))['events'], events)
        events[1] = sized_arrival(final_size + 1, event=True); events[1]['seq'] = 2
        self.assertEqual(canonical_size(page(events)), 49153)
        with self.assertRaises(Unavailable): validate(page(events))

    def test_presentation_has_no_grant_or_new_nullable_cell_metadata(self):
        for key, value in (('capability', None), ('actions', []), ('revision', 1), ('source', None), ('known', True)):
            event = presentation(); event[key] = value
            with self.subTest(key=key), self.assertRaises(Unavailable): validate(page([event]))


class ArrivalClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        kwargs.setdefault('screen', 'items_arriving'); kwargs.setdefault('transform', partial_transform)
        peer = NativeFixture(**kwargs); self.addCleanup(peer.close)
        return peer

    def test_partial_rows_roundtrip_without_row_selection_authority(self):
        peer = self.fixture(); obj = peer.client.observe()
        self.assertEqual([row['cells'] for row in obj['visible']['rows']], PARTIAL_ROWS)
        self.assertFalse(peer.client.visible_rows or peer.client.project_rows or peer.client.base_targets)
        self.assertEqual(peer.client.available_actions, ARRIVAL_ACTIONS)
        before = len(peer.requests)
        with self.assertRaises(Unavailable):
            peer.client.execute(**arguments(obj, action='choose_visible_row', args={'row': 'r0'}))
        self.assertEqual(len(peer.requests), before); self.assertFalse(peer.client.commands)
        self.assertEqual(peer.client.capability, obj['capability'])
        receipt = peer.client.execute(**arguments(obj, action='open_arrival_base'))
        self.assertEqual((receipt['status'], receipt['semantics']), ('dispatched', 'dispatch_not_success'))
        self.assertEqual([row['cells'] for row in receipt['observation']['visible']['rows']], PARTIAL_ROWS)
        self.assertIsNone(receipt['observation']['capability']); self.assertIsNone(peer.client.capability)
        self.assertEqual(peer.client.state, 'NEED_CONTEXT')
        self.assertFalse(peer.client.available_actions or peer.client.visible_rows or peer.client.scroll_directions)
        self.assertEqual(sum(request.startswith(b'E ') for request in peer.requests), 1)

    def test_fresh_replacement_makes_old_partial_grant_stale_without_dispatch(self):
        peer = self.fixture(); old = peer.client.observe(); fresh = peer.client.observe()
        self.assertNotEqual(old['capability'], fresh['capability'])
        before = len(peer.requests)
        with self.assertRaises(Unavailable): peer.client.execute(**arguments(old, action='open_arrival_base'))
        self.assertEqual(len(peer.requests), before); self.assertFalse(peer.client.commands)
        self.assertEqual(peer.client.capability, fresh['capability'])
        self.assertEqual(peer.client.execute(**arguments(fresh, action='open_arrival_base'))['status'], 'dispatched')

    def test_duplicate_is_status_only_preserves_fresh_grant_and_null_empty_receipt(self):
        peer = self.fixture(); obj = peer.client.observe(); args = arguments(obj, action='open_arrival_base')
        receipt = peer.client.execute(**args); original = copy.deepcopy(receipt); fresh = peer.client.observe()
        receipt['observation']['visible']['rows'][0]['cells'][0] = 'Caller mutation'
        self.assertEqual(peer.client.execute(**args), original)
        self.assertEqual(peer.client.capability, fresh['capability'])
        before = len(peer.requests)
        with self.assertRaises(Unavailable): peer.client.execute(**dict(args, action='dismiss_items_arriving'))
        self.assertEqual(len(peer.requests), before)
        self.assertEqual(peer.client.read_result(COMMAND), original)
        self.assertEqual(peer.client.capability, fresh['capability'])
        self.assertEqual(sum(request.startswith(b'E ') for request in peer.requests), 1)
        self.assertEqual(sum(request.startswith(b'S ') for request in peer.requests), 2)

    def test_shown_scroll_direction_is_cached_independently_of_partial_rows(self):
        def transform(op, obj):
            partial_transform(op, obj)
            if op == 'R': obj['visible']['scroll'] = dict(up=False, down=True)
            return obj
        peer = self.fixture(transform=transform); obj = peer.client.observe()
        obj['visible']['rows'].clear(); obj['visible']['scroll'].update(up=True, down=False)
        self.assertEqual(peer.client.scroll_directions, frozenset({'down'}))
        before = len(peer.requests)
        with self.assertRaises(Unavailable):
            peer.client.execute(**arguments(obj, action='scroll_visible_list', args={'direction': 'up'}))
        self.assertEqual(len(peer.requests), before)
        self.assertEqual(peer.client.execute(**arguments(obj, action='scroll_visible_list', args={'direction': 'down'}))['status'], 'dispatched')

    def test_ten_retained_event_kinds_preserve_live_grant_and_detached_null_cells(self):
        peer = self.fixture(); obj = peer.client.observe(); peer.public_events = retained_events()
        before = (peer.client.capability, peer.client.revision, peer.client.event_sequence,
                  peer.client.capability_deadline, peer.client.available_actions, peer.client.scroll_directions)
        first = peer.client.read_events(EPOCH, 0, 16)
        first['events'][0]['visible']['rows'][0]['cells'][0] = ''
        second = peer.client.read_events(EPOCH, 0, 16)
        self.assertEqual(second['events'], retained_events())
        self.assertIsNone(second['events'][0]['visible']['rows'][0]['cells'][0])
        self.assertEqual(before, (peer.client.capability, peer.client.revision, peer.client.event_sequence,
            peer.client.capability_deadline, peer.client.available_actions, peer.client.scroll_directions))
        self.assertFalse(peer.client.commands)
        self.assertEqual(peer.client.execute(**arguments(obj, action='open_arrival_base'))['status'], 'dispatched')

    def test_retained_partial_event_does_not_mint_authority_or_revive_expired_grant(self):
        peer = self.fixture(screen='unsupported'); peer.client.observe(); peer.public_events = [presentation()]
        peer.client.read_events(EPOCH, 0, 16)
        self.assertIsNone(peer.client.capability)
        self.assertFalse(peer.client.available_actions or peer.client.visible_rows or peer.client.commands)
        active = self.fixture(); obj = active.client.observe(); active.public_events = [presentation()]
        active.client.capability_deadline = 0
        active.client.read_events(EPOCH, 0, 16)
        self.assertEqual(active.client.capability_deadline, 0)
        before = len(active.requests)
        with self.assertRaises(Unavailable): active.client.execute(**arguments(obj, action='open_arrival_base'))
        self.assertEqual(len(active.requests), before); self.assertFalse(active.client.commands)

    def test_changed_retained_null_to_empty_closes_transport_without_dispatch(self):
        peer = self.fixture(); peer.client.observe(); peer.public_events = [presentation()]
        peer.client.read_events(EPOCH, 0, 16)
        peer.public_events[0]['visible']['rows'][0]['cells'][0] = ''
        with self.assertRaises(Unavailable): peer.client.read_events(EPOCH, 0, 16)
        self.assertEqual(peer.client.state, 'CLOSED')
        self.assertFalse(any(request.startswith(b'E ') for request in peer.requests))

    def test_changed_duplicate_receipt_null_to_empty_closes_without_replay(self):
        def transform(op, obj):
            partial_transform(op, obj)
            if op == 'S': obj['observation']['visible']['rows'][0]['cells'][0] = ''
            return obj
        peer = self.fixture(transform=transform); obj = peer.client.observe()
        args = arguments(obj, action='open_arrival_base'); peer.client.execute(**args)
        with self.assertRaises(Unavailable): peer.client.execute(**args)
        self.assertEqual(peer.client.state, 'CLOSED')
        self.assertEqual(sum(request.startswith(b'E ') for request in peer.requests), 1)
        self.assertEqual(sum(request.startswith(b'S ') for request in peer.requests), 1)

    def test_malformed_partial_state_fails_closed_before_action(self):
        for cells in ([None, None, None], ['', '', ''], [None, 'Known'], [None, True, 'Known'],
                      [None, 'x' * 1025, None]):
            def transform(op, obj):
                if op == 'R': obj['visible']['rows'][0]['cells'] = copy.deepcopy(cells)
                return obj
            peer = self.fixture(transform=transform)
            with self.subTest(cells=str(cells)[:50]), self.assertRaises(Unavailable): peer.client.observe()
            self.assertEqual(peer.client.state, 'CLOSED')
            self.assertFalse(peer.client.commands or any(request.startswith(b'E ') for request in peer.requests))

    def test_malformed_post_action_null_row_is_nonretryable_uncertainty(self):
        def transform(op, obj):
            partial_transform(op, obj)
            if op == 'E': obj['observation']['visible']['rows'][0]['cells'] = [None] * 3
            return obj
        peer = self.fixture(transform=transform); obj = peer.client.observe()
        args = arguments(obj, action='open_arrival_base')
        with self.assertRaises(a.ActionOutcomeUnknown) as caught: peer.client.execute(**args)
        self.assertEqual(caught.exception.command_id, COMMAND)
        self.assertEqual(peer.client.state, 'CLOSED')
        with self.assertRaises(Unavailable): peer.client.execute(**args)
        self.assertEqual(sum(request.startswith(b'E ') for request in peer.requests), 1)

    def test_actual_server_four_tools_preserve_partial_json_and_duplicate_status_only(self):
        peer = self.fixture(); server = adapter.Server(agent_control=peer.client)
        observed = call(server, 'read_ui_state')['result']
        peer.public_events = retained_events()
        history = call(server, 'read_public_events', dict(epoch=EPOCH, after=0, limit=16))['result']
        args = arguments(observed['structuredContent'], action='open_arrival_base')
        dispatched = call(server, 'execute_ui_action', args)['result']
        fresh = call(server, 'read_ui_state')['result']
        duplicate = call(server, 'execute_ui_action', args)['result']
        status = call(server, 'read_ui_action_result', dict(command_id=COMMAND))['result']
        for reply in (observed, history, dispatched, fresh, duplicate, status):
            self.assertFalse(reply['isError'])
            self.assertEqual([entry['type'] for entry in reply['content']], ['text'])
            self.assertEqual(json.loads(reply['content'][0]['text']), reply['structuredContent'])
        self.assertEqual([row['cells'] for row in observed['structuredContent']['visible']['rows']], PARTIAL_ROWS)
        self.assertEqual(history['structuredContent']['events'], retained_events())
        self.assertEqual(duplicate['structuredContent'], dispatched['structuredContent'])
        self.assertEqual(status['structuredContent'], dispatched['structuredContent'])
        self.assertEqual(peer.client.capability, fresh['structuredContent']['capability'])
        self.assertEqual(sum(request.startswith(b'E ') for request in peer.requests), 1)
        self.assertEqual(sum(request.startswith(b'S ') for request in peer.requests), 2)
        self.assertFalse(peer.errors)


if __name__ == '__main__':
    unittest.main(verbosity=2)
