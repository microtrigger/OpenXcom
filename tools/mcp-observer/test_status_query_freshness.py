"""Deterministic phase barriers, no wall-clock race or real game.

The socket/clock/select seams run actual StanceClient exchange/status code. They
characterize specified schedules, not the historical aggregate failure cause.
"""
import copy
import json
import threading
import unittest
from unittest.mock import patch
import adapter
import supervised_stance as s
from snapshots import Unavailable, exact, require
from test_supervised_stance import COMMAND, EPOCH, NativeFixture, arguments, call, dump, status

NONCE = '0' * 31 + '1'

class ScriptedChannel:
    def __init__(self, raw): self.raw=raw;self.sends=[];self.closed=False
    def send(self, raw): self.sends.append(raw);return len(raw)
    def recvmsg(self, maximum): return (self.raw,[],0,None)
    def close(self): self.closed=True

class PhaseBarrier:
    """Discrete preflight/wait/postflight transcript; never sleeps or repolls."""
    def __init__(self, channel, before=False, after=False):
        self.channel=channel;self.before=before;self.after=after;self.phases=[]
    def __call__(self, readers,writers,errors,timeout):
        index=len(self.phases);self.phases.append(('preflight','receive','postflight')[index])
        ready=self.before if index==0 else True if index==1 else self.after
        return ([self.channel] if ready else [],[],[])

def seamed_client(raw, prior='pending_confirmation'):
    client=object.__new__(s.StanceClient)
    client.channel=ScriptedChannel(raw);client.timeout=.3;client.startup_timeout=.3
    client.state='PENDING' if prior=='pending_confirmation' else 'NEED_CONTEXT';client.control_epoch=EPOCH;client.capability=None;client.capability_kind=None
    client.active=COMMAND;client._query_sequence=0;client.flight=threading.Lock()
    client.commands={COMMAND:{'action':'L','arguments':{},'status':prior}}
    client.inventory_verification_required=client.tactical_verification_required=False
    client.inventory_right_verification_required=False
    return client

