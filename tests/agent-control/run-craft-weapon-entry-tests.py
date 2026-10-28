#!/usr/bin/env python3
"""Compile current-source named-button fixtures; no game execution or live acceptance."""
from pathlib import Path
import argparse, hashlib, json, os, re, shlex, subprocess, sys
from native_test_runner import run_recipe

UNITS=['Observer/AgentSoundPlayback','Observer/AgentUi','Observer/AgentProtocol','Observer/AgentProjectileWitness','Engine/Font','Engine/Surface','Engine/InteractiveSurface','Engine/Language','Engine/LanguagePlurality','Engine/LocalizedText','Engine/Unicode','Engine/Palette','Engine/Action','Engine/FileMap','Engine/Options','Engine/CrossPlatform','Engine/Timer','Engine/Sound','Interface/Text','Interface/NumberText','Interface/TextButton','Interface/ToggleTextButton','Interface/TextList','Interface/ComboBox','Interface/Window','Interface/ArrowButton','Interface/ImageButton','Interface/ScrollBar','lodepng','Engine/RNG','Interface/TextEdit']
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
def generate_current(directory):
 for name,(relative,sigs) in GENERATED.items():(directory/name).write_text('\n\n'.join(block((SOURCE/relative).read_text(),sig) for sig in sigs)+'\n')
 names={'Game':'AgentTableControllerGame','State':'AgentTableControllerState','Screen':'AgentTableControllerScreen','AgentAudioEvents':'AgentTableControllerAudioEvents','AgentController':'AgentTableTestController','AgentFrameCommand':'AgentTableFrameCommand'}
 def rename(text):return re.sub(r'\b('+'|'.join(names)+r')\b',lambda m:names[m.group()],text)
 def includes(text,relative):return re.sub(r'#include "([^"\n]+)"',lambda m:'#include "'+str((SOURCE/relative/m[1]).resolve())+'"',text)
 deps=includes((SOURCE/'tests/agent-control/AgentControllerTestDependencies.h').read_text(),'tests/agent-control');deps=deps.replace('#include <functional>','#include <functional>\n#include "'+str(SOURCE/'src/Engine/Surface.h')+'"');deps=deps.replace('bool agentUiState(Observer::AgentUiState &out)const{if(dirty)return false;out=ui;return true;}','std::function<bool(Observer::AgentUiState&)> project;\n bool agentUiState(Observer::AgentUiState &out)const{if(dirty)return false;if(project)return project(out);out=ui;return true;}')
 overlay='\n'.join(block((SOURCE/'src/Engine/Game.cpp').read_text(),signature) for signature in ('bool Game::agentOverlayClear(','bool Game::agentNonCursorOverlayClear('));deps=deps.replace('bool agentOverlayClear(const Observer::AgentUiState &)const{return true;}','Surface *_cursor=nullptr,*_fpsCounter=nullptr;\n '+overlay.replace('bool Game::agentOverlayClear','bool agentOverlayClear').replace('bool Game::agentNonCursorOverlayClear','bool agentNonCursorOverlayClear'))
 (directory/'AgentTableControllerDependencies.generated.h').write_text(rename(deps));(directory/'AgentTableFrameCommand.generated.h').write_text(rename(includes((SOURCE/'src/Observer/AgentFrameCommand.h').read_text(),'src/Observer')))
 header=(SOURCE/'src/Observer/AgentController.h').read_text().replace('#include "AgentFrameCommand.h"','#include "'+str(directory/'AgentTableFrameCommand.generated.h')+'"');(directory/'AgentTableController.generated.h').write_text(rename(includes(header,'src/Observer')))
 controller=(SOURCE/'src/Observer/AgentController.cpp').read_text().replace('#include "AgentController.h"','#include "'+str(directory/'AgentTableController.generated.h')+'"')
 for dep in ['AgentAudioEvents.h','../Engine/Game.h','../Engine/Screen.h','../Engine/State.h']:controller=controller.replace('#include "'+dep+'"','#include "'+str(directory/'AgentTableControllerDependencies.generated.h')+'"')
 # Unexercised audio and own-base-read subsystem seams; current UI controller stays extracted.
 for signature in ('void AgentController::drainAudioEvents(', 'void AgentController::resetPlayerBases(', 'void AgentController::playerBasesPage('):
  original=block(controller,signature); controller=controller.replace(original,original[:original.index('{')]+'{}')
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

