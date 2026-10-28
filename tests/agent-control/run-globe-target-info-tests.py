#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run native globe target popup fixture through the generic trusted recipe runner."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
from native_test_runner import run_recipe


def load_support(source):
    spec = importlib.util.spec_from_file_location('native_support', source / 'tests/agent-control/run-dogfight-providers.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def extract(source, output):
    support = load_support(source)
    block = support.block
    for name in ('MultipleTargets','TargetInfo'):
        klass=name+'State';body=(source/('src/Geoscape/'+klass+'.cpp')).read_text()
        # Include each literal method once, including constructor and native callbacks.
        signatures=re.findall(r'^\s*((?:[\w:<>, &*]+ )?'+klass+r'::~?'+klass+r'\(|(?:[\w:<>, &*]+ )?'+klass+r'::\w+\()',body,re.M)
        signatures=list(dict.fromkeys(x.strip() for x in signatures))
        (output/(name+'.generated.inc')).write_text('\n'.join(block(body,x) for x in signatures))
        header=(source/('src/Geoscape/'+klass+'.h')).read_text();start=header.index('class '+klass+' :');end=header.index('\n};',start)+3
        declaration=header[start:end].replace('protected:','public:').replace('private:','public:')
        (output/(name+'Class.generated.inc')).write_text(declaration)
    game = (source / 'src/Engine/Game.cpp').read_text()
    (output / 'GameIdentity.generated.inc').write_text(block(game, 'bool Game::isState('))
    state = (source / 'src/Engine/State.cpp').read_text()
    (output / 'StateBase.generated.inc').write_text('\n'.join(block(state, s) for s in [
        'bool State::agentCursorMotionSafe(', 'void State::setModal(', 'void State::think(',
        'bool State::isScreen(', 'void State::toggleScreen(', 'void State::hideAll(', 'void State::showAll(',
    ]))


def integrate(source, sdk, data, output, sanitize=False):
    source, sdk, data, output = [Path(p).resolve() for p in (source, sdk, data, output)]
    support = load_support(source)
    fixture = source / 'tests/agent-control/AgentGlobeTargetInfoTest.cpp'
    units = [source / ('src/' + u + '.cpp') for u in ['Observer/AgentSoundPlayback'] + support.UNITS]
    pending = units + [fixture] + [source / n for n in (
        'src/Geoscape/MultipleTargetsState.cpp', 'src/Geoscape/MultipleTargetsState.h',
        'src/Geoscape/TargetInfoState.cpp', 'src/Geoscape/TargetInfoState.h',  'src/Observer/AgentOutfitting.h',
        'src/Engine/Game.cpp', 'src/Engine/State.cpp')]
    seen = set()
    while pending:
        path = pending.pop().resolve()
        if path in seen:
            continue
        seen.add(path)
        for name in re.findall(r'^\s*#include\s+"([^"\n]+)"', path.read_text(), re.M):
            child = (path.parent / name).resolve()
            if child.is_file() and source in child.parents:
                pending.append(child)
    assets = sorted(p.resolve() for p in data.rglob('*') if p.is_file())
    if not assets:
        raise ValueError('No runtime data files')
    inputs = seen | set(assets) | {Path(__file__).resolve(), Path(__file__).with_name('native_test_runner.py').resolve(),
                                   source / 'tests/agent-control/run-dogfight-providers.py'}
    flags = ['c++', '-std=c++11', '-ffunction-sections', '-fdata-sections',
             '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL', '-D__NO_OPENGL',
             '-I' + str(sdk / 'include/SDL'), '-I' + str(sdk / 'include'), '-I' + str(output)]
    flags += (['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize=vptr,enum',
               '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if sanitize else ['-O0'])
    library = sdk / 'lib/x86_64-linux-gnu'
    env = {'LD_LIBRARY_PATH': str(library) + ':' + os.environ.get('LD_LIBRARY_PATH', ''),
           'ASAN_OPTIONS': 'detect_leaks=0'}
    def step(name, argv, cwd=source):
        return dict(name=name, argv=[str(arg) for arg in argv], cwd=str(cwd), env=dict(env))
    steps = [
        step('extract', [sys.executable, Path(__file__).resolve(), '--source', source,
                         '--sdk', sdk, '--data', data, '--output', output, '--extract-only']),
        step('syntax_start', flags + ['-fsyntax-only', source / 'src/Geoscape/MultipleTargetsState.cpp']),
        step('syntax_select', flags + ['-fsyntax-only', source / 'src/Geoscape/TargetInfoState.cpp']),
        step('build', flags + [fixture] + units + ['-Wl,--gc-sections', '-L' + str(library),
             '-lSDL', '-lSDL_gfx', '-lSDL_image', '-lSDL_mixer', '-lyaml-cpp', '-o', output / 'test']),
        step('test', [output / 'test', data, output / 'native.log'], data / 'common'),
    ]
    receipt = run_recipe(dict(version=1, source=str(source), output=str(output),
                              inputs=sorted(str(p) for p in inputs), steps=steps))
    qualification = dict(all_passed=receipt['all_passed'], inputs_unchanged=receipt['inputs_unchanged'],
                         game_executed=False, live_acceptance=False, generic_receipt='RESULT.json',
                         limitations=[
        'Actual constructor/provider/callbacks and native SDL widgets; Game, Mod/interface and target/lifetime model classes are explicit seams; opening interception/info states is a destination seam.',
        'Native target lifetime guards and popup callback bodies retained; synthetic grayscale palette, not stock pixel appearance or full campaign acceptance.',
        'Explicit source/header/data inputs are sealed; compiler, SDK and dynamic-library dependency closure is not exhaustive.',
        'ASan/UBSan excludes vptr, enum and leak checking.' if sanitize else 'No sanitizer in this run.'])
    (output / 'QUALIFICATION.json').write_text(json.dumps(qualification, indent=2) + '\n')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('source', 'sdk', 'data', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--extract-only', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.extract_only:
        extract(args.source.resolve(), args.output.resolve())
        return 0
    return 0 if integrate(args.source, args.sdk, args.data, args.output, args.sanitize)['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
