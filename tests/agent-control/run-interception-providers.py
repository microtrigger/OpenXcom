#!/usr/bin/env python3
"""Extracted real interception providers and callbacks with explicit domain seams, not live acceptance."""
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
    paths = [source/('src/Geoscape/'+name+'State.cpp') for name in ('Intercept','ConfirmDestination')]
    ui = source/'src/Observer/AgentUi.cpp'
    fixture = source/'tests/agent-control/InterceptionProvidersTest.cpp'
    header=(source/'src/Geoscape/InterceptState.h').read_text()
    native=paths[0].read_text()
    if 'const bool _agentTargetless;' not in header or '_agentTargetless(target == nullptr)' not in native:
        raise RuntimeError('constructor mode invariant changed')
    bodies=[]
    for path in paths:
        cls=path.stem
        bodies += [function(path.read_text(), sig+cls+'::'+name) for sig,name in [('bool ','agentUiState('),('bool ','agentDispatch('),('bool ','agentDestinationFull('),('std::vector<std::uint64_t> ','agentPaintRevisions('),('void ','blit(')]]
    bodies += [function(paths[0].read_text(), 'void InterceptState::'+name+'(Action *)') for name in ('btnCancelClick','btnGotoBaseClick','lstCraftsLeftClick')]
    bodies += [function((source/'src/Geoscape/UfoDetectedState.cpp').read_text(), 'void UfoDetectedState::btnInterceptClick(Action *)')]
    bodies += [function(paths[1].read_text(), 'void ConfirmDestinationState::'+name+'(Action *)') for name in ('btnOkClick','btnCancelClick')]
    (out/'TransferProviders.generated.inc').write_text('\n'.join(bodies)+'\n')
    (out/'Ui.generated.inc').write_text('\n'.join(function(ui.read_text(), sig) for sig in ('std::string AgentUiState::payload()', 'std::string AgentUiState::authorityPayload()', 'bool AgentUiState::sameBinding(', 'bool AgentUiState::offers(', 'bool agentRegionsDisjoint(', 'bool agentProjectInterceptList('))+'\n')
    binary = out/'interception-providers'
    cmd = ['c++','-std=c++11','-Wall','-Wextra','-pedantic','-I'+str(out),'-I'+str(source/'src'),str(fixture),'-o',str(binary)]
    cmd += ['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
    files = paths+[source/'src/Geoscape/InterceptState.h',source/'src/Geoscape/UfoDetectedState.cpp',ui,fixture,source/'src/Observer/AgentUi.h',pathlib.Path(__file__).resolve()]
    result = dict(passed=False, live_acceptance=False, game_executed=False, mcp_dispatch_tested=False,
                  coverage='Actual interception providers, four-column projector, dispatch and ordinary handlers with explicit paint/widget/game seams; no final pixels or socket controller',
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
