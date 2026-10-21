#!/usr/bin/env python3
"""Current-source arithmetic with domain seams; not widget/MCP/live acceptance."""
import argparse, hashlib, json, os, pathlib, subprocess

def function(text, signature):
    start = text.index(signature)
    pos = text.index('{', start)
    depth = 1
    end = pos + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[2])
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    source, out = args.source.resolve(), args.output.resolve()
    if out == source or source in out.parents:
        parser.error('output must be outside checkout')
    out.mkdir(parents=True, exist_ok=False)
    production = source / 'src/Savegame/ProductionCommands.cpp'
    research = source / 'src/Basescape/ResearchInfoState.cpp'
    fixture = source / 'tests/agent-control/AllocationMatrixTest.cpp'
    # Only remove include directives; every production command body is current source.
    (out/'Production.generated.inc').write_text('\n'.join(line for line in production.read_text().splitlines() if not line.startswith('#include'))+'\n')
    (out/'Research.generated.inc').write_text('\n'.join(function(research.read_text(), 'void ResearchInfoState::'+name+'(int change)') for name in ('moreByValue','lessByValue'))+'\n')
    binary = out/'allocation-matrix'
    command = ['c++', '-std=c++11', '-Wall', '-Wextra', '-pedantic', '-I'+str(out), '-I'+str(source/'src/Savegame'), str(fixture), '-o', str(binary)]
    command += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
    result = {'passed':False,'live_acceptance':False,'game_executed':False,'mcp_dispatch_tested':False,'leak_detection':False if args.sanitize else None,'seams':['Base/Production resource getters and setters; no game loop, UI, storage or funds'], 'sha256':{str(p.relative_to(source)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (production,research,fixture,source/'src/Savegame/ProductionCommands.h',source/'src/Savegame/QuantityCommand.h')}}
    try:
        for name, cmd in [('compile',command),('run',[str(binary)])]:
            env = dict(os.environ)
            if args.sanitize:
                env['ASAN_OPTIONS'] = 'detect_leaks=0'  # LeakSanitizer unsupported under executor ptrace.
            run = subprocess.run(cmd, capture_output=True, text=True, timeout=120, env=env)
            (out/(name+'.log')).write_text(run.stdout+run.stderr)
            result[name+'_exit_code'] = run.returncode
            print(name, run.returncode, run.stdout, run.stderr, flush=True)
            if run.returncode:
                raise RuntimeError(name+' failed')
        result['passed'] = True
    finally:
        (out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')

if __name__ == '__main__':
    main()
