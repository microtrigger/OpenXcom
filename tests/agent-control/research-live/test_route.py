import copy
import unittest
from route import Route,RouteError,amount,shown_clock,VisibleClockEvidence

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

class ArticleNavigation(unittest.TestCase):
    def make(self,scroll=True,returned='Laser Pistol'):
        log=[];actions=[]
        r=Route(lambda *a:None,lambda kind,**kw:log.append((kind,kw)),'xcom1',article_navigation=True)
        def state(title):
            return {'visible':{'title':title},'actions':[{'id':a} for a in
                    (['scroll_article_text'] if scroll else [])+['next_ufopaedia_article','previous_ufopaedia_article'] ]}
        r.state=state('Laser Pistol')
        r.ready=lambda allowed:r.state
        def act(before,action,args=None,after=None,guard=None):
            if guard:guard(r.state)
            actions.append((action,args))
            if action=='next_ufopaedia_article':r.state=state('Other Article')
            elif action=='previous_ufopaedia_article':r.state=state(returned)
            return r.state
        r.act=act
        return r,actions,log
    def test_scroll_dispatch_then_next_previous(self):
        r,actions,log=self.make();r.navigate_article('Laser Pistol')
        self.assertEqual(actions,[('scroll_article_text',{'direction':'down'}),('scroll_article_text',{'direction':'up'}),
                                 ('next_ufopaedia_article',None),('previous_ufopaedia_article',None)])
        self.assertEqual(log[-1][0],'article_navigation_return')
    def test_absent_scroll_is_explicit_skip_not_dispatch(self):
        r,actions,log=self.make(scroll=False);r.navigate_article('Laser Pistol')
        self.assertEqual([a[0] for a in actions],['next_ufopaedia_article','previous_ufopaedia_article'])
        skipped=[v for k,v in log if k=='article_scroll_skipped']
        self.assertEqual([v['direction'] for v in skipped],['down','up'])
        self.assertTrue(all(v['scroll_tested'] is False for v in skipped))
    def test_return_title_mismatch_stops_before_close(self):
        r,actions,log=self.make(returned='Wrong')
        with self.assertRaisesRegex(RouteError,'article_title_changed'):r.navigate_article('Laser Pistol')
        self.assertNotIn('close_ufopaedia_article',[a[0] for a in actions])
        self.assertNotIn('article_navigation_return',[k for k,v in log])
    def test_unknown_return_title_is_not_identity(self):
        r,actions,log=self.make(returned=None)
        with self.assertRaises(RouteError):r.navigate_article('Laser Pistol')
    def test_default_navigation_disabled(self):
        self.assertFalse(Route(None,None,'xcom1').article_navigation)

class CompletionAllocation(unittest.TestCase):
    def route(self):
        return Route(None,lambda *a,**k:None,'xcom1',completion_allocation=True)
    def research(self,summary):
        return {'state':'research','visible':{'summary':summary,'rows':[]}}
    def test_numeric_baseline_is_public(self):
        self.assertEqual(self.route().research_summary(self.research(['Scientists Available>10','Scientists Allocated>0','Laboratory Space Available>50'])),[10,0,50])
    def test_unknown_summary_fails(self):
        for summary in (None,[],['10',None,'50'],['10','0','unknown']):
            with self.assertRaises(RouteError):self.route().research_summary(self.research(summary))
    def test_changed_summary_stops(self):
        r=self.route();r.research_baseline=[10,0,50]
        r.ready=lambda states:self.research(['9','1','49'])
        with self.assertRaisesRegex(RouteError,'summary_changed'):r.check_released('pistol')
    def test_wrong_notice_title_label_duplicates_stop(self):
        r=self.route()
        for title,labels in [('Wrong',['Laser Pistol']),('We can now produce',[None]),('We can now produce',['Other']),('We can now produce',['Laser Pistol','Laser Pistol'])]:
            state={'state':'new_possible_manufacture','visible':{'title':title,'rows':[{'cells':[x]} for x in labels]}}
            with self.assertRaises(RouteError):r.notice(state,'new_possible_manufacture',['Laser Pistol'])
    def test_production_actions_rejected_before_read_or_send(self):
        for action in ('start_production','adjust_engineers','change_production_quantity','open_new_production','select_production_project','accept_production','stop_production','cancel_production_start','set_production_sell'):
            f=Fake(actions=(action,));r=Route(f.call,lambda *a,**k:None,'xcom1',f.clock,f.sleep,completion_allocation=True)
            with self.assertRaisesRegex(RouteError,'out_of_scope'):r.act({'example'},action)
            self.assertEqual(f.sent,[]);self.assertEqual(f.n,0)
    def test_default_completion_disabled(self):
        self.assertFalse(Route(None,None,'xcom1').completion_allocation)
    def test_minimal_completion_route_without_production(self):
        r=self.route();actions=[];r.base_name='MCP Research';r.research_baseline=[10,0,50]
        def state(kind):
            visible={}
            if kind=='new_possible_research':visible={'title':'We can now research','rows':[{'cells':['Laser Rifle']}]}
            if kind=='new_possible_manufacture':visible={'title':'We can now produce','rows':[{'cells':['Laser Pistol']}]}
            if kind=='basescape':visible={'base_name':'MCP Research'}
            if kind=='research':visible={'summary':['10','0','50'],'rows':[]}
            return {'state':kind,'visible':visible}
        r.ready=lambda states:state(next(iter(states)))
        r.incidents=lambda states:state(next(iter(states)))
        def act(before,action,args=None,after=None,guard=None):
            current=state(next(iter(before)))
            if guard:guard(current)
            actions.append(action)
            r.state=state(next(iter(after))) if after else current
            return r.state
        r.act=act;r.complete_allocation('Laser Pistol')
        self.assertEqual(actions,['dismiss_new_research','open_manufacture_allocation','close_production_list','open_bases','open_research','close_research_list','return_geoscape','open_pause_menu'])

