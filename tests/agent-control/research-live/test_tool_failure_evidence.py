import ast
import importlib.util
import json
from pathlib import Path
import unittest
from tool_failure_evidence import CODES, safe_tool_failure
from run_research_smoke import Rpc as ResearchRpc
from run_production_smoke import Rpc as ProductionRpc

class FailureEvidenceTest(unittest.TestCase):
    def test_runtime_vocabulary_stays_in_sync(self):
        root=Path(__file__).resolve().parents[3]
        tree=ast.parse((root/'tools/mcp-observer/error_contract.py').read_text())
        codes=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign)
                   and any(isinstance(t,ast.Name) and t.id=='CODES' for t in n.targets))
        self.assertEqual(CODES,codes)

    def test_all_codes_survive_without_arbitrary_fields(self):
        for code in CODES:
            with self.subTest(code=code):
                result={'isError':True,'structuredContent':{'error':'unavailable',
                    'failure':{'code':code,'phase':'read','action_outcome':'not_applicable',
                               'path':'PRIVATE','exception':'PRIVATE','secret':'PRIVATE'},
                    'raw_packet':'PRIVATE','capability':'PRIVATE'}}
                out=safe_tool_failure('read_ui_state',result)
                self.assertEqual(out['failure']['code'],code)
                self.assertNotIn('PRIVATE',json.dumps(out))

    def test_unknown_overrides_claimed_safe_recovery(self):
        out=safe_tool_failure('execute_ui_action',{'structuredContent':{'status':'unavailable',
            'failure':{'code':'native_result_unavailable','phase':'send_or_receipt',
                       'action_outcome':'unknown','severity':'info','recovery':'retry'}}})
        self.assertEqual(out['failure']['severity'],'critical')
        self.assertEqual(out['failure']['recovery'],'reconcile')
        self.assertFalse(out['automatic_replay'])

    def test_untrusted_values_never_serialized(self):
        for value in ['PRIVATE',[],{},None]:
            out=safe_tool_failure(value,{'structuredContent':{'error':'PRIVATE',
                'failure':{'code':value,'phase':'PRIVATE','action_outcome':'PRIVATE'}}})
            self.assertNotIn('PRIVATE',json.dumps(out))
            self.assertNotIn('failure',out)

    def test_old_unavailable_is_preserved_without_invented_cause(self):
        out=safe_tool_failure('read_ui_state',{'isError':True,'structuredContent':{'error':'unavailable'}})
        self.assertEqual(out['error'],'unavailable')
        self.assertNotIn('failure',out)

    def test_both_runners_stop_once_and_keep_reason(self):
        for rpc_type in (ResearchRpc,ProductionRpc):
            for status in ('outcome_unknown','unknown_after_dispatch','unavailable'):
                with self.subTest(rpc=rpc_type.__module__,status=status):
                    rpc=object.__new__(rpc_type);calls=[]
                    result={'isError':False,'structuredContent':{'status':status,'failure':{
                        'code':'native_result_unavailable','phase':'send_or_receipt','action_outcome':'unknown'}}}
                    rpc.request=lambda *a:(calls.append(a) or result)
                    with self.assertRaises(RuntimeError) as caught:rpc.call('execute_ui_action',{})
                    self.assertEqual(str(caught.exception),'tool_error_stop_no_replay')
                    self.assertEqual(caught.exception.tool_failure['failure']['action_outcome'],'unknown')
                    self.assertEqual(len(calls),1)

    def test_legacy_native_uncertainty_is_preserved(self):
        for rpc_type in (ResearchRpc,ProductionRpc):
            for status in ('unavailable','unknown','unknown_after_dispatch'):
                rpc=object.__new__(rpc_type);calls=[]
                rpc.request=lambda *a:(calls.append(a) or {'isError':False,'structuredContent':{'status':status}})
                with self.assertRaises(RuntimeError) as caught:rpc.call('execute_ui_action',{})
                self.assertEqual(caught.exception.tool_failure['status'],status)
                self.assertNotIn('failure',caught.exception.tool_failure)
                self.assertEqual(len(calls),1)

    def test_success_not_reclassified(self):
        for rpc_type in (ResearchRpc,ProductionRpc):
            rpc=object.__new__(rpc_type);obj={'state':'unsupported','capability':None}
            rpc.request=lambda *a:{'isError':False,'structuredContent':obj}
            self.assertEqual(rpc.call('read_ui_state',{}),obj)

if __name__=='__main__':unittest.main()
