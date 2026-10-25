#!/usr/bin/env python3
"""Literal Geoscape interception-entry provider/handler with synthetic widgets; no live/render proof."""
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
    paths = [source/'src/Geoscape/GeoscapeState.cpp']
    ui = source/'src/Observer/AgentUi.cpp'
    fixture = source/'tests/agent-control/GeoscapeInterceptionEntryTest.cpp'
    text=paths[0].read_text()
    # JSON literals contain unmatched braces; exact neighboring function signatures
    # bound these two bodies without counting characters inside strings.
    signatures=('bool GeoscapeState::agentUiState(', 'bool GeoscapeState::agentDispatch(const std::string &action)', 'bool GeoscapeState::agentDispatch(const Observer::AgentRequest &request)')
    if any(text.count(signature)!=1 for signature in signatures):
        raise RuntimeError('Geoscape function signature not unique')
    positions=[text.index(signature) for signature in signatures]
    bodies=[text[positions[i]:positions[i+1]].rstrip() for i in range(2)]
    if not all(body.count('bool GeoscapeState::')==1 and body.endswith('}') and '#endif' not in body for body in bodies):
        raise RuntimeError('Geoscape extraction boundary changed')
    bodies.append(function(text,'void GeoscapeState::btnInterceptClick(Action *)'))
    (out/'GeoEntry.generated.inc').write_text('\n'.join(bodies)+'\n')
    (out/'Ui.generated.inc').write_text('\n'.join(function(ui.read_text(), sig) for sig in ('std::string AgentUiState::payload()', 'std::string AgentUiState::authorityPayload()', 'bool AgentUiState::sameBinding(', 'bool AgentUiState::offers(', 'bool agentRegionsDisjoint(', 'bool agentProjectInterceptList('))+'\n')
    binary = out/'interception-providers'
    cmd = ['c++','-std=c++11','-Wall','-Wextra','-pedantic','-I'+str(out),'-I'+str(source/'src'),str(fixture),'-o',str(binary)]
    cmd += ['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
    files = paths+[ui,fixture,source/'src/Observer/AgentUi.h',pathlib.Path(__file__).resolve()]
    result = dict(passed=False, live_acceptance=False, game_executed=False, mcp_dispatch_tested=False,
                  coverage='Literal Geoscape provider/string-dispatch/INTERCEPT handler and UI binding; synthetic widgets, navigation allocation and disabled flag; no actual renderer/socket/controller/constructor',
                  seams=['Widget rendered labels/masks and list rows', 'paint coverage and overlay predicates', 'game ownership', 'ordinary navigation allocation and zoom-disabled predicate'],
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
