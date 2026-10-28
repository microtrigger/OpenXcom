# SPDX-License-Identifier: GPL-3.0-or-later
"""Explicit user-authorized strategic ownership read model. Never UI action authority."""
import math
from snapshots import Unavailable, exact
SCHEMA='openxcom-player-bases/7'
LIMIT=100000
CAPS='quarters stores laboratories workshops hangars psi_labs containment'
BREAKDOWNS='neutralizations_by_race neutralizations_by_rank hostile_neutralizations_by_weapon hostile_neutralizations_by_ammo missions_by_region missions_by_country missions_by_type missions_by_ufo'.split()
STATS='tu stamina health bravery reactions firing throwing strength psi_strength psi_skill melee'
FIELDS={
 'campaign':'funds time monthly_income monthly_maintenance',
 'base':'name longitude latitude grid_size personnel capacities maintenance expense_ledger defense short_range_detection long_range_detection',
 'facility':'type x y size build_days_remaining build_days_total build_cost monthly_cost capacities radar_range radar_chance defense hit_ratio lift hyperwave mind_shield grav_shield',
 'soldier':'native_id type name rank armor rank_value gender look missions kills recovery_days psi_improvement psi_strength_improvement psi_training craft_id location transfer_id initial_stats current_stats recently_promoted death',
 'soldier_diary':'soldier_id missions months_service days_wounded shots_fired shots_landed accuracy kills stuns panics mind_controls wins score '+ ' '.join(BREAKDOWNS),
 'soldier_commendation':'soldier_id type decoration_level is_new noun name description decoration_description',
 'mission_history':'native_id title type time region country ufo success rating score alien_race daylight loot_value valiant_crux',
 'soldier_mission':'soldier_id mission_id wounded_days',
 'soldier_encounter':'soldier_id mission_id status unit_name weapon',
 'item_definition':'type base_label inventory_label identified logistics physical technical',
 'craft':'native_id type name status location transfer_id longitude latitude speed fuel fuel_max damage damage_max speed_max repair_hours refuel_hours soldier_capacity vehicle_capacity item_capacity space_used space_available monthly_cost weapon_slot_count weapon_capacity low_fuel mission_complete crew home_base_id fuel_consumption fuel_to_home distance_to_home in_dogfight in_battlescape interception_order destination',
 'research':'type progress assigned spent cost',
 'research_knowledge':'type name',
 'research_offer':'type name base_cost needs_item destroys_item',
 'manufacture_offer':'type name category required_space hours_per_unit cost_per_unit required_items produced_items',
 'own_waypoint':'native_id name longitude latitude',
 'production':'type category amount_total amount_produced time_spent assigned_engineers unit_time unit_cost required_space infinite sell required_items produced_items',
 'transfer':'transfer_type hours quantity item_type craft_id soldier_id',
 'stock':'owner_id owner_kind type quantity',
 'weapon':'craft_id type slot ammo ammo_max rearm_rate rearming launcher_item clip_item',
 'vehicle':'owner_id owner_kind type ammo size',
 'equipment_layout':'soldier_id item_type slot ammo_item x y fuse_timer',
}
STRINGS=set('name type rank armor status progress category owner_id owner_kind craft_id soldier_id item_type slot ammo_item launcher_item clip_item transfer_id location transfer_type title region country rating daylight unit_name weapon decoration_description base_label inventory_label'.split())
BOOLS=set('lift hyperwave mind_shield grav_shield psi_training low_fuel mission_complete infinite sell rearming recently_promoted is_new needs_item destroys_item in_dogfight in_battlescape success valiant_crux identified'.split())
NULLABLE={'craft_id','soldier_id','item_type','transfer_id'}
def need(ok):
 if not ok:raise Unavailable('Unavailable')
def string(v,nullable=False):
 if v is None and nullable:return
 need(type(v)is str and len(v.encode('utf-8'))<=4096)
def ident(v):
 string(v);need(bool(v) and len(v.encode('utf-8'))<=512)
def integer(v):need(type(v)is int and -(2**53-1)<=v<=2**53-1)
def number(v):need(type(v)in (int,float) and math.isfinite(v) and abs(v)<=2**53-1)
def objnums(v,keys,nums=False):
 exact(v,keys)
 for n in v.values():(number if nums else integer)(n)
