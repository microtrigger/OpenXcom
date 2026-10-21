#!/usr/bin/env python3
"""Real stock-font TextList capture; no live game or final desktop pixels."""
import argparse,importlib.util,json,os,pathlib,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sdk',type=pathlib.Path,required=True);p.add_argument('--data',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
src=pathlib.Path(__file__).resolve().parents[2];out=a.output.resolve();assert out!=src and src not in out.parents;out.mkdir(parents=True,exist_ok=False)
spec=importlib.util.spec_from_file_location('support',pathlib.Path(__file__).with_name('run-named-button-tests.py'));s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s);s.SOURCE=src
fixture=src/'tests/agent-control/InterceptionListPixelsTest.cpp';files={str(fixture.relative_to(src))}|{'src/'+u+'.cpp' for u in s.UNITS};result={'passed':False,'live_acceptance':False,'game_executed':False,'scope':'Real offscreen stock-font TextList capture and dedicated projection; State::setModal/Game::isState abort seams; null State game pointer','source_sha256':s.closure(files)}
sdk=a.sdk.resolve();lib=sdk/'lib/x86_64-linux-gnu';env=dict(os.environ,LD_LIBRARY_PATH=str(lib),ASAN_OPTIONS='detect_leaks=0');flags=['c++','-std=c++11','-ffunction-sections','-fdata-sections','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-D__NO_OPENGL','-DOPENXCOM_LOADING_PRESENTATION_HELPERS','-I'+str(sdk/'include/SDL'),'-I'+str(sdk/'include')];flags+=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize=vptr','-fno-sanitize-recover=all'] if a.sanitize else ['-O2']
cmd=flags+[str(fixture)]+[str(src/('src/'+u+'.cpp')) for u in s.UNITS]+['-Wl,--gc-sections','-L'+str(lib),'-Wl,-rpath,'+str(lib),'-lSDL','-lSDL_image','-lSDL_gfx','-lSDL_mixer','-lyaml-cpp','-o',str(out/'test')]
try:
 for name,command,cwd in [('compile',cmd,src),('run',[str(out/'test')],a.data.resolve()/'common')]:
  r=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,timeout=900);(out/(name+'.log')).write_text(r.stdout+r.stderr);result[name+'_exit']=r.returncode;print(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:],flush=True);assert r.returncode==0
 assert all(s.sha(src/f)==h for f,h in result['source_sha256'].items());result['passed']=True
finally:(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
