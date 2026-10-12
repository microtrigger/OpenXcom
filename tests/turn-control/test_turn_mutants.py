#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Mutation setup only: does not launch native games or claim runtime kills."""
import contextlib
import importlib.util
import io
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location('turn_mutant', Path(__file__).with_name('make-turn-mutant.py'))
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
                self.assertEqual(changed.count('Turn sensitivity fault:'), 1)
                with self.assertRaises(ValueError):
                    mutant.mutate(name, changed)

    def test_faults_cover_selector_pixel_count_tu_reserve_generations_modal(self):
        changes = mutant.MUTATIONS
        self.assertNotIn('mouseOver', changes['stale-selector'][2])
        self.assertIn('x += 32', changes['wrong-pixel'][2])
        self.assertEqual(changes['duplicate-turn'][2].count('secondaryAction(pos)'), 2)
        self.assertIn('const int tu = 0', changes['free-turn'][2])
        self.assertNotIn('checkReservedTU', changes['reserve-bypass'][2])
        for kind in ('camera-generation', 'pose-generation'):
            self.assertIn('if (false) ++revision', changes[kind][2])
        self.assertIn('indicesHex[1]', changes['wire-pixel'][2])
        self.assertIn('canonicalView(rgb, indicesHex, paletteHex)', changes['wire-pixel'][2])
        self.assertNotIn('_dialogPresented', changes['unseen-modal'][2])
        text = mutant.mutate('unseen-modal', (ROOT / changes['unseen-modal'][0]).read_text())
        self.assertIn('valid = accept && _confirmEventAllowed', text)

    def test_changed_or_ambiguous_anchors_refused(self):
        for name, (_, before, _) in mutant.MUTATIONS.items():
            for source in ('unrelated', before + before):
                with self.assertRaises(ValueError):
                    mutant.mutate(name, source)

    def test_normal_checkout_refused(self):
        out = io.StringIO()
        with contextlib.redirect_stderr(out):
            self.assertEqual(mutant.main(['stale-selector', str(ROOT)]), 1)
        self.assertEqual(out.getvalue(), 'TURN_RUNTIME_MUTANT INCOMPLETE source-or-arguments\n')


if __name__ == '__main__':
    unittest.main()
