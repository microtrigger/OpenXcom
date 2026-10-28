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
  if key=='death':r[key]=None
  elif key=='time' and kind=='campaign':r[key]={x:0 for x in ('year','month','day','hour','minute','second')}
  elif key=='personnel':r[key]={x:0 for x in 'soldiers scientists engineers available_soldiers available_scientists available_engineers allocated_scientists allocated_engineers'.split()}
  elif key=='capacities':r[key]={x:dict(capacity=0,used=0,free=0) if kind=='base' else 0 for x in b.CAPS.split()}
  elif key=='expense_ledger':r[key]=dict(crafts=[dict(type='CRAFT',unit_cost=100,quantity=0,total=0)],salaries=[dict(type='STR_ENGINEERS',unit_cost=10,quantity=2,total=20),dict(type='STR_SCIENTISTS',unit_cost=20,quantity=1,total=20)])
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
 def __init__(self,records=None):
  self.records=records
  parent,self.sock=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);self.client=a.AgentControlClient(parent,timeout=.3,startup_timeout=.3);self.requests=[];self.errors=[];self.sock.settimeout(2);self.t=threading.Thread(target=self.run);self.t.start()
 def run(self):
  try:
   self.sock.send(dump(dict(schema=a.SCHEMA,mode=a.MODE,epoch=EPOCH,ledger_capacity=256,audio_history='mixed_output')))
   while True:
    raw=self.sock.recv(512)
    if not raw:return
    self.requests.append(raw);parts=raw.decode().split()
    if parts[0]=='H':o=dict(schema=a.SCHEMA,mode=a.MODE,status='ready')
    elif parts[0]=='B':o=page(self.records);o['challenge']=parts[1]
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
 def test_expense_ledger_closed_bounds_products_and_old_schema(self):
  r=sample('base');b.record(r)
  for field,value in [('unit_cost',True),('quantity',-1),('quantity',1.0),('total',1),('type','')]:
   bad=copy.deepcopy(r);bad['expense_ledger']['crafts'][0][field]=value
   with self.assertRaises(Unavailable):b.record(bad)
  for transform in (lambda v:v.update(enemy_cost=0),lambda v:v['crafts'].append(copy.deepcopy(v['crafts'][0])),lambda v:v['salaries'][0].update(hidden_rule=True),lambda v:v.update(crafts=[dict(type=str(i),unit_cost=0,quantity=0,total=0) for i in range(257)])):
   bad=copy.deepcopy(r);transform(bad['expense_ledger'])
   with self.assertRaises(Unavailable):b.record(bad)
  for old in ('openxcom-player-bases/1','openxcom-player-bases/2'):
   p=page();p['schema']=old
   with self.assertRaises(Unavailable):b.page(dump(p),CHALLENGE,EPOCH,None,0,16)
  peer=Peer([sample('campaign'),r])
  try:
   o=peer.client.read_player_bases();self.assertEqual(o['records'][1]['expense_ledger'],r['expense_ledger']);self.assertIsNone(peer.client.capability);self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
  finally:peer.close()
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
 def test_history_schema_and_closed_nested_fields(self):
  self.assertEqual(b.SCHEMA,'openxcom-player-bases/3')
  old=page();old['schema']='openxcom-player-bases/1'
  with self.assertRaises(Unavailable):b.page(dump(old),CHALLENGE,EPOCH,None,0,16)
  for kind in ('soldier','soldier_diary','soldier_commendation'):
   for key in ('enemy_type','noun','kill_records','mission_ids','target_id','position','capability','actions'):
    bad=sample(kind);bad[key]='private'
    with self.assertRaises(Unavailable):b.record(bad)
  for key in ('recently_promoted',):
   for value in (0,1,None,'true'):
    bad=sample('soldier');bad[key]=value
    with self.assertRaises(Unavailable):b.record(bad)
  for kind,key in (('soldier_diary','shots_fired'),('soldier_commendation','decoration_level')):
   for value in (True,1.5,'1',None):
    bad=sample(kind);bad[key]=value
    with self.assertRaises(Unavailable):b.record(bad)
  bad=sample('soldier_commendation');bad['is_new']=1
  with self.assertRaises(Unavailable):b.record(bad)
 def test_memorial_and_death_timestamp_without_cause_details(self):
  dead=sample('soldier');dead.update(base_id='',location='memorial',craft_id=None,transfer_id=None,
    death=dict(status='kia',time=dict(year=1999,month=1,day=2,hour=3,minute=4,second=5)))
  for status in ('kia','mia'):
   dead['death']['status']=status;b.record(dead)
  for key,value in (('base_id','base-0'),('location','home'),('craft_id','craft-0'),('transfer_id','transfer-0'),('death',None)):
   bad=copy.deepcopy(dead);bad[key]=value
   with self.assertRaises(Unavailable):b.record(bad)
  for key,value in (('status','enemy_killed'),('cause',{}),('killer_id','hidden'),('time',{})):
   bad=copy.deepcopy(dead);bad['death'][key]=value
   with self.assertRaises(Unavailable):b.record(bad)
  live=sample('soldier');live['death']=copy.deepcopy(dead['death'])
  with self.assertRaises(Unavailable):b.record(live)
  for kind in ('soldier_diary','soldier_commendation'):
   r=sample(kind);r['base_id']='';b.record(r)
  bad=sample('base');bad['base_id']=''
  with self.assertRaises(Unavailable):b.record(bad)
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
  peer=Peer([sample('soldier'),sample('soldier_diary'),sample('soldier_commendation')])
  try:
   peer.client.observe();cap=peer.client.capability;revision=peer.client.revision;available=peer.client.available_actions
   first=peer.client.read_player_bases();second=peer.client.read_player_bases()
   self.assertEqual((first['snapshot'],first['generation']),(second['snapshot'],second['generation']))
   self.assertEqual((peer.client.capability,peer.client.revision,peer.client.available_actions),(cap,revision,available))
   self.assertEqual(peer.client.commands,{})
   self.assertEqual([r['kind'] for r in first['records']],['soldier','soldier_diary','soldier_commendation'])
   self.assertFalse(any(p.startswith(b'E ') for p in peer.requests))
  finally:peer.close()
if __name__=='__main__':unittest.main()