class StatusQueryFreshnessTests(unittest.TestCase):
    def test_initial_and_query_exact_shapes_and_strip_nonce(self):
        for value in s.STATUSES:
            for nonce in (None,NONCE):
                result=s.validate_status(dump(status(value=value,query_nonce=nonce)),COMMAND,nonce)
                self.assertEqual(set(result),{'schema','command_id','status'})
                self.assertEqual(result['schema'],'experimental-stance-status/2')
        for nonce in (None,'0'*32,'a'*32,True,{},1):
            with self.subTest(nonce=type(nonce).__name__),self.assertRaises(Unavailable):
                s.validate_status(dump(status(query_nonce=nonce)),COMMAND,NONCE)
        with self.assertRaises(Unavailable):s.validate_status(dump(status(query_nonce=NONCE)),COMMAND)
        for mutation in ({'schema':'experimental-stance-status/1'},{'command_id':'a'*32},{'extra':True}):
            with self.assertRaises(Unavailable):s.validate_status(dump(dict(status(query_nonce=NONCE),**mutation)),COMMAND,NONCE)
        obj=status(query_nonce=NONCE);del obj['query_nonce']
        with self.assertRaises(Unavailable):s.validate_status(dump(obj),COMMAND,NONCE)

    def test_old_new_hello_fail_closed_without_action(self):
        peer=NativeFixture(hello=dump({'schema':'experimental-stance/1','control_epoch':EPOCH}))
        self.addCleanup(peer.close)
        with self.assertRaises(Unavailable):peer.client.observe()
        self.assertEqual(peer.requests,[]);self.assertEqual(peer.client.state,'CLOSED')
        # Literal legacy hello validation boundary, not an old full client claim.
        def legacy_validate(raw):
            obj=s.parse(raw,256);exact(obj,'schema control_epoch')
            require(obj['schema']=='experimental-stance/1');s.token(obj['control_epoch'])
            return obj['control_epoch']
        with self.assertRaises(Unavailable):legacy_validate(dump({'schema':s.HELLO_SCHEMA,'control_epoch':EPOCH}))
        self.assertEqual(s.validate_hello(dump({'schema':s.HELLO_SCHEMA,'control_epoch':EPOCH})),EPOCH)

    def run_schedule(self, raw, before=False,after=False, prior='pending_confirmation', query_id=COMMAND, sequence=0):
        client=seamed_client(raw,prior);client._query_sequence=sequence
        channel=client.channel;barrier=PhaseBarrier(channel,before,after)
        old=copy.deepcopy(client.commands)
        with patch.object(s.time,'monotonic',return_value=0),patch.object(s.select,'select',barrier):
            with self.assertRaises(Unavailable):client.read_status(query_id)
        self.assertEqual(client.commands,old);self.assertEqual(client.state,'CLOSED')
        self.assertTrue(channel.closed);self.assertEqual(len(channel.sends),0 if before else 1)
        self.assertEqual(barrier.phases,['preflight'] if before else ['preflight','receive','postflight'])
        count=len(channel.sends)
        with self.assertRaises(Unavailable):client.read_status(query_id)
        self.assertEqual(len(channel.sends),count)
        return client,channel

    def test_extra_before_preflight_refuses_without_send(self):
        self.run_schedule(dump(status(query_nonce=NONCE)),before=True)

    def test_late_initial_and_old_same_id_query_after_preflight_refused(self):
        # First packet arrives only at receive, genuine reply held after postflight.
        # Legacy acceptance is SOURCE INFERENCE ONLY, not executed here.
        for stale in (None,'0'*31+'2'):
            self.run_schedule(dump(status(query_nonce=stale)))

    def test_previously_issued_nonce_rejected_for_pending_and_equal_terminal(self):
        for value in ('pending_confirmation','dispatched'):
            # Sequence1 already issued; next query must use2, old1 is rejected
            # even where status monotonicity alone would permit the same value.
            client,channel=self.run_schedule(dump(status(value=value,query_nonce=NONCE)),
                                            prior=value,sequence=1)
            self.assertEqual(channel.sends,[('S'+COMMAND+f'{2:032x}').encode()])
            self.assertEqual(client._query_sequence,2)

    def test_different_id_or_extra_ready_at_postflight_refused(self):
        self.run_schedule(dump(status('a'*32,query_nonce=NONCE)))
        self.run_schedule(dump(status(query_nonce=NONCE)),after=True)

    def test_terminal_cannot_regress_even_with_matching_nonce(self):
        self.run_schedule(dump(status(query_nonce=NONCE)),prior='dispatched')

    def test_fresh_matching_queries_have_distinct_nonce(self):
        client=seamed_client(b'');channel=client.channel
        for ordinal in range(1,4):
            channel.raw=dump(status(query_nonce=f'{ordinal:032x}'))
            barrier=PhaseBarrier(channel)
            with patch.object(s.time,'monotonic',return_value=0),patch.object(s.select,'select',barrier):
                result=client.read_status(COMMAND)
            self.assertEqual(set(result),{'schema','command_id','status'})
            self.assertEqual(channel.sends[-1],('S'+COMMAND+f'{ordinal:032x}').encode())
        self.assertEqual(len(channel.sends),3);self.assertEqual(client._query_sequence,3)

    def test_late_status_cannot_satisfy_fresh_observation(self):
        client=seamed_client(dump(status(query_nonce=None)))
        channel=client.channel;client.state='NEED_CONTEXT'
        barrier=PhaseBarrier(channel)
        with patch.object(s.time,'monotonic',return_value=0),patch.object(s.select,'select',barrier):
            with self.assertRaises(Unavailable):client.observe()
        self.assertEqual(client.state,'CLOSED');self.assertEqual(len(channel.sends),1)
        self.assertEqual(channel.sends[0][:1],b'O')
        self.assertIsNone(client.capability)

    def test_counter_exhaustion_closes_before_send_no_wrap(self):
        client=seamed_client(b'');channel=client.channel;client._query_sequence=s.MAX_QUERY_SEQUENCE
        with self.assertRaises(Unavailable):client.read_status(COMMAND)
        self.assertEqual(channel.sends,[]);self.assertTrue(channel.closed)
        self.assertEqual(client._query_sequence,s.MAX_QUERY_SEQUENCE)

    def test_real_fixture_duplicate_queries_and_public_shape(self):
        peer=NativeFixture();self.addCleanup(peer.close);peer.client.observe();args=arguments(peer.client.capability,EPOCH)
        first=peer.client.request(**args);self.assertEqual(set(first),{'schema','command_id','status'})
        for i in (1,2):
            result=peer.client.request(**args);self.assertEqual(set(result),{'schema','command_id','status'})
            self.assertEqual(peer.requests[-1],('S'+COMMAND+f'{i:032x}').encode())
        self.assertEqual(sum(x[:1]==b'D' for x in peer.requests),1)
        server=adapter.Server(supervised_stance=peer.client)
        response=call(server,'read_stance_status',{'command_id':COMMAND})
        public=response['result']['structuredContent']
        self.assertEqual(set(public),{'schema','command_id','status','semantics'})
        self.assertEqual(public['schema'],s.PUBLIC_STATUS_SCHEMA)

if __name__=='__main__':unittest.main()
