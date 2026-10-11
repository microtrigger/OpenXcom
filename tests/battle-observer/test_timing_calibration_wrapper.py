#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
"""Pure parser/statistics and fake-process checks, not renderer measurements."""
from contextlib import redirect_stderr, redirect_stdout
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('timing_wrapper', Path(__file__).with_name('run-timing-calibration.py'))
wrapper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wrapper)


def records(mode='normal', differences=None, post_differences=None):
    """Build synthetic durations; the fake process never runs game code."""
    result = []
    now = 10
    for pair, (kind, order) in enumerate(wrapper.schedule(mode)):
        diff = (differences or {}).get(pair, 0)
        post_diff = (post_differences or differences or {}).get(pair, 0)
        # Large enough for deliberately negative fake differences in these tests.
        for leg, label in enumerate(order):
            total = 20_000_000 + (diff if label == 'A' else 0)
            post = 10_000_000 + (post_diff if label == 'A' else 0)
            result.append({'type': 'sample', 'index': pair * 2 + leg, 'pair': pair,
                           'kind': kind, 'order': order, 'label': label,
                           't0_ns': now, 'flip_ns': now + total - post, 't1_ns': now + total})
            now += total + wrapper.MIN_SPACING_NS
        result.append({'type': 'pair', 'pair': pair})
    return result


def encoded(items):
    return b''.join(json.dumps(item, separators=(',', ':')).encode() + b'\n' for item in items)


class RecordTests(unittest.TestCase):
    def test_exact_counts_and_fixed_schedule(self):
        # Independent known prefix locks the seed, shifts and parity mapping.
        self.assertEqual(''.join(order[0] for _, order in wrapper.schedule('normal')[:12]),
                         'BABABABAAABB')
        for mode, expected in (('normal', 56), ('mutant', 20)):
            parsed = wrapper.parse_records(encoded(records(mode)), mode)
            self.assertEqual(len(parsed['pairs']), expected)
            self.assertEqual(len(parsed['samples']), 2 * expected)
            self.assertEqual(wrapper.schedule(mode)[:4], wrapper.schedule('normal')[:4])

    def assert_invalid(self, items, mode='normal'):
        with self.assertRaises(wrapper.InvalidRecords):
            wrapper.parse_records(encoded(items), mode)

    def test_required_keys_types_and_no_extra_fields(self):
        for key in wrapper.SAMPLE_KEYS:
            items = records()[:3]
            del items[0][key]
            self.assert_invalid(items)
        for field, value in (('extra', 'private-state'), ('index', True), ('pair', False),
                             ('t0_ns', 1.0), ('flip_ns', '123'), ('t1_ns', None)):
            items = records()[:3]
            items[0][field] = value
            self.assert_invalid(items)
        for marker in ({'type': 'pair', 'pair': True}, {'type': 'pair', 'pair': 1},
                       {'type': 'pair', 'pair': 0, 'extra': 1}):
            self.assert_invalid(records()[:2] + [marker])

    def test_sequence_kind_label_order_and_marker_are_strict(self):
        for field, value in (('index', 1), ('pair', 1), ('kind', 'aa'),
                             ('order', 'AA'), ('label', 'X'), ('type', 'anything')):
            items = records()[:3]
            items[0][field] = value
            self.assert_invalid(items)
        items = records()[:3]
        items[0]['order'] = items[1]['order'] = 'AB' if items[0]['order'] == 'BA' else 'BA'
        items[0]['label'], items[1]['label'] = items[1]['label'], items[0]['label']
        self.assert_invalid(items)
        for items in ([records()[2]], records()[0:1] + records()[2:3],
                      records()[0:2] + records()[3:4],
                      records()[0:3] + records()[0:3], list(reversed(records()[:3]))):
            self.assert_invalid(items)

    def test_positive_global_timestamp_order_and_profile_cap(self):
        for key, value in (('t0_ns', 0), ('t0_ns', -1),
                           ('flip_ns', records()[0]['t0_ns']),
                           ('t1_ns', records()[0]['flip_ns']),
                           ('t1_ns', 120_000_000_001), ('t1_ns', 2 ** 80)):
            items = records()[:3]
            items[0][key] = value
            self.assert_invalid(items)
        items = records()[:3]
        items[1]['t0_ns'] = items[0]['t1_ns']
        self.assert_invalid(items)
        items = records('mutant')[:3]
        items[0]['t1_ns'] = 90_000_000_001
        self.assert_invalid(items, 'mutant')

    def test_spacing_boundary_is_checked_across_all_observed_samples(self):
        valid = records()[:6]
        self.assertEqual(valid[1]['t0_ns'] - valid[0]['t1_ns'], 300_000_000)
        wrapper.parse_records(encoded(valid), 'normal')
        for index in (1, 3):
            invalid = copy.deepcopy(valid)
            for key in ('t0_ns', 'flip_ns', 't1_ns'):
                invalid[index][key] -= 1
            self.assert_invalid(invalid)

    def test_invalid_json_duplicate_keys_nonfinite_utf8_and_framing(self):
        invalid = (b'\n', b'{}\n', b'[]\n', b'null\n', b'{\n', b'\xff\n',
                   b'{"type":"pair","type":"sample","pair":0}\n',
                   b'{"type":"sample","index":NaN}\n', b'x' * (wrapper.RECORD_LIMIT + 1))
        for raw in invalid:
            with self.subTest(raw=raw[:40]), self.assertRaises(wrapper.InvalidRecords):
                wrapper.parse_records(raw, 'normal')
        with self.assertRaises(wrapper.InvalidRecords) as error:
            wrapper.parse_records(encoded(records()[:3]) + b'{', 'normal')
        self.assertEqual(len(error.exception.parsed['pairs']), 1)

    def test_pair_counts_never_exceed_plan(self):
        self.assert_invalid(records() + [{'type': 'pair', 'pair': 56}])
        self.assert_invalid(records('normal'), 'mutant')

    def test_unfinished_halves_remain_observed_not_aggregated(self):
        for trailing in (0, 1, 2):
            parsed = wrapper.parse_records(encoded(records()[:3 + trailing]), 'normal')
            summary = wrapper.aggregate(parsed, 'normal')
            self.assertEqual(summary['observed_samples'], 2 + trailing)
            self.assertEqual(summary['completed_pairs'], 1)
            self.assertEqual(summary['unpaired_samples'], trailing)
            self.assertNotIn('a_minus_b', summary['kinds']['warmup'])
        self.assertEqual(wrapper.parse_records(b'', 'normal'), {'samples': [], 'pairs': []})


