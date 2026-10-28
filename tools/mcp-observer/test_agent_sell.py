# SPDX-License-Identifier: GPL-3.0-or-later
"""Painted Sell/Sack authority only. Synthetic tests are not native/live proof."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, arguments, NativeFixture, CHALLENGE, EPOCH

ACTIONS = ('open_sell','change_sale_quantity','accept_sell','cancel_sell','open_sell_category','scroll_sell')
class SellTests(unittest.TestCase):
    def check(self,obj): return a.validate_state(obj,CHALLENGE,EPOCH)
    def test_closed_shape_no_hidden_data_nullable_text(self):
        for screen in ('sell','sell_category'):
            obj=state(screen=screen);self.check(obj)
            for k in obj['visible']:
                bad=copy.deepcopy(obj);del bad['visible'][k]
                with self.assertRaises(Unavailable):self.check(bad)
            for k in ('item_id','available_count','item_type','inventory','success'):
                bad=copy.deepcopy(obj);bad['visible'][k]=None
                with self.assertRaises(Unavailable):self.check(bad)
        obj=state(screen='sell')
        for k in ('title','funds','total','space_used'):obj['visible'][k]=None
        obj['visible']['headings']=[None]*3;obj['visible']['rows'][0]['cells']=[None]*4
        self.check(obj)
        for h in ([None]*2,[None]*4):
            obj['visible']['headings']=h
            with self.assertRaises(Unavailable):self.check(obj)
    def test_one_step_only_arguments(self):
        for n in (-1,1):a.validate_action_arguments(arguments(action='change_sale_quantity',args=dict(row='r0',operation='adjust',value=n)))
        for n in (-2,0,2,True,'1',None):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='change_sale_quantity',args=dict(row='r0',operation='adjust',value=n)))
        for op in ('set','max','clear'):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='change_sale_quantity',args=dict(row='r0',operation=op,value=1)))
        for amount in ('page','end',1):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='scroll_sell',args=dict(direction='down',amount=amount)))
        for action in ('open_sell','open_sell_category','accept_sell','cancel_sell'):
            a.validate_action_arguments(arguments(action=action,args={}))
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args={'repeat':1}))
    def test_missing_authority_and_malformed_rows(self):
        for key,value,action in [('rows',[],'change_sale_quantity'),('category',None,'open_sell_category'),('scroll',dict(up=False,down=False),'scroll_sell'),('buttons',[],'accept_sell')]:
            obj=state(screen='sell');obj['visible'][key]=value
            with self.assertRaises(Unavailable):self.check(obj)
            if key=='buttons':obj['actions']=[x for x in obj['actions'] if x['id'] not in ('accept_sell','cancel_sell')]
            else:obj['actions']=[x for x in obj['actions'] if x['id']!=action]
            self.check(obj)
        for k,v in [('handle','r1'),('cells',[None]*3),('controls',[]),('controls',['max']),('selected',1)]:
            obj=state(screen='sell');obj['visible']['rows'][0][k]=v
            with self.assertRaises(Unavailable):self.check(obj)
        obj=state(screen='sell');obj['visible']['scrollbar']={'targets':[]}
        with self.assertRaises(Unavailable):self.check(obj)
    def test_exact_state_semantics_and_old_schema(self):
        for screen in a.STATE_ACTIONS:
            for action in set(ACTIONS)-a.STATE_ACTIONS[screen]:
                obj=state(screen=screen);obj['actions']=[dict(id=action,label='Shown',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action])]
                with self.assertRaises(Unavailable):self.check(obj)
        for descriptor in state(screen='sell')['actions']:
            obj=state(screen='sell');next(x for x in obj['actions'] if x['id']==descriptor['id'])['label_source']='model'
            with self.assertRaises(Unavailable):self.check(obj)
        obj=state(screen='sell');obj['schema']='openxcom-agent-control/30'
        with self.assertRaises(Unavailable):self.check(obj)
    def test_stale_and_unoffered_direction_zero_send(self):
        for stale in (True,False):
            def transform(op,obj):
                if 'visible' in obj and obj.get('state')=='sell':obj['visible']['rows'][0]['controls']=['increase']
                return obj
            peer=NativeFixture(screen='sell',transform=transform)
            try:
                old=peer.client.observe()
                if stale:peer.client.observe()
                with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='change_sale_quantity',args=dict(row='r0',operation='adjust',value=-1 if not stale else 1)))
                self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
            finally:peer.close()
    def test_malformed_arguments_zero_executor_send(self):
        peer=NativeFixture(screen='sell')
        try:
            for args in (dict(row='r0',operation='adjust',value=2),dict(row='r0',operation='set',value=1),dict(row='r0',operation='adjust',value=1,hidden_id='x')):
                obj=peer.client.observe()
                with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='change_sale_quantity',args=args))
            self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
        finally:peer.close()
    def test_dispatch_single_step_and_category(self):
        for screen,action,args in [('sell','change_sale_quantity',dict(row='r0',operation='adjust',value=1)),('sell','scroll_sell',dict(direction='down',amount='step')),('sell_category','choose_visible_row',dict(row='r0'))]:
            peer=NativeFixture(screen=screen)
            try:
                obj=peer.client.observe();peer.client.execute(**arguments(obj,action=action,args=args))
                self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)
            finally:peer.close()
if __name__=='__main__':unittest.main()
