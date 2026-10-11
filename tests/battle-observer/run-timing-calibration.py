#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
"""Bounded descriptive timing calibration; never a timing-equivalence test.

All raw records and engine diagnostics stay in a fresh owner-only directory.
The single stdout JSON document contains fixed metadata and aggregates only.
"""
from pathlib import Path
import json
import os
import re
import statistics
import subprocess
import sys
import tempfile
import time

SCHEMA = 'timing-calibration/1'
PROFILES = (('normal', 'xcom1'), ('normal', 'xcom2'),
            ('mutant', 'xcom1'), ('mutant', 'xcom2'))
COUNTS = {'normal': {'warmup': 8, 'aa': 16, 'ab': 32},
          'mutant': {'warmup': 4, 'aa': 0, 'ab': 16}}
PROFILE_SECONDS = {'normal': 120, 'mutant': 90}
WHOLE_SECONDS = 480
MIN_SPACING_NS = 300_000_000
RECORD_LIMIT = 128 * 1024
DIAGNOSTIC_LIMIT = 8 * 1024 * 1024
COMPLETE = b'TIMING_CALIBRATION COMPLETE\n'
INCOMPLETE = re.compile(rb'TIMING_CALIBRATION INCOMPLETE [a-z-]{1,80}\n')
SAMPLE_KEYS = frozenset(('type', 'index', 'pair', 'kind', 'order', 'label',
                         't0_ns', 'flip_ns', 't1_ns'))
METRICS = ('round_trip_ns', 'post_flip_ns')
SHARED_INVALID_LABELS = frozenset((
    'frame-instrumentation', 'preallocated-frame-size', 'nonmonotonic-clock',
    'private-record-descriptor', 'private-record-profile', 'record-write',
    'profile-mode', 'isolated-gate-profile', 'internal-socketpair',
    'internal-channel-setup', 'identical-setup-seed', 'normal-battlescape-top-state'))


def schedule(mode):
    """Mirror the fixed native precomputed schedule, independent of samples."""
    state = 0x4f584354
    result = []
    for kind in ('warmup', 'aa', 'ab'):
        for _ in range(COUNTS[mode][kind]):
            state ^= (state << 13) & 0xffffffff
            state ^= state >> 17
            state ^= (state << 5) & 0xffffffff
            state &= 0xffffffff
            result.append((kind, 'BA' if state & 1 else 'AB'))
    return result


class InvalidRecords(ValueError):
    """Keep the validated prefix while rejecting any malformed continuation."""
    def __init__(self, parsed):
        super().__init__('invalid-records')
        self.parsed = parsed


