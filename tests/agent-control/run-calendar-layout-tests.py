#!/usr/bin/env python3
"""Offscreen actual-font calendar/layout regression; no game loop or live acceptance."""
import argparse, hashlib, json, os, re, runpy, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--data',type=Path,required=True);p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true')
a=p.parse_args();source=Path(__file__).resolve().parents[2];out=a.output.resolve();sdk=a.sdk.resolve()
if out==source or source in out.parents:p.error('output must be new and outside source')
out.mkdir(parents=True,exist_ok=False)
code=(source/'src/Geoscape/GeoscapeState.cpp').read_text()
names=['Hour','HourSep','Min','MinSep','Sec','Weekday','Day','Month','Year','Funds']
buttons=['5Secs','1Min','5Mins','30Mins','1Hour','1Day']
geometry=[]
for prefix,kind,items in [('_txt','Text',names),('_btn','TextButton',buttons)]:
 for name in items:
  variable=prefix+name
  match=re.search(r'\b'+variable+r' = new '+kind+r'\([^;]+;',code)
  if not match:raise RuntimeError('missing current constructor '+variable)
  geometry.append(kind+' *'+match[0])
(out/'CalendarGeometry.generated.inc').write_text('\n'.join(geometry)+'\n')
funds=re.search(r'if \(Options::showFundsOnGeoscape\)\s*\{([^}]+)\}',code)
if not funds:raise RuntimeError('missing funds layout')
(out/'CalendarFunds.generated.inc').write_text(funds[1])
background=code.split('// CalendarBackground: begin',1)[1].split('\n',1)[1].split('// CalendarBackground: end.',1)[0]
(out/'CalendarBackground.generated.inc').write_text(background)
units=runpy.run_path(str(source/'tests/agent-control/run-named-button-tests.py'))['UNITS']
files=[source/'tests/agent-control/AgentCalendarLayoutTest.cpp']+[source/'src'/f'{x}.cpp' for x in units]
sha=lambda q:hashlib.sha256(q.read_bytes()).hexdigest()
result={'all_passed':False,'live_acceptance':False,'game_executed':False,'limitations':['Current constructor expressions and funds branch, real Text/TextButton and stock fonts; no Game/desktop.','English month labels plus mixed-case/Cyrillic samples; not exhaustive translations.','Original UFO/TFTD GEOBORD assets and button style; no full game loop or live screenshot.'],'source_sha256':{str(q.relative_to(source)):sha(q) for q in files+[source/'src/Geoscape/GeoscapeState.cpp']},'steps':[]}
# Record the included source headers and actual font inputs, not only the fixture.
pending=list(files); closure=set(files+[source/'src/Geoscape/GeoscapeState.cpp'])
while pending:
 q=pending.pop()
 for inc in re.findall(r'^\s*#include\s+"([^"\n]+)"',q.read_text(errors='replace'),re.M):
  header=(q.parent/inc).resolve()
  if header.is_file() and source in header.parents and header not in closure:
   closure.add(header);pending.append(header)
result['source_sha256']={str(q.relative_to(source)):sha(q) for q in sorted(closure)}
assets={str(q):sha(q) for q in sorted((a.data.resolve()/'common/Language').rglob('*')) if q.is_file()}
for game in ['UFO','TFTD']:
 q=a.data.resolve()/game/'GEOGRAPH/GEOBORD.SCR';assets[str(q)]=sha(q)
result['runtime_inputs_sha256']=assets
lib=sdk/'lib/x86_64-linux-gnu';env=dict(os.environ,LD_LIBRARY_PATH=str(lib),ASAN_OPTIONS='detect_leaks=0')
flags=['c++','-std=c++11','-ffunction-sections','-fdata-sections','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-D__NO_OPENGL','-DOPENXCOM_LOADING_PRESENTATION_HELPERS','-I'+str(out),'-I'+str(sdk/'include'),'-I'+str(sdk/'include/SDL')]
flags+=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize=vptr','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
command=flags+list(map(str,files))+['-Wl,--gc-sections','-L'+str(lib),'-Wl,-rpath,'+str(lib),'-lSDL','-lSDL_image','-lSDL_gfx','-lSDL_mixer','-lyaml-cpp','-o',str(out/'test')]
for name,cmd,cwd in [('compile',command,source),('test',[str(out/'test'),str(a.data.resolve())],a.data.resolve()/'common')]:
 r=subprocess.run(cmd,cwd=cwd,env=env,capture_output=True,text=True,timeout=900)
 (out/(name+'.log')).write_text(r.stdout+r.stderr);result['steps'].append({'name':name,'returncode':r.returncode})
 (out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
 print(name,r.returncode,r.stdout[-1000:],r.stderr[-2000:],flush=True)
 if r.returncode:raise SystemExit(r.returncode)
if any(sha(source/q)!=h for q,h in result['source_sha256'].items()):raise RuntimeError('source changed during test')
if any(sha(Path(q))!=h for q,h in assets.items()):raise RuntimeError('font input changed during test')
result['all_passed']=True;result['binary_sha256']=sha(out/'test')
(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
