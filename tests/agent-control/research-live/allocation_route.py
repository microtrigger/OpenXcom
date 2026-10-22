"""Opt-in allocation checks at an already open, unsaved test-project screen.
No campaign setup, production start, cancellation or time advancement is implied.
"""
from route import Route, amount, require

class AllocationRoute(Route):
    def numbers(self, state, title):
        visible = state.get('visible', {})
        require(visible.get('title') == title, 'allocation_title_changed_or_unknown')
        return tuple(amount(visible.get(k)) for k in ('available', 'space', 'allocated'))

    def staff_matrix(self, title, scientists=True):
        screen = 'research_info' if scientists else 'manufacture_info'
        action = 'adjust_scientists' if scientists else 'adjust_engineers'
        initial = self.numbers(self.ready({screen}), title)
        available, space, assigned = initial
        require(assigned == 0, 'matrix_requires_zero_initial_allocation')
        limit = min(available, space)
        # Single-step scientist API has no max/set; do not pretend to invoke either.
        require(1 <= limit <= 40, 'staff_matrix_limit_outside_bounded_route')
        require(self.count + 2*limit + 2 <= 128, 'staff_matrix_insufficient_remaining_budget')
        def step(direction, before, after):
            self.act({screen}, action, {'direction': direction}, {screen},
                     lambda state: require(self.numbers(state,title) == before, 'staff_matrix_precondition'))
            require(self.numbers(self.state,title) == after, 'staff_matrix_postcondition')
        for n in range(limit):
            step('increase', (available-n,space-n,n), (available-n-1,space-n-1,n+1))
        full = (available-limit,space-limit,limit)
        step('increase', full, full)  # Valid offered input, beyond resource capacity.
        for n in range(limit,0,-1):
            step('decrease', (available-n,space-n,n), (available-n+1,space-n+1,n-1))
        step('decrease',initial,initial)
        self.record('staff_matrix_verified', master=self.master, screen=screen,
                    maximum=limit, initial=initial, final=self.numbers(self.state,title),
                    methods=['fresh_authority_single_step'], keyboard_tested=False)
        return self.state

    def production_finite_matrix(self, title):
        """Requires a newly opened order showing one unit, zero engineers.
        Checks draft quantities only. Does not commit/stop or claim completion.
        """
        screen = 'manufacture_info'
        initial = self.ready({screen})
        baseline = self.numbers(initial,title)
        require(self.count + 13 <= 128, 'quantity_matrix_insufficient_remaining_budget')
        require(baseline[2] == 0 and amount(initial['visible'].get('units')) == 1,
                'quantity_matrix_requires_new_one_unit_draft')
        previous = [baseline, 1]
        def change(control, operation, value, expected_staff, expected_units):
            args = {'control': control, 'operation': operation}
            if value is not None: args['value'] = value
            self.act({screen}, 'change_production_quantity', args, {screen},
                     lambda s: require(self.numbers(s,title) == previous[0] and amount(s['visible'].get('units')) == previous[1], 'quantity_precondition_changed'))
            require(self.numbers(self.state,title) == expected_staff,'quantity_staff_postcondition')
            require(amount(self.state['visible'].get('units')) == expected_units,'quantity_units_postcondition')
            previous[:] = [expected_staff, expected_units]
        available,space,_ = baseline
        limit = min(available,space)
        require(0 <= limit <= 100000,'quantity_staff_limit_invalid')
        for target in (1,7,10,100000):
            n=min(limit,target)
            change('engineers','set',target,(available-n,space-n,n),1)
        change('engineers','max',None,(available-limit,space-limit,limit),1)
        change('engineers','adjust',1,(available-limit,space-limit,limit),1)
        change('engineers','clear',None,baseline,1)
        # Capacity-sensitive craft orders are deliberately excluded by caller's
        # explicit title selection; any unexpected clamp/warning stops this route.
        for target in (1,7,1000,100000):
            change('units','set',target,baseline,target)
        change('units','adjust',-1000,baseline,99000)
        change('units','clear',None,baseline,1)
        self.record('production_finite_matrix_verified', master=self.master,
                    title=title, finite_max_tested=100000, restored_initial_draft=True,
                    infinite_mode_tested=False, committed=False, schema_overlimit_tested=False)
        return self.state
