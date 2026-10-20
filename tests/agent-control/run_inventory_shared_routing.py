#!/usr/bin/env python3
"""Compile actual inventory method bodies with scoped, explicit model/UI shims.

No full build, linked game, SDL/video initialization, assets, socket, or live
acceptance. Outputs remain under the isolated /11 native-inventory-routing dir.
"""
from pathlib import Path
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import shlex
import subprocess
import time
from test_inventory_routing_architecture import ROOT, BASELINE, BASELINE_SHA256, function

HERE = Path(__file__).resolve().parent
INVENTORY_METHODS = ('mouseOver','mouseClick','movePointer','activateButton','returnHeldItem',
                     'unload','setSelectedItem','setMouseOverItem','getSlotInPosition',
                     'moveItem','overlapItems','canBeStacked','fitItem')
RULE_METHODS = ('checkSlotInPosition','fitItemInSlot','getCost')


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,default=ROOT.parent/'native-inventory-routing')
    parser.add_argument('--sanitized',action='store_true',help='Also run ASan/UBSan, with leak checking disabled')
    args=parser.parse_args()
    out=args.out.resolve()
    if ROOT.parent.resolve() not in out.parents:
        parser.error('Output must be below the authorized isolated /11 root')
    out.mkdir(parents=True,exist_ok=True)
    baseline_path=BASELINE/'src/Battlescape/Inventory.cpp'
    baseline_raw=baseline_path.read_bytes()
    if sha(baseline_raw)!=BASELINE_SHA256:
        raise RuntimeError('Frozen /10 Inventory.cpp seal does not match')
    inventory_path=ROOT/'src/Battlescape/Inventory.cpp'
    rules_path=ROOT/'src/Mod/RuleInventory.cpp'
    inventory_raw=inventory_path.read_bytes();rules_raw=rules_path.read_bytes()
    inputs={str(inventory_path):sha(inventory_raw),str(rules_path):sha(rules_raw),str(baseline_path):sha(baseline_raw),
            str(HERE/'AgentInventorySharedRoutingTest.cpp'):sha((HERE/'AgentInventorySharedRoutingTest.cpp').read_bytes()),
            str(HERE/'test_inventory_routing_architecture.py'):sha((HERE/'test_inventory_routing_architecture.py').read_bytes()),
            str(Path(__file__)):sha(Path(__file__).read_bytes())}
    actual=[];extractions=[]
    for path,raw,cls,names,rename in (
            (inventory_path,inventory_raw,'Inventory',INVENTORY_METHODS,None),
            (rules_path,rules_raw,'RuleInventory',RULE_METHODS,None),
            (baseline_path,baseline_raw,'Inventory',('mouseOver','mouseClick'),'BaselineInventory')):
        for name in names:
            qualified=cls+'::'+name
            original=function(raw.decode(),qualified)
            transformed=original if rename is None else original.replace(qualified,rename+'::'+name,1)
            actual.append(transformed)
            extractions.append({'input':str(path),'method':qualified,'original_sha256':sha(original.encode()),
                                'substitution':None if rename is None else qualified+' -> '+rename+'::'+name,
                                'compiled_sha256':sha(transformed.encode())})
    generated=out/'InventorySharedRoutingActual.inc'
    generated.write_text('// Generated from sealed source inputs; do not hand edit.\nnamespace OpenXcom {\n'+'\n\n'.join(actual)+'\n}\n')
    result={'scope':__doc__,'started_utc':datetime.now(timezone.utc).isoformat(),'game_executed':False,'full_build_run':False,'sdl_initialized':False,
            'runtime_acceptance':False,'supervised_gates_executed':False,'inputs_sha256':inputs,'extractions':extractions,
            'generated_sha256':sha(generated.read_bytes()),'steps':[],'all_passed':False}
    target=out/'RESULT.json'
    def save():target.write_text(json.dumps(result,indent=2)+'\n')
    configurations=[('agent-on-O0',['-O0','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL']),
                    ('agent-on-O2',['-O2','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL']),
                    ('agent-off-O2',['-O2'])]
    if args.sanitized:
        configurations.append(('agent-on-sanitized',['-O1','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-fsanitize=address,undefined','-fno-omit-frame-pointer']))
    for name,extra in configurations:
        binary=out/name
        command=['c++','-std=c++11','-Wall','-Wextra','-Werror',*extra,'-I',str(out),
                 str(HERE/'AgentInventorySharedRoutingTest.cpp'),'-o',str(binary)]
        for stage,cmd in [('compile',command),('execute',[str(binary)])]:
            started=time.monotonic()
            completed=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,timeout=90,
                                     env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
            step={'configuration':name,'stage':stage,'command':shlex.join(cmd),'exit_code':completed.returncode,
                  'stdout':completed.stdout,'stderr':completed.stderr,'elapsed_seconds':round(time.monotonic()-started,3)}
            result['steps'].append(step);save()
            print(name,stage,completed.returncode,completed.stdout,completed.stderr,flush=True)
            if completed.returncode:
                return completed.returncode
    # Detect concurrent production edits instead of attributing results to bytes
    # that were never compiled. A rerun is required if this seal fails.
    result['inputs_unchanged_after_run']=all(Path(path).is_file() and sha(Path(path).read_bytes())==digest for path,digest in inputs.items())
    result['all_passed']=result['inputs_unchanged_after_run'];save()
    return 0 if result['all_passed'] else 2


if __name__=='__main__':
    raise SystemExit(main())
