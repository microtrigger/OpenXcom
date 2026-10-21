"""Bounded ordinary research smoke; only fresh, offered public controls.
Route derived from research_route_v22.py; intentionally not its full acceptance.
"""
import copy
import re
import time
import uuid

class RouteError(RuntimeError):
    pass

INCIDENTS = {'ufo_contact':'dismiss_ufo_contact', 'mission_alert':'dismiss_mission_alert'}
TARGETS = {'xcom1':('Laser Weapons','Laser Pistol'), 'xcom2':('Gauss Technology','Gauss Pistol')}

def require(ok, why):
    if not ok: raise RouteError(why)

def amount(text):
    require(isinstance(text,str),'staff_amount_unknown')
    match = re.fullmatch(r'(?:[A-Za-z][A-Za-z _\n]*[:>]?\s*|>\s*)?([0-9]+)',text.replace('\x01','').strip())
    require(match is not None,'staff_amount_unknown')
    return int(match[1])

def projection(state):
    visible = state.get('visible',{})
    if state.get('state') == 'geoscape':
        visible = {k:visible.get(k) for k in ('buttons','speed','speeds')}
    return state.get('state'),visible,state.get('actions')

class Route:
    def __init__(self,call,record,master,clock=time.monotonic,sleep=time.sleep,allow_unknown_selection=False,article_navigation=False,completion_allocation=False,manufacture_detail=False):
        self.call,self.record,self.master,self.clock,self.sleep=call,record,master,clock,sleep
        self.count=0;self.started=clock();self.state=None
        self.allow_unknown_selection=allow_unknown_selection;self.unknown_selected=set()
        self.article_navigation=article_navigation
        require(not manufacture_detail or completion_allocation,'manufacture_detail_requires_completion_allocation')
        self.completion_allocation=completion_allocation;self.research_baseline=None;self.base_name=None
        self.manufacture_detail=manufacture_detail;self.detail_phase=None

    def read(self):
        require(self.clock()-self.started<600,'overall_route_timeout')
        state=self.call('read_ui_state',{})
        require(isinstance(state,dict),'invalid_state')
        self.state=state
        return state

    def ready(self,allowed):
        deadline=self.clock()+12;previous=None
        while self.clock()<deadline:
            state=self.read()
            require(state.get('state') in set(allowed)|{'unsupported'},'unexpected_screen:'+str(state.get('state')))
            if state.get('state') in allowed and all(state.get(k) is not None for k in ('epoch','capability','revision')):
                if previous and projection(previous)==projection(state):
                    require(previous.get('challenge')!=state.get('challenge') and previous['capability']!=state['capability'],'nonfresh_read_authority')
                    return state
                previous=state
            else:
                previous=None
            self.sleep(.2)
        raise RouteError('readiness_timeout')

    def act(self,before,action,args=None,after=None,guard=None):
        if self.completion_allocation:
            require(action not in {'start_production','adjust_engineers','change_production_quantity','accept_production','stop_production','set_production_sell'},'production_mutation_or_detail_out_of_scope')
            detail_actions={'open_new_production':'open','select_production_project':'select','cancel_production_start':'cancel'}
            if action in detail_actions:
                require(self.manufacture_detail and self.detail_phase==detail_actions[action],'production_mutation_or_detail_out_of_scope:wrong_phase')
        state=self.ready(set(before))
        require(action in {a['id'] for a in state.get('actions',[])},'action_not_offered:'+action)
        if guard:guard(state)
        parameters=args(state) if callable(args) else (args or {})
        require(self.count<128,'dispatch_budget')
        request={k:state[k] for k in ('epoch','capability','revision')}
        request.update(command_id=uuid.uuid4().hex,action=action,args=parameters)
        self.record('before',action=action,state=state['state'],visible=state.get('visible'),arguments=parameters)
        self.count+=1
        receipt=self.call('execute_ui_action',request) # One attempt; no resend/status recovery.
        self.record('receipt',action=action,status=receipt.get('status'))
        require(receipt.get('status')=='dispatched','non_dispatched_stop_no_replay')
        require(isinstance(receipt.get('observation'),dict),'missing_dispatch_observation')
        if after:
            state=self.ready(set(after))
            self.record('after',action=action,state=state['state'],visible=state.get('visible'))
        return state

    def incidents(self,wanted,timeout=12):
        deadline=self.clock()+timeout
        while self.clock()<deadline:
            state=self.read();kind=state.get('state')
            if kind in wanted:return self.ready(set(wanted))
            if kind in INCIDENTS:
                self.act({kind},INCIDENTS[kind])
            else:require(kind in ('unsupported','geoscape'),'unsupported_interrupt:'+str(kind))
            self.sleep(.2)
        raise RouteError('notice_wait_timeout')

    def project_candidate(self,state,title):
        rows=state['visible']['rows']
        exact=[r for r in rows if r.get('cells') and r['cells'][0]==title]
        require(len(exact)<=1,'ambiguous_project_label')
        if exact:return exact[0],False
        if self.allow_unknown_selection and title not in self.unknown_selected:
            unknown=[r for r in rows if r.get('cells')==[None]]
            require(len(unknown)<=1,'ambiguous_unknown_project_rows')
            if unknown:return unknown[0],True
        return None,False

    def confirm_project_title(self,title):
        state=self.ready({'research_info'})
        if state['visible'].get('title')!=title:
            self.record('project_title_mismatch',expected_title=title,
                        visible_title=state['visible'].get('title'),allocation_attempted=False)
            if 'cancel_research_project' in {a['id'] for a in state.get('actions',[])}:
                self.act({'research_info'},'cancel_research_project',after={'new_research'})
            raise RouteError('selected_project_title_wrong_or_unknown_stop_no_further_sampling')
        self.record('project_title_confirmed',visible_title=title)
        return state

    def select_project(self,title):
        self.act({'research'},'open_new_research',after={'new_research'})
        for step in range(17):
            state=self.ready({'new_research'})
            row,unknown=self.project_candidate(state,title)
            if row is not None:break
            require(step<16 and state['visible'].get('scroll',{}).get('down'),'visible_project_not_found:'+title)
            self.act({'new_research'},'scroll_visible_list',{'direction':'down'},after={'new_research'})
        def select(state):
            row,unknown=self.project_candidate(state,title)
            require(row is not None,'project_candidate_changed')
            if unknown:self.unknown_selected.add(title)
            self.record('project_selection',row=row['handle'],shown_label=row['cells'][0],
                        unknown_label=unknown,expected_title_unconfirmed=title,
                        selection_creates_project=True,ordinary_rng_sampled=True,
                        hidden_identity_claimed=False,repeated_sampling=False)
            return {'row':row['handle']}
        self.act({'new_research'},'select_research_project',select,{'research_info'})
        self.confirm_project_title(title)
        def staff(state):
            v=state['visible'];require(v.get('title')==title,'selected_project_title_wrong_or_unknown')
            return [amount(v.get(k)) for k in ('available','space','allocated')]
        initial=staff(self.ready({'research_info'}))
        require(initial[0]==10 and initial[1]>=10 and initial[2]==0,'unexpected_initial_staff')
        for index in range(10):
            expected=[10-index,initial[1]-index,index]
            self.act({'research_info'},'adjust_scientists',{'direction':'increase'},{'research_info'},
                     lambda s:require(staff(s)==expected,'staff_precondition_changed'))
            require(staff(self.state)==[expected[0]-1,expected[1]-1,expected[2]+1],'staff_postcondition_wrong')
        self.act({'research_info'},'accept_research',after={'new_research'},guard=lambda s:require(staff(s)[2]==10,'staff_changed_before_accept'))
        self.act({'new_research'},'close_research_list',after={'research'})

    def completion(self,title):
        deadline=self.clock()+180
        while self.clock()<deadline:
            state=self.read();kind=state.get('state')
            if kind=='research_complete':
                state=self.ready({'research_complete'})
                require(state['visible'].get('topic')==title,'wrong_research_completion')
                self.record('completion',topic=title,visible=state['visible']);return
            if kind in INCIDENTS:self.act({kind},INCIDENTS[kind])
            elif kind=='geoscape':
                if state['visible'].get('speed')!='1 Day':
                    def day(s):
                        rows=[r for r in s['visible']['speeds'] if r.get('label')=='1 Day']
                        require(len(rows)==1,'day_speed_unknown');return {'speed':rows[0]['handle']}
                    self.act({'geoscape'},'choose_geoscape_speed',day)
            else:require(kind=='unsupported','unsupported_research_interrupt:'+str(kind))
            self.sleep(.5)
        raise RouteError('research_completion_timeout:'+title)

    def navigate_article(self,title):
        def same_title(state):
            require(state['visible'].get('title')==title,'article_title_changed')
        for direction in ('down','up'):
            state=self.ready({'ufopaedia_article'})
            same_title(state)
            if 'scroll_article_text' not in {a['id'] for a in state.get('actions',[])}:
                self.record('article_scroll_skipped',direction=direction,
                            reason='action_not_offered',scroll_tested=False)
                continue
            self.act({'ufopaedia_article'},'scroll_article_text',{'direction':direction},
                     {'ufopaedia_article'},guard=same_title)
            same_title(self.state)
            self.record('article_scroll_dispatched',direction=direction,visible=self.state['visible'],
                        movement_claimed=False,full_article_claimed=False)
        self.act({'ufopaedia_article'},'next_ufopaedia_article',after={'ufopaedia_article'},guard=same_title)
        self.record('article_next',visible=self.state['visible'],adjacency_identity_claimed=False)
        self.act({'ufopaedia_article'},'previous_ufopaedia_article',after={'ufopaedia_article'})
        same_title(self.state)
        self.record('article_navigation_return',visible_title=title,stable_article_identity_claimed=False)

    def research_summary(self,state):
        require(state.get('state')=='research','research_summary_wrong_screen')
        values=state.get('visible',{}).get('summary')
        require(isinstance(values,list) and len(values)==3,'research_summary_unavailable')
        return [amount(value) for value in values]

    def check_released(self,phase):
        state=self.ready({'research'})
        summary=self.research_summary(state)
        require(self.research_baseline is not None and summary==self.research_baseline,'research_summary_changed:'+phase)
        self.record('scientists_released',phase=phase,summary=summary,
                    visible_rows=state['visible'].get('rows'),hidden_project_absence_claimed=False)

    def notice(self,state,kind,labels):
        require(state.get('state')==kind,'wrong_notice_screen')
        expected='We can now research' if kind=='new_possible_research' else 'We can now produce'
        require(state['visible'].get('title')==expected,'wrong_notice_title')
        rows=state['visible'].get('rows',[])
        shown=[row['cells'][0] for row in rows if isinstance(row.get('cells'),list) and len(row['cells'])==1]
        require(all(shown.count(label)==1 for label in labels) and
                [label for label in shown if label in labels]==labels,'wrong_or_unknown_notice_labels')

    def production_projection(self,state):
        require(state.get('state')=='manufacture','manufacture_baseline_wrong_screen')
        visible=state.get('visible',{});summary=visible.get('summary');rows=visible.get('rows')
        require(isinstance(summary,list) and len(summary)==4 and all(isinstance(v,str) for v in summary)
                and isinstance(rows,list) and all(isinstance(row,dict) and isinstance(row.get('cells'),list)
                and bool(row['cells']) and all(isinstance(cell,str) for cell in row['cells']) for row in rows),
                'manufacture_baseline_unknown')
        return copy.deepcopy({'summary':summary,'rows':rows})

    def inspect_manufacture_detail(self,pistol):
        require(self.manufacture_detail and self.completion_allocation,'manufacture_detail_disabled')
        baseline=self.production_projection(self.ready({'manufacture'}))
        try:
            self.detail_phase='open'
            self.act({'manufacture'},'open_new_production',after={'new_manufacture'})
            self.detail_phase='select'
            def select(state):
                rows=[r for r in state['visible'].get('rows',[]) if r.get('cells') and r['cells'][0]==pistol]
                require(len(rows)==1,'manufacture_unique_visible_label_required')
                return {'row':rows[0]['handle']}
            self.act({'new_manufacture'},'select_production_project',select,{'manufacture_start'})
            self.detail_phase='cancel'
            detail=self.ready({'manufacture_start'})
            correct=detail['visible'].get('title')==pistol
            self.record('manufacture_detail',visible=detail['visible'],title_confirmed=correct,production_started_claimed=False)
            if not correct:
                if 'cancel_production_start' in {a['id'] for a in detail.get('actions',[])}:
                    self.act({'manufacture_start'},'cancel_production_start',after={'new_manufacture'})
                raise RouteError('manufacture_detail_wrong_title_stop')
            self.act({'manufacture_start'},'cancel_production_start',after={'new_manufacture'},
                     guard=lambda s:require(s['visible'].get('title')==pistol,'manufacture_title_changed_before_cancel'))
            self.detail_phase='close'
            self.act({'new_manufacture'},'close_production_list',after={'manufacture'})
            current=self.production_projection(self.ready({'manufacture'}))
            require(current==baseline,'manufacture_public_projection_changed')
            self.record('manufacture_cancel_verified',public_projection=current,hidden_model_unchanged_claimed=False)
        finally:
            self.detail_phase=None

    def complete_allocation(self,pistol):
        expected=['Laser Rifle'] if self.master=='xcom1' else ['Gauss Pistol Clip','Gauss Rifle']
        self.incidents({'new_possible_research'})
        self.act({'new_possible_research'},'dismiss_new_research',
                 guard=lambda s:self.notice(s,'new_possible_research',expected))
        self.incidents({'new_possible_manufacture'})
        self.act({'new_possible_manufacture'},'open_manufacture_allocation',after={'manufacture'},
                 guard=lambda s:self.notice(s,'new_possible_manufacture',[pistol]))
        self.record('manufacture_allocation_opened',visible=self.state['visible'],production_started_claimed=False)
        if self.manufacture_detail:self.inspect_manufacture_detail(pistol)
        self.act({'manufacture'},'close_production_list')
        self.incidents({'geoscape'})
        self.act({'geoscape'},'open_bases',after={'basescape'})
        self.act({'basescape'},'open_research',after={'research'},
                 guard=lambda s:require(self.base_name is not None and s['visible'].get('base_name')==self.base_name,'base_name_changed'))
        self.check_released('pistol')
        self.act({'research'},'close_research_list',after={'basescape'})
        self.act({'basescape'},'return_geoscape')
        self.incidents({'geoscape'})
        state=self.act({'geoscape'},'open_pause_menu',after={'pause_menu'})
        self.record('final',state=state['state'],visible=state['visible'],dispatches=self.count)

    def run(self):
        starter,pistol=TARGETS[self.master]
        self.act({'main_menu'},'open_new_game',after={'new_game'})
        self.act({'new_game'},'select_difficulty',{'value':'beginner'}, {'new_game'},
                 lambda s:require(any(x['value']=='beginner' for x in s['visible']['difficulty']),'beginner_not_shown'))
        state=self.ready({'new_game'})
        require(type(state['visible'].get('ironman')) is bool,'ironman_unknown')
        if state['visible']['ironman']:self.act({'new_game'},'set_ironman',{'value':False},{'new_game'})
        self.act({'new_game'},'start_new_game',after={'build_base'},guard=lambda s:require(s['visible']['ironman'] is False and any(d['value']=='beginner' and d.get('selected') is True for d in s['visible']['difficulty']),'difficulty_or_ironman_changed'))
        def site(s):
            globe=s['visible'].get('globe');require(globe is not None,'public_globe_missing')
            samples=[p for p in globe['samples'] if p.get('terrain')=='textured']
            require(bool(samples),'public_base_site_missing')
            p=min(samples,key=lambda p:((p['x']-globe['width']/2)**2+(p['y']-globe['height']/2)**2,p['x'],p['y']))
            return {k:p[k] for k in ('x','y')}
        self.act({'build_base'},'place_base_site',site,{'base_name'})
        self.act({'base_name'},'replace_base_name',{'text':'MCP Research'},{'base_name'})
        self.act({'base_name'},'confirm_base_name',guard=lambda s:require(s['visible'].get('name')=='MCP Research','base_name_mismatch'))
        self.incidents({'geoscape'})
        self.act({'geoscape'},'open_bases',after={'basescape'})
        if self.completion_allocation:
            self.base_name=self.ready({'basescape'})['visible'].get('base_name')
            require(self.base_name=='MCP Research','base_name_unknown_or_wrong')
        self.act({'basescape'},'open_research',after={'research'})
        if self.completion_allocation:
            self.research_baseline=self.research_summary(self.ready({'research'}))
            require(self.research_baseline[0]==10 and self.research_baseline[1]==0 and self.research_baseline[2]>=10,'unexpected_research_baseline')
            self.record('research_baseline',summary=self.research_baseline,base_name=self.base_name)
        self.select_project(starter)
        self.act({'research'},'close_research_list',after={'basescape'})
        self.act({'basescape'},'return_geoscape')
        self.completion(starter)
        self.act({'research_complete'},'view_research_reports')
        self.incidents({'new_possible_research'})
        self.act({'new_possible_research'},'open_research_allocation',after={'research'})
        if self.completion_allocation:self.check_released('starter')
        self.select_project(pistol)
        self.act({'research'},'close_research_list')
        self.completion(pistol)
        self.act({'research_complete'},'view_research_reports')
        state=self.incidents({'ufopaedia_article'})
        require(state['visible'].get('title')==pistol,'wrong_article_title')
        self.record('article',visible=state['visible'],full_article_claimed=False)
        if self.article_navigation:self.navigate_article(pistol)
        self.act({'ufopaedia_article'},'close_ufopaedia_article',
                 guard=(lambda s:require(s['visible'].get('title')==pistol,'article_title_changed_before_close')) if self.article_navigation else None)
        if self.completion_allocation:
            self.complete_allocation(pistol)
            return
        # Remaining completion notices are left visible; do not advance time further.
        state=self.incidents({'new_possible_research','new_possible_manufacture','geoscape'})
        if state['state']=='geoscape':
            state=self.act({'geoscape'},'open_pause_menu',after={'pause_menu'})
        self.record('final',state=state['state'],visible=state['visible'],dispatches=self.count)
