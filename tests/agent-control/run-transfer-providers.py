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
    paths = [source/('src/Basescape/'+name+'State.cpp') for name in ('TransferBase','TransferItems','TransferConfirm')]
    ui = source/'src/Observer/AgentUi.cpp'
    fixture = source/'tests/agent-control/TransferProvidersTest.cpp'
    bodies=[]
    for path in paths:
        cls=path.stem
        bodies += [function(path.read_text(), sig+cls+'::'+name) for sig,name in [('bool ','agentUiState('),('bool ','agentDispatch('),('bool ','agentDestinationFull('),('std::vector<std::uint64_t> ','agentPaintRevisions('),('void ','blit(')]]
    (out/'TransferProviders.generated.inc').write_text('\n'.join(bodies)+'\n')
    (out/'Ui.generated.inc').write_text('\n'.join(function(ui.read_text(), sig) for sig in ('std::string AgentUiState::payload()', 'std::string AgentUiState::authorityPayload()', 'bool AgentUiState::sameBinding(', 'bool AgentUiState::offers(', 'bool agentRegionsDisjoint('))+'\n')
    binary = out/'transfer-providers'
    cmd = ['c++','-std=c++11','-Wall','-Wextra','-pedantic','-I'+str(out),'-I'+str(source/'src'),str(fixture),'-o',str(binary)]
    cmd += ['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
    files = paths+[ui,fixture,source/'src/Observer/AgentUi.h',pathlib.Path(__file__).resolve()]
    result = dict(passed=False, live_acceptance=False, game_executed=False, mcp_dispatch_tested=False,
                  coverage='Actual transfer providers/dispatch with explicit paint/widget/game/controller seams; no final pixels or socket controller',
                  seams=['Widget rendered labels/masks and list rows', 'paint coverage and overlay predicates', 'game ownership', 'ordinary callback counters and quantity deltas'],
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
