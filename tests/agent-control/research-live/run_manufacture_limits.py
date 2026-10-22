#!/usr/bin/env python3
"""Manual-entry manufacture limits qualification (no automated research/completion).
Requires legal stock data. Operator prepares campaign through ordinary UI before
creating the fresh focus marker. No save access, audio forwarding or replay.
"""
import argparse
from tool_failure_evidence import safe_tool_failure
import datetime
import hashlib
import json
import os
from pathlib import Path
import selectors
import subprocess
import sys
import time
import uuid

from route import RouteError
from limits_route import LimitsRoute as Route

class SmokeError(RuntimeError):
    pass

def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def public_state(state):
    return {'state': state.get('state'), 'revision': state.get('revision'),
            'actions': [a.get('id') for a in state.get('actions', [])]}

class Rpc:
    def __init__(self, process, timeout=30):
        self.process, self.timeout, self.ident, self.buffer = process, timeout, 0, b''
        self.selector = selectors.DefaultSelector()
        self.selector.register(process.stdout, selectors.EVENT_READ)

    def send(self, obj):
        try:
            self.process.stdin.write(json.dumps(obj, separators=(',', ':')).encode()+b'\n')
            self.process.stdin.flush()
        except (OSError, ValueError):
            raise SmokeError('transport_write_failed') from None

    def request(self, method, params, *, expected_invalid_params=False):
        self.ident += 1
        self.send({'jsonrpc':'2.0','id':self.ident,'method':method,'params':params})
        deadline = time.monotonic()+self.timeout
        while b'\n' not in self.buffer:
            remaining = deadline-time.monotonic()
            if remaining <= 0 or not self.selector.select(remaining):
                raise SmokeError('transport_timeout_no_replay')
            part = os.read(self.process.stdout.fileno(), 65536)
            if not part:
                raise SmokeError('transport_closed_no_replay')
            self.buffer += part
            if len(self.buffer) > 1048576:
                raise SmokeError('response_too_large')
        line, self.buffer = self.buffer.split(b'\n',1)
        try:
            reply = json.loads(line)
        except (ValueError, UnicodeError):
            raise SmokeError('invalid_json_response') from None
        if expected_invalid_params:
            expected = {'jsonrpc':'2.0','id':self.ident,
                        'error':{'code':-32602,'message':'Invalid params'}}
            if reply != expected or type(reply.get('id')) is not int or type(reply['error']['code']) is not int:
                raise SmokeError('unexpected_invalid_probe_response_no_replay')
            return {'status':'invalid_params_rejected','code':-32602,'message':'Invalid params'}
        if (not isinstance(reply, dict) or set(reply) != {'jsonrpc','id','result'}
                or reply.get('jsonrpc') != '2.0' or type(reply.get('id')) is not int
                or reply['id'] != self.ident):
            raise SmokeError('invalid_rpc_response')
        return reply.get('result')

    def initialize(self, verify_retained_history=False):
        result = self.request('initialize', {'protocolVersion':'2025-11-25', 'capabilities':{},
                              'clientInfo':{'name':'research-smoke','version':'1'}})
        if not isinstance(result, dict) or result.get('protocolVersion') != '2025-11-25':
            raise SmokeError('protocol_mismatch')
        self.send({'jsonrpc':'2.0','method':'notifications/initialized'})
        result = self.request('tools/list', {})
        names = {t.get('name') for t in result.get('tools', [])}
        if verify_retained_history and 'read_public_events' not in names:
            raise SmokeError('public_events_tool_missing')
        if not {'read_ui_state','execute_ui_action'} <= names:
            raise SmokeError('semantic_tools_missing')

    def call(self, name, args):
        if name == 'probe_invalid_quantity':
            if (not isinstance(args, dict) or args.get('action') != 'change_production_quantity'
                or args.get('args') != {'control':'units','operation':'set','value':100001}
                or type(args['args']['value']) is not int):
                raise SmokeError('invalid_probe_scope')
            return self.request('tools/call', {'name':'execute_ui_action','arguments':args},
                                expected_invalid_params=True)
        result = self.request('tools/call', {'name':name,'arguments':args})
        if not isinstance(result, dict):
            raise SmokeError('invalid_tool_result')
        obj = result.get('structuredContent')
        if not isinstance(obj, dict):
            raise SmokeError('missing_structured_content')
        failure=obj.get('failure')
        if (result.get('isError') or obj.get('status') in ('outcome_unknown','unknown_after_dispatch')
                or (isinstance(failure,dict) and failure.get('action_outcome')=='unknown')):
            error=SmokeError('tool_error_stop_no_replay')
            error.tool_failure=safe_tool_failure(name,result)
            raise error
        return obj

    def close(self):
        self.selector.close()
        self.process.stdin.close()  # Launcher deliberately leaves game running.
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass  # No terminate/kill, including descendants.
        self.process.stdout.close()

