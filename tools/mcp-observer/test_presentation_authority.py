"""Read-only presentations do not carry UI action authority; explicit fixture integration."""
import copy,importlib.util,json,os,unittest
from pathlib import Path
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state,dump,CHALLENGE,EPOCH
from test_agent_events import page

class PresentationAuthorityTests(unittest.TestCase):
 def test_saved_native_projection_and_strict_ui_authority(self):
  fixture=os.environ.get('PRESENTATION_PUBLIC_FIXTURE');baseline=os.environ.get('PRESENTATION_BASELINE')
  if not fixture or not baseline:self.skipTest('explicit saved-public/native projection and baseline source required')
  saved=json.loads(Path(fixture).read_text());spec=importlib.util.spec_from_file_location('old_presentation_validator',Path(baseline)/'agent_control.py');old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
  cases=[(saved['state'],saved['visible'],{x['id'] for x in saved['actions']}),('psi_training',state(screen='psi_training')['visible'],{'open_psi_training_base'})]
  for screen,visible,actions in cases:
   event=dict(seq=1,kind='presentation',state=screen,visible=copy.deepcopy(visible));obj=page([event])
   with self.subTest(screen=screen):
    with self.assertRaises(Unavailable):old.validate_events(dump(obj),CHALLENGE,EPOCH,0,16,'mixed_output')
    result=a.validate_events(dump(obj),CHALLENGE,EPOCH,0,16,'mixed_output');self.assertEqual(result['events'][0]['visible'],visible)
    # Ordinary UI validation retains exact action-presence equivalence.
    a.validate_visible(screen,visible,actions)
    live=state(screen=screen);live['visible']=copy.deepcopy(visible)
    if screen==saved['state']:live['actions']=copy.deepcopy(saved['actions'])
    a.validate_state(live,CHALLENGE,EPOCH)
    denied=copy.deepcopy(live);denied['actions']=[];denied['capability']=None
    with self.assertRaises(Unavailable):a.validate_state(denied,CHALLENGE,EPOCH)
    with self.assertRaises(Unavailable):a.validate_visible(screen,visible,frozenset())
    bad=copy.deepcopy(obj)
    if screen=='craft_destination_selection':bad['events'][0]['visible']['targets'][0]['width']=0
    else:bad['events'][0]['visible']['rows'][0]['handle']='r1'
    with self.assertRaises(Unavailable):a.validate_events(dump(bad),CHALLENGE,EPOCH,0,16,'mixed_output')
    bad=copy.deepcopy(obj);bad['events'][0]['visible']['hidden_actor']=7
    with self.assertRaises(Unavailable):a.validate_events(dump(bad),CHALLENGE,EPOCH,0,16,'mixed_output')
