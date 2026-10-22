#!/usr/bin/env python3
"""Actual popup constructor/provider/callback bodies with native SDL widgets and explicit domain seams."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    source, sdk, data, output = [p.resolve() for p in (args.source, args.sdk, args.data, args.output)]
    output.mkdir(parents=True, exist_ok=False)
    helper = source / 'tests/agent-control/run-dogfight-providers.py'
    spec = importlib.util.spec_from_file_location('native_support', helper)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    block = support.block
    signatures = [
        'UfoDetectedState::UfoDetectedState(', 'UfoDetectedState::~UfoDetectedState(',
        'void UfoDetectedState::btnInterceptClick(', 'void UfoDetectedState::btnCentreClick(', 'void UfoDetectedState::btnCancelClick(',
        'bool UfoDetectedState::agentDestinationFull(',
        'std::vector<std::uint64_t> UfoDetectedState::agentPaintRevisions(',
        'void UfoDetectedState::blit(', 'bool UfoDetectedState::agentUiState(',
        'bool UfoDetectedState::agentDispatch(',
    ]
    body = (source / 'src/Geoscape/UfoDetectedState.cpp').read_text()
    (output / 'UfoContactTitle.generated.inc').write_text('\n'.join(block(body, s) for s in signatures))
    game = (source / 'src/Engine/Game.cpp').read_text()
    (output / 'GameIdentity.generated.inc').write_text(block(game, 'bool Game::isState('))
    (output / 'GameOverlays.generated.inc').write_text('\n'.join(block(game, x) for x in ['bool Game::agentOverlayClear(', 'bool Game::agentNonCursorOverlayClear(']))
    state = (source / 'src/Engine/State.cpp').read_text()
    (output / 'StateBase.generated.inc').write_text('\n'.join(block(state, s) for s in [
        'bool State::agentCursorMotionSafe(', 'void State::setModal(', 'void State::think(',
        'bool State::isScreen(', 'void State::toggleScreen(', 'void State::hideAll(', 'void State::showAll(',
    ]))
    fixture = source / 'tests/agent-control/UfoContactTitleProviderTest.cpp'
    files = list((source / 'src').rglob('*.cpp')) + list((source / 'src').rglob('*.h'))
    files += [fixture, helper, Path(__file__).resolve(), data / 'common/Language/Font.dat']
    files += list((data / 'common/Language').glob('*.png'))
    files += [source / ('bin/standard/' + game + '/Language/en-US.yml') for game in ('xcom1', 'xcom2')]
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    hashes = {str(p): sha(p) for p in files}
    result = {'all_passed': False, 'source_sha256': hashes, 'live_acceptance': False,
              'game_executed': False, 'limitations': [
                  'Actual source constructor/provider/callbacks and native SDL widgets.',
                  'Game, Mod/interface setup, Ufo/Geoscape/SavedGame domain and rendering loop are explicit seams.',
                  'Synthetic grayscale palette; no real encounter or stock pixel-appearance qualification.',
                  'Sanitizers exclude vptr and leak checking.' if args.sanitize else 'No sanitizer in this run.']}
    save = lambda: (output / 'RESULT.json').write_text(json.dumps(result, indent=2) + '\n')
    flags = ['c++', '-std=c++11', '-ffunction-sections', '-fdata-sections',
             '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL', '-D__NO_OPENGL',
             '-I' + str(sdk / 'include/SDL'), '-I' + str(sdk / 'include'), '-I' + str(output)]
    flags += (['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize=vptr',
               '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2'])
    library = sdk / 'lib/x86_64-linux-gnu'
    env = dict(os.environ, LD_LIBRARY_PATH=str(library) + ':' + os.environ.get('LD_LIBRARY_PATH', ''),
               ASAN_OPTIONS='detect_leaks=0')
    result['compile_command'] = flags + [str(fixture)] + [str(source / ('src/' + u + '.cpp')) for u in support.UNITS] + [
        '-Wl,--gc-sections', '-L' + str(library), '-lSDL', '-lSDL_gfx', '-lSDL_image', '-lSDL_mixer', '-lyaml-cpp', '-o', str(output / 'test')]
    save()
    syntax = subprocess.run(flags + ['-fsyntax-only', str(source / 'src/Geoscape/UfoDetectedState.cpp')], env=env, capture_output=True, text=True)
    (output / 'syntax.log').write_text(syntax.stdout + syntax.stderr)
    result['syntax_exit'] = syntax.returncode
    if syntax.returncode:
        save()
        raise SystemExit(syntax.stderr)
    build = subprocess.run(result['compile_command'], env=env, capture_output=True, text=True)
    (output / 'build.log').write_text(build.stdout + build.stderr)
    result['build_exit'] = build.returncode
    save()
    if build.returncode:
        raise SystemExit(build.stderr)
    test = subprocess.run([str(output / 'test'), str(source)], cwd=data / 'common', env=env, capture_output=True, text=True)
    (output / 'test.log').write_text(test.stdout + test.stderr)
    result['test_exit'] = test.returncode
    result['inputs_unchanged'] = all(sha(Path(p)) == h for p, h in hashes.items())
    result['all_passed'] = result['inputs_unchanged'] and test.returncode == 0 and 'PASS ' in test.stdout
    save()
    print(test.stdout + test.stderr)
    return 0 if result['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
