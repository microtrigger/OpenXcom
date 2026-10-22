import unittest
import json
from unittest.mock import Mock, patch
from run_manufacture_limits import Rpc, SmokeError

class RpcProbeTests(unittest.TestCase):
    def rpc(self,reply):
        r=Rpc.__new__(Rpc);r.ident=0;r.timeout=.01;r.buffer=json.dumps(reply).encode()+b'\n';r.send=Mock();return r
    def envelope(self):return {'jsonrpc':'2.0','id':1,'error':{'code':-32602,'message':'Invalid params'}}
    def args(self):return {'epoch':'e','capability':'c','revision':'r','command_id':'q','action':'change_production_quantity','args':{'control':'units','operation':'set','value':100001}}
    def test_exact_expected_rejection_one_real_call(self):
        r=self.rpc(self.envelope());args=self.args()
        self.assertEqual(r.call('probe_invalid_quantity',args),{'status':'invalid_params_rejected','code':-32602,'message':'Invalid params'})
        r.send.assert_called_once_with({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'execute_ui_action','arguments':args}})
    def test_nonexact_responses_stop_once(self):
        bad=[None,[],{}, {'jsonrpc':'2.0','id':1,'result':{}},dict(self.envelope(),result={}),dict(self.envelope(),data={}),dict(self.envelope(),id=2),dict(self.envelope(),id=True),dict(self.envelope(),jsonrpc='1.0')]
        for err in [{'code':-32602,'message':'Invalid params','data':None},{'code':-32603,'message':'Invalid params'},{'code':-32602,'message':'invalid params'},{'code':-32602.0,'message':'Invalid params'}]:bad.append(dict(self.envelope(),error=err))
        for reply in bad:
            with self.subTest(reply=reply):
                r=self.rpc(reply)
                with self.assertRaises(SmokeError):r.call('probe_invalid_quantity',self.args())
                self.assertEqual(r.send.call_count,1)
    def test_normal_call_still_rejects_errors(self):
        r=self.rpc(self.envelope())
        with self.assertRaises(SmokeError):r.call('execute_ui_action',self.args())
        self.assertEqual(r.send.call_count,1)
    def test_normal_result_unchanged(self):
        obj={'status':'dispatched','observation':{}};r=self.rpc({'jsonrpc':'2.0','id':1,'result':{'structuredContent':obj}})
        self.assertEqual(r.call('execute_ui_action',self.args()),obj)
    def test_normal_envelope_strict_identity_and_result_presence(self):
        valid={'jsonrpc':'2.0','id':1,'result':{'structuredContent':{'state':'unsupported'}}}
        malformed=[{k:v for k,v in valid.items() if k!='jsonrpc'},
                   {k:v for k,v in valid.items() if k!='result'},
                   dict(valid,jsonrpc='1.0'),dict(valid,id=True),dict(valid,id=1.0),
                   dict(valid,id=2),dict(valid,error={}),dict(valid,data={})]
        for reply in malformed:
            r=self.rpc(reply)
            with self.assertRaisesRegex(SmokeError,'invalid_rpc_response'):r.call('read_ui_state',{})
            self.assertEqual(r.send.call_count,1)
        r=self.rpc(valid)
        self.assertEqual(r.call('read_ui_state',{}),{'state':'unsupported'})
    def test_probe_scope_rejected_before_send(self):
        for value in [1,100000,100001.0,None]:
            r=self.rpc(self.envelope());a=self.args();a['args']['value']=value
            with self.assertRaises(SmokeError):r.call('probe_invalid_quantity',a)
            r.send.assert_not_called()
    def test_timeout_is_unknown_no_retry(self):
        r=self.rpc({});r.buffer=b'';r.selector=Mock();r.selector.select.return_value=[]
        with self.assertRaisesRegex(SmokeError,'transport_timeout_no_replay'):r.call('probe_invalid_quantity',self.args())
        self.assertEqual(r.send.call_count,1)

