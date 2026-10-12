#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Run bounded real-state selection checks on a focused graphical SDL display.

Only fixed aggregate result/source labels escape. No saves, assets, captures,
raw states, names, command tokens, response packets, or child logs are retained.
This wrapper is a local operator harness; it deliberately clicks the fixture's
confirmation dialog. It is not an automation/self-approval route for real play.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

PASS = ('SELECTION_CONTROL_GATE PASS {"schema":"selection-control-gate/1","master":"MASTER",'
        '"parity_pairs":10,"adversarial_worlds":19,"three_eligible_actors":true,'
        '"actual_panel_oracle":true,"post_action_panels":10,"shared_stance_ledger":true,'
        '"local_confirmation":true,"timing_equal":false,"full_fairness":false}')
LABEL = re.compile(r'SELECTION_CONTROL_GATE FAIL assertion-[0-9]+')


def run(binary, data, master, runner=subprocess.run):
    """Return aggregate text or fail; strict whitelist never forwards child text."""
    if not binary.is_absolute() or not data.is_absolute() or master not in ('xcom1', 'xcom2'):
        raise ValueError('invalid invocation')
    env = os.environ.copy()
    # Disallow injecting other experimental producers or inherited endpoints.
    for name in tuple(env):
        if name.startswith('OPENXCOM_'):
            del env[name]
    env['OPENXCOM_SELECTION_CONTROL_GATE'] = '1'
    env['OPENXCOM_SELECTION_ENABLED'] = '1'
    env['SDL_AUDIODRIVER'] = 'dummy'
    with tempfile.TemporaryDirectory(prefix='selection-control-') as profile:
        root = Path(profile)
        (root / 'user').mkdir()
        (root / 'config').mkdir()
        (root / 'config' / 'options.cfg').write_text(
            f'mods:\n  - id: {master}\n    active: true\noptions:\n  language: en-US\n  playIntro: false\n')
        result = runner(
            [str(binary), '-master', master, '-data', str(data),
             '-user', str(root / 'user'), '-cfg', str(root / 'config')],
            env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=210, text=True, errors='replace', check=False)
        expected = PASS.replace('MASTER', master)
        if result.returncode != 0 or result.stdout.splitlines().count(expected) != 1:
            labels = [line for line in result.stderr.splitlines() if LABEL.fullmatch(line)]
            raise RuntimeError(labels[0] if len(labels) == 1 else 'SELECTION_CONTROL_GATE FAIL')
    return expected


def main(args=None):
    args = sys.argv[1:] if args is None else args
    if len(args) != 3:
        print('Usage: run-selection-control.py /absolute/path/openxcom /absolute/path/data xcom1|xcom2', file=sys.stderr)
        return 2
    try:
        answer = run(Path(args[0]), Path(args[1]), args[2])
    except ValueError:
        print('SELECTION_CONTROL_GATE FAIL', file=sys.stderr)
        return 2
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        # Even errors from subprocess/tempfiles may contain private paths.
        label = str(error)
        if LABEL.fullmatch(label):
            print(label, file=sys.stderr)
        print('SELECTION_CONTROL_GATE FAIL', file=sys.stderr)
        return 1
    print(answer)
    return 0


if __name__ == '__main__':
    sys.exit(main())
