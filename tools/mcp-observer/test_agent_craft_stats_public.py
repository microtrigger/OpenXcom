# SPDX-License-Identifier: GPL-3.0-or-later
"""Narrow public50 CRAFT extension; synthetic contract tests, not live proof."""
import copy
import json
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,CHALLENGE,EPOCH
from test_agent_research_completion import checked,event
from test_agent_events import page,validate

class CraftStatsPublic(unittest.TestCase):
 def article(self,lines):
  obj=state(screen='ufopaedia_article');obj['visible'].update(craft_stats=lines,details_status='partial' if lines is not None else 'unavailable');return obj
 def accept(self,obj):
  self.assertEqual(checked(obj),obj);self.assertEqual(validate(page([event(obj)]))['events'][0],event(obj))
 def reject(self,obj):
  with self.assertRaises(Unavailable):checked(obj)
  with self.assertRaises(Unavailable):validate(page([event(obj)]))
 def test_exact_identity_and_unchanged_catalog(self):
  self.assertEqual(a.SCHEMA,'openxcom-agent-control/50-craft-stats1');self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS),len(a.definitions())),(261,106,7))
  for wire in ('50','63','51','50-craft-stats2','50-craft-stats0'):
   obj=self.article(None);obj['schema']='openxcom-agent-control/'+wire
   with self.subTest(wire=wire),self.assertRaises(Unavailable):checked(obj)
 def test_hello_ready_strict_branch_identity(self):
  for suffix in ('50-craft-stats1','50','63','51','50-craft-stats2'):
   hello=dict(schema='openxcom-agent-control/'+suffix,mode=a.MODE,epoch=EPOCH,ledger_capacity=256,command_generation=1,audio_history='mixed_output')
   ready=dict(schema=hello['schema'],mode=a.MODE,status='ready')
   for validator,obj in ((a.validate_hello,hello),(a.validate_ready,ready)):
    if suffix=='50-craft-stats1':validator(json.dumps(obj).encode())
    else:
     with self.subTest(suffix=suffix,validator=validator.__name__),self.assertRaises(Unavailable):validator(json.dumps(obj).encode())
 def test_null_and_exact_printed_lines(self):
  for lines in (None,[''],['SPEED\x011000','A\u00a0B','','CR\rpreserved'],['x'*1024]*8,['é'*512],['']*32):self.accept(self.article(lines))
 def test_malformed_or_oversized_lines(self):
  for lines in ([],{},True,'text',[None],[1],[True],[{}],['x']*33,['x'*1025],['é'*513],['x'*1024]*9,['a\nb'],['\ud800']):
   with self.subTest(lines=repr(lines)[:40]):self.reject(self.article(lines))
 def test_details_status_exact_derivation(self):
  for lines in (None,['shown']):
   for status in ('partial','unavailable',None,True,'complete','presented',''):
    if status==('unavailable' if lines is None else 'partial'):continue
    obj=self.article(lines);obj['visible']['details_status']=status;self.reject(obj)
 def test_closed_shape_no_private_superset(self):
  obj=self.article(None);del obj['visible']['craft_stats'];self.reject(obj)
  for key in ('article_stats','item_combat','rule_id','craft_id','damage','max_speed'):
   obj=self.article(None);obj['visible'][key]=None;self.reject(obj)
 def test_other_screens_do_not_admit_craft_field(self):
  for screen in ('research_complete','research_required','target_info'):
   obj=state(screen=screen);obj['visible']['craft_stats']=None
   with self.subTest(screen=screen),self.assertRaises(Unavailable):checked(obj)
 def test_craft_lines_do_not_add_action_authority(self):
  obj=self.article(['shown']);obj['visible']['buttons']=[];obj['actions']=[];obj['capability']=None;self.accept(obj)
  obj['actions']=[dict(id='close_ufopaedia_article',label='OK',label_source=a.label_source('ufopaedia_article','close_ufopaedia_article'),argument_kind='none')]
  with self.assertRaises(Unavailable):checked(obj)
  # Presentation events carry no action catalog and cannot mint authority.
  self.assertEqual(validate(page([event(obj)]))['events'][0],event(obj))
 def test_legacy_body_rules_not_relaxed(self):
  for value in (['bad\nline'],[' leading'],['bad  spacing'],['\x01color']):
   obj=self.article(['valid']);obj['visible']['body_lines']=value;self.reject(obj)
if __name__=='__main__':unittest.main()
