"""Scripted transport regression only; not live or native transfer acceptance."""
import copy
import unittest
from transfer_route import TransferRoute
from route import RouteError

class Fake:
    def __init__(self):
        self.screen='geoscape';self.base=0;self.n=0;self.time=0;self.sent=[]
        self.names=['MCP Research','NEWBASE'];self.qty=[3,0];self.capacity=[50,0]
        self.status='dispatched';self.bad_heading=False;self.mutate_cancel=False;self.unoffered=False
    def clock(self):return self.time
    def sleep(self,n):self.time+=n
    def state(self):
        s=self.screen;b=self.base
        v={'base_name':self.names[b],'base_slots':[{'handle':'b0','selected':b==0},{'handle':'b1','selected':b==1}]}
        acts=[]
        if s=='geoscape':acts=['open_bases','open_pause_menu']
        elif s=='basescape':acts=['select_base','open_base_info','open_transfer','return_geoscape']
        elif s=='base_info':
            v['metrics']=[{'label':'Stores','value':str(self.qty[b])+':'+str(self.capacity[b])}];acts=['open_base_stores','close_base_info']
        elif s=='base_stores':
            v.update(rows=[{'cells':['Pistol',str(self.qty[b]),str(self.qty[b])]}] if self.qty[b] else [],scroll={'down':False});acts=['close_base_stores']
        elif s=='transfer_destination':
            v['rows']=[{'handle':'r0','cells':['NEWBASE','Europe']}];acts=['choose_visible_row']
        elif s=='transfer_items':
            v['rows']=[{'handle':'r0','cells':['Pistol',str(self.qty[0]),'0','0'],'controls':['increase','decrease']}];acts=['change_transfer_quantity','cancel_transfer_items']
        elif s=='error_message':v={'heading':'Other' if self.bad_heading else 'NOT ENOUGH STORE SPACE!','message':None};acts=['dismiss_error']
        if self.unoffered and s=='transfer_items':acts=['cancel_transfer_items']
        return dict(state=s,visible=v,actions=[{'id':a} for a in acts],epoch='e',revision=self.n,capability=str(self.n),challenge=str(self.n))
    def call(self,name,args):
        if name=='read_ui_state':self.n+=1;return self.state()
        self.sent.append(copy.deepcopy(args))
        action=args['action']
        if self.status!='dispatched':return {'status':self.status,'observation':{}}
        transitions={'open_bases':'basescape','open_base_info':'base_info','open_base_stores':'base_stores','close_base_stores':'base_info','close_base_info':'basescape','open_transfer':'transfer_destination','choose_visible_row':'transfer_items','change_transfer_quantity':'error_message','dismiss_error':'transfer_items','cancel_transfer_items':'basescape','return_geoscape':'geoscape','open_pause_menu':'pause_menu'}
        if action=='select_base':self.base=int(args['args']['target'][1:])
        else:self.screen=transitions[action]
        if action=='cancel_transfer_items' and self.mutate_cancel:self.qty[0]-=1
        return {'status':'dispatched','observation':self.state()}
    def route(self):return TransferRoute(self.call,lambda *a,**k:None,'xcom1',self.clock,self.sleep)

class Transfer(unittest.TestCase):
    def test_complete_zero_capacity_route(self):
        f=Fake();f.route().run()
        changes=[a for a in f.sent if a['action']=='change_transfer_quantity']
        self.assertEqual(len(changes),1)
        self.assertEqual(changes[0]['args'],{'row':'r0','operation':'adjust','value':1})
        self.assertNotIn('confirm_transfer',[a['action'] for a in f.sent])
        self.assertEqual(f.qty,[3,0]);self.assertEqual(f.screen,'pause_menu')
        self.assertEqual(len({a['command_id'] for a in f.sent}),len(f.sent))
    def test_nonzero_destination_capacity_stops_before_transfer(self):
        f=Fake();f.capacity[1]=1
        with self.assertRaisesRegex(RouteError,'zero_storage'):f.route().run()
        self.assertNotIn('open_transfer',[a['action'] for a in f.sent])
    def test_no_source_item_stops_before_transfer(self):
        f=Fake();f.qty[0]=0
        with self.assertRaisesRegex(RouteError,'positive_sized'):f.route().run()
    def test_other_warning_not_accepted(self):
        f=Fake();f.bad_heading=True
        with self.assertRaisesRegex(RouteError,'wrong_or_unknown'):f.route().run()
        self.assertNotIn('dismiss_error',[a['action'] for a in f.sent])
    def test_cancel_mutation_detected(self):
        f=Fake();f.mutate_cancel=True
        with self.assertRaisesRegex(RouteError,'cancel_changed'):f.route().run()
    def test_unknown_outcome_not_replayed(self):
        f=Fake();f.status='outcome_unknown'
        with self.assertRaisesRegex(RouteError,'no_replay'):f.route().run()
        self.assertEqual(len(f.sent),1)
    def test_unoffered_change_not_sent(self):
        f=Fake();f.unoffered=True
        with self.assertRaisesRegex(RouteError,'not_offered'):f.route().run()
        self.assertNotIn('change_transfer_quantity',[a['action'] for a in f.sent])
    def test_unknown_capacity_prevents_transfer(self):
        f=Fake();original=f.state
        def state():
            s=original()
            if f.screen=='base_info':s['visible']['metrics'][0]['value']=None
            return s
        f.state=state
        with self.assertRaisesRegex(RouteError,'capacity_unknown'):f.route().run()
        self.assertNotIn('open_transfer',[a['action'] for a in f.sent])
    def test_unknown_store_labels_prevent_zero(self):
        f=Fake();r=f.route();r.ready=lambda _:{'visible':{'rows':[{'cells':[None,'1','1']}],'scroll':{'down':False}}}
        with self.assertRaisesRegex(RouteError,'zero_baseline'):r.stores_item()
    def test_ambiguous_transfer_item_rejected(self):
        r=Fake().route()
        with self.assertRaisesRegex(RouteError,'unique_transfer_item'):r.transfer_row({'visible':{'rows':[{'cells':['Pistol']},{'cells':['Pistol']}]}})

if __name__=='__main__':unittest.main()
