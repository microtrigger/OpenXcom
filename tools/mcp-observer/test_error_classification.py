# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent fault injection: public fixed categories, no automatic action replay."""
import json
import socket
import unittest
from unittest.mock import patch
import adapter
import agent_control as a
from snapshots import Unavailable
from test_agent_control import NativeFixture, arguments, call, COMMAND

class ErrorClassificationTests(unittest.TestCase):
    def fixture(self, **kw):
        n=NativeFixture(**kw);self.addCleanup(n.close)
        return n,adapter.Server(agent_control=n.client)
    def error(self, server, name='read_ui_state', args=None):
        reply=call(server,name,args)['result']
        self.assertTrue(reply['isError'])
        obj=reply['structuredContent']
        self.assertEqual(reply['content'],[{'type':'text','text':json.dumps(obj,separators=(',',':'))}])
        self.assertEqual(set(obj['failure']),{'code','category','severity','phase','action_outcome','recovery'})
        self.assertTrue(all(type(v) is str for v in obj['failure'].values()))
        self.assertNotIn('PRIVATE',json.dumps(obj))
        return obj
    def test_valid_unsupported_is_not_error(self):
        n,s=self.fixture(screen='unsupported')
        reply=call(s,'read_ui_state')['result'];self.assertFalse(reply['isError'])
        self.assertEqual(reply['structuredContent']['state'],'unsupported')
        self.assertNotIn('failure',reply['structuredContent'])
    def test_stale_authority_rejects_before_send(self):
        n,s=self.fixture();args=arguments(n.client.observe());args['capability']='f'*32
        before=len(n.requests);obj=self.error(s,'execute_ui_action',args)
        self.assertEqual(obj['error'],'unavailable')
        self.assertEqual(obj['failure']['code'],'stale_authority')
        self.assertEqual(obj['failure']['action_outcome'],'not_sent')
        self.assertEqual(obj['failure']['phase'],'pre_send');self.assertEqual(len(n.requests),before)
    def test_unoffered_action_rejects_before_send(self):
        n,s=self.fixture();args=arguments(n.client.observe(),action='continue_next_turn')
        obj=self.error(s,'execute_ui_action',args)
        self.assertEqual(obj['failure']['code'],'action_not_offered')
        self.assertEqual(obj['failure']['action_outcome'],'not_sent')
        self.assertFalse(any(r.startswith(b'E ') for r in n.requests))
    def test_read_native_unavailable_is_unspecified_and_closed(self):
        n,s=self.fixture(transform=lambda op,obj:dict(schema=a.SCHEMA,mode=a.MODE,type='error',status='unavailable') if op=='R' else obj)
        obj=self.error(s)
        self.assertEqual(obj['failure']['code'],'native_unavailable_unspecified')
        self.assertEqual(obj['failure']['action_outcome'],'not_applicable')
        self.assertEqual(n.client.state,'CLOSED')
    def test_read_timeout_is_not_unknown_action(self):
        n,s=self.fixture(transform=lambda op,obj:None if op=='R' else obj)
        obj=self.error(s);self.assertEqual(obj['failure']['code'],'read_recovery_required')
        self.assertEqual(obj['failure']['action_outcome'],'not_applicable')
        self.assertEqual(n.client.state,'READ_RECOVERY_REQUIRED');self.assertFalse(any(r.startswith(b'E ') for r in n.requests))
    def test_read_parse_and_validation_faults_are_distinct(self):
        for bad,code in [(b'{PRIVATE','parse_error'),(b'{"PRIVATE":"hidden"}','validation_error')]:
            with self.subTest(code=code):
                n,s=self.fixture(transform=lambda op,obj,bad=bad:bad if op=='R' else obj)
                obj=self.error(s);self.assertEqual(obj['failure']['code'],code)
                self.assertEqual(n.client.state,'CLOSED')
    def test_eof_is_transport_not_guessed_native_reason(self):
        parent,peer=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET)
        client=a.AgentControlClient(parent,timeout=.1,startup_timeout=.1);self.addCleanup(client.close);peer.close()
        obj=self.error(adapter.Server(agent_control=client))
        self.assertEqual(obj['failure']['code'],'transport_eof')
        self.assertNotIn('paint',json.dumps(obj))
    def test_unsolicited_before_send_is_not_sent_and_never_replayed(self):
        n,s=self.fixture();args=arguments(n.client.observe());n.peer.send(b'{}')
        obj=self.error(s,'execute_ui_action',args)
        self.assertEqual(obj['failure']['code'],'unexpected_packet')
        self.assertEqual(obj['failure']['action_outcome'],'not_sent')
        self.assertFalse(any(r.startswith(b'E ') for r in n.requests));self.assertEqual(n.client.state,'CLOSED')
    def test_sent_timeout_or_malformed_remains_unknown_critical(self):
        for bad in [None,b'{PRIVATE',b'{"PRIVATE":"hidden"}']:
            with self.subTest(bad=bad):
                n,s=self.fixture(transform=lambda op,obj,bad=bad:bad if op=='E' else obj)
                args=arguments(n.client.observe());obj=self.error(s,'execute_ui_action',args)
                self.assertEqual(obj['status'],'outcome_unknown');self.assertFalse(obj['retryable'])
                self.assertEqual(obj['command_id'],COMMAND)
                self.assertEqual(obj['semantics'],'not_a_native_receipt')
                self.assertEqual(obj['failure']['action_outcome'],'unknown')
                self.assertEqual(obj['failure']['severity'],'critical')
                self.assertEqual(obj['failure']['phase'],'send_or_receipt')
                self.assertEqual(len([r for r in n.requests if r.startswith(b'E ')]),1)
                count=len(n.requests);call(s,'execute_ui_action',args)
                self.assertEqual(len(n.requests),count);self.assertEqual(n.client.state,'CLOSED')
    def test_native_unavailable_receipt_is_not_unsent_claim(self):
        n,s=self.fixture(transform=lambda op,obj:dict(obj,status='unavailable',observation=None) if op=='E' else obj)
        args=arguments(n.client.observe());reply=call(s,'execute_ui_action',args)['result']
        self.assertFalse(reply['isError']);obj=reply['structuredContent']
        self.assertEqual(obj['status'],'unavailable');self.assertEqual(obj['failure']['code'],'native_result_unavailable')
        self.assertEqual(obj['failure']['action_outcome'],'unknown');self.assertEqual(obj['failure']['severity'],'critical')
        self.assertGreater(n.revision,1)  # Fixture changed UI revision before emitting refusal.
        self.assertNotIn('not_sent',json.dumps(obj));self.assertNotIn('no_mutation',json.dumps(obj))
        self.assertEqual(len([r for r in n.requests if r.startswith(b'E ')]),1)
    def test_native_unknown_receipt_retains_distinct_identity(self):
        n,s=self.fixture(transform=lambda op,obj:dict(obj,status='unknown_after_dispatch',observation=None) if op=='E' else obj)
        args=arguments(n.client.observe());reply=call(s,'execute_ui_action',args)['result']
        self.assertFalse(reply['isError']);self.assertEqual(reply['structuredContent']['status'],'unknown_after_dispatch')
        self.assertEqual(reply['structuredContent']['failure']['code'],'native_result_unknown_after_dispatch')
        self.assertEqual(reply['structuredContent']['failure']['action_outcome'],'unknown')
    def test_result_query_failure_no_second_action(self):
        n,s=self.fixture(transform=lambda op,obj:b'{PRIVATE' if op=='S' else obj)
        args=arguments(n.client.observe());n.client.execute(**args)
        obj=self.error(s,'read_ui_action_result',{'command_id':COMMAND})
        self.assertEqual(obj['failure']['phase'],'result_query')
        self.assertEqual(len([r for r in n.requests if r.startswith(b'E ')]),1)
    def test_generic_legacy_error_does_not_leak_exception(self):
        class OldClient:
            def observe(self):raise Unavailable('PRIVATE path or packet')
        obj=self.error(adapter.Server(agent_control=OldClient()))
        self.assertEqual(obj['error'],'unavailable')
        self.assertEqual(obj['failure']['code'],'legacy_unavailable')
    def test_invalid_schema_still_jsonrpc_rejection(self):
        class Never:
            def execute(self,**kw):raise AssertionError('called')
        reply=call(adapter.Server(agent_control=Never()),'execute_ui_action',{})
        self.assertEqual(reply,{'jsonrpc':'2.0','id':1,'error':{'code':-32602,'message':'Invalid params'}})

    def test_target_not_offered_is_pre_send(self):
        n,s=self.fixture(screen='new_game');args=arguments(n.client.observe(),action='select_difficulty',args={'value':'beginner'})
        n.client.difficulty_values=frozenset()
        obj=self.error(s,'execute_ui_action',args)
        self.assertEqual(obj['failure']['code'],'target_not_offered')
        self.assertEqual(obj['failure']['action_outcome'],'not_sent')
        self.assertFalse(any(r.startswith(b'E ') for r in n.requests))
    def test_command_conflict_does_not_replay(self):
        n,s=self.fixture();args=arguments(n.client.observe());n.client.execute(**args)
        obj=self.error(s,'execute_ui_action',dict(args,action='stand'))
        self.assertEqual(obj['failure']['code'],'command_conflict')
        self.assertEqual(len([r for r in n.requests if r.startswith(b'E ')]),1)
    def test_resource_limit_before_send_preserves_channel_for_explicit_retirement(self):
        n,s=self.fixture();args=arguments(n.client.observe())
        with patch.object(a,'MAX_COMMANDS',0):obj=self.error(s,'execute_ui_action',args)
        self.assertEqual(obj['failure']['code'],'resource_limit')
        self.assertEqual(obj['failure']['action_outcome'],'not_sent')
        self.assertEqual(n.client.state,'READY');self.assertIsNotNone(n.client.channel)
        self.assertEqual(n.client.commands,{})
        self.assertFalse(any(r.startswith(b'E ') or r.startswith(b'L ') for r in n.requests))
    def test_concurrent_request_does_not_close_active_client(self):
        n,s=self.fixture();n.client.observe();before=len(n.requests)
        n.client.flight.acquire()
        try:obj=self.error(s)
        finally:n.client.flight.release()
        self.assertEqual(obj['failure']['code'],'concurrent_request')
        self.assertEqual(len(n.requests),before);self.assertNotEqual(n.client.state,'CLOSED')
    def test_oversized_frame_is_classified_and_closed(self):
        n,s=self.fixture(transform=lambda op,obj:b'x'*(a.MAX_PACKET+1) if op=='R' else obj)
        obj=self.error(s);self.assertEqual(obj['failure']['code'],'transport_framing')
        self.assertEqual(n.client.state,'CLOSED')
    def test_os_error_payload_is_not_public(self):
        n,s=self.fixture();n.client.observe()
        with patch.object(n.client,'exchange',side_effect=OSError('PRIVATE /path and token')):obj=self.error(s)
        self.assertEqual(obj['failure']['code'],'transport_io');self.assertEqual(n.client.state,'CLOSED')
    def test_untyped_execute_error_must_not_claim_no_send(self):
        class LegacyMutator:
            mutations=0
            def execute(self,**args):
                self.mutations+=1
                raise Unavailable('PRIVATE after mutation')
        client=LegacyMutator();obj=self.error(adapter.Server(agent_control=client),'execute_ui_action',arguments())
        self.assertEqual(client.mutations,1)
        self.assertEqual(obj['failure']['action_outcome'],'unknown')
        self.assertEqual(obj['failure']['severity'],'critical');self.assertEqual(obj['failure']['recovery'],'reconcile')
    def test_closed_enum_helper_does_not_echo_unknown_values(self):
        from error_contract import CODES,detail
        for code in CODES:
            obj=detail(code);self.assertEqual(obj['code'],code)
            self.assertEqual(set(obj),{'code','category','severity','phase','action_outcome','recovery'})
        self.assertNotIn('PRIVATE',json.dumps(detail('PRIVATE','PRIVATE','PRIVATE')))

    def test_empty_seqpacket_is_framing_not_eof(self):
        n,s=self.fixture(transform=lambda op,obj:b'' if op=='R' else obj)
        obj=self.error(s)
        self.assertEqual(obj['failure']['code'],'transport_framing')
        self.assertEqual(n.client.state,'CLOSED')

    def test_unknown_native_query_receipt_is_critical_not_pending(self):
        from test_agent_control import result, dump, EPOCH
        for status in ('unknown','pending'):
            obj=result(COMMAND,status=status,observation=None)
            a.validate_result(dump(obj),COMMAND,EPOCH)
            class Query:
                def read_result(self,command_id):return obj
            reply=call(adapter.Server(agent_control=Query()),'read_ui_action_result',{'command_id':COMMAND})['result']
            self.assertFalse(reply['isError']);observed=reply['structuredContent']
            self.assertEqual(observed['status'],status)
            if status=='unknown':
                self.assertEqual(observed['failure']['code'],'native_result_unknown')
                self.assertEqual(observed['failure']['severity'],'critical')
                self.assertEqual(observed['failure']['phase'],'result_query')
                self.assertNotIn('failure',obj)
            else:self.assertNotIn('failure',observed)
    def test_nonstring_legacy_codes_are_private_and_safe(self):
        from error_contract import detail
        for code in (None,False,[],{'PRIVATE':'packet'}):
            self.assertEqual(detail(code,code,code)['code'],'legacy_unavailable')
            class Old:
                def observe(self):raise Unavailable('PRIVATE',failure_code=code)
            obj=self.error(adapter.Server(agent_control=Old()))
            self.assertEqual(obj['failure']['code'],'legacy_unavailable')

    def test_untyped_legacy_result_query_retains_action_uncertainty(self):
        class Old:
            def read_result(self,command_id):raise Unavailable('PRIVATE previous action state')
        obj=self.error(adapter.Server(agent_control=Old()),'read_ui_action_result',{'command_id':COMMAND})
        self.assertEqual(obj['failure']['phase'],'result_query')
        self.assertEqual(obj['failure']['action_outcome'],'unknown')
        self.assertEqual(obj['failure']['severity'],'critical')
        self.assertEqual(obj['failure']['recovery'],'reconcile')

    def test_closed_query_is_unknown_not_proven_nonmutation(self):
        n,s=self.fixture();a0=arguments(n.client.observe());n.client.execute(**a0);n.client.close()
        obj=self.error(s,'read_ui_action_result',{'command_id':COMMAND})
        self.assertEqual(obj['failure']['code'],'channel_closed')
        self.assertEqual(obj['failure']['phase'],'result_query')
        self.assertEqual(obj['failure']['action_outcome'],'unknown')
        self.assertEqual(obj['failure']['severity'],'critical')
        self.assertEqual(sum(r.startswith(b'E ') for r in n.requests),1)
