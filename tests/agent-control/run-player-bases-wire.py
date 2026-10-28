#!/usr/bin/env python3
"""Real controller read-method bodies with deterministic collector/transport seams; no live proof."""
import argparse,hashlib,json,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();r=pathlib.Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
s=(r/'src/Observer/AgentController.cpp').read_text()
def block(sig):
 a=s.index(sig);b=s.index('{',a);depth=1;i=b+1
 while depth:
  if s[i]=='{':depth+=1
  elif s[i]=='}':depth-=1
  i+=1
 return s[a:i]
(o/'Controller.generated.inc').write_text(block('void AgentController::resetPlayerBases()')+'\n'+block('void AgentController::playerBasesPage('))
source=r/'tests/agent-control/AgentPlayerBasesWireTest.cpp';protocol=r/'src/Observer/AgentProtocol.cpp'
result=dict(passed=False,scope='actual controller read/reset bodies; synthetic collector/transport, not full Game or live runtime',source_sha256={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (source,protocol,r/'src/Observer/AgentController.cpp',pathlib.Path(__file__))})
try:
 for name,cmd in [('compile',['c++','-std=c++11','-I'+str(o),str(source),str(protocol),'-o',str(o/'test')]),('run',[str(o/'test')])]:
  z=subprocess.run(cmd,capture_output=True,text=True,timeout=60);(o/(name+'.log')).write_text(z.stdout+z.stderr);result[name]=z.returncode;print(name,z.returncode,z.stdout,z.stderr[-1500:])
  if z.returncode:raise RuntimeError(name+' failed')
 result['passed']=True
finally:(o/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
