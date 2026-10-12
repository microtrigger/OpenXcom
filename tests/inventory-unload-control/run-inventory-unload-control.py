#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Run finite, isolated native fixed ordinary inventory Unload / exact I gate profiles.

Each profile gets a fresh options directory and process. In particular, a lost
inventory destination's unresolved I obligation is never reset to keep testing.
Only fixed aggregate results or one whitelisted assertion label escape;
child output and private paths do not.
This local operator harness confirms fixture dialogs, never real play.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

PROFILES = ('core', 'deny-held', 'timeout', 'capability-timeout', 'disconnect', 'unseen', 'held-y', 'held-mouse', 'invalid-child-left', 'invalid-child-right', 'invalid-child-x2', 'invalid-child-moved', 'release-Y', 'release-mouse', 'stale-input', 'stale-actor', 'stale-reinit', 'stale-held', 'stale-held-position', 'capless-I', 'cross-kind', 'duplicate-pending', 'ledger-I', 'ledger-limit', 'disabled', 'close-disabled', 'independent', 'nontu', 'base', 'debug', 'malformed', 'malformed-short', 'malformed-extra', 'malformed-token', 'malformed-Q-short', 'malformed-Q-long', 'malformed-nul', 'upper-band', 'failed-I', 'send-failure-I', 'lost-I', 'reopen-I', 'save-I', 'parent-I', 'orphan-left-held', 'orphan-left-disconnect', 'orphan-Y', 'orphan-Y-disconnect', 'nested-transition', 'nested-pop', 'nested-save-notice', 'hidden-button', 'moved-button', 'resized-button', 'overlap-button', 'hidden-widget', 'moved-widget', 'widget-actor', 'widget-mode', 'slot-geometry', 'focus', 'palette')
PASS = ('INVENTORY_UNLOAD_CONTROL_GATE PASS {"schema":"inventory-unload-control-gate/1",'
        '"master":"MASTER","profile":"PROFILE","passed":true}')
SUITE_PASS = ('INVENTORY_UNLOAD_CONTROL_SUITE PASS {"schema":"inventory-unload-control-suite/1",'
              '"master":"MASTER","profiles":62,"runtime_gate_pass":true,"full_fairness":false}')
FAIL = 'INVENTORY_UNLOAD_CONTROL_SUITE FAIL'
LABEL = re.compile(r'INVENTORY_UNLOAD_CONTROL_GATE FAIL assertion-[0-9]+')


def _run_profile(binary, data, master, profile, runner):
    env = {name: value for name, value in os.environ.items() if not name.startswith('OPENXCOM_')}
    env.update({'OPENXCOM_INVENTORY_UNLOAD_CONTROL_GATE': '1',
                'OPENXCOM_INVENTORY_UNLOAD_PROFILE': profile,
                'OPENXCOM_MAP_LEFT_CLICK_ENABLED': '1',
                'OPENXCOM_SELECTION_ENABLED': '1',
                'SDL_AUDIODRIVER': 'dummy'})
    env['OPENXCOM_TURN_ENABLED'] = '1'
    env['OPENXCOM_INVENTORY_ENABLED'] = '1'
    if profile != 'disabled':
        env['OPENXCOM_INVENTORY_UNLOAD_ENABLED'] = '1'
    if profile != 'close-disabled':
        env['OPENXCOM_INVENTORY_CLOSE_ENABLED'] = '1'
    if profile != 'independent':
        env['OPENXCOM_INVENTORY_CLICK_ENABLED'] = '1'
        env['OPENXCOM_INVENTORY_RIGHT_CLICK_ENABLED'] = '1'
        env['OPENXCOM_INVENTORY_GROUND_ENABLED'] = '1'
    with tempfile.TemporaryDirectory(prefix='inventory-unload-control-') as temporary:
        root = Path(temporary)
        (root / 'user').mkdir()
        (root / 'config').mkdir()
        (root / 'config' / 'options.cfg').write_text(
            f'mods:\n  - id: {master}\n    active: true\noptions:\n  language: en-US\n  playIntro: false\n',
            encoding='utf-8')
        result = runner(
            [str(binary), '-master', master, '-data', str(data),
             '-user', str(root / 'user'), '-cfg', str(root / 'config')],
            env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=650 if profile == 'core' else 155 if profile in ('timeout', 'capability-timeout') else 45,
            text=True, errors='replace', check=False)
        expected = PASS.replace('MASTER', master).replace('PROFILE', profile)
        verdicts = [line for stream in (result.stdout, result.stderr) for line in stream.splitlines()
                    if 'INVENTORY_UNLOAD_CONTROL_' in line]
        if result.returncode != 0 or result.stdout.splitlines().count(expected) != 1 or verdicts != [expected]:
            labels = [line for line in result.stderr.splitlines() if LABEL.fullmatch(line)]
            if len(labels) == 1 and verdicts == labels:
                raise RuntimeError(labels[0])
            raise RuntimeError(FAIL)


def run(binary, data, master, runner=subprocess.run):
    """Return the suite verdict only after every exact per-profile native PASS."""
    if not binary.is_absolute() or not data.is_absolute() or master not in ('xcom1', 'xcom2'):
        raise ValueError('invalid invocation')
    try:
        for profile in PROFILES:
            _run_profile(binary, data, master, profile, runner)
    except (OSError, subprocess.SubprocessError, RuntimeError) as error:
        # Subprocess and filesystem errors can contain child text/private paths.
        if isinstance(error, RuntimeError) and LABEL.fullmatch(str(error)):
            raise RuntimeError(str(error)) from None
        raise RuntimeError(FAIL) from None
    return SUITE_PASS.replace('MASTER', master)


def main(args=None):
    args = sys.argv[1:] if args is None else args
    if len(args) != 3:
        print('Usage: run-inventory-unload-control.py /absolute/path/openxcom /absolute/path/data xcom1|xcom2',
              file=sys.stderr)
        return 2
    try:
        answer = run(Path(args[0]), Path(args[1]), args[2])
    except ValueError:
        print(FAIL, file=sys.stderr)
        return 2
    except (OSError, subprocess.SubprocessError, RuntimeError) as error:
        if isinstance(error, RuntimeError) and LABEL.fullmatch(str(error)):
            print(str(error), file=sys.stderr)
        print(FAIL, file=sys.stderr)
        return 1
    print(answer)
    return 0


if __name__ == '__main__':
    sys.exit(main())
