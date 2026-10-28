# SPDX-License-Identifier: GPL-3.0-or-later
"""Fresh visible Memorial navigation; owned history never supplies UI authority."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,arguments,NativeFixture,CHALLENGE,EPOCH
SCREENS=('soldier_memorial','memorial_soldier_info','memorial_soldier_diary','memorial_soldier_diary_performance')
class MemorialTests(unittest.TestCase):
 def check(self,o):return a.validate_state(o,CHALLENGE,EPOCH)
 def test_closed_states_old_schema_and_no_hidden_identity(self):
  self.assertEqual(a.SCHEMA,'openxcom-agent-control/40')
  for screen in SCREENS:
   o=state(screen=screen);self.check(o)
   for key in o['visible']:
    bad=copy.deepcopy(o);del bad['visible'][key]
    with self.assertRaises(Unavailable):self.check(bad)
   for key in ('soldier_id','base_id','dead_roster','statistics','mission_ids','death_cause'):
    bad=copy.deepcopy(o);bad['visible'][key]=None
    with self.assertRaises(Unavailable):self.check(bad)
   o['schema']='openxcom-agent-control/39'
   with self.assertRaises(Unavailable):self.check(o)
 def test_dead_state_never_grants_mutation_even_with_forged_button(self):
  for screen in SCREENS:
   for action in ('open_soldier_armor','select_soldier_armor','set_craft_soldier','toggle_psi_training','open_memorial_statistics'):
    o=state(screen=screen);o['actions'].append(dict(id=action,label='Armor',label_source='visible_text',argument_kind=a.ARGUMENT_KINDS.get(action,'none')))
    with self.assertRaises(Unavailable):self.check(o)
   o=state(screen=screen);o['visible']['buttons'].append(dict(id='armor',label='Armor'))
   with self.assertRaises(Unavailable):self.check(o)
  o=state(screen='memorial_soldier_info');o['visible']['death_status']=None;self.check(o)
  o['visible']['death_status']='Killed in action';self.check(o)
  o['visible']['death_status']=True
  with self.assertRaises(Unavailable):self.check(o)
  # Living contract and armor action remain unchanged.
  o=state(screen='soldier_info');self.check(o);self.assertIn('open_soldier_armor',{d['id'] for d in o['actions']});self.assertNotIn('death_status',o['visible'])
 def test_nullable_captions_do_not_replace_row_authority(self):
  o=state(screen='soldier_memorial');o['visible']['rows']=[dict(handle=None,cells=[None]*5,selected=False),dict(handle='r0',cells=[None]*5,selected=False)];self.check(o)
  for rows in ([dict(handle='r1',cells=[None]*5,selected=False)],[dict(handle='r0',cells=[None]*5,selected=False)]*2,[dict(handle='r0',cells=[None]*4,selected=False)],[dict(handle='r0',cells=[None]*5,selected=1)]):
   bad=copy.deepcopy(o);bad['visible']['rows']=rows
   with self.assertRaises(Unavailable):self.check(bad)
  o['visible']['rows']=[]
  with self.assertRaises(Unavailable):self.check(o)
  o['actions']=[d for d in o['actions'] if d['id']!='select_memorial_soldier'];self.check(o)
 def test_shown_labels_noargs_and_row_arguments_are_exact(self):
  for screen in ('soldiers',)+SCREENS:
   o=state(screen=screen);self.check(o)
   for descriptor in o['actions']:
    bad=copy.deepcopy(o);next(d for d in bad['actions'] if d['id']==descriptor['id'])['label']='Invented'
    with self.assertRaises(Unavailable):self.check(bad)
  for action in ('open_memorial','close_memorial','open_soldier_diary','close_soldier_info'):
   a.validate_action_arguments(arguments(action=action,args={}))
   for args in ({'soldier_id':'s0'},{'row':'r0'},{'repeat':2}):
    with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=args))
  for args in ({'row':'r32'},{'row':0},{'soldier_id':'r0'},{'row':'r0','repeat':1}):
   with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='select_memorial_soldier',args=args))
 def test_fresh_row_membership_is_immutable_and_single_use(self):
  for mode in ('missing','mutated','stale'):
   peer=NativeFixture(screen='soldier_memorial')
   try:
    o=peer.client.observe();row='r1'
    if mode=='mutated':o['visible']['rows'].append(dict(handle='r1',cells=[None]*5,selected=False))
    if mode=='stale':peer.client.observe();row='r0'
    with self.assertRaises(Unavailable):peer.client.execute(**arguments(o,action='select_memorial_soldier',args=dict(row=row)))
    self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
   finally:peer.close()
 def test_each_memorial_scroll_control_dispatches_once(self):
  for screen in ('soldier_memorial','memorial_soldier_diary','memorial_soldier_diary_performance'):
   peer=NativeFixture(screen=screen)
   try:
    for i,direction in enumerate(('up','down')):
     o=peer.client.observe();request=arguments(o,action='scroll_visible_list',args=dict(direction=direction),command_id=f'{i+1:032x}')
     receipt=peer.client.execute(**request);self.assertEqual(receipt['status'],'dispatched')
     self.assertEqual(peer.client.execute(**request),receipt);self.assertTrue(peer.requests[-1].startswith(b'S '))
    self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),2)
   finally:peer.close()
 def test_reverse_registered_arrow_labels_and_fresh_scroll(self):
  for screen in ('memorial_soldier_info','memorial_soldier_diary','memorial_soldier_diary_performance'):
   o=state(screen=screen)
   for button in o['visible']['buttons']:
    if button['id'] in ('previous','next'):button['label']='>>' if button['id']=='previous' else '<<'
   for descriptor in o['actions']:
    if descriptor['id'] in ('previous_personnel','next_personnel'):descriptor['label']='>>' if descriptor['id']=='previous_personnel' else '<<'
   self.check(o)
   next(d for d in o['actions'] if d['id']=='previous_personnel')['label']='<<'
   with self.assertRaises(Unavailable):self.check(o)
  class UpOnly(NativeFixture):
   def observation(self,challenge,capability):
    o=super().observation(challenge,capability);o['visible']['scroll']['down']=False;return o
  peer=UpOnly(screen='soldier_memorial')
  try:
   o=peer.client.observe();o['visible']['scroll']['down']=True
   with self.assertRaises(Unavailable):peer.client.execute(**arguments(o,action='scroll_visible_list',args=dict(direction='down')))
   self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
  finally:peer.close()
 def test_full_navigation_duplicates_and_owned_history_no_grants(self):
  transitions={('soldiers','open_memorial'):'soldier_memorial',('soldier_memorial','select_memorial_soldier'):'memorial_soldier_info',('memorial_soldier_info','open_soldier_diary'):'memorial_soldier_diary',('memorial_soldier_diary','open_diary_performance'):'memorial_soldier_diary_performance',('memorial_soldier_diary_performance','close_diary_performance'):'memorial_soldier_diary',('memorial_soldier_diary','close_soldier_diary'):'memorial_soldier_info',('memorial_soldier_info','close_soldier_info'):'soldier_memorial',('soldier_memorial','close_memorial'):'soldiers'}
  peer=NativeFixture(screen='soldiers',transitions=transitions)
  try:
   actions=('open_memorial','select_memorial_soldier','previous_personnel','next_personnel','open_soldier_diary','open_diary_performance','close_diary_performance','close_soldier_diary','close_soldier_info','close_memorial')
   for i,action in enumerate(actions):
    o=peer.client.observe();request=arguments(o,action=action,args={'row':'r0'} if action=='select_memorial_soldier' else {},command_id=f'{i+1:032x}')
    receipt=peer.client.execute(**request);self.assertEqual(receipt['status'],'dispatched');self.assertNotIn('success',receipt)
    self.assertEqual(peer.client.execute(**request),receipt);self.assertTrue(peer.requests[-1].startswith(b'S '))
   self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),len(actions))
  finally:peer.close()
  from test_player_bases import Peer,sample
  peer=Peer([sample('soldier'),sample('soldier_diary')])
  try:
   peer.client.read_player_bases();self.assertIsNone(peer.client.capability)
   with self.assertRaises(Unavailable):peer.client.execute(**arguments(action='select_memorial_soldier',args=dict(row='r0')))
   self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
  finally:peer.close()
if __name__=='__main__':unittest.main()
