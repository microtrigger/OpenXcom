# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-19.
"""Retained completion DTO contracts on wire23; socket fixtures are synthetic, not native/live proof."""
import copy
import itertools
import json
import threading
import unittest

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, result, dump, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate


BUTTONS = {
    'items_arriving': ('ok', 'go_to_base'),
    'production_complete': ('ok', 'destination'),
    'ufo_contact': ('intercept', 'center', 'cancel'),
}
ACTION_BUTTONS = {
    'items_arriving': {'dismiss_items_arriving': 'ok', 'open_arrival_base': 'go_to_base'},
    'production_complete': {'dismiss_production_complete': 'ok', 'open_production_destination': 'destination'},
    'ufo_contact': {'dismiss_ufo_contact': 'cancel'},
}
NEW_ACTIONS = tuple(action for actions in ACTION_BUTTONS.values() for action in actions)
SCREENS = tuple(BUTTONS)
ALL_SCREENS = SCREENS + ('error_message',)
TEXT_FIELDS = {
    'items_arriving': ('title',),
    'production_complete': ('message',),
    'ufo_contact': ('title', 'detected_label', 'hyperwave_label'),
}
VISIBLE_KEYS = {
    'items_arriving': {'title', 'headings', 'rows', 'scroll', 'buttons'},
    'production_complete': {'message', 'buttons'},
    'ufo_contact': {'title', 'detected_label', 'hyperwave_label', 'details_status', 'buttons'},
    'error_message': {'message', 'heading'},
}


def checked(obj):
    return a.validate_state(obj, CHALLENGE, EPOCH)


def without_actions(obj):
    obj.update(actions=[], capability=None)
    return obj


def presentation(screen, seq=1):
    return dict(seq=seq, kind='presentation', state=screen, visible=state(screen=screen)['visible'])


def canonical_size(obj):
    return len(json.dumps(obj, ensure_ascii=True, separators=(',', ':')).encode('ascii'))


def sized_arrival(size, event=False):
    """Build an exact ASCII envelope without exceeding any individual cell limit."""
    obj = presentation('items_arriving') if event else state(screen='items_arriving')
    obj['visible']['rows'] = [dict(handle=f'r{i}', cells=['x', '', ''], selected=False) for i in range(32)]
    left = size - canonical_size(obj)
    if left < 0:
        raise ValueError('Envelope is already larger than requested size')
    for row in obj['visible']['rows']:
        for index in range(3):
            count = min(left, 1024 - len(row['cells'][index]))
            row['cells'][index] += 'x' * count
            left -= count
    if left:
        raise ValueError('Requested size exceeds cell capacity')
    return obj


