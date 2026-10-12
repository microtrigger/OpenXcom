#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Structural sensitivity setup only; these checks never launch a game."""
import contextlib
import importlib.util
import io
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location('selection_mutant', Path(__file__).with_name('make-selection-mutant.py'))
mutant = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mutant)
ROOT = Path(__file__).resolve().parents[2]


class NativeMutations(unittest.TestCase):
    def test_real_source_changes_and_reapplication_rejected(self):
        for name, (path, before, after) in mutant.MUTATIONS.items():
            with self.subTest(name=name):
                original = (ROOT / path).read_text()
                changed = mutant.mutate(name, original)
                self.assertNotEqual(original, changed)
                self.assertIn(after, changed)
                self.assertEqual(changed.count('Selection sensitivity fault:'), 1)
                with self.assertRaises(ValueError):
                    mutant.mutate(name, changed)

    def test_faults_touch_direction_dispatch_count_and_cost(self):
        self.assertIn('btnPrevSoldierClick(0)', mutant.MUTATIONS['next-as-previous'][2])
        self.assertIn('btnNextSoldierClick(0)', mutant.MUTATIONS['previous-as-next'][2])
        self.assertEqual(mutant.MUTATIONS['duplicate-next'][2].count('btnNextSoldierClick(0)'), 2)
        self.assertIn('spendTimeUnits(1)', mutant.MUTATIONS['selection-charges-tu'][2])

    def test_normal_checkout_refused(self):
        out = io.StringIO()
        with contextlib.redirect_stderr(out):
            self.assertEqual(mutant.main(['duplicate-next', str(ROOT)]), 1)
        self.assertEqual(out.getvalue(), 'SELECTION_RUNTIME_MUTANT INCOMPLETE source-or-arguments\n')


if __name__ == '__main__':
    unittest.main()
