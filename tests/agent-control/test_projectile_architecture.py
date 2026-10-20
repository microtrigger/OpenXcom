"""Source boundary tests, not FOV/game/runtime acceptance."""
from pathlib import Path
import unittest,re
R=Path(__file__).resolve().parents[2]
def read(p):return (R/p).read_text()
MAP=read('src/Battlescape/Map.cpp');SURFACE=read('src/Engine/Surface.cpp');SCREEN=read('src/Engine/Screen.cpp');CONTROL=read('src/Observer/AgentController.cpp');UI=read('src/Observer/AgentUi.cpp');EVENTS=read('src/Observer/AgentPublicEvents.h')
class ProjectileArchitecture(unittest.TestCase):
 def test_single_body_tag(self):
  self.assertEqual(MAP.count('_agentProjectileRaster.body(true)'),1)
  start=MAP.index('// draw bullet itself');end=MAP.index('_agentProjectileRaster.body(false)',start)
  part=MAP[start:end];self.assertIn('isVoxelVisible(voxelPos)',part);self.assertIn('blitNShade(surface, bulletPositionScreen.x, bulletPositionScreen.y, 0)',part)
 def test_normal_visibility_order(self):
  self.assertLess(MAP.index('isVoxelVisible(voxelPos)',MAP.index('// draw bullet itself')),MAP.index('_agentProjectileRaster.body(true)'))
 def test_no_new_world_lookup(self):
  part=MAP.split('Observer::AgentProjectileFact Map::agentProjectileFact',1)[1]
  for x in ['_save','_projectile','getUnits','getTile','getPosition','RNG']:self.assertNotIn(x,part)
 def test_map_complete_and_cached(self):
  self.assertLess(MAP.index('if (!_redraw)'),MAP.index('_agentProjectileRaster.begin'))
  self.assertIn('seal(Observer::agentPaintHealthy() && !hiddenMovement)',MAP)
  self.assertIn('_agentDrawViewRevision != _camera->agentViewRevision()',MAP)
 def test_hidden_unknown(self):self.assertIn('!agentMapSnapshot(view) || view.hiddenMovement',MAP)
 def test_actual_shader_write(self):
  for part in [SURFACE.split('struct ColorReplace',1)[1].split('};',1)[0],SURFACE.split('struct StandardShade',1)[1].split('};',1)[0]]:
   self.assertIn('if (src)',part);self.assertIn('agentShaderRaster->wrote(&dest, agentBefore, dest)',part)
 def test_both_shaders_scoped(self):self.assertEqual(SURFACE.count('AgentShaderRasterScope agentScope(surface->_agentProjectileRaster)'),2)
 def test_flash_direct_writes(self):
  self.assertIn('surface->setPixelIterative(&x, &y, pixel)',MAP)
  self.assertIn('_agentProjectileRaster->wrote(getRaw(x, y), *getRaw(x, y), pixel)',read('src/Engine/Surface.h'))
 def test_unknown_bulk_writes(self):
  for name in ['clear','drawRect','drawLine','drawCircle','drawPolygon','drawTexturedPolygon','drawString','resize','rawCopy','loadImage']:
   part=SURFACE.split('void Surface::'+name,1)[1].split('\n}',1)[0];self.assertIn('_agentProjectileRaster->unknownWrite()',part)
 def test_final_flip_before_publish(self):
  game=read('src/Engine/Game.cpp');at=game.index('_agentController.presented()');self.assertLess(game.rfind('_screen->flip();',0,at),at)
  self.assertIn('agentPresentationReady()',SCREEN.split('Screen::agentProjectileFact',1)[1])
 def test_top_overlay_admission(self):
  s=read('src/Battlescape/BattlescapeState.cpp');part=s.split('std::string agentTacticalJson',1)[1].split('bool BattlescapeState::agentUiState',1)[0]
  self.assertIn('agentRegionsUncovered',part);self.assertIn('agentOverlayClear',part);self.assertIn('agentMapModeAdmitted',part)
 def test_output_is_targeted(self):
  s=read('src/Observer/AgentProjectileOutput.cpp');self.assertIn('AgentProjectileOutput::matches(int x, int y',s)
  for x in ['Projectile.h','BattleUnit','TileEngine','SurfaceSet','getUnits','OCR']:self.assertNotIn(x,s)
 def test_event_not_authority(self):
  self.assertNotIn('projectileFact',UI);self.assertNotIn('projectile',read('src/Observer/AgentProtocol.cpp').split('bool agentActionId',1)[1])
 def test_commit_nonreentrant(self):
  body=CONTROL.split('void AgentController::presented()',1)[1];self.assertLess(body.index('_dispatchInProgress'),body.index('_publicEvents.projectile'))
  self.assertLess(body.index('_publicEvents.projectile'),body.index('dispatchQueued(ui)'))
 def test_poll_independent(self):
  self.assertNotIn('_publicEvents.projectile',CONTROL.split('void AgentController::presented()',1)[0])
  self.assertIn('AgentProjectileEpisodes _projectiles',EVENTS)
 def test_bounded_default_off(self):
  s=read('src/CMakeLists.txt');part=s.split('if ( EXPERIMENTAL_AGENT_CONTROL )',1)[1].split('endif()',1)[0]
  self.assertIn('AgentProjectileWitness.cpp',part);self.assertIn('AgentProjectileOutput.cpp',part)
 def test_exact_version(self):self.assertIn('openxcom-agent-control/10',read('src/Observer/AgentProtocol.cpp'));self.assertNotIn('openxcom-agent-control/9',read('src/Observer/AgentProtocol.cpp'))
if __name__=='__main__':unittest.main()
