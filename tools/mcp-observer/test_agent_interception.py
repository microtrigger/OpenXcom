# SPDX-License-Identifier: GPL-3.0-or-later
"""Target-bound interception DTO/authority; synthetic sockets, not live combat proof."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, state, arguments, CHALLENGE, EPOCH

NEW_ACTIONS={'open_contact_interception','cancel_interception','open_intercept_base','confirm_interception','cancel_interception_confirmation'}
class InterceptionTests(unittest.TestCase):
    def checked(self,obj):return a.validate_state(obj,CHALLENGE,EPOCH)
    def fixture(self,**kwargs):
        peer=NativeFixture(**kwargs);self.addCleanup(peer.close);return peer
    def test_exact_closed_shapes_labels_and_missing_buttons(self):
        for screen in a.INTERCEPTION_BUTTON_ACTIONS:
            obj=state(screen=screen);self.checked(obj)
            for key in obj['visible']:
                bad=copy.deepcopy(obj);del bad['visible'][key]
                with self.assertRaises(Unavailable):self.checked(bad)
            for key in ('target_id','craft_id','eligible','fuel','distance','speed','world_coordinates','success'):
                bad=copy.deepcopy(obj);bad['visible'][key]=0
                with self.assertRaises(Unavailable):self.checked(bad)
            for i in range(len(obj['actions'])):
                bad=copy.deepcopy(obj);bad['actions'][i]['label']='invented'
                with self.assertRaises(Unavailable):self.checked(bad)
            bad=copy.deepcopy(obj);bad['visible']['buttons']=[]
            with self.assertRaises(Unavailable):self.checked(bad)
    def test_unknown_strings_no_inferred_eligibility(self):
        obj=state(screen='intercept_craft');obj['visible']['title']=None
        obj['visible']['rows'][0]['cells']=[None]*4;self.checked(obj)
        obj=state(screen='interception_confirm');obj['visible']['target']=None;self.checked(obj)
        for cells in ([None]*3,[None]*5,[True]*4):
            obj=state(screen='intercept_craft');obj['visible']['rows'][0]['cells']=cells
            with self.assertRaises(Unavailable):self.checked(obj)
    def test_canonical_rows_selection_limit_and_empty_authority(self):
        obj=state(screen='intercept_craft');row=obj['visible']['rows'][0]
        obj['visible']['rows']=[dict(copy.deepcopy(row),handle=f'r{i}',selected=i==31) for i in range(32)];self.checked(obj)
        for key,value in [('handle','r32'),('handle','r00'),('selected',True),('selected',1),('controls',['increase'])]:
            bad=copy.deepcopy(obj);bad['visible']['rows'][0][key]=value
            with self.assertRaises(Unavailable):self.checked(bad)
        obj['visible']['rows']=[]
        with self.assertRaises(Unavailable):self.checked(obj)
        obj['actions']=[entry for entry in obj['actions'] if entry['id']!='choose_visible_row'];self.checked(obj)
    def test_wrong_screen_and_payload_injection(self):
        for screen in a.STATE_ACTIONS:
            for action in NEW_ACTIONS-a.STATE_ACTIONS[screen]:
                obj=state(screen=screen);obj['actions']=[dict(id=action,label='Shown',label_source=a.label_source(screen,action),argument_kind='none')]
                with self.subTest(screen=screen,action=action),self.assertRaises(Unavailable):self.checked(obj)
        for action in NEW_ACTIONS:
            self.assertTrue(a.canonical_action_request(arguments(action=action,args={})).endswith(action.encode()))
            for args in ({'row':'r0'},{'target_id':1},{'x':0,'y':0},{'confirm':True}):
                with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=args))
    def test_hidden_base_button_and_contact_button_do_not_authorize(self):
        for screen,action,button in [('intercept_craft','open_intercept_base','base'),('ufo_contact','open_contact_interception','intercept')]:
            obj=state(screen=screen);obj['visible']['buttons']=[x for x in obj['visible']['buttons'] if x['id']!=button]
            with self.assertRaises(Unavailable):self.checked(obj)
            obj['actions']=[x for x in obj['actions'] if x['id']!=action];self.checked(obj)
    def test_actions_use_fresh_single_use_grant(self):
        scenarios={'open_contact_interception':'ufo_contact'}
        for screen in a.INTERCEPTION_BUTTON_ACTIONS:scenarios.update(dict.fromkeys(a.STATE_ACTIONS[screen],screen))
        for action,screen in scenarios.items():
            peer=self.fixture(screen=screen);obj=peer.client.observe()
            self.assertEqual(peer.client.execute(**arguments(obj,action=action))['status'],'dispatched')
            self.assertIsNone(peer.client.capability);self.assertFalse(peer.client.visible_rows or peer.client.scroll_directions)
    def test_stale_rows_and_unshown_scroll_rejected_before_send(self):
        def transform(op,obj):
            if op=='R':obj['visible']['scroll']['up']=False
            return obj
        peer=self.fixture(screen='intercept_craft',transform=transform);old=peer.client.observe();fresh=peer.client.observe()
        for obj,action,args in [(old,'choose_visible_row',{'row':'r0'}),(fresh,'choose_visible_row',{'row':'r31'}),(fresh,'scroll_visible_list',{'direction':'up'})]:
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(obj,action=action,args=args))
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))

if __name__=='__main__':unittest.main()
