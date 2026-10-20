# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic /13 grenade contracts over anonymous AF_UNIX; no game/build proof."""
import copy
import json
import unittest

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, arguments, call, dump, result, state, EPOCH, CAPABILITY, CHALLENGE, COMMAND
from test_agent_events import page, validate


SCHEMA = 'openxcom-agent-control/22'


def descriptor(action, label):
    return dict(id=action, label=label,
                label_source='visible_text' if action in ('choose_weapon_action', 'choose_grenade_timer') else 'visible_control_semantics',
                argument_kind={'choose_weapon_action': 'weapon_action', 'choose_grenade_timer': 'grenade_timer'}.get(action, 'none'))


def grenade_state(count=3, *, title='Set timer', challenge=CHALLENGE, capability=CAPABILITY, revision=1, event_sequence=1):
    timers = [dict(handle=f't{i}', label=str(i)) for i in range(count)]
    actions = ([descriptor('choose_grenade_timer', timers[0]['label'])] if timers else [])
    actions.append(descriptor('cancel_grenade_timer', 'Cancel timer'))
    return dict(schema=SCHEMA, mode='agent-control', type='state', epoch=EPOCH, challenge=challenge,
                capability=capability, revision=revision, event_sequence=event_sequence, state='prime_grenade',
                visible=dict(title=title, timers=timers), actions=actions, semantics='presented_after_request')


def weapon_state(*, challenge=CHALLENGE, capability=CAPABILITY, revision=1, event_sequence=1):
    obj = grenade_state(challenge=challenge, capability=capability, revision=revision, event_sequence=event_sequence)
    obj['state'] = 'weapon_action_menu'
    obj['visible'] = dict(rows=[dict(handle='a0', label='Prime Grenade', accuracy=None, time_units='4'),
                                dict(handle='a1', label='Throw', accuracy='75%', time_units='12')])
    obj['actions'] = [descriptor('choose_weapon_action', 'Prime Grenade'), descriptor('cancel_weapon_action', 'Cancel weapon action')]
    return obj


class GrenadeFixture(NativeFixture):
    def __init__(self, *, count=3, **kwargs):
        self.timer_count = count
        super().__init__(screen=kwargs.pop('screen', 'prime_grenade'), **kwargs)

    def observation(self, challenge, capability):
        if self.screen == 'prime_grenade':
            return grenade_state(self.timer_count, challenge=challenge, capability=capability,
                                 revision=self.revision, event_sequence=self.sequence)
        if self.screen == 'weapon_action_menu':
            return weapon_state(challenge=challenge, capability=capability, revision=self.revision, event_sequence=self.sequence)
        return super().observation(challenge, capability)


