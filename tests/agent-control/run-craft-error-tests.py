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
        'CraftErrorState::CraftErrorState(', 'CraftErrorState::~CraftErrorState(',
        'void CraftErrorState::btnOkClick(', 'void CraftErrorState::btnOk5SecsClick(',
        'bool CraftErrorState::agentDestinationFull(',
        'std::vector<std::uint64_t> CraftErrorState::agentPaintRevisions(',
        'void CraftErrorState::blit(', 'bool CraftErrorState::agentUiState(',
        'bool CraftErrorState::agentDispatch(',
    ]
    body = (source / 'src/Geoscape/CraftErrorState.cpp').read_text()
    (output / 'CraftError.generated.inc').write_text('\n'.join(block(body, s) for s in signatures))
    geo = (source / 'src/Geoscape/GeoscapeState.cpp').read_text()
    (output / 'GeoscapeTimer.generated.inc').write_text(block(geo, 'void GeoscapeState::timerReset('))
    game = (source / 'src/Engine/Game.cpp').read_text()
    (output / 'GameIdentity.generated.inc').write_text(block(game, 'bool Game::isState('))
    state = (source / 'src/Engine/State.cpp').read_text()
    (output / 'StateBase.generated.inc').write_text('\n'.join(block(state, s) for s in [
        'bool State::agentCursorMotionSafe(', 'void State::setModal(', 'void State::think(',
        'bool State::isScreen(', 'void State::toggleScreen(', 'void State::hideAll(', 'void State::showAll(',
    ]))
    fixture = source / 'tests/agent-control/CraftErrorProviderTest.cpp'
    files = list((source / 'src').rglob('*.cpp')) + list((source / 'src').rglob('*.h'))
    files += [fixture, helper, Path(__file__).resolve(), data / 'common/Language/Font.dat']
    files += [source / ('bin/standard/' + game + '/Language/en-US.yml') for game in ('xcom1', 'xcom2')]
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    hashes = {str(p): sha(p) for p in files}
    asset_roots = [(data / name).resolve(strict=True) for name in ('common', 'UFO', 'TFTD')]
    asset_hashes = {str(p): sha(p) for root in asset_roots for p in root.rglob('*') if p.is_file()}
    if not asset_hashes:
        raise RuntimeError('No runtime assets sealed')
    result = {'all_passed': False, 'source_sha256': hashes, 'live_acceptance': False,
              'game_executed': False, 'runtime_asset_sha256': asset_hashes, 'limitations': [
                  'Actual source constructor/provider/callbacks and native SDL widgets.',
                  'Game, Mod/interface setup, Geoscape speed selection and rendering loop are explicit seams.',
                  'Literal OK, OK5Secs and Geoscape timerReset; no Craft/resource object or service effects qualified.',
                  'Synthetic grayscale palette; no real encounter or stock pixel-appearance qualification.',
                  'Sanitizers exclude vptr,enum and leak checking; not full UBSan.' if args.sanitize else 'No sanitizer in this run.']}
    save = lambda: (output / 'RESULT.json').write_text(json.dumps(result, indent=2) + '\n')
    flags = ['c++', '-std=c++11', '-ffunction-sections', '-fdata-sections',
             '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL', '-D__NO_OPENGL',
             '-I' + str(sdk / 'include/SDL'), '-I' + str(sdk / 'include'), '-I' + str(output)]
    flags += (['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize=vptr,enum',
               '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2'])
    library = sdk / 'lib/x86_64-linux-gnu'
    env = dict(os.environ, LD_LIBRARY_PATH=str(library) + ':' + os.environ.get('LD_LIBRARY_PATH', ''),
               ASAN_OPTIONS='detect_leaks=0')
    result['compile_command'] = flags + [str(fixture)] + [str(source / ('src/' + u + '.cpp')) for u in support.UNITS] + [
        '-Wl,--gc-sections', '-L' + str(library), '-lSDL', '-lSDL_gfx', '-lSDL_image', '-lSDL_mixer', '-lyaml-cpp', '-o', str(output / 'test')]
    save()
    syntax = subprocess.run(flags + ['-fsyntax-only', str(source / 'src/Geoscape/CraftErrorState.cpp')], env=env, capture_output=True, text=True)
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
    result['inputs_unchanged'] = all(sha(Path(p)) == h for p, h in hashes.items()) and all(sha(Path(p)) == h for p,h in asset_hashes.items())
    result['all_passed'] = result['inputs_unchanged'] and test.returncode == 0 and 'PASS ' in test.stdout
    save()
    print(test.stdout + test.stderr)
    return 0 if result['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
