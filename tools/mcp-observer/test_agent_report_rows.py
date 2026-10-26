# SPDX-License-Identifier: GPL-3.0-or-later
"""Presented table DTO bounds only; no reconstructed statistics or outcome claims."""
import copy, unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, CHALLENGE, EPOCH
class ReportRowsTests(unittest.TestCase):
 def check(self,o):return a.validate_state(o,CHALLENGE,EPOCH)
 def debrief(self):
  o=state(screen='debriefing');o['visible']['table_status']='shown'
  o['visible']['tables']={'stats':{'status':'shown','rows':[{'cells':['Aliens killed...', '5', '50']}]},'recovery':{'status':'shown','rows':[{'cells':['Alien corpses...', '5', '25']}]},'total':{'status':'shown','rows':[{'cells':['Total...', '141']}]}}
  return o
 def test_presented_rows(self):
  for o in [self.debrief(),state(screen='lost_in_service')]:self.check(o)
 def test_partial_and_unavailable(self):
  o=self.debrief();o['visible']['tables']['stats']={'status':'unavailable','rows':[]};o['visible']['table_status']='partial';self.check(o)
  o['visible']['table_status']='shown'
  with self.assertRaises(Unavailable):self.check(o)
 def test_unknown_empty_and_dot_text(self):
  o=self.debrief();o['visible']['tables']['stats']['rows'][0]['cells']=[None,'','Logical caption...'];self.check(o)
  o=state(screen='lost_in_service');o['visible']['rows']=[{'cells':['name extends allocation','','Rookie','','Kills: 1']}];self.check(o)
 def test_wrong_cells_hidden_keys(self):
  for n in (0,2,4,6):
   o=state(screen='lost_in_service');o['visible']['rows']=[{'cells':['x']*n}]
   with self.assertRaises(Unavailable):self.check(o)
  for key in ('soldier_id','diary','position','logicalIndex','handle'):
   o=state(screen='lost_in_service');o['visible']['rows'][0][key]=1
   with self.assertRaises(Unavailable):self.check(o)
 def test_bounds_and_status(self):
  for rows in ([{'cells':['x']*5}]*129,[{'cells':['x'*1025]*5}]):
   o=state(screen='lost_in_service');o['visible']['rows']=rows
   with self.assertRaises(Unavailable):self.check(o)
  o=state(screen='lost_in_service');o['visible']['table_status']='unavailable'
  with self.assertRaises(Unavailable):self.check(o)
 def test_no_new_selection_authority(self):
  self.assertEqual(a.STATE_ACTIONS['lost_in_service'],{'acknowledge_lost_in_service'})
  self.assertEqual(a.STATE_ACTIONS['debriefing'],{'close_debriefing'})
if __name__=='__main__':unittest.main()
