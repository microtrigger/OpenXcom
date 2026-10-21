#!/usr/bin/env python3
"""Operator-created two-base transfer refusal smoke. Setup gate up to600seconds.
Create isolated ordinary campaign/bases manually in the launched game, destination
without stores, then return to Geoscape and focus before creating the marker.
No hidden state or save edits. No shipment commit, full/overfull or arrival claim.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'research-live'))
from transfer_route import TransferRoute
from route import RouteError
from run_research_smoke import Rpc, SmokeError, sha, utc, wait_focus

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source','binary','data','output','focus-ready-file'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--master',choices=('xcom1','xcom2'),required=True)
    p.add_argument('--sdk',type=Path)
    p.add_argument('--source-base',default='MCP Research')
    p.add_argument('--destination-base',default='NEWBASE')
    p.add_argument('--item',required=True,help='Exact visible ordinary item name, e.g. Pistol or Dart Gun')
    p.add_argument('--focus-timeout',type=float,default=600)
    a=p.parse_args()
    source,binary,data,out,focus=(getattr(a,k).resolve() for k in ('source','binary','data','output','focus_ready_file'))
    if out==source or source in out.parents:p.error('output must be outside checkout')
    if focus.exists():p.error('focus marker must not exist')
    if not 0<a.focus_timeout<=600:p.error('focus timeout must be in (0,600]')
    if not binary.is_file() or not os.access(binary,os.X_OK):p.error('binary must be executable')
    if not all((data/d).is_dir() for d in ('common','UFO' if a.master=='xcom1' else 'TFTD')):p.error('missing legal data')
    out.mkdir(parents=True,exist_ok=False)
    user,config=out/'user',out/'config';user.mkdir();config.mkdir()
    (config/'options.cfg').write_text('mods:\n'+''.join('  - active: '+str(g==a.master).lower()+'\n    id: '+g+'\n' for g in ('xcom1','xcom2'))+'options:\n  language: en-US\n  autosave: false\n')
    scripts=source/'tools/mcp-observer'
    files=list((source/'src').rglob('*.cpp'))+list((source/'src').rglob('*.h'))+list(scripts.glob('*.py'))+list(Path(__file__).resolve().parent.glob('*.py'))+list((Path(__file__).resolve().parents[1]/'research-live').glob('*.py'))
    hashes={str(f):sha(f) for f in files}
    result=dict(started=utc(),all_passed=False,master=a.master,scope='operator-created two-base zero-storage refusal/cancel only',
                source_base=a.source_base,destination_base=a.destination_base,operator_source_slot='b0',operator_destination_slot='b1',item=a.item,full_overfull_tested=False,keyboard_tested=False,game_left_open=True,
                dispatch_budget=128,source_runner_sha256=hashes,binary_sha256=sha(binary),steps=[])
    def save():(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
    def record(kind,**kw):result['steps'].append(dict(time=utc(),kind=kind,**kw));save()
    save()
    cmd=[sys.executable,'-B',str(scripts/'launch_presentation.py'),'--agent-control','--',str(binary),'-data',str(data),'-user',str(user),'-config',str(config),'-master',a.master]
    flags={'autosave':'false','debug':'false','debugUi':'false','traceAI':'false','verboseLogging':'false','useOpenGL':'false','asyncBlit':'false','fullscreen':'false','displayWidth':'960','displayHeight':'600','playIntro':'false','fpsCounter':'false','geoscapeScale':'0','battlescapeScale':'0','nonSquarePixelRatio':'false','customInitialBase':'false','showFundsOnGeoscape':'false'}
    for k,v in flags.items():cmd+=['-'+k,v]
    env=dict(os.environ)
    if a.sdk:
        lib=a.sdk.resolve()/'lib/x86_64-linux-gnu'
        if not lib.is_dir():lib=a.sdk.resolve()/'lib'
        env['LD_LIBRARY_PATH']=str(lib)+(':'+env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
    rpc=None
    try:
        process=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,env=env)
        rpc=Rpc(process)
        (out/'ready.json').write_text(json.dumps(dict(time=utc(),launcher_pid=process.pid,game_ready=False,instruction='Using ordinary visible UI, create source FIRST (b0) and destination SECOND (b1) with requested exact names; destination has no stores. Return to Geoscape, focus this game, then create marker. No save editing.'))+'\n')
        wait_focus(focus,a.focus_timeout);record('operator_focus_marker');rpc.initialize()
        TransferRoute(rpc.call,record,a.master,source_name=a.source_base,destination_name=a.destination_base,item=a.item).run()
        result['all_passed']=True
    except Exception as e:
        result['failure']=str(e) if isinstance(e,(SmokeError,RouteError)) else 'local_runtime_failure'
    finally:
        if rpc:
            try:rpc.close()
            except Exception:pass
        result['inputs_unchanged']=binary.is_file() and sha(binary)==result['binary_sha256'] and all(Path(f).is_file() and sha(f)==h for f,h in hashes.items())
        if not result['inputs_unchanged']:result['all_passed']=False;result['failure']='source_binary_or_runner_changed'
        result['finished']=utc();save()
    print('PASS' if result['all_passed'] else 'PARTIAL: '+result.get('failure','unknown'))
    print('Game remains open; close normally after inspecting final screen.')
    return 0 if result['all_passed'] else 1

if __name__=='__main__':sys.exit(main())
