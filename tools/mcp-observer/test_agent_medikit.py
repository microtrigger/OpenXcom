# SPDX-License-Identifier: GPL-3.0-or-later
import unittest,copy
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,arguments,NativeFixture
class MedikitTests(unittest.TestCase):
 def check(self,x):return a.validate_state(x,x['challenge'],x['epoch'])
 def test_visible(self):self.check(state(screen='medikit'))
 def test_nullable(self):
  x=state(screen='medikit');x['visible'].update(selected_part=None,selected_wounds=None,body=None);x['visible']['charges']={k:None for k in ('heal','stimulant','painkiller')};x['actions']=[d for d in x['actions'] if d['id']!='select_medikit_part'];self.check(x)
 def test_no_hidden_fields(self):
  for k in ('target_id','health','stun','wounds','faction'):
   x=state(screen='medikit');x['visible'][k]=3
   with self.subTest(k=k),self.assertRaises(Unavailable):self.check(x)
 def test_quantity_text_only(self):
  x=state(screen='medikit');x['visible']['charges']['heal']=10
  with self.assertRaises(Unavailable):self.check(x)
 def test_body_unknown_cannot_select(self):
  x=state(screen='medikit');x['visible']['body']=None
  with self.assertRaises(Unavailable):self.check(x)
 def test_target_must_be_on_painted_run(self):
  for target in (dict(x=20,y=20),dict(x=True,y=30),dict(x=320,y=30)):
   x=state(screen='medikit');x['visible']['body']['parts'][0]['target']=target
   with self.subTest(target=target),self.assertRaises(Unavailable):self.check(x)
 def test_duplicate_parts(self):
  x=state(screen='medikit');x['visible']['body']['parts']*=2
  with self.assertRaises(Unavailable):self.check(x)
 def test_runs_no_hidden_or_overlap(self):
  for key,value in [('color',0),('width',500),('secret',True)]:
   x=state(screen='medikit');x['visible']['body']['runs'][0][key]=value
   with self.subTest(key=key),self.assertRaises(Unavailable):self.check(x)
  x=state(screen='medikit');x['visible']['body']['runs']*=2
  with self.assertRaises(Unavailable):self.check(x)
 def test_part_unknown_count_rejected(self):
  x=state(screen='medikit');x['visible']['body']['parts'][0]['wounds']=2
  with self.assertRaises(Unavailable):self.check(x)
 def test_labels(self):
  x=state(screen='medikit');x['actions'][0]['label']='Hidden wounded target'
  with self.assertRaises(Unavailable):self.check(x)
 def test_canonical(self):
  self.assertTrue(a.canonical_action_request(arguments(action='select_medikit_part',args={'row':'r0'})).endswith(b'select_medikit_part r0'))
 def test_fresh_authority(self):
  p=NativeFixture(screen='medikit');self.addCleanup(p.close);old=p.client.observe();fresh=p.client.observe()
  with self.assertRaises(Unavailable):p.client.execute(**arguments(old,action='select_medikit_part',args={'row':'r0'}))
  with self.assertRaises(Unavailable):p.client.execute(**arguments(fresh,action='select_medikit_part',args={'row':'r1'}))
  self.assertEqual(p.client.execute(**arguments(fresh,action='select_medikit_part',args={'row':'r0'}))['status'],'dispatched')
if __name__=='__main__':unittest.main()
