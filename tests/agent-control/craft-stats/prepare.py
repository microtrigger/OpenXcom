# SPDX-License-Identifier: GPL-3.0-or-later
"""Exact source extraction for offscreen provider test, never executes game code."""
from pathlib import Path
import argparse,re,json,hashlib
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args();S=a.source;N=a.output
if not N.is_dir():raise SystemExit('existing exclusive build directory required')
for name in ('Provider.generated.inc','Blit.generated.inc','Overlay.generated.inc','EXTRACTION.json'):
 if (N/name).exists():raise SystemExit('refuse generated-file overwrite')
s=(S/'Ufopaedia/ArticleState.cpp').read_text();start=s.index('void ArticleState::agentRegisterText(');end=s.index('bool ArticleState::agentDispatch(',start);(N/'Provider.generated.inc').write_text(s[start:end])
def method(file,signature):
 s=(S/file).read_text();start=s.index(signature);clean=re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/',lambda m:' '*len(m[0]),s,flags=re.S);b=clean.index('{',start);depth=1;i=b+1
 while depth:
  if clean[i]=='{':depth+=1
  elif clean[i]=='}':depth-=1
  i+=1
 return s[start:i]+'\n'
(N/'Blit.generated.inc').write_text(method('Engine/State.cpp','void State::blit()'))
(N/'Overlay.generated.inc').write_text(method('Engine/Game.cpp','bool Game::agentOverlayClear(')+method('Engine/Game.cpp','bool Game::agentNonCursorOverlayClear('))
(N/'EXTRACTION.json').write_text(json.dumps({n:hashlib.sha256((S/n).read_bytes()).hexdigest()for n in ['Ufopaedia/ArticleState.cpp','Engine/State.cpp','Engine/Game.cpp']},indent=2)+'\n')
