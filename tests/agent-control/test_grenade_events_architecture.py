"""Exact source hook/parity tests. Not runtime FOV/game/flip acceptance."""
from pathlib import Path
import unittest,re,json,hashlib
from test_inventory_routing_architecture import function
S=Path(__file__).resolve().parents[2];R=S.parent;B=R.parent/'agent-control-inventory-overlay-v13d-20261019/source'
read=lambda p:p.read_text()
M=read(S/'src/Battlescape/Map.cpp');C=read(S/'src/Observer/AgentController.cpp');E=read(S/'src/Observer/AgentPublicEvents.h')
def no_agent(s):
 out=[];skipping=0
 for line in s.splitlines(True):
  if line.strip()=='#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL':skipping=1;continue
  if skipping:
   if re.match(r'\s*#if',line):skipping+=1
   elif re.match(r'\s*#endif',line):skipping-=1
  else:out.append(line)
 return ''.join(out)
class GrenadeEvents(unittest.TestCase):
 def test_ordinary_map_and_battlescape_byte_parity(self):
  for p in ['src/Battlescape/Map.cpp','src/Battlescape/Map.h','src/Battlescape/BattlescapeState.cpp']:
   self.assertEqual(no_agent(read(S/p)),no_agent(read(B/p)),p)
 def test_thrown_exact_body_only(self):
  self.assertEqual(M.count('_agentProjectileRaster.role(Observer::AgentRendererRole::ThrownItem)'),1)
  part=M.split('// draw thrown object',1)[1].split('// draw bullet on the correct tile',1)[0]
  self.assertLess(part.index('isVoxelVisible(voxelPos)'),part.index('AgentRendererRole::ThrownItem'))
  self.assertIn('blitNShade(surface, bulletPositionScreen.x - 16, bulletPositionScreen.y - 26, 0)',part)
  self.assertNotIn('AgentRendererRole::ThrownItem',M.split('// draw shadow on the floor',1)[1].split('// draw thrown object',1)[0])
 def test_big_exact_positive_x1_only(self):
  self.assertEqual(M.count('_agentProjectileRaster.role(Observer::AgentRendererRole::LargeExplosion)'),1)
  part=M.split('// check if we got big explosions',1)[1].split('surface->unlock()',1)[0]
  for token in ['if (_explosionInFOV)','if ((*i)->isBig())','if ((*i)->getCurrentFrame() >= 0)','getSurfaceSet("X1.PCK")']:
   self.assertLess(part.index(token),part.index('AgentRendererRole::LargeExplosion'))
  self.assertLess(part.index('AgentRendererRole::LargeExplosion'),part.index('else if ((*i)->isHit())'))
  self.assertNotIn('isVoxelVisible',part)
  self.assertNotIn('AgentRendererRole::LargeExplosion',part.split('if (_flashScreen)',1)[1].split('else',1)[0])
 def test_all5_begin_seal_invalidate(self):
  for name in ['Projectile','Impact','Collapse','ThrownItem','LargeExplosion']:
   self.assertIn('&_agent'+name+'Witness',M)
   self.assertIn('_agent'+name+'Witness.invalidate()',M)
   self.assertIn('_agent'+name+'Witness.seal(Observer::agentPaintHealthy() && !hiddenMovement)',M)
  raster=read(S/'src/Observer/AgentProjectileWitness.cpp');self.assertIn('AgentProjectileWitness *, 5',raster)
  self.assertIn('_witnesses[i]->wrote(x, y, before, after, int(_role) == int(i))',raster)
 def test_no_world_lookup_in_projection(self):
  part=M.split('Observer::AgentProjectileFact Map::agentProjectileFact',1)[1]
  for token in ['_save','_projectile','getUnits','getTile','getPosition','RNG']:self.assertNotIn(token,part)
 def test_same_final_screen_surface_proof(self):
  for p in ['src/Engine/Screen.cpp','src/Engine/Screen.h','src/Engine/Surface.cpp','src/Engine/Surface.h','src/Observer/AgentProjectileOutput.cpp','src/Observer/AgentProjectileOutput.h']:
   self.assertEqual(read(S/p),read(B/p),p)
 def test_private_facts_not_authority(self):
  u=read(S/'src/Observer/AgentUi.cpp');self.assertEqual(u,read(B/'src/Observer/AgentUi.cpp'))
  for token in ['thrownItemFact','largeExplosionFact']:self.assertNotIn(token,u)
 def test_independent_latches_and_nonreentrant_commit(self):
  for role in ['_thrownItems','_largeExplosions']:self.assertIn(role+'.observe(fact)',E)
  part=C.split('void AgentController::presented()',1)[1]
  for name in ['thrownItem','largeExplosion']:
   self.assertLess(part.index('_dispatchInProgress'),part.index('_publicEvents.'+name))
   self.assertLess(part.index('_publicEvents.'+name),part.index('dispatchQueued(ui)'))
   self.assertNotIn('_publicEvents.'+name,C.split('void AgentController::presented()',1)[0])
 def test_no_other_runtime_or_fixture_delta(self):
  changed={'src/Battlescape/Map.cpp','src/Battlescape/Map.h','src/Battlescape/BattlescapeState.cpp','src/Observer/AgentProjectileWitness.cpp','src/Observer/AgentProjectileWitness.h','src/Observer/AgentUi.h','src/Observer/AgentPublicEvents.h','src/Observer/AgentController.cpp','src/Observer/AgentProtocol.cpp'}
  for p,h in json.loads((R/'SOURCE-FROM-V13D-SHA256.json').read_text()).items():
   if (p.startswith('src/') or p.startswith('tests/agent-report-fixture/') or p=='CMakeLists.txt') and p not in changed:self.assertEqual(hashlib.sha256((S/p).read_bytes()).hexdigest(),h,p)
 def test_wire_only_version_changes(self):
  self.assertEqual(read(S/'src/Observer/AgentProtocol.cpp').replace('openxcom-agent-control/15','openxcom-agent-control/14'),read(B/'src/Observer/AgentProtocol.cpp'))
 def test_controller_only_two_commits(self):
  old=read(B/'src/Observer/AgentController.cpp');new=C.replace('!_publicEvents.collapse(ui.collapseFact) || !_publicEvents.thrownItem(ui.thrownItemFact) ||\n        !_publicEvents.largeExplosion(ui.largeExplosionFact))','!_publicEvents.collapse(ui.collapseFact))')
  self.assertEqual(new,old)
 def test_prior_source_frozen(self):
  for p,h in json.loads((R/'SOURCE-FROM-V13D-SHA256.json').read_text()).items():self.assertEqual(hashlib.sha256((B/p).read_bytes()).hexdigest(),h,p)
if __name__=='__main__':unittest.main(verbosity=2)
