import importlib.util,json,pathlib,copy,sys
p=pathlib.Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('mux',p/'mux_callback_audio.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
rows=[json.loads(x) for x in (pathlib.Path(sys.argv[1])/'audio-windows.jsonl').read_text().splitlines()]
f=m.model(rows);assert f['alignment_uncertainty_seconds']<.150;assert f['rate_rounding_error_seconds']<.0001
for kind in ('missing','clockjump','contradiction'):
 r=copy.deepcopy(rows)
 if kind=='missing':r.pop(10)
 elif kind=='clockjump':
  for item in r[len(r)//2:]:
   for k in ('first_realtime_us','last_realtime_us'):item['timing'][k]+=100000
 else:r[4]['timing']['first_monotonic_us']=1
 try:m.model(r)
 except AssertionError:pass
 else:raise AssertionError('accepted '+kind)
print('PASS actual long waveform precise rate and three adverse contracts')
r=copy.deepcopy(rows)
# Slow one observed callback interval dramatically: cannot claim sub150ms.
for item in r[len(r)//2:]:
 for k in ('first_monotonic_us','last_monotonic_us','first_realtime_us','last_realtime_us'):item['timing'][k]+=500000
try:m.model(r)
except AssertionError:pass
else:raise AssertionError('accepted callback discontinuity')
print('PASS callback discontinuity is rejected; gates unchanged')
