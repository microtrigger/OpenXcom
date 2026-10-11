#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
"""Wrapper unit tests with mock subprocess results; never renderer evidence."""
from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('smoke_clipping_wrapper', Path(__file__).with_name('run-smoke-clipping.py'))
wrapper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wrapper)


class WrapperTests(unittest.TestCase):
    def invoke(self, stdout='', stderr='', returncode=0, master='xcom1', error=None):
        output, errors = io.StringIO(), io.StringIO()
        profiles = []

        def run(args, **kwargs):
            profile = Path(args[args.index('-user') + 1]).parent
            profiles.append(profile)
            self.assertEqual(args, ['/test/openxcom', '-master', master, '-data', '/test/data',
                                    '-user', str(profile / 'user'), '-cfg', str(profile / 'config')])
            self.assertTrue((profile / 'user').is_dir())
            self.assertEqual((profile / 'config' / 'options.cfg').read_text(),
                             f'mods:\n  - id: {master}\n    active: true\noptions:\n  language: en-US\n  playIntro: false\n')
            self.assertEqual(kwargs['env']['OPENXCOM_SMOKE_CLIPPING_GATE'], '1')
            self.assertEqual(kwargs['env']['SDL_AUDIODRIVER'], 'dummy')
            self.assertEqual(kwargs['stdout'], subprocess.PIPE)
            self.assertEqual(kwargs['stderr'], subprocess.PIPE)
            self.assertEqual(kwargs['timeout'], 45)
            self.assertTrue(kwargs['text'])
            self.assertEqual(kwargs['errors'], 'replace')
            self.assertFalse(kwargs['check'])
            if error:
                raise error
            return subprocess.CompletedProcess(args, returncode, stdout, stderr)

        with patch.object(sys, 'argv', ['run', '/test/openxcom', '/test/data', master]), \
                patch.object(wrapper.subprocess, 'run', side_effect=run), \
                redirect_stdout(output), redirect_stderr(errors):
            result = wrapper.main()
        self.assertTrue(profiles)
        self.assertTrue(all(not path.exists() for path in profiles))
        return result, output.getvalue(), errors.getvalue()

    def test_pass_contract_is_exact(self):
        self.assertEqual(wrapper.PASS,
                         'SMOKE_CLIPPING_GATE PASS {"schema":"smoke-clipping-gate/1","master":"MASTER",'
                         '"scenarios":4,"frames_per_scenario":8,"equal_pair_frames":8,"responses":32,'
                         '"positive_phases":8,"audio":"subsystem-disabled","simulation":false,'
                         '"timing_equal":false,"full_fairness":false}')

    def test_exact_pass_both_masters_and_diagnostic_filter(self):
        for master in ('xcom1', 'xcom2'):
            with self.subTest(master=master):
                expected = wrapper.PASS.replace('MASTER', master)
                result, out, err = self.invoke('private-loader-info\n' + expected + '\n', 'private-error-info', master=master)
                self.assertEqual((result, out, err), (0, expected + '\n', ''))

    def test_failure_exit_cannot_pass(self):
        result, out, err = self.invoke(wrapper.PASS.replace('MASTER', 'xcom1'), 'private-error-info', 1)
        self.assertEqual((result, out, err), (1, '', 'SMOKE_CLIPPING_GATE FAIL\n'))

    def test_missing_duplicate_wrong_profile_and_forged_result(self):
        good = wrapper.PASS.replace('MASTER', 'xcom1')
        for text in ('', good + '\n' + good, wrapper.PASS.replace('MASTER', 'xcom2'),
                     good.replace('"responses":32', '"responses":31'),
                     good.replace('"scenarios":4', '"scenarios":3'),
                     good.replace('"schema":"smoke-clipping-gate/1"', '"schema":"smoke-density-gate/1"'),
                     good.replace('"simulation":false', '"simulation":true'),
                     good + ' private-suffix', 'private-prefix ' + good):
            with self.subTest(text=text):
                self.assertEqual(self.invoke(text), (1, '', 'SMOKE_CLIPPING_GATE FAIL\n'))

    def test_fixed_assertion_and_no_other_diagnostics(self):
        label = 'SMOKE_CLIPPING_GATE FAIL smoke-clipping-pair-public-transcript'
        result, out, err = self.invoke('', 'private-loader-info\n' + label + '\nprivate-error-info', 1)
        self.assertEqual((result, out, err), (1, '', label + '\nSMOKE_CLIPPING_GATE FAIL\n'))

    def test_malformed_failure_labels_stay_private(self):
        for label in ('SMOKE_CLIPPING_GATE FAIL private/path',
                      'SMOKE_CLIPPING_GATE FAIL private info',
                      'SMOKE_CLIPPING_GATE FAIL Private-info',
                      'SMOKE_CLIPPING_GATE FAIL ' + 'a' * 81,
                      'SMOKE_CLIPPING_GATE FAIL ',
                      'SMOKE_DENSITY_GATE FAIL smoke-pair-public-transcript'):
            with self.subTest(label=label):
                self.assertEqual(self.invoke('', label, 1), (1, '', 'SMOKE_CLIPPING_GATE FAIL\n'))

    def test_multiple_failure_labels_stay_private(self):
        label = 'SMOKE_CLIPPING_GATE FAIL smoke-clipping-pair-public-transcript'
        for labels in (label + '\n' + label,
                       label + '\nSMOKE_CLIPPING_GATE FAIL smoke-clipping-positive-phase'):
            with self.subTest(labels=labels):
                self.assertEqual(self.invoke('', labels, 1), (1, '', 'SMOKE_CLIPPING_GATE FAIL\n'))

    def test_stderr_pass_cannot_pass(self):
        self.assertEqual(self.invoke('', wrapper.PASS.replace('MASTER', 'xcom1')),
                         (1, '', 'SMOKE_CLIPPING_GATE FAIL\n'))

    def test_timeout_and_execution_failure_cleanup(self):
        for error in (subprocess.TimeoutExpired('test', 45), OSError('private-error-info')):
            with self.subTest(error=type(error).__name__):
                self.assertEqual(self.invoke(error=error), (1, '', 'SMOKE_CLIPPING_GATE FAIL\n'))

    def test_invalid_arguments_do_not_run(self):
        for args in (['run'], ['run', 'relative', '/data', 'xcom1'],
                     ['run', '/binary', 'relative', 'xcom1'],
                     ['run', '/binary', '/data', 'invalid'],
                     ['run', '/binary', '/data', 'XCOM1'],
                     ['run', '/binary', '/data', 'xcom1', 'extra']):
            with self.subTest(args=args), patch.object(sys, 'argv', args), \
                    patch.object(wrapper.subprocess, 'run') as run, redirect_stderr(io.StringIO()):
                self.assertEqual(wrapper.main(), 2)
                run.assert_not_called()


if __name__ == '__main__':
    unittest.main()
