#!/usr/bin/env python3
"""Bounded source contracts; not execution of game states or the GUI."""
import unittest
from test_menu_architecture import read, body

class GlobeArchitectureTest(unittest.TestCase):
    def test_no_model_or_legality_in_projection(self):
        for path, method in [('Geoscape/Globe.cpp','Globe::agentGeography'),('Geoscape/BuildNewBaseState.cpp','BuildNewBaseState::agentUiState')]:
            code=body(path,method)
            for forbidden in ('insideLand(', '_cacheLand', 'getPolygon', 'getLongitude', 'getLatitude', 'getRegions', 'getSavedGame'):
                self.assertNotIn(forbidden,code)
    def test_shared_site_path_and_exact_membership(self):
        native=body('Geoscape/BuildNewBaseState.cpp','BuildNewBaseState::agentDispatch')
        ordinary=body('Geoscape/BuildNewBaseState.cpp','BuildNewBaseState::globeClick')
        self.assertIn('selectSite(',native); self.assertIn('selectSite(',ordinary)
        self.assertIn('r.x==request.x && r.y==request.y',native)
        self.assertIn('r.width==1 && r.height==1',native)
        self.assertNotIn('insideLand(',native)
        self.assertIn('insideLand(',body('Geoscape/BuildNewBaseState.cpp','BuildNewBaseState::selectSite'))
    def test_final_shader_mask_and_primitive_health(self):
        code=read('Geoscape/Globe.cpp').split('struct CreateShadow\n',1)[1].split('struct CreateShadowWithoutCache',1)[0]
        # Method is inline; ensure hook follows the final transparent assignment.
        self.assertLess(code.index('dest = 0;'),code.index('agentShadowSample(dest, dest && earth.z)'))
        surface=read('Engine/Surface.cpp')
        for primitive in ('texturedPolygon(', 'filledCircleColor(', 'SDL_LockSurface('):
            self.assertIn('OPENXCOM_AGENT_PAINT('+primitive,surface)
    def test_overlay_layers_and_presented_underlay(self):
        for method,layer in [('drawRadars',1),('drawDetail',2),('drawMarkers',4)]:
            self.assertIn('clearLayer('+str(layer)+')',body('Geoscape/Globe.cpp','Globe::'+method))
        self.assertIn('agentRecordOccluders(_surfaces, _dogfights.empty())',body('Geoscape/GeoscapeState.cpp','GeoscapeState::blit'))
        self.assertIn('agentGeography(ui,_surfaces)',body('Geoscape/BuildNewBaseState.cpp','BuildNewBaseState::agentUiState'))
    def test_idle_paint_does_not_advance_camera_generation(self):
        for method in ('draw','drawShadow','drawDetail','drawMarkers','blit'):
            self.assertNotIn('++_agentViewRevision',body('Geoscape/Globe.cpp','Globe::'+method))
        for method in ('center','rotate','setZoom','resize'):
            self.assertIn('++_agentViewRevision',body('Geoscape/Globe.cpp','Globe::'+method))
    def test_first_base_hidden_cancel_not_advertised(self):
        code=body('Geoscape/BuildNewBaseState.cpp','BuildNewBaseState::agentUiState')
        self.assertIn('_btnCancel->getVisible()',code)
        self.assertNotIn('cancel_menu',code)
    def test_expanded_bounded_transport(self):
        code=read('Observer/AgentController.cpp')
        self.assertIn('char bytes[512]',code); self.assertIn('MSG_TRUNC | MSG_CTRUNC',code)
        self.assertIn('packet.size() <= 49152',code)
    def test_geoscape_only_adds_bounded_visible_navigation(self):
        code=body('Geoscape/GeoscapeState.cpp','GeoscapeState::agentUiState')
        self.assertEqual(code.count('actions.push_back'), 4)
        for action in ('choose_geoscape_speed','open_bases','open_pause_menu'):
            self.assertIn(action,code)
        self.assertIn('AgentUiAction("open_funding", label, "visible_text")',code)
        self.assertNotIn('getSavedGame',code)

if __name__=='__main__': unittest.main(verbosity=2)
