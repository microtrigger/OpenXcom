#!/usr/bin/env python3
"""Diary fixture integration for the generic runner; trusted source/build only."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import tempfile

from native_test_runner import digest, run_recipe


LIMITATIONS = [
    'Literal native Diary Overview and Performance constructors/init/providers/dispatch/nav/OK and SoldierInfo entry callback; actual TextList/report capture and native owned SoldierDiary/MissionStatistics models.',
    'Owning Game, minimal SoldierInfo constructor/selection setter, State cursor initialization and synthetic interface/background resources are seams. Seeded own mission summaries use stock region/UFO type names; no actual mission completion claimed.',
    'Actual Mission constructor/init/provider/nav/OK executes; missing injury entries remain absent. Seeded historical kill receipts and one native synthetic commendation rule/medal exercise Combat/Awards without claiming live achievements; no GUI/Game/save execution.',
    'The explicit input manifest preserves the original harness source/resource/object closure and adds generator/core/link recipe and generated source files. It is not a complete compiler/header/shared-library transitive dependency manifest.',
]


def integrate(source, build, sdk, data, output):
    source, build, sdk, data, output = [Path(p).resolve() for p in (source, build, sdk, data, output)]
    if output.exists() or output == source or source in output.parents:
        raise ValueError('Output must be new and outside source')
    if not output.parent.is_dir():
        raise ValueError('Output parent must exist for temporary generated inputs')
    helper = source / 'tests/agent-control/run-dogfight-providers.py'
    fixture = source / 'tests/agent-control/AgentDiaryTest.cpp'
    state = source / 'src/Engine/State.cpp'
    link_recipe = build / 'src/CMakeFiles/openxcom.dir/link.txt'
    files = [Path(__file__).resolve(), Path(__file__).with_name('native_test_runner.py').resolve(),
             helper, fixture, state, source / 'src/Menu/ErrorMessageState.cpp', link_recipe]
    files += [source / ('src/' + unit + '.' + ext)
              for unit in ('Basescape/SoldierDiaryOverviewState', 'Basescape/SoldierDiaryPerformanceState',
                           'Basescape/SoldierDiaryMissionState', 'Basescape/SoldierInfoState',
                           'Interface/TextList', 'Observer/AgentProtocol')
              for ext in ('cpp', 'h')]
    files += [data / f'standard/{variant}/{name}.rul'
              for variant in ('xcom1', 'xcom2') for name in ('regions', 'ufos')]
    files += [data / 'common/Language/Font.dat', data / 'common/Language/en-US.yml']
    files += [data / f'standard/{variant}/Language/en-US.yml' for variant in ('xcom1', 'xcom2')]
    generation_before = {str(path): digest(path) for path in files}
    spec = importlib.util.spec_from_file_location('diary_source_support', helper)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    # The helper and fixture are explicitly trusted executable source dependencies.
    link_tokens = shlex.split(link_recipe.read_text())
    excluded = ('/main.cpp.o', '/Engine/State.cpp.o', '/Interface/TextList.cpp.o',
                '/Observer/AgentProtocol.cpp.o')
    objects = [(build / 'src' / token).resolve() for token in link_tokens
               if token.endswith('.o') and not token.endswith(excluded)]
    if not objects:
        raise ValueError('No native build object closure')
    with tempfile.TemporaryDirectory(prefix='diary-inputs-', dir=output.parent) as temporary:
        generated = Path(temporary)
        bodies = []
        for name in ('SoldierDiaryOverviewState', 'SoldierDiaryPerformanceState', 'SoldierDiaryMissionState'):
            text = (source / f'src/Basescape/{name}.cpp').read_text()
            signatures = [f'{name}::{name}(', f'{name}::~{name}(']
            signatures += [m.group(1) for m in re.finditer(
                r'\n((?:void|int|bool|std::vector<std::uint64_t>) ' + name + r'::\w+\()', text)]
            bodies.extend(support.block(text, sig) for sig in signatures)
        (generated / 'Diary.generated.inc').write_text('\n'.join(bodies))
        text = (source / 'src/Basescape/SoldierInfoState.cpp').read_text()
        (generated / 'SoldierInfoEntry.generated.inc').write_text(support.block(text, 'void SoldierInfoState::btnDiaryClick('))
        text = state.read_text()
        needle = '_cursorColor = _game->getCursor()->getColor();'
        if text.count(needle) != 1:
            raise ValueError('Unreviewed State cursor seam')
        text = text.replace(needle, '_cursorColor = 0; // Fixture: no Game/window construction.')
        text = re.sub(r'#include "([^"]+)"', lambda match: '#include "' + str((state.parent / match[1]).resolve()) + '"', text)
        (generated / 'State.generated.cpp').write_text(text)
        if any(digest(Path(path)) != value for path, value in generation_before.items()):
            raise ValueError('Inputs changed during source generation')
        flags = ['c++', '-std=c++11', '-O0', '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL',
                 '-DOPENXCOM_EXPERIMENTAL_OBSERVER_DIAGNOSTICS', '-DOPENXCOM_LOADING_PRESENTATION_HELPERS',
                 '-D__NO_OPENGL', '-I' + str(sdk / 'include/SDL'), '-I' + str(sdk / 'include'), '-I' + str(generated)]
        library = sdk / 'lib/x86_64-linux-gnu'
        overlay = {'LD_LIBRARY_PATH': str(library) + ':' + os.environ.get('LD_LIBRARY_PATH', '')}
        units = [('fixture', fixture), ('state', generated / 'State.generated.cpp'),
                 ('list', source / 'src/Interface/TextList.cpp'),
                 ('protocol', source / 'src/Observer/AgentProtocol.cpp')]
        def step(name, argv, cwd=source):
            return dict(name=name, argv=[str(arg) for arg in argv], cwd=str(cwd), env=overlay.copy())
        steps = [step('compile_' + name, flags + ['-c', unit, '-o', output / (name + '.o')]) for name, unit in units]
        steps.append(step('link', ['c++'] + [output / (name + '.o') for name, unit in units] + objects + [
            '-Wl,--gc-sections', '-L' + str(library), '-lSDL', '-lSDL_image', '-lSDL_mixer', '-lSDL_gfx',
            '-lyaml-cpp', '-ldl', '-pthread', '-o', output / 'test']))
        steps.append(step('test', [output / 'test', data.parent, output], data / 'common'))
        generated_files = sorted(generated.iterdir())
        manifest = list(dict.fromkeys(str(path) for path in files + objects + generated_files))
        # These are real individual compile/link/test steps; the core owns every
        # subprocess execution, exit/log receipt and pre/post input hash check.
        receipt = run_recipe(dict(version=1, source=str(source), output=str(output), inputs=manifest, steps=steps))
        qualification = dict(all_passed=receipt['all_passed'], inputs_unchanged=receipt['inputs_unchanged'],
                             game_executed=False, live_acceptance=False, generic_receipt='RESULT.json',
                             limitations=LIMITATIONS, generated_inputs_temporary=True,
                             generated_source_hashes={path.name: digest(path) for path in generated_files},
                             source_revision='caller-supplied; inspect recorded input hashes')
        (output / 'QUALIFICATION.json').write_text(json.dumps(qualification, indent=2) + '\n')
        return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'sdk', 'data', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    try:
        receipt = integrate(args.source, args.build, args.sdk, args.data, args.output)
    except (ValueError, OSError) as error:
        print('Diary runner refused input: ' + type(error).__name__)
        return 2
    return 0 if receipt['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
