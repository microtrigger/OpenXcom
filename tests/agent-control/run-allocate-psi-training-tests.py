#!/usr/bin/env python3
"""Actual popup constructor/provider/callback bodies with native SDL widgets and explicit domain seams."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess


LEGACY_PROBE_MAIN = r'''
int main(int argc,char **argv){
 if(argc!=2||SDL_Init(0)!=0)return 2;FileMap::load("common","./",false);
 Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Language language;language.loadFile(std::string(argv[1])+"/bin/standard/xcom1/Language/en-US.yml");
 SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
 Surface canvas(320,200),bg(320,200);canvas.setPalette(palette);bg.setPalette(palette);bg.clear(40);
 for(int mode=0;mode<2;++mode)for(int kind=0;kind<4;++kind){
   TextList list(280,80,10,50);list.setPalette(palette);list.initText(&big,&small,&language);list.setColumns(3,100,90,90);list.setSelectable(true);list.setBackground(&bg);
   if(mode)list.agentEnableOutfitting(3,false,false);
   const std::string label=kind==1?std::string(language.getString("STR_UNKNOWN")):kind==2?std::string(500,'W'):"NORMAL";
   list.addRow(3,label.c_str(),"VISIBLE","YES");list.addRow(3,"SECOND","0/+0","NO");
   SDL_Rect clipped={0,0,100,200};if(kind==3)SDL_SetClipRect(canvas.getSurface(),&clipped);
   list.blit(&canvas);
#ifdef CURRENT_HAS_PSI
   std::vector<AgentOutfittingRow> psi;if(list.agentPsiTrainingRows(psi)||!psi.empty())return 5;
#endif
   std::vector<AgentListRow> generic;std::vector<AgentOutfittingRow> outfitting;
   const bool g=list.agentRows(generic),o=list.agentOutfittingRows(outfitting);
   std::cout<<"{\"mode\":"<<mode<<",\"case\":"<<kind<<",\"generic_ok\":"<<g<<",\"outfitting_ok\":"<<o<<",\"generic\":[";
   bool comma=false;for(const auto &row:generic){if(comma)std::cout<<",";comma=true;std::cout<<"{\"index\":"<<row.logicalIndex<<",\"selected\":"<<row.selected<<",\"cells\":[";for(size_t c=0;c<row.cells.size();++c){if(c)std::cout<<",";std::cout<<agentJsonString(row.cells[c]);}std::cout<<"]}";}
   std::cout<<"],\"outfitting\":[";comma=false;for(const auto &row:outfitting){if(comma)std::cout<<",";comma=true;std::cout<<"{\"index\":"<<row.logicalIndex<<",\"selected\":"<<row.selected<<",\"cells\":[";for(size_t c=0;c<row.cells.size();++c){if(c)std::cout<<",";std::cout<<(row.cells[c].known?agentJsonString(row.cells[c].text):"null");}std::cout<<"]}";}std::cout<<"]}\n";
   SDL_SetClipRect(canvas.getSurface(),nullptr);
 }
}
'''

def legacy_comparison(source, output, flags, support, library, env, data, fixture, sanitize):
    import io
    import shutil
    import tarfile
    baseline = '7da76f971ab69d20c03e71b68ce45810171b3fed'
    prefix = fixture.read_text().split('namespace Fixture {', 1)[0]
    records = []
    for label in ('baseline', 'current'):
        tree = output / ('legacy-' + label)
        tree.mkdir()
        if label == 'baseline':
            archive = subprocess.run(['git', '-C', str(source), 'archive', baseline, 'src'], check=True, capture_output=True).stdout
            # git archive contains trusted repository source paths only.
            with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
                tar.extractall(tree, filter='data')
        else:
            shutil.copytree(source / 'src', tree / 'src')
        path = tree / 'tests/agent-control/Probe.cpp'
        path.parent.mkdir(parents=True)
        path.write_text(prefix + LEGACY_PROBE_MAIN)
        command = flags + (['-DCURRENT_HAS_PSI'] if label == 'current' else []) + [str(path)] + [str(tree / ('src/' + u + '.cpp')) for u in support.UNITS] + ['-Wl,--gc-sections', '-L'+str(library), '-lSDL', '-lSDL_gfx', '-lSDL_image', '-lSDL_mixer', '-lyaml-cpp', '-o', str(tree/'test')]
        build = subprocess.run(command, env=env, capture_output=True, text=True)
        (tree/'build.log').write_text(build.stdout+build.stderr)
        if build.returncode:
            raise RuntimeError('Legacy probe compilation failed: '+label)
        run = subprocess.run([str(tree/'test'), str(source)], cwd=data/'common', env=env, capture_output=True, text=True)
        (tree/'test.log').write_text(run.stdout+run.stderr)
        rows = [json.loads(line) for line in run.stdout.splitlines()] if run.returncode == 0 else []
        # At least the ordinary untruncated case must be real nonempty coverage.
        if run.returncode or len(rows)!=8 or not rows[0]['generic'] or not rows[4]['outfitting']:
            raise RuntimeError('Legacy probe execution/coverage failed: '+label)
        records.append({'label':label, 'exit_code':run.returncode, 'source_sha256':{str(p.relative_to(tree)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((tree/'src').rglob('*')) if p.is_file()}, 'stdout_sha256':hashlib.sha256(run.stdout.encode()).hexdigest(), 'projection':rows})
    result={'all_passed':records[0]['projection']==records[1]['projection'], 'baseline_commit':baseline, 'records':records, 'cases':['generic and outfitting opt-in each: normal nonempty, literal Unknown, long500W, clipped destination'], 'scope':'Psi opt-in disabled; exact logical projection equality for these eight cases only; current additionally refuses psi capture without opt-in.', 'sanitized':sanitize}
    (output/'LEGACY-COMPARISON.json').write_text(json.dumps(result,indent=2)+'\n')
    if not result['all_passed']:
        raise RuntimeError('Legacy projection changed')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    source, sdk, data, output = [p.resolve() for p in (args.source, args.sdk, args.data, args.output)]
    output.mkdir(parents=True, exist_ok=False)
    helper = source / 'tests/agent-control/run-dogfight-providers.py'
    spec = importlib.util.spec_from_file_location('native_support', helper)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    block = support.block
    signatures = [
        'AllocatePsiTrainingState::AllocatePsiTrainingState(', 'AllocatePsiTrainingState::~AllocatePsiTrainingState(',
        'void AllocatePsiTrainingState::btnOkClick(', 'void AllocatePsiTrainingState::lstSoldiersClick(',
        'bool AllocatePsiTrainingState::agentDestinationFull(',
        'std::vector<std::uint64_t> AllocatePsiTrainingState::agentPaintRevisions(',
        'void AllocatePsiTrainingState::blit(', 'bool AllocatePsiTrainingState::agentUiState(',
        'bool AllocatePsiTrainingState::agentDispatch(',
    ]
    body = (source / 'src/Geoscape/AllocatePsiTrainingState.cpp').read_text()
    (output / 'AllocatePsiTraining.generated.inc').write_text('\n'.join(block(body, s) for s in signatures))
    game = (source / 'src/Engine/Game.cpp').read_text()
    (output / 'GameIdentity.generated.inc').write_text(block(game, 'bool Game::isState('))
    state = (source / 'src/Engine/State.cpp').read_text()
    (output / 'StateBase.generated.inc').write_text('\n'.join(block(state, s) for s in [
        'bool State::agentCursorMotionSafe(', 'void State::setModal(', 'void State::think(',
        'bool State::isScreen(', 'void State::toggleScreen(', 'void State::hideAll(', 'void State::showAll(',
    ]))
    fixture = source / 'tests/agent-control/AllocatePsiTrainingProviderTest.cpp'
    files = list((source / 'src').rglob('*.cpp')) + list((source / 'src').rglob('*.h'))
    files += [fixture, helper, Path(__file__).resolve(), data / 'common/Language/Font.dat']
    files += [source / ('bin/standard/' + game + '/Language/en-US.yml') for game in ('xcom1', 'xcom2')]
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    hashes = {str(p): sha(p) for p in files}
    asset_roots = [(data / name).resolve(strict=True) for name in ('common', 'UFO', 'TFTD')]
    asset_hashes = {str(p): sha(p) for root in asset_roots for p in root.rglob('*') if p.is_file()}
    if not asset_hashes:
        raise RuntimeError('No runtime assets sealed')
    result = {'all_passed': False, 'source_sha256': hashes, 'live_acceptance': False,
              'game_executed': False, 'runtime_asset_sha256': asset_hashes, 'limitations': [
                  'Actual source constructor/provider/callbacks and native SDL widgets.',
                  'Game, Mod/interface setup and rendering loop are explicit seams.',
                  'Literal constructor, native assignment toggle and OK recalculation calls; Soldier/Base/SavedGame and stat-string implementation are seams, no actual psychic improvement qualification.',
                  'Synthetic grayscale palette; no real encounter or stock pixel-appearance qualification.',
                  'Sanitizers exclude vptr,enum and leak checking; not full UBSan.' if args.sanitize else 'No sanitizer in this run.']}
    save = lambda: (output / 'RESULT.json').write_text(json.dumps(result, indent=2) + '\n')
    flags = ['c++', '-std=c++11', '-ffunction-sections', '-fdata-sections',
             '-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL', '-D__NO_OPENGL',
             '-I' + str(sdk / 'include/SDL'), '-I' + str(sdk / 'include'), '-I' + str(output)]
    flags += (['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize=vptr,enum',
               '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2'])
    library = sdk / 'lib/x86_64-linux-gnu'
    env = dict(os.environ, LD_LIBRARY_PATH=str(library) + ':' + os.environ.get('LD_LIBRARY_PATH', ''),
               ASAN_OPTIONS='detect_leaks=0')
    result['compile_command'] = flags + [str(fixture)] + [str(source / ('src/' + u + '.cpp')) for u in support.UNITS] + [
        '-Wl,--gc-sections', '-L' + str(library), '-lSDL', '-lSDL_gfx', '-lSDL_image', '-lSDL_mixer', '-lyaml-cpp', '-o', str(output / 'test')]
    save()
    syntax = subprocess.run(flags + ['-fsyntax-only', str(source / 'src/Geoscape/AllocatePsiTrainingState.cpp')], env=env, capture_output=True, text=True)
    (output / 'syntax.log').write_text(syntax.stdout + syntax.stderr)
    result['syntax_exit'] = syntax.returncode
    if syntax.returncode:
        save()
        raise SystemExit(syntax.stderr)
    build = subprocess.run(result['compile_command'], env=env, capture_output=True, text=True)
    (output / 'build.log').write_text(build.stdout + build.stderr)
    result['build_exit'] = build.returncode
    save()
    if build.returncode:
        raise SystemExit(build.stderr)
    test = subprocess.run([str(output / 'test'), str(source)], cwd=data / 'common', env=env, capture_output=True, text=True)
    (output / 'test.log').write_text(test.stdout + test.stderr)
    result['test_exit'] = test.returncode
    result['inputs_unchanged'] = all(sha(Path(p)) == h for p, h in hashes.items()) and all(sha(Path(p)) == h for p,h in asset_hashes.items())
    result['all_passed'] = result['inputs_unchanged'] and test.returncode == 0 and 'PASS ' in test.stdout
    if result['all_passed']:
        result['legacy_comparison'] = legacy_comparison(source, output, flags, support, library, env, data, fixture, args.sanitize)['all_passed']
        result['inputs_unchanged'] = all(sha(Path(p)) == h for p, h in hashes.items()) and all(sha(Path(p)) == h for p,h in asset_hashes.items())
        result['all_passed'] = result['inputs_unchanged'] and result['legacy_comparison']
    save()
    print(test.stdout + test.stderr)
    return 0 if result['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
