#!/usr/bin/env python3
"""Fresh-campaign scientists matrix only. Focus gate required; leaves game open.
Uses ordinary setup and title-confirmed research selection, then checks allocation
through zero/several/maximum/over-limit/back-to-zero without starting research.
Production is NOT included in this runner's acceptance scope.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from allocation_route import AllocationRoute
from route import RouteError
from run_research_smoke import Rpc, SmokeError, sha, utc, wait_focus

class MatrixComplete(Exception):
    pass

class ScientistCampaignRoute(AllocationRoute):
    def confirm_project_title(self, title):
        state = super().confirm_project_title(title)
        self.staff_matrix(title, scientists=True)
        self.record('scientist_campaign_matrix_complete', master=self.master,
                    dispatches=self.count, final_state=self.state['state'],
                    final_visible=self.state['visible'], research_started=False)
        raise MatrixComplete()

    def run(self):
        # Reuse ordinary setup unchanged; stop at the first confirmed project,
        # before base Route.select_project performs its normal allocation.
        try:
            self._run()
        except MatrixComplete:
            return
        raise RouteError('scientist_matrix_checkpoint_not_reached')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source','binary','data','output','focus-ready-file'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--master',choices=('xcom1','xcom2'),required=True)
    p.add_argument('--sdk',type=Path)
    p.add_argument('--allow-unknown-selection',action='store_true')
    p.add_argument('--focus-timeout',type=float,default=120)
    a=p.parse_args()
    source,binary,data,out,focus=(getattr(a,k).resolve() for k in ('source','binary','data','output','focus_ready_file'))
    if out==source or source in out.parents:p.error('output must be outside checkout')
    if focus.exists():p.error('focus marker must not exist')
    if not 0<a.focus_timeout<=120:p.error('focus timeout must be in (0,120]')
    if not binary.is_file() or not os.access(binary,os.X_OK):p.error('binary must be executable')
    if not all((data/d).is_dir() for d in ('common','UFO' if a.master=='xcom1' else 'TFTD')):p.error('missing legal data')
    out.mkdir(parents=True,exist_ok=False)
    user,config=out/'user',out/'config';user.mkdir();config.mkdir()
    (config/'options.cfg').write_text('mods:\n'+''.join('  - active: '+str(g==a.master).lower()+'\n    id: '+g+'\n' for g in ('xcom1','xcom2'))+'options:\n  language: en-US\n  autosave: false\n')
    scripts=source/'tools/mcp-observer'
    files=list((source/'src').rglob('*.cpp'))+list((source/'src').rglob('*.h'))+list(scripts.glob('*.py'))+list(Path(__file__).resolve().parent.glob('*.py'))
    hashes={str(f):sha(f) for f in files}
    result=dict(started=utc(),all_passed=False,master=a.master,scope='fresh campaign scientists allocation only',
                production_tested=False,keyboard_tested=False,game_left_open=True,
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
        (out/'ready.json').write_text(json.dumps(dict(time=utc(),launcher_pid=process.pid,game_ready=False,instruction='Observe and focus this game, then create focus marker.'))+'\n')
        wait_focus(focus,a.focus_timeout);record('operator_focus_marker');rpc.initialize()
        ScientistCampaignRoute(rpc.call,record,a.master,allow_unknown_selection=a.allow_unknown_selection).run()
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
