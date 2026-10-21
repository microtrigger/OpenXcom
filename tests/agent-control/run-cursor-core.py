#!/usr/bin/env python3
"""Compile current-source cursor methods and actual InteractiveSurface/Action fixture; no game execution or live acceptance."""
from pathlib import Path
import argparse, hashlib, json, os, re, shlex, subprocess, time

UNITS=['Observer/AgentUi','Observer/AgentProtocol','Observer/AgentProjectileWitness','Engine/Font','Engine/Surface','Engine/InteractiveSurface','Engine/Language','Engine/LanguagePlurality','Engine/LocalizedText','Engine/Unicode','Engine/Palette','Engine/Action','Engine/FileMap','Engine/Options','Engine/CrossPlatform','Engine/Timer','Engine/Sound','Interface/Text','Interface/NumberText','Interface/TextButton','Interface/ToggleTextButton','Interface/TextList','Interface/ComboBox','Interface/Window','Interface/ArrowButton','Interface/ImageButton','Interface/ScrollBar','lodepng','Engine/RNG','Interface/TextEdit']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def block(text,signature):
 start=text.index(signature);i=text.index('{',start)+1;depth=1
 while depth:
  if i>=len(text):raise RuntimeError('unclosed '+signature)
  if text.startswith('//',i):i=text.index('\n',i)+1;continue
  if text.startswith('/*',i):i=text.index('*/',i)+2;continue
  if text[i] in ('"',"'"):
   quote=text[i];i+=1
   while text[i]!=quote:i+=2 if text[i]=='\\' else 1
   i+=1;continue
  if text[i]=='{':depth+=1
  elif text[i]=='}':depth-=1
  i+=1
 return text[start:i]
GENERATED={'CoreGame.generated.inc':('src/Engine/Game.cpp',['bool Game::agentCursorWindowPoint(', 'void Game::agentCursorState(', 'bool Game::agentMoveCursor(']),'CoreState.generated.inc':('src/Engine/State.cpp',['void State::setModal(','void State::think(','bool State::isScreen(','void State::toggleScreen(','void State::hideAll(','void State::showAll(','bool State::agentCursorMotionSafe('])}
def generate_current(directory):
 for name,(relative,sigs) in GENERATED.items():(directory/name).write_text('\n\n'.join(block((SOURCE/relative).read_text(),sig) for sig in sigs)+'\n')
def closure(files):
 files=set(files);pending=list(files)
 while pending:
  rel=pending.pop();p=SOURCE/rel
  if not p.is_file():raise RuntimeError('missing source '+rel)
  for inc in re.findall(r'^\s*#include\s+"([^"\n]+)"',p.read_text(errors='replace'),re.M):
   q=(p.parent/inc).resolve()
   if q.is_file() and SOURCE in q.parents:
    n=str(q.relative_to(SOURCE))
    if n not in files:files.add(n);pending.append(n)
 return {r:sha(SOURCE/r) for r in sorted(files)}

