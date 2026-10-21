"""Scripted MCP transport tests, not native or live game evidence."""
import copy
import unittest
from allocation_route import AllocationRoute
from route import RouteError

class Fake:
    def __init__(self, scientists=True, available=10, space=30):
        self.scientists=scientists; self.available=available;self.space=space
        self.assigned=0;self.units=1;self.n=0;self.time=0;self.sent=[]
        self.status='dispatched';self.null=False;self.offered=True;self.broken=False
    def clock(self):return self.time
    def sleep(self,t):self.time+=t
    def call(self,name,args):
        if name=='read_ui_state':
            self.n+=1
            return dict(state='research_info' if self.scientists else 'manufacture_info',
                visible=dict(title='Test',available=str(self.available),space=str(self.space),
                             allocated=None if self.null else str(self.assigned),units=str(self.units)),
                actions=[{'id':x} for x in (['adjust_scientists'] if self.scientists else ['adjust_engineers','change_production_quantity'])] if self.offered else [],
                epoch='e',revision=self.n,challenge=str(self.n),capability=str(self.n))
        assert name=='execute_ui_action'
        self.sent.append(copy.deepcopy(args))
        if self.status=='dispatched' and not self.broken:
            p=args['args'];action=args['action']
            if action in ('adjust_scientists','adjust_engineers'):
                n=min(self.available,self.space,1) if p['direction']=='increase' else -min(self.assigned,1)
                self.assigned+=n;self.available-=n;self.space-=n
            else:
                control,op=p['control'],p['operation']; value=p.get('value',0)
                if control=='engineers':
                    target=0 if op=='clear' else self.assigned+min(self.available,self.space) if op=='max' else value if op=='set' else self.assigned+value
                    target=max(0,min(self.assigned+min(self.available,self.space),target))
                    n=target-self.assigned;self.assigned=target;self.available-=n;self.space-=n
                else:self.units=1 if op=='clear' else max(1,value if op=='set' else self.units+value)
        return dict(status=self.status,observation={'state':'research_info' if self.scientists else 'manufacture_info'})
    def route(self,master='xcom1'):
        return AllocationRoute(self.call,lambda *a,**k:None,master,self.clock,self.sleep)

class Matrix(unittest.TestCase):
    def test_staff_modes_and_capacity_limits(self):
        for master in ('xcom1','xcom2'):
            for scientists in (True,False):
                for available,space in ((10,30),(20,3),(1,1)):
                    with self.subTest(master=master,scientists=scientists,available=available,space=space):
                        f=Fake(scientists,available,space);f.route(master).staff_matrix('Test',scientists)
                        limit=min(available,space)
                        self.assertEqual(len(f.sent),2*limit+2)
                        self.assertEqual([s['args']['direction'] for s in f.sent],['increase']*(limit+1)+['decrease']*(limit+1))
                        self.assertEqual((f.available,f.space,f.assigned),(available,space,0))
                        self.assertEqual(len({s['command_id'] for s in f.sent}),len(f.sent))
                        self.assertEqual(len({s['capability'] for s in f.sent}),len(f.sent))
    def test_exact_production_requests(self):
        f=Fake(False);f.route().production_finite_matrix('Test')
        self.assertEqual([s['args'] for s in f.sent],[
            {'control':'engineers','operation':'set','value':n} for n in (1,7,100000)]+[
            {'control':'engineers','operation':'max'},
            {'control':'engineers','operation':'adjust','value':1},
            {'control':'engineers','operation':'clear'}]+[
            {'control':'units','operation':'set','value':n} for n in (1,7,1000,100000)]+[
            {'control':'units','operation':'adjust','value':-1000},
            {'control':'units','operation':'clear'}])
        self.assertEqual((f.available,f.space,f.assigned,f.units),(10,30,0,1))
    def test_unknown_stops_without_replay(self):
        for method in ('staff_matrix','production_finite_matrix'):
            f=Fake(method=='staff_matrix');f.status='outcome_unknown'
            with self.assertRaisesRegex(RouteError,'no_replay'):getattr(f.route(),method)('Test')
            self.assertEqual(len(f.sent),1)
    def test_unknown_value_prevents_dispatch(self):
        f=Fake();f.null=True
        with self.assertRaises(RouteError):f.route().staff_matrix('Test')
        self.assertEqual(f.sent,[])
    def test_wrong_title_prevents_dispatch(self):
        f=Fake(False)
        with self.assertRaises(RouteError):f.route().production_finite_matrix('Other')
        self.assertEqual(f.sent,[])
    def test_unoffered_prevents_dispatch(self):
        f=Fake();f.offered=False
        with self.assertRaisesRegex(RouteError,'not_offered'):f.route().staff_matrix('Test')
        self.assertEqual(f.sent,[])
    def test_wrong_postcondition_stops(self):
        f=Fake();f.broken=True
        with self.assertRaisesRegex(RouteError,'postcondition'):f.route().staff_matrix('Test')
        self.assertEqual(len(f.sent),1)
    def test_large_single_step_limit_stops_before_any_mutation(self):
        f=Fake(available=100,space=100)
        with self.assertRaisesRegex(RouteError,'bounded_route'):f.route().staff_matrix('Test')
        self.assertEqual(f.sent,[])
    def test_nonzero_initial_staff_not_destroyed(self):
        f=Fake();f.assigned=2
        with self.assertRaises(RouteError):f.route().staff_matrix('Test')
        self.assertEqual(f.sent,[])
    def test_remaining_budget_checked_before_mutation(self):
        for scientists,method in ((True,'staff_matrix'),(False,'production_finite_matrix')):
            f=Fake(scientists);r=f.route();r.count=120
            with self.assertRaisesRegex(RouteError,'remaining_budget'):getattr(r,method)('Test')
            self.assertEqual(f.sent,[])
    def test_existing_quantity_not_destroyed(self):
        f=Fake(False);f.units=8
        with self.assertRaises(RouteError):f.route().production_finite_matrix('Test')
        self.assertEqual(f.sent,[])

if __name__=='__main__':unittest.main()
