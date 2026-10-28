# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-18.
"""Synthetic /12 body targets; these fixtures prove no actual inventory outcome."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import arguments, result, dump, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate
from test_agent_inventory_control import InventoryFixture, inventory, projection


def body_projection(count=3):
    visible = projection()
    visible['targets'] = [dict(handle=f'v{i}', area=('hand', 'ground', 'body')[i % 3],
        label=None if i % 2 else 'BACK PACK', x=(i % 16) * 16, y=(i // 16) * 16,
        width=16, height=16, point=dict(x=(i % 16) * 16 + 8, y=(i // 16) * 16 + 8),
        item='presented' if i % 2 else 'unknown') for i in range(count)]
    return visible


class BodyValidationTests(unittest.TestCase):
    def valid(self, visible):
        return a.validate_state(inventory(visible), CHALLENGE, EPOCH)

    def test_version_and_unchanged_authority_surface(self):
        self.assertEqual(a.SCHEMA, 'openxcom-agent-control/36')
        self.assertEqual((len(a.ACTIONS), len(a.STATE_ACTIONS), len(a.definitions())), (188, 81, 5))
        self.assertEqual(a.MAX_COMMANDS, 256)
        self.assertEqual(a.MAX_PACKET, 49152)
        self.assertEqual(a.MAX_REQUEST, 512)
        self.assertEqual(a.CAPABILITY_TTL, 120)
        self.assertNotIn('choose_fire_mode', a.ACTIONS)

    def test_three_areas_in_state_result_and_retained_event(self):
        visible = body_projection()
        self.valid(visible)
        observation = inventory(visible, challenge=None, capability=None, event_sequence=2)
        a.validate_result(dump(result(observation=observation)), COMMAND, EPOCH)
        validate(page([dict(seq=1, kind='presentation', state='inventory', visible=visible)]))

    def test_stock_85_and_abi_96_are_valid_97_rejected(self):
        for count in (0, 1, 63, 64, 85, 96): self.valid(body_projection(count))
        with self.assertRaises(Unavailable): self.valid(body_projection(97))

    def test_area_is_closed_no_slot_or_item_model_export(self):
        for value in ('belt', 'backpack', 'BODY', 'body ', '', None, 0, False):
            visible = body_projection(); visible['targets'][2]['area'] = value
            with self.subTest(value=value), self.assertRaises(Unavailable): self.valid(visible)
        for key in ('slot_id', 'item_id', 'compatible', 'capacity', 'absolute_index', 'page_index'):
            visible = body_projection(); visible['targets'][2][key] = 1
            with self.subTest(key=key), self.assertRaises(Unavailable): self.valid(visible)

    def test_body_geometry_strict_and_point_within_complete_rectangle(self):
        for key, value in (('x', True), ('y', 1.0), ('width', 0), ('height', -1),
                           ('x', 640), ('y', 400), ('width', 641), ('height', 401)):
            visible = body_projection(); visible['targets'][2][key] = value
            with self.subTest(key=key,value=value), self.assertRaises(Unavailable): self.valid(visible)
        for point in ({'x':31,'y':8}, {'x':48,'y':8}, {'x':40,'y':16}, {'x':40,'y':True}):
            visible=body_projection();visible['targets'][2]['point']=point
            with self.subTest(point=point),self.assertRaises(Unavailable):self.valid(visible)

    def test_body_labels_nullable_utf8_bounded_and_unknown_not_empty(self):
        for text in (None, '', 'Alien Artifact', 'é'*512):
            visible=body_projection();visible['targets'][2]['label']=text;self.valid(visible)
        for key,value in (('label','é'*513),('label','\ud800'),('item','empty'),('item','weapon'),('item',False)):
            visible=body_projection();visible['targets'][2][key]=value
            with self.subTest(key=key),self.assertRaises(Unavailable):self.valid(visible)

    def test_handles_sequential_local_and_extra_descriptor_rejected(self):
        for handle in ('v0','v03','v96','g2','body0'):
            visible=body_projection();visible['targets'][2]['handle']=handle
            with self.subTest(handle=handle),self.assertRaises(Unavailable):self.valid(visible)
        obj=inventory(body_projection());obj['actions'][0]['preconditions']={'slot_empty':True}
        with self.assertRaises(Unavailable):a.validate_state(obj,CHALLENGE,EPOCH)

    def test_prior_version_ready_is_rejected_before_read(self):
        native=InventoryFixture(transform=lambda operation,value: dict(value,schema='openxcom-agent-control/11') if operation=='H' else value)
        self.addCleanup(native.close)
        with self.assertRaises(Unavailable):native.client.observe()
        self.assertEqual(native.requests,[b'H openxcom-agent-control/36'])
        self.assertEqual(native.client.state,'CLOSED')


class BodyClientTests(unittest.TestCase):
    def fixture(self, **kwargs):
        native=InventoryFixture(**kwargs);native.visible=body_projection();self.addCleanup(native.close);return native

    def test_fresh_body_hover_activate_and_exact_canonical_suffix(self):
        native=self.fixture()
        for index,action in enumerate(('inventory_hover','inventory_activate')):
            obj=native.client.observe();args=arguments(obj,command_id=f'{index+1:032x}',action=action,args={'target':'v2'})
            self.assertEqual(native.client.execute(**args)['status'],'dispatched')
            self.assertTrue(native.requests[-1].endswith((action+' v2').encode()))

    def test_forged_body_target_and_json_mutation_cannot_add_authority(self):
        native=self.fixture();obj=native.client.observe();count=len(native.requests)
        obj['visible']['targets'].append(dict(obj['visible']['targets'][2],handle='v3'))
        for target in ('v3','v95'):
            with self.assertRaises(Unavailable):native.client.execute(**arguments(obj,action='inventory_activate',args={'target':target}))
        self.assertEqual(len(native.requests),count);self.assertEqual(native.client.commands,{})
        self.assertEqual(native.client.capability,obj['capability'])

    def test_page_grant_stale_same_handle_fresh_means_current_projection(self):
        native=self.fixture();old=native.client.observe()
        native.visible['targets'][2]['label']='BELT';native.revision+=1
        fresh=native.client.observe();count=len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**arguments(old,action='inventory_hover',args={'target':'v2'}))
        self.assertEqual(len(native.requests),count)
        self.assertEqual(native.client.execute(**arguments(fresh,action='inventory_hover',args={'target':'v2'}))['status'],'dispatched')

    def test_duplicate_status_only_changed_target_conflict(self):
        native=self.fixture();obj=native.client.observe();args=arguments(obj,action='inventory_activate',args={'target':'v2'})
        original=native.client.execute(**args);count=len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**dict(args,args={'target':'v1'}))
        self.assertEqual(len(native.requests),count)
        self.assertEqual(native.client.execute(**args),original)
        self.assertEqual(native.requests[count:],[('S '+COMMAND).encode()])

    def test_body_history_read_is_immutable_and_grant_neutral(self):
        native=self.fixture();obj=native.client.observe()
        native.public_events=[dict(seq=1,kind='presentation',state='inventory',visible=body_projection())]
        before=(native.client.capability,native.client.revision,native.client.inventory_targets,native.client.capability_deadline)
        history=native.client.read_events(EPOCH,0,16);history['events'][0]['visible']['targets'][2]['item']='empty'
        again=native.client.read_events(EPOCH,0,16)
        self.assertEqual(again['events'][0]['visible']['targets'][2]['item'],'unknown')
        self.assertEqual(before,(native.client.capability,native.client.revision,native.client.inventory_targets,native.client.capability_deadline))
        self.assertEqual(native.client.commands,{})
        self.assertEqual(native.client.execute(**arguments(obj,action='inventory_hover',args={'target':'v2'}))['status'],'dispatched')

    def test_malformed_body_after_send_is_unknown_terminal_never_replayed(self):
        def transform(operation,value):
            if operation=='E':value['observation']['visible']['targets'][2]['area']='backpack'
            return value
        native=self.fixture(transform=transform);obj=native.client.observe();args=arguments(obj,action='inventory_activate',args={'target':'v2'})
        with self.assertRaises(a.ActionOutcomeUnknown):native.client.execute(**args)
        count=len(native.requests)
        for operation in (native.client.observe,lambda:native.client.execute(**args),lambda:native.client.read_result(COMMAND)):
            with self.assertRaises(Unavailable):operation()
        self.assertEqual(len(native.requests),count);self.assertEqual(native.client.state,'CLOSED')


if __name__=='__main__':unittest.main()