def main():
    global SOURCE
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2])
    ap.add_argument('--sdk', type=Path, help='Optional SDK prefix containing include/SDL and lib/')
    ap.add_argument('--data', type=Path, required=True, help='Existing runtime data containing common, UFO and TFTD')
    ap.add_argument('--output', type=Path, required=True, help='New directory, outside the source checkout')
    ap.add_argument('--sanitize', action='store_true')
    a = ap.parse_args()
    SOURCE = a.source.resolve()
    data, out = a.data.resolve(), a.output.resolve()
    if out == SOURCE or SOURCE in out.parents:
        ap.error('--output must be outside the source checkout')
    if not all((data / d).is_dir() for d in ('common', 'UFO', 'TFTD')):
        ap.error('--data must contain existing common, UFO and TFTD directories')
    if not (data / 'common/Language/Font.dat').is_file():
        ap.error('missing common/Language/Font.dat')
    out.mkdir(parents=True, exist_ok=False)
    fixture = 'tests/agent-control/CursorCoreTest.cpp'
    files = {fixture, 'src/Engine/Game.h','src/Engine/State.h','src/Engine/InteractiveSurface.h','src/Engine/Action.h'}
    files |= {r for r, _ in GENERATED.values()}
    files |= {f'src/{u}.cpp' for u in UNITS}
    files |= {f'bin/standard/{g}/{f}' for g in ('xcom1','xcom2') for f in ('Language/en-US.yml','interfaces.rul')}
    result = {'all_passed': False, 'game_executed': False, 'live_acceptance': False,
              'full_build_performed': False, 'source_sha256': closure(files),
              'runner_sha256': sha(__file__), 'steps': [],
               'limitations': ['Actual native SDL Action/InteractiveSurface; extracted current Game cursor methods.', 'Screen/window, top-state and rendered cursor are explicit seams; SDL app/mouse state is deterministic link-wrap input.', 'No live GUI, desktop movement or complete Game event-loop qualification.']}
    assets = {str(p): sha(p) for d in ('common/Language','UFO','TFTD')
              for p in sorted((data/d).rglob('*')) if p.is_file()}
    result['runtime_asset_sha256'] = assets
    def save():
        (out/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    save()
    generate_current(out)
    test = out/'Test.cpp'
    test.write_text('#include "'+str(SOURCE/fixture)+'"\n')
    flags = ['c++','-std=c++11', '-ffunction-sections','-fdata-sections',
             '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-D__NO_OPENGL',
             '-DOPENXCOM_LOADING_PRESENTATION_HELPERS', '-I'+str(out),
             '-I'+str(SOURCE/'tests/agent-control')]
    flags += (['-O1','-g','-fsanitize=address,undefined','-fno-sanitize=vptr',
               '-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if a.sanitize else ['-O2'])
    libs = []
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0',
               OPENXCOM_AGENT_TEST_PACKET_DIR=str(out))
    if a.sdk:
        sdk = a.sdk.resolve()
        flags += ['-I'+str(sdk/'include/SDL'), '-I'+str(sdk/'include')]
        lib = sdk/'lib/x86_64-linux-gnu'
        if not lib.is_dir():
            lib = sdk/'lib'
        libs += ['-L'+str(lib), '-Wl,-rpath,'+str(lib)]
        env['LD_LIBRARY_PATH'] = str(lib) + (':'+env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
    else:
        flags += shlex.split(subprocess.check_output(['sdl-config','--cflags'], text=True))
        libs += shlex.split(subprocess.check_output(['sdl-config','--libs'], text=True))
    def run(name, command, cwd=SOURCE):
        start = time.monotonic()
        process = subprocess.run(list(map(str, command)), cwd=cwd, env=env,
                                 capture_output=True, text=True, timeout=900)
        (out/(name+'.log')).write_text(process.stdout+process.stderr)
        result['steps'].append({'name': name, 'command': shlex.join(map(str,command)),
                                'exit_code': process.returncode, 'seconds': time.monotonic()-start})
        save()
        print(name, process.returncode, process.stdout[-2000:], process.stderr[-2000:], flush=True)
        if process.returncode:
            raise RuntimeError(name+' failed; see '+str(out/(name+'.log')))
        if any(s in process.stderr for s in ('runtime error:', 'ERROR: AddressSanitizer', 'UndefinedBehaviorSanitizer')):
            raise RuntimeError(name+' sanitizer diagnostics')
    run('syntax', flags+['-fsyntax-only',test])
    run('compile', flags+[test]+
        [SOURCE/f'src/{u}.cpp' for u in UNITS]+
        ['-Wl,--gc-sections','-Wl,--wrap=SDL_GetMouseState','-Wl,--wrap=SDL_GetAppState','-Wl,--wrap=SDL_WarpMouse']+libs+
        ['-lSDL','-lSDL_image','-lSDL_gfx','-lSDL_mixer','-lyaml-cpp','-o',out/'test'])
    run('test', [out/'test',SOURCE], data/'common')
    if not all(sha(SOURCE/p)==h for p,h in result['source_sha256'].items()):
        raise RuntimeError('source changed during test')
    if not all(sha(p)==h for p,h in assets.items()):
        raise RuntimeError('runtime assets changed during test')
    if sha(__file__) != result['runner_sha256']:
        raise RuntimeError('runner changed during test')
    result['artifact_sha256'] = sha(out/'test')
    result['all_passed'] = True
    save()

if __name__ == '__main__':
    main()
