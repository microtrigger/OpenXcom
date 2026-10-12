#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Wrapper unit checks. These do not launch a game or claim runtime coverage."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('turn_wrapper', Path(__file__).with_name('run-turn-control.py'))
wrapper = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(wrapper)


class Wrapper(unittest.TestCase):
    def result(self, master='xcom1', *, rc=0, out=None, err=''):
        return subprocess.CompletedProcess([], rc, wrapper.PASS.replace('MASTER', master) if out is None else out, err)

    def test_valid_both_masters_and_disposable_profile(self):
        for master in ('xcom1', 'xcom2'):
            observed = []
            def runner(argv, **kw):
                profile = Path(argv[argv.index('-cfg') + 1])
                self.assertTrue(profile.is_dir())
                self.assertEqual((profile / 'options.cfg').read_text(),
                                 f'mods:\n  - id: {master}\n    active: true\noptions:\n  language: en-US\n  playIntro: false\n')
                self.assertEqual(kw['env']['SDL_AUDIODRIVER'], 'dummy')
                self.assertEqual(kw['env']['OPENXCOM_TURN_CONTROL_GATE'], '1')
                self.assertEqual(kw['env']['OPENXCOM_TURN_ENABLED'], '1')
                self.assertEqual(kw['env']['OPENXCOM_SELECTION_ENABLED'], '1')
                self.assertNotIn('OPENXCOM_STANCE_CONTROL_GATE', kw['env'])
                self.assertNotIn('OPENXCOM_STANCE_FD', kw['env'])
                self.assertNotIn('OPENXCOM_TURN_MODAL_PREVIEW', kw['env'])
                self.assertEqual(kw['timeout'], 270)
                observed.append(profile)
                return self.result(master)
            with mock.patch.dict(os.environ, {'OPENXCOM_STANCE_FD': '999'}):
                self.assertEqual(wrapper.run(Path('/binary'), Path('/data'), master, runner), wrapper.PASS.replace('MASTER', master))
            self.assertFalse(observed[0].exists())

    def test_preview_is_explicit_and_not_a_runtime_pass(self):
        for master in ('xcom1', 'xcom2'):
            expected = wrapper.PREVIEW.replace('MASTER', master)
            def runner(argv, **kw):
                self.assertEqual(kw['env']['OPENXCOM_TURN_MODAL_PREVIEW'], '1')
                return subprocess.CompletedProcess([], 0, expected, '')
            self.assertEqual(wrapper.run(Path('/binary'), Path('/data'), master, runner, preview=True), expected)
            with self.assertRaisesRegex(RuntimeError, '^TURN_CONTROL_GATE FAIL$'):
                wrapper.run(Path('/binary'), Path('/data'), master,
                            lambda *a, **kw: subprocess.CompletedProcess([], 0, expected, ''))

    def test_option_nesting_matches_native_loader(self):
        source = (Path(__file__).resolve().parents[2] / 'src/Engine/Options.cpp').read_text()
        self.assertIn('i->load(doc["options"])', source)
        self.assertIn('doc["mods"].begin()', source)

    def test_no_raw_child_output(self):
        for result in (self.result(rc=1, err='secret/path unit/name raw/state'),
                       self.result(out='secret'),
                       self.result(out=wrapper.PASS.replace('MASTER', 'xcom1') * 2),
                       self.result('xcom2')):
            with self.assertRaisesRegex(RuntimeError, '^TURN_CONTROL_GATE FAIL$'):
                wrapper.run(Path('/binary'), Path('/data'), 'xcom1', lambda *a, **kw: result)

    def test_single_assertion_whitelist(self):
        bad = self.result(rc=1, err='hidden\nTURN_CONTROL_GATE FAIL assertion-42\nsecret')
        with self.assertRaisesRegex(RuntimeError, '^TURN_CONTROL_GATE FAIL assertion-42$'):
            wrapper.run(Path('/binary'), Path('/data'), 'xcom1', lambda *a, **kw: bad)

    def test_multiple_assertions_or_suffix_not_forwarded(self):
        for err in ('TURN_CONTROL_GATE FAIL assertion-42 secret',
                    'TURN_CONTROL_GATE FAIL assertion-42\nTURN_CONTROL_GATE FAIL assertion-43'):
            with self.assertRaisesRegex(RuntimeError, '^TURN_CONTROL_GATE FAIL$'):
                wrapper.run(Path('/binary'), Path('/data'), 'xcom1', lambda *a, **kw: self.result(rc=1, err=err))

    def test_relative_and_invalid_master_rejected(self):
        for b, d, m in (('binary', '/data', 'xcom1'), ('/binary', 'data', 'xcom2'), ('/binary', '/data', 'bad')):
            with self.assertRaises(ValueError):
                wrapper.run(Path(b), Path(d), m, lambda *a, **kw: self.fail('must not launch'))

    def test_main_redacts_subprocess_errors(self):
        for error in (OSError('private/path'), subprocess.TimeoutExpired('private_command', 1)):
            out = io.StringIO()
            with mock.patch.object(wrapper, 'run', side_effect=error), contextlib.redirect_stderr(out):
                self.assertEqual(wrapper.main(['/binary', '/data', 'xcom1']), 1)
            self.assertEqual(out.getvalue(), 'TURN_CONTROL_GATE FAIL\n')


if __name__ == '__main__':
    unittest.main()
