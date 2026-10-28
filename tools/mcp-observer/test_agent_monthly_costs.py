# SPDX-License-Identifier: GPL-3.0-or-later
"""Painted expense report UI is separate from independent owned expense reads."""
import copy
import unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, arguments, NativeFixture, CHALLENGE, EPOCH
class MonthlyCostsTests(unittest.TestCase):
 def check(self,o):return a.validate_state(o,CHALLENGE,EPOCH)
 def test_schema_closed_fields_and_nullable_report(self):
  self.assertEqual(a.SCHEMA,'openxcom-agent-control/46')
  o=state(screen='monthly_costs');self.check(o)
  for key in o['visible']:
   bad=copy.deepcopy(o);del bad['visible'][key]
   with self.assertRaises(Unavailable):self.check(bad)
  for key in ('base_id','monthly_income','hidden_cost','funds'):
   bad=copy.deepcopy(o);bad['visible'][key]=0
   with self.assertRaises(Unavailable):self.check(bad)
  o['schema']='openxcom-agent-control/35'
  with self.assertRaises(Unavailable):self.check(o)
  o=state(screen='monthly_costs')
  for key in ('title','craft_rental_label','salaries_label','income','maintenance'):o['visible'][key]=None
  o['visible']['headings']=[None]*3
  for key,n in [('craft_rows',4),('salary_rows',4),('maintenance_rows',2),('total_rows',2)]:o['visible'][key]=[dict(cells=[None]*n)]
  self.check(o)
 def test_row_shapes_bounds_and_no_selection_authority(self):
  for key,n in [('craft_rows',4),('salary_rows',4),('maintenance_rows',2),('total_rows',2)]:
   for value in ([dict(cells=['x']*(n+1))],[dict(cells=['x']*n,handle='r0')],[dict(cells=['x']*n,selected=False)],[dict(cells=[1]*n)],[dict(cells=['x']*n)]*33):
    o=state(screen='monthly_costs');o['visible'][key]=value
    with self.assertRaises(Unavailable):self.check(o)
 def test_labels_buttons_and_state_action_matrix(self):
  for screen,action in [('base_info','open_monthly_costs'),('monthly_costs','close_monthly_costs')]:
   self.check(state(screen=screen))
   for key,value in [('label','Invented'),('argument_kind','row'),('label_source','visible_control_semantics')]:
    o=state(screen=screen);next(x for x in o['actions'] if x['id']==action)[key]=value
    with self.assertRaises(Unavailable):self.check(o)
   o=state(screen=screen);o['visible']['buttons']=[]
   with self.assertRaises(Unavailable):self.check(o)
  for screen in a.STATE_ACTIONS:
   for action in {'open_monthly_costs','close_monthly_costs'}-a.STATE_ACTIONS[screen]:
    o=state(screen=screen);o['actions']=[dict(id=action,label='OK',argument_kind='none',label_source='visible_text')]
    with self.assertRaises(Unavailable):self.check(o)
 def test_exact_noargs_and_stale_zero_send(self):
  for action,screen in [('open_monthly_costs','base_info'),('close_monthly_costs','monthly_costs')]:
   a.validate_action_arguments(arguments(action=action,args={}))
   for args in ({'base_id':'base0'},{'row':'r0'},{'cost':0}):
    with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=args))
   peer=NativeFixture(screen=screen)
   try:
    old=peer.client.observe();peer.client.observe()
    with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action=action,args={}))
    self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
   finally:peer.close()
 def test_open_close_normal_handler_dispatch_and_duplicate(self):
  peer=NativeFixture(screen='base_info',transitions={('base_info','open_monthly_costs'):'monthly_costs',('monthly_costs','close_monthly_costs'):'base_info'})
  try:
   for i,action in enumerate(('open_monthly_costs','close_monthly_costs')):
    o=peer.client.observe();request=arguments(o,action=action,args={},command_id=f'{i+1:032x}')
    result=peer.client.execute(**request);self.assertEqual(result['status'],'dispatched');self.assertNotIn('success',result)
    self.assertEqual(peer.client.execute(**request),result);self.assertTrue(peer.requests[-1].startswith(b'S '))
   self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),2)
  finally:peer.close()
if __name__=='__main__':unittest.main()
