"""Narrow source-path regression checks, not a game/FOV/runtime proof."""
from pathlib import Path
import unittest
R=Path(__file__).resolve().parents[2]
def read(p):return (R/p).read_text()
M=read('src/Battlescape/Map.cpp');S=read('src/Engine/Surface.cpp');U=read('src/Battlescape/UnitSprite.cpp');C=read('src/Observer/AgentController.cpp');E=read('src/Observer/AgentPublicEvents.h')
class ImpactArchitecture(unittest.TestCase):
 def test_impact_only_exact_ordinary_branch(self):
  self.assertEqual(M.count('_agentProjectileRaster.role(Observer::AgentRendererRole::Impact)'),1)
  part=M.split('// check if we got big explosions',1)[1].split('surface->unlock()',1)[0]
  self.assertLess(part.index('if (_explosionInFOV)'),part.index('AgentRendererRole::Impact'))
  self.assertLess(part.index('else if ((*i)->isHit())'),part.index('AgentRendererRole::Impact'))
  self.assertIn('getSurfaceSet("SMOKE.PCK")->getFrame',part.split('AgentRendererRole::Impact',1)[0])
 def test_no_simulation_event_hook(self):
  for f in ['src/Battlescape/ExplosionBState.cpp','src/Battlescape/UnitDieBState.cpp']:
   for token in ['AgentRendererRole','impactFact','collapseFact','agentMarkCollapsePaint']:self.assertNotIn(token,read(f))
 def test_marker_only_reviewed_collapse_draw(self):
  self.assertEqual(U.count('agentMarkCollapsePaint()'),1)
  part=U.split('if (_unit->getStatus() == STATUS_COLLAPSING)',1)[1].split('return;',1)[0]
  self.assertLess(part.index('drawRecolored(torso)'),part.index('agentMarkCollapsePaint'))
  self.assertIn('_drawingRoutine == 0 || _drawingRoutine == 10 || _drawingRoutine == 13 || _drawingRoutine == 14',part)
 def test_map_uses_drawn_cache_not_status_for_role(self):
  part=M.split('// draw unit\n',1)[1].split('// draw fire',1)[0]
  self.assertIn('tmpSurface->agentCollapsePaint()',part);self.assertNotIn('STATUS_COLLAPSING',part)
  self.assertIn('unitShade, mask)',part)
 def test_copy_follows_normal_blit(self):
  part=M.split('void Map::cacheUnit(',1)[1].split('void Map::setProjectile',1)[0]
  self.assertLess(part.index('unitSprite->blit(cache)'),part.index('cache->agentCopyCollapsePaint(*unitSprite)'))
 def test_copy_not_generic_propagation(self):
  part=S.split('Surface::Surface(const Surface& other)',1)[1].split('Surface::~Surface',1)[0]
  self.assertNotIn('other._agentCollapsePaint',part)
  part=S.split('void Surface::agentCopyCollapsePaint',1)[1]
  for token in ['agentPaintHealthy','source.agentCollapsePaint','source.getX() == 0','source.getWidth() == getWidth()','clip_rect']:self.assertIn(token,part)
 def test_marker_mutation_retirement(self):
  for name in ['clear','drawRect','drawLine','drawCircle','drawPolygon','drawTexturedPolygon','drawString','resize','rawCopy','loadImage']:
   self.assertIn('_agentCollapsePaint = false',S.split('void Surface::'+name,1)[1].split('\n}',1)[0])
  self.assertEqual(S.count('surface->_agentCollapsePaint = false'),3)
 def test_all_kinds_receive_actual_write(self):
  part=read('src/Observer/AgentProjectileWitness.cpp').split('void AgentProjectileRaster::wrote',1)[1]
  self.assertIn('_witnesses[i]->wrote(x, y, before, after, int(_role) == int(i))',part)
 def test_new_facts_not_authority(self):
  ui=read('src/Observer/AgentUi.cpp')
  self.assertNotIn('impactFact',ui);self.assertNotIn('collapseFact',ui)
 def test_successful_nonreentrant_commit(self):
  part=C.split('void AgentController::presented()',1)[1]
  for kind in ['impact','collapse']:
   self.assertLess(part.index('_dispatchInProgress'),part.index('_publicEvents.'+kind))
   self.assertLess(part.index('_publicEvents.'+kind),part.index('dispatchQueued(ui)'))
 def test_independent_reducers(self):
  self.assertIn('_impacts.observe(fact)',E);self.assertIn('_collapses.observe(fact)',E)
 def test_frozen_renderer_output_proof(self):
  old=R.parent.parent/'agent-control-projectile-v9-20261018/source'
  for f in ['src/Observer/AgentProjectileOutput.cpp','src/Observer/AgentProjectileOutput.h','src/Engine/Screen.cpp','src/Engine/Screen.h']:
   self.assertEqual((R/f).read_bytes(),(old/f).read_bytes())
if __name__=='__main__':unittest.main()
