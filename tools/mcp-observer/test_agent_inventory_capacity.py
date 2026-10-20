# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic /14 inventory capacity contracts; no GUI, game or build proof."""
import copy
import json
import unittest

import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import arguments, call, dump, result, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate
from test_agent_inventory_body import body_projection
from test_agent_inventory_control import InventoryFixture, inventory


class InventoryCapacityValidationTests(unittest.TestCase):
    def test_exact_identity_capacity_and_unchanged_surface(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/18')
        self.assertEqual(a.INVENTORY_TARGET_HANDLES, tuple(f'v{i}' for i in range(96)))
        self.assertEqual((len(a.ACTIONS), len(a.STATE_ACTIONS), len(a.definitions())), (82, 36, 4))
        self.assertEqual((a.MAX_COMMANDS, a.EVENT_CAPACITY, a.MAX_EVENTS), (256, 256, 16))
        self.assertEqual((a.MAX_PACKET, a.MAX_TEXT_PACKET, a.MAX_REQUEST, a.CAPABILITY_TTL), (49152, 32768, 512, 120))
        self.assertEqual(a.ERROR_SCHEMA, 'openxcom-mcp-agent-control-error/1')
        reply = adapter.Server(agent_control=object()).handle(dict(jsonrpc='2.0', id=1, method='initialize',
            params=dict(protocolVersion=adapter.PROTOCOL, capabilities={}, clientInfo=dict(name='test', version='1'))))
        self.assertEqual(reply['result']['serverInfo']['version'], '0.18.0')

    def test_full_stock_85_and_capacity_96_accepted_in_state_result_and_event(self):
        for count in (85, 96):
            with self.subTest(count=count):
                visible = body_projection(count)
                obj = inventory(visible)
                a.validate_state(obj, CHALLENGE, EPOCH)
                receipt = result(observation=inventory(visible, challenge=None, capability=None, event_sequence=2))
                a.validate_result(dump(receipt), COMMAND, EPOCH)
                validate(page([dict(seq=1, kind='presentation', state='inventory', visible=visible)]))
                self.assertEqual(len(obj['visible']), 9)
                self.assertEqual(set(visible['targets'][-1]), {'handle','area','label','x','y','width','height','point','item'})
                self.assertEqual({target['area'] for target in visible['targets']}, {'hand','ground','body'})

    def test_97_rejected_in_state_result_and_event(self):
        visible = body_projection(97)
        with self.assertRaises(Unavailable):a.validate_state(inventory(visible), CHALLENGE, EPOCH)
        receipt = result(observation=inventory(visible, challenge=None, capability=None, event_sequence=2))
        with self.assertRaises(Unavailable):a.validate_result(dump(receipt), COMMAND, EPOCH)
        with self.assertRaises(Unavailable):validate(page([dict(seq=1, kind='presentation', state='inventory', visible=visible)]))

    def test_v95_canonical_suffix_and_exact_target_schema(self):
        schema = next(tool for tool in a.definitions() if tool['name']=='execute_ui_action')['inputSchema']
        for action in ('inventory_hover', 'inventory_activate'):
            request = arguments(action=action, args={'target':'v95'})
            expected = f"E {COMMAND} {EPOCH} {request['capability']} 1 {action} v95".encode()
            self.assertEqual(a.canonical_action_request(request), expected)
            branch = next(branch for branch in schema['oneOf'] if action in branch['properties']['action']['enum'])
            args = branch['properties']['args']
            self.assertEqual(args['required'], ['target']);self.assertFalse(args['additionalProperties'])
            self.assertEqual(args['properties'], {'target':{'type':'string','enum':[f'v{i}' for i in range(96)]}})

    def test_v96_and_noncanonical_extended_handles_are_malformed(self):
        for target in ('v96', 'v095', 'v95 ', 'v95\n', 'V95', 'v-1', 95, False, None):
            for action in ('inventory_hover', 'inventory_activate'):
                with self.subTest(target=target, action=action), self.assertRaises(Unavailable):
                    a.canonical_action_request(arguments(action=action, args={'target':target}))
        for target in ('v96', 'v094', 'v94'):
            visible = body_projection(96);visible['targets'][-1]['handle'] = target
            with self.assertRaises(Unavailable):a.validate_state(inventory(visible), CHALLENGE, EPOCH)

    def test_extended_targets_keep_geometry_enums_and_unknown_semantics(self):
        for key, value in (('area','belt'), ('item','empty'), ('item','weapon'), ('x',True),
                           ('x',640), ('y',400), ('width',0), ('height',401), ('slot_id','STR_BELT')):
            visible = body_projection(96);visible['targets'][-1][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(Unavailable):
                a.validate_state(inventory(visible), CHALLENGE, EPOCH)
        visible = body_projection(96);target = visible['targets'][-1]
        target['point']['x'] = target['x'] + target['width']
        with self.assertRaises(Unavailable):a.validate_state(inventory(visible), CHALLENGE, EPOCH)

    def test_all_earlier_wires_including_13_rejected_at_every_envelope(self):
        for version in range(1,14):
            old = f'openxcom-agent-control/{version}'
            with self.subTest(version=version):
                with self.assertRaises(Unavailable):
                    a.validate_hello(dump(dict(schema=old, mode=a.MODE, epoch=EPOCH, ledger_capacity=256, audio_history='mixed_output')))
                with self.assertRaises(Unavailable):a.validate_ready(dump(dict(schema=old, mode=a.MODE, status='ready')))
                obj = inventory(body_projection(96));obj['schema'] = old
                with self.assertRaises(Unavailable):a.validate_state(obj, CHALLENGE, EPOCH)
                obj = result();obj['schema'] = old
                with self.assertRaises(Unavailable):a.validate_result(dump(obj), COMMAND, EPOCH)
                obj = page([]);obj['schema'] = old
                with self.assertRaises(Unavailable):validate(obj)
                obj = result(observation=inventory(body_projection(96), challenge=None, capability=None, event_sequence=2))
                obj['observation']['schema'] = old
                with self.assertRaises(Unavailable):a.validate_result(dump(obj), COMMAND, EPOCH)


class InventoryCapacityClientTests(unittest.TestCase):
    def fixture(self, count=96, **kwargs):
        native = InventoryFixture(**kwargs);native.visible = body_projection(count)
        self.addCleanup(native.close)
        return native

    def test_fresh_v95_hover_and_activate_admitted_for_presented_and_unknown(self):
        for action in ('inventory_hover', 'inventory_activate'):
            for item in ('presented', 'unknown'):
                with self.subTest(action=action, item=item):
                    native = self.fixture();native.visible['targets'][-1]['item'] = item
                    obj = native.client.observe();request = arguments(obj, action=action, args={'target':'v95'})
                    self.assertEqual(native.client.execute(**request)['status'], 'dispatched')
                    self.assertEqual(native.requests[-1], a.canonical_action_request(request))
                    self.assertEqual(native.client.inventory_targets, frozenset())
                    self.assertIsNone(native.client.capability)

    def test_unlisted_v95_and_malformed_v96_do_not_send_or_spend_grant(self):
        native = self.fixture(85);obj = native.client.observe();count = len(native.requests)
        for target in ('v95', 'v96'):
            for action in ('inventory_hover', 'inventory_activate'):
                with self.assertRaises(Unavailable):native.client.execute(**arguments(obj, action=action, args={'target':target}))
        self.assertEqual(len(native.requests), count);self.assertEqual(native.client.commands, {})
        self.assertEqual(native.client.capability, obj['capability'])

    def test_v95_requires_current_fresh_listing_and_rejects_mutable_forgery(self):
        native = self.fixture();old = native.client.observe()
        native.visible = body_projection(85);native.revision += 1
        fresh = native.client.observe();count = len(native.requests)
        fresh['visible']['targets'].extend(copy.deepcopy(old['visible']['targets'][85:]))
        for obj in (old, fresh):
            with self.assertRaises(Unavailable):native.client.execute(**arguments(obj, action='inventory_activate', args={'target':'v95'}))
        self.assertEqual(len(native.requests), count);self.assertEqual(native.client.commands, {})
        native.visible = body_projection(96);native.revision += 1
        latest = native.client.observe()
        self.assertEqual(native.client.execute(**arguments(latest, action='inventory_activate', args={'target':'v95'}))['status'], 'dispatched')

    def test_history_listing_cannot_admit_v95(self):
        native = self.fixture(85);obj = native.client.observe()
        native.public_events = [dict(seq=1, kind='presentation', state='inventory', visible=body_projection(96))]
        before = (native.client.capability, native.client.revision, native.client.inventory_targets, native.client.capability_deadline)
        events = native.client.read_events(EPOCH,0,16)
        self.assertEqual(events['events'][0]['visible']['targets'][-1]['handle'], 'v95')
        count = len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**arguments(obj, action='inventory_hover', args={'target':'v95'}))
        self.assertEqual(len(native.requests), count);self.assertEqual(native.client.commands, {})
        self.assertEqual(before, (native.client.capability, native.client.revision, native.client.inventory_targets, native.client.capability_deadline))

    def test_v95_duplicate_queries_status_changed_args_conflict_preserves_new_grant(self):
        native = self.fixture();obj = native.client.observe();request = arguments(obj, action='inventory_activate', args={'target':'v95'})
        first = native.client.execute(**request);fresh = native.client.observe();count = len(native.requests)
        for change in (dict(args={'target':'v94'}), dict(action='inventory_hover')):
            with self.assertRaises(Unavailable):native.client.execute(**dict(request, **change))
        self.assertEqual(len(native.requests), count)
        self.assertEqual(native.client.execute(**request), first)
        self.assertEqual(native.requests[count:], [('S '+COMMAND).encode()])
        self.assertEqual(native.client.capability, fresh['capability'])
        self.assertIn('v95', native.client.inventory_targets)
        self.assertEqual(sum(raw.startswith(b'E ') for raw in native.requests), 1)

    def test_wire13_hello_closes_before_request(self):
        native = self.fixture(hello=dict(schema='openxcom-agent-control/13', mode=a.MODE, epoch=EPOCH,
                                       ledger_capacity=256, audio_history='mixed_output'))
        with self.assertRaises(Unavailable):native.client.observe()
        self.assertEqual(native.requests, []);self.assertEqual(native.client.state, 'CLOSED')

    def test_wire13_ready_state_and_events_close_without_new_authority(self):
        for boundary in ('H', 'R', 'V'):
            with self.subTest(boundary=boundary):
                native = self.fixture(transform=lambda op,obj:dict(obj,schema='openxcom-agent-control/13') if op==boundary else obj)
                if boundary=='V':
                    native.client.observe()
                    with self.assertRaises(Unavailable):native.client.read_events(EPOCH,0,16)
                else:
                    with self.assertRaises(Unavailable):native.client.observe()
                self.assertEqual(native.client.state, 'CLOSED');self.assertEqual(native.client.commands, {})
                self.assertEqual(native.client.inventory_targets, frozenset())

    def test_wire13_or_97_target_receipt_is_terminal_unknown_without_replay(self):
        for malformed in ('wire13', '97targets'):
            def transform(op, obj):
                if op=='E':
                    if malformed=='wire13':obj['schema'] = 'openxcom-agent-control/13'
                    else:obj['observation']['visible'] = body_projection(97)
                return obj
            native = self.fixture(transform=transform);obj = native.client.observe()
            request = arguments(obj, action='inventory_activate', args={'target':'v95'})
            with self.assertRaises(a.ActionOutcomeUnknown) as error:native.client.execute(**request)
            self.assertFalse(error.exception.public_error()['retryable'])
            count = len(native.requests)
            for operation in (native.client.observe, lambda:native.client.execute(**request),
                              lambda:native.client.read_result(COMMAND), lambda:native.client.read_events(EPOCH,0,16)):
                with self.assertRaises(Unavailable):operation()
            self.assertEqual(len(native.requests), count);self.assertEqual(native.client.state, 'CLOSED')
            self.assertEqual(sum(raw.startswith(b'E ') for raw in native.requests), 1)

    def test_all_four_semantic_tools_stay_image_free_at_full_capacity(self):
        native = self.fixture();native.public_events = [dict(seq=1,kind='presentation',state='inventory',visible=body_projection(96))]
        server = adapter.Server(agent_control=native.client)
        observed = call(server,'read_ui_state')['result']
        executed = call(server,'execute_ui_action',arguments(observed['structuredContent'],action='inventory_hover',args={'target':'v95'}))['result']
        status = call(server,'read_ui_action_result',{'command_id':COMMAND})['result']
        history = call(server,'read_public_events',{'epoch':EPOCH,'after':0,'limit':16})['result']
        for reply in (observed,executed,status,history):
            self.assertFalse(reply['isError']);self.assertEqual([item['type'] for item in reply['content']], ['text'])
            self.assertEqual(json.loads(reply['content'][0]['text']), reply['structuredContent'])


if __name__=='__main__':unittest.main()