class ManufactureDetail(unittest.TestCase):
    def scenario(self,labels=('Laser Pistol',),title='Laser Pistol',cancel=True,changed=False,uncertain=False):
        records=[];sent=[]
        r=Route(None,lambda kind,**kw:records.append(kind),'xcom1',completion_allocation=True,manufacture_detail=True)
        baseline={'state':'manufacture','visible':{'summary':['10','0','50','$100'],'rows':[]}}
        r.state=copy.deepcopy(baseline)
        r.ready=lambda allowed:r.state
        def act(before,action,args=None,after=None,guard=None):
            if guard:guard(r.state)
            if callable(args):args(r.state)
            if action=='cancel_production_start' and not cancel:raise RouteError('action_not_offered')
            sent.append(action)
            if action=='open_new_production':
                r.state={'state':'new_manufacture','visible':{'rows':[{'handle':'r'+str(i),'cells':[v]} for i,v in enumerate(labels)]}}
            elif action=='select_production_project':
                if uncertain:raise RouteError('non_dispatched_stop_no_replay')
                r.state={'state':'manufacture_start','visible':{'title':title},'actions':[{'id':'cancel_production_start'}] if cancel else []}
            elif action=='cancel_production_start':r.state={'state':'new_manufacture','visible':{}}
            elif action=='close_production_list':
                r.state=copy.deepcopy(baseline)
                if changed:r.state['visible']['summary'][0]='9'
            return r.state
        r.act=act
        return r,sent,records
    def test_unknown_baseline_cells_failclosed(self):
        r,_,_=self.scenario()
        for summary,rows in [(['10',None,'50','$100'],[]),(['10','0','50','$100'],[{'cells':['Pistol',None]}])]:
            with self.assertRaisesRegex(RouteError,'baseline_unknown'):
                r.production_projection({'state':'manufacture','visible':{'summary':summary,'rows':rows}})
    def test_detail_requires_completion(self):
        with self.assertRaises(RouteError):Route(None,None,'xcom1',manufacture_detail=True)
    def test_success_order_and_phase_reset(self):
        r,sent,records=self.scenario();r.inspect_manufacture_detail('Laser Pistol')
        self.assertEqual(sent,['open_new_production','select_production_project','cancel_production_start','close_production_list'])
        self.assertIsNone(r.detail_phase);self.assertIn('manufacture_cancel_verified',records)
    def test_duplicate_unknown_labels_never_select(self):
        for labels in [(None,),('Laser Pistol','Laser Pistol'),('Other',)]:
            r,sent,_=self.scenario(labels=labels)
            with self.assertRaisesRegex(RouteError,'unique_visible'):r.inspect_manufacture_detail('Laser Pistol')
            self.assertEqual(sent,['open_new_production']);self.assertIsNone(r.detail_phase)
    def test_wrong_unknown_title_cancel_once_then_stop(self):
        for title in ('Other',None):
            r,sent,_=self.scenario(title=title)
            with self.assertRaisesRegex(RouteError,'wrong_title'):r.inspect_manufacture_detail('Laser Pistol')
            self.assertEqual(sent,['open_new_production','select_production_project','cancel_production_start'])
    def test_absent_cancel_stops(self):
        r,sent,_=self.scenario(cancel=False)
        with self.assertRaises(RouteError):r.inspect_manufacture_detail('Laser Pistol')
        self.assertEqual(sent,['open_new_production','select_production_project'])
    def test_unknown_dispatch_does_not_compensate(self):
        r,sent,_=self.scenario(uncertain=True)
        with self.assertRaisesRegex(RouteError,'no_replay'):r.inspect_manufacture_detail('Laser Pistol')
        self.assertEqual(sent,['open_new_production','select_production_project']);self.assertIsNone(r.detail_phase)
    def test_changed_summary_fails(self):
        r,sent,_=self.scenario(changed=True)
        with self.assertRaisesRegex(RouteError,'projection_changed'):r.inspect_manufacture_detail('Laser Pistol')
    def test_detail_phase_not_global_permission(self):
        for action in ('open_new_production','select_production_project','cancel_production_start'):
            f=Fake(actions=(action,));r=Route(f.call,None,'xcom1',f.clock,f.sleep,completion_allocation=True,manufacture_detail=True)
            with self.assertRaisesRegex(RouteError,'wrong_phase'):r.act({'example'},action)
            self.assertEqual(f.sent,[])
    def test_mutations_forbidden_even_in_detail_phase(self):
        for action in ('start_production','adjust_engineers','change_production_quantity','set_production_sell','stop_production','accept_production'):
            f=Fake(actions=(action,));r=Route(f.call,None,'xcom1',f.clock,f.sleep,completion_allocation=True,manufacture_detail=True);r.detail_phase='select'
            with self.assertRaisesRegex(RouteError,'out_of_scope'):r.act({'example'},action)
            self.assertEqual(f.sent,[])

