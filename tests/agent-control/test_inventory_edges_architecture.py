"""Narrow /13c source boundary checks, not game/runtime acceptance."""
from pathlib import Path
import unittest, json, hashlib
from test_inventory_routing_architecture import function
S=Path(__file__).resolve().parents[2];R=S.parent;B=R.parent/'agent-control-grenade-v13-20261019/source'
read=lambda p:p.read_text()
class Edges(unittest.TestCase):
 def test_inventory_only_projection_changes(self):
  old=read(B/'src/Battlescape/Inventory.cpp');new=read(S/'src/Battlescape/Inventory.cpp')
  new=new.replace('x < getWidth() && y < getHeight()', 'x + RuleInventory::SLOT_W < getWidth() && y + RuleInventory::SLOT_H < getHeight()')
  new=new.replace('_agentTargets.size() >= 96','_agentTargets.size() >= 64')
  new=new.replace(function(new,'Inventory::agentGridVisible'),function(old,'Inventory::agentGridVisible'))
  self.assertEqual(new,old)
 def test_provider_only_public_cap_changes(self):
  self.assertEqual(read(S/'src/Battlescape/InventoryState.cpp').replace('out.points.size() == 96','out.points.size() == 64'),read(B/'src/Battlescape/InventoryState.cpp'))
 def test_protocol_only_version_and_inventory_bound_changes(self):
  self.assertEqual(read(S/'src/Observer/AgentProtocol.cpp').replace('openxcom-agent-control/14','openxcom-agent-control/13').replace('decimal(parts[6].substr(1), 95, target)','decimal(parts[6].substr(1), 63, target)'),read(B/'src/Observer/AgentProtocol.cpp'))
 def test_other_production_and_fixture_unchanged(self):
  changed={'src/Battlescape/Inventory.cpp','src/Battlescape/InventoryState.cpp','src/Observer/AgentProtocol.cpp'}
  baseline=json.loads((R/'SOURCE-FROM-V13-SHA256.json').read_text())
  for p,h in baseline.items():
   if (p.startswith('src/') or p.startswith('tests/agent-report-fixture/') or p=='CMakeLists.txt') and p not in changed:
    self.assertEqual(hashlib.sha256((S/p).read_bytes()).hexdigest(),h,p)
 def test_frozen_baseline_unchanged(self):
  for p,h in json.loads((R/'SOURCE-FROM-V13-SHA256.json').read_text()).items():
   self.assertEqual(hashlib.sha256((B/p).read_bytes()).hexdigest(),h,p)
 def test_visible_clip_proof_and_center_bounds(self):
  body=function(read(S/'src/Battlescape/Inventory.cpp'),'Inventory::agentGridVisible')
  for term in ['r.x > 320 - r.width','r.y > 200 - r.height','std::min(right, 319)','std::min(bottom, 199)','bottom == 200','right == 320','_agentShownGridInk']:
   self.assertIn(term,body)
  for term in ['getItem','_selItem','_selUnit','getInventory','getTile','getRules','_groundOffset']:
   self.assertNotIn(term,body)
 def test_stock_candidate_count(self):
  import yaml
  for master in ['xcom1','xcom2']:
   data=yaml.safe_load(read(S/'bin/standard'/master/'inventories.rul'))['invs']
   body=sum(len(x.get('slots',[])) for x in data if x['id'] not in ['STR_GROUND','STR_LEFT_HAND','STR_RIGHT_HAND'])
   ground=next(x for x in data if x['id']=='STR_GROUND');self.assertEqual((ground['x'],ground['y']),(0,152))
   self.assertEqual(body,23);self.assertEqual(body+2+20*3,85);self.assertLessEqual(85,96)
 def test_handlers_and_no_hidden_page_preflight(self):
  for name in ['movePointer','activateButton','mouseClick','arrangeGround','unload','returnHeldItem']:
   self.assertEqual(function(read(S/'src/Battlescape/Inventory.cpp'),'Inventory::'+name),function(read(B/'src/Battlescape/Inventory.cpp'),'Inventory::'+name))
  body=function(read(S/'src/Battlescape/InventoryState.cpp'),'InventoryState::agentUiState')
  self.assertIn('agentGridVisible',body);self.assertIn('agentOverlayClear',body);self.assertIn('agentProjectionReady',body)
  self.assertNotIn('_groundOffset',body);self.assertNotIn('getInventory',body)
if __name__=='__main__':unittest.main(verbosity=2)
