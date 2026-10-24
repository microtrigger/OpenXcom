#!/usr/bin/env python3
"""Bounded literal-method tests; no game, GUI, resources or synthetic-live claim."""
import argparse, pathlib, subprocess, json, hashlib, os
p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--sanitize',action='store_true');args=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(args.output).resolve();out.mkdir(parents=True,exist_ok=False)
source=(root/'src/Battlescape/Map.cpp').read_text()
def method(signature):
 start=source.index(signature);brace=source.index('{',start);depth=1;i=brace+1
 while depth:
  depth+=(source[i]=='{')-(source[i]=='}');i+=1
 return source[start:i]
(out/'MapContext.generated.inc').write_text('\n'.join(method(x) for x in ['bool Map::agentMapSnapshot(', 'AgentMapCell Map::agentDescribeTerrain(', 'void Map::agentRecordTerrain(']))
paths=['src/Battlescape/Map.cpp','src/Battlescape/Map.h','src/Battlescape/CameraProjection.h','src/Battlescape/BattlescapeState.cpp','tests/agent-control/AgentMapContextTest.cpp','tests/agent-control/run-map-context-tests.py']
sha=lambda x:hashlib.sha256(x.read_bytes()).hexdigest();hashes={x:sha(root/x) for x in paths}
flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-g'] if args.sanitize else ['-O2']
cmd=['c++','-std=c++11','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL',*flags,'-I'+str(out),str(root/'tests/agent-control/AgentMapContextTest.cpp'),'-o',str(out/'test')]
build=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(build.stdout+build.stderr)
result={'source_sha256':hashes,'build_exit':build.returncode,'live':False,'sanitizer_scope':'ASan/UBSan; LeakSanitizer disabled due unsupported ptrace runtime' if args.sanitize else 'none','seams':'Tile/Game/Camera models and render witness seeding; literal production methods, not full renderer or game'}
if build.returncode==0:
 test=subprocess.run([str(out/'test')],capture_output=True,text=True,env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0') if args.sanitize else None);(out/'test.log').write_text(test.stdout+test.stderr);result['test_exit']=test.returncode;print(test.stdout+test.stderr)
result['inputs_unchanged']=all(sha(root/x)==h for x,h in hashes.items());result['all_passed']=result.get('test_exit')==0 and result['inputs_unchanged'];(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');raise SystemExit(0 if result['all_passed'] else 1)
