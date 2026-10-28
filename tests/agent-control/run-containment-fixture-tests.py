#!/usr/bin/env python3
"""Prepare-only containment fixture syntax/arguments; no Game or campaign setup."""
import argparse
import json
import os
from pathlib import Path
import sys

from native_test_runner import run_recipe


def verify_symbols(path):
    symbols = Path(path).read_text()
    return ('AgentReportFixture::prepare(int&, char**)' in symbols and
            'AgentReportFixture::start(' not in symbols and 'OpenXcom::Game::Game(' not in symbols)


def integrate(source, sdk, output):
    source, sdk, output = [Path(p).resolve() for p in (source, sdk, output)]
    script = Path(__file__).resolve()
    fixture = source / 'tests/agent-report-fixture/AgentReportFixture.cpp'
    arguments = source / 'tests/agent-control/AgentReportFixtureArgumentsTest.cpp'
    architecture = ['test_containment_fixture_architecture.py', 'test_report_fixture_architecture.py',
                    'test_inventory_fixture_architecture.py']
    # The inventory architecture test reads the production source subtree. Hash
    # that explicit source closure rather than just the top-level Python tests.
    files = [script, script.with_name('native_test_runner.py'), source / 'CMakeLists.txt', fixture, arguments]
    files += [source / 'tests/agent-control' / name for name in architecture]
    files += [source / 'tests/agent-control/test_strategy_widget_architecture.py']
    files += sorted((source / 'src').rglob('*.cpp')) + sorted((source / 'src').rglob('*.h'))
    files += sorted((source / 'tests/agent-report-fixture').glob('*.h'))
    library = sdk / 'lib/x86_64-linux-gnu'
    files.append(library / 'libyaml-cpp.so')
    overlay = {'LD_LIBRARY_PATH': str(library) + ':' + os.environ.get('LD_LIBRARY_PATH', ''),
               'PYTHONDONTWRITEBYTECODE': '1'}
    def step(name, argv):
        return dict(name=name, argv=[str(arg) for arg in argv], cwd=str(source), env=overlay.copy())
    flags = ['c++', '-std=c++11', '-D__NO_OPENGL', '-DOPENXCOM_LOADING_PRESENTATION_HELPERS',
             '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL', '-DOPENXCOM_EXPERIMENTAL_AGENT_REPORT_FIXTURE',
             '-I' + str(sdk / 'include'), '-I' + str(sdk / 'include/SDL'), '-ffunction-sections', '-fdata-sections']
    steps = [step('compile_fixture', flags + ['-c', fixture, '-o', output / 'fixture.o']),
             step('link_arguments', flags + [arguments, output / 'fixture.o', library / 'libyaml-cpp.so',
                                            '-Wl,--gc-sections', '-o', output / 'arguments']),
             step('isolated_symbols', ['nm', '-C', '--defined-only', output / 'arguments']),
             step('verify_symbols', [sys.executable, '-B', script, '--verify-symbols', output / 'isolated_symbols.log'])]
    for alias in ('alien-control-v1', 'gameplay-v1', 'gameplay-armored-v1', 'gameplay-vehicle-v1',
                  'gameplay-mission-v1', 'containment-stock-v1'):
        steps.append(step('arguments-' + alias, [output / 'arguments', alias]))
    for optimized in (False, True):
        for name in architecture:
            steps.append(step(('optimized-' if optimized else 'normal-') + Path(name).stem,
                              [sys.executable, '-B'] + (['-O'] if optimized else []) + [source / 'tests/agent-control' / name]))
    receipt = run_recipe(dict(version=1, source=str(source), output=str(output),
                             inputs=list(dict.fromkeys(str(path.resolve()) for path in files)), steps=steps))
    qualification = dict(all_passed=receipt['all_passed'], inputs_unchanged=receipt['inputs_unchanged'],
                         game_executed=False, setup_runtime_executed=False, live_acceptance=False,
                         generic_receipt='RESULT.json', source_revision='caller-supplied; inspect recorded input hashes',
                         scope='Actual fixture compilation, prepare-only native arguments, isolated-symbol verification and selected source-architecture tests.',
                         limitations=['No campaign setup or Game execution. Resource-dependent mission architecture and live acceptance are separate suites, not claimed here.',
                                      'The manifest includes architecture-scanned source files; compiler/SDK header and toolchain transitive dependency closure is not complete.'])
    (output / 'QUALIFICATION.json').write_text(json.dumps(qualification, indent=2) + '\n')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-symbols', type=Path, help=argparse.SUPPRESS)
    for name in ('source', 'sdk', 'output'):
        parser.add_argument('--' + name, type=Path)
    args = parser.parse_args()
    if args.verify_symbols:
        return 0 if verify_symbols(args.verify_symbols) else 1
    if any(getattr(args, name) is None for name in ('source', 'sdk', 'output')):
        parser.error('--source, --sdk and --output are required')
    try:
        receipt = integrate(args.source, args.sdk, args.output)
    except (ValueError, OSError) as error:
        print('Containment fixture runner refused input: ' + type(error).__name__)
        return 2
    return 0 if receipt['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