class ActualAdapterBoundaryTests(unittest.TestCase):
    def test_exact_real_adapter_rejection_before_execute(self):
        import sys
        from pathlib import Path
        adapter_dir=Path(__file__).resolve().parents[3]/'tools/mcp-observer'
        sys.path.insert(0,str(adapter_dir))
        try:
            import adapter
            self.assertEqual(Path(adapter.__file__).resolve().parent,adapter_dir)
            spy=Mock();spy.execute.return_value={'status':'dispatched'}
            server=adapter.Server(agent_control=spy);server.state='ready'
            args={'command_id':'3'*32,'epoch':'1'*32,'capability':'2'*32,'revision':1,
                  'action':'change_production_quantity','args':{'control':'units','operation':'set','value':100001}}
            request={'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':'execute_ui_action','arguments':args}}
            self.assertEqual(server.handle(request),{'jsonrpc':'2.0','id':1,'error':{'code':-32602,'message':'Invalid params'}})
            spy.execute.assert_not_called()
            args['args']['value']=100000
            result=server.handle(request)
            spy.execute.assert_called_once_with(**args)
            self.assertEqual(result['result']['structuredContent'],{'status':'dispatched'})
        finally:sys.path.pop(0)

class ClassifiedFailureTests(unittest.TestCase):
    def test_unknown_native_receipt_stops_with_safe_details(self):
        rpc=Rpc.__new__(Rpc)
        rpc.request=Mock(return_value={'isError':False,'structuredContent':{'status':'unavailable','failure':{
            'code':'native_result_unavailable','phase':'send_or_receipt','action_outcome':'unknown'}}})
        with self.assertRaises(SmokeError) as caught:rpc.call('execute_ui_action',{})
        self.assertEqual(caught.exception.tool_failure['failure']['severity'],'critical')
        self.assertEqual(rpc.request.call_count,1)
    def test_classified_read_failure_retained(self):
        rpc=Rpc.__new__(Rpc)
        rpc.request=Mock(return_value={'isError':True,'structuredContent':{'error':'unavailable','failure':{
            'code':'transport_eof','phase':'read','action_outcome':'not_applicable'}}})
        with self.assertRaises(SmokeError) as caught:rpc.call('read_ui_state',{})
        self.assertEqual(caught.exception.tool_failure['failure']['code'],'transport_eof')
        self.assertEqual(rpc.request.call_count,1)

class ManualOnlyLauncherTests(unittest.TestCase):
    def test_launches_only_limits_and_requires_completion_marker(self):
        import tempfile
        from pathlib import Path
        import run_manufacture_limits as cli
        for completed in (False,True):
            with self.subTest(completed=completed),tempfile.TemporaryDirectory() as temp:
                root=Path(temp);source=root/'source';(source/'src').mkdir(parents=True)
                (source/'src/fake.cpp').write_text('// fixture only\n')
                (source/'tools/mcp-observer').mkdir(parents=True)
                data=root/'data';(data/'common').mkdir(parents=True);(data/'UFO').mkdir()
                binary=root/'binary';binary.write_text('fixture');binary.chmod(0o700)
                out=root/'result';focus=root/'ready'
                argv=['runner','--source',str(source),'--binary',str(binary),'--data',str(data),
                      '--master','xcom1','--output',str(out),'--focus-ready-file',str(focus)]
                process=Mock(pid=123);rpc=Mock()
                def route_factory(call,record,master):
                    self.assertEqual(master,'xcom1')
                    instance=Mock()
                    if completed:instance.run.side_effect=lambda:record('limits_phase_complete')
                    return instance
                with patch('sys.argv',argv),patch.object(cli.subprocess,'Popen',return_value=process) as spawn,patch.object(cli,'Rpc',return_value=rpc),patch.object(cli,'Route',side_effect=route_factory) as route,patch.object(cli,'wait_focus') as wait:
                    self.assertEqual(cli.main(),0 if completed else 1)
                    route.assert_called_once();rpc.initialize.assert_called_once_with()
                    wait.assert_called_once_with(focus,480)
                    self.assertNotIn('pass_fds',spawn.call_args.kwargs)
                result=json.loads((out/'RESULT.json').read_text())
                self.assertEqual(result['all_passed'],completed)
                self.assertFalse(result['research_automation_claimed'])
                self.assertTrue(result['inputs_unchanged'])
                if not completed:self.assertEqual(result['failure'],'limits_phase_not_verified')
    def test_manual_wait_bound_and_research_flags_rejected(self):
        import run_manufacture_limits as cli
        common=['runner','--source','source','--binary','binary','--data','data',
                '--master','xcom1','--output','output','--focus-ready-file','focus']
        for extra in (['--focus-timeout','481'],['--completion-allocation']):
            with patch('sys.argv',common+extra),patch.object(cli.subprocess,'Popen') as spawn:
                with self.assertRaises(SystemExit):cli.main()
                spawn.assert_not_called()
