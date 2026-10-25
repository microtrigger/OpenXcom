#!/usr/bin/env python3
"""Scoped PCM callback/mixer/transport checks; no game/GUI or physical audibility claim."""
import argparse,pathlib,subprocess,os,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--sdk',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();s=pathlib.Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False);sdk=a.sdk.resolve()
t=(s/'src/Observer/AgentController.cpp').read_text();start='void AgentController::drainAudioEvents()';end='void AgentController::poll()'
if t.count(start)!=1 or t.count(end)!=1:raise RuntimeError('controller extraction ambiguous')
(o/'Drain.generated.inc').write_text(t[t.index(start):t.index(end)])
base=['c++','-std=c++11','-pthread','-I'+str(sdk/'usr/include/SDL'),'-I'+str(o)]+(['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'] if a.sanitize else ['-O2'])
env=dict(os.environ);env['LD_LIBRARY_PATH']=str(sdk/'usr/lib/x86_64-linux-gnu')+':'+env.get('LD_LIBRARY_PATH','');env['ASAN_OPTIONS']='detect_leaks=0'
files=['src/Observer/AgentAudioEvents.cpp','src/Observer/AgentAudioEvents.h','src/Observer/AgentController.cpp','tests/agent-control/AgentAudioEventsTest.cpp','tests/agent-control/AgentAudioMixerTest.cpp','tests/agent-control/AgentAudioPcmTransportTest.cpp','tests/agent-control/run-audio-pcm.py','tests/agent-control/test_audio_events_architecture.py']
r={'passed':False,'sha256':{f:hashlib.sha256((s/f).read_bytes()).hexdigest() for f in files},'results':{},'lsan':False,'game_executed':False}
try:
 for name in ['Events','Mixer','PcmTransport']:
  cmd=base+[str(s/('tests/agent-control/AgentAudio'+name+'Test.cpp'))]
  if name=='PcmTransport':cmd+=[str(s/'src/Observer/AgentProtocol.cpp')]
  else:cmd+=[str(s/'src/Observer/AgentAudioEvents.cpp')]
  if name=='Mixer':cmd+=['-L'+str(sdk/'usr/lib/x86_64-linux-gnu'),'-lSDL_mixer','-lSDL','-Wl,--wrap=Mix_SetPostMix']
  cmd+=['-o',str(o/name)]
  for phase,c in [('compile',cmd),('run',[str(o/name)])]:
   z=subprocess.run(c,capture_output=True,text=True,timeout=120,env=env);(o/(name+'-'+phase+'.log')).write_text(z.stdout+z.stderr);r['results'][name+'-'+phase]=z.returncode;print(name,phase,z.returncode,flush=True)
   if z.returncode:raise RuntimeError(name+' '+phase+' failed')
 r['passed']=True
finally:(o/'RESULT.json').write_text(json.dumps(r,indent=2)+'\n')
