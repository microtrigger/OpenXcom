# SPDX-License-Identifier: GPL-3.0-or-later
"""Visible containment draft controls, never hidden inventory authority."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,arguments,NativeFixture,CHALLENGE,EPOCH
ACTIONS={'right_click_base_facility_cell','increase_containment_removal','decrease_containment_removal','remove_selected_aliens','cancel_containment'}
class ContainmentTests(unittest.TestCase):
 def check(self,o):return a.validate_state(o,CHALLENGE,EPOCH)
 def test_schema_fields_and_nullable_cells(self):
  self.assertEqual(a.SCHEMA,'openxcom-agent-control/47');o=state(screen='manage_containment');self.check(o)
  for key in o['visible']:
   bad=copy.deepcopy(o);del bad['visible'][key]
   with self.assertRaises(Unavailable):self.check(bad)
  for key in ('base_id','alien_id','available_quantity','overfull','can_remove'):
   bad=copy.deepcopy(o);bad['visible'][key]=None
   with self.assertRaises(Unavailable):self.check(bad)
  o['schema']='openxcom-agent-control/36'
  with self.assertRaises(Unavailable):self.check(o)
  o=state(screen='manage_containment');o['visible']['rows'][0]['cells']=[None]*4;self.check(o)
 def test_rows_direction_witnesses_exact_and_bounded(self):
  for field,value in [('handle','r1'),('selected',1),('cells',[None]*3),('controls',['decrease','increase']),('controls',['increase','increase']),('controls',['hidden'])]:
   o=state(screen='manage_containment');o['visible']['rows'][0][field]=value
   with self.assertRaises(Unavailable):self.check(o)
  o=state(screen='manage_containment');o['visible']['rows'][0]['controls']=[]
  with self.assertRaises(Unavailable):self.check(o)
  o['actions']=[x for x in o['actions'] if x['id'] not in ('increase_containment_removal','decrease_containment_removal')];self.check(o)
  blank=copy.deepcopy(o);blank['visible']['rows'][0]['cells']=[None]*4
  with self.assertRaises(Unavailable):self.check(blank)
  o['visible']['rows']=[dict(handle=f'r{i}',cells=[None]*4,selected=False,controls=[]) for i in range(33)]
  with self.assertRaises(Unavailable):self.check(o)
 def test_actual_buttons_not_overfull_assumptions_and_labels(self):
  o=state(screen='manage_containment');o['visible']['buttons']=[];o['actions']=[x for x in o['actions'] if x['id'] not in ('remove_selected_aliens','cancel_containment')];self.check(o)
  for action in ('remove_selected_aliens','cancel_containment'):
   bad=copy.deepcopy(o);bad['actions'].append(next(x for x in state(screen='manage_containment')['actions'] if x['id']==action))
   with self.assertRaises(Unavailable):self.check(bad)
  for action in ACTIONS:
   screen='basescape' if action=='right_click_base_facility_cell' else 'manage_containment'
   for key,value in [('label','Invented'),('argument_kind','text'),('label_source','wrong')]:
    o=state(screen=screen);next(x for x in o['actions'] if x['id']==action)[key]=value
    with self.assertRaises(Unavailable):self.check(o)
  for screen in a.STATE_ACTIONS:
   for action in ACTIONS-a.STATE_ACTIONS[screen]:
    o=state(screen=screen);o['actions']=[dict(id=action,label='X',label_source=a.label_source(screen,action),argument_kind=a.ARGUMENT_KINDS[action])]
    with self.assertRaises(Unavailable):self.check(o)
 def test_args_are_one_step_only_no_quantity_or_repeat(self):
  for action in ('increase_containment_removal','decrease_containment_removal'):
   a.validate_action_arguments(arguments(action=action,args={'row':'r0'}))
   for args in ({'row':'r0','value':10},{'row':'r0','operation':'max'},{'row':'r0','repeat':2},{'row':'r32'},{'row':0},{'row':True}):
    with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=args))
  for action in ('remove_selected_aliens','cancel_containment'):
   a.validate_action_arguments(arguments(action=action,args={}))
   with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args={'confirm':True}))
  for v in (True,False,1.0,-1,6,'0'):
   with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action='right_click_base_facility_cell',args=dict(x=v,y=0)))
 def test_per_row_direction_stale_and_mutable_return_zero_send(self):
  class OneDirection(NativeFixture):
   def observation(self,challenge,capability):
    o=super().observation(challenge,capability);o['visible']['rows'][0]['controls']=['increase'];o['visible']['rows'].append(dict(handle='r1',cells=['Other','1','0','0'],selected=False,controls=['decrease']));return o
  for mode in ('wrong_direction','unknown','mutated','stale'):
   peer=OneDirection(screen='manage_containment')
   try:
    o=peer.client.observe();row='r0';action='decrease_containment_removal'
    if mode=='unknown':row='r2'
    if mode=='mutated':o['visible']['rows'][0]['controls'].append('decrease')
    if mode=='stale':peer.client.observe();action='increase_containment_removal'
    with self.assertRaises(Unavailable):peer.client.execute(**arguments(o,action=action,args={'row':row}))
    self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
   finally:peer.close()
 def test_remove_requires_ui_grant_and_snapshot_read_cannot_supply_it(self):
  from test_player_bases import Peer,sample
  peer=Peer([sample('base')])
  try:
   peer.client.read_player_bases();self.assertIsNone(peer.client.capability)
   with self.assertRaises(Unavailable):peer.client.execute(**arguments(action='remove_selected_aliens',args={}))
   self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
  finally:peer.close()
  peer=NativeFixture(screen='manage_containment')
  try:
   o=peer.client.observe();receipt=peer.client.execute(**arguments(o,action='remove_selected_aliens',args={}))
   self.assertEqual(receipt['status'],'dispatched');self.assertNotIn('aliens_removed',receipt)
   self.assertFalse(peer.client.containment_rows)
  finally:peer.close()
 def test_fresh_single_use_receipt_duplicate_and_cancel(self):
  peer=NativeFixture(screen='basescape',transitions={('basescape','right_click_base_facility_cell'):'manage_containment',('manage_containment','cancel_containment'):'basescape'})
  try:
   for i,(action,args) in enumerate([('right_click_base_facility_cell',dict(x=0,y=0)),('increase_containment_removal',dict(row='r0')),('decrease_containment_removal',dict(row='r0')),('cancel_containment',{})]):
    o=peer.client.observe();request=arguments(o,action=action,args=args,command_id=f'{i+1:032x}');receipt=peer.client.execute(**request)
    self.assertEqual(receipt['status'],'dispatched');self.assertNotIn('success',receipt);self.assertFalse(peer.client.containment_rows)
    self.assertEqual(peer.client.execute(**request),receipt);self.assertTrue(peer.requests[-1].startswith(b'S '))
   self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),4)
  finally:peer.close()
if __name__=='__main__':unittest.main()
