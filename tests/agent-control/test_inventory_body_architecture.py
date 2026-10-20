"""Bounded /12 source and stock-layout checks; no runtime/game claim."""
from pathlib import Path
import hashlib,json,re,unittest,yaml
from test_inventory_routing_architecture import function,tokens
ROOT=Path(__file__).resolve().parents[2]
OLD=ROOT.parent.parent/'agent-control-inventory-v11b-20261018/source'
class BodyArchitecture(unittest.TestCase):
 def test_exact_both_stock_layout_counts(self):
  files=[ROOT/'bin/standard'/g/'inventories.rul' for g in ('xcom1','xcom2')]
  self.assertEqual(files[0].read_bytes(),files[1].read_bytes())
  for p in files:
   inv=yaml.safe_load(p.read_text())['invs'];self.assertEqual(sum(len(x['slots']) for x in inv if 'slots' in x),23)
   self.assertEqual(sum(x.get('type')==1 for x in inv),2)
   ground=sum(1 for x in range(0,321,16) for y in range(152,201,16) if x+16<320 and y+16<200)
   self.assertEqual(ground,38);self.assertEqual(ground+2+23,63)
 def test_normal_draw_and_handlers_unchanged(self):
  old=(OLD/'src/Battlescape/Inventory.cpp').read_text();new=(ROOT/'src/Battlescape/Inventory.cpp').read_text()
  for name in ('drawGrid','mouseOver','mouseClick','movePointer','activateButton','returnHeldItem','unload','arrangeGround','drawItems'):
   strip=lambda s:tokens(re.sub(r'#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL\n.*?#endif\n','',function(s,'Inventory::'+name),flags=re.S))
   self.assertEqual(strip(old),strip(new),name)
 def test_provider_and_action_authority_unchanged(self):
  for p in ['src/Battlescape/InventoryState.cpp','src/Battlescape/Inventory.h','src/Observer/AgentController.cpp','src/Observer/AgentInventoryPaint.h','src/Observer/AgentUi.cpp']:
   self.assertEqual((ROOT/p).read_bytes(),(OLD/p).read_bytes(),p)
 def test_grid_capture_without_inventory_or_eligibility_queries(self):
  s=(ROOT/'src/Battlescape/Inventory.cpp').read_text();grid=function(s,'Inventory::drawGrid')
  for token in ['getInventory()','getSelectedUnit','getAmmoItem','getCompatibleAmmo','spendTimeUnits','canBeStacked','fitItemInSlot','_groundOffset']:
   self.assertNotIn(token,grid)
  self.assertIn('agentInventoryBodyLayout',grid);self.assertIn('agentInventoryBodyIntersects',grid)
  self.assertIn('if (body) agentCaptureTarget',grid)
 def test_helper_only_stock_geometry(self):
  s=(ROOT/'src/Observer/AgentInventoryBody.h').read_text()
  for token in ['BattleItem','BattleUnit','getRules','getItem(','getAmmo','getUnits','RNG','SDL_','Game*','Game *']:
   self.assertNotIn(token,s)
  self.assertIn('slots.size() != agentInventoryBodyCount(id)',s)
  self.assertIn('slots[j].x == slots[i].x',s)
 def test_protocol_only_version_changed(self):
  p='src/Observer/AgentProtocol.cpp';self.assertEqual((ROOT/p).read_text(),(OLD/p).read_text().replace('openxcom-agent-control/11','openxcom-agent-control/12'))
 def test_target_cap_and_no_semantic_grenade_authority(self):
  s=(ROOT/'src/Battlescape/Inventory.cpp').read_text();self.assertIn('_agentTargets.size() >= 64',s)
  p=(ROOT/'src/Observer/AgentProtocol.cpp').read_text()
  for action in ['choose_weapon_action','prime_grenade','select_timer','throw_grenade']:
   self.assertNotIn('"'+action+'"',p)
 def test_original_native_inputs_preserved(self):
  seal=json.loads((ROOT.parent/'SOURCE-FROM-V11B-SHA256.json').read_text())
  self.assertTrue(all(hashlib.sha256((OLD/p).read_bytes()).hexdigest()==h for p,h in seal.items()))
if __name__=='__main__':unittest.main(verbosity=2)
