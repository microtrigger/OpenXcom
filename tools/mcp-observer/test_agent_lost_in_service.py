# SPDX-License-Identifier: GPL-3.0-or-later
"""Current presented report rows, no model query or guaranteed successor."""
import copy,json,unittest
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture,state,arguments,CHALLENGE,EPOCH
ACTION='acknowledge_lost_in_service'
class LostInServiceTests(unittest.TestCase):
    def checked(self,obj):return a.validate_state(json.loads(json.dumps(obj)),CHALLENGE,EPOCH)
    def test_exact_nullable_title_only_catalog(self):
        self.assertEqual((len(a.ACTIONS),len(a.STATE_ACTIONS)),(198,85))
        self.assertEqual(a.STATE_ACTIONS['lost_in_service'],{ACTION})
        self.assertEqual(a.ARGUMENT_KINDS[ACTION],'none')
        for title in (None,'','Lost in Service','é'*512):
            obj=state(screen='lost_in_service');obj['visible']['title']=title;self.checked(obj)
        for key in ('soldiers','kills','rank','diary','total','selected','success'):
            obj=state(screen='lost_in_service');obj['visible'][key]=[]
            with self.assertRaises(Unavailable):self.checked(obj)
        for title in (True,1,{},'x'*1025):
            obj=state(screen='lost_in_service');obj['visible']['title']=title
            with self.assertRaises(Unavailable):self.checked(obj)
    def test_exact_button_empty_args_and_neighbour_refusal(self):
        obj=state(screen='lost_in_service');self.checked(obj)
        for buttons in ([],[dict(id='ok',label='Wrong')],[dict(id='cancel',label='Ok')],[dict(id='ok',label=None)]):
            bad=copy.deepcopy(obj);bad['visible']['buttons']=buttons
            with self.assertRaises(Unavailable):self.checked(bad)
        readonly=copy.deepcopy(obj);readonly.update(actions=[],capability=None);readonly['visible']['buttons']=[];self.checked(readonly)
        a.validate_action_arguments(arguments(action=ACTION,args={}))
        for args in ({'confirm':True},{'row':'r0'},{'x':1}):
            with self.assertRaises(Unavailable):a.validate_action_arguments(arguments(action=ACTION,args=args))
        for screen in ('promotions','commendations','geoscape'):
            bad=state(screen=screen);bad['actions']=obj['actions']
            with self.assertRaises(Unavailable):self.checked(bad)
    def test_stale_grant_zero_send(self):
        peer=NativeFixture(screen='lost_in_service')
        try:
            old=peer.client.observe();peer.client.observe()
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(old,action=ACTION))
            self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
        finally:peer.close()
    def test_one_dispatch_duplicate_query_no_successor_assumption(self):
        peer=NativeFixture(screen='lost_in_service',transitions={('lost_in_service',ACTION):'unsupported'})
        try:
            request=arguments(peer.client.observe(),action=ACTION);receipt=peer.client.execute(**request)
            self.assertEqual(receipt['status'],'dispatched');self.assertEqual(peer.client.execute(**request),receipt)
            after=peer.client.observe();self.assertEqual(after['state'],'unsupported')
            with self.assertRaises(Unavailable):peer.client.execute(**arguments(after,action=ACTION,command_id='f'*32))
            self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)
        finally:peer.close()
    def test_unknown_receipt_closes_no_replay(self):
        peer=NativeFixture(screen='lost_in_service',transform=lambda op,obj:b'{"private":"DO_NOT_EXPORT"}' if op=='E' else obj)
        try:
            request=arguments(peer.client.observe(),action=ACTION)
            with self.assertRaises(a.ActionOutcomeUnknown) as caught:peer.client.execute(**request)
            self.assertNotIn('DO_NOT_EXPORT',str(caught.exception.public_error()));self.assertEqual(peer.client.state,'CLOSED')
            with self.assertRaises(Unavailable):peer.client.execute(**request)
            self.assertEqual(sum(p.startswith(b'E ') for p in peer.requests),1)
        finally:peer.close()