class ClockChecks(unittest.TestCase):
    def state(self,revision=1,**kw):
        visible=dict(year='2000',month='Feb',day='28th',hour='23',minute='59',second='59');visible.update(kw)
        return {'state':'geoscape','visible':visible,'revision':revision}
    def test_leap_day_and_calendar_invalid(self):
        self.assertIsNotNone(shown_clock(self.state(day='29th')))
        for fields in ({'year':'1900','day':'29th'},{'day':'30th'},{'month':'Apr','day':'31st'},
                       {'hour':'24'},{'minute':'60'},{'second':'60'},{'year':'0000'},{'month':'Unknown'}):
            self.assertIsNone(shown_clock(self.state(**fields)))
    def test_null_or_nonstring_unknown(self):
        self.assertIsNone(shown_clock(self.state(hour=None)))
        self.assertIsNone(shown_clock(self.state(year=2000)))
    def test_success_forward_leap_transition(self):
        e=VisibleClockEvidence();e.observe(self.state());e.observe(self.state(revision=2,day='29th',hour='0',minute='0',second='0'))
        self.assertEqual(e.verified()['later_observed']['time'],'2000-02-29T00:00:00')
    def test_no_sample_or_noadvance_partial(self):
        for count in (0,1,3):
            e=VisibleClockEvidence()
            for _ in range(count):e.observe(self.state())
            with self.assertRaisesRegex(RouteError,'unproven'):e.verified()
    def test_unparseable_only_is_not_evidence(self):
        e=VisibleClockEvidence();e.observe(self.state(day=None))
        with self.assertRaises(RouteError):e.verified()
    def test_backward_clock_rejected(self):
        e=VisibleClockEvidence();e.observe(self.state())
        with self.assertRaisesRegex(RouteError,'backward'):e.observe(self.state(revision=2,second='58'))
    def test_revision_regression_rejected(self):
        e=VisibleClockEvidence();e.observe(self.state(revision=2))
        with self.assertRaisesRegex(RouteError,'revision_regressed'):e.observe(self.state(revision=1,day='29th'))
    def test_invalid_revision_is_not_sample(self):
        for revision in (None,True,-1,'2'):
            e=VisibleClockEvidence();e.observe(self.state(revision=revision))
            with self.assertRaises(RouteError):e.verified()
    def test_each_project_evidence_resets_and_no_extra_action(self):
        r=Route(None,lambda *a,**k:None,'xcom1',verify_visible_clock=True);seen=[]
        def complete(title):
            seen.append(r.clock_evidence)
            r.clock_evidence.observe(self.state());r.clock_evidence.observe(self.state(revision=2,day='29th'))
            r.clock_evidence.verified()
        r._completion=complete;r.completion('starter');r.completion('pistol')
        self.assertIsNot(seen[0],seen[1]);self.assertIsNone(r.clock_evidence);self.assertEqual(r.count,0)
    def test_default_off(self):
        r=Route(None,None,'xcom1');self.assertFalse(r.verify_visible_clock)

