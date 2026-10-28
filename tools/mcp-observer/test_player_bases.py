# SPDX-License-Identifier: GPL-3.0-or-later
"""Owned strategic read model: closed fields, independent channel read, no UI grants."""
import copy,json,socket,threading,unittest
import agent_control as a
import player_bases as b
from snapshots import Unavailable
from test_agent_control import state,EPOCH,CHALLENGE,dump
TOKEN='a'*32

def sample(kind, index=0):
 r=dict(kind=kind,id=f'{kind}-{index}',base_id='base-0')
 for key in b.FIELDS[kind].split():
  if key=='time' and kind=='campaign':r[key]={x:0 for x in ('year','month','day','hour','minute','second')}
  elif key=='personnel':r[key]={x:0 for x in 'soldiers scientists engineers available_soldiers available_scientists available_engineers allocated_scientists allocated_engineers'.split()}
  elif key=='capacities':r[key]={x:dict(capacity=0,used=0,free=0) if kind=='base' else 0 for x in b.CAPS.split()}
  elif key=='maintenance':r[key]={x:0 for x in ('craft','personnel','facilities','total')}
  elif key in ('initial_stats','current_stats'):r[key]={x:0 for x in b.STATS.split()}
  elif key in ('required_items','produced_items'):r[key]={'ITEM':1}
  elif key=='crew':r[key]=['soldier-0']
  elif key in b.BOOLS:r[key]=False
  elif key in b.STRINGS and not(key=='slot' and kind=='weapon'):r[key]='shown'
  else:r[key]=0
 if kind=='campaign':r['id']='campaign_0';r['base_id']=''
 if kind=='base':r['grid_size']=6;r['id']=r['base_id']
 if kind in ('soldier','craft'):r['location']='home_roster' if kind=='craft' else 'home';r['transfer_id']=None
 if kind=='transfer':r['transfer_type']='item';r['item_type']='ITEM';r['craft_id']=None;r['soldier_id']=None
 return r

def page(records=None,offset=0,total=None):
 records=[sample('base')] if records is None else records
 total=len(records) if total is None else total
 return dict(schema=b.SCHEMA,type='player_bases',epoch=EPOCH,challenge=CHALLENGE,campaign_epoch='b'*32,status='ok',reason=None,snapshot=TOKEN,generation=1,offset=offset,next_offset=offset+len(records),total=total,complete=offset+len(records)==total,records=records)

class Peer:
 def __init__(self):
  parent,self.sock=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);self.client=a.AgentControlClient(parent,timeout=.3,startup_timeout=.3);self.requests=[];self.errors=[];self.sock.settimeout(2);self.t=threading.Thread(target=self.run);self.t.start()
 def run(self):
  try:
   self.sock.send(dump(dict(schema=a.SCHEMA,mode=a.MODE,epoch=EPOCH,ledger_capacity=256,audio_history='mixed_output')))
   while True:
    raw=self.sock.recv(512)
    if not raw:return
    self.requests.append(raw);parts=raw.decode().split()
    if parts[0]=='H':o=dict(schema=a.SCHEMA,mode=a.MODE,status='ready')
    elif parts[0]=='B':o=page();o['challenge']=parts[1]
    elif parts[0]=='R':o=state();o['challenge']=parts[1]
    else:raise RuntimeError('unexpected mutation')
    self.sock.send(dump(o))
  except Exception as e:self.errors.append(e)
 def close(self):
  self.client.close();self.t.join(2);self.sock.close()
  if self.t.is_alive() or self.errors:raise AssertionError(self.errors)

class BasesTests(unittest.TestCase):
 def test_all_types_closed_and_no_enemy_fields(self):
  for k in b.FIELDS:
   r=sample(k);b.record(r)
   for key in tuple(r):
    bad=copy.deepcopy(r);del bad[key]
    with self.assertRaises(Unavailable):b.record(bad)
   for key in ('enemy','destination','target','tactical','hidden_aliens','occupancy_prediction'):
    bad=copy.deepcopy(r);bad[key]=None
    with self.assertRaises(Unavailable):b.record(bad)
 def test_multi_base_and_related_entities(self):
  records=[sample(k) for k in b.FIELDS];second=sample('base');second['id']=second['base_id']='base-1';records.append(second)
  b.page(dump(page(records)),CHALLENGE,EPOCH,None,0,16)
 def test_page_exact_consistency_and_errors(self):
  b.page(dump(page()),CHALLENGE,EPOCH,None,0,16)
  for key,value in [('epoch','f'*32),('challenge','f'*32),('next_offset',2),('complete',False),('total',0),('records',[sample('base'),sample('base')])]:
   p=page();p[key]=value
   with self.assertRaises(Unavailable):b.page(dump(p),CHALLENGE,EPOCH,None,0,16)
  p=page([]);p.update(status='unavailable',reason='no_campaign',snapshot=None,total=None,complete=False,generation=0)
  b.page(dump(p),CHALLENGE,EPOCH,None,0,16)
  p['complete']=True
  with self.assertRaises(Unavailable):b.page(dump(p),CHALLENGE,EPOCH,None,0,16)
 def test_arguments_no_authority_or_bad_pagination(self):
  b.arguments({});b.arguments(dict(snapshot=TOKEN,offset=16,limit=1))
  for args in ({'offset':1},{'limit':0},{'limit':17},{'snapshot':'x'},{'offset':True},{'capability':TOKEN},{'execute':True}):
   with self.assertRaises(Unavailable):b.arguments(args)
 def test_adapter_routes_dedicated_readonly_tool(self):
  import adapter
  from test_agent_control import call
  class ReadOnly:
   def read_player_bases(self,**args):return page()
  server=adapter.Server(agent_control=ReadOnly())
  result=call(server,'read_player_bases',{})
  self.assertNotIn('error',result)
  self.assertIn('read_player_bases',{d['name'] for d in a.definitions()})
 def test_first_read_no_ui_observation(self):
  peer=Peer()
  try:
   obj=peer.client.read_player_bases();self.assertEqual(obj['records'][0]['kind'],'base')
   self.assertEqual([p.split()[0] for p in peer.requests],[b'H',b'B'])
   self.assertIsNone(peer.client.capability);self.assertEqual(peer.client.commands,{})
  finally:peer.close()
 def test_read_preserves_existing_ui_grant_and_ledger(self):
  peer=Peer()
  try:
   peer.client.observe();cap=peer.client.capability;revision=peer.client.revision;available=peer.client.available_actions
   first=peer.client.read_player_bases();second=peer.client.read_player_bases()
   self.assertEqual((first['snapshot'],first['generation']),(second['snapshot'],second['generation']))
   self.assertEqual((peer.client.capability,peer.client.revision,peer.client.available_actions),(cap,revision,available))
   self.assertEqual(peer.client.commands,{})
  finally:peer.close()
if __name__=='__main__':unittest.main()
