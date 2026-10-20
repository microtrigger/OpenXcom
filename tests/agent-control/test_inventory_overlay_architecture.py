"""Narrow /13d source boundary checks. No runtime/game acceptance."""
from pathlib import Path
import unittest,json,hashlib
from test_inventory_routing_architecture import function
S=Path(__file__).resolve().parents[2];R=S.parent;B=R.parent/'agent-control-grenade-edges-v13c-20261019/source'
read=lambda p:p.read_text()
class Overlay(unittest.TestCase):
 def test_provider_only_region_gate(self):
  old=read(B/'src/Battlescape/InventoryState.cpp');new=read(S/'src/Battlescape/InventoryState.cpp')
  addition="""        // FPS and cursor paint after this state. Omit only their covered regions;
        // the controller still checks the complete retained projection again.
        AgentUiState candidate; candidate.regions.push_back(r);
        if (!_game->agentOverlayClear(candidate)) return false;
"""
  self.assertEqual(new.replace(addition,''),old)
 def test_global_overlay_guard_identical(self):
  self.assertEqual(function(read(S/'src/Engine/Game.cpp'),'Game::agentOverlayClear'),function(read(B/'src/Engine/Game.cpp'),'Game::agentOverlayClear'))
 def test_controller_only_same_predicates_and_private_diagnostic(self):
  old=read(B/'src/Observer/AgentController.cpp');new=read(S/'src/Observer/AgentController.cpp')
  self.assertEqual(new.replace(function(new,'AgentController::sample'),function(old,'AgentController::sample')),old)
  sample=function(new,'AgentController::sample')
  for term in ['top->agentUiState(out)','agentRegionsDisjoint(out)','_game->agentOverlayClear(out)','out = AgentUiState();','!provider || !disjoint || !overlayClear']:
   self.assertIn(term,sample)
 def test_diagnostic_closed_render_only_fields(self):
  body=function(read(S/'src/Engine/Game.cpp'),'Game::agentRecordAdmissionFailure')
  for term in ['OPENXCOM_AGENT_ADMISSION_DIAGNOSTICS','records >= 32','record == previous','Log(LOG_INFO)','AGENT_ADMISSION_DIAGNOSTIC','catch (...)','getVisible()','getX()','getY()','getWidth()','getHeight()']:
   self.assertIn(term,body)
  for term in ['getSavedGame','getUnit','getItem','getTile','_states','getText','getName','_binding','_epoch','_capability','sendPacket','invalidate','_publicEvents']:
   self.assertNotIn(term,body)
 def test_other_native_and_fixture_unchanged(self):
  changed={'src/Battlescape/InventoryState.cpp','src/Engine/Game.cpp','src/Engine/Game.h','src/Observer/AgentController.cpp'}
  for p,h in json.loads((R/'SOURCE-FROM-V13C-SHA256.json').read_text()).items():
   if (p.startswith('src/') or p.startswith('tests/agent-report-fixture/') or p=='CMakeLists.txt') and p not in changed:
    self.assertEqual(hashlib.sha256((S/p).read_bytes()).hexdigest(),h,p)
 def test_frozen_baseline_unchanged(self):
  for p,h in json.loads((R/'SOURCE-FROM-V13C-SHA256.json').read_text()).items():
   self.assertEqual(hashlib.sha256((B/p).read_bytes()).hexdigest(),h,p)
 def test_wire_and_handlers_unchanged(self):
  for p in ['src/Observer/AgentProtocol.cpp','src/Battlescape/Inventory.cpp']:
   self.assertEqual(read(S/p),read(B/p))
  for name in ['agentBindingCurrent','agentDispatch','btnOkClick','btnUnloadClick','btnGroundClick']:
   self.assertEqual(function(read(S/'src/Battlescape/InventoryState.cpp'),'InventoryState::'+name),function(read(B/'src/Battlescape/InventoryState.cpp'),'InventoryState::'+name))
 def test_full_coverage_still_refused(self):
  body=function(read(S/'src/Battlescape/InventoryState.cpp'),'InventoryState::agentUiState')
  self.assertIn('return !out.regions.empty()',body)
  self.assertIn('agentRegionsDisjoint(out)',body)
  self.assertIn('!agentInventoryInside(r)',body)
 def test_fixture_and_python_byte_exact(self):
  for p,h in json.loads((R/'SOURCE-FROM-V13C-SHA256.json').read_text()).items():
   if p.startswith('tests/agent-report-fixture/') or p.startswith('tools/mcp-observer/'):
    self.assertEqual(hashlib.sha256((S/p).read_bytes()).hexdigest(),h,p)
if __name__=='__main__':unittest.main(verbosity=2)
