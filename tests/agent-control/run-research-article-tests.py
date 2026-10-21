#!/usr/bin/env python3
"""Offscreen stock article/report stack fixture; explicit seams, not live acceptance."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, os, re, shlex, subprocess, time
ARTICLE_CLASSES=['ArticleState','ArticleStateCraft','ArticleStateCraftWeapon','ArticleStateVehicle','ArticleStateItem','ArticleStateArmor','ArticleStateBaseFacility','ArticleStateTextImage','ArticleStateText','ArticleStateUfo','ArticleStateTFTD','ArticleStateTFTDCraft','ArticleStateTFTDCraftWeapon','ArticleStateTFTDVehicle','ArticleStateTFTDItem','ArticleStateTFTDArmor','ArticleStateTFTDFacility','ArticleStateTFTDUso']
RULES=['ArticleDefinition','RuleCraft','RuleCraftWeapon','RuleUfo','RuleItem','RuleBaseFacility','RuleInterface','Unit','Armor','RuleInventory','RuleTerrain','MapBlock','RuleResearch']
def main():
    global SOURCE, UNITS, GENERATED, closure, generate_current, sha, block
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2])
    ap.add_argument('--sdk', type=Path, help='Optional SDK prefix containing include/SDL and lib/')
    ap.add_argument('--data', type=Path, required=True, help='Existing runtime data containing common, UFO and TFTD')
    ap.add_argument('--output', type=Path, required=True, help='New directory, outside the source checkout')
    ap.add_argument('--sanitize', action='store_true')
    a = ap.parse_args()
    SOURCE = a.source.resolve()
    spec=importlib.util.spec_from_file_location('article_support',SOURCE/'tests/agent-control/run-article-text-tests.py')
    helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper);helper.SOURCE=SOURCE
    sha,block,closure,generate_current,GENERATED=helper.sha,helper.block,helper.closure,helper.generate_current,helper.GENERATED
    UNITS=helper.UNITS+['Ufopaedia/'+c for c in ARTICLE_CLASSES]+['Mod/'+c for c in RULES]+['Engine/SurfaceSet','Engine/Zoom','Savegame/GameTime','Observer/AgentAudioEvents']
    data, out = a.data.resolve(), a.output.resolve()
    if out == SOURCE or SOURCE in out.parents:
        ap.error('--output must be outside the source checkout')
    if not all((data / d).is_dir() for d in ('common', 'UFO', 'TFTD')):
        ap.error('--data must contain existing common, UFO and TFTD directories')
    if not (data / 'common/Language/Font.dat').is_file():
        ap.error('missing common/Language/Font.dat')
    out.mkdir(parents=True, exist_ok=False)
    fixture = 'tests/agent-control/AgentResearchArticleTestV22.cpp'
    candidate=Path(__file__).with_name('AgentResearchArticleTestV22.cpp')
    helper_path=SOURCE/'tests/agent-control/run-article-text-tests.py'
    files = {fixture, 'tests/agent-control/AgentOutfittingProviderTestV21.cpp',
             'tests/agent-control/AgentControllerTestDependencies.h',
             'src/Observer/AgentController.cpp', 'src/Observer/AgentController.h',
             'src/Observer/AgentFrameCommand.h', 'src/Basescape/BasescapeState.cpp',
             'src/Ufopaedia/Ufopaedia.cpp','src/Geoscape/ResearchCompleteState.cpp','src/Geoscape/GeoscapeState.cpp'}
    files |= {r for r, _ in GENERATED.values()}
    files |= {f'src/{u}.cpp' for u in UNITS}
    files |= {str(p.relative_to(SOURCE)) for g in ('xcom1','xcom2') for p in (SOURCE/f'bin/standard/{g}').glob('*.rul')}
    files |= {f'bin/standard/{g}/{f}' for g in ('xcom1','xcom2') for f in ('Language/en-US.yml','interfaces.rul')}
    result = {'all_passed': False, 'game_executed': False, 'live_acceptance': False,
              'full_build_performed': False, 'source_sha256': closure(files),
              'runner_sha256': sha(__file__), 'steps': [],
              'fixture_sha256':sha(candidate),'helper_sha256':sha(helper_path),
              'limitations': ['Actual article constructors, report resolution/callback and navigation bodies with explicit owning/resource/availability seams.',
                              'Popup owning shell is not a real popup constructor; suppression uses exact daily-completion block with researched-set seam.',
                              'No live Game loop, full daily completion, research eligibility, RNG or campaign progression claim.']}
    assets = {str(p): sha(p) for d in ('common/Language','UFO','TFTD')
              for p in sorted((data/d).rglob('*')) if p.is_file()}
    result['runtime_asset_sha256'] = assets
    def save():
        (out/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    save()
    generate_current(out)
    test = out/'Test.cpp'
    base=(SOURCE/'tests/agent-control/AgentOutfittingProviderTestV21.cpp').read_text();base=base.replace('void State::blit(){}',block((SOURCE/'src/Engine/State.cpp').read_text(),'void State::blit('));base=base.replace('State::~State(){}','State::~State(){for(auto*s:_surfaces)delete s;}');base=base.replace('../../src/',str(SOURCE/'src')+'/');(out/'ArticleHarnessBase.generated.inc').write_text(base)
    ordinary=(SOURCE/'src/Engine/State.cpp').read_text();(out/'ArticleStateOrdinary.generated.inc').write_text('\n'.join(block(ordinary,x) for x in ['void State::add(Surface *surface)','LocalizedText State::tr(const std::string &id) const','LocalizedText State::tr(const std::string &id, unsigned n) const','void State::centerAllSurfaces(']))
    nav=(SOURCE/'src/Ufopaedia/Ufopaedia.cpp').read_text();(out/'ArticleNavigation.generated.inc').write_text('size_t Ufopaedia::_current_index=0;\n'+'\n'.join(block(nav,x) for x in ['size_t Ufopaedia::getArticleIndex(', 'void Ufopaedia::openArticle(Game *game, const std::string &article_id)', 'ArticleState *Ufopaedia::createArticleState(','void Ufopaedia::next(','void Ufopaedia::prev(']))
    fixture=candidate.read_text().replace('../../src/',str(SOURCE/'src')+'/');(out/'Test.cpp').write_text(fixture)
    callback=block((SOURCE/'src/Geoscape/ResearchCompleteState.cpp').read_text(),'void ResearchCompleteState::btnReportClick(').replace('ResearchCompleteState::','ReportPopupHarness::');(out/'ReportCallback.generated.inc').write_text(callback)
    geo=(SOURCE/'src/Geoscape/GeoscapeState.cpp').read_text();start=geo.index('const RuleResearch * newResearch = research;');end=geo.index('// 3e. handle core research',start)
    (out/'ReportSuppression.generated.inc').write_text('const RuleResearch *selectReportResearch(const RuleResearch *research,SuppressionGame *_game) {\n'+geo[start:end]+'return newResearch;\n}')
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
    run('test', [out/'test',SOURCE], data/'common')
    if not all(sha(SOURCE/p)==h for p,h in result['source_sha256'].items()):
        raise RuntimeError('source changed during test')
    if not all(sha(p)==h for p,h in assets.items()):
        raise RuntimeError('runtime assets changed during test')
    if sha(__file__) != result['runner_sha256']:
        raise RuntimeError('runner changed during test')
    if sha(candidate)!=result['fixture_sha256'] or sha(helper_path)!=result['helper_sha256']:raise RuntimeError('fixture/helper changed during test')
    packets=sorted(out.glob('*-wire-*.json'));schedules=sorted(out.glob('*-schedule.json'))
    result['packet_sha256']={p.name:sha(p) for p in packets+schedules}
    result['schedule_count']=len(schedules);result['packet_count']=len(packets)
    result['artifact_sha256'] = sha(out/'test')
    result['all_passed'] = True
    save()

if __name__ == '__main__':
    main()
