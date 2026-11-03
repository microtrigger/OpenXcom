"""Wire64 synthetic contract checks; not native paint or gameplay."""
import copy
import unittest
import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, arguments, NativeFixture, CHALLENGE, EPOCH

NAMES = ('focus_craft_name', 'replace_craft_name', 'finish_craft_name')
def editing(obj=None):
    obj = state(screen='craft_info') if obj is None else obj
    obj['visible']['buttons'] = []
    obj['actions'] = [dict(id=k,label=a.OUTFITTING_SEMANTIC_LABELS[k],label_source='visible_control_semantics',argument_kind=a.ARGUMENT_KINDS[k]) for k in NAMES[1:]]
    return obj

def checked(obj): return a.validate_state(obj, CHALLENGE, EPOCH)

class EditingFixture(NativeFixture):
    def observation(self, challenge, capability):
        return editing(super().observation(challenge, capability))

class CraftName64Tests(unittest.TestCase):
    def test_unfocused_and_editing(self):
        checked(state(screen='craft_info')); checked(editing())
    def test_empty_name_is_visible_not_unknown(self):
        obj=editing(); obj['visible']['name']=''; checked(obj)
    def test_hidden_name_never_authorizes(self):
        for obj in (editing(),state(screen='craft_info')):
            obj['visible']['name']=None
            with self.assertRaises(Unavailable): checked(obj)
    def test_replace_finish_atomic_pair(self):
        for i in range(2):
            obj=editing(); obj['actions'].pop(i)
            with self.assertRaises(Unavailable): checked(obj)
    def test_focus_cannot_coexist_editing(self):
        obj=editing(); obj['actions'] += [x for x in state(screen='craft_info')['actions'] if x['id']==NAMES[0]]
        with self.assertRaises(Unavailable): checked(obj)
    def test_no_navigation_during_editing(self):
        obj=editing(); base=state(screen='craft_info'); obj['visible']['buttons']=base['visible']['buttons']; obj['actions'] += [x for x in base['actions'] if x['id']!=NAMES[0]]
        with self.assertRaises(Unavailable): checked(obj)
    def test_exact_labels(self):
        for k in NAMES:
            obj=state(screen='craft_info') if k==NAMES[0] else editing()
            next(x for x in obj['actions'] if x['id']==k)['label']='Invented'
            with self.assertRaises(Unavailable): checked(obj)
    def test_old_wire_rejected(self):
        obj=editing(); obj['schema']='openxcom-agent-control/63'
        with self.assertRaises(Unavailable): checked(obj)
    def test_wrong_state_rejected(self):
        obj=state(screen='target_info'); obj['actions']+=editing()['actions']
        with self.assertRaises(Unavailable): checked(obj)
    def test_text_byte_boundaries_and_empty(self):
        for text in ('','Test','A/B','x'*128,'é'*64): a.validate_action_arguments(arguments(action=NAMES[1],args={'text':text}))
        for text in ('x'*129,'é'*65,'a\n','a\x00'):
            with self.assertRaises(Unavailable): a.validate_action_arguments(arguments(action=NAMES[1],args={'text':text}))
    def fixture(self, **kw):
        peer=NativeFixture(screen='craft_info',**kw); self.addCleanup(peer.close); return peer
    def test_each_action_routes_once(self):
        for k in NAMES:
            peer=NativeFixture(screen='craft_info') if k==NAMES[0] else EditingFixture(screen='craft_info'); self.addCleanup(peer.close)
            obs=peer.client.observe(); args=arguments(obs,action=k)
            receipt=peer.client.execute(**args); self.assertEqual(receipt['status'],'dispatched')
            self.assertEqual(peer.client.execute(**args),receipt)
            self.assertEqual(sum(x.startswith(b'E ') for x in peer.requests),1)
    def test_unoffered_and_stale_no_send(self):
        peer=self.fixture(); old=peer.client.observe(); fresh=peer.client.observe()
        for args in (arguments(old,action=NAMES[0]),arguments(fresh,action=NAMES[1])):
            with self.assertRaises(Unavailable): peer.client.execute(**args)
        self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))

if __name__=='__main__': unittest.main()
