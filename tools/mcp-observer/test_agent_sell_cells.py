# SPDX-License-Identifier: GPL-3.0-or-later
"""Sell62 native-cell acceptance without changing wire58 or sale authority."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, NativeFixture, arguments, dump, result, COMMAND, EPOCH
from test_agent_events import page, validate

class SellCellTests(unittest.TestCase):
    def valid(self,obj):return a.validate_state(obj,obj['challenge'],obj['epoch'])
    def test_current_schema_unchanged(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/66');self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(286,109))
    def test_name_value_independent_unknown_and_shown(self):
        for name in (None,'Rifle Clip','Aquanaut'):
            for value in (None,'$1,234','$0'):
                obj=state(screen='sell');obj['visible']['rows'][0]['cells']=[name,'5','0',value];self.valid(obj)
                self.assertEqual(obj['visible']['rows'][0]['cells'],[name,'5','0',value])
    def test_quantities_remain_independently_nullable_strings(self):
        for remaining in (None,'5','0'):
            for draft in (None,'0','1','5'):
                obj=state(screen='sell');obj['visible']['rows'][0]['cells']=['Rifle Clip',remaining,draft,'$1,234'];self.valid(obj)
        obj=state(screen='sell');obj['visible']['rows'][0]['cells']=['Rifle Clip',5,0,'$1,234']
        with self.assertRaises(Unavailable):self.valid(obj)
    def test_no_extra_price_or_identity_fields(self):
        for key in ('item_id','unit_price','sell_cost','stock','selected_count','rule'):
            obj=state(screen='sell');obj['visible']['rows'][0][key]=0
            with self.assertRaises(Unavailable):self.valid(obj)
    def test_caption_null_does_not_remove_painted_arrow_authority(self):
        obj=state(screen='sell');obj['visible']['rows'][0]['cells']=[None]*4;before=copy.deepcopy(obj['actions']);self.valid(obj);self.assertEqual(obj['actions'],before)
    def test_result_and_event_accept_current_cells(self):
        obj=state(screen='sell',challenge=None,capability=None,event_sequence=2);obj['visible']['rows'][0]['cells']=['Rifle Clip','5','1','$1,234']
        a.validate_result(dump(result(observation=obj)),COMMAND,EPOCH);validate(page([dict(seq=1,kind='presentation',state='sell',visible=obj['visible'])]))
    def test_same_caption_not_stale_row_identity(self):
        peer=NativeFixture(screen='sell');self.addCleanup(peer.close)
        old=peer.client.observe();peer.revision+=1;fresh=peer.client.observe();count=len(peer.requests)
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='change_sale_quantity',args=dict(row='r0',operation='adjust',value=1)))
        self.assertEqual(len(peer.requests),count);self.assertEqual(peer.client.capability,fresh['capability'])
    def test_duplicate_draft_adjustment_never_double_sells(self):
        peer=NativeFixture(screen='sell');self.addCleanup(peer.close);obj=peer.client.observe();request=arguments(obj,action='change_sale_quantity',args=dict(row='r0',operation='adjust',value=1))
        receipt=peer.client.execute(**request);self.assertEqual(peer.client.execute(**request),receipt);self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)
        self.assertFalse(any(b'accept_sell' in x for x in peer.requests))

if __name__=='__main__':unittest.main()
