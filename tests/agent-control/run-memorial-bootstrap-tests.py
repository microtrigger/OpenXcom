#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the trusted default-off native fixture parser; never initializes Game."""
import argparse
import hashlib
import json
import os
import shlex
import tempfile
from pathlib import Path
from native_test_runner import run_recipe

DRIVER = r'''
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <iostream>
#include <vector>
void require(bool ok) { if (!ok) throw std::runtime_error("rejected"); }
'''
CASES = r'''
int main() {
 struct Case { std::vector<std::string> args; const char *control; int code; std::string output; };
 std::vector<Case> cases={
  {{},nullptr,0,"0 0 1"},
  {{"--agent-report-fixture","memorial-history-v1"},"1",0,"1 1 1"},
  {{"--agent-report-fixture","memorial-history-v1","-master","xcom2"},"1",0,"1 1 3"},
  {{"--agent-report-fixture","memorial-history-v1"},nullptr,7,""},
  {{"--agent-report-fixture","memorial-history-v1"},"0",7,""},
  {{"--agent-report-fixture"},"1",7,""},
  {{"--agent-report-fixture","unknown"},"1",7,""},
  {{"--agent-report-fixture","memorial-history-v1","--agent-report-fixture","memorial-history-v1"},"1",7,""},
  {{"--agent-report-fixture","containment-stock-v1"},"1",0,"1 0 1"}
 };
 for (std::size_t i=0;i<cases.size();++i) {
  auto c=cases[i]; unsetenv("OPENXCOM_AGENT_CONTROL");
  if(c.control) setenv("OPENXCOM_AGENT_CONTROL",c.control,1);
  requested=finished=gameplay=armored=equippedVehicle=missionContext=false;
  inventoryBody=inventoryPaging=manufactureCampaign=completionCampaign=outfittingCampaign=containmentCampaign=memorialCampaign=false;
  std::vector<std::string> values={"parser"}; values.insert(values.end(),c.args.begin(),c.args.end());
  std::vector<char*> argv; for(auto &v:values) argv.push_back(&v[0]); argv.push_back(nullptr);
  int argc=static_cast<int>(values.size()),code=0; std::string output;
  try { prepare(argc,argv.data());output=std::to_string(requested)+" "+std::to_string(memorialCampaign)+" "+std::to_string(argc); }
  catch (...) { code=7; }
  if(code!=c.code || output!=c.output) {std::cerr<<"FAIL case "<<i<<"\n";return 1;}
 }
 std::cout<<"PASS: 9 actual native parser cases; no campaign initialized\n";
}
'''


def integrate(source, output, build=None, sdk=None):
    source, output = Path(source).resolve(), Path(output).resolve()
    if (build is None)!=(sdk is None):raise ValueError('build and sdk must be paired')
    cpp = source / 'tests/agent-report-fixture/AgentReportFixture.cpp'
    original = cpp.read_bytes(); text = original.decode()
    globals_ = text[text.index('bool requested ='):text.index('void require(')]
    parser = text[text.index('void prepare('):text.index('bool waiting()')]
    with tempfile.TemporaryDirectory(prefix='memorial-parser-inputs-', dir=output.parent) as temporary:
        generated = Path(temporary) / 'parser.cpp'
        generated.write_text(DRIVER + globals_ + parser + CASES)
        if cpp.read_bytes() != original:
            raise ValueError('Parser source changed during extraction')
        inputs=[cpp, source / 'tests/agent-report-fixture/AgentReportFixture.h', generated,
                Path(__file__).resolve(), Path(__file__).with_name('native_test_runner.py').resolve()]
        def step(name, argv):
            return dict(name=name,argv=[str(x) for x in argv],cwd=str(source),env={})
        steps=[step('compile_parser',['c++','-std=c++11',generated,'-o',output/'parser']),
               step('test_parser',[output/'parser'])]
        if build is not None:
            build,sdk=Path(build).resolve(),Path(sdk).resolve()
            begin='        for(int i=0;i<3;++i)';end='        base->setName("SYNTHETIC Memorial");'
            if text.count(begin)!=1 or text.count(end)!=1:raise ValueError('Unreviewed Memorial setup seam')
            loop=Path(temporary)/'MemorialSetup.generated.inc'
            loop.write_text(text[text.index(begin):text.index(end)])
            fixture=source/'tests/agent-report-fixture/AgentMemorialBootstrapTest.cpp'
            link_recipe=build/'src/CMakeFiles/openxcom.dir/link.txt'
            objects=[(build/'src'/token).resolve() for token in shlex.split(link_recipe.read_text())
                     if token.endswith('.o') and not token.endswith('/main.cpp.o')]
            if not objects:raise ValueError('No native build object closure')
            inputs += [loop,fixture,link_recipe]+objects
            inputs += [source/p for p in ('src/Savegame/Soldier.cpp','src/Savegame/Soldier.h','src/Savegame/SoldierDiary.cpp','src/Savegame/SavedGame.cpp','src/Savegame/MissionStatistics.h')]
            library=sdk/'lib/x86_64-linux-gnu'
            flags=['c++','-std=c++11','-O0','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-DOPENXCOM_EXPERIMENTAL_OBSERVER_DIAGNOSTICS','-DOPENXCOM_LOADING_PRESENTATION_HELPERS','-D__NO_OPENGL','-I'+str(sdk/'include/SDL'),'-I'+str(sdk/'include'),'-I'+str(temporary)]
            model_steps=[step('compile_model',flags+['-c',fixture,'-o',output/'model.o']),
                step('link_model',['c++',output/'model.o']+objects+['-Wl,--gc-sections','-L'+str(library),'-lSDL','-lSDL_image','-lSDL_mixer','-lSDL_gfx','-lyaml-cpp','-ldl','-pthread','-o',output/'model']),
                step('test_model',[output/'model'])]
            for model_step in model_steps:model_step['env']={'LD_LIBRARY_PATH':str(library)+':'+os.environ.get('LD_LIBRARY_PATH','')}
            steps+=model_steps
        if cpp.read_bytes()!=original:raise ValueError('Bootstrap source changed during preparation')
        receipt=run_recipe(dict(version=1,source=str(source),output=str(output),inputs=list(dict.fromkeys(str(p) for p in inputs)),steps=steps))
        (output/'QUALIFICATION.json').write_text(json.dumps(dict(
            all_passed=receipt['all_passed'],inputs_unchanged=receipt['inputs_unchanged'],
            game_executed=False,live_acceptance=False,model_loop_checked=build is not None,source_revision='caller-supplied; inspect recorded input hashes',
            generated_source_sha256=hashlib.sha256(generated.read_bytes()).hexdigest(),
            limitations=['Literal native parser/globals extracted into an isolated nine-case driver; no Game/campaign/save execution.',
                        'The parser driver resets selection flags/environment; optional model mode executes only the literal three-soldier setup loop against synthetic native models, not Game/start or a saved campaign.',
                        'Explicit source manifest only, not a complete compiler/header dependency closure.']),indent=2)+'\n')
        return receipt


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--build',type=Path);parser.add_argument('--sdk',type=Path)
    args=parser.parse_args()
    try:receipt=integrate(args.source,args.output,args.build,args.sdk)
    except (ValueError,OSError) as error:
        print('Memorial parser runner refused input: '+type(error).__name__);return 2
    return 0 if receipt['all_passed'] else 1

if __name__=='__main__':raise SystemExit(main())
