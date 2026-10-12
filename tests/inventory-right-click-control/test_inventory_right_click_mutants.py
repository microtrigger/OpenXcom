# SPDX-License-Identifier: GPL-3.0-or-later
"""Pure mutation application and predeclared endpoint tests; no native evidence."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SPEC=importlib.util.spec_from_file_location('right_mutant',HERE/'make-inventory-right-click-mutant.py')
m=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(m)
MAP=json.loads((HERE/'fault-map.json').read_text())

class Mutants(unittest.TestCase):
    def tree(self,root):
        for relative in {v[0] for v in m.MUTATIONS.values()}:
            p=root/relative;p.parent.mkdir(parents=True,exist_ok=True);p.write_text((ROOT/relative).read_text())
        p=root/'tests/inventory-right-click-control/InventoryRightClickControlGate.cpp'
        p.parent.mkdir(parents=True);p.write_text((HERE/p.name).read_text())
    def call(self,args):
        out,err=io.StringIO(),io.StringIO()
        with contextlib.redirect_stdout(out),contextlib.redirect_stderr(err): rc=m.main(args)
        return rc,out.getvalue(),err.getvalue()
    def test_exact_map_and_finite_source_scope(self):
        self.assertEqual(set(MAP['faults']),set(m.MUTATIONS))
        self.assertEqual(len(m.MUTATIONS),25)
        source=(HERE/'InventoryRightClickControlGate.cpp').read_text()
        for name,(file,edits) in m.MUTATIONS.items():
            self.assertTrue(file.startswith('src/'))
            f=MAP['faults'][name]
            self.assertIn('"'+f['profile']+'"',source)
            self.assertRegex(source,r'\b[A-Z_]+='+str(f['assertion'])+r'\b')
            self.assertEqual(f['composite'],len(edits)>1)
            self.assertNotIn('tests/',file)
    def test_all_current_source_anchors_unique_mutated_once(self):
        for name,(file,edits) in m.MUTATIONS.items():
            with self.subTest(name=name):
                source=(ROOT/file).read_text()
                result=m.mutate(name,source)
                self.assertNotEqual(source,result)
                self.assertEqual(result.count(m.MARKER),1)
                for before,after in edits:
                    self.assertEqual(source.count(before),1)
                    self.assertIn(after,result)
                with self.assertRaises(ValueError):m.mutate(name,result)
                with self.assertRaises(ValueError):m.mutate(name,'')
                with self.assertRaises(ValueError):m.mutate(name,source+source)
    def test_unload_branches_survive_right_only_faults(self):
        source=(ROOT/'src/Observer/StanceController.cpp').read_text()
        kind=m.mutate('right-kind-confusion',source)
        self.assertNotIn('||  ||',kind)
        self.assertIn("(_contextKind == 'Q' && inventoryUnload)",kind)
        self.assertIn("!(((_contextKind == 'U' || _contextKind == 'H') && inventoryRightClick) || (_contextKind == 'Q'",kind)
        paired=m.mutate('paired-release-clickthrough',source)
        self.assertIn("if (owned && (false || _action == 'f') && event.type == SDL_MOUSEBUTTONDOWN)",paired)
        save=m.mutate('synchronous-save-notice-ignored',source)
        notice='    if (_inventoryUnloadDispatching) _inventoryUnloadUnexpected = true;'
        self.assertEqual(save.count(notice),source.count(notice))
        self.assertEqual(save.count('    if (_inventoryRightDispatching) _inventoryRightUnexpected = true;'),
                         source.count('    if (_inventoryRightDispatching) _inventoryRightUnexpected = true;')-1)
    def test_disposable_only_single_fault_and_no_oracle_edits(self):
        for name,(relative,_) in m.MUTATIONS.items():
            with tempfile.TemporaryDirectory(prefix='right-mutant-test-') as temp:
                root=Path(temp);self.tree(root)
                before={str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*') if p.is_file()}
                rc,out,err=self.call([name,str(root)])
                self.assertEqual((rc,err),(0,''));self.assertIn('APPLIED '+name+' source-sha256=',out)
                after={str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*') if p.is_file()}
                self.assertEqual([f for f in before if before[f]!=after[f]],[relative])
                rc,out,err=self.call([next(iter(m.MUTATIONS)),str(root)])
                self.assertEqual((rc,out,err),(1,'',m.FAIL+'\n'))
    def test_no_normal_checkout_relative_unknown_or_missing(self):
        for args in [[],['wrong','/tmp'],[next(iter(m.MUTATIONS)),str(ROOT)],[next(iter(m.MUTATIONS)),'.'],[next(iter(m.MUTATIONS)),'/nonexistent/private/path']]:
            self.assertEqual(self.call(args),(1,'',m.FAIL+'\n'))
    def test_symlink_and_hardlink_to_normal_are_refused(self):
        kind=next(iter(m.MUTATIONS));relative=m.MUTATIONS[kind][0]
        for hard in (False,True):
            with tempfile.TemporaryDirectory(prefix='right-alias-test-',dir=ROOT.parent) as temp:
                root=Path(temp);self.tree(root);p=root/relative;p.unlink()
                if hard:p.hardlink_to(ROOT/relative)
                else:p.symlink_to(ROOT/relative)
                self.assertEqual(self.call([kind,str(root)]),(1,'',m.FAIL+'\n'))
    def test_missing_duplicate_anchor_never_writes(self):
        kind=next(iter(m.MUTATIONS));relative=m.MUTATIONS[kind][0]
        for text in ('private',''.join([(ROOT/relative).read_text()]*2)):
            with tempfile.TemporaryDirectory(prefix='right-anchor-test-') as temp:
                root=Path(temp);self.tree(root);p=root/relative;p.write_text(text)
                self.assertEqual(self.call([kind,str(root)]),(1,'',m.FAIL+'\n'))
                self.assertEqual(p.read_text(),text)

if __name__=='__main__':unittest.main()
