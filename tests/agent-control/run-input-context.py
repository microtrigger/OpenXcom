#!/usr/bin/env python3
"""Bounded actual input-context serializer and native control bodies with synthetic dependencies."""
import argparse,hashlib,json,os,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
s=pathlib.Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
def between(t,start,end):
 if t.count(start)!=1 or t.count(end)!=1: raise RuntimeError("nonunique extraction anchors")
 return t[t.index(start):t.index(end)].rstrip()
def fn(t,sig):
 start=t.index(sig);i=t.index('{',start)+1;n=1
 while n:n+=(t[i]=='{')-(t[i]=='}');i+=1
 return t[start:i]
b=(s/'src/Battlescape/BattlescapeGame.cpp').read_text();v=(s/'src/Battlescape/BattlescapeState.cpp').read_text()
helper=between(v,'std::string BattlescapeState::agentInputContext(', 'bool BattlescapeState::agentUiState(')
start=b.index('\t\t\t\t// after throwing');start=b.rfind('if (_save->getSide() == FACTION_PLAYER)',0,start);i=b.index('{',start)+1;n=1
while n:n+=(b[i]=='{')-(b[i]=='}');i+=1
block=b[start:i]
(o/'Methods.generated.inc').write_text(helper+'\n'+fn(b,'void BattlescapeGame::setupCursor()')+'\n'+fn(b,'bool BattlescapeGame::cancelCurrentAction(bool bForce)')+'\nvoid BattlescapeGame::afterShot(BattleAction action,bool actionFailed){'+block+'}\n')
enums=[]
for f,name in [('src/Battlescape/Map.h','CursorType'),('src/Battlescape/BattlescapeGame.h','BattleActionType')]:
 t=(s/f).read_text();start=t.index('enum '+name+' ');enums.append(t[start:t.index(';',start)+1])
(o/'Enums.generated.inc').write_text('\n'.join(enums))
# Exact source insertion and unchanged binding presence: structural, not full provider execution.
if 'out.visible += ",\\"input_context\\":" + agentInputContext' not in v: raise RuntimeError('missing insertion')
if not (v.index('out.visible += ",\\"input_context\\":')<v.index('out.controlVisible = out.visible;')): raise RuntimeError("source binding check failed")
if not ('current->targeting ? 1ULL : 0ULL' in v): raise RuntimeError("source binding check failed")
files=[s/x for x in ['src/Battlescape/BattlescapeState.cpp','src/Battlescape/BattlescapeState.h','src/Battlescape/BattlescapeGame.cpp','src/Battlescape/BattlescapeGame.h','src/Battlescape/Map.h','tests/agent-control/AgentInputContextTest.cpp','tests/agent-control/run-input-context.py']]
r={'passed':False,'seams':['synthetic selected actor/save/map/parent/pathfinding','literal native post-shot block extracted, not whole state machine','provider insertion structural, helper JSON actual'],'hashes':{str(x.relative_to(s)):hashlib.sha256(x.read_bytes()).hexdigest() for x in files}}
cmd=['c++','-std=c++11','-Wall','-Wextra','-I'+str(o),str(files[-2]),'-o',str(o/'test')]+(['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'] if a.sanitize else ['-O2'])
try:
 for name,c in [('compile',cmd),('run',[str(o/'test')])]:
  env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0'
  z=subprocess.run(c,capture_output=True,text=True,timeout=120,env=env);(o/(name+'.log')).write_text(z.stdout+z.stderr);r[name]=z.returncode;print(name,z.returncode,z.stdout,z.stderr,flush=True)
  if z.returncode:raise RuntimeError(name+' failed')
 r['passed']=True
finally:(o/'RESULT.json').write_text(json.dumps(r,indent=2)+'\n')
