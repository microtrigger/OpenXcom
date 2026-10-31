# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import argparse,re,json,hashlib
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('baseline',type=Path);a=p.parse_args();S=a.source;N=a.output
baseline_sha256=hashlib.sha256(a.baseline.read_bytes()).hexdigest()
if baseline_sha256!='a3b2fa188da3e3d05ced40c480ccedca59dfe3a10fcd2dfdc671b0b39a270c53':raise SystemExit('baseline does not match pinned public commit')
names=['Provider.generated.inc','Blit.generated.inc','Overlay.generated.inc','InitBaseline.generated.inc','InitFixed.generated.inc','EXTRACTION.json']
for name in names:
 if(N/name).exists():raise SystemExit('refuse overwrite')
def method(file,signature):
 s=(S/file).read_text();start=s.index(signature);clean=re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/',lambda m:' '*len(m[0]),s,flags=re.S);b=clean.index('{',start);depth=1;i=b+1
 while depth:
  if clean[i]=='{':depth+=1
  elif clean[i]=='}':depth-=1
  i+=1
 return s[start:i]+'\n'
f='Basescape/SoldierDiaryPerformanceState.cpp';s=(S/f).read_text();start=s.index('bool SoldierDiaryPerformanceState::agentDestinationFull()');end=s.index('bool SoldierDiaryPerformanceState::agentDispatch(',start);(N/'Provider.generated.inc').write_text(s[start:end])
for name,text in [('Fixed',s),('Baseline',a.baseline.read_text())]:
 start=text.index('void SoldierDiaryPerformanceState::init()');start=text.index('{',start)+1;end=text.index('\n\tif (_list->empty())',start)
 (N/('Init'+name+'.generated.inc')).write_text(text[start:end])
(N/'Blit.generated.inc').write_text(method('Engine/State.cpp','void State::blit()'))
(N/'Overlay.generated.inc').write_text(method('Engine/Game.cpp','bool Game::agentOverlayClear(')+method('Engine/Game.cpp','bool Game::agentNonCursorOverlayClear('))
(N/'EXTRACTION.json').write_text(json.dumps({'baseline_sha256':baseline_sha256,'source':{f:hashlib.sha256((S/f).read_bytes()).hexdigest()for f in [f,'Engine/State.cpp','Engine/Game.cpp']},'scope':'Exact provider/paint/dead-roster methods; init prefix through visibility controls, excludes model population; explicit fixture wrappers; dispatch excluded'},indent=2)+'\n')
