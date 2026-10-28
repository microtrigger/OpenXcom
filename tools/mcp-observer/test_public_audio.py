from test_audio_clock_fixture import timing
import base64,copy,json,os,pathlib,struct,unittest
import agent_control as a
import adapter,public_audio as p
from snapshots import Unavailable
from test_agent_control import EPOCH,CHALLENGE

def event(seq,pcm_sequence=None):
 return dict(seq=seq,kind='audio_output',duration_ms=100,nonzero=True,source=None,sound=None,position=None,
             timing=timing(pcm_sequence or seq),pcm_sequence=pcm_sequence or seq,profile=p.PROFILE,pcm_hex='0100'+'0000'*4409)
def page(events,after=0,newest=None):
 newest=events[-1]['seq'] if newest is None else newest
 return dict(schema=a.SCHEMA,mode=a.MODE,type='events',challenge=CHALLENGE,epoch=EPOCH,audio_history='mixed_output',audio_status='active',sound_playback=dict(playback_epoch=0,status='inactive'),after=after,oldest=max(1,newest-255),newest=newest,next_cursor=events[-1]['seq'] if events else after,gap=after+1<max(1,newest-255),events=events)
def checked(value):return a.validate_events(json.dumps(value).encode(),CHALLENGE,EPOCH,value['after'],16,'mixed_output')
class Peer:
 def __init__(self,pages):self.pages=pages;self.calls=[]
 def read_events(self,epoch,after,limit):
  self.calls.append((epoch,after,limit));obj=self.pages.pop(0)
  if obj['after']!=after:raise AssertionError('cursor mismatch')
  return checked(obj)

class PublicAudioTests(unittest.TestCase):
 def test_actual_native_event_and_mcp_audio_content(self):
  path=os.environ.get('NATIVE_PCM_EVENT_LOG')
  if path is None:self.skipTest('Set NATIVE_PCM_EVENT_LOG for actual native integration')
  lines=pathlib.Path(path).read_text().splitlines();raw=json.loads(next(x[6:] for x in lines if x.startswith('EVENT ')))
  checked(page([raw]));peer=Peer([page([raw])]);server=adapter.Server.__new__(adapter.Server);server.agent_control=peer
  result=server.agent('read_public_audio',dict(epoch=EPOCH,after=0,limit=8))
  self.assertFalse(result['isError']);self.assertEqual(result['content'][1]['mimeType'],'audio/wav')
  wav=base64.b64decode(result['content'][1]['data'],validate=True)
  self.assertEqual(wav[44:],bytes.fromhex(raw['pcm_hex']));self.assertEqual(wav[:4],b'RIFF');self.assertEqual(struct.unpack_from('<I',wav,24)[0],22050)
  self.assertNotIn('pcm_hex',result['content'][0]['text'])
  server.agent_control=Peer([page([raw])]);metadata=server.agent('read_public_events',dict(epoch=EPOCH,after=0,limit=16))
  self.assertNotIn('pcm_hex',json.dumps(metadata));self.assertEqual(metadata['structuredContent']['events'][0]['pcm_sequence'],1)
 def test_contiguous_gap_status_cutoff_and_deferred(self):
  status=dict(seq=3,kind='audio_status',status='unavailable',partial_window_discarded=True)
  peer=Peer([page([event(1),event(2)],newest=5),page([status,event(4,4),event(5,5)],after=2,newest=5)])
  result=p.read_audio(peer,EPOCH,0,8);obj=result['structuredContent']
  self.assertTrue(obj['gap']);self.assertEqual(len(obj['segments']),2);self.assertEqual([s['frames'] for s in obj['segments']],[4410,4410]);self.assertEqual(obj['next_cursor'],5)
  peer=Peer([page([event(1),event(2)],newest=2)])
  obj=p.read_audio(peer,EPOCH,0,1)['structuredContent'];self.assertTrue(obj['deferred']);self.assertEqual(obj['next_cursor'],1)
  peer=Peer([page([event(1)],newest=2),page([event(2),event(3)],after=1,newest=3)])
  obj=p.read_audio(peer,EPOCH,0,8)['structuredContent'];self.assertEqual(obj['cutoff'],2);self.assertEqual(obj['next_cursor'],2);self.assertEqual(obj['windows'],2)
 def test_closed_pcm_shape_and_nonzero_consistency(self):
  for key,value in [('pcm_hex','bad'),('profile','device-44100'),('pcm_sequence',True),('nonzero',False),('source','enemy')]:
   e=event(1);e[key]=value
   with self.subTest(key=key),self.assertRaises(Unavailable):checked(page([e]))
  e=event(1);del e['pcm_hex']
  with self.assertRaises(Unavailable):checked(page([e]))
  with self.assertRaises(Unavailable):checked(page([event(1,2),event(2,1)]))
 def test_callback_clocks_strict_and_preserved(self):
  e=event(1)
  obj=p.read_audio(Peer([page([e])]),EPOCH,0,8)['structuredContent']
  self.assertEqual(obj['schema'],'openxcom-public-audio/2')
  self.assertEqual(obj['temporal_alignment'],'postmix_callback_clock_not_speaker_time')
  self.assertEqual(obj['events'][0]['timing'],e['timing'])
  for key,value in [('clock','receipt_time'),('first_realtime_us',0),('first_monotonic_us',True),
                    ('last_realtime_us',1),('last_monotonic_us',1),('first_frame_offset',2205),
                    ('last_frame_end_offset',0),('last_callback_frames',65537)]:
   bad=event(1);bad['timing'][key]=value
   with self.subTest(key=key),self.assertRaises(Unavailable):checked(page([bad]))
  for key in tuple(e['timing']):
   bad=event(1);del bad['timing'][key]
   with self.subTest(missing=key),self.assertRaises(Unavailable):checked(page([bad]))
  bad=event(1);del bad['timing']
  with self.assertRaises(Unavailable):checked(page([bad]))
 def test_callback_clock_order_within_and_across_pages(self):
  first=event(1);second=event(2);second['timing']=timing(0)
  with self.assertRaises(Unavailable):checked(page([first,second]))
  with self.assertRaises(Unavailable):
   p.read_audio(Peer([page([first],newest=2),page([second],after=1,newest=2)]),EPOCH,0,8)
  # Multiple windows from the same actual callback may share its clocks.
  second['timing']=dict(first['timing'])
  checked(page([first,second]))
 def test_page_and_response_bounds_no_extra_reads(self):
  pages=[page([dict(seq=i,kind='displayed_message',text='X',report=None,source=None,position=None)],after=i-1,newest=20) for i in range(1,9)]
  peer=Peer(pages);result=p.read_audio(peer,EPOCH,0,8);self.assertEqual(len(peer.calls),8);self.assertTrue(result['structuredContent']['deferred']);self.assertEqual(result['structuredContent']['windows'],0)
  for limit in (0,9,True):
   with self.assertRaises(Unavailable):p.validate_arguments(dict(epoch=EPOCH,after=0,limit=limit))
  names={x['name'] for x in a.definitions()};self.assertIn('read_public_audio',names)
