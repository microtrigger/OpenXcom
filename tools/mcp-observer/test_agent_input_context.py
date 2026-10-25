"""Current public tactical input mode, never a promise of movement or firing."""
import copy,json,os,pathlib,unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,CHALLENGE,EPOCH

class InputContextTests(unittest.TestCase):
 def checked(self,context):
  obj=state();obj['visible']['input_context']=context
  return a.validate_state(json.loads(json.dumps(obj)),CHALLENGE,EPOCH)
 def test_exact_modes_and_old_shape_fail_closed(self):
  for cursor in ('normal','aim','throw','waypoint','psi','none','other'):
   self.checked(dict(mode='normal',selected_action='none',cursor=cursor))
   for action in ('snap','aimed','auto','throw','launch','psi_panic','psi_control','use','other'):
    self.checked(dict(mode='targeting',selected_action=action,cursor=cursor))
  for mode in ('busy','unavailable'):self.checked(dict(mode=mode,selected_action=None,cursor=None))
  obj=state();del obj['visible']['input_context']
  with self.assertRaises(Unavailable):a.validate_state(obj,CHALLENGE,EPOCH)
  bad=[dict(mode='normal',selected_action='auto',cursor='aim'),dict(mode='targeting',selected_action='none',cursor='aim'),dict(mode='targeting',selected_action='auto',cursor=None),dict(mode='busy',selected_action='auto',cursor='aim'),dict(mode='unavailable',selected_action=None,cursor='normal'),dict(mode=True,selected_action=None,cursor=None),dict(mode='targeting',selected_action='hostile',cursor='aim')]
  for context in bad:
   with self.subTest(context=context),self.assertRaises(Unavailable):self.checked(context)
  context=dict(mode='normal',selected_action='none',cursor='normal',weapon_id='hidden')
  with self.assertRaises(Unavailable):self.checked(context)
 def test_nonmap_never_exports_input_state(self):
  for mode in ('hidden_movement','unavailable'):
   a.validate_input_context(dict(mode='unavailable',selected_action=None,cursor=None),mode)
   for context in (dict(mode='normal',selected_action='none',cursor='normal'),dict(mode='targeting',selected_action='auto',cursor='aim'),dict(mode='busy',selected_action=None,cursor=None)):
    with self.assertRaises(Unavailable):a.validate_input_context(context,mode)
 def test_actual_native_serialized_input_context(self):
  path=os.environ.get('NATIVE_INPUT_CONTEXT_PAYLOADS')
  if path is None:self.skipTest('Set NATIVE_INPUT_CONTEXT_PAYLOADS to actual native fixture output')
  records=json.loads(pathlib.Path(path).read_text());self.assertIs(type(records),list);self.assertTrue(records)
  modes=set();actions=set()
  for context in records:
   modes.add(context['mode']);actions.add(context['selected_action']);self.checked(context)
  self.assertTrue({'normal','targeting','busy'}<=modes)
  self.assertIn('auto',actions)
