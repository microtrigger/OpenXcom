# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-19.
"""Wire19 unknown-label research controls; synthetic cases are not native paint proof."""
import copy
import json
import unittest
import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, call, EPOCH, CHALLENGE, COMMAND
from test_agent_events import page, validate


def row_control(labels=(None,'Motion Scanner','Medi-Kit')):
    obj=state(screen='new_research')
    obj['visible']['rows']=[dict(handle=f'r{i}',cells=[label],selected=False) for i,label in enumerate(labels)]
    return obj


def checked(obj):return a.validate_state(obj,CHALLENGE,EPOCH)


class ResearchControlValidation(unittest.TestCase):
    def test_wire19_no_new_actions_states_tools_or_event_shapes(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/25')
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS),len(a.definitions())),(144,61,4))
        obj=row_control();self.assertEqual(checked(obj),obj)
        obj['schema']='openxcom-agent-control/18'
        with self.assertRaises(Unavailable):checked(obj)

    def test_unknown_and_known_rows_use_same_explicit_control_semantics(self):
        for labels in ((None,),('Laser Weapons',),(None,'Particle Disturbance Sensor','Medi-Kit')):
            obj=row_control(labels);checked(obj)
            action=next(x for x in obj['actions'] if x['id']=='select_research_project')
            self.assertEqual(action,dict(id='select_research_project',label='Select research project',label_source='visible_control_semantics',argument_kind='project_row'))

    def test_no_bounds_hidden_index_or_label_guesses(self):
        for key,value in [('bounds',dict(x=0,y=0,width=20,height=8)),('logical_index',0),('project_id','STR_GAUSS'),('name','Gauss Technology'),('text_complete',False),('eligible',True)]:
            obj=row_control();obj['visible']['rows'][0][key]=value
            with self.subTest(key=key),self.assertRaises(Unavailable):checked(obj)
        for label in ('Gauss Technology','Unknown project','Select first row',''):
            obj=row_control();next(x for x in obj['actions'] if x['id']=='select_research_project')['label']=label
            with self.assertRaises(Unavailable):checked(obj)

    def test_nullable_cell_is_confined_to_new_research(self):
        for screen in ('research','manufacture','new_manufacture','manufacture_start','funding','base_stores','base_transfers','new_battle_dropdown'):
            obj=state(screen=screen)
            if screen=='manufacture_start':obj['visible']['rows']=[dict(handle='r0',cells=['Alloy','1','2'],selected=False)]
            obj['visible']['rows'][0]['cells'][0]=None
            with self.subTest(screen=screen),self.assertRaises(Unavailable):checked(obj)

    def test_null_does_not_allow_other_cell_types_or_extra_columns(self):
        for cells in ([],[None,None],[False],[0],[{}],[[]],['x'*1025],['é'*513]):
            obj=row_control();obj['visible']['rows'][0]['cells']=cells
            with self.subTest(cells=cells),self.assertRaises(Unavailable):checked(obj)
        for value in ('é'*512,'TU>\x0150'):
            obj=row_control((value,));self.assertEqual(checked(obj)['visible']['rows'][0]['cells'],[value])

    def test_unknown_control_still_requires_title_rows_and_nonempty_text_if_known(self):
        for key,value in [('title',None),('rows',[])]:
            obj=row_control();obj['visible'][key]=value
            with self.assertRaises(Unavailable):checked(obj)
        obj=row_control(('',))
        with self.assertRaises(Unavailable):checked(obj)
        obj=row_control();obj['actions']=[x for x in obj['actions'] if x['id']!='select_research_project'];checked(obj)

    def test_rows_keep_public_array_order_and_compact_handles(self):
        labels=[None,'Zulu','Alpha',None];obj=row_control(labels)
        self.assertEqual([r['cells'][0] for r in checked(obj)['visible']['rows']],labels)
        # Python preserves admitted native order; it does not invent a logical-index or alphabetical ordering.
        for handles in (['r1','r0','r2','r3'],['r0','r0','r2','r3'],['r0','r2','r3','r4']):
            bad=copy.deepcopy(obj)
            for row,handle in zip(bad['visible']['rows'],handles):row['handle']=handle
            with self.assertRaises(Unavailable):checked(bad)

    def test_capacity_selected_and_current_research_label_binding_unchanged(self):
        obj=row_control([None]*32);checked(obj)
        obj['visible']['rows'].append(dict(handle='r32',cells=[None],selected=False))
        with self.assertRaises(Unavailable):checked(obj)
        obj=row_control();obj['visible']['rows'][0]['selected']=True;obj['visible']['rows'][1]['selected']=True
        with self.assertRaises(Unavailable):checked(obj)
        obj=state(screen='research');action=next(x for x in obj['actions'] if x['id']=='select_research_project')
        self.assertEqual(action['label_source'],'visible_text');self.assertEqual(action['label'],obj['visible']['rows'][0]['cells'][0])
        action.update(label='Select research project',label_source='visible_control_semantics')
        with self.assertRaises(Unavailable):checked(obj)

    def test_retained_unknown_control_projection_has_no_action_authority(self):
        event=dict(seq=1,kind='presentation',state='new_research',visible=row_control()['visible'])
        self.assertEqual(validate(page([event]))['events'],[event])
        for key,value in [('actions',[]),('capability','1'*32),('project_id','Gauss')]:
            bad=copy.deepcopy(event);bad[key]=value
            with self.assertRaises(Unavailable):validate(page([bad]))
        bad=copy.deepcopy(event);bad['state']='research'
        with self.assertRaises(Unavailable):validate(page([bad]))


