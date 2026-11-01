# SPDX-License-Identifier: GPL-3.0-or-later
import copy,unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,arguments
class UnitInfoTests(unittest.TestCase):
 def check(self,x): return a.validate_state(x,x['challenge'],x['epoch'])
 def test_visible(self): self.check(state(screen='unit_info'))
 def test_hidden_psi(self):
  x=state(screen='unit_info')
  for r in x['visible']['rows']:
   if r['id'].startswith('psi_'):r.update(label=None,value=None,bar=None)
  self.check(x)
  # Independent label occlusion must not erase another visible widget.
  x['visible']['rows'][0]['label']=None;self.check(x)
 def test_exact_rows(self):
  for operation in ('missing','duplicate','hidden'):
   x=state(screen='unit_info');r=x['visible']['rows']
   if operation=='missing':r.pop()
   elif operation=='duplicate':r[1]=copy.deepcopy(r[0])
   else:r[0]['raw_stats']={}
   with self.assertRaises(Unavailable):self.check(x)
 def test_bar_bounds(self):
  for key,value in [('x',True),('x',320),('y',200),('width',171),('height',181)]:
   x=state(screen='unit_info');x['visible']['rows'][0]['bar'][key]=value
   with self.subTest(key=key,value=value),self.assertRaises(Unavailable):self.check(x)
 def test_run_bounds_and_hidden(self):
  for key,value in [('x',True),('width',151),('y',5),('color',0),('color',256),('color',False),('secret',42)]:
   x=state(screen='unit_info');x['visible']['rows'][0]['bar']['runs'][0][key]=value
   with self.subTest(key=key,value=value),self.assertRaises(Unavailable):self.check(x)
 def test_overlapping_runs(self):
  x=state(screen='unit_info');x['visible']['rows'][0]['bar']['runs'].append(dict(x=20,y=0,width=2,color=5))
  with self.assertRaises(Unavailable):self.check(x)
 def test_button_authority(self):
  x=state(screen='unit_info');x['visible']['buttons']=[]
  with self.assertRaises(Unavailable):self.check(x)
  x['actions']=[d for d in x['actions'] if d['id']=='close_unit_info'];self.check(x)
 def test_action_label(self):
  x=state(screen='unit_info');x['actions'][0]['label']='Enemy'
  with self.assertRaises(Unavailable):self.check(x)
if __name__=='__main__':unittest.main()
