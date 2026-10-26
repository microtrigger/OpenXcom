# SPDX-License-Identifier: GPL-3.0-or-later
import copy,unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,arguments,CHALLENGE,EPOCH

def failure():
 obj=state(screen='monthly_report');obj['visible']['failure']='Council terminated project'
 obj['visible']['buttons']=[{'id':'failure_ok','label':'OK'}]
 obj['actions']=[dict(id='acknowledge_monthly_failure',label='OK',label_source='visible_text',argument_kind='none')]
 return obj
class TerminalTests(unittest.TestCase):
 def check(self,obj):return a.validate_state(obj,CHALLENGE,EPOCH)
 def test_valid_modes(self):
  for o in (failure(),state(screen='slideshow'),state(screen='statistics')):self.check(o)
 def test_no_arguments(self):
  for action in ('acknowledge_monthly_failure','advance_slideshow','close_statistics'):
   a.validate_action_arguments(arguments(action=action))
   for args in ({'index':0},{'skip':True},{'count':2},{'save':True}):
    with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=action,args=args))
 def test_no_hidden_details(self):
  for obj in (failure(),state(screen='slideshow'),state(screen='statistics')):
   for field in ('slide_count','slide_index','path','ending','score','stats','future_caption'):
    bad=copy.deepcopy(obj);bad['visible'][field]='private'
    with self.assertRaises(Unavailable):self.check(bad)
 def test_null_caption_title(self):
  for screen,field in [('slideshow','caption'),('statistics','title')]:
   obj=state(screen=screen);obj['visible'][field]=None;self.check(obj)
 def test_button_authority(self):
  for obj in (failure(),state(screen='slideshow'),state(screen='statistics')):
   obj['visible']['buttons']=[]
   with self.assertRaises(Unavailable):self.check(obj)
 def test_failure_not_normal(self):
  obj=failure();obj['actions']+=state(screen='monthly_report')['actions'];obj['visible']['buttons']+=[{'id':'ok','label':'Ok'}]
  with self.assertRaises(Unavailable):self.check(obj)
 def test_cross_state_denial(self):
  objects=[failure(),state(screen='slideshow'),state(screen='statistics')]
  for obj in objects:
   for other in objects:
    if obj['state']==other['state']:continue
    bad=copy.deepcopy(obj);bad['actions']=other['actions']
    with self.assertRaises(Unavailable):self.check(bad)
if __name__=='__main__':unittest.main()
