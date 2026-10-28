#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run trusted native owned-base model checks through the generic runner; no Game."""
import argparse
import json
import os
import shlex
from pathlib import Path
from native_test_runner import run_recipe

LIMITATIONS = [
    'Actual native models/collector with synthetic in-memory rules injected through test-only private access; not live acceptance.',
    'UI-equivalent pure research/manufacture query sets, known route pointer membership, guarded craft estimates, completed archive joins and researched tangible-item article metadata checked.',
    'No Game/window/campaign-load/player-save execution. Existing matching full-build objects are reused and explicitly hashed.',
    'Non-Target IDs track observed live bindings; unseen deletion/address reuse between captures cannot be distinguished. IDs never authorize mutations.',
    'The manifest is explicit, not a complete compiler, SDK-header, dynamic-library or toolchain transitive dependency closure.',
]


def integrate(source, build, sdk, output):
    source, build, sdk, output = [Path(p).resolve() for p in (source, build, sdk, output)]
    link_recipe = build / 'src/CMakeFiles/openxcom.dir/link.txt'
    files = [source / p for p in (
        'src/Observer/AgentPlayerBases.cpp', 'src/Observer/AgentPlayerBases.h',
        'tests/agent-control/AgentPlayerBasesTest.cpp', 'src/Savegame/Soldier.h',
        'src/Savegame/Base.h', 'src/Savegame/SavedGame.cpp', 'src/Savegame/Craft.cpp',
        'src/Savegame/Craft.h', 'src/Savegame/Waypoint.h',
        'src/Savegame/MissionStatistics.h', 'src/Savegame/SoldierDiary.cpp',
        'src/Savegame/BattleUnitStatistics.h', 'src/Mod/RuleItem.cpp',
        'src/Mod/ArticleDefinition.cpp')]
    files += [Path(__file__).resolve(), Path(__file__).with_name('native_test_runner.py').resolve(), link_recipe]
    objects = [(build / 'src' / token).resolve() for token in shlex.split(link_recipe.read_text())
               if token.endswith('.o') and not token.endswith(('/main.cpp.o', '/AgentPlayerBases.cpp.o'))]
    if not objects:
        raise ValueError('No native build object closure')
    flags = ['c++', '-std=c++11', '-O0', '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL',
             '-DOPENXCOM_EXPERIMENTAL_OBSERVER_DIAGNOSTICS', '-DOPENXCOM_LOADING_PRESENTATION_HELPERS',
             '-D__NO_OPENGL', '-I' + str(sdk / 'include/SDL'), '-I' + str(sdk / 'include')]
    library = sdk / 'lib/x86_64-linux-gnu'
    overlay = {'LD_LIBRARY_PATH': str(library) + ':' + os.environ.get('LD_LIBRARY_PATH', '')}
    def step(name, argv):
        return dict(name=name, argv=[str(arg) for arg in argv], cwd=str(source), env=overlay.copy())
    steps = [step('compile_fixture', flags + ['-c', files[2], '-o', output / 'fixture.o']),
             step('compile_collector', flags + ['-c', files[0], '-o', output / 'collector.o']),
             step('link', ['c++', output / 'fixture.o', output / 'collector.o'] + objects + [
                 '-Wl,--gc-sections', '-L' + str(library), '-lSDL', '-lSDL_image', '-lSDL_mixer',
                 '-lSDL_gfx', '-lyaml-cpp', '-ldl', '-pthread', '-o', output / 'test']),
             step('test', [output / 'test', output / 'records.json'])]
    receipt = run_recipe(dict(version=1, source=str(source), output=str(output),
                              inputs=list(dict.fromkeys(str(p) for p in files + objects)), steps=steps))
    qualification = dict(all_passed=receipt['all_passed'], inputs_unchanged=receipt['inputs_unchanged'],
                         game_executed=False, live_acceptance=False, generic_receipt='RESULT.json',
                         limitations=LIMITATIONS, source_revision='caller-supplied; inspect recorded input hashes')
    (output / 'QUALIFICATION.json').write_text(json.dumps(qualification, indent=2) + '\n')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'sdk', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    try:
        receipt = integrate(args.source, args.build, args.sdk, args.output)
    except (ValueError, OSError) as error:
        print('Player-bases runner refused input: ' + type(error).__name__)
        return 2
    return 0 if receipt['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
