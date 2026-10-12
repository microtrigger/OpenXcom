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

SPEC = importlib.util.spec_from_file_location('inventory_close_mutant', Path(__file__).with_name('make-inventory-close-mutant.py'))
mutant = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mutant)
ROOT = Path(__file__).resolve().parents[2]


class NativeMutations(unittest.TestCase):
    def make_copy(self, root, kind):
        gate = root / 'tests/inventory-close-control/InventoryCloseControlGate.cpp'
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
        self.assertEqual(len(mutant.MUTATIONS), 18)
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
                self.assertEqual(changed.count('InventoryCloseControlGate::'), original.count('InventoryCloseControlGate::'))
                self.assertEqual(changed.count('OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE'),
                                 original.count('OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE'))

    def test_faults_are_meaningful_actual_native_changes(self):
        edits = mutant.MUTATIONS
        self.assertEqual(edits['duplicate-close'][2].count('state->btnOkClick(0)'), 2)
        self.assertIn('_game->popState()', edits['direct-pop'][2])
        self.assertNotIn('btnOkClick', edits['direct-pop'][2])
        self.assertIn('init()', edits['cancel-init'][2])
        self.assertIn('_inv->arrangeGround()', edits['cancel-arrange'][2])
        self.assertNotIn('_inventoryCloseConsumeEvent', edits['dismiss-clickthrough'][2])
        self.assertNotIn('selectionRevision', edits['stale-actor'][2])
        self.assertNotIn('uiRevision', edits['stale-reinit'][2])
        self.assertNotIn('->applyGravity', edits['skip-gravity'][2])
        self.assertNotIn('->calculateTerrainLighting', edits['skip-lighting'][2])
        self.assertNotIn('->recalculateFOV', edits['skip-FOV'][2])
        self.assertNotIn('inventoryCloseRestored', edits['premature-V'][2])
        self.assertIn('_awaitingCloseView = _closeRestoring = false', edits['close-barrier-bypass'][2])
        self.assertIn('indicesHex[1] =', edits['wire-J-pixel'][2])
        self.assertNotIn('getSelectedItem', edits['held-item-grant'][2])
        self.assertNotIn('prior->second.request', edits['id-conflict-bypass'][2])

    def test_unseen_overlay_removes_both_close_presentation_guards(self):
        original = (ROOT / mutant.CONTROLLER).read_text()
        changed = mutant.mutate('unseen-overlay', original)
        self.assertIn('_confirmEventAllowed = !_yDown && event.key.keysym.mod == KMOD_NONE;', changed)
        self.assertIn('valid = accept && _confirmEventAllowed', changed)
        # The old separate State confirmation remains guarded.
        self.assertEqual(changed.count('valid = accept && _dialogPresented'), 1)

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
        for args in ([], ['duplicate-close'], ['missing-fault', str(ROOT)],
                     ['duplicate-close', 'relative'], ['duplicate-close', str(ROOT)],
                     ['duplicate-close', str(ROOT / 'tests')],
                     ['duplicate-close', '/private/nonexistent'], ['duplicate-close', str(ROOT), 'extra']):
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
                self.assertEqual(out, 'INVENTORY_CLOSE_RUNTIME_MUTANT APPLIED ' + name +
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
            target, _, _ = self.make_copy(root, 'duplicate-close')
            other = root / mutant.MUTATIONS['skip-gravity'][0]
            other.parent.mkdir(parents=True, exist_ok=True)
            original_other = (ROOT / mutant.MUTATIONS['skip-gravity'][0]).read_text()
            other.write_text(original_other)
            self.assertEqual(self.call_main(['duplicate-close', str(root)])[0], 0)
            self.assert_rejected(['skip-gravity', str(root)])
            self.assertEqual(other.read_text(), original_other)

    def test_symlink_and_hardlink_to_normal_source_refused(self):
        for link_kind in ('symlink', 'hardlink'):
            # A sibling directory is on the same filesystem for the real hardlink.
            with self.subTest(link_kind=link_kind), tempfile.TemporaryDirectory(dir=ROOT.parent) as temporary:
                root = Path(temporary)
                target, _, _ = self.make_copy(root, 'duplicate-close')
                target.unlink()
                original = ROOT / mutant.CONTROLLER
                before = original.read_bytes()
                if link_kind == 'symlink':
                    target.symlink_to(original)
                else:
                    os.link(original, target)
                self.assert_rejected(['duplicate-close', str(root)])
                self.assertEqual(original.read_bytes(), before)

    def test_symlink_alias_of_normal_checkout_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            alias = Path(temporary) / 'alias'
            alias.symlink_to(ROOT, target_is_directory=True)
            self.assert_rejected(['duplicate-close', str(alias)])

    def test_filesystem_errors_do_not_leak_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_copy(root, 'duplicate-close')
            with mock.patch.object(Path, 'write_text', side_effect=OSError('secret/source/path')):
                self.assert_rejected(['duplicate-close', str(root)])


if __name__ == '__main__':
    unittest.main()