class StatisticsTests(unittest.TestCase):
    def test_descriptive_values_signs_and_even_median(self):
        self.assertEqual(wrapper.describe([-8, 0, 2, 10]),
                         {'count': 4, 'mean': 1.0, 'median': 1.0, 'min': -8, 'max': 10,
                          'sign_counts': {'negative': 1, 'zero': 1, 'positive': 2}})
        self.assertEqual(wrapper.describe([1, 2])['median'], 1.5)
        self.assertEqual(wrapper.describe([7])['median'], 7)
        self.assertEqual(wrapper.describe([]),
                         {'count': 0, 'mean': None, 'median': None, 'min': None, 'max': None,
                          'sign_counts': {'negative': 0, 'zero': 0, 'positive': 0}})

    def test_no_outlier_trimming(self):
        stats = wrapper.describe([-9_000_000, 1, 1, 1, 9_000_003])
        self.assertEqual((stats['count'], stats['mean'], stats['median'], stats['min'], stats['max']),
                         (5, 1.2, 1, -9_000_000, 9_000_003))

    def test_sign_is_always_a_minus_b_regardless_of_order(self):
        differences = {pair: (pair + 1) * (-1 if pair % 2 else 1) for pair in range(56)}
        parsed = wrapper.parse_records(encoded(records(differences=differences)), 'normal')
        summary = wrapper.aggregate(parsed, 'normal')
        for kind in ('warmup', 'aa', 'ab'):
            group = summary['kinds'][kind]
            wanted = [differences[pair] for pair, (item_kind, _) in enumerate(wrapper.schedule('normal'))
                      if item_kind == kind]
            if kind != 'warmup':
                for metric in wrapper.METRICS:
                    self.assertEqual(group['a_minus_b'][metric], wrapper.describe(wanted))
            else:
                self.assertNotIn('a_minus_b', group)
            self.assertEqual(sum(group['order_counts'].values()), group['completed_pairs'])

    def mutant_summary(self, differences=None, post_differences=None):
        if differences is None:
            differences = {pair: 3_000_000 for pair in range(4, 20)}
        return wrapper.aggregate(wrapper.parse_records(
            encoded(records('mutant', differences, post_differences)), 'mutant'), 'mutant')

    def test_control_requires_complete_both_metrics_and_exact_thresholds(self):
        good = self.mutant_summary()
        self.assertTrue(wrapper.control(good, 'complete')['detectable_control'])
        for status in ('incomplete', 'not-run'):
            self.assertEqual(wrapper.control(good, status),
                             {'detectable_control': False, 'result': 'inconclusive'})
        for metric in wrapper.METRICS:
            for key, value in (('median', 2_999_999), ('sign_counts', {'positive': 13, 'zero': 3, 'negative': 0})):
                bad = copy.deepcopy(good)
                bad['kinds']['ab']['a_minus_b'][metric][key] = value
                self.assertFalse(wrapper.control(bad, 'complete')['detectable_control'])
        bad = copy.deepcopy(good)
        bad['kinds']['ab']['completed_pairs'] = 15
        self.assertFalse(wrapper.control(bad, 'complete')['detectable_control'])
        differences = {pair: 3_000_000 if pair >= 6 else 0 for pair in range(4, 20)}
        self.assertTrue(wrapper.control(self.mutant_summary(differences), 'complete')['detectable_control'])
        differences[6] = 0
        self.assertFalse(wrapper.control(self.mutant_summary(differences), 'complete')['detectable_control'])

    def test_two_metrics_use_distinct_timestamp_intervals(self):
        round_trip = {pair: 5_000_000 for pair in range(4, 20)}
        post_flip = {pair: 1_000_000 for pair in range(4, 20)}
        summary = self.mutant_summary(round_trip, post_flip)
        measured = summary['kinds']['ab']['a_minus_b']
        self.assertEqual(measured['round_trip_ns']['median'], 5_000_000)
        self.assertEqual(measured['post_flip_ns']['median'], 1_000_000)
        self.assertFalse(wrapper.control(summary, 'complete')['detectable_control'])

    def test_warmups_do_not_satisfy_measured_control(self):
        only_warmup = self.mutant_summary({pair: 8_000_000 for pair in range(4)})
        self.assertFalse(wrapper.control(only_warmup, 'complete')['detectable_control'])


