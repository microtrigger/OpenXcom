#!/usr/bin/env python3
"""Compile current-source mission/research popup fixtures; no live acceptance."""
from pathlib import Path
import argparse, hashlib, json, os, re, shlex, subprocess, time

UNITS=['Observer/AgentSoundPlayback','Observer/AgentAudioEvents','Observer/AgentUi','Observer/AgentProtocol','Observer/AgentProjectileWitness','Engine/Font','Engine/Surface','Engine/InteractiveSurface','Engine/Language','Engine/LanguagePlurality','Engine/LocalizedText','Engine/Unicode','Engine/Palette','Engine/Action','Engine/FileMap','Engine/Options','Engine/CrossPlatform','Engine/Timer','Engine/Sound','Interface/Text','Interface/NumberText','Interface/TextButton','Interface/ToggleTextButton','Interface/TextList','Interface/ComboBox','Interface/Window','Interface/ArrowButton','Interface/ImageButton','Interface/ScrollBar','lodepng','Engine/RNG','Interface/TextEdit']
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
CLASSES={'SoldiersState':['btnOkClick','lstSoldiersClick'],'SoldierInfoState':['btnOkClick','btnPrevClick','btnNextClick','btnArmorClick'],'CraftsState':['btnOkClick','lstCraftsClick'],'CraftInfoState':['btnOkClick','btnCrewClick','btnEquipClick','btnArmorClick'],'CraftSoldiersState':['btnOkClick'],'CraftEquipmentState':['btnOkClick'],'CraftArmorState':['btnOkClick','openSoldierArmor'],'SoldierArmorState':['btnCancelClick','lstArmorClick']}
GENERATED={c+'Methods.generated.inc':('src/Basescape/'+c+'.cpp',['void '+c+'::blit(','bool '+c+'::agentUiState(','bool '+c+'::agentDispatch(']+['void '+c+'::'+n+'(' for n in callbacks]) for c,callbacks in CLASSES.items()}
GENERATED.update({'CompletionOrdinaryBlit.generated.inc':('src/Engine/State.cpp',['void State::blit(']),'CompletionOverlay.generated.inc':('src/Engine/Game.cpp',['bool Game::agentOverlayClear(','bool Game::agentNonCursorOverlayClear(']),'CompletionStateBase.generated.inc':('src/Engine/State.cpp',['bool State::agentCursorMotionSafe(','void State::setModal(','void State::think(','bool State::isScreen(','void State::toggleScreen(','void State::hideAll(','void State::showAll(']),'CompletionGameIdentity.generated.inc':('src/Engine/Game.cpp',['bool Game::isState('])})
POPUPS={'ResearchCompleteState':['btnOkClick','btnReportClick'], 'NewPossibleResearchState':['btnOkClick','btnResearchClick'], 'NewPossibleManufactureState':['btnOkClick','btnManufactureClick'], 'ResearchRequiredState':['btnOkClick'], 'MissionDetectedState':['btnInterceptClick','btnCenterClick','btnCancelClick']}
GENERATED.update({c+'Methods.generated.inc':('src/Geoscape/'+c+'.cpp', ['bool '+c+'::agentDestinationFull(', 'std::vector<std::uint64_t> '+c+'::agentPaintRevisions(', 'void '+c+'::blit(', 'bool '+c+'::agentUiState(', 'bool '+c+'::agentDispatch(']+['void '+c+'::'+n+'(' for n in callbacks]) for c,callbacks in POPUPS.items()})
def generate_current(directory):
 for name,(relative,sigs) in GENERATED.items():(directory/name).write_text('\n\n'.join(block((SOURCE/relative).read_text(),sig) for sig in sigs)+'\n')
 names={'Game':'AgentTableControllerGame','State':'AgentTableControllerState','Screen':'AgentTableControllerScreen','AgentAudioEvents':'AgentTableControllerAudioEvents','AgentController':'AgentTableTestController','AgentFrameCommand':'AgentTableFrameCommand'}
 def rename(text):return re.sub(r'\b('+'|'.join(names)+r')\b',lambda m:names[m.group()],text)
 def includes(text,relative):return re.sub(r'#include "([^"\n]+)"',lambda m:'#include "'+str((SOURCE/relative/m[1]).resolve())+'"',text)
 deps=includes((SOURCE/'tests/agent-control/AgentControllerTestDependencies.h').read_text(),'tests/agent-control');deps=deps.replace('#include <functional>','#include <functional>\n#include "'+str(SOURCE/'src/Engine/Surface.h')+'"');deps=deps.replace('bool agentUiState(Observer::AgentUiState &out)const{if(dirty)return false;out=ui;return true;}','std::function<bool(Observer::AgentUiState&)> project;\n bool agentUiState(Observer::AgentUiState &out)const{if(dirty)return false;if(project)return project(out);out=ui;return true;}')
 overlay='\n'.join(block((SOURCE/'src/Engine/Game.cpp').read_text(),signature) for signature in ('bool Game::agentOverlayClear(','bool Game::agentNonCursorOverlayClear('));deps=deps.replace('bool agentOverlayClear(const Observer::AgentUiState &)const{return true;}','Surface *_cursor=nullptr,*_fpsCounter=nullptr;\n '+overlay.replace('bool Game::agentOverlayClear','bool agentOverlayClear').replace('bool Game::agentNonCursorOverlayClear','bool agentNonCursorOverlayClear'))
 # Keep the inert test queue, but extract current PCM event layout/limits.
 audio=(SOURCE/'src/Observer/AgentAudioEvents.h').read_text()
 limits=audio[audio.index('    enum { FramesPerWindow'):audio.index('    struct Event')]
 event=block(audio, '    struct Event')+';'
 deps=deps.replace(' struct Event { enum Kind { Window, Gap, Status } kind=Window; enum State { Pending,Active,Unavailable } state=Pending;\n bool nonzero=false,partialWindowDiscarded=false;std::uint64_t droppedWindows=0; };', limits+event)
 (directory/'AgentTableControllerDependencies.generated.h').write_text(rename(deps));(directory/'AgentTableFrameCommand.generated.h').write_text(rename(includes((SOURCE/'src/Observer/AgentFrameCommand.h').read_text(),'src/Observer')))
 header=(SOURCE/'src/Observer/AgentController.h').read_text().replace('#include "AgentFrameCommand.h"','#include "'+str(directory/'AgentTableFrameCommand.generated.h')+'"');(directory/'AgentTableController.generated.h').write_text(rename(includes(header,'src/Observer')))
 controller=(SOURCE/'src/Observer/AgentController.cpp').read_text().replace('#include "AgentController.h"','#include "'+str(directory/'AgentTableController.generated.h')+'"')
 for dep in ['AgentAudioEvents.h','../Engine/Game.h','../Engine/Screen.h','../Engine/State.h']:controller=controller.replace('#include "'+dep+'"','#include "'+str(directory/'AgentTableControllerDependencies.generated.h')+'"')
 controller=controller.replace('return std::chrono::duration_cast<std::chrono::milliseconds>(\n        std::chrono::steady_clock::now().time_since_epoch()).count();','return agentTestTime;');(directory/'ActualCompletionController.cpp').write_text(rename(includes(controller,'src/Observer')))
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
    ap.add_argument('--master', choices=('xcom1','xcom2','both'), default='both')
    a = ap.parse_args()
    SOURCE = a.source.resolve()
    data, out = a.data.resolve(), a.output.resolve()
    if out == SOURCE or SOURCE in out.parents:
        ap.error('--output must be outside the source checkout')
    resource_dirs = ('UFO', 'TFTD') if a.master == 'both' else (('UFO',) if a.master == 'xcom1' else ('TFTD',))
    if not all((data / d).is_dir() for d in ('common',) + resource_dirs):
        ap.error('--data must contain existing common, UFO and TFTD directories')
    if not (data / 'common/Language/Font.dat').is_file():
        ap.error('missing common/Language/Font.dat')
    out.mkdir(parents=True, exist_ok=False)
    fixture = 'tests/agent-control/AgentMissionAlertTest.cpp'
    files = {fixture, 'tests/agent-control/AgentOutfittingProviderTestV21.cpp',
             'tests/agent-control/AgentControllerTestDependencies.h',
             'src/Observer/AgentController.cpp', 'src/Observer/AgentController.h',
             'src/Observer/AgentFrameCommand.h', 'src/Basescape/BasescapeState.cpp'}
    files |= {r for r, _ in GENERATED.values()}
    files |= {f'src/{u}.cpp' for u in UNITS}
    files |= {f'bin/standard/{g}/{f}' for g in ('xcom1','xcom2') for f in ('Language/en-US.yml','interfaces.rul')}
    result = {'all_passed': False, 'game_executed': False, 'live_acceptance': False,
              'full_build_performed': False, 'master': a.master, 'source_sha256': closure(files),
              'runner_sha256': sha(__file__), 'steps': [],
              'limitations': ['Offscreen stock-font/palette fixture, not a game loop or live save.',
                              'Retained owning state/controller scaffolding; no article construction or packets.',
                              'Controller dependency/time seams; other destination callbacks are inert counters.',
                              'Mission/Globe/Geoscape owning seams; actual current provider, dispatch, native callbacks and controller are extracted.']}
    assets = {str(p): sha(p) for d in ('common/Language',) + resource_dirs
              for p in sorted((data/d).rglob('*')) if p.is_file()}
    result['runtime_asset_sha256'] = assets
    def save():
        (out/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    save()
    generate_current(out)
    # The old common fixture used a cursor as an occluding overlay. Current
    # player parity excludes ordinary cursors; use the real FPS overlay seam.
    base=(SOURCE/'tests/agent-control/AgentOutfittingProviderTestV21.cpp').read_text()
    start=base.index('template<class T>void common(')
    end=base.index('template<class T>void paintedFailure(', start)
    base=base[:start]+base[start:end].replace('->_cursor', '->_fpsCounter')+base[end:]
    base=base.replace('../../src/', str(SOURCE/'src')+'/')
    (out/'MissionHarnessBase.generated.inc').write_text(base)
    notice=(SOURCE/'src/Geoscape/NewPossibleResearchState.cpp').read_text()
    start=notice.index('    const std::string payload = out.payload();')
    end=notice.index('    return !out.regions.empty()',start)
    (out/'NoticeBound.generated.inc').write_text(notice[start:end])
    test = out/'Test.cpp'
    test.write_text('#include <bits/stdc++.h>\n#define private public\n#include "'+str(SOURCE/'src/Interface/Text.h')+'"\n#undef private\nstatic int agentTextOffset(const OpenXcom::Text&t){return t._scrollY;}\n#include "'+str(SOURCE/fixture)+'"\n')
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
    run('compile', flags+[test,out/'ActualCompletionController.cpp']+
        [SOURCE/f'src/{u}.cpp' for u in UNITS]+
        ['-Wl,--gc-sections']+libs+
        ['-lSDL','-lSDL_image','-lSDL_gfx','-lSDL_mixer','-lyaml-cpp','-o',out/'test'])
    run('test', [out/'test',SOURCE,a.master], data/'common')
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
