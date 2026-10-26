#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offscreen native current drawn report cell regression; no live acceptance."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
sys.dont_write_bytecode = True


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--sdk', type=Path, required=True)
    ap.add_argument('--data', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--master', choices=('xcom1',), required=True)
    a = ap.parse_args()
    source, sdk, data, out = [p.resolve() for p in (a.source,a.sdk,a.data,a.output)]
    if out == source or source in out.parents:
        ap.error('output must be outside source')
    if not (data/'common/Language/Font.dat').is_file() or not (data/'UFO').is_dir():
        ap.error('data must contain existing common fonts and UFO resources')
    out.mkdir(parents=True, exist_ok=False)
    provider = source/'src/Interface/TextList.cpp'
    fixture = Path(__file__).resolve().with_name('ReportCellsTest.cpp')
    helper = source/'tests/agent-control/run-dogfight-providers.py'
    spec = importlib.util.spec_from_file_location('native_support',helper)
    support = importlib.util.module_from_spec(spec);spec.loader.exec_module(support)
    units = ['Observer/AgentSoundPlayback'] + support.UNITS
    files = [source/'src'/f'{u}.cpp' for u in units]+[provider,fixture,helper,Path(__file__).resolve()]
    # Seal transitive local headers without loading saves or other user files.
    pending=list(files);seen=set()
    while pending:
        p=pending.pop().resolve()
        if p in seen:continue
        seen.add(p)
        for name in re.findall(r'^\s*#include\s+"([^"\n]+)"',p.read_text(),re.M):
            for child in (p.parent/name,source/'src'/name,source/'src/Geoscape'/name):
                child=child.resolve()
                if child.is_file():
                    pending.append(child);break
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    hashes={str(p):sha(p) for p in seen}
    assets={str(p):sha(p) for p in (data/'common/Language').glob('*') if p.is_file()}
    lib=sdk/'lib/x86_64-linux-gnu'
    if not lib.is_dir():lib=sdk/'lib'
    env=dict(os.environ,LD_LIBRARY_PATH=str(lib)+':'+os.environ.get('LD_LIBRARY_PATH',''),PYTHONDONTWRITEBYTECODE='1')
    flags=['c++','-std=c++11','-O0','-ffunction-sections','-fdata-sections',
           '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-D__NO_OPENGL',
           '-iquote',str(source/'src'),'-I'+str(sdk/'include/SDL'),'-I'+str(sdk/'include'),'-I'+str(out)]
    result={'all_passed':False,'master':a.master,'game_executed':False,'live_acceptance':False,
            'source_sha256':hashes,'runtime_font_assets_sha256':assets,
            'candidate_provider':str(provider),'steps':[],
            'limitations':['Native offscreen TextList widgets; no game/save/models.', 'Inert State/Game linker seams; no game loop or live acceptance.']}
    def save():(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
    def run(name,command,cwd=None):
        p=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True)
        (out/(name+'.log')).write_text(p.stdout+p.stderr)
        result['steps'].append({'name':name,'command':command,'exit_code':p.returncode});save()
        if p.returncode:raise RuntimeError(name+' failed; see '+str(out/(name+'.log')))
        return p.stdout
    save()
    run('syntax',flags+['-fsyntax-only',str(provider)])
    run('build',flags+[str(fixture)]+[str(source/'src'/f'{u}.cpp') for u in units]+[
        '-Wl,--gc-sections','-L'+str(lib),'-lSDL','-lSDL_image','-lSDL_gfx','-lSDL_mixer','-lyaml-cpp','-o',str(out/'test')])
    output=run('test',[str(out/'test')],data/'common')
    result['inputs_unchanged']=all(sha(Path(p))==h for p,h in hashes.items()) and all(sha(Path(p))==h for p,h in assets.items())
    result['all_passed']=result['inputs_unchanged'] and 'PASS ' in output
    result['artifact_sha256']=sha(out/'test');save();print(output)
    return 0 if result['all_passed'] else 1


if __name__=='__main__':raise SystemExit(main())
