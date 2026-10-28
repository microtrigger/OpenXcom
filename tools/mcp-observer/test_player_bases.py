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
  if key in ('death','home_base_id','fuel_to_home','distance_to_home'):r[key]=None
  elif key=='destination':r[key]=dict(kind='none',base_id=None,waypoint_id=None)
  elif key=='time' and kind in ('campaign','mission_history'):r[key]={x:0 for x in ('year','month','day','hour','minute','second')}
  elif key in b.BREAKDOWNS:r[key]={'KNOWN':1}
  elif key in ('noun','description','ufo','alien_race'):r[key]=None
  elif key=='mission_id':r[key]='mission-0'
  elif key=='logistics':r[key]=dict(storage_size=0.1,sell_price=10,purchase_price=None,purchase_delivery_hours=None)
  elif key=='physical':r[key]=dict(weight=1,width=1,height=1,clip_capacity=0,medikit=None)
  elif key=='technical':r[key]=None
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
 if kind in ('research_knowledge','own_waypoint','mission_history','item_definition'):r['base_id']=''
 if kind=='mission_history':r['daylight']='day'
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
   for key in ('enemy','target','tactical','hidden_aliens','occupancy_prediction') + (() if k=='craft' else ('destination',)):
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
  for old in ('openxcom-player-bases/1','openxcom-player-bases/2','openxcom-player-bases/3','openxcom-player-bases/4'):
   p=page();p['schema']=old
   with self.assertRaises(Unavailable):b.page(dump(p),CHALLENGE,EPOCH,None,0,16)
  peer=Peer([sample('campaign'),r])
  try:
   o=peer.client.read_player_bases();self.assertEqual(o['records'][1]['expense_ledger'],r['expense_ledger']);self.assertIsNone(peer.client.capability);self.assertFalse(any(x.startswith(b'E ') for x in peer.requests))
  finally:peer.close()
 def test_multi_base_and_related_entities(self):
  records=[sample(k) for k in b.FIELDS];second=sample('base');second['id']=second['base_id']='base-1';records.append(second)
  for offset in range(0,len(records),16):
   b.page(dump(page(records[offset:offset+16],offset,len(records))),CHALLENGE,EPOCH,None if offset==0 else TOKEN,offset,16)
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
  self.assertEqual(b.SCHEMA,'openxcom-player-bases/5')
  old=page();old['schema']='openxcom-player-bases/1'
  with self.assertRaises(Unavailable):b.page(dump(old),CHALLENGE,EPOCH,None,0,16)
  for kind in ('soldier','soldier_diary','soldier_commendation'):
   for key in ('enemy_type','kill_records','mission_ids','target_id','position','capability','actions') + (() if kind=='soldier_commendation' else ('noun',)):
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
 def test_known_research_offers_are_closed_read_records(self):
  for kind in ('research_knowledge','research_offer','manufacture_offer','own_waypoint'):
   r=sample(kind);b.record(r)
   for field in ('unlock_graph','future_research','enemy_target','actions','capability','rule_pointer'):
    bad=copy.deepcopy(r);bad[field]=[]
    with self.assertRaises(Unavailable):b.record(bad)
  for kind in ('research_knowledge','own_waypoint'):
   bad=sample(kind);bad['base_id']='base-0'
   with self.assertRaises(Unavailable):b.record(bad)
  for kind in ('research_offer','manufacture_offer'):
   bad=sample(kind);bad['base_id']=''
   with self.assertRaises(Unavailable):b.record(bad)
  for field in ('needs_item','destroys_item'):
   for value in (0,1,None,'true'):
    bad=sample('research_offer');bad[field]=value
    with self.assertRaises(Unavailable):b.record(bad)
  for kind,field in (('research_offer','base_cost'),('manufacture_offer','hours_per_unit'),('manufacture_offer','cost_per_unit')):
   for value in (True,1.2,None,'1'):
    bad=sample(kind);bad[field]=value
    with self.assertRaises(Unavailable):b.record(bad)
  r=sample('manufacture_offer');r['required_items']={str(i):1 for i in range(1000)};b.record(r)
  r['required_items']['overflow']=1
  with self.assertRaises(Unavailable):b.record(r)
  r=sample('production');r['required_items']={str(i):1 for i in range(1001)};b.record(r)
 def test_safe_craft_destination_closed_and_nullable_home_metrics(self):
  r=sample('craft')
  for kind in ('none','base','waypoint','other'):
   r['destination']=dict(kind=kind,base_id='base-1' if kind=='base' else None,waypoint_id='waypoint-1' if kind=='waypoint' else None);b.record(r)
  for home in (None,'base-0'):
   for fuel,distance in ((None,None),(None,0.2),(12,0.2)):
    r.update(home_base_id=home,fuel_to_home=fuel,distance_to_home=distance)
    if home is None and (fuel is not None or distance is not None):
     with self.assertRaises(Unavailable):b.record(r)
    else:b.record(r)
  r.update(home_base_id='base-0',fuel_consumption=-1,fuel_to_home=-2,distance_to_home=0);b.record(r)
  for field,value in (('fuel_to_home',True),('distance_to_home',float('nan')),('distance_to_home',-0.1),('in_dogfight',1),('in_battlescape',None),('interception_order','1'),('home_base_id','')):
   bad=copy.deepcopy(r);bad[field]=value
   with self.assertRaises(Unavailable):b.record(bad)
  for destination in (dict(kind='ufo',base_id=None,waypoint_id=None),dict(kind='base',base_id=None,waypoint_id=None),dict(kind='other',base_id='base-1',waypoint_id=None),dict(kind='waypoint',base_id=None,waypoint_id=''),dict(kind='none',base_id=None,waypoint_id=None,longitude=1),dict(kind='base',base_id='base-1',waypoint_id='waypoint-1')):
   bad=copy.deepcopy(r);bad['destination']=destination
   with self.assertRaises(Unavailable):b.record(bad)
  for field in ('altitude','enemy_id','target_coordinates','meet_point','detected'):
   bad=copy.deepcopy(r);bad[field]=None
   with self.assertRaises(Unavailable):b.record(bad)
 def test_new_read_records_never_send_ui_actions(self):
  records=[sample(k) for k in ('research_knowledge','research_offer','manufacture_offer','own_waypoint','craft')]
  peer=Peer(records)
  try:
   result=peer.client.read_player_bases();self.assertEqual(result['records'],records)
   self.assertIsNone(peer.client.capability);self.assertEqual(peer.client.commands,{})
   self.assertEqual([p.split()[0] for p in peer.requests],[b'H',b'B'])
  finally:peer.close()
 def test_archived_history_refs_maps_and_localized_awards(self):
  for kind in ('mission_history','soldier_mission','soldier_encounter'):
   r=sample(kind);b.record(r)
   if kind!='mission_history':r['base_id']='';b.record(r)
   for key in ('enemy_id','victim_id','turn','coordinates','raw_rank','raw_race','capability'):
    bad=copy.deepcopy(r);bad[key]=1
    with self.assertRaises(Unavailable):b.record(bad)
  for key,value in (('base_id','base-0'),('daylight','dusk'),('success',1),('time',{}),('score',True),('ufo',4)):
   bad=sample('mission_history');bad[key]=value
   with self.assertRaises(Unavailable):b.record(bad)
  for kind in ('soldier_mission','soldier_encounter'):
   for key in ('mission_id','soldier_id'):
    for value in ('',None,'x'*513,3):
     bad=sample(kind);bad[key]=value
     with self.assertRaises(Unavailable):b.record(bad)
  for key in b.BREAKDOWNS:
   r=sample('soldier_diary');r[key]={str(i):1 for i in range(1000)};b.record(r)
   for value in ({'a':True},{'a':1.5},{'a':None},{str(i):1 for i in range(1001)},[]):
    r[key]=value
    with self.assertRaises(Unavailable):b.record(r)
  for description in (None,'Award description'):
   r=sample('soldier_commendation');r.update(noun='Localized noun',description=description,name='Award',decoration_description='Level');b.record(r)
  r['noun']=True
  with self.assertRaises(Unavailable):b.record(r)
 def test_owned_item_grouped_metadata_closed_and_unknown_technical(self):
  item=sample('item_definition');b.record(item)
  technical=dict(article_id='KNOWN_ARTICLE',fire_modes=[dict(mode='snap',accuracy=60,tu=20,tu_unit='percent')],damage=dict(type='AP',power=30,pellets=None),compatible_ammo=['OWN_AMMO'])
  item.update(identified=True,technical=technical);item['physical']['medikit']=dict(heal=3,painkiller=2,stimulant=1);b.record(item)
  for group,field,value in (('logistics','storage_size',float('inf')),('logistics','sell_price',True),('logistics','purchase_price',10),('physical','weight',False),('physical','medikit',dict(heal=1,painkiller=1,stimulant=True)),('technical','article_id',None),('technical','compatible_ammo',['A']*2),('technical','compatible_ammo',['A','B','C','D'])):
   bad=copy.deepcopy(item);bad[group][field]=value
   with self.assertRaises(Unavailable):b.record(bad)
  for group in ('logistics','physical','technical'):
   bad=copy.deepcopy(item);bad[group]['hidden_rules']=[]
   with self.assertRaises(Unavailable):b.record(bad)
  for modes in ([dict(mode='burst',accuracy=1,tu=1,tu_unit='percent')],[dict(mode='snap',accuracy=True,tu=1,tu_unit='percent')],[dict(mode='snap',accuracy=1,tu=1,tu_unit='seconds')],[dict(mode=x,accuracy=1,tu=1,tu_unit='percent') for x in ('aimed','snap')],[dict(mode='snap',accuracy=1,tu=1,tu_unit='percent')]*2):
   bad=copy.deepcopy(item);bad['technical']['fire_modes']=modes
   with self.assertRaises(Unavailable):b.record(bad)
  for field,value in (('power',True),('pellets',1.2),('raw_damage_type',1)):
   bad=copy.deepcopy(item);bad['technical']['damage'][field]=value
   with self.assertRaises(Unavailable):b.record(bad)
  item['technical']['damage']=None;item['logistics'].update(purchase_price=-1,purchase_delivery_hours=0);b.record(item)
 def test_history_and_item_records_do_not_create_authority(self):
  records=[sample(k) for k in ('mission_history','soldier_mission','soldier_encounter','item_definition')]
  peer=Peer(records)
  try:
   self.assertEqual(peer.client.read_player_bases()['records'],records)
   self.assertIsNone(peer.client.capability);self.assertEqual(peer.client.commands,{})
   self.assertEqual([p.split()[0] for p in peer.requests],[b'H',b'B'])
  finally:peer.close()
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
