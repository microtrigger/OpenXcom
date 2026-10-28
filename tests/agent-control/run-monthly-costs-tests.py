#!/usr/bin/env python3
"""Offscreen native MonthlyCosts report/provider; explicit owning/resource/rule-subset seams."""
import argparse,hashlib,importlib.util,json,os,re,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser()
for key in ('source','build','sdk','data','output'):p.add_argument('--'+key,type=Path,required=True)
a=p.parse_args();s,build,sdk,data,out=[getattr(a,k).resolve() for k in ('source','build','sdk','data','output')];out.mkdir(parents=True,exist_ok=False)
spec=importlib.util.spec_from_file_location('support',s/'tests/agent-control/run-dogfight-providers.py');h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
c='MonthlyCostsState';t=(s/'src/Basescape/MonthlyCostsState.cpp').read_text();sigs=[f'{c}::{c}(',f'{c}::~{c}(',f'void {c}::btnOkClick(',f'bool {c}::agentDestinationFull(',f'std::vector<std::uint64_t> {c}::agentPaintRevisions(',f'void {c}::blit(',f'bool {c}::agentUiState(',f'bool {c}::agentDispatch(']
(out/'MonthlyCosts.generated.inc').write_text('\n'.join(h.block(t,x) for x in sigs))
(out/'BaseInfoClick.generated.inc').write_text(h.block((s/'src/Basescape/BaseInfoState.cpp').read_text(),'void BaseInfoState::btnMonthlyCostsClick('))
state=s/'src/Engine/State.cpp';t=state.read_text();assert t.count('_cursorColor = _game->getCursor()->getColor();')==1;t=t.replace('_cursorColor = _game->getCursor()->getColor();','_cursorColor = 0; // Fixture: no Game/window construction.')
t=re.sub(r'#include "([^"]+)"',lambda m:'#include "'+str((state.parent/m[1]).resolve())+'"',t);(out/'State.generated.cpp').write_text(t)
flags=['c++','-std=c++11','-O0','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-DOPENXCOM_EXPERIMENTAL_OBSERVER_DIAGNOSTICS','-DOPENXCOM_LOADING_PRESENTATION_HELPERS','-D__NO_OPENGL','-I'+str(sdk/'include/SDL'),'-I'+str(sdk/'include'),'-I'+str(out)]
lib=sdk/'lib/x86_64-linux-gnu';env=dict(os.environ,LD_LIBRARY_PATH=str(lib)+':'+os.environ.get('LD_LIBRARY_PATH',''));sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
files=[Path(__file__).resolve(),s/'tests/agent-control/AgentMonthlyCostsTest.cpp',state,s/'src/Menu/ErrorMessageState.cpp']+[s/f'src/{u}.{ext}' for u in ['Basescape/MonthlyCostsState','Basescape/BaseInfoState','Interface/TextList','Observer/AgentProtocol'] for ext in ['cpp','h']]
files += [data/f'standard/{v}/{f}.rul' for v in ('xcom1','xcom2') for f in ('crafts','soldiers','vars')]
files += [data/'common/Language/Font.dat'] + [data/('standard/'+v+'/Language/en-US.yml') for v in ('xcom1','xcom2')]
result={'all_passed':False,'game_executed':False,'live_acceptance':False,'source_sha256':{str(f):sha(f) for f in files}, 'limitations':['Actual native MonthlyCosts constructor/provider/dispatch/ordinary OK, literal BaseInfo entry callback, actual SDL Text/TextList widgets and native Base/SavedGame/Craft/Soldier/Country cost models.', 'Game ownership, minimal BaseInfo constructor and State cursor initialization are seams. Synthetic palette/background/interface, stock UFO/TFTD cost-only native rule subsets; no full Mod/game/save load.', 'No window/game execution. Report data comes only from painted native logical cells; no provider model fallback.']}
def store():(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
def run(name,cmd,cwd=s):
 r=subprocess.run([str(x) for x in cmd],cwd=cwd,env=env,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);result[name+'_exit']=r.returncode;store();print(name,r.returncode,r.stdout[-1500:],r.stderr[-5000:],flush=True)
 if r.returncode:raise SystemExit(r.returncode)
units=[('fixture',files[1]),('state',out/'State.generated.cpp'),('list',s/'src/Interface/TextList.cpp'),('protocol',s/'src/Observer/AgentProtocol.cpp')]
for name,unit in units:run('compile_'+name,flags+['-c',unit,'-o',out/(name+'.o')])
link=shlex.split((build/'src/CMakeFiles/openxcom.dir/link.txt').read_text());exclude=['/main.cpp.o','/Engine/State.cpp.o','/Interface/TextList.cpp.o','/Observer/AgentProtocol.cpp.o'];objects=[(build/'src'/x).resolve() for x in link if x.endswith('.o') and not any(x.endswith(e) for e in exclude)]
result['object_sha256']={str(x):sha(x) for x in objects};store()
run('link',['c++']+[out/(n+'.o') for n,u in units]+objects+['-Wl,--gc-sections','-L'+str(lib),'-lSDL','-lSDL_image','-lSDL_mixer','-lSDL_gfx','-lyaml-cpp','-ldl','-pthread','-o',out/'test'])
run('test',[out/'test',data.parent,out],data/'common');result['inputs_unchanged']=all(sha(f)==v for f,v in result['source_sha256'].items()) and all(sha(f)==v for f,v in result['object_sha256'].items());result['all_passed']=result['inputs_unchanged'];store();raise SystemExit(0 if result['all_passed'] else 1)
