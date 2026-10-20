#!/usr/bin/env python3
"""Compare exact current-source OFF/ON button rasters, not full-game rendering."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location('named_support', Path(__file__).with_name('run-named-button-tests.py'))
support = importlib.util.module_from_spec(spec)
spec.loader.exec_module(support)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2])
    ap.add_argument('--sdk', type=Path)
    ap.add_argument('--data', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    source, data, out = a.source.resolve(), a.data.resolve(), a.output.resolve()
    if out == source or source in out.parents:
        ap.error('--output must be outside the checkout')
    if not all((data/d).is_dir() for d in ('common','UFO','TFTD')):
        ap.error('--data must contain common, UFO and TFTD')
    out.mkdir(parents=True, exist_ok=False)
    support.SOURCE = source
    fixture = source/'tests/agent-control/AgentNamedButtonPixelsV21e.cpp'
    files = {str(fixture.relative_to(source)), 'src/Engine/State.cpp'} | {f'src/{u}.cpp' for u in support.UNITS}
    files |= {f'bin/standard/{g}/Language/en-US.yml' for g in ('xcom1','xcom2')}
    result = {'all_passed': False, 'game_executed': False, 'live_acceptance': False,
              'scope': 'Exact current-source OFF/ON offscreen stock-button raster parity only; no historical baseline',
              'source_sha256': support.closure(files), 'steps': [],
              'runner_sha256': support.sha(__file__),
              'helper_sha256': support.sha(Path(__file__).with_name('run-named-button-tests.py'))}
    assets = {str(p): support.sha(p) for d in ('common/Language','UFO','TFTD')
              for p in (data/d).rglob('*') if p.is_file()}
    result['runtime_asset_sha256'] = assets
    def save(): (out/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    save()
    wrapper = out/'Pixels.cpp'
    wrapper.write_text('#include "'+str(fixture)+'"\n#include "'+str(source/'src/Engine/State.h')+'"\nnamespace OpenXcom {\n'+support.block((source/'src/Engine/State.cpp').read_text(), 'void State::setModal(')+'\n}\n')
    for enabled in (False, True):
        name = 'on' if enabled else 'off'
        flags = ['c++','-std=c++11','-O2','-ffunction-sections','-fdata-sections',
                 '-D__NO_OPENGL','-DOPENXCOM_LOADING_PRESENTATION_HELPERS','-DOPENXCOM_NAMED_BUTTON_TEST']
        if enabled: flags += ['-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL']
        env = dict(os.environ)
        libs = []
        if a.sdk:
            sdk = a.sdk.resolve()
            lib = sdk/'lib/x86_64-linux-gnu'
            if not lib.is_dir(): lib = sdk/'lib'
            flags += ['-I'+str(sdk/'include/SDL'),'-I'+str(sdk/'include')]
            libs += ['-L'+str(lib),'-Wl,-rpath,'+str(lib)]
            env['LD_LIBRARY_PATH'] = str(lib)+((':'+env['LD_LIBRARY_PATH']) if env.get('LD_LIBRARY_PATH') else '')
        else:
            flags += shlex.split(subprocess.check_output(['sdl-config','--cflags'], text=True))
            libs += shlex.split(subprocess.check_output(['sdl-config','--libs'], text=True))
        units = [source/f'src/{u}.cpp' for u in support.UNITS if enabled or not u.startswith('Observer/')]
        command = flags+[str(wrapper)]+list(map(str,units))+['-Wl,--gc-sections']+libs+[
            '-lSDL','-lSDL_image','-lSDL_gfx','-lSDL_mixer','-lyaml-cpp','-o',str(out/name)]
        p = subprocess.run(command, capture_output=True, timeout=900)
        (out/(name+'-compile.log')).write_bytes(p.stdout+p.stderr)
        result['steps'].append({'mode':name, 'command':shlex.join(command), 'compile_exit':p.returncode})
        save()
        if p.returncode: raise RuntimeError(name+' compile failed')
        with (out/(name+'.pixels')).open('wb') as stream:
            p = subprocess.run([out/name,source], cwd=data/'common', env=env,
                               stdout=stream, stderr=subprocess.PIPE, timeout=900)
        (out/(name+'-run.log')).write_bytes(p.stderr)
        result['steps'][-1].update(run_exit=p.returncode,
            pixel_bytes=(out/(name+'.pixels')).stat().st_size,
            pixel_sha256=support.sha(out/(name+'.pixels')))
        save()
        print(name, result['steps'][-1], flush=True)
        if p.returncode: raise RuntimeError(name+' run failed')
    if len({s['pixel_sha256'] for s in result['steps']}) != 1:
        raise RuntimeError('OFF/ON pixel mismatch')
    if not all(s['pixel_bytes']>0 for s in result['steps']): raise RuntimeError('empty pixels')
    if not all(support.sha(source/p)==h for p,h in result['source_sha256'].items()):
        raise RuntimeError('source changed during check')
    if not all(support.sha(p)==h for p,h in assets.items()): raise RuntimeError('assets changed')
    if support.sha(__file__) != result['runner_sha256'] or support.sha(Path(__file__).with_name('run-named-button-tests.py')) != result['helper_sha256']:
        raise RuntimeError('runner changed')
    result['all_passed'] = True
    save()
    print('PASS exact current-source OFF/ON raster parity', flush=True)

if __name__ == '__main__': main()
