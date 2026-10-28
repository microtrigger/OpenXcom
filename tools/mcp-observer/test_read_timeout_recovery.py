# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic same-channel R recovery; fake clock/select/packet peer, no game."""
import copy
import socket
import unittest
from contextlib import ExitStack
from unittest.mock import patch
import agent_control as a
from snapshots import Unavailable
from test_agent_control import state, result, dump, EPOCH, CAPABILITY

A, B = 'a'*32, 'b'*32


def observed(challenge=A, capability='6'*32, revision=2, sequence=2):
    return dump(state(challenge=challenge, capability=capability, revision=revision, event_sequence=sequence))


def expired(challenge=A, epoch=EPOCH):
    return dump(dict(schema=a.SCHEMA, mode=a.MODE, type='read_error',
                     read_error_schema='openxcom-agent-read-error/1', epoch=epoch,
                     challenge=challenge, reason='read_expired'))


class Peer:
    def __init__(self, frames):
        self.frames=list(frames);self.sent=[];self.receives=0;self.closed=False
    def send(self, packet):
        self.sent.append(packet);return len(packet)
    def recvmsg(self, size):
        self.receives+=1
        value=self.frames.pop(0)
        return value if type(value) is tuple else (value, [], 0, None)
    def recv(self, size, flags):return b'X'
    def close(self):self.closed=True


class Scenario:
    def __init__(self, frames, selects, *, new=False):
        left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET)
        self.client=a.AgentControlClient(left,timeout=.2,action_timeout=.2,startup_timeout=.2)
        left.close();right.close()
        self.peer=Peer(frames);self.client.channel=self.peer
        if not new:
            self.client.state='READY';self.client._handshake_complete=True;self.client.epoch=EPOCH
            self.client.revision=1;self.client.event_sequence=1;self.client.last_capability=CAPABILITY
            self.client.capability=CAPABILITY;self.client.available_actions=frozenset({'kneel'})
            self.client.grid_cells=frozenset({'g0'});self.client.capability_deadline=99
        self.clock=0.;self.selects=list(selects);self.waits=[]
    def select(self, readers, writers, errors, timeout):
        ready,advance=self.selects.pop(0);self.waits.append(timeout);self.clock+=advance
        return (readers if ready else [], [], [])
    def __enter__(self):
        self.stack=ExitStack()
        self.stack.enter_context(patch.object(a.time,'monotonic',side_effect=lambda:self.clock))
        self.stack.enter_context(patch.object(a.select,'select',side_effect=self.select))
        self.stack.enter_context(patch.object(a.secrets,'token_hex',side_effect=[A,B]))
        return self
    def __exit__(self,*args):self.stack.close();self.client.close()


FIRST=[(False,0),(False,.2)]
RECOVER=[(True,.03),(False,0),(True,.01),(False,0)]


