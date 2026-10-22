"""Scripted public-MCP response tests; not native rules or live qualification."""
import copy
import unittest
from unittest.mock import patch
from test_allocation_route import Fake
from limits_route import LimitsRoute
from route import RouteError

class LimitsFake(Fake):
    def __init__(self,master='xcom1',available=10,space=30):
        super().__init__(False,available,space)
        self.master=master;self.title='Laser Pistol' if master=='xcom1' else 'Gauss Pistol'
        self.screen='manufacture';self.order=False;self.reopened=False;self.funds=100000
        self.probes=[];self.events=[];self.change_probe=False;self.bad_receipt=False
        self.bad_reopen=False;self.unoffered=False;self.unknown_action=None
    def call(self,name,args):
        if name=='read_ui_state':
            s=super().call(name,args);s['state']=self.screen
            s['visible'].update(title=self.title,sell={'pressed':False})
            if self.screen=='manufacture':
                s['visible']={'summary':[str(self.available),str(self.assigned),str(self.space),str(self.funds)],
                              'rows':[{'handle':'order','cells':[self.title,'7']}] if self.order else []}
                actions=['open_new_production','select_production_project']
            elif self.screen=='new_manufacture':
                s['visible']={'rows':[{'handle':'new','cells':[self.title]}]};actions=['select_production_project','close_production_list']
            elif self.screen=='manufacture_start':actions=['start_production']
            else:actions=['change_production_quantity','accept_production','stop_production']
            s['actions']=[{'id':a} for a in actions if not(self.unoffered and a=='change_production_quantity')]
            return s
        if name=='probe_invalid_quantity':
            self.probes.append(copy.deepcopy(args))
            if self.change_probe:self.units=1
            return {'status':'invalid_params_rejected','code':-1 if self.bad_receipt else -32602,'message':'Invalid params'}
        action=args['action'];self.sent.append(copy.deepcopy(args))
        if action==self.unknown_action:return {'status':'unknown_after_dispatch','observation':None}
        if action=='change_production_quantity':
            p=args['args']
            if p=={'control':'units','operation':'max'}:self.units='∞'
            else:
                self.sent.pop();super().call(name,args)
        elif action=='open_new_production':self.screen='new_manufacture'
        elif action=='select_production_project':
            self.reopened=self.screen=='manufacture'
            self.screen='manufacture_info' if self.reopened else 'manufacture_start'
            if self.reopened and self.bad_reopen:self.units=6
        elif action=='start_production':self.screen='manufacture_info'
        elif action=='accept_production':self.order=True;self.screen='new_manufacture';self.funds-=8000
        elif action=='close_production_list':self.screen='manufacture'
        elif action=='stop_production':self.order=False;self.screen='manufacture'
        else:raise AssertionError(action)
        return {'status':'dispatched','observation':{'state':self.screen}}
    def route(self):return LimitsRoute(self.call,lambda kind,**v:self.events.append((kind,v)),self.master,self.clock,self.sleep)

class LimitsTests(unittest.TestCase):
    def test_full_plan_both_masters_and_capacity_sources(self):
        for master in ('xcom1','xcom2'):
            for available,space in ((10,30),(30,10)):
                f=LimitsFake(master,available,space);r=f.route();r.run()
                self.assertEqual((f.available,f.space,f.assigned,f.units,f.order),(available,space,0,1,False))
                actions=[s['action'] for s in f.sent]
                self.assertEqual(actions.count('accept_production'),1)
                self.assertEqual(actions.count('stop_production'),1)
                self.assertNotIn('choose_geoscape_speed',actions)
                self.assertEqual(f.funds,92000)
                self.assertEqual(len(f.probes),1);self.assertEqual(f.probes[0]['args']['value'],100001)
                self.assertLessEqual(r.count,128)
                self.assertEqual(len({s['capability'] for s in f.sent}),len(f.sent))
                self.assertEqual(f.events[-1][0],'limits_phase_complete')
                self.assertFalse(f.events[-1][1]['inventory_verified'])
                self.assertEqual(f.events[-1][1]['requests'],30)
                self.assertNotIn('dispatches',f.events[-1][1])
                self.assertEqual(len(f.sent),29)
    def test_existing_order_refused_without_dispatch(self):
        f=LimitsFake();f.order=True
        with self.assertRaisesRegex(RouteError,'empty_production_list'):f.route().run()
        self.assertEqual(f.sent,[])
    def test_unknown_dispatch_stops_without_stop_or_replay(self):
        f=LimitsFake();f.unknown_action='change_production_quantity'
        with self.assertRaisesRegex(RouteError,'no_replay'):f.route().run()
        self.assertEqual([s['action'] for s in f.sent].count('change_production_quantity'),1)
        self.assertNotIn('stop_production',[s['action'] for s in f.sent])
    def test_unoffered_quantity_never_sent(self):
        f=LimitsFake();f.unoffered=True
        with self.assertRaisesRegex(RouteError,'not_offered'):f.route().run()
        self.assertNotIn('change_production_quantity',[s['action'] for s in f.sent])
    def test_bad_probe_and_changed_projection_stop(self):
        for attr in ('bad_receipt','change_probe'):
            f=LimitsFake();setattr(f,attr,True)
            with self.assertRaises(RouteError):f.route().run()
            self.assertEqual(len(f.probes),1)
            self.assertNotIn('accept_production',[s['action'] for s in f.sent])
    def test_reopen_quantity_loss_stops_not_silently_repaired(self):
        f=LimitsFake();f.bad_reopen=True
        with self.assertRaisesRegex(RouteError,'persisted_quantity'):f.route().run()
        self.assertNotIn('stop_production',[s['action'] for s in f.sent])
