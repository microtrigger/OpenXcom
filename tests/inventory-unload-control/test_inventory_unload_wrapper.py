#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pure fake-runner checks. These never launch a game or claim native PASS."""
import ast
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import re
import subprocess
import unittest
from unittest import mock

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('unload_wrapper', HERE / 'run-inventory-unload-control.py')
w = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(w)

class Wrapper(unittest.TestCase):
    def result(self, profile, master='xcom1', rc=0, out=None, err=''):
        expected=w.PASS.replace('MASTER',master).replace('PROFILE',profile)
        return subprocess.CompletedProcess([],rc,expected if out is None else out,err)
    def run_suite(self, runner, master='xcom1'):
        return w.run(Path('/private/binary'),Path('/private/data'),master,runner)
    def test_profiles_and_native_contract(self):
        source=(HERE/'InventoryUnloadControlGate.cpp').read_text()
        block=re.search(r'const std::set<std::string> profiles=\{([^}]+)\}',source).group(1)
        self.assertEqual(tuple(ast.literal_eval(x) for x in block.split(',')),w.PROFILES)
        self.assertEqual(len(w.PROFILES),len(set(w.PROFILES)))
        self.assertIn('"profiles":'+str(len(w.PROFILES)),w.SUITE_PASS)
        block=re.search(r'std::cout << ("INVENTORY_UNLOAD_CONTROL_GATE PASS .*?) << std::endl;',source).group(1)
        parts=block.split(' << ')
        self.assertEqual(parts[1::2],['master','profile'])
        for master in ('xcom1','xcom2'):
            for profile in w.PROFILES:
                self.assertEqual(ast.literal_eval(parts[0])+master+ast.literal_eval(parts[2])+profile+ast.literal_eval(parts[4]),w.PASS.replace('MASTER',master).replace('PROFILE',profile))
    def test_all_profiles_fresh_files_and_explicit_environment(self):
        for master in ('xcom1','xcom2'):
            seen=[]
            def runner(argv,**kw):
                profile=kw['env']['OPENXCOM_INVENTORY_UNLOAD_PROFILE']
                self.assertEqual(profile,w.PROFILES[len(seen)])
                self.assertTrue(all(not x.exists() for x in seen))
                root=Path(argv[argv.index('-cfg')+1]).parent
                self.assertTrue((root/'user').is_dir())
                self.assertIn(f'id: {master}',(root/'config/options.cfg').read_text())
                env=kw['env']
                self.assertEqual(env['SDL_AUDIODRIVER'],'dummy')
                self.assertEqual(env.get('OPENXCOM_INVENTORY_UNLOAD_ENABLED'),None if profile=='disabled' else '1')
                self.assertEqual(env.get('OPENXCOM_INVENTORY_CLOSE_ENABLED'),None if profile=='close-disabled' else '1')
                self.assertEqual(env.get('OPENXCOM_INVENTORY_CLICK_ENABLED'),None if profile=='independent' else '1')
                self.assertEqual(env.get('OPENXCOM_INVENTORY_GROUND_ENABLED'),None if profile=='independent' else '1')
                self.assertNotIn('OPENXCOM_STANCE_FD',env)
                self.assertEqual(env['UNRELATED_TEST_VALUE'],'kept')
                self.assertEqual(kw['timeout'],650 if profile=='core' else 155 if profile in ('timeout','capability-timeout') else 45)
                self.assertEqual((kw['stdout'],kw['stderr'],kw['text'],kw['check']),(subprocess.PIPE,subprocess.PIPE,True,False))
                seen.append(root)
                return self.result(profile,master)
            with mock.patch.dict(os.environ,{'OPENXCOM_STANCE_FD':'secret','UNRELATED_TEST_VALUE':'kept'}):
                self.assertEqual(self.run_suite(runner,master),w.SUITE_PASS.replace('MASTER',master))
            self.assertEqual(len(seen),len(w.PROFILES))
            self.assertTrue(all(not x.exists() for x in seen))
    def test_exact_pass_required(self):
        expected=w.PASS.replace('MASTER','xcom1').replace('PROFILE','core')
        cases=[(0,'' ,''),(1,expected,''),(0,expected+'\n'+expected,''),(0,' '+expected,''),(0,expected+'\nINVENTORY_UNLOAD_CONTROL_GATE FAIL assertion-101',''),(0,'',expected),(0,expected,'INVENTORY_UNLOAD_CONTROL_GATE FAIL assertion-101'),(0,expected.replace('core','wrong'),''),(0,expected,'INVENTORY_UNLOAD_CONTROL_OTHER hidden')]
        for rc,out,err in cases:
            with self.subTest(rc=rc,out=out,err=err):
                with self.assertRaisesRegex(RuntimeError,'^'+w.FAIL+'$'):
                    self.run_suite(lambda *a,**k:self.result('core',rc=rc,out=out,err=err))
    def test_only_whitelisted_failure_escapes(self):
        label='INVENTORY_UNLOAD_CONTROL_GATE FAIL assertion-101'
        with self.assertRaisesRegex(RuntimeError,'^'+label+'$'):
            self.run_suite(lambda *a,**k:self.result('core',rc=1,out='/private/detail',err=label+'\n/private/path'))
        for err in [label+' private',label+'\n'+label,'INVENTORY_UNLOAD_CONTROL_GATE FAIL assertion-bad',label+'\nINVENTORY_UNLOAD_CONTROL_GATE PASS private']:
            with self.assertRaisesRegex(RuntimeError,'^'+w.FAIL+'$'):
                self.run_suite(lambda *a,**k:self.result('core',rc=1,out='',err=err))
    def test_fail_fast_and_redacted_exceptions(self):
        for error in [OSError('/private/path'),subprocess.TimeoutExpired('/private/path',45),subprocess.SubprocessError('private')]:
            calls=[]
            def runner(*a,**k): calls.append(True); raise error
            with self.assertRaisesRegex(RuntimeError,'^'+w.FAIL+'$'): self.run_suite(runner)
            self.assertEqual(calls,[True])
    def test_invalid_invocation_never_launches(self):
        runner=mock.Mock()
        for binary,data,master in [(Path('relative'),Path('/data'),'xcom1'),(Path('/bin'),Path('relative'),'xcom1'),(Path('/bin'),Path('/data'),'bad')]:
            with self.assertRaises(ValueError): w.run(binary,data,master,runner)
        runner.assert_not_called()
    def test_cli_redaction(self):
        for args,code in [([],2),(['/bin','/data','bad'],2)]:
            out,err=io.StringIO(),io.StringIO()
            with contextlib.redirect_stdout(out),contextlib.redirect_stderr(err): self.assertEqual(w.main(args),code)
            self.assertEqual(out.getvalue(),'')
        with mock.patch.object(w,'run',side_effect=RuntimeError('/private/secret')):
            err=io.StringIO()
            with contextlib.redirect_stderr(err): self.assertEqual(w.main(['/bin','/data','xcom1']),1)
            self.assertEqual(err.getvalue(),w.FAIL+'\n')

if __name__=='__main__': unittest.main()
