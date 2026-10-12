#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Actual production-source mutation setup tests; no native fault kill claimed."""
import contextlib
from hashlib import sha256
import importlib.util
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('inventory_open_mutant', Path(__file__).with_name('make-inventory-open-mutant.py'))
mutant = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mutant)
ROOT = Path(__file__).resolve().parents[2]


class NativeMutations(unittest.TestCase):
    def make_copy(self, root, kind):
        gate = root / 'tests/inventory-open-control/InventoryOpenControlGate.cpp'
        gate.parent.mkdir(parents=True)
        gate.write_text('fixture marker, not a production mutation')
        target = root / mutant.MUTATIONS[kind][0]
        target.parent.mkdir(parents=True)
        target.write_text((ROOT / mutant.MUTATIONS[kind][0]).read_text())
        untouched = root / 'src/unchanged.cpp'
        untouched.write_text('unchanged production source')
        return target, gate, untouched

    def call_main(self, args):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = mutant.main(args)
        return code, stdout.getvalue(), stderr.getvalue()

    def assert_rejected(self, args):
        self.assertEqual(self.call_main(args), (1, '', mutant.FAIL + '\n'))

    def test_all_unique_real_production_anchors_change_and_reapplication_fails(self):
        self.assertEqual(len(mutant.MUTATIONS), 15)
        for name, (path, before, after) in mutant.MUTATIONS.items():
            with self.subTest(name=name):
                self.assertTrue(path.startswith('src/'))
                self.assertNotIn('Gate', path)
                self.assertNotIn('tests/', path)
                original = (ROOT / path).read_text()
                self.assertEqual(original.count(before), 1)
                changed = mutant.mutate(name, original)
                self.assertNotEqual(original, changed)
                self.assertIn(after, changed)
                self.assertEqual(changed.count(mutant.MARKER), 1)
                with self.assertRaises(ValueError):
                    mutant.mutate(name, changed)

    def test_only_declared_production_blocks_are_changed(self):
        for name, (path, before, after) in mutant.MUTATIONS.items():
            with self.subTest(name=name):
                original = (ROOT / path).read_text()
                expected = original.replace(before, '    // ' + mutant.MARKER + ' ' + name + '\n' + after, 1)
                for anchor, replacement in mutant.EXTRA_EDITS.get(name, ()):
                    expected = expected.replace(anchor, replacement, 1)
                changed = mutant.mutate(name, original)
                self.assertEqual(changed, expected)
                self.assertEqual(changed.count('InventoryOpenControlGate::'), original.count('InventoryOpenControlGate::'))
                self.assertEqual(changed.count('OPENXCOM_EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE'),
                                 original.count('OPENXCOM_EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE'))

    def test_faults_change_dispatch_cost_wire_and_barriers(self):
        edits = mutant.MUTATIONS
        self.assertEqual(edits['duplicate-open'][2].count('btnInventoryClick(0)'), 2)
        self.assertIn('btnKneelClick(0)', edits['wrong-handler'][2])
        self.assertNotIn('btnInventoryClick', edits['wrong-handler'][2])
        self.assertIn('spendTimeUnits(1)', edits['spend-TU'][2])
        self.assertIn('indicesHex[1] =', edits['wire-pixel'][2])
        self.assertIn('\\"capability\\"', edits['inventory-capability'][2])
        self.assertIn('_awaitingInventoryView = false', edits['B-barrier-bypass'][2])
        self.assertIn('_awaitingView = false', edits['I-clears-post-map-barrier'][2])
        self.assertIn('invalidate(true, false, false)', edits['pending-I-input-cancel'][2])
        self.assertIn('state->init()', edits['forced-init'][2])
        self.assertIn('+ 120000', edits['deadline-extension'][2])
        self.assertIn("_contextKind = 'I'", edits['deadline-extension'][1])
        self.assertNotIn('prior->second.request', edits['id-conflict-bypass'][2])
        for kind in ('stale-camera', 'stale-pose'):
            self.assertEqual(edits[kind][2], '        if (false) ++revision;\n')

    def test_unseen_modal_disables_both_actual_presentation_guards(self):
        original = (ROOT / mutant.CONTROLLER).read_text()
        changed = mutant.mutate('unseen-modal', original)
        self.assertIn('_confirmEventAllowed = !_yDown && event.key.keysym.mod == KMOD_NONE;', changed)
        self.assertIn('valid = accept && _confirmEventAllowed', changed)
        tactical = changed[changed.index('void StanceController::confirm(StanceConfirmState *'): ]
        self.assertNotIn('valid = accept && _dialogPresented', tactical)
        overlay = original[original.index('void StanceController::confirmInventoryClose('):original.index('void StanceController::confirm(StanceConfirmState *')]
        self.assertIn(overlay, changed)

    def test_missing_first_flip_wait_changes_only_pending_palette_allowance(self):
        path, before, after = mutant.MUTATIONS['missing-first-flip-palette-wait']
        self.assertEqual(path, 'src/Engine/Screen.cpp')
        self.assertIn('_firstColor == 0 && _numColors == 256', before)
        self.assertEqual(after, '        _observerPaletteExact &&\n')
        original = (ROOT / path).read_text()
        changed = mutant.mutate('missing-first-flip-palette-wait', original)
        method = original.index('bool Screen::inventoryWaitingAllowed() const')
        end = original.index('\n}\n', method) + 3
        self.assertTrue(changed.startswith(original[:method]))
        self.assertTrue(changed.endswith(original[end:]))
        # All strict final-presentation checks and ordinary display behavior stay
        # byte-for-byte unchanged outside this pending-I wait predicate.
        self.assertIn('_observerPaletteExact &&', changed[method:])

    def test_missing_ambiguous_and_extra_guard_anchors_are_refused(self):
        for name, (path, before, _) in mutant.MUTATIONS.items():
            original = (ROOT / path).read_text()
            for source in ('unrelated', original.replace(before, '', 1), original + before):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    mutant.mutate(name, source)
            for extra, _ in mutant.EXTRA_EDITS.get(name, ()):
                for source in (original.replace(extra, '', 1), original + extra):
                    with self.assertRaises(ValueError):
                        mutant.mutate(name, source)

    def test_normal_checkout_and_invalid_invocations_refused(self):
        for args in ([], ['duplicate-open'], ['missing-fault', str(ROOT)],
                     ['duplicate-open', 'relative'], ['duplicate-open', str(ROOT)],
                     ['duplicate-open', str(ROOT / 'tests')],
                     ['duplicate-open', '/private/nonexistent'], ['duplicate-open', str(ROOT), 'extra']):
            self.assert_rejected(args)

    def test_separate_disposable_copies_change_only_one_production_file(self):
        for name, (relative, _, _) in mutant.MUTATIONS.items():
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                target, gate, untouched = self.make_copy(root, name)
                before = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
                original = target.read_text()
                code, out, err = self.call_main([name, str(root)])
                self.assertEqual(code, 0)
                self.assertEqual(err, '')
                changed = mutant.mutate(name, original)
                self.assertEqual(out, 'INVENTORY_OPEN_RUNTIME_MUTANT APPLIED ' + name +
                                 ' source-sha256=' + sha256(changed.encode()).hexdigest() + '\n')
                after = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
                self.assertEqual(before.keys(), after.keys())
                self.assertEqual([path for path in before if before[path] != after[path]], [Path(relative)])
                self.assertEqual(target.read_text(), changed)
                self.assert_rejected([name, str(root)])
                self.assertEqual(target.read_text(), changed)

    def test_faults_cannot_be_combined_even_in_different_production_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target, _, _ = self.make_copy(root, 'duplicate-open')
            other = root / mutant.MUTATIONS['stale-camera'][0]
            other.parent.mkdir(parents=True, exist_ok=True)
            original_other = (ROOT / mutant.MUTATIONS['stale-camera'][0]).read_text()
            other.write_text(original_other)
            self.assertEqual(self.call_main(['duplicate-open', str(root)])[0], 0)
            self.assert_rejected(['stale-camera', str(root)])
            self.assertEqual(other.read_text(), original_other)

    def test_symlink_and_hardlink_to_normal_source_refused(self):
        for link_kind in ('symlink', 'hardlink'):
            # A sibling directory is on the same filesystem for the real hardlink.
            with self.subTest(link_kind=link_kind), tempfile.TemporaryDirectory(dir=ROOT.parent) as temporary:
                root = Path(temporary)
                target, _, _ = self.make_copy(root, 'duplicate-open')
                target.unlink()
                original = ROOT / mutant.CONTROLLER
                before = original.read_bytes()
                if link_kind == 'symlink':
                    target.symlink_to(original)
                else:
                    os.link(original, target)
                self.assert_rejected(['duplicate-open', str(root)])
                self.assertEqual(original.read_bytes(), before)

    def test_symlink_alias_of_normal_checkout_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            alias = Path(temporary) / 'alias'
            alias.symlink_to(ROOT, target_is_directory=True)
            self.assert_rejected(['duplicate-open', str(alias)])

    def test_filesystem_errors_do_not_leak_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_copy(root, 'duplicate-open')
            with mock.patch.object(Path, 'write_text', side_effect=OSError('secret/source/path')):
                self.assert_rejected(['duplicate-open', str(root)])


if __name__ == '__main__':
    unittest.main()
