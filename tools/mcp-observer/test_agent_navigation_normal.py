# SPDX-License-Identifier: GPL-3.0-or-later
"""Normal painted craft Return/Patrol, not hidden fuel/model eligibility."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, NativeFixture, arguments, dump, result, COMMAND, EPOCH
from test_agent_events import page, validate

def normal():
    obj=state(screen='geoscape_craft_navigation')
    ids={'return_craft_to_base':'base','patrol_craft_current_position':'patrol','open_craft_destination':'target','cancel_craft_navigation':'cancel'}
    obj['visible']['buttons']=[dict(id=b,label=b.title()) for b in ids.values()]
    obj['actions']=[dict(id=k,label=v.title(),label_source='visible_text',argument_kind='none') for k,v in ids.items()]
    return obj

class NormalNavigationTests(unittest.TestCase):
    def valid(self,obj):return a.validate_state(obj,obj['challenge'],obj['epoch'])
    def invalid(self,obj):
        with self.assertRaises(Unavailable):self.valid(obj)
    def test_schema_no_new_actions(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/66');self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(286,109));self.valid(normal())
    def test_each_button_independent_visibility(self):
        ids=a.INTERCEPTION_BUTTON_ACTIONS['geoscape_craft_navigation']
        for action in ('return_craft_to_base','patrol_craft_current_position'):
            obj=normal();obj['actions']=[d for d in obj['actions'] if d['id']!=action];obj['visible']['buttons']=[d for d in obj['visible']['buttons'] if d['id']!=ids[action]];self.valid(obj)
            obj=normal();obj['visible']['buttons']=[d for d in obj['visible']['buttons'] if d['id']!=ids[action]];self.invalid(obj)
    def test_no_model_flag_for_button_authority(self):
        obj=state(screen='geoscape_craft_navigation');self.valid(obj)
        for key in ('fuel','low_fuel','mission_complete','destination_id','can_return'):
            obj=normal();obj['visible'][key]=False;self.invalid(obj)
    def test_normal_still_rejects_last_known(self):
        obj=normal();obj['visible']['buttons'].append(dict(id='last_known',label='Last'));obj['actions'].append(dict(id='go_to_craft_last_known_position',label='Last',label_source='visible_text',argument_kind='none'));self.invalid(obj)
    def test_redirect_still_rejects_cancel(self):
        obj=normal();obj['visible']['redirect']='REDIRECT CRAFT';self.invalid(obj)
    def test_exact_labels_noargs(self):
        for action in ('return_craft_to_base','patrol_craft_current_position'):
            self.assertTrue(a.canonical_action_request(arguments(action=action,args={})).endswith(action.encode()))
            with self.assertRaises(Unavailable):a.canonical_action_request(arguments(action=action,args={'force':True}))
            obj=normal();next(d for d in obj['actions'] if d['id']==action)['label']='Unshown';self.invalid(obj)
    def test_result_and_event(self):
        obj=normal();obj.update(challenge=None,capability=None,event_sequence=2);a.validate_result(dump(result(observation=obj)),COMMAND,EPOCH)
        validate(page([dict(seq=1,kind='presentation',state='geoscape_craft_navigation',visible=obj['visible'])]))
    def test_old_schema_rejected(self):
        obj=normal();obj['schema']='openxcom-agent-control/56';self.invalid(obj)

if __name__=='__main__':unittest.main()
