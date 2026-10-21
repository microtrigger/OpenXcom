import copy
import unittest
from route import Route,RouteError,amount

class Fake:
    def __init__(self,actions=('go',),status='dispatched'):
        self.n=0;self.sent=[];self.actions=actions;self.status=status;self.time=0
    def clock(self):return self.time
    def sleep(self,n):self.time+=n
    def call(self,name,args):
        if name=='read_ui_state':
            self.n+=1
            return dict(state='example',visible={},actions=[{'id':x} for x in self.actions],
                        epoch='e',capability=str(self.n),revision=self.n,challenge=str(self.n))
        self.sent.append(copy.deepcopy(args))
        return dict(status=self.status,observation={'state':'example'})
    def route(self):return Route(self.call,lambda *a,**k:None,'xcom1',self.clock,self.sleep)

class Guards(unittest.TestCase):
    def test_offer_required_no_send(self):
        f=Fake(actions=())
        with self.assertRaisesRegex(RouteError,'not_offered'):f.route().act({'example'},'go')
        self.assertEqual(f.sent,[])
    def test_unknown_no_retry(self):
        f=Fake(status='outcome_unknown')
        with self.assertRaisesRegex(RouteError,'no_replay'):f.route().act({'example'},'go')
        self.assertEqual(len(f.sent),1)
    def test_pending_no_retry(self):
        f=Fake(status='pending')
        with self.assertRaises(RouteError):f.route().act({'example'},'go')
        self.assertEqual(len(f.sent),1)
    def test_fresh_read_before_dispatch(self):
        f=Fake();r=f.route();r.act({'example'},'go');r.act({'example'},'go')
        self.assertEqual([x['capability'] for x in f.sent],['2','4'])
        self.assertNotEqual(f.sent[0]['command_id'],f.sent[1]['command_id'])
    def test_failed_title_guard_does_not_send(self):
        f=Fake()
        def guard(s):raise RouteError('wrong_title')
        with self.assertRaisesRegex(RouteError,'wrong_title'):f.route().act({'example'},'go',guard=guard)
        self.assertEqual(f.sent,[])
    def test_changed_screen_is_not_acted_on(self):
        f=Fake()
        with self.assertRaisesRegex(RouteError,'unexpected_screen'):f.route().act({'other'},'go')
        self.assertEqual(f.sent,[])
    def test_budget_no_send(self):
        f=Fake();r=f.route();r.count=128
        with self.assertRaisesRegex(RouteError,'budget'):r.act({'example'},'go')
        self.assertEqual(f.sent,[])
    def test_semantic_parameters_from_current_state(self):
        f=Fake();f.route().act({'example'},'go',lambda s:{'row':s['capability']})
        self.assertEqual(f.sent[0]['args'],{'row':'2'})
    def test_unsupported_resets_readiness(self):
        f=Fake();original=f.call
        def call(name,args):
            state=original(name,args)
            if name=='read_ui_state' and f.n==2:state['state']='unsupported'
            return state
        r=Route(call,lambda *a,**k:None,'xcom1',f.clock,f.sleep)
        self.assertEqual(r.ready({'example'})['revision'],4)
    def test_missing_authority_resets_readiness(self):
        f=Fake();original=f.call
        def call(name,args):
            state=original(name,args)
            if name=='read_ui_state' and f.n==2:state['capability']=None
            return state
        r=Route(call,lambda *a,**k:None,'xcom1',f.clock,f.sleep)
        self.assertEqual(r.ready({'example'})['revision'],4)
    def test_staff_parse_failclosed(self):
        self.assertEqual(amount('Scientists Available>10'),10)
        for value in [None,'unknown','Available: -1','3 of 10']:
            with self.assertRaises(RouteError):amount(value)

class UnknownSelection(unittest.TestCase):
    def route(self, enabled=True):
        return Route(lambda *a:None,lambda *a,**k:None,'xcom2',allow_unknown_selection=enabled)
    def rows(self,*labels):
        return {'visible':{'rows':[{'handle':'r'+str(i),'cells':[label]} for i,label in enumerate(labels)]}}
    def test_default_still_strict(self):
        self.assertEqual(self.route(False).project_candidate(self.rows(None),'Gauss Technology'),(None,False))
    def test_unique_unknown_once_per_project(self):
        r=self.route();state=self.rows(None,'Other')
        self.assertEqual(r.project_candidate(state,'Gauss Technology')[0]['handle'],'r0')
        r.unknown_selected.add('Gauss Technology')
        self.assertEqual(r.project_candidate(state,'Gauss Technology'),(None,False))
    def test_multiple_unknown_rows_failclosed(self):
        with self.assertRaisesRegex(RouteError,'ambiguous_unknown'):
            self.route().project_candidate(self.rows(None,None),'Gauss Technology')
    def test_exact_label_priority_over_unknowns(self):
        row,unknown=self.route().project_candidate(self.rows(None,None,'Gauss Technology'),'Gauss Technology')
        self.assertEqual(row['handle'],'r2');self.assertFalse(unknown)
    def test_duplicate_exact_labels_failclosed(self):
        with self.assertRaisesRegex(RouteError,'ambiguous_project'):
            self.route().project_candidate(self.rows('Gauss Technology','Gauss Technology'),'Gauss Technology')
    def test_wrong_or_null_title_cancel_once_without_allocation(self):
        for title in ('Other',None):
            r=self.route();actions=[]
            r.ready=lambda allowed:{'visible':{'title':title},'actions':[{'id':'cancel_research_project'}]}
            r.act=lambda before,action,**kw:actions.append(action)
            with self.assertRaisesRegex(RouteError,'stop_no_further_sampling'):
                r.confirm_project_title('Gauss Technology')
            self.assertEqual(actions,['cancel_research_project'])
    def test_no_cancel_offer_stops_without_action(self):
        r=self.route();actions=[]
        r.ready=lambda allowed:{'visible':{'title':None},'actions':[]}
        r.act=lambda *a,**kw:actions.append(a)
        with self.assertRaises(RouteError):r.confirm_project_title('Gauss Technology')
        self.assertEqual(actions,[])
    def test_matching_detail_does_not_cancel(self):
        r=self.route();actions=[]
        r.ready=lambda allowed:{'visible':{'title':'Gauss Technology'},'actions':[]}
        r.act=lambda *a,**kw:actions.append(a)
        r.confirm_project_title('Gauss Technology');self.assertEqual(actions,[])

if __name__=='__main__':unittest.main()
