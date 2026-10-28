#!/usr/bin/env python3
"""Native model collector checks using an existing full-build object closure; does not run Game."""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source',required=True,type=Path);p.add_argument('--build',required=True,type=Path);p.add_argument('--sdk',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args();s,build,sdk,out=[x.resolve() for x in (a.source,a.build,a.sdk,a.output)];out.mkdir(parents=True,exist_ok=False)
flags=['c++','-std=c++11','-O0','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-DOPENXCOM_EXPERIMENTAL_OBSERVER_DIAGNOSTICS','-DOPENXCOM_LOADING_PRESENTATION_HELPERS','-D__NO_OPENGL','-I'+str(sdk/'include/SDL'),'-I'+str(sdk/'include')]
lib=sdk/'lib/x86_64-linux-gnu';env=dict(os.environ,LD_LIBRARY_PATH=str(lib)+':'+os.environ.get('LD_LIBRARY_PATH',''));sha=lambda x:hashlib.sha256(Path(x).read_bytes()).hexdigest()
files=[s/'src/Observer/AgentPlayerBases.cpp',s/'src/Observer/AgentPlayerBases.h',s/'tests/agent-control/AgentPlayerBasesTest.cpp',Path(__file__).resolve()]
result={'all_passed':False,'game_executed':False,'live_acceptance':False,'source_sha256':{str(x):sha(x) for x in files},'limitations':['Actual native models/collector, synthetic in-memory rules injected through test-only private access.','No Game/window/campaign-load/player-save execution. Full build object closure reused and sealed below.','Non-Target entity IDs track observed live bindings; unseen deletion/address reuse between captures is not distinguishable. IDs never authorize mutations.']}
def store():(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
def run(name,cmd):
 r=subprocess.run([str(x) for x in cmd],cwd=s,env=env,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);result[name+'_exit']=r.returncode;store();print(name,r.returncode,r.stdout[-1000:],r.stderr[-3000:],flush=True)
 if r.returncode:raise SystemExit(r.returncode)
run('compile_fixture',flags+['-c',files[2],'-o',out/'fixture.o']);run('compile_collector',flags+['-c',files[0],'-o',out/'collector.o'])
link=shlex.split((build/'src/CMakeFiles/openxcom.dir/link.txt').read_text());objects=[(build/'src'/x).resolve() for x in link if x.endswith('.o') and not x.endswith('/main.cpp.o') and not x.endswith('/AgentPlayerBases.cpp.o')]
result['object_sha256']={str(x):sha(x) for x in objects};store()
run('link',['c++',out/'fixture.o',out/'collector.o']+objects+['-Wl,--gc-sections','-L'+str(lib),'-lSDL','-lSDL_image','-lSDL_mixer','-lSDL_gfx','-lyaml-cpp','-ldl','-pthread','-o',out/'test'])
run('test',[out/'test',out/'records.json']);result['inputs_unchanged']=all(sha(f)==v for f,v in result['source_sha256'].items()) and all(sha(f)==v for f,v in result['object_sha256'].items());result['all_passed']=result['inputs_unchanged'];store();raise SystemExit(0 if result['all_passed'] else 1)
