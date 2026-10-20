"""Read-only boundaries for the explicitly scoped minimap presentation provider."""
from pathlib import Path
import unittest
S=Path(__file__).resolve().parents[2]
class MiniMapArchitecture(unittest.TestCase):
 def setUp(self):
  self.view=(S/'src/Battlescape/MiniMapView.cpp').read_text();self.state=(S/'src/Battlescape/MiniMapState.cpp').read_text();self.core=(S/'src/Observer/AgentMiniMapPresentation.cpp').read_text();self.screen=(S/'src/Engine/Screen.cpp').read_text()
 def test_provider_no_world_traversal(self):
  provider=self.state[self.state.index('bool MiniMapState::agentUiState'):]
  for forbidden in ('getTile(', 'getUnits(', 'getInventory(', 'getPosition(', 'getFaction(', 'getViewLevel(', 'getCenterPosition('):self.assertNotIn(forbidden,provider)
 def test_normal_draw_visibility(self):
  self.assertIn('if (t->getUnit() && t->getUnit()->getVisible())',self.view)
  self.assertIn('if (t->isDiscovered(2) && !t->getInventory()->empty())',self.view)
  self.assertIn('int shade = 16;',self.view)
 def test_reticle_retirement(self):
  begin=self.view.index('_agentPresentation.retire(');end=self.view.index('_agentPresentation.seal(',begin)
  self.assertEqual(self.view[begin:end].count('drawLine('),4)
  self.assertIn('agentAttachProjectileRaster(nullptr)',self.view[begin:end])
  self.assertIn('agentAttachProjectileRaster(_agentPresentation.raster());',self.view[end:])
 def test_complete_primitive(self):
  self.assertIn('glyph->getWidth() == 4 && glyph->getHeight() == 4',self.view)
  self.assertIn('_witnesses[k].completeGlyph(',self.core)
 def test_same_shared_activation(self):
  self.assertIn('activatePoint(origX, origY);',self.view)
  self.assertIn('_miniMapView->agentActivatePoint(fresh.points[index].first',self.state)
 def test_final_and_overlay_gates(self):
  self.assertIn('agentCaptureOutput(_miniMapView, output)',self.state)
  self.assertIn('agentRegionsUncovered(part.regions, _miniMapView, _surfaces)',self.state)
  self.assertIn('_game->agentOverlayClear(part)',self.state)
  self.assertIn('return !out.regions.empty() && agentRegionsDisjoint(out)',self.state)
 def test_bounded_content(self):
  for guard in ('rectangles == 512','count == 128','out.points.size() == 160','bounded = rectangles || count'):
   self.assertIn(guard,self.state)
 def test_private_context(self):
  self.assertIn('out.context.push_back(_miniMapView->agentCameraRevision())',self.state)
  self.assertIn('out.widgets.push_back({_miniMapView, _miniMapView->agentPresentation().revision()})',self.state)
  self.assertIn('_agentDrawCameraRevision == _camera->agentViewRevision()',self.view)
 def test_successful_output_reuse(self):
  self.assertIn('if (!agentCaptureOutput(map, output)) return Observer::AgentProjectileFact::Unknown',self.screen)
  self.assertIn('output.capture(_agentLastFlipOk, useOpenGL() || use32bitScaler()',self.screen)
if __name__=='__main__':unittest.main()
