"""Runner policy tests; no game or display is started."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import run_basescape_return_smoke as smoke

class FakeGame:
    def __init__(self):
        self.state='geoscape'; self.calls=[]; self.revision=0
        self.status='dispatched'; self.hide=None; self.bad_observation=False
    def call(self,name,args):
        self.calls.append((name,args))
        if name=='read_ui_state':
            self.revision+=1
            return dict(state=self.state, actions=[{'id':a} for b,a,c in smoke.ROUTE if b==self.state and a!=self.hide], epoch='a'*32,capability='b'*32,revision=self.revision)
        if name=='execute_ui_action':
            if args['revision']!=self.revision:raise AssertionError('stale authority')
            self.state=next(c for b,a,c in smoke.ROUTE if a==args['action'])
            return dict(status=self.status,observation={'state':'wrong' if self.bad_observation else self.state})
        raise AssertionError('unexpected call')
    def sends(self):return [a for n,a in self.calls if n=='execute_ui_action']

def quiet(*args,**kwargs):pass

class RouteTests(unittest.TestCase):
    def test_complete_route(self):
        g=FakeGame();rows=[]
        smoke.run_route(g.call,lambda k,**v:rows.append(v))
        self.assertEqual([a['action'] for a in g.sends()],[a for _,a,_ in smoke.ROUTE])
        self.assertEqual(len({a['command_id'] for a in g.sends()}),6)
        self.assertEqual(sum(n=='read_ui_state' for n,_ in g.calls),12)
        self.assertTrue(all('capability' not in r for r in rows))
    def test_missing_action(self):
        g=FakeGame();g.hide='open_bases'
        with self.assertRaisesRegex(smoke.SmokeError,'action_not_offered'):smoke.run_route(g.call,quiet)
        self.assertEqual(g.sends(),[])
    def test_unsupported_state(self):
        g=FakeGame();g.state='unsupported'
        with self.assertRaisesRegex(smoke.SmokeError,'unexpected_precondition'):smoke.run_route(g.call,quiet)
        self.assertEqual(g.sends(),[])
    def test_no_replay(self):
        for status in ('outcome_unknown','unknown_after_dispatch','unavailable','pending'):
            g=FakeGame();g.status=status
            with self.assertRaises(smoke.SmokeError):smoke.run_route(g.call,quiet)
            self.assertEqual(len(g.sends()),1)
    def test_transport_failure_no_retry(self):
        g=FakeGame()
        def call(n,a):
            if n=='execute_ui_action':
                g.calls.append((n,a));raise smoke.SmokeError('transport_timeout_no_replay')
            return g.call(n,a)
        with self.assertRaises(smoke.SmokeError):smoke.run_route(call,quiet)
        self.assertEqual(len(g.sends()),1)
    def test_wrong_receipt_observation(self):
        g=FakeGame();g.bad_observation=True
        with self.assertRaisesRegex(smoke.SmokeError,'receipt_postcondition_missing'):smoke.run_route(g.call,quiet)
        self.assertEqual(len(g.sends()),1)
    def test_gate_before_fresh_authority(self):
        g=FakeGame();order=[]
        def gate():order.append(len(g.calls));g.revision+=100
        smoke.run_route(g.call,quiet,gate)
        self.assertEqual(g.calls[order[0]][0],'read_ui_state')
        self.assertGreater(g.sends()[-1]['revision'],100)
    def test_gate_state_change_stops(self):
        g=FakeGame()
        def gate():g.state='unsupported'
        with self.assertRaises(smoke.SmokeError):smoke.run_route(g.call,quiet,gate)
        self.assertEqual(len(g.sends()),5)
    def test_missing_authority(self):
        g=FakeGame()
        def call(n,a):
            s=g.call(n,a);s['capability']=None;return s
        with self.assertRaisesRegex(smoke.SmokeError,'missing_authority'):smoke.run_route(call,quiet)
        self.assertEqual(g.sends(),[])

class TransportTests(unittest.TestCase):
    def test_error_structured_content(self):
        rpc=object.__new__(smoke.Rpc)
        rpc.request=lambda *a:{'structuredContent':{'status':'outcome_unknown'},'isError':True}
        with self.assertRaisesRegex(smoke.SmokeError,'no_replay'):rpc.call('execute_ui_action',{})
    def test_missing_structured_content(self):
        rpc=object.__new__(smoke.Rpc);rpc.request=lambda *a:{'content':[]}
        with self.assertRaises(smoke.SmokeError):rpc.call('read_ui_state',{})
    def test_real_rpc_line(self):
        code='import sys,json; a=json.loads(sys.stdin.readline()); print(json.dumps({"id":a["id"],"result":{"ok":True}}),flush=True)'
        p=subprocess.Popen([sys.executable,'-c',code],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
        rpc=smoke.Rpc(p)
        try:self.assertEqual(rpc.request('ping',{}),{'ok':True})
        finally:rpc.close()
    def test_wrong_response_id(self):
        code='import sys,json; sys.stdin.readline(); print(json.dumps({"id":99,"result":{}}),flush=True)'
        p=subprocess.Popen([sys.executable,'-c',code],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
        rpc=smoke.Rpc(p)
        try:
            with self.assertRaises(smoke.SmokeError):rpc.request('ping',{})
        finally:rpc.close()
    def test_gate_timeout(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(smoke.SmokeError,'operator_focus_timeout'):smoke.wait_focus(Path(d)/'absent',0)
    def test_gate_exists(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'ready';p.touch();smoke.wait_focus(p,1)

if __name__=='__main__':unittest.main()
