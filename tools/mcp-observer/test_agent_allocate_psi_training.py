# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic strict authority tests, not live training or capacity simulation."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

SCREEN = 'allocate_psi_training'
TOGGLE = 'toggle_psi_training'
CLOSE = 'close_psi_training_allocation'

class AllocatePsiTrainingTests(unittest.TestCase):
    def checked(self, obj):
        return a.validate_state(obj, CHALLENGE, EPOCH)

    def fixture(self, **kwargs):
        peer = NativeFixture(screen=SCREEN, **kwargs)
        self.addCleanup(peer.close)
        return peer

    def test_current_cells_unknown_and_duplicate_names(self):
        obj = state(screen=SCREEN)
        obj['visible']['remaining'] = 'Remaining Psi Lab Capacity>0'
        obj['visible']['rows'] = [dict(handle=f'r{i}', cells=['Same', 'Unknown', '0/+0', 'No'], selected=False) for i in range(2)]
        self.assertEqual(self.checked(obj), obj)
        for field in ('title', 'remaining'):
            for value in (None, '', 'x' * 1024):
                candidate=copy.deepcopy(obj);candidate['visible'][field]=value;self.checked(candidate)
        obj['visible']['rows'][0]['cells']=[None,'Unknown','500\nlogical caption','Yes']
        self.checked(obj)

    def test_no_hidden_values_or_missing_keys(self):
        obj=state(screen=SCREEN)
        for key in ('capacity','psi_strength','soldier_id','base_id','eligible','training_complete'):
            candidate=copy.deepcopy(obj);candidate['visible'][key]=42
            with self.assertRaises(Unavailable):self.checked(candidate)
        for key in obj['visible']:
            candidate=copy.deepcopy(obj);del candidate['visible'][key]
            with self.assertRaises(Unavailable):self.checked(candidate)

    def test_rows_strict_current_handles_and_four_cells(self):
        for mutate in (lambda v:v['rows'][0].update(handle='r1'),lambda v:v['rows'][0].update(cells=['A']*3),
                       lambda v:v['rows'][0].update(cells=['A']*5),lambda v:v['rows'][0].update(selected=1),
                       lambda v:v['rows'][0].update(soldier_id='hidden'),lambda v:v.update(headings=[None]*3)):
            obj=state(screen=SCREEN);mutate(obj['visible'])
            with self.assertRaises(Unavailable):self.checked(obj)
        obj=state(screen=SCREEN);obj['visible']['rows']=[]
        with self.assertRaises(Unavailable):self.checked(obj)
        obj['actions']=[x for x in obj['actions'] if x['id']!=TOGGLE];self.checked(obj)

    def test_exact_action_arguments_and_no_desired_state(self):
        for bad in ({},{'row':'r32'},{'row':0},{'row':'r0','assigned':True},{'row':'r0','repeat':2}):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=TOGGLE,args=bad))
        a.validate_action_arguments(arguments(action=TOGGLE,args={'row':'r0'}))
        a.validate_action_arguments(arguments(action=CLOSE,args={}))
        with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=CLOSE,args={'cancel':True}))

    def test_button_pair_and_explicit_semantic_labels(self):
        obj=state(screen=SCREEN);obj['visible']['buttons']=[]
        with self.assertRaises(Unavailable):self.checked(obj)
        obj['actions']=[x for x in obj['actions'] if x['id']!=CLOSE];self.checked(obj)
        obj=state(screen=SCREEN)
        next(x for x in obj['actions'] if x['id']==TOGGLE)['label']='Assign all'
        with self.assertRaises(Unavailable):self.checked(obj)

    def test_neighbours_never_authorize_new_actions(self):
        for screen in a.STATE_ACTIONS:
            if screen==SCREEN:continue
            for action in (TOGGLE,CLOSE):
                obj=state(screen=screen);obj['actions']=[dict(id=action,label='Shown',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action])]
                with self.subTest(screen=screen,action=action),self.assertRaises(Unavailable):self.checked(obj)

    def test_read_stale_and_unlisted_row_never_send(self):
        peer=self.fixture();old=peer.client.observe();fresh=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action=TOGGLE))
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,action=TOGGLE,args={'row':'r1'}))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))

    def test_toggle_receipt_duplicate_is_query_not_second_toggle(self):
        peer=self.fixture();request=arguments(peer.client.observe(),action=TOGGLE)
        receipt=peer.client.execute(**request);self.assertEqual(receipt['status'],'dispatched')
        self.assertEqual(peer.client.execute(**request),receipt)
        self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)
        with self.assertRaises(Unavailable):peer.client.execute(**dict(request,args={'row':'r1'}))
        self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)

    def test_close_transition_not_training_success_or_auto_action(self):
        peer=self.fixture(transitions={(SCREEN,CLOSE):'unsupported'})
        receipt=peer.client.execute(**arguments(peer.client.observe(),action=CLOSE));self.assertEqual(receipt['status'],'dispatched')
        obj=peer.client.observe();self.assertEqual(obj['state'],'unsupported')
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action=TOGGLE))
        self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)

    def test_scroll_requires_shown_direction(self):
        obj=state(screen=SCREEN);obj['visible']['scroll']={'up':False,'down':False}
        with self.assertRaises(Unavailable):self.checked(obj)
        obj['actions']=[x for x in obj['actions'] if x['id']!='scroll_visible_list'];self.checked(obj)

if __name__=='__main__':unittest.main()
