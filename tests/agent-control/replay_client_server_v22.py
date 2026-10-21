#!/usr/bin/env python3
"""Exact native socket bytes through the real Client and MCP Server; no game launch."""
import collections,datetime,hashlib,json,socket,sys,threading
from pathlib import Path
from unittest.mock import patch
import argparse, copy, importlib
ROOT=Path(__file__).resolve().parent

def configure(source):
 global adapter, a
 sys.dont_write_bytecode=True
 sys.path.insert(0,str(Path(source).resolve()/'tools/mcp-observer'))
 adapter=importlib.import_module('adapter')
 a=importlib.import_module('agent_control')

def call(server,name,args=None):
 server.state='ready'
 return server.handle(dict(jsonrpc='2.0',id=1,method='tools/call',params=dict(name=name,arguments={} if args is None else args)))

CHECKS=0
def require(ok,msg):
 global CHECKS
 CHECKS+=1
 if not ok:raise ValueError(msg)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return adapter.strict_json(p.read_bytes())
def action(request):
 f=request.decode('ascii').split(' ');require(f[0]=='E' and len(f)>=6,'Invalid native E')
 kind=a.ARGUMENT_KINDS[f[5]]
 if kind in ('production_quantity','purchase_quantity','equipment_quantity'):
  require(len(f) in (8,9),'Unexpected quantity suffix')
  args={'control' if kind=='production_quantity' else 'row':f[6],'operation':f[7]}
  if len(f)==9:args['value']=int(f[8])
 elif kind=='crew_assignment':
  require(len(f)==8 and f[7] in ('true','false'),'Unexpected desired crew suffix')
  args={'row':f[6],'assigned':f[7]=='true'}
 elif kind=='boolean_value':
  require(len(f)==7 and f[6] in ('true','false'),'Unexpected desired Sell suffix')
  args={'enabled':f[6]=='true'}
 elif kind=='purchase_scroll':
  require(len(f)==8,'Unexpected semantic scroll suffix');args=dict(direction=f[6],amount=f[7])
 else:
  key={'geoscape_speed':'speed','base_target':'target','scroll_direction':'direction','visible_row':'row',
       'project_row':'row','outfitting_row':'row','staff_direction':'direction','purchase_position':'target'}.get(kind)
  require((kind=='none' and len(f)==6) or (key is not None and len(f)==7),'Unexpected action suffix')
  args={} if kind=='none' else {key:f[6]}
 result=dict(command_id=f[1],epoch=f[2],capability=f[3],revision=int(f[4]),action=f[5],args=args)
 a.validate_action_arguments(result)
 require(a.canonical_action_request(result)==request,'Noncanonical native E');return result

def plan(schedule,responses):
 """Classify only a proven fresh-grant, unadvertised-action native negative.

 This is exercised at MCP, not omitted from coverage. Other native failures
 (notably offered-action ABA failures) must still traverse the real socket.
 """
 mapped=[]; indices=[]; known={}; blocked={}; duplicates=0; state=None
 for i,entry in enumerate(schedule[1:],1):
  request=entry['request'].encode('ascii')
  if request.startswith(b'R '):
   state=a.parse(responses[i])
  elif request.startswith(b'E '):
   args=action(request); key=args['command_id']
   if key in known:
    require(known[key]==request,'Changed native duplicate')
    request=b'S '+key.encode();duplicates+=1
   elif state is not None and args['action'] not in {x['id'] for x in state.get('actions',[])}:
    require(state.get('capability') is not None and
      all(args[k]==state[k] for k in ('epoch','capability','revision')),
      'Unadvertised action has stale or absent authority')
    result=a.parse(responses[i])
    require(result.get('type')=='result' and result.get('command_id')==key and
      result.get('status')=='unavailable' and result.get('observation') is None and
      result.get('event_sequence')==state['event_sequence'],
      'Unadvertised action is not an unchanged native negative')
    blocked[i]=args
    continue
   else:
    known[key]=request;state=None
  mapped.append(request);indices.append(i)
 return mapped,indices,known,blocked,duplicates

def client_snapshot(client):
 # Include the complete mutable ledger, authority and event bookkeeping.
 return {k:(v if k in ('channel','flight') else copy.deepcopy(v)) for k,v in vars(client).items()}

