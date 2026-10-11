#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
"""Structural sensitivity setup checks; not substitutes for real GUI mutant runs."""
import contextlib
import importlib.util
import io
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location('stance_mutant', Path(__file__).with_name('make-runtime-mutant.py'))
mutant = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mutant)
ROOT = Path(__file__).resolve().parents[2]


class NativeMutations(unittest.TestCase):
    def test_each_changes_exact_real_source_and_refuses_reapplication(self):
        for name, (path, before, after) in mutant.MUTATIONS.items():
            with self.subTest(name=name):
                original = (ROOT / path).read_text()
                changed = mutant.mutate(name, original)
                self.assertNotEqual(original, changed)
                self.assertIn(after, changed)
                self.assertEqual(changed.count('Sensitivity fault:'), 1)
                with self.assertRaises(ValueError):
                    mutant.mutate(name, changed)

    def test_faults_are_actual_calls_cost_and_queue(self):
        self.assertIn('setSelectedUnit', mutant.MUTATIONS['wrong-actor'][2])
        self.assertEqual(mutant.MUTATIONS['duplicate'][2].count('btnKneelClick(0)'), 2)
        self.assertIn('isKneeled()?7:3', mutant.MUTATIONS['wrong-tu'][2])
        self.assertNotIn('checkReactionFire', mutant.MUTATIONS['reaction-bypass'][2])
        self.assertNotIn('_dialogPresented', mutant.MUTATIONS['unseen-modal'][2])

    def test_normal_checkout_refused(self):
        out = io.StringIO()
        with contextlib.redirect_stderr(out):
            self.assertEqual(mutant.main(['duplicate', str(ROOT)]), 1)
        self.assertEqual(out.getvalue(), 'STANCE_RUNTIME_MUTANT INCOMPLETE source-or-arguments\n')


if __name__ == '__main__':
    unittest.main()
