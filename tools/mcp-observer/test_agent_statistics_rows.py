# SPDX-License-Identifier: GPL-3.0-or-later
"""Painted campaign Statistics rows and ordinary Memorial navigation, synthetic peers."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture,state,arguments,CHALLENGE,EPOCH
from test_agent_events import page,validate,presentation

class StatisticsRowsTests(unittest.TestCase):
 def check(self,obj):return a.validate_state(obj,CHALLENGE,EPOCH)
 def shown(self):return state(screen='statistics')
 def test_closed_schema_shape_and_readonly_rows(self):
  obj=self.shown();self.check(obj)
  self.assertEqual(a.SCHEMA,'openxcom-agent-control/48')
  self.assertEqual(len(a.ACTIONS),250)
  for key in obj['visible']:
   bad=copy.deepcopy(obj);del bad['visible'][key]
   with self.assertRaises(Unavailable):self.check(bad)
  for key in ('save_id','mission_ids','soldier_id','model_stats','total_count'):
   bad=self.shown();bad['visible'][key]=0
   with self.assertRaises(Unavailable):self.check(bad)
  for key in ('handle','selected','logicalIndex','stat_id'):
   bad=self.shown();bad['visible']['table']['rows'][0][key]=0
   with self.assertRaises(Unavailable):self.check(bad)
  obj['schema']='openxcom-agent-control/44'
  with self.assertRaises(Unavailable):self.check(obj)
 def test_nullable_display_strings_and_exact_two_columns(self):
  for value in (None,'','0','$12,345','é'*100):
   obj=self.shown();obj['visible']['title']=value;obj['visible']['table']['rows']=[dict(cells=[value,value])];self.check(obj)
  for value in (True,0,[],{},'é'*513):
   obj=self.shown();obj['visible']['table']['rows'][0]['cells'][1]=value
   with self.assertRaises(Unavailable):self.check(obj)
  for count in (0,1,3):
   obj=self.shown();obj['visible']['table']['rows']=[dict(cells=[None]*count)]
   with self.assertRaises(Unavailable):self.check(obj)
 def test_table_bounds_and_unavailable_semantics(self):
  obj=self.shown();obj['visible']['table']=dict(status='unavailable',rows=[]);self.check(obj)
  obj=self.shown();obj['visible']['table']['rows']*=128;self.check(obj)
  for table in (dict(status='unavailable',rows=[dict(cells=['a','b'])]),dict(status='partial',rows=[]),dict(status='shown',rows=[dict(cells=[None,None])]*129),dict(status='shown',rows=[],total=0)):
   obj=self.shown();obj['visible']['table']=table
   with self.assertRaises(Unavailable):self.check(obj)
 def test_scroll_button_authority_and_labels(self):
  for scroll in ({},dict(up=True,down=1),dict(up=False,down=False),dict(up=True,down=True,left=True)):
   obj=self.shown();obj['visible']['scroll']=scroll
   with self.assertRaises(Unavailable):self.check(obj)
  obj=self.shown();obj['visible']['scroll']=dict(up=False,down=False);obj['actions']=[x for x in obj['actions'] if x['id']!='scroll_visible_list'];self.check(obj)
  for entry in self.shown()['actions']:
   obj=self.shown();next(x for x in obj['actions'] if x['id']==entry['id'])['label']='Forged'
   with self.assertRaises(Unavailable):self.check(obj)
  obj=self.shown();obj['visible']['buttons']=[]
  with self.assertRaises(Unavailable):self.check(obj)
 def test_memorial_statistics_button_is_visible_noarg_only(self):
  obj=state(screen='soldier_memorial');self.check(obj)
  self.assertIn('open_statistics',{x['id'] for x in obj['actions']})
  for change in ('missing','empty','order'):
   bad=copy.deepcopy(obj)
   if change=='missing':bad['visible']['buttons']=[x for x in bad['visible']['buttons'] if x['id']!='statistics']
   elif change=='empty':bad['visible']['buttons'][-1]['label']=''
   else:bad['visible']['buttons'].reverse()
   with self.assertRaises(Unavailable):self.check(bad)
  for args in ({'save_id':'s0'},{'row':'r0'},{'mode':'ending'},[],None):
   with self.assertRaises(Unavailable):a.validate_action_arguments(dict(arguments(action='open_statistics'),args=args))
  a.validate_action_arguments(arguments(action='open_statistics',args={}))
 def test_fresh_dispatch_and_duplicate_receipts_no_replay(self):
  for screen,action,args in [('soldier_memorial','open_statistics',{}),('statistics','scroll_visible_list',{'direction':'down'}),('statistics','close_statistics',{})]:
   peer=NativeFixture(screen=screen);self.addCleanup(peer.close)
   old=peer.client.observe();fresh=peer.client.observe()
   with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action=action,args=args))
   request=arguments(fresh,action=action,args=args);receipt=peer.client.execute(**request)
   self.assertEqual(peer.client.execute(**request),receipt)
   self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)
 def test_unadvertised_direction_rejected_before_send(self):
  def transform(op,obj):
   if op=='R':obj['visible']['scroll']['up']=False
   return obj
  peer=NativeFixture(screen='statistics',transform=transform);self.addCleanup(peer.close)
  observed=peer.client.observe();observed['visible']['scroll']['up']=True
  with self.assertRaises(Unavailable):peer.client.execute(**arguments(observed,action='scroll_visible_list',args={'direction':'up'}))
  self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
 def test_lost_receipt_is_terminal_unknown_not_retried(self):
  peer=NativeFixture(screen='soldier_memorial',transform=lambda op,obj:None if op=='E' else obj);self.addCleanup(peer.close)
  request=arguments(peer.client.observe(),action='open_statistics',args={})
  with self.assertRaises(a.ActionOutcomeUnknown):peer.client.execute(**request)
  count=len(peer.requests)
  with self.assertRaises(Unavailable):peer.client.execute(**request)
  self.assertEqual(len(peer.requests),count)
 def test_readonly_rows_never_grant_selection(self):
  peer=NativeFixture(screen='statistics');self.addCleanup(peer.close);observed=peer.client.observe()
  self.assertFalse(peer.client.visible_rows)
  for action in ('select_memorial_soldier','select_diary_mission'):
   with self.assertRaises(Unavailable):peer.client.execute(**arguments(observed,action=action,args={'row':'r0'}))

 def test_retained_presentation_never_grants_new_authority(self):
  event=presentation(screen='statistics');validate(page([event]))
  peer=NativeFixture(screen='soldier_memorial');self.addCleanup(peer.close)
  observed=peer.client.observe();peer.public_events.append(event)
  peer.client.read_events(EPOCH,0,16)
  self.assertEqual(peer.client.available_actions,{x['id'] for x in observed['actions']})
  with self.assertRaises(Unavailable):peer.client.execute(**arguments(observed,action='close_statistics',args={}))
  malformed=copy.deepcopy(event);malformed['visible']['table']['rows'][0]['soldier_id']=1
  with self.assertRaises(Unavailable):validate(page([malformed]))

if __name__=='__main__':unittest.main()