def replay(schedule_path,schedule):
 directory=schedule_path.parent
 responses=[(directory/e['response_file']).read_bytes() for e in schedule]
 require(schedule[0]['request'] is None and schedule[1]['request']=='H '+a.SCHEMA,'Invalid handshake schedule')
 original=[e['request'].encode() for e in schedule[1:]]
 mapped,indices,known,blocked,duplicates=plan(schedule,responses)
 challenges=[r.decode().split(' ')[1] for r in original if r[:2] in (b'R ',b'V ')]
 left,right=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);right.settimeout(5)
 seen=[];errors=[];done=threading.Event()
 def peer():
  try:
   right.send(responses[0])
   for request,response in zip(mapped,(responses[i] for i in indices)):
    actual=right.recv(a.MAX_REQUEST+1);seen.append(actual)
    if actual!=request:raise ValueError('Exact native request mismatch: '+repr((actual,request)))
    right.send(response)
   done.wait(5)
  except BaseException as e:errors.append(repr(e))
  finally:right.close()
 thread=threading.Thread(target=peer);thread.start();client=a.AgentControlClient(left);server=adapter.Server(agent_control=client)
 def tool(name,args=None):
  response=call(server,name,args);require('result' in response and response['result']['isError'] is False,'MCP failure '+name+': '+repr(response))
  obj=response['result'];require([x['type'] for x in obj['content']]==['text'] and json.loads(obj['content'][0]['text'])==obj['structuredContent'],'Detached text/DTO');return obj['structuredContent']
 pages=[];states=[];receipts=[]
 try:
  with patch('agent_control.secrets.token_hex',side_effect=challenges):
   state=tool('read_ui_state');require(state==a.parse(responses[2]),'State bytes transformed');states.append(state)
   for index,(entry,response) in enumerate(zip(schedule[3:],responses[3:]),3):
    r=entry['request'].encode();kind=r[:1]
    if kind==b'R':value=tool('read_ui_state');states.append(value)
    elif kind==b'V':
     _,_,epoch,after,limit=r.decode().split(' ')
     auth=tuple(getattr(client,k) for k in ('capability','revision','event_sequence','capability_deadline','available_actions','geoscape_speeds','base_targets','scroll_directions','project_rows','staff_directions','quantity_directions','purchase_rows','purchase_targets','outfitting_rows','equipment_rows'))
     value=tool('read_public_events',dict(epoch=epoch,after=int(after),limit=int(limit)));pages.append(value)
     require(auth==tuple(getattr(client,k) for k in ('capability','revision','event_sequence','capability_deadline','available_actions','geoscape_speeds','base_targets','scroll_directions','project_rows','staff_directions','quantity_directions','purchase_rows','purchase_targets','outfitting_rows','equipment_rows')),'V changed action authority')
    elif kind==b'E':
     args=action(r)
     if index in blocked:
      require(args['command_id'] not in client.commands and client.state=='READY' and
              args['action'] not in client.available_actions,'Not the planned preflight negative')
      before=client_snapshot(client)
      with patch.object(client,'exchange',side_effect=AssertionError('Forbidden action attempted native exchange')) as exchange:
       rejected=call(server,'execute_ui_action',args)
       require(exchange.call_count==0,'Preflight rejection sent native request')
      require(client_snapshot(client)==before,'Preflight rejection changed client state or ledger')
      require(rejected.get('result',{}).get('isError') is True and
              rejected['result'].get('structuredContent')=={'error':'unavailable'},'Missing exact MCP preflight rejection')
      require([x['type'] for x in rejected['result']['content']]==['text'] and
              json.loads(rejected['result']['content'][0]['text'])=={'error':'unavailable'},'Detached rejection DTO')
      continue
     value=tool('execute_ui_action',args);receipts.append(value)
    elif kind==b'S':value=tool('read_ui_action_result',dict(command_id=r.decode().split(' ')[1]));receipts.append(value)
    else:raise ValueError('Unknown native request')
    require(value==a.parse(response),'Native response transformed')
   require(len(client.commands)==len(known),'Ledger admission count changed')
   # Re-read of a terminal native request must not acquire a new command identity.
   require(seen==mapped,'Incomplete exact replay')
 finally:client.close();done.set();thread.join(6)
 require(not thread.is_alive() and not errors,'Socket replay failed '+repr(errors))

 return dict(schedule=schedule_path.name,schedule_sha256=sha(schedule_path),responses=len(responses),
  response_sha256=[hashlib.sha256(x).hexdigest() for x in responses],request_counts=dict(collections.Counter(r[:1].decode() for r in mapped)),
  receipt_statuses=[x['status'] for x in receipts],states=[x['state'] for x in states],pages=len(pages),
  duplicate_E_mapped_to_status_S=duplicates,native_new_commands=len(known),
  action_ids=sorted({r.decode().split(' ')[5] for r in original if r.startswith(b'E ')}),
  unmodified_response_bytes=True,event_authority_neutral=True,
  preflight_rejected_native_cases=sorted(blocked),preflight_rejections=len(blocked),
  preflight_no_exchange_and_state_unchanged=True,
  maximum_response_bytes=max(map(len,responses)))


def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--source',type=Path,required=True)
 parser.add_argument('--packets',type=Path,required=True,help='Native schedule JSON or directory of *-schedule.json and response files')
 parser.add_argument('--output',type=Path,required=True,help='New directory outside source and packet inputs')
 args=parser.parse_args();source=args.source.resolve();packets=args.packets.resolve();output=args.output.resolve()
 require(not output.exists(),'Output must be new')
 require(not output.is_relative_to(source) and not output.is_relative_to(packets if packets.is_dir() else packets.parent),'Output must be outside inputs')
 configure(source)
 schedules=sorted(packets.glob('*-schedule.json')) if packets.is_dir() else [packets]
 require(bool(schedules),'No schedules')
 inputs={p:sha(p) for p in (source/'tools/mcp-observer').glob('*.py')}
 inputs[Path(__file__).resolve()]=sha(Path(__file__).resolve())
 for p in schedules:
  inputs[p]=sha(p)
  for entry in load(p):
   response=(p.parent/entry['response_file']).resolve()
   require(response.is_relative_to(p.parent.resolve()),'Response path escapes packet directory')
   inputs[response]=sha(response)
 output.mkdir(parents=True)
 results=[];error=None
 try:
  for p in schedules:results.append(replay(p,load(p)))
 except Exception as e:error=str(e)
 require(all(sha(p)==digest for p,digest in inputs.items()),'Inputs changed during replay')
 result=dict(all_passed=error is None,error=error,checks=CHECKS,results=results,
             timestamp_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
             source=str(source),packets=str(packets),inputs_sha256={str(p):v for p,v in inputs.items()},
             limitations='Recorded native packet replay only; no game, GUI, assets or new native execution.')
 (output/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(dict(all_passed=error is None,schedules=len(results),checks=CHECKS,error=error)))
 return 0 if error is None else 1

if __name__=='__main__':sys.exit(main())
