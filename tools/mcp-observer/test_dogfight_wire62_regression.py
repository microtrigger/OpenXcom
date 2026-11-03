# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent explicit-wire routing/negative guards. Synthetic socket only."""
import copy,unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture,FixtureFailure,arguments,state
from test_agent_multi_dogfight import MultiFixture,multi
class Wire62Regression(unittest.TestCase):
 def test_each_window_exact_payload_and_single_dispatch(self):
  for w in a.DOGFIGHT_WINDOWS:
   with self.subTest(window=w):
    peer=MultiFixture(multi(4));self.addCleanup(peer.close);fresh=peer.client.observe()
    cmd=arguments(fresh,action='choose_dogfight_mode',args={'window':w,'mode':'cautious'})
    receipt=peer.client.execute(**cmd);self.assertEqual(receipt['status'],'dispatched');self.assertEqual(peer.client.execute(**cmd),receipt)
    sent=[p for p in peer.requests if p.startswith(b'E ')];self.assertEqual(len(sent),1);self.assertEqual(sent[0].split(b' ')[-3:],[b'choose_dogfight_mode',w.encode(),b'cautious'])
 def test_modes_remain_bound_to_distinct_windows(self):
  template=multi(2);template['visible']['windows'][0]['buttons']=[dict(id='cautious',selected=True)];template['visible']['windows'][1]['buttons']=[dict(id='aggressive',selected=True)]
  for window,wrong,right in [('w0','aggressive','cautious'),('w1','cautious','aggressive')]:
   with self.subTest(window=window):
    peer=MultiFixture(template);self.addCleanup(peer.close);fresh=peer.client.observe()
    with self.assertRaises(Unavailable):peer.client.execute(**arguments(fresh,action='choose_dogfight_mode',args={'window':window,'mode':wrong}))
    self.assertFalse(any(p.startswith(b'E ') for p in peer.requests));self.assertEqual(peer.client.capability,fresh['capability'])
    self.assertEqual(peer.client.execute(**arguments(fresh,action='choose_dogfight_mode',args={'window':window,'mode':right}))['status'],'dispatched')
 def test_same_handle_replacement_rejects_old_grant_and_accepts_fresh(self):
  peer=MultiFixture(multi(2));self.addCleanup(peer.close);old=peer.client.observe()
  peer.revision+=1;peer.template['visible']['windows'][1]['buttons']=[dict(id='disengage',selected=False)];fresh=peer.client.observe()
  with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action='choose_dogfight_mode',args={'window':'w1','mode':'disengage'}))
  self.assertFalse(any(p.startswith(b'E ')for p in peer.requests));self.assertEqual(peer.client.capability,fresh['capability'])
  self.assertEqual(peer.client.execute(**arguments(fresh,action='choose_dogfight_mode',args={'window':'w1','mode':'disengage'}))['status'],'dispatched')
 def test_fixture_rejects_legacy_missing_and_extra_arity(self):
  for variant in ('missing','extra'):
   with self.subTest(variant=variant):
    peer=NativeFixture(screen='dogfight');fresh=peer.client.observe();wire=a.canonical_action_request(arguments(fresh,action='choose_dogfight_mode',args={'window':'w0','mode':'standoff'})).split(b' ')
    if variant=='missing':wire.pop(-2)
    else:wire.append(b'extra')
    peer.client.channel.send(b' '.join(wire));peer.thread.join(2);self.assertFalse(peer.thread.is_alive())
    with self.assertRaisesRegex(FixtureFailure,'verification_arity'):peer.close()
 def test_default_dogfight_arguments_are_explicit(self):
  self.assertEqual(arguments(state(screen='dogfight'),action='choose_dogfight_mode')['args'],{'window':'w0','mode':'standoff'})
if __name__=='__main__':unittest.main()