def record(r):
 need(type(r)is dict and type(r.get('kind'))is str and r['kind']in FIELDS)
 k=r['kind'];exact(r,'kind id base_id '+FIELDS[k]);ident(r['id']);
 if k=='campaign':need(r['id']=='campaign_0' and r['base_id']=='')
 elif k in ('research_knowledge','own_waypoint','mission_history','item_definition'):need(r['base_id']=='')
 elif k in ('soldier','soldier_diary','soldier_commendation','soldier_mission','soldier_encounter') and r['base_id']=='':pass
 else:ident(r['base_id'])
 if k=='base':need(r['id']==r['base_id'] and r['grid_size']==6)
 for key,v in r.items():
  if key in ('kind','id','base_id'):continue
  if key in ('mission_id','soldier_id') and k in ('soldier_mission','soldier_encounter'):
   ident(v)
  elif key in ('noun','description') and k=='soldier_commendation':string(v,True)
  elif key in ('ufo','alien_race') and k=='mission_history':string(v,True)
  elif key in BREAKDOWNS:
   need(type(v)is dict and len(v)<=1000)
   for a,b in v.items():string(a);integer(b)
  elif key=='logistics':
   exact(v,'storage_size sell_price purchase_price purchase_delivery_hours');number(v['storage_size'])
   for field in ('sell_price','purchase_price','purchase_delivery_hours'):
    if v[field] is not None:integer(v[field])
   need((v['purchase_price'] is None)==(v['purchase_delivery_hours'] is None))
  elif key=='physical':
   exact(v,'weight width height clip_capacity medikit')
   for field in ('weight','width','height','clip_capacity'):integer(v[field])
   if v['medikit'] is not None:objnums(v['medikit'],'heal painkiller stimulant')
  elif key=='technical':
   if v is not None:
    exact(v,'article_id fire_modes damage compatible_ammo');string(v['article_id'])
    need(type(v['fire_modes'])is list and len(v['fire_modes'])<=3);modes=[]
    for mode in v['fire_modes']:
     exact(mode,'mode accuracy tu tu_unit');need(mode['mode'] in ('auto','snap','aimed') and mode['tu_unit'] in ('absolute','percent'));integer(mode['accuracy']);integer(mode['tu']);modes.append(mode['mode'])
    need(modes==[x for x in ('auto','snap','aimed') if x in modes])
    if v['damage'] is not None:
     exact(v['damage'],'type power pellets');string(v['damage']['type']);integer(v['damage']['power'])
     if v['damage']['pellets'] is not None:integer(v['damage']['pellets'])
    need(type(v['compatible_ammo'])is list and len(v['compatible_ammo'])<=3)
    for ammo in v['compatible_ammo']:string(ammo)
    need(len(v['compatible_ammo'])==len(set(v['compatible_ammo'])))
  elif key=='home_base_id':
   if v is not None:ident(v)
  elif key in ('repair_hours','refuel_hours'):
   if v is not None:integer(v);need(0<=v<=2**31-1)
  elif key in ('fuel_to_home','distance_to_home'):
   if v is not None:(number if key=='distance_to_home' else integer)(v)
  elif key=='destination':
   exact(v,'kind base_id waypoint_id');need(v['kind'] in ('none','base','waypoint','other'))
   for field,kind in (('base_id','base'),('waypoint_id','waypoint')):
    if v['kind']==kind:ident(v[field])
    else:need(v[field] is None)
  elif key=='death':
   if v is not None:
    exact(v,'status time');need(v['status'] in ('kia','mia'));objnums(v['time'],'year month day hour minute second')
  elif key=='time' and k in ('campaign','mission_history'):objnums(v,'year month day hour minute second')
  elif key=='personnel':objnums(v,'soldiers scientists engineers available_soldiers available_scientists available_engineers allocated_scientists allocated_engineers')
  elif key=='capacities':
   exact(v,CAPS)
   for entry in v.values():
    if k=='base':objnums(entry,'capacity used free',True)
    else:integer(entry)
  elif key=='expense_ledger':
   exact(v,'crafts salaries')
   for rows in v.values():
    need(type(rows)is list and len(rows)<=256);seen=set()
    for row in rows:
     exact(row,'type unit_cost quantity total');string(row['type']);need(bool(row['type']) and row['type'] not in seen);seen.add(row['type'])
     for field in ('unit_cost','quantity','total'):integer(row[field])
     need(-(2**31)<=row['unit_cost']<=2**31-1 and 0<=row['quantity']<=2**31-1)
     need(row['total']==row['unit_cost']*row['quantity'])
  elif key=='maintenance':objnums(v,'craft personnel facilities total')
  elif key in ('initial_stats','current_stats'):
   exact(v,STATS)
   for stat,n in v.items():
    if stat in ('psi_strength','psi_skill') and n is None:continue
    integer(n)
  elif key in ('psi_improvement','psi_strength_improvement'):
   if v is not None:integer(v)
  elif key in ('required_items','produced_items'):
   need(type(v)is dict and len(v)<=(1000 if k=='manufacture_offer' else 4096))
   for a,b in v.items():string(a);integer(b)
  elif key=='crew':
   need(type(v)is list and len(v)<=4096)
   for a in v:ident(a)
   need(len(v)==len(set(v)))
  elif key in ('longitude','latitude'):number(v)
  elif key in BOOLS:need(type(v)is bool)
  elif key in STRINGS and not(key=='slot' and k=='weapon'):string(v,key in NULLABLE and k in ('transfer','soldier','craft'))
  else:integer(v)
 if k=='craft':
  need(r['location']in ('home_roster','in_transit'))
  if r['status']!='STR_REPAIRS' or r['damage']<=0:need(r['repair_hours'] is None)
  if r['status']!='STR_REFUELLING' or r['fuel_max']<=r['fuel']:need(r['refuel_hours'] is None)
  if r['home_base_id'] is None:need(r['fuel_to_home'] is None and r['distance_to_home'] is None)
  if r['distance_to_home'] is not None:need(r['distance_to_home']>=0)
 if k=='soldier':
  need(r['location'] in ('home','in_transit','memorial'))
  need((r['base_id']=='') == (r['location']=='memorial'))
  need((r['death'] is not None) == (r['location']=='memorial'))
  if r['location']=='memorial':need(r['craft_id'] is None and r['transfer_id'] is None)
 if k=='mission_history':need(r['daylight'] in ('day','night'))
 if k=='transfer':need(r['transfer_type']in ('item','craft','soldier','scientist','engineer'))
 return r