def extract(source, out):
    global SOURCE
    SOURCE = source
    fixture = "tests/agent-control/AgentOutfittingProviderTestV21.cpp"
    generate_current(out)
    current = (SOURCE/'src/Basescape/BasescapeState.cpp').read_text()
    (out/'BasescapeNamed.generated.inc').write_text('\n'.join(block(current, s) for s in
        ('bool BasescapeState::agentUiState(', 'bool BasescapeState::agentDispatch(',
         'void BasescapeState::btnGeoscapeClick(')))
    test = out/'Test.cpp'
    test.write_text('#define main legacy_outfitting_main\n#include "'+str(SOURCE/fixture)+'"\n#undef main\n'+r'''int main(int argc,char**argv){check(argc==2,"runtime root");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen only");Options::baseXResolution=320;Options::baseYResolution=200;Options::keyOk=SDLK_RETURN;Options::keyCancel=SDLK_ESCAPE;Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}for(const std::string master:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");craftWeaponEntries(big,small,lang,argv[1],master);}check(agentPaintHealthy()&&!SDL_WasInit(SDL_INIT_VIDEO),"no game or GUI");std::cout<<"CraftInfo weapon entry PASS "<<checks<<" checks\n";}''')


def integrate(source, sdk, data, out, sanitize=False):
    global SOURCE
    SOURCE, sdk, data, out = [Path(p).resolve() for p in (source, sdk, data, out)]
    fixture = 'tests/agent-control/AgentOutfittingProviderTestV21.cpp'
    files = {fixture, 'tests/agent-control/AgentControllerTestDependencies.h',
             'src/Observer/AgentController.cpp', 'src/Observer/AgentController.h',
             'src/Observer/AgentFrameCommand.h', 'src/Basescape/BasescapeState.cpp'}
    files |= {r for r, _ in GENERATED.values()}
    files |= {f'src/{u}.cpp' for u in UNITS}
    inputs = [SOURCE / r for r in closure(files)]
    assets = sorted(p.resolve() for d in ('common/Language', 'UFO', 'TFTD', 'standard')
                    for p in (data / d).rglob('*') if p.is_file())
    if not assets or not all((data / d).is_dir() for d in ('common', 'UFO', 'TFTD')):
        raise ValueError('Existing common, UFO and TFTD data are required')
    inputs += assets + [Path(__file__).resolve(), Path(__file__).with_name('native_test_runner.py').resolve()]
    flags = ['c++', '-std=c++11', '-ffunction-sections', '-fdata-sections',
             '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL', '-D__NO_OPENGL',
             '-DOPENXCOM_LOADING_PRESENTATION_HELPERS', '-I' + str(out),
             '-I' + str(SOURCE / 'tests/agent-control'), '-I' + str(sdk / 'include/SDL'), '-I' + str(sdk / 'include')]
    flags += (['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize=vptr',
               '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if sanitize else ['-O2'])
    lib = sdk / 'lib/x86_64-linux-gnu'
    if not lib.is_dir(): lib = sdk / 'lib'
    env = dict(ASAN_OPTIONS='detect_leaks=0', OPENXCOM_AGENT_TEST_PACKET_DIR=str(out),
               LD_LIBRARY_PATH=str(lib) + ':' + os.environ.get('LD_LIBRARY_PATH', ''))
    def step(name, argv, cwd=SOURCE):
        return dict(name=name, argv=[str(a) for a in argv], cwd=str(cwd), env=dict(env))
    steps = [step('extract', [sys.executable, Path(__file__).resolve(), '--source', SOURCE,
                              '--sdk', sdk, '--data', data, '--output', out, '--extract-only']),
             step('syntax', flags + ['-fsyntax-only', out / 'Test.cpp']),
             step('compile', flags + [out / 'Test.cpp', out / 'ActualCompletionController.cpp'] +
                  [SOURCE / f'src/{u}.cpp' for u in UNITS] + ['-Wl,--gc-sections', '-L' + str(lib),
                   '-Wl,-rpath,' + str(lib), '-lSDL', '-lSDL_image', '-lSDL_gfx', '-lSDL_mixer', '-lyaml-cpp', '-o', out / 'test']),
             step('test', [out / 'test', data.parent], data / 'common')]
    receipt = run_recipe(dict(version=1, source=str(SOURCE), output=str(out),
                              inputs=sorted(set(str(p) for p in inputs)), steps=steps))
    qualification = dict(all_passed=receipt['all_passed'], inputs_unchanged=receipt['inputs_unchanged'],
                         game_executed=False, live_acceptance=False, generic_receipt='RESULT.json',
                         limitations=[
        'Only CraftInfo weapon entry matrix is executed, not the full legacy outfitting fixture.',
        'Actual provider and dispatcher/controller are extracted; constructor layout is reproduced.',
        'Destination slot callbacks are counters; modal mutation proof is separate. Audio and owned-base-read methods are inert and unexercised.',
        'Offscreen native stock-font/palette tests are not a real campaign, game loop or live save.',
        'Source/header/data inputs are explicit; compiler, SDK and dynamic-library closure is not exhaustive.',
        'ASan/UBSan excludes vptr and leak checking.' if sanitize else 'No sanitizer in this run.'])
    (out / 'QUALIFICATION.json').write_text(json.dumps(qualification, indent=2) + '\n')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('source', 'sdk', 'data', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--extract-only', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.extract_only:
        extract(args.source.resolve(), args.output.resolve())
        return 0
    return 0 if integrate(args.source, args.sdk, args.data, args.output, args.sanitize)['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
