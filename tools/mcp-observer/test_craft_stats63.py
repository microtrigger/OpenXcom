"""Wire63 synthetic DTO/socket coverage, not native paint/live acceptance."""
import copy, unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, NativeFixture, EPOCH, CHALLENGE, dump

def shown(lines):
 q=state(screen='ufopaedia_article');q['visible'].update(craft_stats=lines,details_status='partial' if lines is not None else 'unavailable');return q
class CraftStats63(unittest.TestCase):
 def valid(self,q):return a.validate_state(q,q['challenge'],q['epoch'])
 def invalid(self,q):
  with self.assertRaises(Unavailable):self.valid(q)
 def test_version_and_unchanged_actions(self):
  self.assertEqual(a.SCHEMA,'openxcom-agent-control/66');self.assertEqual(len(a.ACTIONS),286);self.assertEqual(len(a.STATE_ACTIONS),109)
 def test_exact_printed_lines_and_blank(self):
  for lines in (None,['MAXIMUM SPEED>\x012 400','', 'ACCELERATION>\x018'],[''],['a\rb']):self.valid(shown(lines))
 def test_types_no_partial_unknown_or_model_values(self):
  for lines in ([],{},'text',1,True,[None],[42],[True],[{}],[['a','b']]):self.invalid(shown(lines))
 def test_line_and_aggregate_budgets(self):
  self.valid(shown(['x'*256]*32));self.valid(shown(['é'*512]));self.valid(shown(['']*32))
  for lines in (['']*33,['x'*1025],['é'*513],['x'*257]*32):self.invalid(shown(lines))
 def test_embedded_linefeeds_rejected_not_normalized(self):
  for line in ('a\nb','\n','a\n') : self.invalid(shown([line]))
 def test_closed_shape(self):
  q=shown(None);del q['visible']['craft_stats'];self.invalid(q)
  for key in ('craft_rule','maximum_speed','hidden_stats'):
   q=shown(['speed']);q['visible'][key]=1;self.invalid(q)
 def test_status_independence(self):
  q=shown(['speed']);q['visible']['details_status']='unavailable';self.invalid(q)
  q=shown(None);q['visible']['details_status']='partial';self.invalid(q)
  q=shown(['speed']);q['visible']['article_stats']={'rows':[['A','B']],'scroll':{'up':False,'down':False}};self.valid(q)
 def test_no_craft_scroll_authority(self):
  q=shown(['speed']);q['actions'].append(dict(id='scroll_article_stats',label='Scroll article stats',label_source='visible_control_semantics',argument_kind='scroll_direction'));self.invalid(q)
 def test_prior_future_state(self):
  for version in (59,60,61,62,63,64,65,67):
   q=shown(['speed']);q['schema']=f'openxcom-agent-control/{version}';self.invalid(q)
 def test_prior_future_hello(self):
  for version in (62,63,64,65,67):
   q=dict(schema=f'openxcom-agent-control/{version}',mode=a.MODE,epoch=EPOCH,ledger_capacity=256,command_generation=1,audio_history='mixed_output')
   with self.assertRaises(Unavailable):a.validate_hello(dump(q))
 def test_prior_future_ready(self):
  for version in (62,63,64,65,67):
   with self.assertRaises(Unavailable):a.validate_ready(dump(dict(schema=f'openxcom-agent-control/{version}',mode=a.MODE,status='ready')))
 def test_native_fixture_old_hello_no_mutating_send(self):
  q=dict(schema='openxcom-agent-control/62',mode=a.MODE,epoch=EPOCH,ledger_capacity=256,command_generation=1,audio_history='mixed_output')
  peer=NativeFixture(hello=q);self.addCleanup(peer.close)
  with self.assertRaises(Unavailable):peer.client.observe()
  self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
if __name__=='__main__':unittest.main()
