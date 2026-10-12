# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-bound controls. A pass is not native or live SDK acceptance evidence."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[2]
def source(relative):return (ROOT/relative).read_text()

class SourceContract(unittest.TestCase):
    def test_explicit_default_off_independent_activation(self):
        cmake=source('CMakeLists.txt');controller=source('src/Observer/StanceController.cpp')
        for flag in ('EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK','EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE'):
            self.assertRegex(cmake,r'option\s*\(\s*'+flag+r'\s+"[^"]*"\s+OFF\s*\)')
        self.assertIn('_inventoryRightClickEnabled = _inventoryCloseEnabled && inventoryRightClick && std::string(inventoryRightClick) == "1";',controller)
        block=re.search(r'if\s*\(\s*EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK\s+AND(.*?)endif',cmake,re.S).group(1)
        self.assertIn('EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE',block)
        self.assertNotIn('EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK',block)
        self.assertNotIn('EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND',block)
    def test_one_ordinary_right_path_without_prediction_or_postpop_access(self):
        s=source('src/Battlescape/InventoryState.cpp')
        body=s.split('void InventoryState::inventoryControlRightClick(int x, int y)',1)[1].split('\n}\n',1)[0]
        for part in ('event.button.button = SDL_BUTTON_RIGHT','Action rightAction(&event, 1.0, 1.0, 0, 0);','rightAction.setSender(_inv); rightAction.setMouseAction(x, y, _inv->getX(), _inv->getY());'):
            self.assertIn(part,body)
        self.assertEqual(body.count('_inv->mouseOver(&rightAction, this);'),1)
        self.assertEqual(body.count('_inv->mouseClick(&rightAction, this);'),1)
        self.assertLess(body.index('_inv->mouseOver'),body.index('_inv->mouseClick'))
        self.assertNotIn('_inv',body.split('_inv->mouseClick(&rightAction, this);')[1])
        for forbidden in ('getSelectedItem(', 'setSelectedItem(', 'btnOkClick(', 'arrangeGround(', 'spendTimeUnits(', 'moveItem('):self.assertNotIn(forbidden,body)
    def test_unresolved_obligation_precedes_handler_and_witnessed_resolution(self):
        s=source('src/Observer/StanceController.cpp')
        body=s.split("if (_action == 'x')",1)[1].split('#endif',1)[0]
        before,after=body.split('state->inventoryControlRightClick(_targetX, _targetY);')
        self.assertIn('_awaitingInventoryView = true; _inventoryDestination = 0;',before)
        self.assertIn('_inventoryRightDispatching = true;',before)
        self.assertIn('if (_inventoryRightUnexpected) return;',after)
        self.assertIn('if (!sameSave) return;',after)
        self.assertNotIn('state->',after)
        self.assertIn('_inventoryDestination = state;',after)
        self.assertIn('_awaitingView = _awaitingCloseView = _closeRestoring = true;',after)
        self.assertIn('if (sendPacket(packet) && _awaitingInventoryView && _frameInventory == _inventoryDestination)',s)
        self.assertIn('if (state == _inventoryDestination) _inventoryDestination = 0;',s)
    def test_wire_is_strict_and_kind_separated(self):
        s=source('src/Observer/StanceController.cpp')
        self.assertIn("_inventoryRightClickEnabled && n == 103 && request[0] == 'X' &&\n        request.substr(97).find_first_not_of(\"0123456789\") == std::string::npos;",s)
        self.assertIn("(_contextKind == 'U' && inventoryRightClick)",s)
        self.assertIn('(inventoryRightClick && !((x >= 0 && x < 32 && y >= 63 && y < 111) ||\n            (x >= 128 && x < 160 && y >= 63 && y < 111) || (x >= 0 && x < 320 && y >= 152 && y < 200)))',s)
        domain=lambda x,y:(0<=x<32 and 63<=y<111) or (128<=x<160 and 63<=y<111) or (0<=x<320 and 152<=y<200)
        self.assertEqual(sum(domain(x,y) for x in range(320) for y in range(200)),18432)
        for point in ((0,63),(31,110),(128,63),(159,110),(0,152),(319,199)):self.assertTrue(domain(*point))
        for point in ((32,80),(127,80),(160,80),(0,62),(0,111),(0,151),(0,200),(320,160)):self.assertFalse(domain(*point))
    def test_release_ownership_preserves_later_ordinary_pairs(self):
        s=source('src/Observer/StanceController.cpp')
        self.assertIn('if (inventory && !pointerOwned && event.type == SDL_MOUSEBUTTONDOWN &&',s)
        self.assertIn('if (!rightOwned && event.type == SDL_MOUSEBUTTONDOWN',s)
        self.assertIn('if (!rightOwned && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_y)',s)
        self.assertIn('const bool owned = inventory->inventoryCloseOverlayOwned();',s)
        self.assertIn('_inventoryConsumedButtons |= button;',s)
        self.assertIn('_inventoryOwnedYRelease = true;',s)
        self.assertIn('if (_stanceController.beforeEvent(_event, _states.back())) continue;',source('src/Engine/Game.cpp'))
    def test_gate_uses_full_physical_semantic_natural_oracles(self):
        s=source('tests/inventory-right-click-control/InventoryRightClickControlGate.cpp')
        for part in ('SDL_GetVideoSurface()', 's.save=b->save()', 's.rng=RNG::getSeed()', 'getAmmoQuantity()', 'getFuseTimer()', 'InventoryRightClickGateAccess::stacks(inventory)', 'InventoryRightClickGateAccess::text(inventory)', 'before=snapshot();', 'manualPhasePixels.at(p).count(lastPixels)==1', 'InventoryRightClickGateAccess::unresolved(owner)', 'owner->pushState(new State); owner->popState();'):
            self.assertIn(part,s)
        for forbidden in ('SDL_WarpMouse(', '->_animTimer', '->_fade =', 'drawPrimers();','random_state', 'setstate('):self.assertNotIn(forbidden,s)
        self.assertIn('(manual()?manualPhasePixels.size():commandPhases.size())==8&&manualTicks>=8',s)
        self.assertIn('if((!heldCase()||p>0)&&p!=manualLastPhase)',s)
    def test_primed_and_refused_fixtures_wait_for_natural_warning_retirement(self):
        s=source('tests/inventory-right-click-control/InventoryRightClickControlGate.cpp')
        prepared=s.split('else if(stage==WAIT_PREPARED&&inputs.empty()) {',1)[1].split('} else if(stage==SEND_COMMAND)',1)[0]
        wait='if((fixture()==7||fixture()==8)&&InventoryRightClickGateAccess::warning(inventory)->getVisible()) return;'
        self.assertIn(wait,prepared)
        self.assertLess(prepared.index(wait),prepared.index("remember(); request('I');"))
        self.assertNotIn('setVisible(',prepared)
        self.assertNotIn('SDL_Delay(',prepared)
    def test_dispatch_hover_does_not_reenter_from_restoration_motion(self):
        s=source('tests/inventory-right-click-control/InventoryRightClickControlGate.cpp')
        body=s.split('void inventoryMouseOverEntered(',1)[1].split('void inventoryClickEntered(',1)[0]
        guard='if(!action||action->getDetails()->type!=SDL_MOUSEBUTTONUP||action->getDetails()->button.button!=SDL_BUTTON_RIGHT) return;'
        prehandler='expect(PREHANDLER,InventoryRightClickGateAccess::unresolved(owner));'
        self.assertIn(guard,body)
        self.assertEqual(body.count(prehandler),1)
        self.assertLess(body.index(guard),body.index(prehandler))
        self.assertLess(body.index(prehandler),body.index('owner->pushState(new State); owner->popState();'))
        click=s.split('void inventoryClickEntered(',1)[1].split('void inventoryInvClickEntered(',1)[0]
        self.assertIn(prehandler,click)
    def test_initial_transients_retained_and_both_paths_wait_collection_entry_phase_advance(self):
        s=source('tests/inventory-right-click-control/InventoryRightClickControlGate.cpp')
        after=s.split('void afterView()',1)[1].split('void beginTerminal()',1)[0]
        self.assertIn('retainFrame(lastPixels,lastViewPhase,manual()?"manual-post-view":"command-post-view");',after)
        self.assertIn('manualTicks=0; stage=COLLECT_ENTRY;',after)
        self.assertNotIn('manualLastPhase=',after)
        entry=s.split('if(stage==COLLECT_ENTRY) {',1)[1].split('if(stage==COLLECT) {',1)[0]
        self.assertIn('dispatched(true); lastPixels=display(); lastViewPhase=phase();',entry)
        retain='retainFrame(lastPixels,lastViewPhase,manual()?"manual-collection-entry":"command-collection-entry");'
        seed='manualLastPhase=lastViewPhase; stage=COLLECT; return;'
        self.assertIn(retain,entry)
        self.assertIn(seed,entry)
        self.assertLess(entry.index(retain),entry.index(seed))
        self.assertIn('void afterFrame(Game *game)',s[:s.index('if(stage==COLLECT_ENTRY) {')])
        collect=s.split('if(stage==COLLECT) {',1)[1].split('if(stage==WAIT_SETTLE',1)[0]
        self.assertIn('if((!heldCase()||p>0)&&p!=manualLastPhase)',collect)
        self.assertIn('manualPhasePixels.at(p).count(lastPixels)==1',collect)
        self.assertIn('(manual()?manualPhasePixels.size():commandPhases.size())==8&&manualTicks>=8',collect)
        for forbidden in ('SDL_Delay(', 'drawPrimers(', 'setVisible(', '->_animTimer'):
            self.assertNotIn(forbidden,after+entry+collect)
    def test_old_gate_line_endpoints_preserved_and_isolated_regression(self):
        s=source('tests/inventory-click-control/InventoryClickControlGate.cpp')
        self.assertEqual(s.splitlines()[484],'    if(!running||pendingSetup) return; if(profile=="orphan-left-release"&&orphanAdvance(game)) return;')
        self.assertIn('check(clicks==baselineClicks+1&&callbacks==baselineCallbacks+1,9005);',s)
        self.assertIn('The owned mouse UP is deliberately missing.',s)

if __name__=='__main__':unittest.main()
