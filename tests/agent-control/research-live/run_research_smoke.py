#!/usr/bin/env python3
"""Operator-focused real-game smoke. Leaves game open; close it normally afterward.
Requires explicit matching WIP source/binary and legal stock data. No focus bypass,
GUI automation, save access, asset copying or mutation replay is performed.
"""
import argparse
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

from route import Route, RouteError

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

    def request(self, method, params):
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
        if not isinstance(reply, dict) or reply.get('id') != self.ident or 'error' in reply:
            raise SmokeError('invalid_rpc_response')
        return reply.get('result')

    def initialize(self):
        result = self.request('initialize', {'protocolVersion':'2025-11-25', 'capabilities':{},
                              'clientInfo':{'name':'research-smoke','version':'1'}})
        if not isinstance(result, dict) or result.get('protocolVersion') != '2025-11-25':
            raise SmokeError('protocol_mismatch')
        self.send({'jsonrpc':'2.0','method':'notifications/initialized'})
        result = self.request('tools/list', {})
        names = {t.get('name') for t in result.get('tools', [])}
        if not {'read_ui_state','execute_ui_action'} <= names:
            raise SmokeError('semantic_tools_missing')

    def call(self, name, args):
        result = self.request('tools/call', {'name':name,'arguments':args})
        if not isinstance(result, dict):
            raise SmokeError('invalid_tool_result')
        obj = result.get('structuredContent')
        if not isinstance(obj, dict):
            raise SmokeError('missing_structured_content')
        if result.get('isError') or obj.get('status') in ('outcome_unknown','unknown_after_dispatch'):
            raise SmokeError('tool_error_stop_no_replay')
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
    ap.add_argument('--verify-visible-clock', action='store_true', help='Require observed public Geoscape clock advance separately during each completion wait')
    ap.add_argument('--manufacture-detail', action='store_true', help='Requires completion-allocation; inspect exact labelled production detail then cancel, never start')
    ap.add_argument('--completion-allocation', action='store_true', help='Check released scientist baseline, manufacture notice/allocation without production start, and final pause')
    ap.add_argument('--article-navigation', action='store_true', help='Exercise offered scroll down/up, then next/previous and require original title before close')
    ap.add_argument('--allow-unknown-selection', action='store_true', help='Allow one uniquely visible null-label selection per project; confirm detail title or cancel once and stop')
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--binary', type=Path, required=True)
    ap.add_argument('--data', type=Path, required=True)
    ap.add_argument('--master', choices=('xcom1','xcom2'), required=True)
    ap.add_argument('--output', type=Path, required=True, help='New isolated directory outside checkout')
    ap.add_argument('--sdk', type=Path, help='Optional dependency prefix (usr)')
    ap.add_argument('--focus-ready-file', type=Path, required=True,
                    help='Must not exist; operator creates after observing and focusing own game window')
    ap.add_argument('--focus-timeout', type=float, default=120)
    a = ap.parse_args()
    if a.manufacture_detail and not a.completion_allocation:ap.error('--manufacture-detail requires --completion-allocation')
    source = a.source.resolve()
    binary, data, out, focus = (p.resolve() for p in (a.binary,a.data,a.output,a.focus_ready_file))
    if not 0 < a.focus_timeout <= 120: ap.error('focus timeout must be in (0,120]')
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
    result = {'verify_visible_clock':a.verify_visible_clock, 'manufacture_detail':a.manufacture_detail, 'completion_allocation':a.completion_allocation, 'article_navigation':a.article_navigation, 'allow_unknown_selection':a.allow_unknown_selection, 'runner_sha256':runner_hashes, 'started':utc(), 'all_passed':False, 'master':a.master,
              'binary_sha256':sha(binary), 'source_sha256':hashes, 'steps':[],
              'game_left_open':True, 'scope':'Ordinary fresh stock research smoke; not full v22 acceptance'}
    def save(): (out/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    def record(kind, **values):
        result['steps'].append(dict(time=utc(), kind=kind, **values)); save()
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
            'instruction':'Verify and focus own game window, then create focus-ready file.'})+'\n')
        wait_focus(focus,a.focus_timeout)
        record('operator_focus_marker')
        rpc.initialize()
        Route(rpc.call,record,a.master,allow_unknown_selection=a.allow_unknown_selection,article_navigation=a.article_navigation,completion_allocation=a.completion_allocation,manufacture_detail=a.manufacture_detail,verify_visible_clock=a.verify_visible_clock).run()
        if not all(sha(Path(__file__).resolve().parent/p)==h for p,h in runner_hashes.items()) or sha(binary) != result['binary_sha256'] or not all(sha(source/p)==h for p,h in hashes.items()):
            raise SmokeError('source_or_binary_changed')
        result['all_passed'] = True
    except Exception as error:
        result['failure'] = str(error) if isinstance(error, (SmokeError,RouteError)) else 'local_runtime_failure'
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
