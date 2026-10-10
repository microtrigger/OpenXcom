#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
"""Run the asset-dependent effects gate with a fresh disposable profile.

Requires a real focused graphical SDL display and separately installed lawful
resources. Suppress ordinary engine logs; expose only the fixed aggregate result.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

PASS = ('OBSERVER_EFFECTS_GATE PASS {"schema":"observer-effects-gate/1","master":"MASTER","worlds":2,'
        '"checkpoints_per_world":3,"requests":3,"engine_rng":true,"libc_rng":"unverified",'
        '"audio":"subsystem-disabled","timing_equal":false,"full_fairness":false}')


def main():
    if len(sys.argv) != 4:
        print('Usage: run-effects.py /absolute/path/openxcom /absolute/path/data xcom1|xcom2', file=sys.stderr)
        return 2
    binary, data = map(Path, sys.argv[1:3])
    master = sys.argv[3]
    if not binary.is_absolute() or not data.is_absolute() or master not in ('xcom1', 'xcom2'):
        print('OBSERVER_EFFECTS_GATE FAIL', file=sys.stderr)
        return 2
    env = os.environ.copy()
    env['OPENXCOM_OBSERVER_EFFECTS_GATE'] = '1'
    try:
        with tempfile.TemporaryDirectory(prefix='observer-effects-') as profile:
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
            lines = result.stdout.splitlines()
            if result.returncode != 0 or lines.count(PASS.replace("MASTER", master)) != 1:
                raise RuntimeError('gate failed')
    except (OSError, RuntimeError, subprocess.TimeoutExpired):
        print('OBSERVER_EFFECTS_GATE FAIL', file=sys.stderr)
        return 1
    print(PASS.replace("MASTER", master))
    return 0


if __name__ == '__main__':
    sys.exit(main())