def arguments(args):
 need(type(args)is dict and not(set(args)-{'snapshot','offset','limit'}))
 snapshot=args.get('snapshot');offset=args.get('offset',0);limit=args.get('limit',16)
 from agent_control import token
 if snapshot is not None:token(snapshot)
 need(type(offset)is int and 0<=offset<=LIMIT and type(limit)is int and 1<=limit<=16)
 need(snapshot is not None or offset==0)
 return snapshot,offset,limit

def page(raw,challenge,epoch,snapshot,offset,limit):
 from agent_control import parse,token,MAX_COUNTER
 o=parse(raw);exact(o,'schema type epoch challenge campaign_epoch status reason snapshot generation offset next_offset total complete records')
 need(o['schema']==SCHEMA and o['type']=='player_bases' and o['epoch']==epoch and o['challenge']==challenge)
 token(o['campaign_epoch']);integer(o['generation']);need(0<=o['generation']<=MAX_COUNTER)
 need(type(o['complete'])is bool and type(o['records'])is list and len(o['records'])<=limit)
 need(type(o['offset'])is int and o['offset']==offset)
 need(type(o['next_offset'])is int and 0<=o['next_offset']<=LIMIT)
 if o['status']=='unavailable':
  string(o['reason']);need(o['reason'] in ('no_campaign','wrong_thread','invalid_base_data','record_limit','snapshot_expired','invalid_offset') and o['snapshot']is None and o['total']is None and not o['complete'] and o['records']==[] and o['next_offset']==offset)
  return o
 need(o['status']=='ok' and o['reason']is None);token(o['snapshot'])
 need(snapshot is None or snapshot==o['snapshot'])
 need(type(o['total'])is int and 0<=o['total']<=LIMIT and o['generation']>0)
 need(o['next_offset']==offset+len(o['records']) and offset<=o['next_offset']<=o['total'])
 need(o['complete']==(o['next_offset']==o['total']) and (o['complete'] or bool(o['records'])))
 for r in o['records']:record(r)
 need(len({r['id'] for r in o['records']})==len(o['records']))
 return o

def definition():
 return {'name':'read_player_bases','description':'Read all player-owned bases and their facilities, personnel, own-soldier service aggregates, commendations and memorial, craft and safe own destinations, owned waypoints, stock, transfers, discovered research and currently offered research/manufacturing, completed mission and own-soldier history, and player-known owned-item definitions independently of the open UI, focus or paint. User-authorized strategic ownership data only: no tactical/enemy state. This grants no mutation or construction authority. First call omits snapshot with offset0; follow returned snapshot and next_offset until complete. Pages belong to a frozen content generation. New first reads sample current data; unchanged data retain generation. Unavailable/expired snapshots are explicit, never empty-success or silent truncation. Mutations still require ordinary fresh UI controls.',
 'inputSchema':{'type':'object','properties':{'snapshot':{'type':'string','pattern':'^[0-9a-f]{32}$'},'offset':{'type':'integer','minimum':0,'maximum':LIMIT},'limit':{'type':'integer','minimum':1,'maximum':16}},'additionalProperties':False},
 'annotations':{'readOnlyHint':True,'destructiveHint':False,'idempotentHint':True,'openWorldHint':False}}