class HistoryGuards(unittest.TestCase):
    def runner(self,newest=20,chunk=16,mutate=None):
        from route import RetainedHistoryEvidence
        calls=[];logs=[]
        def call(name,args):
            self.assertEqual(name,'read_public_events');calls.append(dict(args))
            n=len(calls);after=args['after'];oldest=max(1,newest-255)
            events=[dict(seq=i,kind='presentation',state='pause_menu',visible={}) for i in range(max(after+1,oldest),min(newest+1,max(after+1,oldest)+chunk))]
            p=dict(schema='openxcom-agent-control/26',mode='agent-control',type='events',epoch='epoch',challenge=str(n),after=after,oldest=oldest,newest=newest,next_cursor=events[-1]['seq'] if events else after,gap=after+1<oldest,events=events)
            if mutate:mutate(p,n)
            return p
        h=RetainedHistoryEvidence(call,lambda event,**kw:logs.append((event,kw)),'epoch')
        return h,calls,logs
    def test_bounded_success(self):
        h,c,l=self.runner();h.verify();self.assertEqual([x['after'] for x in c],[0,16,16]);self.assertEqual(l[-1][1]['common_records'],4);self.assertFalse(l[-1][1]['whole_route_completeness_claimed'])
    def test_initial_gap_explicit_not_full_history(self):
        h,c,l=self.runner(300);h.verify();self.assertTrue(l[-1][1]['initial_expired_gap']);self.assertEqual(l[-1][1]['retained_records'],256)
    def test_one_record_pages_supported(self):
        h,c,l=self.runner(256,1);h.verify();self.assertEqual(len(c),257)
    def test_epoch_and_schema_rejected(self):
        for key,value in [('epoch','other'),('schema','old'),('mode','other')]:
            h,_,_=self.runner(mutate=lambda p,n:p.update({key:value}))
            with self.assertRaises(RouteError):h.verify()
    def test_counter_bool_sequence_and_cursor_rejected(self):
        mutations=[lambda p,n:p.update(newest=True),lambda p,n:p.update(next_cursor=0),lambda p,n:p['events'][0].update(seq=2),lambda p,n:p.update(gap=True),lambda p,n:p.update(events=[])]
        for m in mutations:
            h,_,_=self.runner(mutate=m)
            with self.assertRaises(RouteError):h.verify()
    def test_retained_mutation_rejected(self):
        def mutate(p,n):
            if n==3:p['events'][0]['visible']={'changed':True}
        h,_,_=self.runner(mutate=mutate)
        with self.assertRaisesRegex(RouteError,'retained_record_changed'):h.verify()
    def test_fresh_challenge_required(self):
        h,_,_=self.runner(mutate=lambda p,n:p.update(challenge='same'))
        with self.assertRaisesRegex(RouteError,'challenge'):h.verify()
    def test_empty_history_unproven(self):
        h,_,_=self.runner(0)
        with self.assertRaisesRegex(RouteError,'no_retained_records'):h.verify()
    def test_expiration_during_drain_rejected(self):
        def mutate(p,n):
            if n==2:
                p.update(newest=300,oldest=45,gap=True,events=[dict(seq=45,kind='presentation')],next_cursor=45)
        h,_,_=self.runner(mutate=mutate)
        with self.assertRaisesRegex(RouteError,'expired_during_drain'):h.verify()
    def test_new_tail_not_chased(self):
        def mutate(p,n):
            if n>1:p['newest']=30
        h,c,_=self.runner(mutate=mutate);h.verify();self.assertEqual(len(c),3)
    def test_bounds_regression_rejected(self):
        def mutate(p,n):
            if n==2:p['newest']=19
        h,_,_=self.runner(mutate=mutate)
        with self.assertRaisesRegex(RouteError,'bounds_regressed'):h.verify()
    def test_optin_tool_requirement(self):
        from run_research_smoke import Rpc,SmokeError
        rpc=object.__new__(Rpc);rpc.send=lambda *a:None
        rpc.request=lambda method,args:({'protocolVersion':'2025-11-25'} if method=='initialize' else {'tools':[{'name':'read_ui_state'},{'name':'execute_ui_action'}]})
        rpc.initialize()
        with self.assertRaisesRegex(SmokeError,'public_events_tool_missing'):rpc.initialize(True)
    def test_timeout(self):
        h,c,_=self.runner();h.clock=lambda:h.deadline
        with self.assertRaisesRegex(RouteError,'timeout'):h.verify()
        self.assertEqual(c,[])
    def test_default_off_no_extra_reads(self):
        f=Fake();r=f.route();r._run=lambda:None;r.run();self.assertEqual(f.n,0)
    def test_optin_requires_final_pause(self):
        f=Fake();r=f.route();r.verify_retained_history=True;r._run=lambda:None
        with self.assertRaisesRegex(RouteError,'unexpected_screen'):r.run()
        self.assertEqual(f.sent,[])

