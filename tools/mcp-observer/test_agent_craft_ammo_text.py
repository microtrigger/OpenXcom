# SPDX-License-Identifier: GPL-3.0-or-later
"""CraftInfo multiline caption contract; no native paint or model truth proof."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, dump, result, COMMAND, EPOCH
from test_agent_events import page, validate

class CraftAmmoTextTests(unittest.TestCase):
    def valid(self,obj):return a.validate_state(obj,obj['challenge'],obj['epoch'])
    def invalid(self,obj):
        with self.assertRaises(Unavailable):self.valid(obj)
    def test_schema_shape_actions_unchanged(self):
        self.assertEqual(a.SCHEMA,'openxcom-agent-control/66');self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(286,109))
        obj=state(screen='craft_info');self.valid(obj);self.assertEqual(set(obj['visible']['weapons'][0]),{'name','ammo'})
    def test_exact_three_line_caption_preserved(self):
        for caption in ('AMMO> 0\nMAX> 200\n(4 hours)','AMMO> 0\nMAX> 200\n(4 heures)','AMMO> \x010\nMAX> \x01200\n(4 hours)'):
            obj=state(screen='craft_info');obj['visible']['weapons'][1]['ammo']=caption
            self.assertEqual(self.valid(obj)['visible']['weapons'][1]['ammo'],caption)
    def test_independent_null_not_zero_or_readiness(self):
        for index in (0,1):
            obj=state(screen='craft_info');obj['visible']['weapons'][index]=dict(name=None,ammo=None);self.valid(obj)
            obj['visible']['weapons'][1-index]=dict(name='Shown',ammo='AMMO> 0');self.valid(obj)
    def test_scalar_text_not_count_or_structured_model(self):
        for bad in (0,200,True,[],{},dict(current=0,maximum=200,hours=4)):
            obj=state(screen='craft_info');obj['visible']['weapons'][1]['ammo']=bad;self.invalid(obj)
    def test_existing_utf8_bound(self):
        obj=state(screen='craft_info');obj['visible']['weapons'][1]['ammo']='é'*512;self.valid(obj)
        obj['visible']['weapons'][1]['ammo']+='x';self.invalid(obj)
    def test_hidden_fields_not_added(self):
        for key in ('weapon_id','ammo_id','current_ammo','max_ammo','rearm_hours','ready','can_fire','rules'):
            obj=state(screen='craft_info');obj['visible']['weapons'][1][key]=0;self.invalid(obj)
    def test_result_retained_event_preserve_lines(self):
        obj=state(screen='craft_info',challenge=None,capability=None,event_sequence=2);obj['visible']['weapons'][1]['ammo']='AMMO> 0\nMAX> 200\n(4 hours)'
        a.validate_result(dump(result(observation=obj)),COMMAND,EPOCH)
        validate(page([dict(seq=1,kind='presentation',state='craft_info',visible=obj['visible'])]))
    def test_prior_semantic_version_rejected(self):
        obj=state(screen='craft_info');obj['schema']='openxcom-agent-control/57';self.invalid(obj)

if __name__=='__main__':unittest.main()
