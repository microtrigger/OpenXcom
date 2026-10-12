#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Actual-source mutation setup only; no native runtime kill is claimed here."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('map_left_click_mutant', Path(__file__).with_name('make-map-left-click-mutant.py'))
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
                self.assertEqual(changed.count('Map left-click sensitivity fault:'), 1)
                with self.assertRaises(ValueError):
                    mutant.mutate(name, changed)

    def test_faults_change_real_dispatch_cost_interruption_and_waiting(self):
        changes = mutant.MUTATIONS
        self.assertNotIn('mouseOver', changes['stale-selector'][2])
        self.assertIn('x += 32', changes['wrong-pixel'][2])
        self.assertEqual(changes['duplicate-click'][2].count('mapClick(&action)'), 2)
        self.assertIn('SDL_BUTTON_RIGHT', changes['wrong-button'][2])
        self.assertIn('spendTimeUnits(0)', changes['free-walk'][2])
        self.assertIn('spendEnergy(0)', changes['free-energy'][2])
        for name, original in (('reserve-bypass', 'checkReservedTU'),
                               ('contact-bypass', 'unitSpotted'),
                               ('reaction-bypass', 'checkReactionFire')):
            self.assertIn(original, changes[name][1])
            self.assertNotIn(original, changes[name][2])
        self.assertIn('unitOpensDoor(_unit, true, dir)', changes['door-right-cost'][2])
        self.assertNotIn('PATH_NONE', changes['preview-bypass'][2])
        self.assertIn('+ 1500', changes['short-busy-wait'][2])
        self.assertNotIn('prior->second.request', changes['id-conflict-bypass'][2])

    def test_scope_does_not_change_right_click_or_gate(self):
        for name, (path, _, _) in mutant.MUTATIONS.items():
            self.assertTrue(path.startswith('src/'))
            self.assertNotIn('Gate', path)
            if name in ('stale-selector', 'wrong-pixel', 'duplicate-click', 'wrong-button'):
                original = (ROOT / path).read_text()
                changed = mutant.mutate(name, original)
                before_left = original[:original.index('void BattlescapeState::mapLeftClickControlClick')]
                self.assertTrue(changed.startswith(before_left))
                self.assertEqual(changed.count('_battleGame->primaryAction(pos);'),
                                 original.count('_battleGame->primaryAction(pos);'))

    def test_both_presentation_guards_and_both_timeout_bounds_changed(self):
        source = (ROOT / 'src/Observer/StanceController.cpp').read_text()
        modal = mutant.mutate('unseen-modal', source)
        self.assertIn('valid = accept && _confirmEventAllowed', modal)
        timeout = mutant.mutate('timeout-disabled', source)
        self.assertIn('_challengeDeadline = _mapLeftClickDispatchTime + 800000;', timeout)
        self.assertNotIn('else if (now() - _mapLeftClickDispatchTime >= 8000)', timeout)

    def test_changed_or_ambiguous_anchors_refused(self):
        for name, (_, before, _) in mutant.MUTATIONS.items():
            for source in ('unrelated', before + before):
                with self.assertRaises(ValueError):
                    mutant.mutate(name, source)

    def test_normal_checkout_refused(self):
        out = io.StringIO()
        with contextlib.redirect_stderr(out):
            self.assertEqual(mutant.main(['stale-selector', str(ROOT)]), 1)
        self.assertEqual(out.getvalue(), 'MAP_LEFT_CLICK_RUNTIME_MUTANT INCOMPLETE source-or-arguments\n')

    def test_disposable_copy_applied_with_only_fixed_fingerprint(self):
        for name, (path, _, _) in mutant.MUTATIONS.items():
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                gate = root / 'tests/map-left-click-control/MapLeftClickControlGate.cpp'
                gate.parent.mkdir(parents=True)
                gate.write_text('fixture marker')
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                original = (ROOT / path).read_text()
                target.write_text(original)
                out = io.StringIO()
                with contextlib.redirect_stdout(out):
                    self.assertEqual(mutant.main([name, str(root)]), 0)
                self.assertRegex(out.getvalue(), '^MAP_LEFT_CLICK_RUNTIME_MUTANT APPLIED '+name+' source-sha256=[0-9a-f]{64}\n$')
                self.assertEqual(target.read_text(), mutant.mutate(name, original))
                self.assertEqual(gate.read_text(), 'fixture marker')


if __name__ == '__main__':
    unittest.main()
