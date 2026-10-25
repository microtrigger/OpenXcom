"""Synthetic closed playback event contract; not native playback/audibility proof."""
import copy,json,unittest
import agent_control as a
import adapter
from snapshots import Unavailable
from test_agent_events import page
from test_agent_control import EPOCH,CHALLENGE,dump

def issued(seq=1,epoch=1,sound_sequence=None,sound_id='1'*32):
 return dict(seq=seq,kind='sound_play_issued',playback_epoch=epoch,sound_sequence=seq if sound_sequence is None else sound_sequence,monotonic_ms=100,sound_id=sound_id,channel_gain_128=128,chunk_gain_128=64,loop_start=False,position=None,camera=None,semantics='accepted_positive_gain_playback')
def playback_page(events,after=0,newest=None,epoch=1,status='active'):
 value=page(events,after=after,newest=newest,history='unavailable');value['sound_playback']=dict(playback_epoch=epoch,status=status);return value

def checked(value):return a.validate_events(dump(value),CHALLENGE,EPOCH,value['after'],16,value['audio_history'])

class SoundPlaybackTests(unittest.TestCase):
 def test_closed_wire_and_actual_server(self):
  event=issued();event['camera']=dict(viewport=dict(x=0,y=0,width=320,height=140),frame=2,revision=3,age_ms=251,stale=True)
  event['position']=dict(angle_degrees=359,distance_effect=255)
  original=playback_page([event]);self.assertEqual(checked(original),original)
  class Peer:
   def read_events(self,epoch,after,limit):return checked(original)
  server=adapter.Server.__new__(adapter.Server);server.agent_control=Peer()
  result=server.agent('read_public_events',dict(epoch=EPOCH,after=0,limit=16))
  self.assertFalse(result['isError']);self.assertEqual(json.loads(result['content'][0]['text']),original)
  self.assertEqual(result['structuredContent']['events'][0]['sound_id'],'1'*32)
  for key,value in [('filename','secret.wav'),('sound_id','laserrifle'),('sound_sequence',True),('channel_gain_128',0),('chunk_gain_128',129),('loop_start',1),('semantics','enemy_shot')]:
   bad=copy.deepcopy(original);bad['events'][0][key]=value
   with self.subTest(key=key),self.assertRaises(Unavailable):checked(bad)
  for change in [lambda e:e['position'].update(world_x=4),lambda e:e['camera'].update(actor=7),lambda e:e['camera'].update(stale=False)]:
   bad=copy.deepcopy(original);change(bad['events'][0])
   with self.assertRaises(Unavailable):checked(bad)
  for event in [dict(seq=1,kind='sound_playback_status',playback_epoch=1,status='unavailable',reason='controller_disabled',discarded_pending=100),dict(seq=1,kind='sound_playback_gap',playback_epoch=1,dropped_events=20)]:checked(playback_page([event],status='unavailable'))
  bad=copy.deepcopy(original);del bad['sound_playback']
  with self.assertRaises(Unavailable):checked(bad)
  # Backward PCM fixture remains valid only with explicit inactive playback metadata.
  from test_public_audio import page as pcm_page,event as pcm_event
  checked(pcm_page([pcm_event(1)]))
