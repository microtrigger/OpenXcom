#!/usr/bin/env python3
"""Compile actual native status parser slice and reply bodies with explicit transport/ledger seams."""
import argparse, hashlib, importlib.util, json, os, subprocess
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 source=a.source.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 helper=source/'tests/agent-control/run-dogfight-providers.py';spec=importlib.util.spec_from_file_location('support',helper);support=importlib.util.module_from_spec(spec);spec.loader.exec_module(support)
 cpp=source/'src/Observer/StanceController.cpp';body=cpp.read_text();block=support.block
 (out/'Token.generated.inc').write_text(block(body,'bool token('))
 (out/'Replies.generated.inc').write_text(block(body,'void StanceController::replyStatus(')+'\n'+block(body,'void StanceController::unavailable('))
 poll=block(body,'void StanceController::poll(');start=poll.index("    if (n == 65 && request[0] == 'S'");end=poll.index('    const std::string id = request.substr(65, 32);',start)
 (out/'Parser.generated.inc').write_text(poll[start:end])
 hello=[line.strip() for line in body.splitlines() if 'sendPacket(' in line and 'experimental-stance/2' in line];assert len(hello)==1
 (out/'Hello.generated.inc').write_text(hello[0]+'\n')
 fixture=source/'tests/agent-control/SupervisedStatusQueryTest.cpp';files=[cpp,source/'src/Observer/StanceController.h',fixture,Path(__file__).resolve(),helper]+list(out.glob('*.inc'));sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest();closure={str(f):sha(f) for f in files}
 flags=['c++','-std=c++11','-I'+str(out)]+(['-O1','-g','-fsanitize=address,undefined','-fno-sanitize=vptr,enum','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if a.sanitize else ['-O2'])
 command=flags+[str(fixture),'-o',str(out/'test')];result={'all_passed':False,'source_sha256':closure,'compile_command':command,'limitations':['Literal parser slice includes status gate and ordinary malformed-request rejection; full poll admission/IO and feature-on game paths are omitted.','Ledger, send, close are explicit seams; downstream action sentinel must never be reached. No game or socket execution.','Hello test extracts actual send expression, not full constructor admission.','Sanitizer excludes vptr/enum with leak checking disabled; not full UBSan.' if a.sanitize else 'Normal only.']};save=lambda:(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');save()
 b=subprocess.run(command,capture_output=True,text=True);(out/'build.log').write_text(b.stdout+b.stderr);result['build_exit']=b.returncode;save()
 if b.returncode:print(b.stderr);return 1
 t=subprocess.run([str(out/'test')],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),capture_output=True,text=True);(out/'test.log').write_text(t.stdout+t.stderr);result['test_exit']=t.returncode;result['inputs_unchanged']=all(sha(Path(f))==h for f,h in closure.items());result['all_passed']=t.returncode==0 and result['inputs_unchanged'] and 'PASS ' in t.stdout;save();print(t.stdout+t.stderr);return 0 if result['all_passed'] else 1
if __name__=='__main__':raise SystemExit(main())
