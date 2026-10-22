#!/usr/bin/env python3
"""Compile actual Surface/SDL_gfx; synthetic raster, no game or live acceptance."""
import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--fixture-source', type=Path)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expect-empty-clip-failure', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    source = args.source.resolve()
    fixtures = (args.fixture_source or source).resolve()
    output = args.output.resolve()
    sdk = args.sdk.resolve()
    output.mkdir(parents=True, exist_ok=False)

    units = [
        'Engine/Surface', 'Observer/AgentUi', 'Observer/AgentProtocol',
        'Observer/AgentProjectileWitness', 'Engine/Palette', 'Engine/FileMap',
        'Engine/Unicode', 'Engine/Options', 'Engine/CrossPlatform',
        'Engine/RNG', 'lodepng',
    ]
    fixture = fixtures / 'tests/agent-control/SurfaceTexturedPolygonTest.cpp'
    files = [
        *(source / 'src').rglob('*.cpp'),
        *(source / 'src').rglob('*.h'),
        fixture,
        Path(__file__).resolve(),
    ]
    hashes = {str(path): sha(path) for path in files}
    library = sdk / 'lib/x86_64-linux-gnu'
    library_path = str(library)
    if os.environ.get('LD_LIBRARY_PATH'):
        library_path += ':' + os.environ['LD_LIBRARY_PATH']
    env = dict(os.environ, LD_LIBRARY_PATH=library_path, ASAN_OPTIONS='detect_leaks=0')
    command = [
        'c++', '-std=c++11', '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL',
        '-D__NO_OPENGL', '-ffunction-sections', '-fdata-sections',
        '-I' + str(sdk / 'include/SDL'), '-I' + str(sdk / 'include'),
    ]
    if args.sanitize:
        command += [
            '-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize=vptr',
            '-fno-omit-frame-pointer', '-fno-sanitize-recover=all',
        ]
    else:
        command += ['-O2']
    command += [str(fixture)]
    command += [str(source / 'src' / (unit + '.cpp')) for unit in units]
    command += [
        '-Wl,--gc-sections', '-L' + str(library), '-lSDL', '-lSDL_gfx',
        '-lSDL_image', '-lSDL_mixer', '-lyaml-cpp', '-o', str(output / 'test'),
    ]
    result = {
        'all_passed': False,
        'expected_baseline_failure': args.expect_empty_clip_failure,
        'source_sha256': hashes,
        'game_executed': False,
        'live_acceptance': False,
        'sanitizer_scope': (
            'address,undefined excluding vptr (unlinked unrelated UI types)'
            if args.sanitize else 'none'
        ),
        'command': command,
    }

    def save():
        (output / 'RESULT.json').write_text(json.dumps(result, indent=2) + '\n')

    save()
    build = subprocess.run(command, env=env, capture_output=True, text=True)
    (output / 'build.log').write_text(build.stdout + build.stderr)
    result['build_exit_code'] = build.returncode
    save()
    if build.returncode:
        raise SystemExit(build.stderr)

    test = subprocess.run([str(output / 'test')], env=env, capture_output=True, text=True)
    (output / 'test.log').write_text(test.stdout + test.stderr)
    result['test_exit_code'] = test.returncode
    result['source_unchanged'] = all(sha(Path(path)) == value for path, value in hashes.items())
    if args.expect_empty_clip_failure:
        expected = (
            test.returncode == 1
            and test.stderr.strip() == 'FAIL empty-clip poisoned paint audit'
        )
    else:
        expected = test.returncode == 0 and 'PASS ' in test.stdout
    result['all_passed'] = result['source_unchanged'] and expected
    save()
    print(test.stdout + test.stderr, end='')
    return 0 if result['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
