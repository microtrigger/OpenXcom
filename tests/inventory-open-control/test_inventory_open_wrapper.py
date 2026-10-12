#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Fake-runner checks only: no game, binary, assets, or runtime claim."""
import contextlib
import ast
import importlib.util
import io
import json
import os
import re
from pathlib import Path
import subprocess
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('inventory_open_wrapper', Path(__file__).with_name('run-inventory-open-control.py'))
wrapper = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(wrapper)


class Wrapper(unittest.TestCase):
    def result(self, profile='core', master='xcom1', *, rc=0, out=None, err=''):
        expected = wrapper.PASS.replace('MASTER', master).replace('PROFILE', profile)
        return subprocess.CompletedProcess([], rc, expected if out is None else out, err)

    def run_gate(self, runner, master='xcom1'):
        return wrapper.run(Path('/private/binary'), Path('/private/data'), master, runner)

    def test_exact_profile_set_and_aggregate_contract(self):
        self.assertEqual(wrapper.PROFILES, ('core', 'focus', 'overlay', 'pop', 'state', 'save', 'display',
                                          'ledger-limit', 'post-r', 'post-l', 'disabled', 'turn-disabled',
                                          'nontu', 'debug'))
        self.assertEqual(len(set(wrapper.PROFILES)), 14)
        self.assertEqual(wrapper.PASS, 'INVENTORY_OPEN_CONTROL_GATE PASS {"schema":"inventory-open-control-gate/1","master":"MASTER","profile":"PROFILE","passed":true}')
        self.assertEqual(wrapper.SUITE_PASS, 'INVENTORY_OPEN_CONTROL_SUITE PASS {"schema":"inventory-open-control-suite/1","master":"MASTER","profiles":14,"runtime_gate_pass":true,"full_fairness":false}')

    def test_native_per_profile_output_matches_exact_wrapper_contract(self):
        source = Path(__file__).with_name('InventoryOpenControlGate.cpp').read_text()
        blocks = re.findall(r'std::cout\s*<<\s*("INVENTORY_OPEN_CONTROL_GATE PASS .*?)\s*<<\s*std::endl;', source, re.S)
        self.assertEqual(len(blocks), 1)
        # Decode C++ string literals but retain and verify both interpolations.
        parts = re.split(r'\s*<<\s*', blocks[0])
        self.assertEqual(len(parts), 5)
        self.assertEqual(parts[1], 'master')
        self.assertEqual(parts[3], 'profile')
        for master in ('xcom1', 'xcom2'):
            for profile in wrapper.PROFILES:
                native = ast.literal_eval(parts[0]) + master + ast.literal_eval(parts[2]) + profile + ast.literal_eval(parts[4])
                self.assertEqual(native, wrapper.PASS.replace('MASTER', master).replace('PROFILE', profile))

    def test_native_accepts_exactly_all_wrapper_profiles(self):
        source = Path(__file__).with_name('InventoryOpenControlGate.cpp').read_text()
        blocks = re.findall(r'const\s+std::set<std::string>\s+profiles\s*=\s*\{([^}]*)\};\s*require\(profiles.count\(profile\)==1\);', source)
        self.assertEqual(len(blocks), 1)
        accepted = tuple(ast.literal_eval(part.strip()) for part in blocks[0].split(','))
        self.assertEqual(accepted, wrapper.PROFILES)

    def test_both_masters_all_profiles_and_fresh_disposable_options(self):
        for master in ('xcom1', 'xcom2'):
            observed = []
            def runner(argv, **kw):
                profile = kw['env']['OPENXCOM_INVENTORY_OPEN_PROFILE']
                self.assertEqual(profile, wrapper.PROFILES[len(observed)])
                for _, old in observed:
                    self.assertFalse(old.exists())
                root = Path(argv[argv.index('-cfg') + 1]).parent
                self.assertNotIn(root, [old for _, old in observed])
                self.assertEqual(argv, ['/private/binary', '-master', master, '-data', '/private/data',
                                       '-user', str(root / 'user'), '-cfg', str(root / 'config')])
                self.assertTrue((root / 'user').is_dir())
                self.assertEqual((root / 'config/options.cfg').read_text(),
                                 f'mods:\n  - id: {master}\n    active: true\noptions:\n  language: en-US\n  playIntro: false\n')
                self.assertEqual(kw['timeout'], 240 if profile == 'core' else 45)
                self.assertIs(kw['stdout'], subprocess.PIPE)
                self.assertIs(kw['stderr'], subprocess.PIPE)
                self.assertIs(kw['text'], True)
                self.assertEqual(kw['errors'], 'replace')
                self.assertIs(kw['check'], False)
                self.assertEqual(kw['env']['SDL_AUDIODRIVER'], 'dummy')
                observed.append((profile, root))
                return self.result(profile, master)
            answer = self.run_gate(runner, master)
            self.assertEqual(answer, wrapper.SUITE_PASS.replace('MASTER', master))
            self.assertEqual([profile for profile, _ in observed], list(wrapper.PROFILES))
            self.assertTrue(all(not root.exists() for _, root in observed))

    def test_every_profile_has_exact_explicit_environment(self):
        def runner(argv, **kw):
            profile = kw['env']['OPENXCOM_INVENTORY_OPEN_PROFILE']
            expected = {'OPENXCOM_INVENTORY_OPEN_CONTROL_GATE': '1',
                        'OPENXCOM_INVENTORY_OPEN_PROFILE': profile,
                        'OPENXCOM_MAP_LEFT_CLICK_ENABLED': '1',
                        'OPENXCOM_SELECTION_ENABLED': '1'}
            if profile != 'disabled':
                expected['OPENXCOM_INVENTORY_ENABLED'] = '1'
            if profile != 'turn-disabled':
                expected['OPENXCOM_TURN_ENABLED'] = '1'
            self.assertEqual({key: value for key, value in kw['env'].items() if key.startswith('OPENXCOM_')}, expected)
            self.assertEqual(kw['env']['SDL_AUDIODRIVER'], 'dummy')
            self.assertEqual(kw['env']['UNRELATED_TEST_VALUE'], 'preserved')
            return self.result(profile)
        inherited = {'OPENXCOM_INVENTORY_ENABLED': '0', 'OPENXCOM_TURN_ENABLED': '0',
                     'OPENXCOM_STANCE_FD': 'secret', 'OPENXCOM_AUDIO_FD': '19',
                     'OPENXCOM_INVENTORY_OPEN_PROFILE': 'untrusted',
                     'OPENXCOM_INVENTORY_OPEN_CONTROL_GATE': '0',
                     'OPENXCOM_MAP_LEFT_CLICK_MODAL_PREVIEW': '1',
                     'OPENXCOM_TURN_CONTROL_GATE': '1', 'SDL_AUDIODRIVER': 'alsa',
                     'UNRELATED_TEST_VALUE': 'preserved'}
        with mock.patch.dict(os.environ, inherited):
            self.run_gate(runner)
            self.assertEqual(os.environ['OPENXCOM_STANCE_FD'], 'secret')
            self.assertEqual(os.environ['OPENXCOM_INVENTORY_ENABLED'], '0')

    def test_failed_profile_prevents_aggregate_and_later_processes(self):
        for failed in wrapper.PROFILES:
            observed = []
            def runner(argv, **kw):
                profile = kw['env']['OPENXCOM_INVENTORY_OPEN_PROFILE']
                observed.append(profile)
                return self.result(profile, rc=1 if profile == failed else 0)
            with self.subTest(failed=failed), self.assertRaisesRegex(RuntimeError, '^'+wrapper.FAIL+'$'):
                self.run_gate(runner)
            self.assertEqual(observed, list(wrapper.PROFILES[:wrapper.PROFILES.index(failed) + 1]))

    def test_duplicate_mixed_wrong_profile_master_and_malformed_verdicts_rejected(self):
        expected = self.result().stdout
        wrong_master = self.result(master='xcom2').stdout
        wrong_profile = self.result(profile='focus').stdout
        aggregate = wrapper.SUITE_PASS.replace('MASTER', 'xcom1')
        cases = [(expected + '\n' + expected, ''), (expected, expected),
                 (expected + '\n  ' + expected, ''), (expected, 'log: ' + wrong_profile),
                 (expected + '\n' + wrong_profile, ''), (expected, wrong_master),
                 (wrong_profile, ''), (wrong_master, ''), ('', expected),
                 (expected, 'INVENTORY_OPEN_CONTROL_GATE FAIL assertion-42'),
                 (expected + '\n' + aggregate, ''), (aggregate, ''),
                 (expected.replace('true', 'false'), ''),
                 (expected + ' secret/path', ''), (' ' + expected, ''),
                 (expected.replace('"passed":true', '"passed":true,"other":true'), ''),
                 (expected.replace('"master":"xcom1"', '"master":"xcom1","master":"xcom1"'), '')]
        for out, err in cases:
            with self.subTest(out=out, err=err), self.assertRaisesRegex(RuntimeError, '^'+wrapper.FAIL+'$'):
                self.run_gate(lambda *args, **kw: self.result(out=out, err=err))

    def test_child_text_is_never_forwarded_on_success(self):
        def runner(argv, **kw):
            expected = self.result(kw['env']['OPENXCOM_INVENTORY_OPEN_PROFILE']).stdout
            return self.result(out='secret/path\n' + expected + '\nprivate names', err='raw states')
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            answer = self.run_gate(runner)
        self.assertEqual(stdout.getvalue(), '')
        self.assertEqual(stderr.getvalue(), '')
        self.assertEqual(answer, wrapper.SUITE_PASS.replace('MASTER', 'xcom1'))

    def test_error_text_redacted_and_profile_removed(self):
        for error in (OSError('private/path'), subprocess.TimeoutExpired('private command', 1,
                       output='private stdout', stderr='private stderr'), RuntimeError('private log')):
            seen = []
            def runner(argv, **kw):
                seen.append(Path(argv[argv.index('-cfg') + 1]).parent)
                raise error
            with self.subTest(error=type(error).__name__), self.assertRaisesRegex(RuntimeError, '^'+wrapper.FAIL+'$'):
                self.run_gate(runner)
            self.assertEqual(len(seen), 1)
            self.assertFalse(seen[0].exists())

    def test_exactly_one_stderr_assertion_is_preserved_and_temporary_profile_removed(self):
        label = 'INVENTORY_OPEN_CONTROL_GATE FAIL assertion-42'
        roots = []
        def runner(argv, **kw):
            roots.append(Path(argv[argv.index('-cfg') + 1]).parent)
            return self.result(rc=1, out='private stdout', err='private/path\n' + label + '\nprivate state')
        with self.assertRaisesRegex(RuntimeError, '^' + label + '$'):
            self.run_gate(runner)
        self.assertEqual(len(roots), 1)
        self.assertFalse(roots[0].exists())

    def test_duplicate_malformed_stdout_and_mixed_assertions_are_not_forwarded(self):
        label = 'INVENTORY_OPEN_CONTROL_GATE FAIL assertion-42'
        second = 'INVENTORY_OPEN_CONTROL_GATE FAIL assertion-43'
        expected = self.result().stdout
        cases = [('', label + '\n' + label), ('', label + '\n' + second),
                 ('', label + ' secret/path'), ('', ' ' + label),
                 ('', label.replace('42', 'private/path')), (label, ''),
                 (label, label), (expected, label),
                 ('', label + '\n' + self.result(profile='focus').stdout),
                 ('', label + '\nINVENTORY_OPEN_CONTROL_GATE FAIL'),
                 ('', label + '\nlog: ' + second)]
        for out, err in cases:
            with self.subTest(out=out, err=err), self.assertRaisesRegex(RuntimeError, '^' + wrapper.FAIL + '$'):
                self.run_gate(lambda *args, **kw: self.result(rc=1, out=out, err=err))

    def test_main_emits_one_sanitized_assertion_and_suite_failure_only(self):
        label = 'INVENTORY_OPEN_CONTROL_GATE FAIL assertion-42'
        stdout, stderr = io.StringIO(), io.StringIO()
        def runner(argv, **kw):
            return self.result(rc=1, out='private/path', err='private state\n' + label)
        native_run = wrapper.run
        with mock.patch.object(wrapper, 'run', side_effect=lambda b, d, m: native_run(b, d, m, runner)), \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            self.assertEqual(wrapper.main(['/private/binary', '/private/data', 'xcom1']), 1)
        self.assertEqual(stdout.getvalue(), '')
        self.assertEqual(stderr.getvalue(), label + '\n' + wrapper.FAIL + '\n')

    def test_cleanup_on_rejected_verdict_in_late_profile(self):
        seen = []
        def runner(argv, **kw):
            seen.append(Path(argv[argv.index('-cfg') + 1]).parent)
            profile = kw['env']['OPENXCOM_INVENTORY_OPEN_PROFILE']
            return self.result(profile, out='private failure' if profile == 'debug' else None)
        with self.assertRaises(RuntimeError):
            self.run_gate(runner)
        self.assertEqual(len(seen), 14)
        self.assertTrue(all(not path.exists() for path in seen))

    def test_relative_paths_invalid_master_and_preview_rejected(self):
        for binary, data, master in (('binary', '/data', 'xcom1'), ('/binary', 'data', 'xcom2'),
                                     ('/binary', '/data', 'secret-master')):
            with self.assertRaises(ValueError):
                wrapper.run(Path(binary), Path(data), master, lambda *a, **kw: self.fail('must not launch'))
        stderr = io.StringIO()
        with mock.patch.object(wrapper, 'run') as run, contextlib.redirect_stderr(stderr):
            self.assertEqual(wrapper.main(['--preview-modal', '/binary', '/data', 'xcom1']), 2)
            run.assert_not_called()
        self.assertNotIn('PASS', stderr.getvalue())

    def test_main_has_only_one_exact_aggregate_after_all_profiles(self):
        for master in ('xcom1', 'xcom2'):
            observed = []
            stdout, stderr = io.StringIO(), io.StringIO()
            def runner(argv, **kw):
                self.assertEqual(stdout.getvalue(), '')
                self.assertEqual(stderr.getvalue(), '')
                profile = kw['env']['OPENXCOM_INVENTORY_OPEN_PROFILE']
                observed.append(profile)
                return self.result(profile, master)
            native_run = wrapper.run
            with mock.patch.object(wrapper, 'run', side_effect=lambda b, d, m: native_run(b, d, m, runner)), \
                    contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                self.assertEqual(wrapper.main(['/private/binary', '/private/data', master]), 0)
            self.assertEqual(observed, list(wrapper.PROFILES))
            self.assertEqual(stdout.getvalue(), wrapper.SUITE_PASS.replace('MASTER', master) + '\n')
            self.assertEqual(stderr.getvalue(), '')
            payload = json.loads(stdout.getvalue().split(' PASS ', 1)[1])
            self.assertIs(payload['runtime_gate_pass'], True)
            self.assertIs(payload['full_fairness'], False)

    def test_main_redacts_every_expected_error(self):
        for error, code in ((ValueError('private/path'), 2), (OSError('private/path'), 1),
                            (subprocess.TimeoutExpired('secret-command', 1), 1),
                            (RuntimeError('secret child text'), 1)):
            stdout, stderr = io.StringIO(), io.StringIO()
            with mock.patch.object(wrapper, 'run', side_effect=error), contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                self.assertEqual(wrapper.main(['/private/binary', '/private/data', 'xcom1']), code)
            self.assertEqual(stdout.getvalue(), '')
            self.assertEqual(stderr.getvalue(), wrapper.FAIL + '\n')

    def test_option_nesting_matches_native_loader(self):
        source = (Path(__file__).resolve().parents[2] / 'src/Engine/Options.cpp').read_text()
        self.assertIn('i->load(doc["options"])', source)
        self.assertIn('doc["mods"].begin()', source)


if __name__ == '__main__':
    unittest.main()
