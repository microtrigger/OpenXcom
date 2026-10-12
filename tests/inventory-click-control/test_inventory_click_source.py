# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-bound contract checks; these are not native runtime evidence."""
from pathlib import Path
import re
import unittest
ROOT = Path(__file__).resolve().parents[2]

def source(relative):
    return (ROOT / relative).read_text()

class SourceContract(unittest.TestCase):
    def test_default_off_build_and_literal_runtime(self):
        cmake = source('CMakeLists.txt')
        self.assertRegex(cmake, r'option\s*\(\s*EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK\s+"[^"]*"\s+OFF\s*\)')
        self.assertRegex(cmake, r'option\s*\(\s*EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE\s+"[^"]*"\s+OFF\s*\)')
        self.assertIn('_inventoryClickEnabled = _inventoryCloseEnabled && inventoryClick && std::string(inventoryClick) == "1";', source('src/Observer/StanceController.cpp'))

    def test_dispatch_is_one_hover_then_one_ordinary_callback(self):
        text = source('src/Battlescape/InventoryState.cpp')
        body = text.split('void InventoryState::inventoryControlClick(int x, int y)',1)[1].split('\n}\n',1)[0]
        self.assertEqual(body.count('_inv->mouseOver(&action, this);'),1)
        self.assertEqual(body.count('_inv->mouseClick(&action, this);'),1)
        self.assertLess(body.index('_inv->mouseOver'), body.index('_inv->mouseClick'))
        for expected in ['event.button.button = SDL_BUTTON_LEFT','Action action(&event, 1.0, 1.0, 0, 0);','action.setSender(_inv); action.setMouseAction(x, y, _inv->getX(), _inv->getY());']:
            self.assertIn(expected, body)
        for forbidden in ['moveItem(', 'setSelectedItem(', 'fitItem(', 'spendTimeUnits(', 'arrangeGround(', 'SDL_WarpMouse', 'draw(', 'blit(']:
            self.assertNotIn(forbidden, body)
        self.assertIn('InteractiveSurface::mouseClick(action, state);',source('src/Battlescape/Inventory.cpp'))

    def test_static_domain_has_exact_rectangles_and_strict_decimal(self):
        text=source('src/Observer/StanceController.cpp')
        self.assertIn('(inventoryClick && !((x >= 0 && x < 32 && y >= 63 && y < 111) ||\n            (x >= 128 && x < 160 && y >= 63 && y < 111) || (x >= 0 && x < 320 && y >= 152 && y < 200)))',text)
        self.assertIn("_inventoryClickEnabled && n == 103 && request[0] == 'E' &&\n        request.substr(97).find_first_not_of(\"0123456789\") == std::string::npos;",text)
        domain=lambda x,y:(0<=x<32 and 63<=y<111) or (128<=x<160 and 63<=y<111) or (0<=x<320 and 152<=y<200)
        self.assertEqual(sum(domain(x,y) for x in range(320) for y in range(200)),18432)
        for x,y in [(0,63),(31,110),(128,63),(159,110),(0,152),(319,199)]: self.assertTrue(domain(x,y))
        for x,y in [(-1,160),(320,160),(32,80),(127,80),(160,80),(0,62),(0,111),(0,151),(0,200)]: self.assertFalse(domain(x,y))

    def test_marker_source_and_dynamic_button_geometry(self):
        text=source('src/Observer/InventoryCloseOverlay.cpp')
        self.assertIn('_pointer->setX(controller->targetX() - 8); _pointer->setY(controller->targetY() - 8);',text)
        self.assertIn('if (squared >= 25 && squared <= 36)',text)
        self.assertIn('y >= _yes->getY() && y < _yes->getY() + _yes->getHeight()',text)
        self.assertIn('y >= _no->getY() && y < _no->getY() + _no->getHeight()',text)
        self.assertIn('if (_clickMode) _pointer->blit(surface);',text)
        self.assertIn('_yes->setY(bannerY + 1); _no->setY(bannerY + 17);',text)
        self.assertIn('return top < 144 && bottom > 112 ? 24 : 112;',source('src/Battlescape/Inventory.cpp'))
        for top in range(-48,201):
            banner=24 if top<144 and top+48>112 else 112
            self.assertTrue(top+48<=banner or top>=banner+32)

    def test_barrier_precedes_dispatch_and_successful_send_clears(self):
        text=source('src/Observer/StanceController.cpp')
        self.assertIn('_awaitingInventoryView = true; _inventoryDestination = state;\n        state->inventoryControlClick(_targetX, _targetY);',text)
        self.assertIn('if (sendPacket(packet) && _awaitingInventoryView && _frameInventory == _inventoryDestination)',text)
        self.assertIn("(_contextKind == 'H' && inventoryClick)",text)
        self.assertIn('_commands.size() >= 64',text)
        self.assertIn('heldRevision == o.heldRevision',text)
        self.assertIn('const bool owned = inventory->inventoryCloseOverlayOwned();',text)

    def test_gate_oracles_are_pre_request_and_natural(self):
        text=source('tests/inventory-click-control/InventoryClickControlGate.cpp')
        self.assertIn("before=snapshot(); beforePixels=lastPixels; baselineInits=inits;",text)
        self.assertIn('SDL_GetVideoSurface()',text)
        self.assertIn('s.save=b->save()',text)
        self.assertIn('s.rng=RNG::getSeed()',text)
        self.assertIn('getAmmoQuantity()',text)
        self.assertIn('verifyOverlayPixels()',text)
        self.assertIn('InventoryClickGateAccess::awaiting(owner)&&InventoryClickGateAccess::destination(owner)==inventory',text)
        for forbidden in ['SDL_WarpMouse(', '->_animTimer', '->_fade =', 'drawPrimers();', 'random_state', 'setstate(']: self.assertNotIn(forbidden,text)

    def test_initialized_zero_phase_never_completes_final_parity(self):
        text=source('tests/inventory-click-control/InventoryClickControlGate.cpp')
        self.assertIn('require(phase>0||std::string(kind)=="overlay"||std::string(kind).find("warning")!=std::string::npos);',text)
        self.assertIn('else if(phase>0&&phase!=manualLastPhase)',text)
        self.assertIn('(manual()?manualPhasePixels.size():commandPhases.size())==8&&manualTicks>=8',text)
        self.assertIn('setVisible(false); mouseEdge(160,80,true,SDL_BUTTON_RIGHT); mouseEdge(160,80,false,SDL_BUTTON_RIGHT);',text)
        self.assertIn('setX(241); mouseEdge(160,80,true,SDL_BUTTON_X2); mouseEdge(160,80,false,SDL_BUTTON_X2);',text)

    def test_ring_composition_includes_later_layers_and_exposed_pixels(self):
        text=source('tests/inventory-click-control/InventoryClickControlGate.cpp')
        self.assertIn('for(Surface *component:{frame,text,yes,no,cursor}) if(renderedColor(component,x,y,color)) covered=true;',text)
        self.assertIn('require(ring>0&&exposed>0);',text)

if __name__ == '__main__': unittest.main()
