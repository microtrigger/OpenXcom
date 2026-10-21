#!/usr/bin/env python3
"""Extracted real transfer callbacks with explicit domain seams, not live acceptance."""
import argparse, hashlib, json, os, pathlib, subprocess

def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[2])
    p.add_argument('--output', type=pathlib.Path, required=True)
    p.add_argument('--sanitize', action='store_true')
    a = p.parse_args()
    source, out = a.source.resolve(), a.output.resolve()
    if out == source or source in out.parents:
        p.error('output must be outside checkout')
    out.mkdir(parents=True, exist_ok=False)
    transfer = source/'src/Basescape/TransferItemsState.cpp'
    base = source/'src/Savegame/Base.cpp'
    fixture = source/'tests/agent-control/TransferRulesTest.cpp'
    sigs = ['void TransferItemsState::'+s for s in ['increaseByValue(int change)', 'decreaseByValue(int change)', 'completeTransfer()', 'btnCancelClick(Action *)']]
    (out/'Transfer.generated.inc').write_text('\n'.join(function(transfer.read_text(), s) for s in sigs)+'\n')
    (out/'Stores.generated.inc').write_text(function(base.read_text(), 'bool Base::storesOverfull(double offset)')+'\n')
    binary = out/'transfer-rules'
    cmd = ['c++','-std=c++11','-Wall','-Wextra','-pedantic','-I'+str(out),'-I'+str(source/'src'),str(fixture),'-o',str(binary)]
    cmd += ['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
    files = [transfer,base,fixture,source/'src/fmath.h',pathlib.Path(__file__).resolve()]
    result = dict(passed=False, live_acceptance=False, game_executed=False, mcp_dispatch_tested=False,
                  coverage='Non-alien item transfer draft capacity, quantity, cancel and single commit; no personnel/craft/alien acceptance',
                  seams=['Base capacity and storage getters', 'Game navigation/error collection', 'Transfer payload setters', 'ItemContainer removal', 'UI refresh counter'],
                  sha256={str(f.relative_to(source)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files})
    try:
        for name, command in [('compile',cmd),('run',[str(binary)])]:
            env = dict(os.environ)
            if a.sanitize: env['ASAN_OPTIONS']='detect_leaks=0'
            r = subprocess.run(command, capture_output=True, text=True, timeout=120, env=env)
            (out/(name+'.log')).write_text(r.stdout+r.stderr)
            result[name+'_exit_code']=r.returncode
            print(name,r.returncode,r.stdout,r.stderr,flush=True)
            if r.returncode: raise RuntimeError(name+' failed')
        result['passed']=True
    finally:
        (out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__ == '__main__': main()
