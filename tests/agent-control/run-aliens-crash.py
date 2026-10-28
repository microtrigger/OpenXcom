#!/usr/bin/env python3
"""Actual crash-recovery popup constructor/provider/callback with native widgets; explicit domain seams."""
import argparse,ast,hashlib,importlib.util,json,os,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--data',type=pathlib.Path,required=True);p.add_argument('--sdk',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False);sdk=a.sdk.resolve();prefix=sdk/'usr' if (sdk/'usr/include').exists() else sdk
# Share the established native widget link-unit inventory, not generated game state.
tree=ast.parse((r/'tests/agent-control/run-dogfight-providers.py').read_text());units=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='UNITS' for t in n.targets));
helper=r/'tests/agent-control/run-dogfight-providers.py';spec=importlib.util.spec_from_file_location('helper',helper);support=importlib.util.module_from_spec(spec);spec.loader.exec_module(support);block=support.block
body=(r/'src/Battlescape/AliensCrashState.cpp').read_text()
(o/'Crash.generated.inc').write_text('\n'.join(block(body,sig) for sig in ['AliensCrashState::AliensCrashState(', 'AliensCrashState::~AliensCrashState(', 'void AliensCrashState::btnOkClick(', 'std::vector<std::uint64_t> AliensCrashState::agentPaintRevisions(', 'void AliensCrashState::blit(', 'bool AliensCrashState::agentUiState(', 'bool AliensCrashState::agentDispatch(']))
(o/'GameIdentity.generated.inc').write_text(block((r/'src/Engine/Game.cpp').read_text(),'bool Game::isState('))
(o/'StateBase.generated.inc').write_text('\n'.join(block((r/'src/Engine/State.cpp').read_text(),sig) for sig in ['bool State::agentCursorMotionSafe(', 'void State::setModal(', 'void State::think(', 'bool State::isScreen(', 'void State::toggleScreen(', 'void State::hideAll(', 'void State::showAll(']))
units+=['Observer/AgentSoundPlayback']
flags=['c++','-std=c++11','-O2','-ffunction-sections','-fdata-sections','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-D__NO_OPENGL','-DOPENXCOM_LOADING_PRESENTATION_HELPERS','-I'+str(o),'-I'+str(prefix/'include/SDL'),'-I'+str(prefix/'include')]
lib=prefix/'lib/x86_64-linux-gnu';env=dict(os.environ,LD_LIBRARY_PATH=str(lib));result={'passed':False,'scope':'native constructor/provider/callback; synthetic Game/Mod/Debriefing lifecycle, palette; not live gameplay'}
files=[r/('src/'+u+'.cpp') for u in units]+[r/'src/Battlescape/AliensCrashState.h',r/'src/Battlescape/AliensCrashState.cpp',pathlib.Path(__file__).resolve(),r/'tests/agent-control/AliensCrashProviderTest.cpp']
files += [a.data.resolve()/('standard/'+g+'/Language/en-US.yml') for g in ('xcom1','xcom2')]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
result['source_sha256']={str(p):sha(p) for p in files}
try:
 cmds=[('compile',flags+[str(r/'tests/agent-control/AliensCrashProviderTest.cpp')]+[str(r/('src/'+u+'.cpp')) for u in units]+['-Wl,--gc-sections','-L'+str(lib),'-Wl,-rpath,'+str(lib),'-lSDL','-lSDL_image','-lSDL_gfx','-lSDL_mixer','-lyaml-cpp','-o',str(o/'test')]),('run',[str(o/'test'),str(a.data.resolve())])]
 for name,cmd in cmds:
  z=subprocess.run(cmd,capture_output=True,text=True,env=env,cwd=a.data.resolve()/'common',timeout=180);(o/(name+'.log')).write_text(z.stdout+z.stderr);result[name]=z.returncode;print(name,z.returncode,z.stdout[-1500:],z.stderr[-1500:],flush=True)
  if z.returncode:raise RuntimeError(name+' failed')
 if any(sha(pathlib.Path(p))!=h for p,h in result['source_sha256'].items()):raise RuntimeError('source changed during qualification')
 result['artifact_sha256']=sha(o/'test')
 result['passed']=True
finally:(o/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