class MarkerTests(unittest.TestCase):
    def test_private_diagnostics_and_exact_marker(self):
        self.assertEqual(wrapper.inspect_markers(b'private-path\n' + wrapper.COMPLETE, b'private-data\n'), (1, 0))
        self.assertEqual(wrapper.inspect_markers(b'', b'TIMING_CALIBRATION INCOMPLETE test-label\n'), (0, 1))
        self.assertEqual(wrapper.inspect_markers(b'', b''), (0, 0))

    def test_forged_duplicate_wrong_channel_and_bounded_markers(self):
        for stdout, stderr in ((wrapper.COMPLETE * 2, b''), (wrapper.COMPLETE.rstrip(), b''),
                               (b'TIMING_CALIBRATION COMPLETE extra\n', b''),
                               (b'', wrapper.COMPLETE),
                               (wrapper.COMPLETE, b'TIMING_CALIBRATION INCOMPLETE fail\n'),
                               (b'', b'TIMING_CALIBRATION INCOMPLETE path/secret\n'),
                               (b'', b'TIMING_CALIBRATION INCOMPLETE ' + b'a' * 81 + b'\n'),
                               (b'', b'TIMING_CALIBRATION INCOMPLETE fail\n' * 2),
                               (b'x' * (wrapper.DIAGNOSTIC_LIMIT + 1), b'')):
            with self.subTest(stdout=stdout[:60], stderr=stderr[:60]):
                self.assertIsNone(wrapper.inspect_markers(stdout, stderr))


