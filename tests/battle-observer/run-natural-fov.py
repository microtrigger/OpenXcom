#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
"""Run six natural-FOV/pause samples with lawful resources and a disposable profile.

A real focused graphical SDL display is required. Ordinary engine output is
suppressed; only fixed aggregate results and source assertion labels escape.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

PASS = ('NATURAL_FOV_GATE PASS {"schema":"natural-fov-gate/1","master":"MASTER",'
        '"samples":6,"requests":6,"pixel_oracle":true,"hud_oracle":true,'
        '"pause_recovery":true,"audio":"initialized-muted-dummy","timing_equal":false,"full_fairness":false}')


def main():
    if len(sys.argv) != 4:
        print('Usage: run-natural-fov.py /absolute/path/openxcom /absolute/path/data xcom1|xcom2', file=sys.stderr)
        return 2
    binary, data = map(Path, sys.argv[1:3])
    master = sys.argv[3]
    if not binary.is_absolute() or not data.is_absolute() or master not in ('xcom1', 'xcom2'):
        print('NATURAL_FOV_GATE FAIL', file=sys.stderr)
        return 2
    env = os.environ.copy()
    env['OPENXCOM_NATURAL_FOV_GATE'] = '1'
    env['SDL_AUDIODRIVER'] = 'dummy'
    try:
        with tempfile.TemporaryDirectory(prefix='natural-fov-') as profile:
            root = Path(profile)
            (root / 'user').mkdir()
            (root / 'config').mkdir()
            (root / 'config' / 'options.cfg').write_text(
                f'mods:\n  - id: {master}\n    active: true\nlanguage: en-US\nplayIntro: false\n')
            result = subprocess.run(
                [str(binary), '-master', master, '-data', str(data),
                 '-user', str(root / 'user'), '-cfg', str(root / 'config')],
                env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=45,
                text=True, errors='replace', check=False)
            if result.returncode != 0 or result.stdout.splitlines().count(PASS.replace('MASTER', master)) != 1:
                labels = [line for line in result.stderr.splitlines()
                          if re.fullmatch(r'NATURAL_FOV_GATE FAIL assertion-[0-9]+', line)]
                if len(labels) == 1:
                    print(labels[0], file=sys.stderr)
                raise RuntimeError('gate failed')
    except (OSError, RuntimeError, subprocess.TimeoutExpired):
        print('NATURAL_FOV_GATE FAIL', file=sys.stderr)
        return 1
    print(PASS.replace('MASTER', master))
    return 0


if __name__ == '__main__':
    sys.exit(main())
