#!/usr/bin/env python3
"""Actual offscreen BattlescapeButton render/selection proof; no game/runtime claim."""
import argparse,ast,hashlib,json,os,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('--sdk',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False);sdk=a.sdk.resolve();prefix=sdk/'usr' if (sdk/'usr/include').exists() else sdk
# Share the established native widget link-unit inventory, not generated game state.
tree=ast.parse((r/'tests/agent-control/run-dogfight-providers.py').read_text());units=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='UNITS' for t in n.targets));units+=['Interface/BattlescapeButton']
flags=['c++','-std=c++11','-O2','-ffunction-sections','-fdata-sections','-DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL','-D__NO_OPENGL','-DOPENXCOM_LOADING_PRESENTATION_HELPERS','-I'+str(prefix/'include/SDL'),'-I'+str(prefix/'include')]
lib=prefix/'lib/x86_64-linux-gnu';env=dict(os.environ,LD_LIBRARY_PATH=str(lib));result={'passed':False,'scope':'real native widget with synthetic offscreen palette/geometry; not live gameplay'}
files=[r/('src/'+u+'.cpp') for u in units]+[r/'src/Interface/BattlescapeButton.h',pathlib.Path(__file__).resolve(),r/'tests/agent-control/AgentReserveButtonTest.cpp']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
result['source_sha256']={str(p.relative_to(r)):sha(p) for p in files}
try:
 cmds=[('compile',flags+[str(r/'tests/agent-control/AgentReserveButtonTest.cpp')]+[str(r/('src/'+u+'.cpp')) for u in units]+['-Wl,--gc-sections','-L'+str(lib),'-Wl,-rpath,'+str(lib),'-lSDL','-lSDL_image','-lSDL_gfx','-lSDL_mixer','-lyaml-cpp','-o',str(o/'test')]),('run',[str(o/'test')])]
 for name,cmd in cmds:
  z=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=180);(o/(name+'.log')).write_text(z.stdout+z.stderr);result[name]=z.returncode;print(name,z.returncode,z.stdout[-1500:],z.stderr[-1500:],flush=True)
  if z.returncode:raise RuntimeError(name+' failed')
 if any(sha(r/p)!=h for p,h in result['source_sha256'].items()):raise RuntimeError('source changed during qualification')
 result['artifact_sha256']=sha(o/'test')
 result['passed']=True
finally:(o/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