class UnknownControlFixture(NativeFixture):
    def observation(self,challenge,capability):
        obj=super().observation(challenge,capability)
        if self.screen=='new_research':obj['visible']=row_control()['visible']
        elif self.screen=='research_info':obj['visible']['title']='Gauss Technology'
        return obj


class ResearchControlClient(unittest.TestCase):
    def fixture(self,**kwargs):
        peer=UnknownControlFixture(screen='new_research',**kwargs);self.addCleanup(peer.close);return peer

    def test_unknown_first_control_ordinary_selection_and_separate_later_title(self):
        peer=self.fixture(transitions={('new_research','select_research_project'):'research_info'})
        before=peer.client.observe();handle=before['visible']['rows'][0]['handle'];self.assertIsNone(before['visible']['rows'][0]['cells'][0])
        receipt=peer.client.execute(**arguments(before,action='select_research_project',args={'row':handle}))
        self.assertEqual(receipt['status'],'dispatched');self.assertEqual(receipt['observation']['visible']['title'],'Gauss Technology')
        self.assertIsNone(before['visible']['rows'][0]['cells'][0]);self.assertIsNone(receipt['observation']['capability'])
        fresh=peer.client.observe();self.assertEqual(fresh['visible']['title'],'Gauss Technology')
        self.assertFalse(peer.client.project_rows)

    def test_unknown_control_freshness_then_exact_duplicate_status_only(self):
        peer=self.fixture();old=peer.client.observe();fresh=peer.client.observe()
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='select_research_project'))
        args=arguments(fresh,action='select_research_project');first=peer.client.execute(**args);self.assertEqual(peer.client.execute(**args),first)
        self.assertEqual(peer.requests[-1],('S '+COMMAND).encode())
        with self.assertRaises(Unavailable):peer.client.execute(**dict(args,args={'row':'r1'}))
        self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)

    def test_unoffered_unknown_control_is_not_authority(self):
        peer=self.fixture();obj=peer.client.observe();count=len(peer.requests)
        with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action='select_research_project',args={'row':'r31'}))
        self.assertEqual(len(peer.requests),count);self.assertEqual(peer.client.capability,obj['capability'])

    def test_event_unknown_label_cannot_refresh_or_change_grant(self):
        peer=self.fixture();obj=peer.client.observe();saved=(peer.client.capability,peer.client.revision,peer.client.project_rows)
        peer.public_events=[dict(seq=1,kind='presentation',state='new_research',visible=row_control([None])['visible'])]
        peer.client.read_events(EPOCH,0,16);self.assertEqual(saved,(peer.client.capability,peer.client.revision,peer.client.project_rows))
        peer.public_events[0]['visible']['rows'][0]['cells'][0]='Guessed later name'
        with self.assertRaises(Unavailable):peer.client.read_events(EPOCH,0,16)
        self.assertEqual(peer.client.state,'CLOSED')

    def test_uncertainty_after_unknown_control_selection_never_replays(self):
        peer=self.fixture(transform=lambda op,obj:b'{' if op=='E' else obj);obj=peer.client.observe();args=arguments(obj,action='select_research_project')
        with self.assertRaises(a.ActionOutcomeUnknown):peer.client.execute(**args)
        with self.assertRaises(Unavailable):peer.client.execute(**args)
        self.assertEqual(len([x for x in peer.requests if x.startswith(b'E ')]),1)

    def test_real_server_preserves_null_text_and_returns_native_dispatch_scope(self):
        peer=self.fixture();server=adapter.Server(agent_control=peer.client)
        result=call(server,'read_ui_state')['result'];obj=result['structuredContent']
        self.assertIsNone(obj['visible']['rows'][0]['cells'][0]);self.assertEqual(json.loads(result['content'][0]['text']),obj)
        reply=call(server,'execute_ui_action',arguments(obj,action='select_research_project'))['result']
        self.assertFalse(reply['isError']);self.assertEqual(reply['structuredContent']['semantics'],'dispatch_not_success')


if __name__=='__main__':unittest.main()
