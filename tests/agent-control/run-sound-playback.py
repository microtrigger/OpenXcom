#!/usr/bin/env python3
"""Production observer/literal Sound hooks with synthetic mixer+entropy. No game/GUI."""
import argparse,pathlib,hashlib,json,subprocess,os,re
p=argparse.ArgumentParser();p.add_argument('--sdk',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();s=pathlib.Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
def fn(t,sig):
 if t.count(sig)!=1:raise RuntimeError('ambiguous function')
 start=t.index(sig);i=t.index('{',start)+1;n=1
 while n:n+=(t[i]=='{')-(t[i]=='}');i+=1
 return t[start:i]
sound=(s/'src/Engine/Sound.cpp').read_text();bodies=[fn(sound,x) for x in ['void Sound::play(int channel, int angle, int distance) const','void Sound::loop()']];(o/'Sound.generated.inc').write_text('\n'.join(bodies)+'\n')
# Existing ordinary handlers unchanged after removing this opt-in observer blocks.
old=subprocess.check_output(['git','show','10148fbb2a69387cde21843fac7e7a3f17572558:src/Engine/Sound.cpp'],cwd=s,text=True)
for sig,new in zip(['void Sound::play(int channel, int angle, int distance) const','void Sound::loop()'],bodies):
 stripped=re.sub(r'#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL\n.*?#endif\n?','',new,flags=re.S)
 if stripped.split()!=fn(old,sig).split():raise RuntimeError('native handler changed')
controller=(s/'src/Observer/AgentController.cpp').read_text();game=(s/'src/Engine/Game.cpp').read_text();battle=(s/'src/Battlescape/BattlescapeState.cpp').read_text()
if 'sample(ui)' not in fn(controller,'void AgentController::presented()') or 'publicCameraKnown' not in controller:raise RuntimeError('camera presentation binding absent')
if '_agentAudioEvents.poll(_agentController.enabled() && !Observer::AgentSoundPlayback::primaryMode());' not in game:raise RuntimeError('PCM mode isolation absent')
if 'Observer::AgentSoundPlayback::instance().boundary(false);' not in fn(game,'void Game::setSavedGame(SavedGame *save)'):raise RuntimeError('save boundary absent')
if 'ui.publicCameraRevision = map->agentPaintedViewRevision();' not in battle or 'return _agentDrawViewRevision;' not in (s/'src/Battlescape/Map.h').read_text():raise RuntimeError('painted revision anchor absent')
files=['src/Observer/AgentSoundPlayback.h','src/Observer/AgentSoundPlayback.cpp','src/Engine/Sound.cpp','src/Engine/Game.cpp','src/Observer/AgentController.cpp','src/Observer/AgentUi.h','src/Battlescape/BattlescapeState.cpp','src/Battlescape/Map.h','src/CMakeLists.txt','tests/agent-control/AgentSoundPlaybackTest.cpp','tests/agent-control/run-sound-playback.py']
r={'passed':False,'source_hashes':{f:hashlib.sha256((s/f).read_bytes()).hexdigest() for f in files},'limits':['synthetic mixer gains/query/acceptedplay','wrapped deterministic entropy/failure','literal actual Sound play/loop and actual observer','camera cache tested directly; provider/flip/load anchors structural only','no live physical audibility or fullsuite'],'lsan':False}
cmd=['c++','-std=c++11','-pthread','-Wall','-Wextra','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-I'+str(a.sdk.resolve()/'usr/include/SDL'),'-I'+str(o),str(s/files[-2]),str(s/'src/Observer/AgentSoundPlayback.cpp'),'-Wl,--wrap=open','-Wl,--wrap=read','-Wl,--wrap=close','-o',str(o/'test')]+(['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'] if a.sanitize else ['-O2'])
try:
 for name,c in [('compile',cmd),('run',[str(o/'test')])]:
  env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0';z=subprocess.run(c,capture_output=True,text=True,timeout=120,env=env);(o/(name+'.log')).write_text(z.stdout+z.stderr);r[name]=z.returncode;print(name,z.returncode,flush=True)
  if z.returncode:raise RuntimeError(name+' failed')
 r['passed']=True
finally:(o/'RESULT.json').write_text(json.dumps(r,indent=2)+'\n')
