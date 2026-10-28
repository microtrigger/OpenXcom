# SPDX-License-Identifier: GPL-3.0-or-later
"""Diary branch action authority comes from actual shown controls, not history IDs."""
import copy
import unittest
from unittest.mock import patch
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,arguments,NativeFixture,CHALLENGE,EPOCH
NEW={'select_diary_mission','open_diary_combat','open_diary_awards','previous_diary_mission','next_diary_mission','close_diary_mission'}
class DiaryBranchesTests(unittest.TestCase):
 def check(self,o):return a.validate_state(o,CHALLENGE,EPOCH)
 def test_schema_closed_modes_mission_fields(self):
  self.assertEqual(a.SCHEMA,'openxcom-agent-control/46')
  for screen in ('soldier_diary','soldier_diary_performance','soldier_diary_mission'):
   o=state(screen=screen);self.check(o)
   for key in o['visible']:
    bad=copy.deepcopy(o);del bad['visible'][key]
    with self.assertRaises(Unavailable):self.check(bad)
   for key in ('mission_id','soldier_id','target_id','coordinates','hidden_kills'):
    bad=copy.deepcopy(o);bad['visible'][key]=None
    with self.assertRaises(Unavailable):self.check(bad)
   o['schema']='openxcom-agent-control/38'
   with self.assertRaises(Unavailable):self.check(o)
  for mode in ('missions','combat','awards'):
   o=state(screen='soldier_diary_performance');o['visible']['mode']=mode;self.check(o)
  for mode in ('kills','DIARY_MISSIONS',None,True,1):
   o=state(screen='soldier_diary_performance');o['visible']['mode']=mode
   with self.assertRaises(Unavailable):self.check(o)
 def test_nullable_text_does_not_remove_native_row_authority(self):
  o=state(screen='soldier_diary');o['visible']['rows']=[dict(handle=None,cells=[None]*5,selected=False),dict(handle='r0',cells=[None]*5,selected=False),dict(handle=None,cells=['Clipped']+[None]*4,selected=False),dict(handle='r1',cells=['']*5,selected=True)];self.check(o)
  for handle in ('r2','r0',0,False):
   bad=copy.deepcopy(o);bad['visible']['rows'][3]['handle']=handle
   with self.assertRaises(Unavailable):self.check(bad)
  bad=copy.deepcopy(o);bad['visible']['rows'][1]['selected']=True
  with self.assertRaises(Unavailable):self.check(bad)
  for row in o['visible']['rows']:row['handle']=None
  with self.assertRaises(Unavailable):self.check(o)
  o['actions']=[x for x in o['actions'] if x['id']!='select_diary_mission'];self.check(o)
 def test_action_labels_buttons_state_matrix_and_args(self):
  for action in NEW:
   a.validate_action_arguments(arguments(action=action,args={'row':'r0'} if action=='select_diary_mission' else {}))
   for args in ({'mission_id':0},{'row':'r0','mission_id':1},{'soldier':'s0'},{'mode':'awards'}):
    with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=args))
  for screen in a.STATE_ACTIONS:
   for action in NEW-a.STATE_ACTIONS[screen]:
    o=state(screen=screen);o['actions']=[dict(id=action,label='X',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action])]
    with self.assertRaises(Unavailable):self.check(o)
  for screen in a.DIARY_BUTTON_ACTIONS:
   for action in a.STATE_ACTIONS[screen]:
    for key,value in [('label','Wrong'),('label_source','wrong'),('argument_kind','hidden')]:
     o=state(screen=screen);next(x for x in o['actions'] if x['id']==action)[key]=value
     with self.assertRaises(Unavailable):self.check(o)
 def test_mission_and_awards_reports_not_actionable_tooltips(self):
  o=state(screen='soldier_diary_mission')
  for key in a.DIARY_MISSION_TEXT.split():o['visible'][key]=None
  o['visible']['rows']=[dict(cells=[None]*3)];self.check(o)
  for rows in ([dict(cells=[None]*4)],[dict(cells=[None]*3,handle='r0')],[dict(cells=[None]*3)]*33):
   bad=copy.deepcopy(o);bad['visible']['rows']=rows
   with self.assertRaises(Unavailable):self.check(bad)
  o=state(screen='soldier_diary_performance');o['visible']['mode']='awards';o['visible']['description']='Currently painted only';self.check(o)
  o['visible']['rows'][0]['handle']='r0'
  with self.assertRaises(Unavailable):self.check(o)
 def test_current_handle_not_model_or_mutable_or_stale(self):
  for mode in ('null','unknown','mutated','stale'):
   class Rows(NativeFixture):
    def observation(self,challenge,capability):
     o=super().observation(challenge,capability);o['visible']['rows'].insert(0,dict(handle=None,cells=['Read only']+[None]*4,selected=False));o['visible']['rows'][1]['cells']=[None]*5;return o
   peer=Rows(screen='soldier_diary')
   try:
    o=peer.client.observe();row='r1'
    if mode=='null':row=None
    if mode=='mutated':o['visible']['rows'][0]['handle']='r1'
    if mode=='stale':peer.client.observe();row='r0'
    with self.assertRaises(Unavailable):peer.client.execute(**arguments(o,action='select_diary_mission',args=dict(row=row)))
    self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
   finally:peer.close()
  peer=Rows(screen='soldier_diary')
  try:
   o=peer.client.observe();self.assertEqual(peer.client.execute(**arguments(o,action='select_diary_mission',args={'row':'r0'}))['status'],'dispatched')
  finally:peer.close()
 def test_native_branch_steps_duplicate_and_ownedread_no_grant(self):
  transitions={('soldier_diary','open_diary_combat'):'soldier_diary_performance',('soldier_diary_performance','close_diary_performance'):'soldier_diary',('soldier_diary','select_diary_mission'):'soldier_diary_mission',('soldier_diary_mission','close_diary_mission'):'soldier_diary'}
  peer=NativeFixture(screen='soldier_diary',transitions=transitions)
  try:
   for i,action in enumerate(('open_diary_combat','open_diary_awards','open_diary_performance','close_diary_performance','select_diary_mission','previous_diary_mission','next_diary_mission','close_diary_mission')):
    o=peer.client.observe();request=arguments(o,action=action,args={'row':'r0'} if action=='select_diary_mission' else {},command_id=f'{i+1:032x}');receipt=peer.client.execute(**request);self.assertEqual(receipt['status'],'dispatched');self.assertNotIn('success',receipt)
    self.assertEqual(peer.client.execute(**request),receipt);self.assertTrue(peer.requests[-1].startswith(b'S '))
   self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),8)
  finally:peer.close()
  from test_player_bases import Peer,sample
  peer=Peer([sample('soldier'),sample('soldier_diary')])
  try:
   peer.client.read_player_bases();self.assertIsNone(peer.client.capability)
   with self.assertRaises(Unavailable):peer.client.execute(**arguments(action='select_diary_mission',args={'row':'r0'}))
   self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
  finally:peer.close()
if __name__=='__main__':unittest.main()
