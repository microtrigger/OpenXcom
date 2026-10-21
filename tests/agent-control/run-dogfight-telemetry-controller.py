#!/usr/bin/env python3
"""Actual socket/controller/frame telemetry-binding regression; synthetic UI, not live combat."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def includes(text, base):
    return re.sub(r'#include "([^"\n]+)"',
                  lambda match: '#include "'+str((base/match[1]).resolve())+'"', text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--sdk', type=Path, required=True, help='SDK prefix with include/SDL and include')
    parser.add_argument('--output', type=Path, required=True, help='New external evidence directory')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    source, sdk, output = args.source.resolve(), args.sdk.resolve(), args.output.resolve()
    if output == source or source in output.parents:
        parser.error('--output must be outside the source checkout')
    if not (sdk/'include/SDL/SDL.h').is_file():
        parser.error('SDK must contain include/SDL/SDL.h')
    output.mkdir(parents=True, exist_ok=False)
    deps = includes((source/'tests/agent-control/AgentControllerTestDependencies.h').read_text(),
                    source/'tests/agent-control')
    dependency = output/'DogfightTelemetryDependencies.generated.h'
    dependency.write_text(deps)
    controller = (source/'src/Observer/AgentController.cpp').read_text()
    for include in ('AgentAudioEvents.h', '../Engine/Game.h', '../Engine/Screen.h', '../Engine/State.h'):
        old = '#include "'+include+'"'
        if controller.count(old) != 1:
            raise RuntimeError('Controller dependency seam changed: '+include)
        controller = controller.replace(old, '#include "'+str(dependency)+'"')
    clock = ('return std::chrono::duration_cast<std::chrono::milliseconds>(\n'
             '        std::chrono::steady_clock::now().time_since_epoch()).count();')
    if controller.count(clock) != 1:
        raise RuntimeError('Controller clock seam changed')
    controller = controller.replace(clock, 'return agentTestTime;')
    generated = output/'ActualController.cpp'
    generated.write_text(includes(controller, source/'src/Observer'))
    fixture = source/'tests/agent-control/AgentDogfightTelemetryControllerTest.cpp'
    paths = [fixture, source/'tests/agent-control/AgentControllerTestDependencies.h']
    paths += [source/('src/Observer/'+name) for name in (
        'AgentController.cpp', 'AgentController.h', 'AgentFrameCommand.h', 'AgentUi.cpp',
        'AgentUi.h', 'AgentProtocol.cpp', 'AgentProtocol.h', 'AgentPublicEvents.h')]
    source_hash = {str(path.relative_to(source)): sha(path) for path in paths}
    exe = output/'controller-test'
    command = ['c++', '-std=c++11', '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL', '-D__NO_OPENGL',
               '-ffunction-sections', '-fdata-sections', '-I'+str(sdk/'include/SDL'),
               '-I'+str(sdk/'include'), '-I'+str(output), '-Wl,--gc-sections']
    command += (['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize=vptr',
                 '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2'])
    command += [str(fixture), str(generated), str(source/'src/Observer/AgentUi.cpp'),
                str(source/'src/Observer/AgentProtocol.cpp'), '-o', str(exe)]
    result = dict(all_passed=False, game_executed=False, live_acceptance=False,
                  source_sha256=source_hash, runner_sha256=sha(Path(__file__)), command=command,
                  limitations=['Actual Controller/parser/ledger/frame/binding; synthetic dogfight DTOs.',
                               'Game/State/Screen/audio/input/time are explicit test seams.',
                               'Split authority is constructed in the fixture, not provider proof.',
                               'No actual rendering, modal provider, stock encounter or latency qualification.',
                               'Sanitizers disable leaks (executor ptrace) and vptr (dependency seams).'])
    def save():
        (output/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    save()
    build = subprocess.run(command, capture_output=True, text=True)
    (output/'build.log').write_text(build.stdout+build.stderr)
    result['build_exit_code'] = build.returncode
    if build.returncode:
        save()
        raise SystemExit(build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True,
                         env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
    (output/'test.log').write_text(run.stdout+run.stderr)
    result['test_exit_code'] = run.returncode
    result['source_unchanged'] = all(sha(source/relative) == digest for relative,digest in source_hash.items())
    result['all_passed'] = run.returncode == 0 and result['source_unchanged']
    save()
    print(run.stdout+run.stderr, end='')
    if not result['all_passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