class ProcessTests(unittest.TestCase):
    def invoke(self, actions=None, clock=None):
        actions = actions or {}
        output, errors = io.StringIO(), io.StringIO()
        calls, profiles = [], []
        temporary = tempfile.TemporaryDirectory(prefix='timing-wrapper-unit-')
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name) / 'private'

        def run(args, **kwargs):
            number = len(calls)
            calls.append((args, kwargs))
            mode, master = wrapper.PROFILES[number]
            self.assertEqual(args[0], '/private/' + mode)
            self.assertEqual(args[args.index('-master') + 1], master)
            profile = Path(args[args.index('-user') + 1]).parent
            profiles.append(profile)
            self.assertEqual((profile / 'config' / 'options.cfg').read_text(),
                             f'mods:\n  - id: {master}\n    active: true\noptions:\n  language: en-US\n  playIntro: false\n')
            self.assertEqual(stat.S_IMODE(root.stat().st_mode), 0o700)
            self.assertEqual(kwargs['env']['SDL_AUDIODRIVER'], 'dummy')
            self.assertEqual(kwargs['env']['OPENXCOM_TIMING_CALIBRATION'], mode)
            fd = int(kwargs['env']['OPENXCOM_TIMING_RECORD_FD'])
            self.assertEqual(kwargs['pass_fds'], (fd,))
            self.assertEqual(stat.S_IMODE(os.fstat(fd).st_mode), 0o600)
            for stream in (kwargs['stdout'], kwargs['stderr']):
                self.assertEqual(stat.S_IMODE(os.fstat(stream.fileno()).st_mode), 0o600)
            self.assertGreater(kwargs['timeout'], 0)
            self.assertLessEqual(kwargs['timeout'], wrapper.PROFILE_SECONDS[mode])
            action = actions.get(number, {})
            raw = action.get('raw', encoded(records(mode)))
            os.write(fd, raw)
            kwargs['stdout'].write(action.get('stdout', b'private-host-path\n' + wrapper.COMPLETE))
            kwargs['stderr'].write(action.get('stderr', b'private-secret\n'))
            if action.get('error'):
                raise action['error']
            return subprocess.CompletedProcess(args, action.get('returncode', 0))

        with patch.object(wrapper.subprocess, 'run', side_effect=run), \
                patch.object(wrapper.time, 'monotonic', side_effect=clock or (lambda: 100.0)), \
                redirect_stdout(output), redirect_stderr(errors):
            result = wrapper.main(['/private/normal', '/private/mutant', '/private/data', str(root)])
        self.assertTrue(all(not profile.exists() for profile in profiles))
        self.assertEqual(errors.getvalue(), '')
        self.assertEqual(len(output.getvalue().splitlines()), 1)
        self.assertNotIn('private-secret', output.getvalue())
        self.assertNotIn('private-host-path', output.getvalue())
        self.assertNotIn(str(root), output.getvalue())
        return result, json.loads(output.getvalue()), calls, root

    def test_four_profiles_exact_order_private_files_and_cleanup(self):
        result, value, calls, root = self.invoke()
        self.assertEqual(result, 0)
        self.assertEqual(value['status'], 'complete')
        self.assertFalse(value['timing_equivalence_claimed'])
        self.assertEqual(len(calls), 4)
        self.assertEqual(len(list(root.iterdir())), 12)
        for mode, master in wrapper.PROFILES:
            raw = root / (mode + '-' + master + '.records.jsonl')
            self.assertEqual(raw.read_bytes(), encoded(records(mode)))
            self.assertEqual(stat.S_IMODE(raw.stat().st_mode), 0o600)
        self.assertTrue(all(profile['status'] == 'complete' for profile in value['profiles']))
        self.assertTrue(all(profile['control']['result'] == 'inconclusive'
                            for profile in value['profiles'] if profile['mode'] == 'mutant'))

    def test_fake_complete_mutants_can_meet_fixed_control_without_affecting_normal(self):
        delay = {pair: 5_000_000 for pair in range(4, 20)}
        raw = encoded(records('mutant', delay))
        result, value, calls, _ = self.invoke({2: {'raw': raw}, 3: {'raw': raw}})
        self.assertEqual((result, len(calls)), (0, 4))
        for profile in value['profiles'][:2]:
            self.assertIsNone(profile['control'])
        for profile in value['profiles'][2:]:
            self.assertEqual(profile['control'], {'detectable_control': True, 'result': 'detectable-control'})
            self.assertEqual(profile['counts_and_statistics']['remaining_not_run_samples'], 0)

    def test_incomplete_profile_keeps_pairs_and_runs_later_profiles_once(self):
        raw = encoded(records()[:4])
        result, value, calls, root = self.invoke({0: {'raw': raw, 'stdout': b'', 'returncode': 1,
            'stderr': b'private-secret\nTIMING_CALIBRATION INCOMPLETE test-label\n'}})
        self.assertEqual(result, 1)
        self.assertEqual(len(calls), 4)
        first = value['profiles'][0]
        self.assertEqual(first['reason'], 'native-exit')
        self.assertEqual(first['counts_and_statistics']['completed_pairs'], 1)
        self.assertEqual(first['counts_and_statistics']['unpaired_samples'], 1)
        self.assertEqual(first['counts_and_statistics']['expected_samples'], 112)
        self.assertEqual(first['counts_and_statistics']['remaining_not_run_samples'], 109)
        self.assertEqual(first['counts_and_statistics']['unvalidated_or_unrun_samples'], 0)
        self.assertEqual((root / 'normal-xcom1.records.jsonl').read_bytes(), raw)

    def test_timeout_and_execution_failure_are_not_retried(self):
        for error, reason in ((subprocess.TimeoutExpired('private-command', 120), 'timeout'),
                              (OSError('private-error'), 'execution-error')):
            result, value, calls, _ = self.invoke({0: {'raw': b'', 'stdout': b'', 'error': error}})
            self.assertEqual((result, len(calls), value['profiles'][0]['reason']), (1, 4, reason))

    def test_bad_records_stop_remaining_profiles_and_keep_raw_bytes(self):
        raw = encoded(records()[:3]) + b'private-state-malformed\n'
        result, value, calls, root = self.invoke({0: {'raw': raw}})
        self.assertEqual((result, len(calls)), (1, 1))
        self.assertEqual(value['profiles'][0]['reason'], 'instrumentation-invalid')
        self.assertEqual(value['profiles'][0]['counts_and_statistics']['completed_pairs'], 1)
        self.assertIsNone(value['profiles'][0]['counts_and_statistics']['remaining_not_run_samples'])
        self.assertEqual(value['profiles'][0]['counts_and_statistics']['unvalidated_or_unrun_samples'], 110)
        self.assertEqual(value['profiles'][1]['counts_and_statistics']['remaining_not_run_samples'], 112)
        self.assertTrue(all(item['status'] == 'not-run' and item['reason'] == 'shared-instrumentation-invalid'
                            for item in value['profiles'][1:]))
        self.assertEqual((root / 'normal-xcom1.records.jsonl').read_bytes(), raw)

    def test_shared_native_instrumentation_labels_stop_remaining_profiles(self):
        for label in wrapper.SHARED_INVALID_LABELS:
            with self.subTest(label=label):
                result, value, calls, _ = self.invoke({0: {'raw': b'', 'stdout': b'', 'returncode': 1,
                    'stderr': b'TIMING_CALIBRATION INCOMPLETE ' + label.encode() + b'\n'}})
                self.assertEqual((result, len(calls)), (1, 1))
                self.assertEqual(value['profiles'][0]['reason'], 'instrumentation-invalid')

    def test_bad_markers_and_forged_completion_stop_remaining_profiles(self):
        for action in ({'stdout': wrapper.COMPLETE * 2}, {'raw': b''}, {'returncode': 1}):
            result, value, calls, _ = self.invoke({0: action})
            self.assertEqual((result, len(calls)), (1, 1))
            self.assertEqual(value['profiles'][0]['reason'], 'instrumentation-invalid')

    def test_missing_completion_is_incomplete_but_independent_profiles_continue(self):
        result, value, calls, _ = self.invoke({0: {'stdout': b'private-host-path\n'}})
        self.assertEqual((result, len(calls)), (1, 4))
        self.assertEqual(value['profiles'][0]['reason'], 'native-incomplete')

    def test_whole_budget_reduces_profile_timeout_and_prevents_next_launch(self):
        # Launch at 100, second launch at 570 (10s left), next check expired.
        result, value, calls, _ = self.invoke(clock=iter((100, 570, 570, 581)).__next__)
        self.assertEqual(result, 1)
        self.assertEqual([call[1]['timeout'] for call in calls], [120, 10])
        self.assertTrue(all(item['reason'] == 'whole-budget-exhausted' for item in value['profiles'][2:]))

    def test_budget_is_rechecked_after_profile_preparation(self):
        result, value, calls, _ = self.invoke(clock=iter((100, 579, 581, 582)).__next__)
        self.assertEqual((result, len(calls)), (1, 1))
        self.assertTrue(all(item['reason'] == 'whole-budget-exhausted' for item in value['profiles'][1:]))

    def test_invalid_arguments_existing_directory_and_symlink_do_not_launch(self):
        with tempfile.TemporaryDirectory() as existing:
            path = Path(existing)
            (path / 'link').symlink_to(path, target_is_directory=True)
            arguments = ([], ['/normal', '/mutant', 'relative', '/private'],
                         ['/normal', '/mutant', '/data', str(path)],
                         ['/normal', '/mutant', '/data', str(path / 'link')])
            for args in arguments:
                with patch.object(wrapper.subprocess, 'run') as run, redirect_stdout(io.StringIO()) as stdout:
                    self.assertEqual(wrapper.main(args), 2)
                    run.assert_not_called()
                    self.assertEqual(json.loads(stdout.getvalue())['status'], 'incomplete')


if __name__ == '__main__':
    unittest.main()
