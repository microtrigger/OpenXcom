#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Run bounded real-state turn checks on a focused graphical SDL display.

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

PASS = ('TURN_CONTROL_GATE PASS {"schema":"turn-control-gate/1","master":"MASTER",'
        '"parity_pairs":15,"adversarial_worlds":29,"ordinary_timer_outcomes":true,'
        '"actual_full_view_oracle":true,"post_outcome_views":30,"first_contact_interrupt":true,'
        '"ordinary_door":true,"shared_action_ledger":true,"animation_grant_survives":true,'
        '"local_confirmation":true,"timing_equal":false,"full_fairness":false}')
PREVIEW = ('TURN_CONTROL_GATE PREVIEW {"schema":"turn-control-preview/1","master":"MASTER",'
           '"modal_presented":true,"runtime_gate_pass":false}')
LABEL = re.compile(r'TURN_CONTROL_GATE FAIL assertion-[0-9]+')


def run(binary, data, master, runner=subprocess.run, *, preview=False):
    """Return aggregate text or fail; strict whitelist never forwards child text."""
    if not binary.is_absolute() or not data.is_absolute() or master not in ('xcom1', 'xcom2'):
        raise ValueError('invalid invocation')
    env = os.environ.copy()
    # Disallow injecting other experimental producers or inherited endpoints.
    for name in tuple(env):
        if name.startswith('OPENXCOM_'):
            del env[name]
    env['OPENXCOM_TURN_CONTROL_GATE'] = '1'
    env['OPENXCOM_TURN_ENABLED'] = '1'
    env['OPENXCOM_SELECTION_ENABLED'] = '1'
    env['SDL_AUDIODRIVER'] = 'dummy'
    if preview:
        env['OPENXCOM_TURN_MODAL_PREVIEW'] = '1'
    with tempfile.TemporaryDirectory(prefix='turn-control-') as profile:
        root = Path(profile)
        (root / 'user').mkdir()
        (root / 'config').mkdir()
        (root / 'config' / 'options.cfg').write_text(
            f'mods:\n  - id: {master}\n    active: true\noptions:\n  language: en-US\n  playIntro: false\n')
        result = runner(
            [str(binary), '-master', master, '-data', str(data),
             '-user', str(root / 'user'), '-cfg', str(root / 'config')],
            env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=270, text=True, errors='replace', check=False)
        expected = (PREVIEW if preview else PASS).replace('MASTER', master)
        if result.returncode != 0 or result.stdout.splitlines().count(expected) != 1:
            labels = [line for line in result.stderr.splitlines() if LABEL.fullmatch(line)]
            raise RuntimeError(labels[0] if len(labels) == 1 else 'TURN_CONTROL_GATE FAIL')
    return expected


def main(args=None):
    args = sys.argv[1:] if args is None else args
    preview = bool(args and args[0] == '--preview-modal')
    if preview:
        args = args[1:]
    if len(args) != 3:
        print('Usage: run-turn-control.py [--preview-modal] /absolute/path/openxcom /absolute/path/data xcom1|xcom2', file=sys.stderr)
        return 2
    try:
        answer = run(Path(args[0]), Path(args[1]), args[2], preview=preview)
    except ValueError:
        print('TURN_CONTROL_GATE FAIL', file=sys.stderr)
        return 2
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        # Even errors from subprocess/tempfiles may contain private paths.
        label = str(error)
        if LABEL.fullmatch(label):
            print(label, file=sys.stderr)
        print('TURN_CONTROL_GATE FAIL', file=sys.stderr)
        return 1
    print(answer)
    return 0


if __name__ == '__main__':
    sys.exit(main())
