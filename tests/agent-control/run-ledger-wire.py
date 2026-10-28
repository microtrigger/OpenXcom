#!/usr/bin/env python3
"""Real controller rollover/poll bodies with local socket and deterministic UI seams; no live proof."""
import argparse,hashlib,json,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();r=pathlib.Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
s=(r/'src/Observer/AgentController.cpp').read_text()
def block(sig):
 a=s.index(sig)
 # Controller functions end at column zero; avoids counting JSON literal braces.
 b=s.index('\n}',a)+2
 return s[a:b]
(o/'LedgerController.generated.inc').write_text('\n'.join(block(sig) for sig in ('std::string AgentController::resultPacket(', 'void AgentController::receipt(', 'void AgentController::rollover(', 'void AgentController::poll()')))
source=r/'tests/agent-control/AgentLedgerWireTest.cpp';protocol=r/'src/Observer/AgentProtocol.cpp'
result=dict(passed=False,scope='actual controller poll/rollover/receipt bodies and protocol, local socket; synthetic UI/ledger completion, not Game or live runtime',source_sha256={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (source,protocol,r/'src/Observer/AgentController.cpp',pathlib.Path(__file__))})
try:
 for name,cmd in [('compile',['c++','-std=c++11','-I'+str(o),str(source),str(protocol),'-o',str(o/'test')]),('run',[str(o/'test')])]:
  z=subprocess.run(cmd,capture_output=True,text=True,timeout=60);(o/(name+'.log')).write_text(z.stdout+z.stderr);result[name]=z.returncode;print(name,z.returncode,z.stdout,z.stderr[-1500:])
  if z.returncode:raise RuntimeError(name+' failed')
 result['passed']=True
finally:(o/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
