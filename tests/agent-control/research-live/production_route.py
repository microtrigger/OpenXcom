"""Separate opt-in production policy; legacy research-only guard is unchanged."""
from allocation_route import AllocationRoute
from route import Route, RouteError, require, amount, INCIDENTS

class ProductionRoute(AllocationRoute):
    def open_draft(self,title):
        self.act({'manufacture'},'open_new_production',after={'new_manufacture'})
        def row(state):
            rows=[r for r in state['visible'].get('rows',[]) if r.get('cells') and r['cells'][0]==title]
            require(len(rows)==1,'production_unique_visible_label_required')
            return {'row':rows[0]['handle']}
        self.act({'new_manufacture'},'select_production_project',row,{'manufacture_start'})
        self.act({'manufacture_start'},'start_production',after={'manufacture_info'},
                 guard=lambda s:require(s['visible'].get('title')==title,'production_detail_title_unknown'))
        self.record('editable_order_created',title=title,first_item_charged_claimed=False)

    def quantity(self,title,control,operation,value=None):
        args={'control':control,'operation':operation}
        if value is not None:args['value']=value
        return self.act({'manufacture_info'},'change_production_quantity',args,{'manufacture_info'},
                        guard=lambda s:require(s['visible'].get('title')==title,'production_title_changed'))

    def infinite_roundtrip(self,title):
        before=self.ready({'manufacture_info'})
        staff=self.numbers(before,title)
        require(amount(before['visible'].get('units'))==1,'infinite_requires_one_unit')
        for _ in range(2):
            state=self.quantity(title,'units','max')
            units=state['visible'].get('units')
            require(isinstance(units,str) and units.replace('\x01','').lstrip('> ').strip()=='∞','infinite_mode_not_visible')
            require(self.numbers(state,title)==staff,'infinite_changed_staff')
        state=self.quantity(title,'units','clear')
        require(amount(state['visible'].get('units'))==1 and self.numbers(state,title)==staff,'infinite_clear_wrong')
        self.record('infinite_roundtrip_verified',title=title)

    def close_manufacture_to_geoscape(self):
        self.act({'manufacture'},'close_production_list')
        state=self.ready({'geoscape','basescape'})
        if state['state']=='basescape':
            self.act({'basescape'},'return_geoscape')
        self.incidents({'geoscape'})

    def scan_stores(self,title):
        """Traverse to a demonstrated list boundary; unknown labels forbid absence=0."""
        seen={};unknown=False;previous=None
        for page in range(49):
            state=self.ready({'base_stores'});visible=state['visible']
            rows=visible.get('rows');require(isinstance(rows,list),'stores_rows_unknown')
            require(rows or visible.get('scroll',{}).get('down') is False,'stores_empty_scrolling_view')
            window=[];names=[]
            for row in rows:
                cells=row.get('cells') if isinstance(row,dict) else None
                require(isinstance(cells,list) and len(cells)==3,'stores_columns_unknown')
                window.append(cells)
                name=cells[0]
                if not isinstance(name,str) or not name:
                    unknown=True;continue
                names.append(name)
                if name==title:
                    value=amount(cells[1])
                    require(name not in seen or seen[name]==value,'stores_quantity_changed_during_scan')
                    seen[name]=value
            require(len(names)==len(set(names)),'stores_ambiguous_visible_labels')
            self.record('stores_visible_page',title=title,page=page,rows=window)
            # Arrow projection reports visibility, not necessarily movability.
            # A fresh stable identical viewport after one step proves boundary.
            if window==previous or visible.get('scroll',{}).get('down') is False:
                require(title in seen or not unknown,'stores_absence_unprovable_unknown_labels')
                value=seen.get(title,0)
                self.record('stores_count_verified',title=title,quantity=value,
                            absent_after_complete_scan=title not in seen,pages=page+1)
                return value
            require(page<48,'stores_scan_bound_reached')
            require(visible.get('scroll',{}).get('down') is True,'stores_scroll_unknown')
            previous=window
            self.act({'base_stores'},'scroll_visible_list',{'direction':'down'},{'base_stores'})
        raise RouteError('stores_scan_bound_reached')

    def inventory_from_manufacture(self,title):
        self.close_manufacture_to_geoscape()
        self.act({'geoscape'},'open_bases',after={'basescape'})
        self.act({'basescape'},'open_base_info',after={'base_info'},guard=lambda s:require(s['visible'].get('base_name')=='MCP Research','inventory_base_changed'))
        self.act({'base_info'},'open_base_stores',after={'base_stores'})
        value=self.scan_stores(title)
        self.act({'base_stores'},'close_base_stores',after={'base_info'})
        self.act({'base_info'},'close_base_info',after={'basescape'})
        self.act({'basescape'},'return_geoscape')
        self.incidents({'geoscape'})
        return value

    def run_production(self,title):
        inventory_before=self.inventory_from_manufacture(title)
        self.act({'geoscape'},'open_bases',after={'basescape'})
        self.act({'basescape'},'open_manufacture',after={'manufacture'})
        baseline=self.production_projection(self.ready({'manufacture'}))
        self.open_draft(title)
        self.production_finite_matrix(title)
        self.infinite_roundtrip(title)
        # This is Stop Production (removes the new editable order), NOT Cancel.
        self.act({'manufacture_info'},'stop_production',after={'new_manufacture'})
        self.act({'new_manufacture'},'close_production_list',after={'manufacture'})
        require(self.production_projection(self.state)==baseline,'draft_stop_public_projection_changed')
        self.record('draft_stop_verified',cancel_semantics_claimed=False)
        self.open_draft(title)
        staff=self.numbers(self.ready({'manufacture_info'}),title)
        require(min(staff[0],staff[1])>=3,'insufficient_visible_engineers_or_workshops')
        s=self.quantity(title,'engineers','set',3)
        require(self.numbers(s,title)==(staff[0]-3,staff[1]-3,3),'three_engineers_not_visible')
        s=self.quantity(title,'units','set',3)
        require(amount(s['visible'].get('units'))==3,'three_units_not_visible')
        require(isinstance(s['visible'].get('sell'),dict) and s['visible']['sell'].get('pressed') is False,'sell_mode_unknown_or_enabled')
        self.act({'manufacture_info'},'accept_production',after={'new_manufacture'},
                 guard=lambda s:require(self.numbers(s,title)[2]==3 and amount(s['visible'].get('units'))==3 and isinstance(s['visible'].get('sell'),dict) and s['visible']['sell'].get('pressed') is False,'order_changed_before_accept'))
        self.act({'new_manufacture'},'close_production_list',after={'manufacture'})
        self.record('three_unit_order_accepted',visible=self.state['visible'],inventory_increment_claimed=False)
        self.close_manufacture_to_geoscape()
        self.wait_completion(title)
        self.act({'production_complete'},'open_production_destination')
        state=self.ready({'manufacture'})
        self.record('production_completion_destination',visible=state['visible'])
        inventory_after=self.inventory_from_manufacture(title)
        require(inventory_after==inventory_before+3,'produced_inventory_delta_not_three')
        self.record('inventory_delta_verified',title=title,before=inventory_before,
                    after=inventory_after,delta=3,inventory_verified=True)
        self.act({'geoscape'},'open_pause_menu',after={'pause_menu'})
        self.record('production_phase_complete',dispatches=self.count,inventory_verified=True)

    def wait_completion(self,title):
        deadline=self.clock()+180
        while self.clock()<deadline:
            state=self.read();kind=state.get('state')
            if kind=='production_complete':
                state=self.ready({'production_complete'});message=state['visible'].get('message')
                require(isinstance(message,str) and title in message,'production_completion_message_unknown_or_other')
                require(message.rstrip().lower().endswith('is complete'),'production_notice_not_success')
                self.record('production_completed_notice',message=message,inventory_increment_claimed=False)
                return
            if kind in INCIDENTS:self.act({kind},INCIDENTS[kind])
            elif kind=='geoscape':
                if state['visible'].get('speed')!='1 Day':
                    def speed(s):
                        rows=[r for r in s['visible']['speeds'] if r.get('label')=='1 Day']
                        require(len(rows)==1,'day_speed_unknown');return {'speed':rows[0]['handle']}
                    self.act({'geoscape'},'choose_geoscape_speed',speed)
            else:require(kind=='unsupported','unsupported_production_interrupt:'+str(kind))
            self.sleep(.5)
        raise RouteError('production_completion_timeout')

class ProductionCampaignRoute(Route):
    def __init__(self,*args,**kwargs):
        require(kwargs.get('completion_allocation') is True,'production_runner_requires_completion_allocation')
        require(not kwargs.get('verify_retained_history'),'production_runner_history_scope_not_supported')
        super().__init__(*args,**kwargs)

    def complete_allocation(self,pistol):
        expected=['Laser Rifle'] if self.master=='xcom1' else ['Gauss Pistol Clip','Gauss Rifle']
        self.incidents({'new_possible_research'})
        self.act({'new_possible_research'},'dismiss_new_research',guard=lambda s:self.notice(s,'new_possible_research',expected))
        self.incidents({'new_possible_manufacture'})
        self.act({'new_possible_manufacture'},'open_manufacture_allocation',after={'manufacture'},
                 guard=lambda s:self.notice(s,'new_possible_manufacture',[pistol]))
        self.record('production_phase_handoff',research_dispatches=self.count,
                    research_budget=128,production_budget=128,total_budget=256,
                    description='Separate authorized production phase; research guard/count unchanged.')
        phase=ProductionRoute(self.call,self.record,self.master,self.clock,self.sleep)
        phase.run_production(pistol)
