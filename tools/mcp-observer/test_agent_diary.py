# SPDX-License-Identifier: GPL-3.0-or-later
"""Visible Diary/Performance reports, never owned-history action authority."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,arguments,NativeFixture,CHALLENGE,EPOCH
SCREENS=('soldier_diary','soldier_diary_performance')
NEW={'open_soldier_diary','open_diary_performance','close_soldier_diary','close_diary_performance'}
class DiaryTests(unittest.TestCase):
 def check(self,o):return a.validate_state(o,CHALLENGE,EPOCH)
 def test_exact_schemas_old_refusal_and_unknown_model(self):
  self.assertEqual(a.SCHEMA,'openxcom-agent-control/40')
  for screen in SCREENS:
   o=state(screen=screen);self.check(o)
   for key in o['visible']:
    bad=copy.deepcopy(o);del bad['visible'][key]
    with self.assertRaises(Unavailable):self.check(bad)
   for key in ('soldier_id','mission_id','kills','awards','enemy','history','statistics'):
    bad=copy.deepcopy(o);bad['visible'][key]=None
    with self.assertRaises(Unavailable):self.check(bad)
   o['schema']='openxcom-agent-control/37'
   with self.assertRaises(Unavailable):self.check(o)
 def test_report_cells_nullable_exact_shapes(self):
  for screen in SCREENS:
   for key,n in ([('rows',5)] if screen=='soldier_diary' else [('rows',2),('totals',4)]):
    o=state(screen=screen);o['visible'][key]=[dict(handle='r0',cells=[None]*n,selected=False)] if screen=='soldier_diary' else [dict(cells=[None]*n)];self.check(o)
    for rows in ([dict(cells=['x']*(n+1))],[dict(cells=[1]*n)],[dict(cells=['x']*n,handle='r0')],[dict(cells=['x']*n,selected=False)],[dict(cells=['x']*n)]*33):
     bad=copy.deepcopy(o);bad['visible'][key]=rows
     with self.assertRaises(Unavailable):self.check(bad)
    o['visible'][key]=[];o['actions']=[x for x in o['actions'] if x['id']!='select_diary_mission'];self.check(o)
   for headings in ([],[None]*4,'Mission'):
    o=state(screen='soldier_diary');o['visible']['headings']=headings
    with self.assertRaises(Unavailable):self.check(o)
 def test_button_grants_labels_and_unsupported_tooltips(self):
  for screen in ('soldier_info',)+SCREENS:
   o=state(screen=screen);self.check(o)
   mapping=a.OUTFITTING_BUTTON_ACTIONS[screen] if screen=='soldier_info' else a.DIARY_BUTTON_ACTIONS[screen]
   for action,button in mapping.items():
    for key,value in [('label','Invented'),('label_source','visible_control_semantics'),('argument_kind','project_row')]:
     bad=copy.deepcopy(o);next(x for x in bad['actions'] if x['id']==action)[key]=value
     with self.assertRaises(Unavailable):self.check(bad)
    bad=copy.deepcopy(o);bad['visible']['buttons']=[x for x in bad['visible']['buttons'] if x['id']!=button]
    with self.assertRaises(Unavailable):self.check(bad)
  for screen in SCREENS:
   for button in ('tooltip','hidden_award','mission_details'):
    o=state(screen=screen);o['visible']['buttons'].append(dict(id=button,label='Visible'))
    with self.assertRaises(Unavailable):self.check(o)
   for action in ('choose_visible_row','select_diary_award','open_diary_tooltip','open_diary_mission'):
    o=state(screen=screen);o['actions'].append(dict(id=action,label='Visible',label_source='visible_text',argument_kind='none'))
    with self.assertRaises(Unavailable):self.check(o)
 def test_state_matrix_and_noargs_exact(self):
  for screen in a.STATE_ACTIONS:
   for action in NEW-a.STATE_ACTIONS[screen]:
    o=state(screen=screen);o['actions']=[dict(id=action,label='X',label_source='visible_text',argument_kind='none')]
    with self.assertRaises(Unavailable):self.check(o)
  for action in NEW|{'previous_personnel','next_personnel'}:
   a.validate_action_arguments(arguments(action=action,args={}))
   for args in ({'soldier_id':'s0'},{'row':'r0'},{'display':'DIARY_KILLS'},{'repeat':2}):
    with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=args))
 def test_fresh_scroll_direction_and_no_mutable_or_stale_grant(self):
  class UpOnly(NativeFixture):
   def observation(self,challenge,capability):
    o=super().observation(challenge,capability);o['visible']['scroll']['down']=False;return o
  for screen in SCREENS:
   for mode in ('direction','mutated','stale'):
    peer=UpOnly(screen=screen)
    try:
     o=peer.client.observe();direction='down'
     if mode=='mutated':o['visible']['scroll']['down']=True
     if mode=='stale':peer.client.observe();direction='up'
     with self.assertRaises(Unavailable):peer.client.execute(**arguments(o,action='scroll_visible_list',args=dict(direction=direction)))
     self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
    finally:peer.close()
   o=state(screen=screen);o['visible']['scroll']=dict(up=False,down=False)
   with self.assertRaises(Unavailable):self.check(o)
   o['actions']=[x for x in o['actions'] if x['id']!='scroll_visible_list'];self.check(o)
 def test_normal_handler_flow_duplicates_and_snapshot_noauthority(self):
  transitions={('soldier_info','open_soldier_diary'):'soldier_diary',('soldier_diary','open_diary_performance'):'soldier_diary_performance',('soldier_diary_performance','close_diary_performance'):'soldier_diary',('soldier_diary','close_soldier_diary'):'soldier_info'}
  peer=NativeFixture(screen='soldier_info',transitions=transitions)
  try:
   for i,action in enumerate(('open_soldier_diary','previous_personnel','next_personnel','open_diary_performance','previous_personnel','next_personnel','close_diary_performance','close_soldier_diary')):
    o=peer.client.observe();request=arguments(o,action=action,args={},command_id=f'{i+1:032x}');receipt=peer.client.execute(**request);self.assertEqual(receipt['status'],'dispatched');self.assertNotIn('success',receipt)
    self.assertEqual(peer.client.execute(**request),receipt);self.assertTrue(peer.requests[-1].startswith(b'S '))
   self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),8)
  finally:peer.close()
  from test_player_bases import Peer,sample
  peer=Peer([sample('soldier'),sample('soldier_diary')])
  try:
   peer.client.read_player_bases();self.assertIsNone(peer.client.capability)
   with self.assertRaises(Unavailable):peer.client.execute(**arguments(action='open_diary_performance',args={}))
   self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
  finally:peer.close()
if __name__=='__main__':unittest.main()