def wait_focus(path, timeout):
    deadline = time.monotonic()+timeout
    while not path.is_file():
        if time.monotonic() >= deadline:
            raise SmokeError('operator_focus_timeout')
        time.sleep(0.2)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--binary', type=Path, required=True)
    ap.add_argument('--data', type=Path, required=True)
    ap.add_argument('--master', choices=('xcom1','xcom2'), required=True)
    ap.add_argument('--output', type=Path, required=True, help='New isolated directory outside checkout')
    ap.add_argument('--sdk', type=Path, help='Optional dependency prefix (usr)')
    ap.add_argument('--focus-ready-file', type=Path, required=True,
                    help='Must not exist; operator creates after observing and focusing own game window')
    ap.add_argument('--focus-timeout', type=float, default=480)
    a = ap.parse_args()
    source = a.source.resolve()
    binary, data, out, focus = (p.resolve() for p in (a.binary,a.data,a.output,a.focus_ready_file))
    if not 0 < a.focus_timeout <= 480: ap.error('focus timeout must be in (0,480]')
    if out == source or source in out.parents: ap.error('output must be outside checkout')
    if focus.exists(): ap.error('focus-ready file must not already exist')
    if not binary.is_file() or not os.access(binary, os.X_OK): ap.error('binary must be executable')
    if not all((data/d).is_dir() for d in ('common','UFO' if a.master=='xcom1' else 'TFTD')):
        ap.error('data must contain common and selected stock game')
    out.mkdir(parents=True, exist_ok=False)
    user, config = out/'user', out/'config'
    user.mkdir(); config.mkdir()
    (config/'options.cfg').write_text('mods:\n'+''.join(
        '  - active: '+str(g==a.master).lower()+'\n    id: '+g+'\n' for g in ('xcom1','xcom2'))+
        'options:\n  language: en-US\n  autosave: false\n')
    scripts = source/'tools/mcp-observer'
    files = list((source/'src').rglob('*.cpp'))+list((source/'src').rglob('*.h'))
    files += list((source/'tests/agent-report-fixture').glob('*'))+[p for p in scripts.glob('*.py') if not p.name.startswith('test_')]
    hashes = {str(p.relative_to(source)):sha(p) for p in files if p.is_file()}
    runner_hashes={p.name:sha(p) for p in Path(__file__).resolve().parent.glob('*.py')}
    result = {'runner_sha256':runner_hashes,'started':utc(),'all_passed':False,'master':a.master,
              'binary_sha256':sha(binary),'source_sha256':hashes,'steps':[],
              'game_left_open':True,'limits_verified':False,'research_automation_claimed':False,
              'scope':'Manual UI setup, then bounded manufacture limits and reopen/Stop; no completion or refund claim',
              'phase_dispatch_limits':{'limits':128}}
    def save(): (out/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    def record(kind, **values):
        result['steps'].append(dict(time=utc(), kind=kind, **values))
        if kind=='limits_phase_complete':result['limits_verified']=True
        save()
    save()
    command = [sys.executable,'-B',str(scripts/'launch_presentation.py'),'--agent-control','--',str(binary),
               '-data',str(data),'-user',str(user),'-config',str(config),'-master',a.master]
    flags = {'autosave':'false','debug':'false','debugUi':'false','traceAI':'false','verboseLogging':'false',
             'useOpenGL':'false','asyncBlit':'false','fullscreen':'false','displayWidth':'960','displayHeight':'600',
             'playIntro':'false','fpsCounter':'false','geoscapeScale':'0','battlescapeScale':'0',
             'nonSquarePixelRatio':'false','customInitialBase':'false','showFundsOnGeoscape':'false'}
    for key, value in flags.items(): command += ['-'+key,value]
    env = dict(os.environ)
    if a.sdk:
        lib = a.sdk.resolve()/'lib/x86_64-linux-gnu'
        if not lib.is_dir(): lib = a.sdk.resolve()/'lib'
        env['LD_LIBRARY_PATH'] = str(lib)+(':'+env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
    rpc = None
    try:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.DEVNULL, env=env)
        rpc = Rpc(process)
        (out/'ready.json').write_text(json.dumps({'time':utc(), 'launcher_started':True,
            'game_ready':False,'launcher_pid':process.pid,'master':a.master,
            'instruction':'Prepare ordinary campaign, research stock pistol, open empty Manufacture, focus game, then create marker.'})+'\n')
        wait_focus(focus,a.focus_timeout)
        record('operator_focus_marker')
        rpc.initialize()
        record('manual_manufacture_setup',research_automation_claimed=False)
        Route(rpc.call,record,a.master).run()
        if not all(sha(Path(__file__).resolve().parent/p)==h for p,h in runner_hashes.items()) or sha(binary) != result['binary_sha256'] or not all(sha(source/p)==h for p,h in hashes.items()):
            raise SmokeError('source_or_binary_changed')
        if not result['limits_verified']:raise SmokeError('limits_phase_not_verified')
        result['all_passed'] = True
    except Exception as error:
        result['failure'] = str(error) if isinstance(error, (SmokeError,RouteError)) else 'local_runtime_failure'
        if hasattr(error,'tool_failure'):result['tool_failure']=error.tool_failure
    finally:
        if rpc:
            try: rpc.close()
            except Exception: pass
        try:
            result['inputs_unchanged'] = sha(binary)==result['binary_sha256'] and all(sha(source/p)==h for p,h in hashes.items()) and all(sha(Path(__file__).resolve().parent/p)==h for p,h in runner_hashes.items())
        except OSError:
            result['inputs_unchanged'] = False
        if not result['inputs_unchanged']:
            result['all_passed']=False; result['failure']='source_binary_or_runner_changed'
        result['finished'] = utc(); save()
    print('PASS' if result['all_passed'] else 'PARTIAL: '+result.get('failure','unknown'))
    print('Game may remain open; operator must close its window normally.')
    return 0 if result['all_passed'] else 1

if __name__ == '__main__':
    sys.exit(main())