class GrenadeValidationTests(unittest.TestCase):
    def valid(self, obj):return a.validate_state(obj, obj['challenge'], EPOCH)

    def invalid(self, obj):
        with self.assertRaises(Unavailable):self.valid(obj)

    def test_exact_version_closed_counts_and_replaced_weapon_name(self):
        self.assertEqual(a.SCHEMA, SCHEMA)
        self.assertEqual((len(a.definitions()), len(a.ACTIONS), len(a.STATE_ACTIONS)), (4, 96, 41))
        self.assertEqual(a.MAX_COMMANDS, 256)
        self.assertNotIn('choose_fire_mode', a.ACTIONS)
        self.assertEqual(a.WEAPON_ACTION_HANDLES, tuple(f'a{i}' for i in range(6)))
        self.assertEqual(a.GRENADE_TIMER_HANDLES, tuple(f't{i}' for i in range(24)))
        self.assertEqual(a.STATE_ACTIONS['weapon_action_menu'], {'choose_weapon_action', 'cancel_weapon_action'})
        self.assertEqual(a.STATE_ACTIONS['prime_grenade'], {'choose_grenade_timer', 'cancel_grenade_timer'})
        self.assertEqual(a.ARGUMENT_KINDS['choose_weapon_action'], 'weapon_action')
        self.assertEqual(a.ARGUMENT_KINDS['choose_grenade_timer'], 'grenade_timer')
        self.assertEqual(a.ARGUMENT_KINDS['cancel_grenade_timer'], 'none')

    def test_prime_state_empty_one_and_maximum_visible_timer_list(self):
        for count in (0, 1, 24):self.valid(grenade_state(count))
        self.invalid(grenade_state(25))
        self.valid(weapon_state())

    def test_title_nullable_empty_and_1024_utf8_bytes(self):
        for title in (None, '', 'é'*512, 'a'*1024):self.valid(grenade_state(title=title))
        for title in ('é'*513, 'a'*1025, '\ud800', False, 0, [], {}):
            with self.subTest(title=repr(title)[:24]):self.invalid(grenade_state(title=title))

    def test_timer_label_nonempty_and_utf8_byte_bound(self):
        for label in ('0', 'Turn 1', 'é'*512, 'a'*1024):
            obj = grenade_state(1);obj['visible']['timers'][0]['label'] = label;obj['actions'][0]['label'] = label
            self.valid(obj)
        for label in ('', None, False, 0, [], {}, 'é'*513, 'a'*1025, '\ud800'):
            obj = grenade_state(1);obj['visible']['timers'][0]['label'] = label;obj['actions'][0]['label'] = label
            with self.subTest(label=repr(label)[:24]):self.invalid(obj)

    def test_timer_handles_must_be_sequential_current_local_text(self):
        for handle in ('t1', 't00', 't24', 't-1', 'a0', 'T0', 't0 ', 't0\n', 0, True, None):
            obj = grenade_state();obj['visible']['timers'][0]['handle'] = handle
            with self.subTest(handle=handle):self.invalid(obj)
        obj = grenade_state();obj['visible']['timers'][1]['handle'] = 't0';self.invalid(obj)
        obj = grenade_state();obj['visible']['timers'].reverse();self.invalid(obj)

    def test_visible_and_timer_fields_are_exact_without_hidden_fuse_or_item_data(self):
        for field in ('item_id', 'fuse', 'timer_value', 'default_timer', 'remaining_turns', 'unit_id', 'throw_target', 'armed'):
            obj = grenade_state();obj['visible'][field] = 1
            with self.subTest(visible_field=field):self.invalid(obj)
            obj = grenade_state();obj['visible']['timers'][0][field] = 1
            with self.subTest(timer_field=field):self.invalid(obj)
        for field in ('title', 'timers'):
            obj = grenade_state();obj['visible'].pop(field);self.invalid(obj)
        for value in (None, {}, (), False, 't0'):
            obj = grenade_state();obj['visible']['timers'] = value;self.invalid(obj)
        for value in (None, [], 0, '0'):
            obj = grenade_state();obj['visible']['timers'][0] = value;self.invalid(obj)

    def test_choose_descriptor_is_exact_first_admitted_label_and_source(self):
        for field, value in (('label', 'Turn 999'), ('label_source', 'visible_control_semantics'),
                             ('argument_kind', 'weapon_action'), ('argument_kind', 'point'), ('preconditions', {})):
            obj = grenade_state();obj['actions'][0][field] = value
            with self.subTest(field=field):self.invalid(obj)
        obj = grenade_state();obj['actions'][0]['label'] = obj['visible']['timers'][1]['label'];self.invalid(obj)
        obj = grenade_state(0);obj['actions'].insert(0, descriptor('choose_grenade_timer', '0'));self.invalid(obj)

    def test_cancel_descriptor_has_literal_label_and_control_source(self):
        for field, value in (('label', 'Cancel'), ('label', 'Cancel grenade timer'), ('label_source', 'visible_text'),
                             ('argument_kind', 'grenade_timer'), ('label', None)):
            obj = grenade_state();obj['actions'][-1][field] = value
            with self.subTest(field=field, value=value):self.invalid(obj)

    def test_cross_modal_actions_are_not_accepted(self):
        for action, label in (('choose_weapon_action', 'Prime Grenade'), ('cancel_weapon_action', 'Cancel weapon action'),
                              ('map_primary', 'Map primary'), ('inventory_activate', 'Inventory activate')):
            obj = grenade_state();obj['actions'].append(descriptor(action, label));self.invalid(obj)
        obj = weapon_state();obj['actions'].append(descriptor('cancel_grenade_timer', 'Cancel timer'));self.invalid(obj)
        obj = grenade_state();obj['actions'].append(copy.deepcopy(obj['actions'][0]));self.invalid(obj)

    def test_weapon_rows_accept_prime_throw_as_literal_labels_without_classification(self):
        self.valid(weapon_state())
        obj = weapon_state();obj['visible']['rows'][0]['label'] = 'Unfamiliar shown action';obj['actions'][0]['label'] = 'Unfamiliar shown action'
        self.valid(obj)
        for key in ('action_type', 'grenade', 'throw', 'fuse', 'compatibility'):
            bad = copy.deepcopy(obj);bad['visible']['rows'][0][key] = True;self.invalid(bad)
        for field, value in (('label', 'Generic choose'), ('argument_kind', 'fire_mode'), ('label_source', 'visible_control_semantics')):
            bad = weapon_state();bad['actions'][0][field] = value;self.invalid(bad)

    def test_action_args_are_exact_with_canonical_suffixes(self):
        for action, args, suffix in [('choose_weapon_action', {'row':'a5'}, ' a5'),
                                     ('choose_grenade_timer', {'timer':'t23'}, ' t23'), ('cancel_grenade_timer', {}, '')]:
            request = arguments(grenade_state(), action=action, args=args)
            self.assertEqual(a.canonical_action_request(request),
                f'E {COMMAND} {EPOCH} {CAPABILITY} 1 {action}{suffix}'.encode())
            for extra in ('row_id', 'fuse', 'seconds', 'turns', 'value', 'x', 'item_id'):
                with self.subTest(action=action, extra=extra), self.assertRaises(Unavailable):
                    a.validate_action_arguments(dict(request, args=dict(args, **{extra:1})))
            for value in (None, [], 't0', 1, False):
                with self.assertRaises(Unavailable):a.validate_action_arguments(dict(request, args=value))

    def test_timer_and_weapon_argument_domains_reject_types_and_out_of_range(self):
        for action, key, values in [
            ('choose_grenade_timer', 'timer', ('t24','t-1','t00','t1 ','t0\n','T0','a0','r0','',True,0,1.0,None,[],{})),
            ('choose_weapon_action', 'row', ('a6','a-1','a00','A0','a1 ','a0\n','t0','r0','',False,0,1.0,None,[],{}))]:
            for value in values:
                with self.subTest(action=action, value=value), self.assertRaises(Unavailable):
                    a.validate_action_arguments(arguments(grenade_state(), action=action, args={key:value}))
        for action, args in [('choose_grenade_timer', {}), ('choose_grenade_timer', {'row':'t0'}),
                             ('choose_weapon_action', {'timer':'a0'}), ('choose_fire_mode', {'row':'a0'}),
                             ('cancel_grenade_timer', {'timer':'t0'})]:
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(grenade_state(), action=action, args=args))

    def test_action_schema_has_exact_new_branches_no_old_fire_branch(self):
        execute = next(tool for tool in a.definitions() if tool['name']=='execute_ui_action')['inputSchema']
        self.assertEqual(set(execute['properties']['action']['enum']), set(a.ACTIONS))
        for action, key, handles in [('choose_weapon_action','row',a.WEAPON_ACTION_HANDLES),
                                     ('choose_grenade_timer','timer',a.GRENADE_TIMER_HANDLES)]:
            branches = [b for b in execute['oneOf'] if action in b['properties']['action']['enum']]
            self.assertEqual(len(branches), 1)
            schema = branches[0]['properties']['args']
            self.assertEqual(schema['required'], [key]);self.assertFalse(schema['additionalProperties'])
            self.assertEqual(schema['properties'], {key:{'type':'string','enum':list(handles)}})
        self.assertNotIn('choose_fire_mode', json.dumps(execute))

    def test_schema12_is_rejected_at_every_native_boundary(self):
        old = 'openxcom-agent-control/12'
        hello = dict(schema=old, mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output')
        with self.assertRaises(Unavailable):a.validate_hello(dump(hello))
        with self.assertRaises(Unavailable):a.validate_ready(dump(dict(schema=old, mode=a.MODE, status='ready')))
        obj = grenade_state();obj['schema'] = old;self.invalid(obj)
        obj = result();obj['schema'] = old
        with self.assertRaises(Unavailable):a.validate_result(dump(obj), COMMAND, EPOCH)
        obj = page([]);obj['schema'] = old
        with self.assertRaises(Unavailable):validate(obj)

    def test_state_result_and_retained_prime_projection_have_no_hidden_authority(self):
        obj = grenade_state();self.valid(obj)
        observation = grenade_state(challenge=None, capability=None, event_sequence=2)
        a.validate_result(dump(result(observation=observation)), COMMAND, EPOCH)
        event = dict(seq=1, kind='presentation', state='prime_grenade', visible=obj['visible'])
        validate(page([event]))
        for field in ('actions', 'capability', 'timer_value', 'item_id'):
            with self.assertRaises(Unavailable):validate(page([dict(event, **{field:'hidden'})]))
        observation['capability'] = CAPABILITY
        with self.assertRaises(Unavailable):a.validate_result(dump(result(observation=observation)), COMMAND, EPOCH)

    def test_grenade_state_does_not_introduce_ninth_event_kind(self):
        for kind in ('grenade_primed', 'grenade_thrown', 'grenade_timer', 'explosion', 'fuse_changed'):
            with self.assertRaises(Unavailable):validate(page([dict(seq=1, kind=kind)]))

    def test_maximal_unicode_expansion_still_obeys_outer_response_bound(self):
        obj = grenade_state(24)
        for timer in obj['visible']['timers']:timer['label'] = 'é'*512
        obj['actions'][0]['label'] = obj['visible']['timers'][0]['label']
        self.invalid(obj)


class GrenadeClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native = GrenadeFixture(**kwargs);self.addCleanup(native.close);return native

    def test_timer_selection_and_cancel_have_exact_wire_and_clear_authority(self):
        native = self.fixture()
        for index, (action, args) in enumerate((('choose_grenade_timer', {'timer':'t2'}), ('cancel_grenade_timer', {}))):
            observed = native.client.observe();request = arguments(observed, command_id=f'{index+1:032x}', action=action, args=args)
            reply = native.client.execute(**request)
            self.assertEqual(reply['status'], 'dispatched');self.assertIsNone(reply['observation']['capability'])
            self.assertEqual(native.requests[-1], a.canonical_action_request(request))
            self.assertEqual(native.client.grenade_timers, frozenset());self.assertEqual(native.client.weapon_actions, frozenset())
            self.assertIsNone(native.client.capability)

    def test_unlisted_timer_and_weapon_row_preserve_grant_and_ledger(self):
        for screen, action, args in [('prime_grenade','choose_grenade_timer',{'timer':'t23'}),
                                     ('weapon_action_menu','choose_weapon_action',{'row':'a5'})]:
            native = self.fixture(screen=screen);obj = native.client.observe();count = len(native.requests)
            with self.assertRaises(Unavailable):native.client.execute(**arguments(obj, action=action, args=args))
            self.assertEqual(len(native.requests), count);self.assertEqual(native.client.commands, {})
            self.assertEqual(native.client.capability, obj['capability'])

    def test_mutated_returned_timer_dto_cannot_add_or_remove_cached_authority(self):
        native = self.fixture();obj = native.client.observe();count = len(native.requests)
        self.assertIsInstance(native.client.grenade_timers, frozenset)
        obj['visible']['timers'].append(dict(handle='t3', label='3'))
        with self.assertRaises(Unavailable):native.client.execute(**arguments(obj, action='choose_grenade_timer', args={'timer':'t3'}))
        self.assertEqual(len(native.requests), count);self.assertEqual(native.client.commands, {})
        obj['visible']['timers'] = [];obj['actions'] = []
        self.assertEqual(native.client.execute(**arguments(obj, action='choose_grenade_timer', args={'timer':'t1'}))['status'], 'dispatched')

    def test_mutated_weapon_dto_cannot_add_or_remove_cached_authority(self):
        native = self.fixture(screen='weapon_action_menu');obj = native.client.observe();count = len(native.requests)
        self.assertIsInstance(native.client.weapon_actions, frozenset)
        obj['visible']['rows'].append(dict(handle='a2', label='Fake', accuracy=None, time_units=None))
        with self.assertRaises(Unavailable):native.client.execute(**arguments(obj, action='choose_weapon_action', args={'row':'a2'}))
        self.assertEqual(len(native.requests), count)
        obj['visible']['rows'] = [];obj['actions'] = []
        self.assertEqual(native.client.execute(**arguments(obj, action='choose_weapon_action', args={'row':'a1'}))['status'], 'dispatched')

    def test_new_read_invalidates_prior_timer_grant_even_with_same_shown_label(self):
        native = self.fixture();old = native.client.observe();native.revision += 1
        new = native.client.observe();count = len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**arguments(old, action='choose_grenade_timer', args={'timer':'t0'}))
        self.assertEqual(len(native.requests), count);self.assertEqual(native.client.capability, new['capability'])
        self.assertEqual(native.client.execute(**arguments(new, action='choose_grenade_timer', args={'timer':'t0'}))['status'], 'dispatched')

    def test_exact_duplicate_is_status_only_and_changed_timer_conflicts(self):
        native = self.fixture();obj = native.client.observe();request = arguments(obj, action='choose_grenade_timer', args={'timer':'t0'})
        first = native.client.execute(**request);new = native.client.observe();count = len(native.requests)
        for changed in (dict(args={'timer':'t1'}), dict(action='cancel_grenade_timer',args={})):
            with self.assertRaises(Unavailable):native.client.execute(**dict(request, **changed))
        self.assertEqual(len(native.requests), count)
        self.assertEqual(native.client.execute(**request), first)
        self.assertTrue(native.requests[-1].startswith(b'S '))
        self.assertEqual(sum(r.startswith(b'E ') for r in native.requests), 1)
        self.assertEqual(native.client.capability, new['capability'])
        self.assertEqual(native.client.grenade_timers, frozenset({'t0','t1','t2'}))

    def test_mutated_arguments_and_receipt_cannot_rewrite_terminal_identity(self):
        native = self.fixture();obj = native.client.observe();request = arguments(obj, action='choose_grenade_timer', args={'timer':'t0'})
        original = copy.deepcopy(request);first = native.client.execute(**request);saved = copy.deepcopy(first)
        request['args']['timer'] = 't1';first['observation']['visible']['timers'][0]['label'] = 'forged'
        with self.assertRaises(Unavailable):native.client.execute(**request)
        self.assertEqual(native.client.execute(**original), saved)

    def test_popup_transition_requires_fresh_r_and_clears_each_other_handle_set(self):
        native = self.fixture(screen='weapon_action_menu', transitions={
            ('weapon_action_menu','choose_weapon_action'):'prime_grenade', ('prime_grenade','choose_grenade_timer'):'battlescape'})
        weapon = native.client.observe()
        self.assertEqual(native.client.weapon_actions, frozenset({'a0','a1'}));self.assertFalse(native.client.grenade_timers)
        receipt = native.client.execute(**arguments(weapon, action='choose_weapon_action', args={'row':'a0'}))
        self.assertEqual(receipt['observation']['state'], 'prime_grenade')
        self.assertFalse(native.client.weapon_actions);self.assertFalse(native.client.grenade_timers)
        count = len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**arguments(weapon, command_id='5'*32, action='choose_grenade_timer', args={'timer':'t0'}))
        self.assertEqual(len(native.requests), count)
        prime = native.client.observe();self.assertFalse(native.client.weapon_actions)
        self.assertEqual(native.client.grenade_timers, frozenset({'t0','t1','t2'}))
        native.client.execute(**arguments(prime, command_id='6'*32, action='choose_grenade_timer', args={'timer':'t0'}))
        battle = native.client.observe();self.assertEqual(battle['state'], 'battlescape')
        self.assertFalse(native.client.weapon_actions);self.assertFalse(native.client.grenade_timers)

    def test_cross_modal_request_is_local_rejection_not_native_dispatch(self):
        for screen, action, args in [('prime_grenade','choose_weapon_action',{'row':'a0'}),
                                     ('weapon_action_menu','choose_grenade_timer',{'timer':'t0'}),
                                     ('battlescape','cancel_grenade_timer',{})]:
            native = self.fixture(screen=screen);obj = native.client.observe();count = len(native.requests)
            with self.assertRaises(Unavailable):native.client.execute(**arguments(obj, action=action, args=args))
            self.assertEqual(len(native.requests), count);self.assertEqual(native.client.commands, {})

    def test_event_reads_are_grant_neutral_and_returned_history_is_detached(self):
        native = self.fixture();obj = native.client.observe()
        native.public_events = [dict(seq=1, kind='presentation', state='prime_grenade', visible=grenade_state()['visible'])]
        before = (native.client.capability, native.client.revision, native.client.grenade_timers,
                  native.client.weapon_actions, native.client.capability_deadline)
        history = native.client.read_events(EPOCH, 0, 16)
        history['events'][0]['visible']['timers'][0]['label'] = 'mutated by caller'
        reread = native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(reread['events'][0]['visible']['timers'][0]['label'], '0')
        self.assertEqual(before, (native.client.capability, native.client.revision, native.client.grenade_timers,
                                 native.client.weapon_actions, native.client.capability_deadline))
        self.assertEqual(native.client.commands, {})
        self.assertEqual(native.client.execute(**arguments(obj, action='choose_grenade_timer', args={'timer':'t2'}))['status'], 'dispatched')

    def test_changed_retained_timer_presentation_closes_without_action(self):
        native = self.fixture();native.client.observe()
        native.public_events = [dict(seq=1, kind='presentation', state='prime_grenade', visible=grenade_state()['visible'])]
        native.client.read_events(EPOCH, 0, 16)
        native.public_events[0]['visible']['timers'][0]['label'] = 'Different shown label'
        with self.assertRaises(Unavailable):native.client.read_events(EPOCH, 0, 16)
        self.assertEqual(native.client.state, 'CLOSED');self.assertFalse(native.client.grenade_timers)
        self.assertFalse(any(r.startswith(b'E ') for r in native.requests))

    def test_timeout_after_send_is_terminal_without_replay_or_status_poll(self):
        native = self.fixture(transform=lambda kind,obj: None if kind=='E' else obj, timeout=.03)
        request = arguments(native.client.observe(), action='choose_grenade_timer', args={'timer':'t0'})
        with self.assertRaises(a.ActionOutcomeUnknown):native.client.execute(**request)
        count = len(native.requests)
        for operation in (native.client.observe, lambda:native.client.execute(**request), lambda:native.client.read_result(COMMAND)):
            with self.assertRaises(Unavailable):operation()
        self.assertEqual(len(native.requests), count);self.assertEqual(native.client.state, 'CLOSED')
        self.assertEqual(sum(r.startswith(b'E ') for r in native.requests), 1)
        self.assertFalse(any(r.startswith(b'S ') for r in native.requests))

    def test_malformed_reply_after_send_is_private_terminal_uncertainty(self):
        native = self.fixture(transform=lambda kind,obj: b'{"hidden_fuse":"SECRET"}' if kind=='E' else obj)
        request = arguments(native.client.observe(), action='choose_grenade_timer', args={'timer':'t0'})
        with self.assertRaises(a.ActionOutcomeUnknown) as caught:native.client.execute(**request)
        self.assertEqual(caught.exception.command_id, COMMAND)
        self.assertNotIn('SECRET', json.dumps(caught.exception.public_error()))
        self.assertEqual(native.client.state, 'CLOSED');self.assertFalse(native.client.grenade_timers)

    def test_native_unknown_after_dispatch_is_never_success_or_action_replay(self):
        def transform(kind, obj):
            return result(obj['command_id'], 'unknown_after_dispatch', obj['event_sequence']) if kind=='E' else obj
        native = self.fixture(transform=transform);obj = native.client.observe()
        request = arguments(obj, action='choose_grenade_timer', args={'timer':'t0'})
        reply = native.client.execute(**request)
        self.assertEqual(reply['status'], 'unknown_after_dispatch');self.assertIsNone(reply['observation'])
        self.assertIsNone(native.client.capability);self.assertFalse(native.client.grenade_timers)
        self.assertEqual(sum(r.startswith(b'E ') for r in native.requests), 1)

    def test_schema12_handshake_rejected_before_any_action_or_read(self):
        for stage in ('hello', 'ready'):
            old = 'openxcom-agent-control/12'
            kwargs = ({'hello':dict(schema=old, mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output')}
                      if stage=='hello' else {'transform':lambda kind,obj:dict(obj, schema=old) if kind=='H' else obj})
            native = self.fixture(**kwargs)
            with self.assertRaises(Unavailable):native.client.observe()
            self.assertEqual(native.client.state, 'CLOSED')
            self.assertFalse(any(r.startswith((b'R ',b'E ')) for r in native.requests))

    def test_mcp_rejects_invalid_timer_schema_before_native_send(self):
        native = self.fixture();obj = native.client.observe();server = adapter.Server(agent_control=native.client)
        count = len(native.requests)
        for value in ('t24', True, 0, None):
            response = call(server, 'execute_ui_action', arguments(obj, action='choose_grenade_timer', args={'timer':value}))
            self.assertEqual(response['error']['code'], -32602)
        self.assertEqual(len(native.requests), count);self.assertEqual(native.client.commands, {})

    def test_timer_commands_share_the_existing_non_evicting_256_ledger(self):
        native = self.fixture()
        for index in range(256):
            obj = native.client.observe()
            native.client.execute(**arguments(obj, command_id=f'{index:032x}', action='choose_grenade_timer', args={'timer':'t0'}))
        self.assertEqual(len(native.client.commands), 256)
        obj = native.client.observe();count = len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**arguments(obj, command_id='f'*32, action='cancel_grenade_timer', args={}))
        self.assertEqual(len(native.requests), count);self.assertEqual(len(native.client.commands), 256)
        self.assertEqual(native.client.state, 'CLOSED')


if __name__=='__main__':unittest.main()
