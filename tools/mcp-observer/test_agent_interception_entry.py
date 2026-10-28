"""Current visible Geo INTERCEPT entry; no target/craft eligibility inference."""
import copy,json,unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture,state,arguments,CHALLENGE,EPOCH
ACTION='open_interception'
def shown(obj):
 if obj.get('type')=='state' and obj.get('state')=='geoscape' and not any(x['id']==ACTION for x in obj['actions']):
  obj['actions'].append(dict(id=ACTION,label=next(x['label'] for x in obj['visible']['buttons'] if x['id']=='intercept'),label_source='visible_text',argument_kind='none'))
 return obj
class InterceptionEntryTests(unittest.TestCase):
 def test_catalog_and_exact_button_argument_contract(self):
  self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(199,85));obj=shown(state(screen='geoscape'));a.validate_state(json.loads(json.dumps(obj)),CHALLENGE,EPOCH)
  for change in ('missing','label','neighbour'):
   bad=copy.deepcopy(obj)
   if change=='missing':bad['visible']['buttons']=[x for x in bad['visible']['buttons'] if x['id']!='intercept']
   elif change=='label':bad['actions'][-1]['label']='Wrong'
   else:bad['state']='funding'
   with self.assertRaises(Unavailable):a.validate_state(bad,CHALLENGE,EPOCH)
  for args in ({'target':'ufo'},{'row':'r0'},{'x':0}):
   with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=ACTION,args=args))
  a.validate_action_arguments(arguments(action=ACTION,args={}))
 def test_current_button_one_dispatch_and_stale_zero_send(self):
  peer=NativeFixture(screen='geoscape',transform=lambda op,obj:shown(obj) if op=='R' else obj,transitions={('geoscape',ACTION):'intercept_craft'})
  try:
   old=peer.client.observe();fresh=peer.client.observe()
   with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action=ACTION))
   self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
   request=arguments(fresh,action=ACTION);result=peer.client.execute(**request);self.assertEqual(result['status'],'dispatched');self.assertEqual(peer.client.execute(**request),result)
   self.assertEqual(peer.client.observe()['state'],'intercept_craft');self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)
  finally:peer.close()
 def test_unknown_still_closes_no_replay(self):
  peer=NativeFixture(screen='geoscape',transform=lambda op,obj:shown(obj) if op=='R' else b'{}' if op=='E' else obj)
  try:
   request=arguments(peer.client.observe(),action=ACTION)
   with self.assertRaises(a.ActionOutcomeUnknown):peer.client.execute(**request)
   with self.assertRaises(Unavailable):peer.client.execute(**request)
   self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)
  finally:peer.close()
