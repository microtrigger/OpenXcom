#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual reserve projection/dispatch/handler bodies with explicit synthetic dependencies."""
import argparse, pathlib, subprocess, json, hashlib
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
s=pathlib.Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
t=(s/'src/Battlescape/BattlescapeState.cpp').read_text()
def block(start,end):return t[t.index(start):t.index(end,t.index(start))]
def fn(sig):
 i=t.index(sig);j=t.index('{',i)+1;n=1
 while n:n+=(t[j]=='{')-(t[j]=='}');j+=1
 return t[i:j]
provider=block('    // Read the drawn radio-button selection','    // Only admitted camera-grid points')
dispatch=block('    if (request.action == "reserve_none"','    if (request.action == "map_primary"')
(o/'Methods.inc').write_text('void BattlescapeState::project(Observer::AgentUiState &out) const {\n'+provider+'}\n'+ 'bool BattlescapeState::dispatch(const Observer::AgentRequest &request) { Observer::AgentUiState shown;project(shown);if(!shown.offers(request.action))return false;\n'+dispatch+'return false;}\n'+fn('bool BattlescapeState::allowButtons(bool allowSaving) const')+'\n'+fn('void BattlescapeState::btnReserveClick(Action *action)'))
files=[s/'tests/agent-control/AgentReserveControlsTest.cpp',s/'src/Battlescape/BattlescapeState.cpp']
result={'passed':False,'seams':['synthetic SDL Action/button, save, map, battle game and widget visibility','literal reserve provider, dispatch branch, allowButtons and btnReserveClick bodies','full paint/overlay and controller freshness require separate qualification'],'hashes':{str(f.relative_to(s)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}}
try:
 for name,cmd in [('compile',['g++','-std=c++11','-Wall','-Wextra','-I'+str(o),str(files[0]),'-o',str(o/'test')]),('run',[str(o/'test')])]:
  r=subprocess.run(cmd,capture_output=True,text=True,timeout=120);(o/(name+'.log')).write_text(r.stdout+r.stderr);print(name,r.returncode,r.stdout,r.stderr);result[name]=r.returncode
  if r.returncode:raise RuntimeError(name)
 result['passed']=True
finally:(o/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