def unique_object(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError('duplicate-key')
        result[key] = value
    return result


def parse_records(raw, mode):
    """Validate every record, order, timestamp and completion marker strictly.

    An unfinished final half/pair is valid incomplete data. A torn JSON line is
    malformed. Nothing gets sorted, trimmed, retried or synthesized.
    """
    parsed = {'samples': [], 'pairs': []}
    pending = []
    expected = schedule(mode)
    previous_t1 = 0
    try:
        if len(raw) > RECORD_LIMIT:
            raise ValueError('record-limit-or-framing')
        for line in raw.splitlines(keepends=True):
            if line == b'\n' or not line.endswith(b'\n') or len(line) > 1024:
                raise ValueError('record-framing')
            record = json.loads(line.decode('utf-8'), object_pairs_hook=unique_object,
                                parse_constant=lambda _: (_ for _ in ()).throw(ValueError('constant')))
            if not isinstance(record, dict):
                raise ValueError('record-object')
            pair_index = len(parsed['pairs'])
            if pair_index >= len(expected):
                raise ValueError('extra-record')
            kind, order = expected[pair_index]
            if record.get('type') == 'sample':
                if set(record) != SAMPLE_KEYS or len(pending) >= 2:
                    raise ValueError('sample-schema')
                for key in ('index', 'pair', 't0_ns', 'flip_ns', 't1_ns'):
                    if type(record[key]) is not int:
                        raise ValueError('integer-required')
                leg = len(pending)
                if (record['index'] != pair_index * 2 + leg or
                        record['pair'] != pair_index or record['kind'] != kind or
                        record['order'] != order or record['label'] != order[leg]):
                    raise ValueError('sample-sequence')
                if not (previous_t1 < record['t0_ns'] < record['flip_ns'] < record['t1_ns']
                        <= PROFILE_SECONDS[mode] * 1_000_000_000):
                    raise ValueError('timestamp-order-or-cap')
                if parsed['samples'] and record['t0_ns'] - previous_t1 < MIN_SPACING_NS:
                    raise ValueError('sample-spacing')
                previous_t1 = record['t1_ns']
                parsed['samples'].append(record)
                pending.append(record)
            elif record.get('type') == 'pair':
                if (set(record) != {'type', 'pair'} or type(record['pair']) is not int or
                        record['pair'] != pair_index or len(pending) != 2):
                    raise ValueError('pair-sequence')
                halves = {sample['label']: sample for sample in pending}
                parsed['pairs'].append({'kind': kind, 'order': order,
                                        'A': halves['A'], 'B': halves['B']})
                pending = []
            else:
                raise ValueError('unknown-record')
    except (ValueError, TypeError, UnicodeError, RecursionError):
        raise InvalidRecords(parsed) from None
    return parsed


def describe(values):
    """Descriptive A-minus-B values only; no inference or outlier filtering."""
    count = len(values)
    return {'count': count,
            'mean': sum(values) / count if count else None,
            'median': statistics.median(values) if count else None,
            'min': min(values) if count else None,
            'max': max(values) if count else None,
            'sign_counts': {'negative': sum(value < 0 for value in values),
                            'zero': sum(value == 0 for value in values),
                            'positive': sum(value > 0 for value in values)}}


def aggregate(parsed, mode, instrumentation_valid=True):
    kinds = {}
    for kind in ('warmup', 'aa', 'ab'):
        pairs = [pair for pair in parsed['pairs'] if pair['kind'] == kind]
        metrics = {}
        for metric, start in (() if kind == 'warmup' else
                              (('round_trip_ns', 't0_ns'), ('post_flip_ns', 'flip_ns'))):
            values = [(pair['A']['t1_ns'] - pair['A'][start]) -
                      (pair['B']['t1_ns'] - pair['B'][start]) for pair in pairs]
            metrics[metric] = describe(values)
        kinds[kind] = {'expected_pairs': COUNTS[mode][kind], 'completed_pairs': len(pairs),
                       'order_counts': {order: sum(pair['order'] == order for pair in pairs)
                                        for order in ('AB', 'BA')}}
        if kind != 'warmup':
            kinds[kind]['a_minus_b'] = metrics
    expected_samples = 2 * sum(COUNTS[mode].values())
    unobserved = expected_samples - len(parsed['samples'])
    return {'expected_samples': expected_samples,
            'observed_samples': len(parsed['samples']),
            'remaining_not_run_samples': unobserved if instrumentation_valid else None,
            'unvalidated_or_unrun_samples': 0 if instrumentation_valid else unobserved,
            'completed_pairs': len(parsed['pairs']),
            'unpaired_samples': len(parsed['samples']) - 2 * len(parsed['pairs']),
            'kinds': kinds}


def empty_profile(mode, master, reason):
    return {'mode': mode, 'master': master, 'status': 'not-run', 'reason': reason,
            'counts_and_statistics': aggregate({'samples': [], 'pairs': []}, mode),
            'control': {'detectable_control': False, 'result': 'inconclusive'} if mode == 'mutant' else None}


def control(summary, status):
    measured = summary['kinds']['ab']
    detected = (status == 'complete' and measured['completed_pairs'] == 16 and
                all(measured['a_minus_b'][metric]['sign_counts']['positive'] >= 14 and
                    measured['a_minus_b'][metric]['median'] >= 3_000_000
                    for metric in METRICS))
    return {'detectable_control': detected,
            'result': 'detectable-control' if detected else 'inconclusive'}


def private_file(path):
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, 'O_NOFOLLOW', 0)
    fd = os.open(str(path), flags, 0o600)
    os.fchmod(fd, 0o600)
    return os.fdopen(fd, 'wb', buffering=0)


def read_bounded(path, limit):
    with path.open('rb') as stream:
        return stream.read(limit + 1)


def inspect_markers(stdout, stderr):
    """Ignore ordinary private diagnostics, but never trust a forged marker."""
    if len(stdout) > DIAGNOSTIC_LIMIT or len(stderr) > DIAGNOSTIC_LIMIT:
        return None
    complete = 0
    incomplete = 0
    for output, is_stdout in ((stdout, True), (stderr, False)):
        for line in output.splitlines(keepends=True):
            if not line.startswith(b'TIMING_CALIBRATION'):
                continue
            if is_stdout and line == COMPLETE:
                complete += 1
            elif not is_stdout and INCOMPLETE.fullmatch(line):
                incomplete += 1
            else:
                return None
    if complete > 1 or incomplete > 1 or (complete and incomplete):
        return None
    return complete, incomplete


