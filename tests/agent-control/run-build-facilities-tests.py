#!/usr/bin/env python3
"""Offscreen facilities preview only; actual widgets and source bodies, explicit domain seams."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--sdk',type=Path,required=True);p.add_argument('--data',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s,sdk,data,out=[x.resolve() for x in (a.source,a.sdk,a.data,a.output)];out.mkdir(parents=True,exist_ok=False)
spec=importlib.util.spec_from_file_location('support',s/'tests/agent-control/run-dogfight-providers.py');h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h);h.UNITS += ['Observer/AgentSoundPlayback','Observer/AgentAudioEvents']
bodies=[]
for c,ordinary in [('PlaceFacilityState',['btnCancelClick']),('BuildFacilitiesState',['PopulateBuildList','init','btnOkClick','lstFacilitiesClick'])]:
 text=(s/f'src/Basescape/{c}.cpp').read_text()
 sigs=[f'bool {c}InkCurrent(',f'{c}::{c}(',f'{c}::~{c}(']+[f'void {c}::{x}(' for x in ordinary]+[f'bool {c}::agentDestinationFull(',f'std::vector<std::uint64_t> {c}::agentPaintRevisions(',f'void {c}::blit(',f'bool {c}::agentUiState(',f'bool {c}::agentDispatch(']
 bodies.extend(h.block(text,x) for x in sigs)
(out/'Facilities.generated.inc').write_text('\n'.join(bodies))
(out/'GameIdentity.generated.inc').write_text(h.block((s/'src/Engine/Game.cpp').read_text(),'bool Game::isState('))
text=(s/'src/Engine/State.cpp').read_text();(out/'StateBase.generated.inc').write_text('\n'.join(h.block(text,x) for x in ['bool State::agentCursorMotionSafe(','void State::setModal(','void State::think(','bool State::isScreen(','void State::toggleScreen(','void State::hideAll(','void State::showAll(']))
fixture=s/'tests/agent-control/BuildFacilitiesProviderTest.cpp';files=[fixture,Path(__file__).resolve()]+[s/f'src/Basescape/{c}.{ext}' for c in ['BuildFacilitiesState','PlaceFacilityState','BasescapeState'] for ext in ['cpp','h']]+[s/f'src/{u}.cpp' for u in h.UNITS];files += [s/'tests/agent-control/run-dogfight-providers.py',data/'common/Language/Font.dat'] + [data/('standard/'+v+'/Language/en-US.yml') for v in ('xcom1','xcom2')];hashes={str(f):h.sha(f) for f in files}
flags=['c++','-std=c++11','-O0','-ffunction-sections','-fdata-sections','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-D__NO_OPENGL','-I'+str(sdk/'include/SDL'),'-I'+str(sdk/'include'),'-I'+str(out)]
lib=sdk/'lib/x86_64-linux-gnu';env=dict(os.environ,LD_LIBRARY_PATH=str(lib)+':'+os.environ.get('LD_LIBRARY_PATH',''))
result={'all_passed':False,'live_acceptance':False,'game_executed':False,'source_sha256':hashes,'limitations':['Game/Mod/rule-enumeration/base-view/render-loop/lifecycle seams; actual native widgets, constructors, providers, dispatch, row selection, scroll and cancel handlers.','No placement handler execution, economic construction or native BaseView qualification.','Synthetic grayscale palette; UFO/TFTD stock localization/font.']}
def run(name,cmd,cwd=s):
 r=subprocess.run([str(x) for x in cmd],cwd=cwd,env=env,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);result[name+'_exit']=r.returncode;(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');print(name,r.returncode,r.stdout[-1000:],r.stderr[-4000:],flush=True)
 if r.returncode:raise SystemExit(r.returncode)
run('syntax',flags+['-fsyntax-only']+[s/f'src/Basescape/{c}.cpp' for c in ['BuildFacilitiesState','PlaceFacilityState','BasescapeState']])
run('build',flags+[fixture]+[s/f'src/{u}.cpp' for u in h.UNITS]+['-Wl,--gc-sections','-L'+str(lib),'-lSDL','-lSDL_gfx','-lSDL_image','-lSDL_mixer','-lyaml-cpp','-o',out/'test'])
run('test',[out/'test',data.parent],data/'common');result['inputs_unchanged']=all(h.sha(Path(f))==v for f,v in hashes.items());result['all_passed']=result['inputs_unchanged'];(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
