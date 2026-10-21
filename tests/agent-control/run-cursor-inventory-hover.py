#!/usr/bin/env python3
"""Bounded current Inventory mouseOver/movePointer routing with explicit lookup/domain seams."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('extract',root/'tests/agent-control/run-cursor-core.py');extract=importlib.util.module_from_spec(spec);spec.loader.exec_module(extract)
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();out=a.output.resolve()
if out==root or root in out.parents:p.error('output must be outside source')
out.mkdir(parents=True,exist_ok=False)
paths=[root/'src/Battlescape/Inventory.cpp',root/'tests/agent-control/CursorInventoryHoverTest.cpp',Path(__file__).resolve(),root/'tests/agent-control/run-cursor-core.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
seals={str(p):sha(p) for p in paths};source=paths[0].read_text();(out/'CursorInventoryHover.generated.inc').write_text('\n'.join(extract.block(source,s) for s in ['void Inventory::mouseOver(', 'bool Inventory::movePointer(']))
r={'all_passed':False,'source_sha256':seals,'live_acceptance':False,'steps':[]}
flags=['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
for cmd in [['c++','-std=c++11','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL',*flags,'-I'+str(out),str(paths[1]),'-o',str(out/'test')],[str(out/'test')]]:
 c=subprocess.run(cmd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'));r['steps'].append({'command':cmd,'exit_code':c.returncode,'stdout':c.stdout,'stderr':c.stderr});(out/'RESULT.json').write_text(json.dumps(r,indent=2));print(c.stdout,c.stderr)
 if c.returncode:raise SystemExit(c.returncode)
r['all_passed']=all(sha(Path(p))==h for p,h in seals.items());(out/'RESULT.json').write_text(json.dumps(r,indent=2));raise SystemExit(0 if r['all_passed'] else 1)
