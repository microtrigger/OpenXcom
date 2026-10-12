# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
"""Source-bound checks only. These do not launch a game or prove native acceptance."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[2]
def source(path): return (ROOT/path).read_text(encoding='utf-8')
def body(text,start,end='\n}\n'): return text.split(start,1)[1].split(end,1)[0]

class SourceContract(unittest.TestCase):
    def test_default_off_and_independent_close_prerequisite(self):
        cmake=source('CMakeLists.txt');controller=source('src/Observer/StanceController.cpp')
        for flag in ('EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD','EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE'):
            self.assertRegex(cmake,r'option\s*\(\s*'+flag+r'\s+"[^"]*"\s+OFF\s*\)')
        self.assertIn('_inventoryUnloadEnabled = _inventoryCloseEnabled && inventoryUnload && std::string(inventoryUnload) == "1";',controller)
        block=re.search(r'if\s*\(\s*EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD\s+AND(.*?)endif',cmake,re.S).group(1)
        self.assertIn('EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE',block)
        for other in ('CLICK','RIGHT_CLICK','GROUND'):
            self.assertNotIn('EXPERIMENTAL_SUPERVISED_INVENTORY_'+other,block)
    def test_fixed_full_state_route_has_no_outcome_shortcut(self):
        b=body(source('src/Battlescape/InventoryState.cpp'),'void InventoryState::inventoryControlUnloadEvent(Uint8 type)')
        for part in ('event.type = type','event.button.button = SDL_BUTTON_LEFT','event.button.x = 304; event.button.y = 44;','Action action(&event, 1.0, 1.0, 0, 0);','State::handle(&action);'):
            self.assertIn(part,b)
        self.assertEqual(b.count('State::handle(&action);'),1)
        for forbidden in ('_inv->','btnUnloadClick(', 'getAmmoItem(', 'getSelectedItem(', 'spendTimeUnits(', 'setSelectedItem(', '_inventoryCloseConsumeEvent ='):
            self.assertNotIn(forbidden,b)
        self.assertNotIn('this',b.split('State::handle(&action);')[1].replace('// The ordinary route may have changed the stack. Do not touch this again.',''))
    def test_pre_down_null_I_and_synchronous_continuity_before_up(self):
        c=source('src/Observer/StanceController.cpp');b=body(c,"if (_action == 'f')",'#endif')
        before,after=b.split('state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONDOWN);')
        self.assertIn('_awaitingInventoryView = true; _inventoryDestination = 0;',before)
        self.assertIn('_inventoryUnloadDispatching = true;',before)
        for guard in ('!_inventoryUnloadUnexpected','_game->isState(state)','_game->inventoryParentState(origin.parent)','_game->getSavedGame() == origin.savedGame','origin.savedGame->getSavedBattle() == origin.save'):
            self.assertIn(guard,after)
        self.assertIn('if (survives()) state->inventoryControlUnloadEvent(SDL_MOUSEBUTTONUP);',after)
        self.assertIn('if (survived) _inventoryDestination = state;',after)
        self.assertNotIn('_awaitingCloseView = true',b)
        self.assertNotIn('_awaitingInventoryView = false',b)
        for fn in ('beforePush(State *state)','beforePop(State *state)','beforeSaveChange()'):
            self.assertIn('if (_inventoryUnloadDispatching) _inventoryUnloadUnexpected = true;',body(c,'void StanceController::'+fn))
    def test_strict_wire_and_private_kind_binding(self):
        c=source('src/Observer/StanceController.cpp')
        self.assertIn("_inventoryUnloadEnabled && n == 97 && request[0] == 'F';",c)
        self.assertIn("(_contextKind == 'Q' && inventoryUnload)",c)
        self.assertIn('experimental-inventory-unload-context/1',c)
        self.assertIn("(_inventoryUnloadEnabled && request[0] == 'Q')",c)
        for part in ('heldRevision == o.heldRevision','selectionX == o.selectionX','selectionY == o.selectionY'):
            self.assertIn(part,c)
        self.assertIn('if (sendPacket(packet) && _awaitingInventoryView && _frameInventory == _inventoryDestination)',c)
        self.assertIn('if (state == _inventoryDestination) _inventoryDestination = 0;',c)
    def test_visible_geometry_not_hidden_eligibility(self):
        s=source('src/Battlescape/InventoryState.cpp');b=body(s,'bool InventoryState::inventoryUnloadContext(')
        for part in ('_btnUnload->turnContextVisible()','_btnUnload->stanceContextVisible()','_btnUnload->getX() != 288','_btnUnload->getY() != 32','_btnUnload->getWidth() != 32','_btnUnload->getHeight() != 25','_surfaces.rbegin()','inventorySeen && unloadSeen'):
            self.assertIn(part,b)
        v=body(source('src/Battlescape/Inventory.cpp'),'bool Inventory::inventoryUnloadPresentationAllowed() const')
        for forbidden in ('getAmmoQuantity','getAmmoItem','needsAmmo','spendTimeUnits','getTimeUnits','fitItem','_selItem->'):
            self.assertNotIn(forbidden,b+v)
        self.assertIn('getInventories()',v)
        self.assertIn('intersectsUnload',v)
    def test_normal_handler_precedence_and_success_sound_source(self):
        v=body(source('src/Battlescape/Inventory.cpp'),'bool Inventory::unload()')
        stages=['if (_selItem == 0)','STR_NO_AMMUNITION_LOADED','if (_selItem->getAmmoItem() == 0 || !_selItem->needsAmmo())','STR_BOTH_HANDS_MUST_BE_EMPTY','spendTimeUnits(8)','STR_LEFT_HAND','STR_RIGHT_HAND','_selItem->setAmmoItem(0);','setSelectedItem(0);']
        self.assertEqual([v.index(x) for x in stages],sorted(v.index(x) for x in stages))
        for hidden in ('getAmmoQuantity()', 'fitItem', 'getReserved'):
            self.assertNotIn(hidden,v)
        b=body(source('src/Battlescape/InventoryState.cpp'),'void InventoryState::btnUnloadClick(Action *action)')
        for part in ('if (_inv->unload())','_txtItem->setText("");','_txtAmmo->setText("");','_selAmmo->clear();','updateStats();','getSoundByDepth(0, Mod::ITEM_DROP)->play();'):
            self.assertIn(part,b)
    def test_release_guards_compile_for_unload_alone(self):
        c=source('src/Observer/StanceController.cpp')
        union='#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)'
        self.assertIn(union,c)
        for part in ("(_action == 'x' || _action == 'f')",'const bool owned = inventory->inventoryCloseOverlayOwned();','if (inventory && !pointerOwned && event.type == SDL_MOUSEBUTTONDOWN &&','if (!rightOwned && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_y)','_inventoryOwnedYRelease = true;','_inventoryConsumedButtons |= button;'):
            self.assertIn(part,c)
    def test_finite_physical_manual_path_and_independent_expected_outcomes(self):
        g=source('tests/inventory-unload-control/InventoryUnloadControlGate.cpp')
        for part in ('FixtureCount=24','if(manual()) { click(targetX,targetY,SDL_BUTTON_LEFT);','targetX=304; targetY=44;','SDL_GetVideoSurface()','s.save=b->save()','s.rng=RNG::getSeed()','getPreviousOwner()','getTurnFlag()','isAmmo()','initialGroundIds','initialUnitIds','player->getTimeUnits()==initialTU-(success?8:0)','fixtureAmmo->setAmmoQuantity(0)','STR_THERMAL_TAZER'):
            self.assertIn(part,g)
        for forbidden in ('inventoryControlUnloadEvent(', '->btnUnloadClick(', '->unload()', 'SDL_WarpMouse(', '->_animTimer', '->_fade =', 'drawPrimers();', 'random_state', 'setstate('):
            self.assertNotIn(forbidden,g)
    def test_exact_callback_order_and_actual_lookup_hook(self):
        g=source('tests/inventory-unload-control/InventoryUnloadControlGate.cpp')
        self.assertIn('{"down","hover-down","down-return","up","hover-up","click","invClick","button","up-return"}',g)
        self.assertIn('expect(DISPATCH,clicks==baselineClicks+1); // Before any duplicate handler',g)
        self.assertIn('expect(SOUND,depth==0&&sound==Mod::ITEM_DROP',g)
        self.assertIn('InventoryUnloadControlGate::soundLookup(depth, sound);',source('src/Mod/Mod.cpp'))
        for name in ('inventoryUnloadEventEntered','inventoryUnloadEventReturned'):
            self.assertIn('InventoryUnloadControlGate::'+name,source('src/Engine/State.cpp'))
        self.assertIn('realUpWithoutModal==0&&inventoryYDown==0&&inventoryYUp==0',g)
        returned=body(g,'void inventoryUnloadEventReturned(', 'void inventoryMouseOverEntered(')
        self.assertLess(returned.index('expect(TRANSITION,owner->isState(who))'),returned.index('InventoryUnloadGateAccess::pressed(who'))
        self.assertIn('mouseEdge(330,210,true); // Held input outside all ordinary surfaces.',g)
        self.assertIn('motion(330,210); // Establish normal mouse-out presentation before baseline.',g)
    def test_warning_visibility_and_actual_frames_before_natural_settled_phase(self):
        g=source('tests/inventory-unload-control/InventoryUnloadControlGate.cpp')
        self.assertIn('if(stage==WAIT_ACTION) expect(WARNING,InventoryUnloadGateAccess::warning(inventory)->getVisible()==!warning.empty());',g)
        self.assertIn('"manual-first-rendered-after-action":"command-first-rendered-after-action"',g)
        self.assertIn('"manual-post-view":"command-post-view"',g)
        self.assertIn('stage=WAIT_WARNING;',g)
        self.assertIn('if(stage==WAIT_WARNING&&inventory&&!InventoryUnloadGateAccess::warning(inventory)->getVisible()) stage=COLLECT_ENTRY;',g)
        self.assertIn('"manual-collection-entry":"command-collection-entry"',g)
        self.assertIn('manualLastPhase=lastViewPhase; stage=COLLECT; return;',g)
        self.assertIn('if(p>0&&p!=manualLastPhase)',g)
        self.assertIn('manualPhasePixels.at(p).count(lastPixels)==1',g)
        self.assertIn('(manual()?manualPhasePixels.size():commandPhases.size())==8&&manualTicks>=8',g)
    def test_palette_profile_establishes_unsupported_screen_presentation(self):
        gate=source('tests/inventory-unload-control/InventoryUnloadControlGate.cpp')
        setup=body(gate,'if(profile=="palette") {','        command(); stage=WAIT_PENDING;')
        snapshot='std::copy(game->getScreen()->getPalette(),game->getScreen()->getPalette()+256,palette);'
        reset='game->getScreen()->resetDisplay(false);'
        partial='game->getScreen()->setPalette(palette,0,1,false);'
        self.assertIn(snapshot,setup)
        self.assertEqual(setup.count(reset),1)
        self.assertEqual(setup.count(partial),1)
        self.assertLess(setup.index(snapshot),setup.index(reset))
        self.assertLess(setup.index(reset),setup.index(partial))
        precondition='require(!game->getScreen()->stanceAllowed()&&!game->getScreen()->inventoryWaitingAllowed());'
        self.assertEqual(setup.count(precondition),1)
        self.assertLess(setup.index(partial),setup.index(precondition))
        self.assertNotIn('getSurface()->setPalette',setup)
        self.assertNotIn('getSurface()->getPalette',setup)
        screen=source('src/Engine/Screen.cpp')
        reset_body=body(screen,'void Screen::resetDisplay(bool resetVideo)')
        palette_body=body(screen,'void Screen::setPalette(SDL_Color* colors, int firstcolor, int ncolors, bool immediately)')
        self.assertIn('_observerPaletteExact = false;',reset_body)
        self.assertIn('setPalette(getPalette());',reset_body)
        self.assertIn('(_numColors != ncolors) && (_firstColor != firstcolor)',palette_body)
        self.assertIn('_numColors = ncolors;',palette_body)
        self.assertIn('_firstColor = firstcolor;',palette_body)
        self.assertIn('if (immediately && _screen->format->BitsPerPixel == 8)',palette_body)
        self.assertIn('_observerPaletteExact &&',body(screen,'bool Screen::stanceAllowed() const'))
        self.assertIn('(_observerPaletteExact || (_firstColor == 0 && _numColors == 256))',body(screen,'bool Screen::inventoryWaitingAllowed() const'))
        # The reset queues full (0,256); replacement has the SAME first color,
        # so the merge condition is false and the deferred palette becomes (0,1).
        previous_first,previous_count,first,count=0,256,0,1
        merge=previous_count and previous_count!=count and previous_first!=first
        self.assertFalse(merge)
        exact=False
        self.assertFalse(exact or (first==0 and count==256))
        # resetDisplay also invalidates the grant; this is not an isolated
        # single-guard sensitivity claim or evidence of native execution.
        self.assertIn('_stanceController->invalidate(true);',reset_body)
    def test_fixed_semantic_endpoints_and_real_TTL(self):
        g=source('tests/inventory-unload-control/InventoryUnloadControlGate.cpp')
        self.assertNotIn('__LINE__',g)
        self.assertIn('#define require(ok) check((ok),SUPPORT)',g)
        self.assertIn('SDL_Delay(120100);',g)
        for profile in ('nested-transition','nested-pop','nested-save-notice','save-I','parent-I','reopen-I','send-failure-I','malformed-Q-short','malformed-nul','independent'):
            self.assertIn('"'+profile+'"',g)
        malformed=body(g,'} else if(stage==MALFORMED_REPLY&&available())','} else if(stage==WAIT_RETIRE')
        self.assertNotIn('sendPacket',malformed)
    def test_forwarding_preserves_other_gate_blocks(self):
        s=source('tests/stance-control/StanceControlGate.cpp')
        for method in ('prepare','silence','loaded','afterCleanup','beforeEvents','acceptEvent','afterEvents','beforeFrame','afterFrame','kneelClicked','selectionClicked','exitStatus'):
            self.assertIn('InventoryUnloadControlGate::'+method,s)
            self.assertIn('InventoryRightClickControlGate::'+method,s)

if __name__=='__main__': unittest.main()
