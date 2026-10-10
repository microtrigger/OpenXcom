#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Run the bounded battlefield-audio gate with separately supplied lawful data.

A focused graphical SDL display is required; audio uses the initialized dummy
driver. Ordinary engine output is suppressed. Only the fixed aggregate result
and fixed source-assertion labels may escape this runner.
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

if not __debug__:
    sys.exit('FAIL: battlefield-audio checks require Python assertions (disable optimization)')

PASS = ('BATTLEFIELD_AUDIO_GATE PASS {"schema":"battlefield-audio-gate/1","master":"MASTER",'
        '"worlds":4,"orientation_pairs":2,"requests":2,"pcm_oracle":true,"cue_witness":true,'
        '"engine_rng":true,"libc_rng":"unverified","audio":"initialized-normal-volume-dummy",'
        '"music":"normal","timing_equal":false,"full_fairness":false}')


def main():
    if len(sys.argv) != 4:
        print('Usage: run-audio.py /absolute/path/openxcom /absolute/path/data xcom1|xcom2',
              file=sys.stderr)
        return 2
    binary, data = map(Path, sys.argv[1:3])
    master = sys.argv[3]
    if not binary.is_absolute() or not data.is_absolute() or master not in ('xcom1', 'xcom2'):
        print('BATTLEFIELD_AUDIO_GATE FAIL', file=sys.stderr)
        return 2
    env = os.environ.copy()
    env['OPENXCOM_BATTLEFIELD_AUDIO_GATE'] = '1'
    env['SDL_AUDIODRIVER'] = 'dummy'
    try:
        with tempfile.TemporaryDirectory(prefix='battlefield-audio-') as profile:
            root = Path(profile)
            (root / 'user').mkdir()
            (root / 'config').mkdir()
            # OptionInfo values belong under options. The mods list is top-level.
            (root / 'config' / 'options.cfg').write_text(
                f'mods:\n  - id: {master}\n    active: true\n'
                'options:\n'
                '  language: en-US\n'
                '  playIntro: false\n'
                '  soundVolume: 84\n'
                '  musicVolume: 84\n'
                '  uiVolume: 42\n'
                '  audioSampleRate: 22050\n'
                '  skipNextTurnScreen: false\n'
                '  autosave: false\n', encoding='utf-8')
            result = subprocess.run(
                [str(binary), '-master', master, '-data', str(data),
                 '-user', str(root / 'user'), '-cfg', str(root / 'config')],
                env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90,
                text=True, errors='replace', check=False)
            if result.returncode != 0 or result.stdout.splitlines().count(PASS.replace('MASTER', master)) != 1:
                labels = [line for line in result.stderr.splitlines()
                          if re.fullmatch(r'BATTLEFIELD_AUDIO_GATE FAIL assertion-[0-9]+', line)]
                if len(labels) == 1:
                    print(labels[0], file=sys.stderr)
                raise RuntimeError('gate failed')
    except (OSError, RuntimeError, subprocess.TimeoutExpired):
        print('BATTLEFIELD_AUDIO_GATE FAIL', file=sys.stderr)
        return 1
    print(PASS.replace('MASTER', master))
    return 0


if __name__ == '__main__':
    sys.exit(main())
