# SPDX-License-Identifier: GPL-3.0-or-later
"""Inventory arrow contract tests, not native selection/TU/gameplay proof."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_inventory_control import inventory, InventoryFixture
from test_agent_control import arguments, dump, result, COMMAND, EPOCH
from test_agent_events import page, validate

class InventorySelectionTests(unittest.TestCase):
    def valid(self,obj):return a.validate_state(obj,obj['challenge'],obj['epoch'])
    def test_catalog_and_noarg_contract(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/66')
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(286,109))
        for action in a.INVENTORY_SELECTION_LABELS:
            self.assertEqual(a.ARGUMENT_KINDS[action],'none')
            self.assertIn(action,a.STATE_ACTIONS['inventory'])
            self.assertEqual([s for s,ids in a.STATE_ACTIONS.items() if action in ids],['inventory'])
            self.assertTrue(a.canonical_action_request(arguments(action=action,args={})).endswith(action.encode()))
            for extra in ({'unit':1},{'index':0},{'repeat':2},{'target':'v0'}):
                with self.assertRaises(Unavailable):a.canonical_action_request(arguments(action=action,args=extra))
    def test_arrow_presence_is_not_eligibility_or_roster(self):
        obj=inventory();obj['visible']['targets']=[]
        obj['actions']=[x for x in obj['actions'] if x['id'] in a.INVENTORY_SELECTION_LABELS]
        self.valid(obj)
        obj['visible']['held']=dict(status='presented',x=0,y=0,width=1,height=1);self.valid(obj)
        obj['visible']['time_units']='TIME UNITS> 0';self.valid(obj)
        for key in ('unit_id','roster','next_unit','eligible_count','can_switch'):
            bad=copy.deepcopy(obj);bad['visible'][key]=None
            with self.assertRaises(Unavailable):self.valid(bad)
    def test_labels_and_sources_closed(self):
        for action,label in a.INVENTORY_SELECTION_LABELS.items():
            for field,value in (('label',label+' alien'),('label_source','visible_text'),('argument_kind','choice')):
                obj=inventory();next(d for d in obj['actions'] if d['id']==action)[field]=value
                with self.assertRaises(Unavailable):self.valid(obj)
    def test_arrows_independently_absent(self):
        for absent in (set(a.INVENTORY_SELECTION_LABELS),{'previous_inventory_unit'},{'next_inventory_unit'}):
            obj=inventory();obj['actions']=[d for d in obj['actions'] if d['id'] not in absent];self.valid(obj)
    def test_stale_capability_no_send(self):
        native=InventoryFixture();self.addCleanup(native.close)
        old=native.client.observe();native.revision+=1;fresh=native.client.observe();count=len(native.requests)
        with self.assertRaises(Unavailable):native.client.execute(**arguments(old,action='next_inventory_unit',args={}))
        self.assertEqual(len(native.requests),count)
        self.assertEqual(native.client.capability,fresh['capability'])
    def test_duplicate_receipt_not_second_switch(self):
        native=InventoryFixture();self.addCleanup(native.close)
        obj=native.client.observe();request=arguments(obj,action='next_inventory_unit',args={})
        first=native.client.execute(**request)
        self.assertEqual(native.client.execute(**request),first)
        self.assertEqual(sum(x.startswith(b'E ') for x in native.requests),1)
        with self.assertRaises(Unavailable):native.client.execute(**dict(request,action='previous_inventory_unit'))
    def test_result_and_event_keep_visible_shape(self):
        obj=inventory(challenge=None,capability=None,event_sequence=2)
        a.validate_result(dump(result(observation=obj)),COMMAND,EPOCH)
        validate(page([dict(seq=1,kind='presentation',state='inventory',visible=obj['visible'])]))
    def test_previous_wire_rejected(self):
        obj=inventory();obj['schema']='openxcom-agent-control/53'
        with self.assertRaises(Unavailable):self.valid(obj)

if __name__=='__main__':unittest.main()