class HistoryPrerequisiteTests(unittest.TestCase):
    def test_constructor_rejects_before_rpc(self):
        calls=[]
        with self.assertRaisesRegex(RouteError,'retained_history_requires_completion_allocation'):
            Route(lambda *args:calls.append(args),lambda *a,**k:None,'xcom1',verify_retained_history=True)
        self.assertEqual(calls,[])
    def test_cli_rejects_before_launch_or_output(self):
        import contextlib,io,sys
        from unittest.mock import patch
        import run_research_smoke as cli
        args=['runner','--verify-retained-history','--source','/unused','--binary','/unused','--data','/unused','--master','xcom1','--output','/unused','--focus-ready-file','/unused']
        error=io.StringIO()
        with patch.object(sys,'argv',args),patch.object(cli.subprocess,'Popen') as launch,patch.object(cli.Path,'mkdir') as mkdir,contextlib.redirect_stderr(error):
            with self.assertRaises(SystemExit) as stopped:cli.main()
        self.assertEqual(stopped.exception.code,2)
        self.assertIn('--verify-retained-history requires --completion-allocation',error.getvalue())
        launch.assert_not_called();mkdir.assert_not_called()




class QueuedResearchCompletion(unittest.TestCase):
    def run_case(self, initial, title='Laser Weapons', receipt='dispatched', replacement='research_complete'):
        n=[0];sent=[]
        def call(name,args):
            if name=='execute_ui_action':
                sent.append(args)
                return {'status':receipt,'observation':{}}
            n[0]+=1
            kind=initial if n[0]==1 else replacement
            return dict(state=kind,visible={'topic':title,'speed':'5 Secs'},
                        actions=[{'id':'dismiss_ufo_contact'},{'id':'choose_geoscape_speed'}],
                        epoch='e',revision=n[0],challenge=str(n[0]),capability=str(n[0]))
        r=Route(call,lambda *a,**kw:None,'xcom1')
        return r,sent
    def test_queued_completion_replaces_incident_before_dispatch(self):
        r,sent=self.run_case('ufo_contact');r._completion('Laser Weapons')
        self.assertEqual(sent,[])
    def test_queued_completion_replaces_geoscape_before_speed_dispatch(self):
        r,sent=self.run_case('geoscape');r._completion('Laser Weapons')
        self.assertEqual(sent,[])
    def test_wrong_completion_topic_still_rejected(self):
        r,sent=self.run_case('ufo_contact',title='Other')
        with self.assertRaisesRegex(RouteError,'wrong_research_completion'):r._completion('Laser Weapons')
        self.assertEqual(sent,[])
    def test_unrelated_replacement_not_allowed(self):
        r,sent=self.run_case('ufo_contact',replacement='manufacture')
        with self.assertRaisesRegex(RouteError,'unexpected_screen:manufacture'):r._completion('Laser Weapons')
        self.assertEqual(sent,[])

    def test_unknown_outcome_still_stops(self):
        r,_=self.run_case('ufo_contact');calls=[]
        def act(*a,**kw):
            calls.append(1);raise RouteError('non_dispatched_stop_no_replay')
        r.act=act
        with self.assertRaisesRegex(RouteError,'no_replay'):r._completion('Laser Weapons')
        self.assertEqual(calls,[1])

if __name__=='__main__':unittest.main()
