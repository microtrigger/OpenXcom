#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md.
"""Run the smoke-clipping gate with lawful resources and a disposable profile.

A real focused graphical SDL display is required. Ordinary engine diagnostics
stay private; only the exact aggregate result and fixed assertion labels escape.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

PASS = ('SMOKE_CLIPPING_GATE PASS {"schema":"smoke-clipping-gate/1","master":"MASTER",'
        '"scenarios":4,"frames_per_scenario":8,"equal_pair_frames":8,"responses":32,'
        '"positive_phases":8,"audio":"subsystem-disabled","simulation":false,'
        '"timing_equal":false,"full_fairness":false}')


def main():
    if len(sys.argv) != 4:
        print('Usage: run-smoke-clipping.py /absolute/path/openxcom /absolute/path/data xcom1|xcom2', file=sys.stderr)
        return 2
    binary, data = map(Path, sys.argv[1:3])
    master = sys.argv[3]
    if not binary.is_absolute() or not data.is_absolute() or master not in ('xcom1', 'xcom2'):
        print('SMOKE_CLIPPING_GATE FAIL', file=sys.stderr)
        return 2
    env = os.environ.copy()
    env['OPENXCOM_SMOKE_CLIPPING_GATE'] = '1'
    # Game initialization may open a device; the native fixture closes the mixer
    # and the entire audio subsystem before resources or battle setup are loaded.
    env['SDL_AUDIODRIVER'] = 'dummy'
    try:
        with tempfile.TemporaryDirectory(prefix='smoke-clipping-') as profile:
            root = Path(profile)
            (root / 'user').mkdir()
            (root / 'config').mkdir()
            (root / 'config' / 'options.cfg').write_text(
                f'mods:\n  - id: {master}\n    active: true\noptions:\n  language: en-US\n  playIntro: false\n')
            result = subprocess.run(
                [str(binary), '-master', master, '-data', str(data),
                 '-user', str(root / 'user'), '-cfg', str(root / 'config')],
                env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=45,
                text=True, errors='replace', check=False)
            if result.returncode != 0 or result.stdout.splitlines().count(PASS.replace('MASTER', master)) != 1:
                labels = [line for line in result.stderr.splitlines()
                          if re.fullmatch(r'SMOKE_CLIPPING_GATE FAIL [a-z-]{1,80}', line)]
                if len(labels) == 1:
                    print(labels[0], file=sys.stderr)
                raise RuntimeError('gate failed')
    except (OSError, RuntimeError, subprocess.TimeoutExpired):
        print('SMOKE_CLIPPING_GATE FAIL', file=sys.stderr)
        return 1
    print(PASS.replace('MASTER', master))
    return 0


if __name__ == '__main__':
    sys.exit(main())
