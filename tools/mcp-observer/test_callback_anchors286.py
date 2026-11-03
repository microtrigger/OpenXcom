import copy,unittest
import agent_control as a
from test_public_audio import event,page,checked
from snapshots import Unavailable
class Anchors286Tests(unittest.TestCase):
 def setUp(self):
  self.e=event(1);t=self.e['timing'];t.update(first_frame_offset=100,first_callback_frames=1024,last_frame_end_offset=257,last_callback_frames=1024,last_monotonic_us=1200000,last_realtime_us=1200000,callback_anchors=[dict(offset_frames=-100,monotonic_us=1100000,realtime_us=1100000,frames=1024),dict(offset_frames=924,monotonic_us=1150000,realtime_us=1150000,frames=1024),dict(offset_frames=1948,monotonic_us=1200000,realtime_us=1200000,frames=1024)])
 def rejected(self):
  with self.assertRaises(Unavailable):checked(page([self.e]))
 def test_valid_crossing_all_anchors(self):self.assertEqual(checked(page([self.e]))['events'][0]['timing'],self.e['timing'])
 def test_missing_anchor(self):del self.e['timing']['callback_anchors'][1];self.rejected()
 def test_old_sparse_rejected(self):del self.e['timing']['callback_anchors'];self.rejected()
 def test_last_frame_disagreement(self):self.e['timing']['last_frame_end_offset']+=1;self.rejected()
 def test_clock_disagreement(self):self.e['timing']['callback_anchors'][1]['monotonic_us']=1100000;self.rejected()
 def test_max_integer(self):self.e['timing']['callback_anchors'][1]['realtime_us']=2**53;self.rejected()
 def test_boolean(self):self.e['timing']['callback_anchors'][1]['frames']=True;self.rejected()
 def test_over_capacity(self):self.e['timing']['callback_anchors']*=11;self.rejected()
 def test_unknown_extra_data(self):self.e['timing']['callback_anchors'][1]['state']='forbidden';self.rejected()
 def test_wrong_first_offset(self):self.e['timing']['callback_anchors'][0]['offset_frames']=0;self.rejected()
 def test_delay_retained_not_repaired(self):
  self.e['timing']['callback_anchors'][1]['monotonic_us']=1190000
  self.assertEqual(checked(page([self.e]))['events'][0]['timing']['callback_anchors'][1]['monotonic_us'],1190000)
 def test_shared_callback_across_pages(self):
  from test_public_audio import Peer,EPOCH
  import public_audio as p
  first=event(1);second=event(2)
  first['timing']['first_callback_frames']=first['timing']['last_callback_frames']=4410
  first['timing']['callback_anchors'][0]['frames']=4410
  second['timing']=copy.deepcopy(first['timing']);second['timing']['first_frame_offset']=2205;second['timing']['last_frame_end_offset']=4410;second['timing']['callback_anchors'][0]['offset_frames']=-2205
  result=p.read_audio(Peer([page([first],newest=2),page([second],after=1,newest=2)]),EPOCH,0,8)
  self.assertEqual(result['structuredContent']['windows'],2)
  # Each event alone valid, shared callback contradictory across pages.
  for key in ('first_monotonic_us','last_monotonic_us','first_realtime_us','last_realtime_us'):second['timing'][key]+=1
  second['timing']['callback_anchors'][0]['monotonic_us']+=1;second['timing']['callback_anchors'][0]['realtime_us']+=1
  with self.assertRaises(Unavailable):p.read_audio(Peer([page([first],newest=2),page([second],after=1,newest=2)]),EPOCH,0,8)
 def test_wrong_wire_rejected(self):
  import json
  x=page([self.e]);x['schema']='openxcom-agent-control/61'
  from test_public_audio import CHALLENGE,EPOCH
  with self.assertRaises(Unavailable):a.validate_events(json.dumps(x).encode(),CHALLENGE,EPOCH,0,16,'mixed_output')
if __name__=='__main__':unittest.main()
