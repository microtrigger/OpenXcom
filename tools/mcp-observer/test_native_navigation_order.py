"""Cross-language read validation using actual native provider payload output.

Only the transport envelope is synthetic; native state/visible/cursor/actions
are consumed without reordering. No gameplay or persistence claim.
"""
import copy,json,os,pathlib,unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import CHALLENGE,EPOCH,CAPABILITY

class NativeNavigationOrderTests(unittest.TestCase):
    def records(self):
        supplied=os.environ.get('NATIVE_NAVIGATION_PAYLOADS')
        if supplied is None:
            self.skipTest('Native integration artifact unavailable; set NATIVE_NAVIGATION_PAYLOADS explicitly')
        path=pathlib.Path(supplied)
        records=json.loads(path.read_text())
        self.assertIs(type(records),list)
        self.assertGreaterEqual(len(records),2)
        return records
    def envelope(self,payload):
        obj=dict(schema=a.SCHEMA,mode=a.MODE,type='state',challenge=CHALLENGE,
                 epoch=EPOCH,revision=1,capability=CAPABILITY,event_sequence=1,
                 semantics='presented_after_request')
        self.assertEqual(set(payload),{'state','visible','cursor','actions'})
        obj.update(copy.deepcopy(payload))
        return json.loads(json.dumps(obj))
    def test_actual_native_modes_validate_without_reordering(self):
        modes=set()
        for payload in self.records():
            obj=self.envelope(payload)
            self.assertEqual(obj['state'],'geoscape_craft_navigation')
            redirect=obj['visible']['redirect'] is not None
            modes.add(redirect)
            self.assertEqual([b['id'] for b in obj['visible']['buttons']],
                ['base','patrol','target','last_known'] if redirect else ['target','cancel'])
            self.assertIs(a.validate_state(obj,CHALLENGE,EPOCH),obj)
        self.assertEqual(modes,{False,True})
    def test_wrong_order_and_mode_still_rejected(self):
        for payload in self.records():
            obj=self.envelope(payload)
            bad=copy.deepcopy(obj);bad['visible']['buttons'].reverse()
            with self.assertRaises(Unavailable):a.validate_state(bad,CHALLENGE,EPOCH)
            if obj['visible']['redirect'] is not None:
                bad=copy.deepcopy(obj);bad['visible']['redirect']=None
                with self.assertRaises(Unavailable):a.validate_state(bad,CHALLENGE,EPOCH)
