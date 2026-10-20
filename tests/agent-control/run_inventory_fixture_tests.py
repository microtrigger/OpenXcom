#!/usr/bin/env python3
"""Bounded serialization and prepare-only tests. Never starts Game/GUI or builds the game.

Uses the retained SDK, with new objects only. Link garbage collection removes the
uninvoked start() implementation; symbol inspection verifies that separation.
The ASan/UBSan test disables LeakSanitizer because the managed runner uses ptrace.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk', type=Path, default=ROOT.parent.parent / 'minimal-mcp-build-20261018/sdk/usr')
parser.add_argument('--out', type=Path)
args = parser.parse_args()
sdk = args.sdk.resolve()
out = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix='openxcom-inventory-fixture-'))
out.mkdir(parents=True, exist_ok=True)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
yaml_library = sdk / 'lib/x86_64-linux-gnu/libyaml-cpp.a'
result = {
    'schema': 'openxcom-inventory-fixture-focused/1',
    'scope': __doc__,
    'game_executed': False,
    'game_built': False,
    'arrangement_tested': False,
    'leak_sanitizer_enabled': False,
    'source_sha256': {},
    'compiled_input_sha256': {},
    'binary_sha256': {},
    'steps': [],
}
inputs = [
    'tests/agent-report-fixture/AgentReportFixture.cpp',
    'tests/agent-report-fixture/AgentReportFixture.h',
    'tests/agent-report-fixture/AgentVehicleFixture.h',
    'tests/agent-report-fixture/AgentInventoryFixture.h',
    'tests/agent-control/AgentInventoryFixtureTest.cpp',
    'tests/agent-control/AgentInventoryFixtureArgumentsTest.cpp',
    'tests/agent-control/AgentReportFixtureArgumentsTest.cpp',
    'tests/agent-control/test_inventory_fixture_architecture.py',
    'tests/agent-control/test_mission_fixture_architecture.py',
    'tests/agent-control/test_report_fixture_architecture.py',
    'tests/agent-control/test_strategy_widget_architecture.py',
    'tests/agent-control/run_inventory_fixture_tests.py',
    'bin/standard/xcom1/items.rul',
    'bin/standard/xcom2/items.rul',
    'bin/standard/xcom1/crafts.rul',
    'bin/standard/xcom2/crafts.rul',
    'CMakeLists.txt',
    'src/Savegame/SavedBattleGame.cpp',
    'src/Savegame/BattleItem.cpp',
    'src/Battlescape/BattlescapeGenerator.cpp',
    'src/Battlescape/Inventory.cpp',
    'src/Battlescape/DebriefingState.cpp',
    'src/Battlescape/BattlescapeGame.cpp',
    'src/Battlescape/PsiAttackBState.cpp',
    'src/Observer/AgentController.cpp',
]
result['source_sha256'] = {path: sha(ROOT / path) for path in inputs}
result['compiled_input_sha256'][str(yaml_library)] = sha(yaml_library)
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')


def save():
    (out / 'RESULT.json').write_text(json.dumps(result, indent=2) + '\n')


def run(name, command):
    started = time.monotonic()
    completed = subprocess.run(command, cwd=ROOT, env=env, text=True,
                               capture_output=True, timeout=120)
    result['steps'].append({
        'name': name, 'command': shlex.join(str(arg) for arg in command),
        'cwd': str(ROOT), 'exit_code': completed.returncode,
        'stdout': completed.stdout, 'stderr': completed.stderr,
        'seconds': round(time.monotonic() - started, 3),
    })
    save()
    shown_stdout = completed.stdout if len(completed.stdout) <= 3000 else completed.stdout[:3000] + '\n[remaining output recorded in RESULT.json]\n'
    print(name, completed.returncode, shown_stdout, completed.stderr, flush=True)
    if completed.returncode:
        raise SystemExit(completed.returncode)
    return completed.stdout


run('compiler version', ['c++', '--version'])
common = ['c++', '-std=c++11', '-I' + str(sdk / 'include')]
for variant, sanitizer in [('normal', []), ('sanitized', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'])]:
    binary = out / ('serialization-' + variant)
    run('compile serialization ' + variant, common + ['-Wall', '-Wextra', '-Werror', '-pedantic'] + sanitizer + [
        '-MMD', '-MF', str(out / ('serialization-' + variant + '.d')),
        str(ROOT / 'tests/agent-control/AgentInventoryFixtureTest.cpp'),
        str(yaml_library), '-o', str(binary)])
    run('execute serialization ' + variant, [str(binary), str(ROOT)])
    result['binary_sha256'][binary.name] = sha(binary)

flags = common + ['-D__NO_OPENGL', '-DOPENXCOM_LOADING_PRESENTATION_HELPERS',
                  '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL', '-DOPENXCOM_EXPERIMENTAL_AGENT_REPORT_FIXTURE',
                  '-I' + str(sdk / 'include/SDL'), '-ffunction-sections', '-fdata-sections']
fixture = out / 'fixture.o'
run('compile fixture syntax and current prepare implementation', flags + [
    '-MMD', '-MF', str(out / 'fixture.d'), '-c',
    str(ROOT / 'tests/agent-report-fixture/AgentReportFixture.cpp'), '-o', str(fixture)])
result['binary_sha256'][fixture.name] = sha(fixture)
old_aliases = ['alien-control-v1', 'gameplay-v1', 'gameplay-armored-v1', 'gameplay-vehicle-v1', 'gameplay-mission-v1']
for name, test, aliases in [
    ('arguments', 'AgentInventoryFixtureArgumentsTest.cpp', old_aliases + ['inventory-body-v1', 'inventory-paging-v1']),
    ('legacy-arguments', 'AgentReportFixtureArgumentsTest.cpp', old_aliases),
]:
    binary = out / name
    run('link prepare-only ' + name, flags + ['-MMD', '-MF', str(out / (name + '.d')),
        str(ROOT / 'tests/agent-control' / test), str(fixture), str(yaml_library),
        '-Wl,--gc-sections', '-o', str(binary)])
    symbols = run('verify isolated symbols ' + name, ['nm', '-C', '--defined-only', str(binary)])
    if 'OpenXcom::AgentReportFixture::prepare(int&, char**)' not in symbols or any(
            forbidden in symbols for forbidden in ['OpenXcom::AgentReportFixture::start(', 'OpenXcom::Game::Game(']):
        raise RuntimeError('Prepare-only test isolation failed')
    for alias in aliases:
        run(name + ' ' + alias, [str(binary), alias])
    result['binary_sha256'][binary.name] = sha(binary)

for optimized in [False, True]:
    for architecture in ['test_inventory_fixture_architecture.py', 'test_mission_fixture_architecture.py', 'test_report_fixture_architecture.py']:
        run(('optimized ' if optimized else '') + architecture,
            [sys.executable, '-B'] + (['-O'] if optimized else []) + [str(ROOT / 'tests/agent-control' / architecture)])

for dependency_file in sorted(out.glob('*.d')):
    dependencies = shlex.split(dependency_file.read_text().replace('\\\n', ' ').split(':', 1)[1])
    for name in dependencies:
        path = Path(name)
        if not path.is_absolute():
            path = ROOT / path
        path = path.resolve()
        result['compiled_input_sha256'][str(path)] = sha(path)
if result['source_sha256'] != {path: sha(ROOT / path) for path in inputs}:
    raise RuntimeError('Source inputs changed during tests; rerun for a coherent result')
result['all_passed'] = True
save()
digest = sha(out / 'RESULT.json')
(out / 'RESULT.sha256').write_text(digest + '  RESULT.json\n')
print('RESULT:', out / 'RESULT.json')
print('RESULT SHA256:', digest)
