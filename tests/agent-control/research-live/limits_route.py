"""Bounded manual-entry manufacture limits; no time advance/completion claim.
Requires an empty Manufacture list and a normally researched stock pistol.
"""
import uuid
from production_route import ProductionRoute
from route import require, amount

class LimitsRoute(ProductionRoute):
    def probe_invalid_quantity(self, title):
        before = self.ready({'manufacture_info'})
        require(before['visible'].get('title') == title, 'production_title_changed')
        require(amount(before['visible'].get('units')) == 100000, 'overlimit_probe_requires_visible_100000')
        require('change_production_quantity' in {a['id'] for a in before.get('actions', [])},
                'quantity_action_not_offered')
        require(self.count < 128, 'dispatch_budget')
        request = {key: before[key] for key in ('epoch', 'capability', 'revision')}
        request.update(command_id=uuid.uuid4().hex, action='change_production_quantity',
                       args={'control': 'units', 'operation': 'set', 'value': 100001})
        self.record('schema_overlimit_probe_before', title=title, visible=before['visible'],
                    attempted_value=100001, expected_jsonrpc_code=-32602)
        self.count += 1
        receipt = self.call('probe_invalid_quantity', request) # One request; never retry.
        require(receipt == {'status': 'invalid_params_rejected', 'code': -32602,
                            'message': 'Invalid params'}, 'unexpected_schema_probe_response')
        after = self.ready({'manufacture_info'})
        require(after['visible'] == before['visible'], 'schema_rejection_visible_state_changed')
        self.record('schema_overlimit_rejection_verified', title=title, attempted_value=100001,
                    jsonrpc_code=-32602, visible_value=amount(after['visible'].get('units')),
                    visible_projection_unchanged=True, native_dispatch_claimed=False)
        return after


    def run(self):
        title={'xcom1':'Laser Pistol','xcom2':'Gauss Pistol'}[self.master]
        baseline=self.production_projection(self.ready({'manufacture'}))
        require(baseline['rows']==[], 'limits_requires_empty_production_list')
        self.record('limits_phase_begin',title=title,baseline=baseline,dispatch_budget=128,
                    production_completion_claimed=False,time_advance_requested=False)
        self.open_draft(title)
        self.production_finite_matrix(title)
        self.quantity(title,'units','set',100000)
        self.probe_invalid_quantity(title)
        self.quantity(title,'units','clear')
        self.infinite_roundtrip(title)
        initial=self.numbers(self.ready({'manufacture_info'}),title)
        require(initial[2]==0 and min(initial[:2])>=7,'limits_requires_seven_visible_engineers_and_spaces')
        state=self.quantity(title,'engineers','set',7)
        expected=(initial[0]-7,initial[1]-7,7)
        require(self.numbers(state,title)==expected,'limits_seven_engineers_mismatch')
        state=self.quantity(title,'units','set',7)
        def unchanged(s):
            require(self.numbers(s,title)==expected and amount(s['visible'].get('units'))==7,
                    'limits_persisted_quantity_mismatch')
            require(isinstance(s['visible'].get('sell'),dict) and s['visible']['sell'].get('pressed') is False,
                    'limits_sell_enabled_or_unknown')
        unchanged(state)
        self.act({'manufacture_info'},'accept_production',after={'new_manufacture'},guard=unchanged)
        self.act({'new_manufacture'},'close_production_list',after={'manufacture'})
        def exact_row(s):
            rows=s['visible'].get('rows',[])
            require(len(rows)==1 and rows[0].get('cells') and rows[0]['cells'][0]==title,
                    'limits_unique_order_not_visible')
            return {'row':rows[0]['handle']}
        self.act({'manufacture'},'select_production_project',exact_row,{'manufacture_info'})
        unchanged(self.state)
        self.record('limits_reopen_verified',title=title,units=7,engineers=7,
                    time_advance_requested=False,completion_claimed=False)
        state=self.quantity(title,'engineers','clear')
        require(self.numbers(state,title)==initial,'limits_staff_clear_mismatch')
        state=self.quantity(title,'units','clear')
        require(amount(state['visible'].get('units'))==1,'limits_clear_new_order_not_one')
        self.act({'manufacture_info'},'stop_production',after={'manufacture'},
                 guard=lambda s:require(self.numbers(s,title)==initial and amount(s['visible'].get('units'))==1,
                                        'limits_stop_precondition_changed'))
        final=self.production_projection(self.state)
        require(final['rows']==baseline['rows'] and final['summary'][:3]==baseline['summary'][:3],
                'limits_stop_staff_or_orders_changed')
        self.record('limits_funds_observed',before=baseline['summary'][3],after=final['summary'][3],
                    first_item_accept_may_charge=True,refund_claimed=False)
        self.record('limits_phase_complete',requests=self.count,title=title,
                    stopped_order=True,cancel_semantics_claimed=False,
                    public_staff_workshops_and_orders_restored=True,inventory_verified=False,
                    funds_or_material_refund_claimed=False,
                    finite_adapter_limit=100000,infinite_mode_tested=True,
                    keyboard_or_physical_input_tested=False)