def run_profile(binary, data, output, mode, master, budget):
    """Return a sanitized aggregate and whether shared instrumentation is bad."""
    entry = empty_profile(mode, master, 'native-incomplete')
    entry['status'] = 'incomplete'
    prefix = output / (mode + '-' + master)
    records_path = prefix.with_suffix('.records.jsonl')
    stdout_path = prefix.with_suffix('.stdout')
    stderr_path = prefix.with_suffix('.stderr')
    outcome = 'returned'
    returncode = None
    invalid = False
    parsed = {'samples': [], 'pairs': []}
    try:
        with private_file(records_path) as records, private_file(stdout_path) as stdout, \
                private_file(stderr_path) as stderr, \
                tempfile.TemporaryDirectory(prefix='timing-calibration-') as profile:
            root = Path(profile)
            (root / 'user').mkdir(mode=0o700)
            (root / 'config').mkdir(mode=0o700)
            options = root / 'config' / 'options.cfg'
            with private_file(options) as config:
                config.write(('mods:\n  - id: ' + master + '\n    active: true\noptions:\n'
                              '  language: en-US\n  playIntro: false\n').encode('ascii'))
            env = os.environ.copy()
            env['OPENXCOM_TIMING_CALIBRATION'] = mode
            env['OPENXCOM_TIMING_RECORD_FD'] = str(records.fileno())
            env['SDL_AUDIODRIVER'] = 'dummy'
            now = time.monotonic()
            if budget['deadline'] is None:
                budget['deadline'] = now + WHOLE_SECONDS
            timeout = min(PROFILE_SECONDS[mode], budget['deadline'] - now)
            if timeout <= 0:
                return empty_profile(mode, master, 'whole-budget-exhausted'), False
            try:
                result = subprocess.run(
                    [str(binary), '-master', master, '-data', str(data),
                     '-user', str(root / 'user'), '-cfg', str(root / 'config')],
                    env=env, stdout=stdout, stderr=stderr, pass_fds=(records.fileno(),),
                    timeout=timeout, check=False)
                returncode = result.returncode
            except subprocess.TimeoutExpired:
                outcome = 'timeout'
            except (OSError, ValueError):
                outcome = 'execution-error'
        try:
            parsed = parse_records(read_bounded(records_path, RECORD_LIMIT), mode)
        except InvalidRecords as error:
            parsed = error.parsed
            invalid = True
        private_stderr = read_bounded(stderr_path, DIAGNOSTIC_LIMIT)
        markers = inspect_markers(read_bounded(stdout_path, DIAGNOSTIC_LIMIT), private_stderr)
        shared_failure = any(b'TIMING_CALIBRATION INCOMPLETE ' + label.encode('ascii') + b'\n'
                             in private_stderr.splitlines(keepends=True)
                             for label in SHARED_INVALID_LABELS)
        if markers is None or shared_failure:
            invalid = True
        if invalid:
            entry['reason'] = 'instrumentation-invalid'
        elif outcome != 'returned':
            entry['reason'] = outcome
        elif markers[0] and (returncode != 0 or len(parsed['pairs']) != len(schedule(mode))):
            invalid = True
            entry['reason'] = 'instrumentation-invalid'
        elif returncode == 0 and markers == (1, 0):
            entry['status'] = 'complete'
            entry['reason'] = None
        elif returncode != 0:
            entry['reason'] = 'native-exit'
    except OSError:
        entry['reason'] = 'private-output-error'
        invalid = True
    summary = aggregate(parsed, mode, instrumentation_valid=not invalid)
    entry['counts_and_statistics'] = summary
    if mode == 'mutant':
        entry['control'] = control(summary, entry['status'])
    return entry, invalid


def document(profiles, status):
    return {'schema': SCHEMA, 'status': status,
            'limits_seconds': {'normal_profile': 120, 'mutant_profile': 90, 'whole': 480},
            'metrics': {'round_trip_ns': 'native-harness-return-path',
                        'post_flip_ns': 'post-flip-native-harness-return-path'},
            'simulation': False, 'audio': 'subsystem-disabled',
            'timing_equivalence_claimed': False, 'profiles': profiles}


def emit(value):
    print(json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False))


def main(argv=None):
    args = sys.argv[1:] if argv is None else argv
    profiles = []
    if len(args) != 4 or any(not Path(value).is_absolute() for value in args):
        emit(document([empty_profile(mode, master, 'invalid-arguments')
                       for mode, master in PROFILES], 'incomplete'))
        return 2
    normal, mutant, data, output = map(Path, args)
    try:
        # A pre-existing directory, including a symlink, is never accepted.
        output.mkdir(mode=0o700, parents=False, exist_ok=False)
        output.chmod(0o700)
    except OSError:
        emit(document([empty_profile(mode, master, 'private-output-error')
                       for mode, master in PROFILES], 'incomplete'))
        return 2
    stop_reason = None
    budget = {'deadline': None}
    for mode, master in PROFILES:
        if stop_reason:
            profiles.append(empty_profile(mode, master, stop_reason))
            continue
        # run_profile starts the budget immediately before the first launch.
        if budget['deadline'] is not None and budget['deadline'] <= time.monotonic():
            stop_reason = 'whole-budget-exhausted'
            profiles.append(empty_profile(mode, master, stop_reason))
            continue
        entry, invalid = run_profile(normal if mode == 'normal' else mutant, data, output,
                                     mode, master, budget)
        profiles.append(entry)
        if invalid:
            stop_reason = 'shared-instrumentation-invalid'
    complete = all(entry['status'] == 'complete' for entry in profiles)
    emit(document(profiles, 'complete' if complete else 'incomplete'))
    return 0 if complete else 1


if __name__ == '__main__':
    sys.exit(main())