class CompletionContractTests(unittest.TestCase):
    def test_wire_counts_closed_state_catalog_and_previous_wires_rejected(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/24')
        self.assertEqual((len(a.ACTIONS), len(a.STATE_ACTIONS), len(a.definitions())), (119, 49, 4))
        self.assertEqual((a.MAX_COMMANDS, a.EVENT_CAPACITY, a.MAX_TEXT_PACKET, a.MAX_PACKET, a.MAX_REQUEST),
                         (256, 256, 32768, 49152, 512))
        self.assertEqual(a.COMPLETION_BUTTONS, BUTTONS)
        self.assertEqual(a.COMPLETION_BUTTON_ACTIONS, ACTION_BUTTONS)
        for screen, actions in ACTION_BUTTONS.items():
            expected = set(actions) | ({'scroll_visible_list'} if screen == 'items_arriving' else set())
            self.assertEqual(a.STATE_ACTIONS[screen], expected)
        for version in range(1, 24):
            obj = state(screen='production_complete'); obj['schema'] = f'openxcom-agent-control/{version}'
            with self.subTest(version=version), self.assertRaises(Unavailable):
                checked(obj)
        for screen in ('research_complete', 'research_report', 'new_possible', 'ufo_intercept', 'items_arrived'):
            self.assertNotIn(screen, a.STATE_ACTIONS)
            obj = state(screen='unsupported'); obj['state'] = screen
            with self.subTest(screen=screen), self.assertRaises(Unavailable):
                checked(obj)

    def test_new_actions_are_rejected_in_every_wrong_state(self):
        for screen in a.STATE_ACTIONS:
            for action in set(NEW_ACTIONS) - a.STATE_ACTIONS[screen]:
                obj = state(screen=screen)
                obj.update(capability='2' * 32, actions=[dict(id=action, label='Shown',
                    label_source='visible_text', argument_kind='none')])
                with self.subTest(screen=screen, action=action), self.assertRaises(Unavailable):
                    checked(obj)

    def test_exact_visible_keys_and_no_hidden_identity_prediction_or_variant(self):
        for screen in ALL_SCREENS:
            obj = state(screen=screen); checked(obj)
            self.assertEqual(set(obj['visible']), VISIBLE_KEYS[screen])
            for key in VISIBLE_KEYS[screen]:
                bad = copy.deepcopy(obj); del bad['visible'][key]
                with self.subTest(screen=screen, missing=key), self.assertRaises(Unavailable):
                    checked(bad)
            for key in ('base_id', 'item_id', 'ufo_id', 'count', 'capacity', 'queue', 'progress', 'variant',
                        'cost', 'success', 'cause', 'size', 'race', 'mission', 'destination_id', 'details'):
                bad = copy.deepcopy(obj); bad['visible'][key] = None
                with self.subTest(screen=screen, extra=key), self.assertRaises(Unavailable):
                    checked(bad)

    def test_arrival_headings_nullable_and_three_cells_keep_known_text(self):
        obj = state(screen='items_arriving')
        obj['visible'].update(title=None, headings=[None, '', 'Quantity'])
        obj['visible']['rows'][0]['cells'] = ['', '\x01not a number', '∞']
        self.assertEqual(checked(obj), obj)
        for headings in (None, (), [], [None] * 2, [None] * 4, [True] * 3):
            bad = copy.deepcopy(obj); bad['visible']['headings'] = headings
            with self.subTest(headings=headings), self.assertRaises(Unavailable): checked(bad)
        for cells in (None, (), [], ['x'] * 2, ['x'] * 4, [None] * 3, [False] * 3, [1] * 3):
            bad = copy.deepcopy(obj); bad['visible']['rows'][0]['cells'] = cells
            with self.subTest(cells=cells), self.assertRaises(Unavailable): checked(bad)

    def test_arrival_row_count_compact_order_and_read_only_selection(self):
        obj = state(screen='items_arriving'); obj['visible']['rows'] = []
        checked(obj)
        obj['visible']['rows'] = [dict(handle=f'r{i}', cells=['Item', '', 'Base'], selected=False) for i in range(32)]
        checked(obj)
        for key, value in (('handle', 'r32'), ('handle', 'r00'), ('handle', 'r1'), ('handle', 0),
                           ('selected', True), ('selected', 0), ('selected', None), ('controls', ['increase'])):
            bad = copy.deepcopy(obj); bad['visible']['rows'][0][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(Unavailable): checked(bad)
        for key in ('handle', 'cells', 'selected'):
            bad = copy.deepcopy(obj); del bad['visible']['rows'][0][key]
            with self.subTest(missing=key), self.assertRaises(Unavailable): checked(bad)
        bad = copy.deepcopy(obj); bad['visible']['rows'].reverse()
        with self.assertRaises(Unavailable): checked(bad)
        obj['visible']['rows'].append(dict(handle='r32', cells=['', '', ''], selected=False))
        with self.assertRaises(Unavailable): checked(obj)

    def test_scroll_projection_is_strict_shown_booleans_with_exact_semantic_label(self):
        for up, down in ((True, False), (False, True), (True, True)):
            obj = state(screen='items_arriving'); obj['visible']['scroll'] = dict(up=up, down=down)
            checked(obj)
        obj = state(screen='items_arriving'); obj['visible']['scroll'] = dict(up=False, down=False)
        with self.assertRaises(Unavailable): checked(obj)
        obj['actions'] = [entry for entry in obj['actions'] if entry['id'] != 'scroll_visible_list']
        checked(obj)
        for scroll in (None, {}, {'up': True}, {'up': 1, 'down': False},
                       {'up': False, 'down': None}, {'up': True, 'down': False, 'total': 1}):
            bad = state(screen='items_arriving'); bad['visible']['scroll'] = scroll
            with self.subTest(scroll=scroll), self.assertRaises(Unavailable): checked(bad)
        obj = state(screen='items_arriving')
        descriptor = next(entry for entry in obj['actions'] if entry['id'] == 'scroll_visible_list')
        self.assertEqual(descriptor, dict(id='scroll_visible_list', label='Scroll visible list',
            label_source='visible_control_semantics', argument_kind='scroll_direction'))
        descriptor['label'] = 'Scroll Visible List'
        with self.assertRaises(Unavailable): checked(obj)

    def test_buttons_are_closed_ordered_bounded_subsets_and_labels_nonempty(self):
        for screen, order in BUTTONS.items():
            original = state(screen=screen)
            for count in range(len(order) + 1):
                for subset in itertools.combinations(order, count):
                    obj = without_actions(copy.deepcopy(original))
                    obj['visible']['buttons'] = [entry for entry in obj['visible']['buttons'] if entry['id'] in subset]
                    checked(obj)
            malformed = [None, {}, list(reversed(original['visible']['buttons'])),
                         original['visible']['buttons'] + [original['visible']['buttons'][0]],
                         [dict(id='unknown', label='Shown')], [dict(id=order[0], label='Shown', enabled=True)],
                         [dict(id=order[0])], [dict(label='Shown')]]
            malformed += [[dict(id=order[0], label=value)] for value in ('', None, False, 0, [], {})]
            for buttons in malformed:
                obj = without_actions(copy.deepcopy(original)); obj['visible']['buttons'] = buttons
                with self.subTest(screen=screen, buttons=buttons), self.assertRaises(Unavailable): checked(obj)

    def test_each_new_action_requires_its_matching_shown_button(self):
        for screen, mapping in ACTION_BUTTONS.items():
            for action, button in mapping.items():
                obj = state(screen=screen)
                obj['actions'] = [entry for entry in obj['actions'] if entry['id'] == action]
                obj['visible']['buttons'] = [entry for entry in obj['visible']['buttons'] if entry['id'] != button]
                with self.subTest(screen=screen, action=action), self.assertRaises(Unavailable): checked(obj)
                without_actions(obj); checked(obj)

    def test_nullable_body_title_and_contact_labels_do_not_remove_button_authority(self):
        for screen, fields in TEXT_FIELDS.items():
            for values in itertools.product((None, '', 'Shown'), repeat=len(fields)):
                obj = state(screen=screen); obj['visible'].update(zip(fields, values))
                if screen == 'items_arriving': obj['visible']['headings'] = [None] * 3
                original_actions = copy.deepcopy(obj['actions'])
                self.assertEqual(checked(obj)['actions'], original_actions)
                event = dict(seq=1, kind='presentation', state=screen, visible=obj['visible'])
                self.assertEqual(validate(page([event]))['events'], [event])
        obj = state(screen='ufo_contact')
        for value in (None, '', 'presented', 'available', False, {}, []):
            obj['visible']['details_status'] = value
            with self.subTest(status=value), self.assertRaises(Unavailable): checked(obj)

    def test_every_new_text_and_button_label_is_bounded_by_utf8_bytes(self):
        targets = [(screen, (field,)) for screen, fields in TEXT_FIELDS.items() for field in fields]
        targets += [('items_arriving', ('headings', 0)), ('items_arriving', ('rows', 0, 'cells', 0))]
        targets += [(screen, ('buttons', i, 'label')) for screen in SCREENS for i in range(len(BUTTONS[screen]))]
        for screen, path in targets:
            for value, valid in (('é' * 512, True), ('😀' * 256, True), ('x' * 1025, False),
                                 ('é' * 513, False), ('😀' * 257, False), ('\ud800', False)):
                obj = without_actions(state(screen=screen)); target = obj['visible']
                for key in path[:-1]: target = target[key]
                target[path[-1]] = value
                with self.subTest(screen=screen, path=path, bytes_case=repr(value[:4]), valid=valid):
                    if valid: checked(obj)
                    else:
                        with self.assertRaises(Unavailable): checked(obj)

    def test_error_message_body_retains_4096_heading_1024_and_independent_nullability(self):
        for message, heading in itertools.product((None, '', 'Body\x02Heading\nText'), (None, 'Independent', '\x01Color flip')):
            obj = state(screen='error_message'); obj['visible'] = dict(message=message, heading=heading)
            self.assertEqual(checked(obj)['visible'], dict(message=message, heading=heading))
            event = dict(seq=1, kind='presentation', state='error_message', visible=obj['visible'])
            self.assertEqual(validate(page([event]))['events'][0]['visible'], obj['visible'])
        for field, maximum in (('message', 4096), ('heading', 1024)):
            for text in ('x' * maximum, 'é' * (maximum // 2)):
                obj = state(screen='error_message'); obj['visible'][field] = text; checked(obj)
            for text in ('x' * (maximum + 1), 'é' * (maximum // 2 + 1), '\ud800', 1, False, [], {}):
                obj = state(screen='error_message'); obj['visible'][field] = text
                with self.subTest(field=field, text=repr(text)[:20]), self.assertRaises(Unavailable): checked(obj)
        for heading in ('', '\x02', '\x02Heading', 'Head\x02Body', 'Heading\x02'):
            obj = state(screen='error_message'); obj['visible']['heading'] = heading
            with self.subTest(heading=repr(heading)), self.assertRaises(Unavailable): checked(obj)
            event = dict(seq=1, kind='presentation', state='error_message', visible=obj['visible'])
            with self.subTest(presentation_heading=repr(heading)), self.assertRaises(Unavailable): validate(page([event]))
        obj = state(screen='error_message'); obj['visible'] = dict(message='Header\x02Body', heading=None)
        self.assertIsNone(checked(obj)['visible']['heading'])

    def test_new_action_descriptors_bind_exact_label_source_kind_and_shown_text(self):
        for screen, mapping in ACTION_BUTTONS.items():
            for action, button_id in mapping.items():
                original = state(screen=screen)
                descriptor = next(entry for entry in original['actions'] if entry['id'] == action)
                self.assertEqual((descriptor['label_source'], descriptor['argument_kind']), ('visible_text', 'none'))
                for key, value in (('label', descriptor['label'].lower()), ('label', 'Unshown'),
                                   ('label_source', 'visible_control_semantics'), ('label_source', 'model'),
                                   ('argument_kind', 'visible_row'), ('args', {})):
                    obj = copy.deepcopy(original); next(entry for entry in obj['actions'] if entry['id'] == action)[key] = value
                    with self.subTest(action=action, key=key), self.assertRaises(Unavailable): checked(obj)
                for label in ('é' * 512, '\x01Displayed\nLabel', ' '):
                    obj = copy.deepcopy(original)
                    next(entry for entry in obj['visible']['buttons'] if entry['id'] == button_id)['label'] = label
                    next(entry for entry in obj['actions'] if entry['id'] == action)['label'] = label
                    checked(obj)
                obj = copy.deepcopy(original)
                next(entry for entry in obj['visible']['buttons'] if entry['id'] == button_id)['label'] = 'Changed'
                with self.assertRaises(Unavailable): checked(obj)

    def test_five_none_actions_have_empty_args_exact_canonical_suffix_and_schema(self):
        branches = a.definitions()[1]['inputSchema']['oneOf']
        for action in NEW_ACTIONS:
            self.assertEqual(a.ARGUMENT_KINDS[action], 'none')
            args = arguments(action=action, args={})
            self.assertTrue(a.canonical_action_request(args).endswith((' ' + action).encode('ascii')))
            branch = next(entry for entry in branches if action in entry['properties']['action']['enum'])
            self.assertEqual(branch['properties']['args'], dict(type='object', properties={}, required=[], additionalProperties=False))
            for extra in ({'button': 'ok'}, {'row': 'r0'}, {'base_id': 1}, {'input': 'left_click'},
                          {'x': 1, 'y': 2}, {'success': True}, [], None, ''):
                bad = copy.deepcopy(args); bad['args'] = extra
                with self.subTest(action=action, args=extra), self.assertRaises(Unavailable): a.validate_action_arguments(bad)

    def test_contact_intercept_and_center_are_visible_but_never_executable(self):
        obj = state(screen='ufo_contact')
        self.assertEqual([entry['id'] for entry in obj['visible']['buttons']], ['intercept', 'center', 'cancel'])
        self.assertEqual([entry['id'] for entry in obj['actions']], ['dismiss_ufo_contact'])
        for action in ('intercept', 'center', 'intercept_ufo', 'center_ufo', 'open_interception',
                       'intercept_ufo_contact', 'center_ufo_contact', 'open_ufo_details'):
            self.assertNotIn(action, a.ACTIONS)
            with self.subTest(action=action), self.assertRaises(Unavailable):
                a.validate_action_arguments(arguments(action=action, args={}))
        for action in ('center_selected_unit', 'center_enemy', 'open_bases', 'choose_visible_row'):
            bad = copy.deepcopy(obj)
            bad['actions'] = [dict(id=action, label='Center', label_source=a.label_source('ufo_contact', action),
                                  argument_kind=a.ARGUMENT_KINDS[action])]
            with self.subTest(action=action), self.assertRaises(Unavailable): checked(bad)

    def test_presentations_preserve_dtos_without_new_event_kinds_or_authority(self):
        events = [presentation(screen, i + 1) for i, screen in enumerate(ALL_SCREENS)]
        self.assertEqual(validate(page(events))['events'], events)
        for event in events:
            for key, value in (('capability', None), ('actions', []), ('revision', 1), ('success', True)):
                bad = copy.deepcopy(event); bad.update(seq=1); bad[key] = value
                with self.subTest(state=event['state'], key=key), self.assertRaises(Unavailable): validate(page([bad]))
        for kind in ('items_arriving', 'items_arrived', 'production_complete', 'manufacture_completed',
                     'ufo_contact', 'ufo_detected', 'error_heading', 'research_complete'):
            with self.subTest(kind=kind), self.assertRaises(Unavailable): validate(page([dict(seq=1, kind=kind)]))
        event = presentation('ufo_contact'); event['visible']['buttons'] = event['visible']['buttons'][:2]
        self.assertEqual(validate(page([event]))['events'][0], event)

    def test_presentation_dtos_reuse_state_validation_without_reading_actions(self):
        for screen in ALL_SCREENS:
            original = presentation(screen)
            for key in VISIBLE_KEYS[screen]:
                event = copy.deepcopy(original); del event['visible'][key]
                with self.subTest(screen=screen, key=key), self.assertRaises(Unavailable): validate(page([event]))
            event = copy.deepcopy(original); event['visible']['model_data'] = None
            with self.assertRaises(Unavailable): validate(page([event]))
        for screen, key, value in (('items_arriving', 'rows', [dict(handle='r0', cells=[None] * 3, selected=False)]),
                                  ('production_complete', 'message', 'é' * 513),
                                  ('ufo_contact', 'details_status', 'available'),
                                  ('error_message', 'heading', 'é' * 513)):
            event = presentation(screen); event['visible'][key] = value
            with self.subTest(screen=screen), self.assertRaises(Unavailable): validate(page([event]))

    def test_state_canonical_ascii_budget_and_escape_expansion(self):
        obj = sized_arrival(32768); self.assertEqual(canonical_size(obj), 32768); checked(obj)
        with self.assertRaises(Unavailable): checked(sized_arrival(32769))
        obj = state(screen='items_arriving')
        obj['visible']['rows'] = [dict(handle=f'r{i}', cells=['\x01' * 1024] * 3, selected=False) for i in range(32)]
        with self.assertRaises(Unavailable): checked(obj)
        observation = sized_arrival(32768)
        observation.update(challenge=None, capability=None)
        envelope = result(observation=observation, event_sequence=observation['event_sequence'])
        with self.assertRaises(Unavailable): a.validate_result(dump(envelope), COMMAND, EPOCH)

    def test_presentation_and_page_canonical_ascii_byte_limits(self):
        event = sized_arrival(32768, event=True)
        self.assertEqual(canonical_size(event), 32768)
        self.assertEqual(validate(page([event]))['events'][0], event)
        with self.assertRaises(Unavailable): validate(page([sized_arrival(32769, event=True)]))
        events = [sized_arrival(24000, event=True), sized_arrival(24000, event=True)]
        events[1]['seq'] = 2
        envelope = page(events)
        growth = a.MAX_PACKET - canonical_size(envelope)
        self.assertGreater(growth, 0)
        events[1] = sized_arrival(24000 + growth, event=True); events[1]['seq'] = 2
        self.assertEqual(canonical_size(page(events)), 49152); validate(page(events))
        events[1]['visible']['rows'][-1]['cells'][-1] = 'x'
        self.assertEqual(canonical_size(page(events)), 49153)
        with self.assertRaises(Unavailable): validate(page(events))


class CompletionClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        peer = NativeFixture(**kwargs); self.addCleanup(peer.close)
        return peer

    def test_all_five_actions_dispatch_once_with_consumed_grant_and_no_native_success_claim(self):
        for screen, mapping in ACTION_BUTTONS.items():
            for action in mapping:
                peer = self.fixture(screen=screen, transitions={(screen, action): 'geoscape'})
                obj = peer.client.observe(); reply = peer.client.execute(**arguments(obj, action=action))
                self.assertEqual((reply['status'], reply['semantics']), ('dispatched', 'dispatch_not_success'))
                self.assertEqual(reply['observation']['state'], 'geoscape')
                self.assertIsNone(reply['observation']['capability']); self.assertIsNone(peer.client.capability)
                self.assertFalse(peer.client.available_actions or peer.client.scroll_directions)
                self.assertEqual(peer.client.state, 'NEED_CONTEXT')
                self.assertEqual(sum(request.startswith(b'E ') for request in peer.requests), 1)
                self.assertFalse(peer.errors)

    def test_stale_grants_and_unoffered_actions_fail_before_send(self):
        for screen, mapping in ACTION_BUTTONS.items():
            action = next(iter(mapping)); peer = self.fixture(screen=screen)
            old = peer.client.observe(); fresh = peer.client.observe(); before = len(peer.requests)
            with self.assertRaises(Unavailable): peer.client.execute(**arguments(old, action=action))
            self.assertEqual(len(peer.requests), before)
            wrong_action = 'dismiss_ufo_contact' if screen != 'ufo_contact' else 'open_arrival_base'
            with self.assertRaises(Unavailable): peer.client.execute(**arguments(fresh, action=wrong_action))
            self.assertEqual(len(peer.requests), before); self.assertFalse(peer.client.commands)
            self.assertEqual(peer.client.capability, fresh['capability'])
            self.assertEqual(peer.client.execute(**arguments(fresh, action=action))['status'], 'dispatched')

    def test_arrival_scroll_requires_fresh_shown_direction_and_caches_are_detached(self):
        def transform(op, obj):
            if op == 'R' and obj['state'] == 'items_arriving': obj['visible']['scroll'] = dict(up=False, down=True)
            return obj
        peer = self.fixture(screen='items_arriving', transform=transform); obj = peer.client.observe()
        obj['visible']['scroll'].update(up=True, down=False)
        self.assertEqual(peer.client.scroll_directions, frozenset({'down'}))
        with self.assertRaises(Unavailable):
            peer.client.execute(**arguments(obj, action='scroll_visible_list', args={'direction': 'up'}))
        self.assertFalse(any(request.startswith(b'E ') for request in peer.requests))
        self.assertEqual(peer.client.execute(**arguments(obj, action='scroll_visible_list', args={'direction': 'down'}))['status'], 'dispatched')
        self.assertFalse(peer.client.scroll_directions)
        peer.screen = 'ufo_contact'; peer.client.observe(); self.assertFalse(peer.client.scroll_directions)
        peer.screen = 'unsupported'; peer.client.observe(); self.assertIsNone(peer.client.capability)

    def test_duplicate_commands_are_status_only_and_conflicts_do_not_replace_fresh_grant(self):
        peer = self.fixture(screen='items_arriving'); obj = peer.client.observe()
        args = arguments(obj, action='dismiss_items_arriving'); receipt = peer.client.execute(**args)
        fresh = peer.client.observe()
        self.assertEqual(peer.client.execute(**args), receipt)
        self.assertEqual(peer.client.capability, fresh['capability'])
        changed = dict(args, action='open_arrival_base')
        with self.assertRaises(Unavailable): peer.client.execute(**changed)
        self.assertEqual(sum(request.startswith(b'E ') for request in peer.requests), 1)
        self.assertEqual(sum(request.startswith(b'S ') for request in peer.requests), 1)
        receipt['observation']['visible']['title'] = 'Caller mutation'
        self.assertNotEqual(peer.client.read_result(COMMAND)['observation']['visible']['title'], 'Caller mutation')

    def test_event_reads_preserve_authority_revision_deadline_and_detached_records(self):
        peer = self.fixture(screen='items_arriving'); obj = peer.client.observe()
        peer.public_events = [presentation(screen, i + 1) for i, screen in enumerate(ALL_SCREENS)]
        before = (peer.client.capability, peer.client.revision, peer.client.event_sequence,
                  peer.client.capability_deadline, peer.client.available_actions, peer.client.scroll_directions)
        first = peer.client.read_events(EPOCH, 0, 16)
        first['events'][0]['visible']['title'] = 'Caller mutation'
        self.assertEqual(peer.client.read_events(EPOCH, 0, 16)['events'], peer.public_events)
        self.assertEqual(before, (peer.client.capability, peer.client.revision, peer.client.event_sequence,
            peer.client.capability_deadline, peer.client.available_actions, peer.client.scroll_directions))
        self.assertFalse(peer.client.commands)
        self.assertEqual(peer.client.execute(**arguments(obj, action='open_arrival_base'))['status'], 'dispatched')

    def test_completion_history_does_not_mint_authority_and_changed_record_closes_transport(self):
        peer = self.fixture(screen='unsupported'); peer.client.observe()
        peer.public_events = [presentation('ufo_contact')]
        peer.client.read_events(EPOCH, 0, 16)
        self.assertIsNone(peer.client.capability); self.assertFalse(peer.client.available_actions or peer.client.commands)
        peer.public_events[0]['visible']['title'] = 'Different painted title'
        with self.assertRaises(Unavailable): peer.client.read_events(EPOCH, 0, 16)
        self.assertEqual(peer.client.state, 'CLOSED')
        self.assertFalse(any(request.startswith(b'E ') for request in peer.requests))

    def test_single_flight_reserves_and_spends_authority_before_completion_reply(self):
        entered, release = threading.Event(), threading.Event()
        def transform(op, obj):
            if op == 'E': entered.set(); release.wait(2)
            return obj
        peer = self.fixture(screen='production_complete', transform=transform, timeout=2)
        obj = peer.client.observe(); args = arguments(obj, action='open_production_destination'); out = []
        def execute():
            try: out.append(peer.client.execute(**args))
            except Exception as error: out.append(error)
        worker = threading.Thread(target=execute); worker.start()
        try:
            self.assertTrue(entered.wait(1)); self.assertEqual(peer.client.state, 'PENDING')
            self.assertIsNone(peer.client.capability); self.assertIn(COMMAND, peer.client.commands)
            with self.assertRaises(Unavailable): peer.client.execute(**args)
            with self.assertRaises(Unavailable): peer.client.observe()
        finally:
            release.set(); worker.join(3)
        self.assertFalse(worker.is_alive()); self.assertEqual(out[0]['status'], 'dispatched')
        self.assertEqual(sum(request.startswith(b'E ') for request in peer.requests), 1)

    def test_silent_pending_and_unknown_completion_receipts_never_replay(self):
        for mode in ('silent', 'pending', 'unknown_after_dispatch'):
            def transform(op, obj):
                if op == 'E' or (op == 'S' and mode == 'unknown_after_dispatch'):
                    if mode == 'silent': return None
                    obj.update(status=mode, observation=None)
                return obj
            peer = self.fixture(screen='ufo_contact', transform=transform); obj = peer.client.observe()
            args = arguments(obj, action='dismiss_ufo_contact')
            if mode == 'unknown_after_dispatch':
                self.assertEqual(peer.client.execute(**args)['status'], mode)
                self.assertEqual(peer.client.execute(**args)['status'], mode)
                with self.assertRaises(Unavailable): peer.client.execute(**dict(args, command_id='9' * 32))
            else:
                with self.assertRaises(a.ActionOutcomeUnknown) as caught: peer.client.execute(**args)
                self.assertEqual(caught.exception.command_id, COMMAND); self.assertEqual(peer.client.state, 'CLOSED')
                with self.assertRaises(Unavailable): peer.client.execute(**args)
            self.assertEqual(sum(request.startswith(b'E ') for request in peer.requests), 1)

    def test_server_four_tools_return_exact_text_and_structured_completion_data(self):
        for screen, mapping in ACTION_BUTTONS.items():
            peer = self.fixture(screen=screen); server = adapter.Server(agent_control=peer.client)
            observed = call(server, 'read_ui_state')['result']
            peer.public_events = [presentation(screen)]
            history = call(server, 'read_public_events', dict(epoch=EPOCH, after=0, limit=16))['result']
            action = next(iter(mapping))
            dispatched = call(server, 'execute_ui_action', arguments(observed['structuredContent'], action=action))['result']
            status = call(server, 'read_ui_action_result', dict(command_id=COMMAND))['result']
            for reply in (observed, history, dispatched, status):
                self.assertFalse(reply['isError']); self.assertEqual([entry['type'] for entry in reply['content']], ['text'])
                self.assertEqual(json.loads(reply['content'][0]['text']), reply['structuredContent'])
            self.assertEqual(history['structuredContent']['events'], peer.public_events)
            self.assertEqual(status['structuredContent'], dispatched['structuredContent'])
            self.assertFalse(peer.errors)

    def test_server_timeout_is_structured_nonretryable_uncertainty_not_native_receipt(self):
        peer = self.fixture(screen='production_complete', transform=lambda op, obj: None if op == 'E' else obj)
        server = adapter.Server(agent_control=peer.client)
        observed = call(server, 'read_ui_state')['result']['structuredContent']
        reply = call(server, 'execute_ui_action', arguments(observed, action='dismiss_production_complete'))['result']
        self.assertTrue(reply['isError']); error = reply['structuredContent']
        self.assertEqual((error['status'], error['semantics'], error['retryable']),
                         ('outcome_unknown', 'not_a_native_receipt', False))
        self.assertEqual(sum(request.startswith(b'E ') for request in peer.requests), 1)
        self.assertEqual(peer.client.state, 'CLOSED')


if __name__ == '__main__':
    unittest.main(verbosity=2)
