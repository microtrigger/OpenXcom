# SPDX-License-Identifier: GPL-3.0-or-later
# Created 2026-10-10. See the OpenXcom project's LICENSE.txt for full terms.
import unittest
from usage_ledger import UsageLedger, extract_usage


def fake():
    # Synthetic test data ONLY, never measured usage evidence.
    return {'id':'never-persist-me', 'output':'private', 'usage':{'input_tokens':100,
            'input_tokens_details':{'cached_tokens':20}, 'output_tokens':30,
            'output_tokens_details':{'reasoning_tokens':10}, 'total_tokens':130}}


class LedgerTests(unittest.TestCase):
    def test_counts_and_no_double_count(self):
        l=UsageLedger(); l.finish(l.begin(), fake()); l.finish(l.begin(), fake())
        r=l.report(); self.assertTrue(r['usage_complete'])
        self.assertEqual(r['totals']['total_tokens'],260)
        self.assertEqual(r['totals']['cached_tokens'],40)
        self.assertEqual(r['totals']['reasoning_tokens'],20)
        self.assertNotIn('never-persist-me',str(r)); self.assertNotIn('private',str(r))
    def test_unknown_not_zero(self):
        l=UsageLedger(); l.finish(l.begin(),fake()); l.finish(l.begin(),status='failed')
        self.assertFalse(l.report()['usage_complete']); self.assertIsNone(l.report()['totals']['input_tokens'])
    def test_pending_and_empty_fail_gate(self):
        l=UsageLedger(); self.assertFalse(l.report()['usage_complete'])
        l.begin(); self.assertFalse(l.report()['usage_complete'])
    def test_mandatory_cached(self):
        f=fake(); f['usage'].pop('input_tokens_details'); l=UsageLedger(); l.finish(l.begin(),f)
        self.assertFalse(l.report()['usage_complete'])
    def test_optional_reasoning_stays_unknown(self):
        f=fake(); f['usage'].pop('output_tokens_details'); l=UsageLedger(); l.finish(l.begin(),f)
        self.assertTrue(l.report()['usage_complete']); self.assertIsNone(l.report()['totals']['reasoning_tokens'])
    def test_invalid_usage(self):
        for key,value in [('input_tokens',True),('total_tokens',131),('output_tokens',-1)]:
            f=fake(); f['usage'][key]=value
            with self.assertRaises(ValueError): extract_usage(f)
    def test_duplicate_finish(self):
        l=UsageLedger(); i=l.begin(); l.finish(i,fake())
        with self.assertRaises(ValueError): l.finish(i,fake())

if __name__=='__main__': unittest.main()
