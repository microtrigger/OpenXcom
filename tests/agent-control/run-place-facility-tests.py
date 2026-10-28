#!/usr/bin/env python3
"""Offscreen actual native placement; explicit Game/resource/error-construction seams."""
import argparse,hashlib,importlib.util,json,os,re,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser()
for key in ('source','build','sdk','data','output'):p.add_argument('--'+key,type=Path,required=True)
a=p.parse_args();s,build,sdk,data,out=[getattr(a,k).resolve() for k in ('source','build','sdk','data','output')];out.mkdir(parents=True,exist_ok=False)
spec=importlib.util.spec_from_file_location('support',s/'tests/agent-control/run-dogfight-providers.py');h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
t=(s/'src/Basescape/PlaceFacilityState.cpp').read_text();c='PlaceFacilityState';sigs=[f'bool {c}InkCurrent(',f'{c}::{c}(',f'{c}::~{c}(']+[f'void {c}::{x}(' for x in ['btnCancelClick','viewClick']]+[f'bool {c}::agentDestinationFull(',f'std::vector<std::uint64_t> {c}::agentPaintRevisions(',f'void {c}::blit(',f'bool {c}::agentUiState(',f'bool {c}::agentDispatch(']
(out/'PlaceFacility.generated.inc').write_text('\n'.join(h.block(t,x) for x in sigs))
(out/'ErrorDismiss.generated.inc').write_text(h.block((s/'src/Menu/ErrorMessageState.cpp').read_text(),'void ErrorMessageState::btnOkClick('))
state=s/'src/Engine/State.cpp';t=state.read_text();assert t.count('_cursorColor = _game->getCursor()->getColor();')==1;t=t.replace('_cursorColor = _game->getCursor()->getColor();','_cursorColor = 0; // Fixture: no Game/window construction.')
t=re.sub(r'#include "([^"]+)"',lambda m:'#include "'+str((state.parent/m[1]).resolve())+'"',t);(out/'State.generated.cpp').write_text(t)
flags=['c++','-std=c++11','-O0','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-DOPENXCOM_EXPERIMENTAL_OBSERVER_DIAGNOSTICS','-DOPENXCOM_LOADING_PRESENTATION_HELPERS','-D__NO_OPENGL','-I'+str(sdk/'include/SDL'),'-I'+str(sdk/'include'),'-I'+str(out)]
lib=sdk/'lib/x86_64-linux-gnu';env=dict(os.environ,LD_LIBRARY_PATH=str(lib)+':'+os.environ.get('LD_LIBRARY_PATH',''));sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
files=[Path(__file__).resolve(),s/'tests/agent-control/AgentPlaceFacilityTest.cpp',state,s/'src/Menu/ErrorMessageState.cpp']+[s/f'src/{u}.{ext}' for u in ['Basescape/PlaceFacilityState','Basescape/BaseView','Observer/AgentProtocol'] for ext in ['cpp','h']]
files += [data/'common/Language/Font.dat'] + [data/('standard/'+v+'/Language/en-US.yml') for v in ('xcom1','xcom2')]
result={'all_passed':False,'game_executed':False,'live_acceptance':False,'source_sha256':{str(f):sha(f) for f in files},'limitations':['Actual native BaseView draw/hover/click/placement validation and actual Base/Facility/SavedGame models. Literal PlaceFacilityState constructor/provider/dispatch/viewClick source bodies.','Game ownership, State cursor initialization and synthetic texture/interface resources are seams. Error popup construction is a seam; dismiss handler body and button callback are native.','Stock UFO/TFTD localization/font with synthetic palette/texture, no Game/window/save execution.']}
def store():(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
def run(name,cmd,cwd=s):
 r=subprocess.run([str(x) for x in cmd],cwd=cwd,env=env,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);result[name+'_exit']=r.returncode;store();print(name,r.returncode,r.stdout[-1500:],r.stderr[-5000:],flush=True)
 if r.returncode:raise SystemExit(r.returncode)
units=[('fixture',files[1]),('state',out/'State.generated.cpp'),('view',s/'src/Basescape/BaseView.cpp'),('protocol',s/'src/Observer/AgentProtocol.cpp')]
for name,unit in units:run('compile_'+name,flags+['-c',unit,'-o',out/(name+'.o')])
link=shlex.split((build/'src/CMakeFiles/openxcom.dir/link.txt').read_text());exclude=['/main.cpp.o','/Engine/State.cpp.o','/Basescape/BaseView.cpp.o','/Observer/AgentProtocol.cpp.o'];objects=[(build/'src'/x).resolve() for x in link if x.endswith('.o') and not any(x.endswith(e) for e in exclude)]
result['object_sha256']={str(x):sha(x) for x in objects};store()
run('link',['c++']+[out/(n+'.o') for n,u in units]+objects+['-Wl,--gc-sections','-L'+str(lib),'-lSDL','-lSDL_image','-lSDL_mixer','-lSDL_gfx','-lyaml-cpp','-ldl','-pthread','-o',out/'test'])
run('test',[out/'test',data.parent,out],data/'common');result['inputs_unchanged']=all(sha(f)==v for f,v in result['source_sha256'].items()) and all(sha(f)==v for f,v in result['object_sha256'].items());result['all_passed']=result['inputs_unchanged'];store();raise SystemExit(0 if result['all_passed'] else 1)
