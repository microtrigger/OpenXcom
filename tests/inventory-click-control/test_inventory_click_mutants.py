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

SPEC = importlib.util.spec_from_file_location('inventory_click_mutant', Path(__file__).with_name('make-inventory-click-mutant.py'))
mutant = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mutant)
ROOT = Path(__file__).resolve().parents[2]


class NativeMutations(unittest.TestCase):
    def make_copy(self, root, kind):
        gate = root / 'tests/inventory-click-control/InventoryClickControlGate.cpp'
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
        self.assertEqual(len(mutant.MUTATIONS), 35)
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
                self.assertEqual(changed.count('InventoryClickControlGate::'), original.count('InventoryClickControlGate::'))
                self.assertEqual(changed.count('OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE'),
                                 original.count('OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE'))

    def test_faults_are_meaningful_actual_native_changes(self):
        edits = mutant.MUTATIONS
        self.assertEqual(edits['duplicate-click'][2].count('_inv->mouseClick(&action, this)'), 2)
        self.assertNotIn('mouseClick', edits['missing-click'][2])
        self.assertNotIn('mouseOver', edits['missing-hover'][2])
        self.assertIn('setMouseAction(160, 80', edits['wrong-hover'][2])
        self.assertIn('_awaitingInventoryView = false', edits['click-barrier-bypass'][2])
        self.assertIn('sendPacket(packet), true', edits['failed-send-barrier-bypass'][2])
        self.assertEqual(len(mutant.EXTRA_EDITS['unseen-overlay']), 2)

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
        for args in ([], ['duplicate-click'], ['missing-fault', str(ROOT)],
                     ['duplicate-click', 'relative'], ['duplicate-click', str(ROOT)],
                     ['duplicate-click', str(ROOT / 'tests')],
                     ['duplicate-click', '/private/nonexistent'], ['duplicate-click', str(ROOT), 'extra']):
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
                self.assertEqual(out, 'INVENTORY_CLICK_RUNTIME_MUTANT APPLIED ' + name +
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
            target, _, _ = self.make_copy(root, 'duplicate-click')
            other = root / mutant.MUTATIONS['wire-H-pixel'][0]
            other.parent.mkdir(parents=True, exist_ok=True)
            original_other = (ROOT / mutant.MUTATIONS['wire-H-pixel'][0]).read_text()
            other.write_text(original_other)
            self.assertEqual(self.call_main(['duplicate-click', str(root)])[0], 0)
            self.assert_rejected(['wire-H-pixel', str(root)])
            self.assertEqual(other.read_text(), original_other)

    def test_symlink_and_hardlink_to_normal_source_refused(self):
        for link_kind in ('symlink', 'hardlink'):
            # A sibling directory is on the same filesystem for the real hardlink.
            with self.subTest(link_kind=link_kind), tempfile.TemporaryDirectory(dir=ROOT.parent) as temporary:
                root = Path(temporary)
                target, _, _ = self.make_copy(root, 'duplicate-click')
                target.unlink()
                original = ROOT / mutant.MUTATIONS['duplicate-click'][0]
                before = original.read_bytes()
                if link_kind == 'symlink':
                    target.symlink_to(original)
                else:
                    os.link(original, target)
                self.assert_rejected(['duplicate-click', str(root)])
                self.assertEqual(original.read_bytes(), before)

    def test_symlink_alias_of_normal_checkout_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            alias = Path(temporary) / 'alias'
            alias.symlink_to(ROOT, target_is_directory=True)
            self.assert_rejected(['duplicate-click', str(alias)])

    def test_filesystem_errors_do_not_leak_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_copy(root, 'duplicate-click')
            with mock.patch.object(Path, 'write_text', side_effect=OSError('secret/source/path')):
                self.assert_rejected(['duplicate-click', str(root)])


if __name__ == '__main__':
    unittest.main()