class ReadRecoveryTests(unittest.TestCase):
    def pending(self,s):
        with self.assertRaises(Unavailable) as error:s.client.observe()
        self.assertEqual(error.exception.failure_code,'read_recovery_required')
        self.assertEqual(s.client.state,'READ_RECOVERY_REQUIRED');self.assertFalse(s.peer.closed)
        self.assertEqual(s.peer.receives,0);self.assertEqual(s.peer.sent,[('R '+A).encode()])
        self.assertIsNone(s.client.capability);self.assertFalse(s.client.available_actions)
        self.assertFalse(s.client.grid_cells);self.assertEqual(s.client.capability_deadline,0)

    def test_delayed_state_retired_once_then_fresh_state_same_deadline(self):
        with Scenario([observed(),observed(B,'7'*32,3,3)],FIRST+RECOVER) as s:
            previous=dump(result(event_sequence=1));s.client.commands['3'*32]={'result':previous}
            self.pending(s);obj=s.client.observe()
            self.assertIs(s.client.commands['3'*32]['result'],previous)
            self.assertEqual(obj['challenge'],B);self.assertEqual(s.client.capability,'7'*32)
            self.assertEqual(s.peer.sent,[('R '+A).encode(),('R '+B).encode()])
            self.assertEqual(s.peer.receives,2);self.assertIsNone(s.client._read_recovery)
            self.assertAlmostEqual(s.waits[2],.2);self.assertAlmostEqual(s.waits[4],.17)
            self.assertTrue(all(not p.startswith(b'E ') for p in s.peer.sent))

    def test_correlated_expiry_and_old_generic_fail_closed(self):
        with Scenario([expired(),observed(B,'7'*32,2,2)],FIRST+RECOVER) as s:
            self.pending(s);self.assertEqual(s.client.observe()['challenge'],B)
        generic=dump(dict(schema=a.SCHEMA,mode=a.MODE,type='error',status='unavailable'))
        with Scenario([generic],FIRST+[(True,0)]) as s:
            self.pending(s)
            with self.assertRaises(Unavailable):s.client.observe()
            self.assertTrue(s.peer.closed);self.assertEqual(len(s.peer.sent),1)

    def test_wrong_retired_identity_lower_bounds_and_private_shape_stop(self):
        bad=[observed('c'*32),expired(epoch='9'*32),observed(revision=0),observed(sequence=0),
             observed(capability=CAPABILITY),b'{PRIVATE',b'']
        extra=state(challenge=A,capability='6'*32);extra['hidden']='PRIVATE';bad.append(dump(extra))
        for frame in bad:
            with self.subTest(frame_kind=type(frame).__name__),Scenario([frame],FIRST+[(True,0)]) as s:
                self.pending(s)
                with patch.object(s.client,'empty_packet_code',return_value='transport_eof'):
                    with self.assertRaises(Unavailable):s.client.observe()
                self.assertTrue(s.peer.closed);self.assertEqual(len(s.peer.sent),1)

    def test_extra_and_delayed_duplicate_after_zero_poll_stop(self):
        with Scenario([observed()],FIRST+[(True,0),(True,0)]) as s:
            self.pending(s)
            with self.assertRaises(Unavailable):s.client.observe()
            self.assertTrue(s.peer.closed);self.assertEqual(len(s.peer.sent),1)
        with Scenario([observed(),observed()],FIRST+RECOVER) as s:
            self.pending(s)
            with self.assertRaises(Unavailable):s.client.observe()
            self.assertTrue(s.peer.closed);self.assertEqual(len(s.peer.sent),2)
            self.assertEqual(s.peer.receives,2)

    def test_fresh_reuse_regression_and_recovery_timeout_do_not_recurse(self):
        for frame in (observed(B,'6'*32,3,3),observed(B,CAPABILITY,3,3),
                      observed(B,'7'*32,1,3),observed(B,'7'*32,3,1)):
            with Scenario([observed(),frame],FIRST+RECOVER) as s:
                self.pending(s)
                with self.assertRaises(Unavailable):s.client.observe()
                self.assertTrue(s.peer.closed)
        with Scenario([],FIRST+[(False,.2)]) as s:
            self.pending(s)
            with self.assertRaises(Unavailable):s.client.observe()
            self.assertTrue(s.peer.closed);self.assertEqual(len(s.peer.sent),1)
        with Scenario([observed()],FIRST+[(True,.03),(False,0),(False,.17)]) as s:
            self.pending(s)
            with self.assertRaises(Unavailable):s.client.observe()
            self.assertTrue(s.peer.closed);self.assertEqual(len(s.peer.sent),2)

    def test_readable_late_and_received_late_are_not_eligible(self):
        with Scenario([],[(False,0),(True,.2)]) as s:
            with self.assertRaises(Unavailable) as error:s.client.observe()
            self.assertEqual(error.exception.failure_code,'transport_timeout')
            self.assertTrue(s.peer.closed);self.assertEqual(s.peer.receives,0)
        with Scenario([observed()],[(False,0),(True,0)]) as s:
            original=s.peer.recvmsg
            def late(size):
                result=original(size);s.clock+=.2;return result
            s.peer.recvmsg=late
            with self.assertRaises(Unavailable):s.client.observe()
            self.assertTrue(s.peer.closed);self.assertEqual(s.peer.receives,1)

    def test_no_handshake_short_send_or_unresolved_action_cannot_recover(self):
        with Scenario([],FIRST) as s:
            s.client._handshake_complete=False
            with self.assertRaises(Unavailable):s.client.observe()
            self.assertTrue(s.peer.closed)
        with Scenario([],[(False,0)]) as s:
            s.peer.send=lambda packet:0
            with self.assertRaises(Unavailable):s.client.observe()
            self.assertTrue(s.peer.closed)
        for status in (None,'pending','unavailable','unknown_after_dispatch','unknown'):
            with Scenario([],FIRST) as s:
                s.client.commands['3'*32]={'result':None if status is None else dump({'status':status})}
                with self.assertRaises(Unavailable):s.client.observe()
                self.assertTrue(s.peer.closed)

    def test_pending_blocks_execute_query_and_events_without_send(self):
        with Scenario([],FIRST) as s:
            self.pending(s);before=list(s.peer.sent)
            s.client.commands['3'*32]={'result':dump({'status':'dispatched'})}
            operations=(lambda:s.client.execute('3'*32,EPOCH,CAPABILITY,1,'kneel',{}),
                        lambda:s.client.read_result('3'*32),lambda:s.client.read_events(EPOCH,0,1))
            for operation in operations:
                with self.assertRaises(Unavailable):operation()
                self.assertEqual(s.peer.sent,before);self.assertFalse(s.peer.closed)

    def test_handshake_completion_is_independent_of_new_state(self):
        hello=dump(dict(schema=a.SCHEMA,mode=a.MODE,epoch=EPOCH,ledger_capacity=256,command_generation=1,audio_history='mixed_output'))
        ready=dump(dict(schema=a.SCHEMA,mode=a.MODE,status='ready'))
        with Scenario([hello,ready],[(True,0),(False,0),(True,0),(False,0)]+FIRST,new=True) as s:
            with self.assertRaises(Unavailable) as error:s.client.observe()
            self.assertEqual(error.exception.failure_code,'read_recovery_required')
            self.assertTrue(s.client._handshake_complete);self.assertFalse(s.peer.closed)

    def test_versioned_read_error_exactness_and_fresh_challenge(self):
        obj=a.parse(expired());self.assertEqual(a.validate_read_error(obj,A,EPOCH),obj)
        for key,value in (('read_error_schema','openxcom-agent-read-error/0'),('reason','PRIVATE'),
                          ('capability',CAPABILITY),('challenge','c'*32)):
            candidate=dict(obj);candidate[key]=value
            with self.assertRaises(Unavailable):a.validate_read_error(candidate,A,EPOCH)
        with Scenario([observed()],FIRST+[(True,0)]) as s:
            self.pending(s)
            with patch.object(a.secrets,'token_hex',return_value=A):
                with self.assertRaises(Unavailable):s.client.observe()
            self.assertTrue(s.peer.closed);self.assertEqual(len(s.peer.sent),1)

if __name__=='__main__':unittest.main()
