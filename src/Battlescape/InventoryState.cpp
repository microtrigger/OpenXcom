#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE
#include "../../tests/inventory-open-control/InventoryOpenControlGate.h"
#endif
// Fork modification 2026-10-12: default-off capless presented inventory view; GPL-3.0-or-later.
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "InventoryState.h"
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
#include "../Observer/InventoryCloseOverlay.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE
#include "../../tests/inventory-close-control/InventoryCloseControlGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
#include "../../tests/inventory-click-control/InventoryClickControlGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
#include "../../tests/inventory-right-click-control/InventoryRightClickControlGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
#include "../../tests/inventory-unload-control/InventoryUnloadControlGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
#include "../../tests/inventory-ground-control/InventoryGroundControlGate.h"
#endif
#include <algorithm>
#include "Inventory.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentInventoryPaint.h"
#include "WarningMessage.h"
#endif
#include "../Engine/Game.h"
#include "../Engine/FileMap.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Screen.h"
#include "../Engine/Palette.h"
#include "../Engine/Surface.h"
#include "../Interface/Text.h"
#include "../Interface/BattlescapeButton.h"
#include "../Engine/Action.h"
#include "../Engine/InteractiveSurface.h"
#include "../Engine/Sound.h"
#include "../Engine/SurfaceSet.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Tile.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/Soldier.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleInventory.h"
#include "../Mod/Armor.h"
#include "../Engine/Options.h"
#include "UnitInfoState.h"
#include "BattlescapeState.h"
#include "BattlescapeGenerator.h"
#include "TileEngine.h"
#include "../Mod/RuleInterface.h"

namespace OpenXcom
{

static const int _templateBtnX = 288;
static const int _createTemplateBtnY = 90;
static const int _applyTemplateBtnY  = 113;

/**
 * Initializes all the elements in the Inventory screen.
 * @param game Pointer to the core game.
 * @param tu Does Inventory use up Time Units?
 * @param parent Pointer to parent Battlescape.
 */
InventoryState::InventoryState(bool tu, BattlescapeState *parent) : _tu(tu), _parent(parent)
{
	_battleGame = _game->getSavedGame()->getSavedBattle();

	if (Options::maximizeInfoScreens)
	{
		Options::baseXResolution = Screen::ORIGINAL_WIDTH;
		Options::baseYResolution = Screen::ORIGINAL_HEIGHT;
		_game->getScreen()->resetDisplay(false);
	}
	else if (!_battleGame->getTileEngine())
	{
		Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, true);
		_game->getScreen()->resetDisplay(false);
	}

	// Create objects
	_bg = new Surface(320, 200, 0, 0);
	_soldier = new Surface(320, 200, 0, 0);
	_txtName = new Text(210, 17, 28, 6);
	_txtTus = new Text(40, 9, 245, 24);
	_txtWeight = new Text(70, 9, 245, 24);
	_txtFAcc = new Text(50, 9, 245, 32);
	_txtReact = new Text(50, 9, 245, 40);
	_txtPSkill = new Text(50, 9, 245, 48);
	_txtPStr = new Text(50, 9, 245, 56);
	_txtItem = new Text(160, 9, 128, 140);
	_txtAmmo = new Text(66, 24, 254, 64);
	_btnOk = new BattlescapeButton(35, 22, 237, 1);
	_btnPrev = new BattlescapeButton(23, 22, 273, 1);
	_btnNext = new BattlescapeButton(23, 22, 297, 1);
	_btnUnload = new BattlescapeButton(32, 25, 288, 32);
	_btnGround = new BattlescapeButton(32, 15, 289, 137);
	_btnRank = new BattlescapeButton(26, 23, 0, 0);
	_btnCreateTemplate = new BattlescapeButton(32, 22, _templateBtnX, _createTemplateBtnY);
	_btnApplyTemplate = new BattlescapeButton(32, 22, _templateBtnX, _applyTemplateBtnY);
	_selAmmo = new Surface(RuleInventory::HAND_W * RuleInventory::SLOT_W, RuleInventory::HAND_H * RuleInventory::SLOT_H, 272, 88);
	_inv = new Inventory(_game, 320, 200, 0, 0, _parent == 0);

	// Set palette
	setPalette("PAL_BATTLESCAPE");

	add(_bg);

	// Set up objects
	_game->getMod()->getSurface("TAC01.SCR")->blit(_bg);

	add(_soldier);
	add(_txtName, "textName", "inventory", _bg);
	add(_txtTus, "textTUs", "inventory", _bg);
	add(_txtWeight, "textWeight", "inventory", _bg);
	add(_txtFAcc, "textFiring", "inventory", _bg);
	add(_txtReact, "textReaction", "inventory", _bg);
	add(_txtPSkill, "textPsiSkill", "inventory", _bg);
	add(_txtPStr, "textPsiStrength", "inventory", _bg);
	add(_txtItem, "textItem", "inventory", _bg);
	add(_txtAmmo, "textAmmo", "inventory", _bg);
	add(_btnOk, "buttonOK", "inventory", _bg);
	add(_btnPrev, "buttonPrev", "inventory", _bg);
	add(_btnNext, "buttonNext", "inventory", _bg);
	add(_btnUnload, "buttonUnload", "inventory", _bg);
	add(_btnGround, "buttonGround", "inventory", _bg);
	add(_btnRank, "rank", "inventory", _bg);
	add(_btnCreateTemplate, "buttonCreate", "inventory", _bg);
	add(_btnApplyTemplate, "buttonApply", "inventory", _bg);
	add(_selAmmo);
	add(_inv);

	// move the TU display down to make room for the weight display
	if (Options::showMoreStatsInInventoryView)
	{
		_txtTus->setY(_txtTus->getY() + 8);
	}

	centerAllSurfaces();



	_txtName->setBig();
	_txtName->setHighContrast(true);

	_txtTus->setHighContrast(true);

	_txtWeight->setHighContrast(true);

	_txtFAcc->setHighContrast(true);

	_txtReact->setHighContrast(true);

	_txtPSkill->setHighContrast(true);

	_txtPStr->setHighContrast(true);

	_txtItem->setHighContrast(true);

	_txtAmmo->setAlign(ALIGN_CENTER);
	_txtAmmo->setHighContrast(true);

	_btnOk->onMouseClick((ActionHandler)&InventoryState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&InventoryState::btnOkClick, Options::keyCancel);
	_btnOk->onKeyboardPress((ActionHandler)&InventoryState::btnOkClick, Options::keyBattleInventory);
	_btnOk->setTooltip("STR_OK");
	_btnOk->onMouseIn((ActionHandler)&InventoryState::txtTooltipIn);
	_btnOk->onMouseOut((ActionHandler)&InventoryState::txtTooltipOut);

	_btnPrev->onMouseClick((ActionHandler)&InventoryState::btnPrevClick);
	_btnPrev->onKeyboardPress((ActionHandler)&InventoryState::btnPrevClick, Options::keyBattlePrevUnit);
	_btnPrev->setTooltip("STR_PREVIOUS_UNIT");
	_btnPrev->onMouseIn((ActionHandler)&InventoryState::txtTooltipIn);
	_btnPrev->onMouseOut((ActionHandler)&InventoryState::txtTooltipOut);

	_btnNext->onMouseClick((ActionHandler)&InventoryState::btnNextClick);
	_btnNext->onKeyboardPress((ActionHandler)&InventoryState::btnNextClick, Options::keyBattleNextUnit);
	_btnNext->setTooltip("STR_NEXT_UNIT");
	_btnNext->onMouseIn((ActionHandler)&InventoryState::txtTooltipIn);
	_btnNext->onMouseOut((ActionHandler)&InventoryState::txtTooltipOut);

	_btnUnload->onMouseClick((ActionHandler)&InventoryState::btnUnloadClick);
	_btnUnload->setTooltip("STR_UNLOAD_WEAPON");
	_btnUnload->onMouseIn((ActionHandler)&InventoryState::txtTooltipIn);
	_btnUnload->onMouseOut((ActionHandler)&InventoryState::txtTooltipOut);

	_btnGround->onMouseClick((ActionHandler)&InventoryState::btnGroundClick);
	_btnGround->setTooltip("STR_SCROLL_RIGHT");
	_btnGround->onMouseIn((ActionHandler)&InventoryState::txtTooltipIn);
	_btnGround->onMouseOut((ActionHandler)&InventoryState::txtTooltipOut);

	_btnRank->onMouseClick((ActionHandler)&InventoryState::btnRankClick);
	_btnRank->setTooltip("STR_UNIT_STATS");
	_btnRank->onMouseIn((ActionHandler)&InventoryState::txtTooltipIn);
	_btnRank->onMouseOut((ActionHandler)&InventoryState::txtTooltipOut);

	_btnCreateTemplate->onMouseClick((ActionHandler)&InventoryState::btnCreateTemplateClick);
	_btnCreateTemplate->onKeyboardPress((ActionHandler)&InventoryState::btnCreateTemplateClick, Options::keyInvCreateTemplate);
	_btnCreateTemplate->setTooltip("STR_CREATE_INVENTORY_TEMPLATE");
	_btnCreateTemplate->onMouseIn((ActionHandler)&InventoryState::txtTooltipIn);
	_btnCreateTemplate->onMouseOut((ActionHandler)&InventoryState::txtTooltipOut);

	_btnApplyTemplate->onMouseClick((ActionHandler)&InventoryState::btnApplyTemplateClick);
	_btnApplyTemplate->onKeyboardPress((ActionHandler)&InventoryState::btnApplyTemplateClick, Options::keyInvApplyTemplate);
	_btnApplyTemplate->onKeyboardPress((ActionHandler)&InventoryState::onClearInventory, Options::keyInvClear);
	_btnApplyTemplate->onKeyboardPress((ActionHandler)&InventoryState::onAutoequip, Options::keyInvAutoEquip);
	_btnApplyTemplate->setTooltip("STR_APPLY_INVENTORY_TEMPLATE");
	_btnApplyTemplate->onMouseIn((ActionHandler)&InventoryState::txtTooltipIn);
	_btnApplyTemplate->onMouseOut((ActionHandler)&InventoryState::txtTooltipOut);


	// only use copy/paste buttons in setup (i.e. non-tu) mode
	if (_tu)
	{
		_btnCreateTemplate->setVisible(false);
		_btnApplyTemplate->setVisible(false);
	}
	else
	{
		_updateTemplateButtons(true);
	}

	_inv->draw();
	_inv->setTuMode(_tu);
	_inv->setSelectedUnit(_game->getSavedGame()->getSavedBattle()->getSelectedUnit());
	_inv->onMouseClick((ActionHandler)&InventoryState::invClick, 0);
	_inv->onMouseOver((ActionHandler)&InventoryState::invMouseOver);
	_inv->onMouseOut((ActionHandler)&InventoryState::invMouseOut);

	_txtTus->setVisible(_tu);
	_txtWeight->setVisible(Options::showMoreStatsInInventoryView);
	_txtFAcc->setVisible(Options::showMoreStatsInInventoryView && !_tu);
	_txtReact->setVisible(Options::showMoreStatsInInventoryView && !_tu);
	_txtPSkill->setVisible(Options::showMoreStatsInInventoryView && !_tu);
	_txtPStr->setVisible(Options::showMoreStatsInInventoryView && !_tu);
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
    _inventoryCloseOverlay = new Observer::InventoryCloseOverlay(this, _game->getMod()->getInterface("battlescape"));
    add(_inventoryCloseOverlay);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    InventoryClickControlGate::inventoryConstructed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    InventoryRightClickControlGate::inventoryConstructed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::inventoryConstructed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    InventoryGroundControlGate::inventoryConstructed(this);
#endif
}

static void _clearInventoryTemplate(std::vector<EquipmentLayoutItem*> &inventoryTemplate)
{
	for (std::vector<EquipmentLayoutItem*>::iterator eraseIt = inventoryTemplate.begin();
		 eraseIt != inventoryTemplate.end();
		 eraseIt = inventoryTemplate.erase(eraseIt))
	{
		delete *eraseIt;
	}
}

/**
 *
 */
InventoryState::~InventoryState()
{
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE
    InventoryCloseControlGate::inventoryDestroyed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    InventoryClickControlGate::inventoryDestroyed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    InventoryRightClickControlGate::inventoryDestroyed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::inventoryDestroyed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    InventoryGroundControlGate::inventoryDestroyed(this);
#endif
	_clearInventoryTemplate(_curInventoryTemplate);

	if (_battleGame->getTileEngine())
	{
		if (Options::maximizeInfoScreens)
		{
			Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, true);
			_game->getScreen()->resetDisplay(false);
		}
		Tile *inventoryTile = _battleGame->getSelectedUnit()->getTile();
		_battleGame->getTileEngine()->applyGravity(inventoryTile);
		_battleGame->getTileEngine()->calculateTerrainLighting(); // dropping/picking up flares
		_battleGame->getTileEngine()->recalculateFOV();
	}
	else
	{
		Screen::updateScale(Options::geoscapeScale, Options::baseXGeoscape, Options::baseYGeoscape, true);
		_game->getScreen()->resetDisplay(false);
	}
}

/**
 * Updates all soldier stats when the soldier changes.
 */
void InventoryState::init()
{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
    ++_inventoryUiRevision;
    _inventoryCloseShownActor = 0;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    _inventoryInitialized = false;
#endif
	State::init();
	BattleUnit *unit = _battleGame->getSelectedUnit();

	// no selected unit, close inventory
	if (unit == 0)
	{
		btnOkClick(0);
		return;
	}
	// skip to the first unit with inventory
	if (!unit->hasInventory())
	{
		if (_parent)
		{
			_parent->selectNextPlayerUnit(false, false, true, _tu);
		}
		else
		{
			_battleGame->selectNextPlayerUnit(false, false, true);
		}
		// no available unit, close inventory
		if (_battleGame->getSelectedUnit() == 0 || !_battleGame->getSelectedUnit()->hasInventory())
		{
			// starting a mission with just vehicles
			btnOkClick(0);
			return;
		}
		else
		{
			unit = _battleGame->getSelectedUnit();
		}
	}

	unit->setCache(0);
	_soldier->clear();
	_btnRank->clear();

	_txtName->setBig();
	_txtName->setText(unit->getName(_game->getLanguage()));
	_inv->setSelectedUnit(unit);
	Soldier *s = unit->getGeoscapeSoldier();
	if (s)
	{
		SurfaceSet *texture = _game->getMod()->getSurfaceSet("SMOKE.PCK");
		texture->getFrame(20 + s->getRank())->setX(0);
		texture->getFrame(20 + s->getRank())->setY(0);
		texture->getFrame(20 + s->getRank())->blit(_btnRank);

		std::string look = s->getArmor()->getSpriteInventory();
		if (s->getGender() == GENDER_MALE)
			look += "M";
		else
			look += "F";
		if (s->getLook() == LOOK_BLONDE)
			look += "0";
		if (s->getLook() == LOOK_BROWNHAIR)
			look += "1";
		if (s->getLook() == LOOK_ORIENTAL)
			look += "2";
		if (s->getLook() == LOOK_AFRICAN)
			look += "3";
		look += ".SPK";
		if (!_game->getMod()->getSurface(look, false))
		{
			look = s->getArmor()->getSpriteInventory() + ".SPK";
		}
		if (!_game->getMod()->getSurface(look, false))
		{
			look = s->getArmor()->getSpriteInventory();
		}
		_game->getMod()->getSurface(look)->blit(_soldier);
	}
	else
	{
		Surface *armorSurface = _game->getMod()->getSurface(unit->getArmor()->getSpriteInventory(), false);
		if (!armorSurface)
		{
			armorSurface = _game->getMod()->getSurface(unit->getArmor()->getSpriteInventory() + ".SPK", false);
		}
		if (!armorSurface)
		{
			armorSurface = _game->getMod()->getSurface(unit->getArmor()->getSpriteInventory() + "M0.SPK", false);
		}
		if (armorSurface)
		{
			armorSurface->blit(_soldier);
		}
	}

	updateStats();
	_refreshMouse();
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    _inventoryInitialized = true;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
    _inventoryCloseShownActor = unit;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE
    InventoryCloseControlGate::inventoryInitialized(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    InventoryClickControlGate::inventoryInitialized(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    InventoryRightClickControlGate::inventoryInitialized(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::inventoryInitialized(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    InventoryGroundControlGate::inventoryInitialized(this);
#endif
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE
    InventoryOpenControlGate::inventoryInitialized(this);
#endif
}

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
bool InventoryState::inventoryViewAllowed() const
{
    // Validate lifetime before accessing parent/save. This is a pixels-only
    // observer: no item enumeration, held-item inspection or redraw helpers.
    if (!_inventoryInitialized || !_tu || _modal || !_game->isState(const_cast<InventoryState *>(this)) ||
        !_game->inventoryParentState(_parent) || !_game->getSavedGame() ||
        _game->getSavedGame()->getSavedBattle() != _battleGame || !_battleGame ||
        _parent->_save != _battleGame || !_battleGame->getTileEngine() ||
        _battleGame->getDebugMode() || _battleGame->getSide() != FACTION_PLAYER ||
        !_game->getScreen()->inventoryWaitingAllowed()) return false;
    BattleUnit *actor = _battleGame->getSelectedUnit();
    if (!actor || actor->getFaction() != FACTION_PLAYER || actor->getOriginalFaction() != FACTION_PLAYER ||
        actor->getType() != "SOLDIER" || actor->isOut()) return false;
    return _bg->turnContextVisible() && _bg->getX() == 0 && _bg->getY() == 0 &&
        _bg->getWidth() == 320 && _bg->getHeight() == 200 &&
        _inv->turnContextVisible() && _inv->getX() == 0 && _inv->getY() == 0 &&
        _inv->getWidth() == 320 && _inv->getHeight() == 200;
}
void InventoryState::blit()
{
    State::blit();
    _game->getStanceController()->armInventory(this);
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    InventoryClickControlGate::inventoryDisplayed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    InventoryRightClickControlGate::inventoryDisplayed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::inventoryDisplayed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    InventoryGroundControlGate::inventoryDisplayed(this);
#endif
}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
bool InventoryState::inventoryCloseOverlayOwned() const
{
    // Event ownership survives a hidden/moved presentation child. Such a
    // proposal cannot receive consent, but its dismissal must not click through.
    return _inventoryCloseOverlay && _modal == _inventoryCloseOverlay;
}
bool InventoryState::inventoryCloseOverlayActive() const
{
    return inventoryCloseOverlayOwned() && _inventoryCloseOverlay->active();
}
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND
bool InventoryState::inventoryGroundControlVisible() const
{
    // This is the actual ordinary Ground button, including its upstream
    // one-column right-edge clipping. Never inspect items, pages or offsets.
    return _btnGround->turnContextVisible() && _btnGround->stanceContextVisible() &&
        _btnGround->getX() == 289 && _btnGround->getY() == 137 &&
        _btnGround->getWidth() == 32 && _btnGround->getHeight() == 15;
}
#endif
bool InventoryState::inventoryCloseContext(Observer::InventoryCloseContext &context, bool overlay) const
{
    return inventoryControlContext(context, overlay, false);
}
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
bool InventoryState::inventoryClickContext(Observer::InventoryCloseContext &context, bool overlay) const
{
    if (!inventoryControlContext(context, overlay, true) || !_inv->inventoryClickPresentationAllowed() ||
        _inv->_selUnit != context.actor || !_inv->_tu || _inv->_base ||
        _txtName->getX() != 28 || _txtName->getY() != 6 || _txtName->getWidth() != 210 || _txtName->getHeight() != 17) return false;
    // State::handle normally routes to every visible interactive surface.
    // Reject other controls covering our pointer domains, so direct ordinary
    // Inventory dispatch cannot skip another visible button's callback.
    for (Surface *surface : _surfaces)
    {
        if (surface == _inv || surface == _inventoryCloseOverlay || !surface->getVisible() ||
            !dynamic_cast<InteractiveSurface *>(surface)) continue;
        const int x = surface->getX(), y = surface->getY(), w = surface->getWidth(), h = surface->getHeight();
        if ((x < 32 && x + w > 0 && y < 111 && y + h > 63) ||
            (x < 160 && x + w > 128 && y < 111 && y + h > 63) ||
            (x < 320 && x + w > 0 && y < 200 && y + h > 152)) return false;
    }
    context.heldRevision = _inv->_heldSelectionRevision;
    context.selectionX = _inv->_selection->getX(); context.selectionY = _inv->_selection->getY();
    return true;
}
int InventoryState::inventoryClickBannerY() const { return _inv->inventoryClickBannerY(); }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK
void InventoryState::inventoryControlClick(int x, int y)
{
    // Canonical pointer coordinates, not a requested item/slot destination.
    // Ordinary hover centers the held sprite before exactly one normal click.
    SDL_Event event = {}; event.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_LEFT; event.button.state = SDL_RELEASED;
    event.button.x = x; event.button.y = y;
    Action action(&event, 1.0, 1.0, 0, 0);
    action.setSender(_inv); action.setMouseAction(x, y, _inv->getX(), _inv->getY());
    _inv->mouseOver(&action, this);
    _inv->mouseClick(&action, this);
    // Inventory::mouseClick retains its ordinary InteractiveSurface callback
    // tail, including InventoryState::invClick and normal stats refresh.
}
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK
void InventoryState::inventoryControlRightClick(int x, int y)
{
    // Dispatch exactly the ordinary pointer path, including its registered
    // callback tail. TU mode is checked by the held-presentation context.
    SDL_Event event = {}; event.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_RIGHT; event.button.state = SDL_RELEASED;
    event.button.x = x; event.button.y = y;
    Action rightAction(&event, 1.0, 1.0, 0, 0);
    rightAction.setSender(_inv); rightAction.setMouseAction(x, y, _inv->getX(), _inv->getY());
    _inv->mouseOver(&rightAction, this);
    _inv->mouseClick(&rightAction, this);
    // The ordinary handler may have popped this state. Do not access it here.
}
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD
bool InventoryState::inventoryUnloadContext(Observer::InventoryCloseContext &context, bool overlay) const
{
    if (!inventoryClickContext(context, overlay) || !_inv->inventoryUnloadPresentationAllowed() ||
        !_btnUnload->turnContextVisible() || !_btnUnload->stanceContextVisible() || _btnUnload->stanceToggled() ||
        _btnUnload->getX() != 288 || _btnUnload->getY() != 32 ||
        _btnUnload->getWidth() != 32 || _btnUnload->getHeight() != 25) return false;
    // State::handle reverse-routes every surface. Prove the actual inventory is
    // encountered before the actual Unload button, with no overlapping control.
    bool inventorySeen = false, unloadSeen = false;
    for (auto i = _surfaces.rbegin(); i != _surfaces.rend(); ++i)
    {
        Surface *surface = *i;
        if (surface == _inv) { inventorySeen = true; continue; }
        if (surface == _btnUnload) { if (!inventorySeen) return false; unloadSeen = true; continue; }
        if (surface == _inventoryCloseOverlay || !surface->getVisible()) continue;
        const int x = surface->getX(), y = surface->getY(), w = surface->getWidth(), h = surface->getHeight();
        if (x < 320 && x + w > 288 && y < 57 && y + h > 32 &&
            dynamic_cast<InteractiveSurface *>(surface)) return false;
    }
    return inventorySeen && unloadSeen;
}
int InventoryState::inventoryUnloadBannerY() const
{
    // The live selection surface is always 48px high. These disjoint bands
    // leave the soldier name and Unload button exposed and cannot both cross it.
    const int top = _inv->_selection->getY(), bottom = top + _inv->_selection->getHeight();
    return top < 184 && bottom > 152 ? 64 : 152;
}
void InventoryState::inventoryControlUnloadEvent(Uint8 type)
{
    SDL_Event event = {}; event.type = type;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
    event.button.x = 304; event.button.y = 44;
    Action action(&event, 1.0, 1.0, 0, 0);
    ++_inventoryUiRevision;
    // Bypass only InventoryState's consume latch: it belongs to the REAL local
    // confirmation event, not this synthetic pair. Never clear its ownership.
    // Preserve every ordinary surface, hover, pressed/inversion and callback.
    State::handle(&action);
    // The ordinary route may have changed the stack. Do not touch this again.
}
#endif
bool InventoryState::inventoryControlContext(Observer::InventoryCloseContext &context, bool overlay, bool allowHeld) const
{
    // The exact inventory and immediate parent must still be alive before any
    // save/actor dereference. Held-item inspection is refusal-only.
    if (!_inventoryInitialized || !_tu || !_game->isState(const_cast<InventoryState *>(this)) ||
        !_game->inventoryParentState(_parent) || !_game->getSavedGame() ||
        _game->getSavedGame()->getSavedBattle() != _battleGame || !_battleGame ||
        _parent->_save != _battleGame || !_battleGame->getTileEngine() ||
        _battleGame->getDebugMode() || _battleGame->getSide() != FACTION_PLAYER ||
        !_game->getScreen()->stanceAllowed() || SDL_GetModState() != KMOD_NONE ||
        (overlay ? !inventoryCloseOverlayActive() : (_modal || SDL_GetMouseState(0, 0))) ||
        (!allowHeld && _inv->getSelectedItem())) return false;
    BattleUnit *actor = _battleGame->getSelectedUnit();
    if (!actor || actor != _inventoryCloseShownActor || actor->getFaction() != FACTION_PLAYER ||
        actor->getOriginalFaction() != FACTION_PLAYER || actor->getType() != "SOLDIER" ||
        actor->isOut() || actor->isFloating() || actor->getStatus() != STATUS_STANDING ||
        _txtName->getText().empty() || _txtName->getText() != actor->getName(_game->getLanguage())) return false;
    Surface *widgets[] = {_bg, _inv, _txtName, _btnOk};
    for (Surface *widget : widgets)
        if (!widget->turnContextVisible() || widget->getX() < 0 || widget->getY() < 0 ||
            widget->getX() + widget->getWidth() > 320 || widget->getY() + widget->getHeight() > 200) return false;
    if (!_txtName->stanceContextVisible() || !_btnOk->stanceContextVisible() ||
        _bg->getWidth() != 320 || _bg->getHeight() != 200 || _inv->getWidth() != 320 || _inv->getHeight() != 200) return false;
    context = Observer::InventoryCloseContext();
    context.state = const_cast<InventoryState *>(this); context.parent = _parent;
    context.savedGame = _game->getSavedGame(); context.save = _battleGame; context.actor = actor;
    context.selectionRevision = _battleGame->stanceControlRevision();
    context.actorRevision = actor->stanceControlRevision(); context.uiRevision = _inventoryUiRevision;
    return true;
}
bool InventoryState::showInventoryCloseOverlay(Observer::StanceController *controller)
{
    if (_modal || !_inventoryCloseOverlay) return false;
    _inventoryCloseOverlay->show(controller);
    if (!_inventoryCloseOverlay->active()) return false;
    setModal(_inventoryCloseOverlay);
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE
    InventoryCloseControlGate::closeOverlayShown(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    InventoryClickControlGate::clickOverlayShown(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    InventoryRightClickControlGate::clickOverlayShown(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::clickOverlayShown(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    InventoryGroundControlGate::groundOverlayShown(this);
#endif
    return true;
}
void InventoryState::dismissInventoryCloseOverlay()
{
    if (!_inventoryCloseOverlay) return;
    const bool owned = inventoryCloseOverlayOwned();
    if (_modal == _inventoryCloseOverlay) setModal(0);
    _inventoryCloseOverlay->dismiss();
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    if (owned) InventoryClickControlGate::clickOverlayDismissed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    if (owned) InventoryRightClickControlGate::clickOverlayDismissed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    if (owned) InventoryUnloadControlGate::clickOverlayDismissed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    if (owned) InventoryGroundControlGate::groundOverlayDismissed(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE
    if (owned) InventoryCloseControlGate::closeOverlayDismissed(this);
#else
    (void)owned;
#endif
}
#endif

/**
 * Updates the soldier stats (Weight, TU).
 */
void InventoryState::updateStats()
{
	BattleUnit *unit = _battleGame->getSelectedUnit();

	_txtTus->setText(tr("STR_TIME_UNITS_SHORT").arg(unit->getTimeUnits()));

	int weight = unit->getCarriedWeight(_inv->getSelectedItem());
	_txtWeight->setText(tr("STR_WEIGHT").arg(weight).arg(unit->getBaseStats()->strength));
	if (weight > unit->getBaseStats()->strength)
	{
		_txtWeight->setSecondaryColor(_game->getMod()->getInterface("inventory")->getElement("weight")->color2);
	}
	else
	{
		_txtWeight->setSecondaryColor(_game->getMod()->getInterface("inventory")->getElement("weight")->color);
	}

	_txtFAcc->setText(tr("STR_ACCURACY_SHORT").arg((int)(unit->getBaseStats()->firing * unit->getHealth()) / unit->getBaseStats()->health));

	_txtReact->setText(tr("STR_REACTIONS_SHORT").arg(unit->getBaseStats()->reactions));

	if (unit->getBaseStats()->psiSkill > 0)
	{
		_txtPSkill->setText(tr("STR_PSIONIC_SKILL_SHORT").arg(unit->getBaseStats()->psiSkill));
	}
	else
	{
		_txtPSkill->setText("");
	}

	if (unit->getBaseStats()->psiSkill > 0 || (Options::psiStrengthEval && _game->getSavedGame()->isResearched(_game->getMod()->getPsiRequirements())))
	{
		_txtPStr->setText(tr("STR_PSIONIC_STRENGTH_SHORT").arg(unit->getBaseStats()->psiStrength));
	}
	else
	{
		_txtPStr->setText("");
	}
}

/**
 * Saves the soldiers' equipment-layout.
 */
void InventoryState::saveEquipmentLayout()
{
	for (std::vector<BattleUnit*>::iterator i = _battleGame->getUnits()->begin(); i != _battleGame->getUnits()->end(); ++i)
	{
		// we need X-Com soldiers only
		if ((*i)->getGeoscapeSoldier() == 0) continue;

		std::vector<EquipmentLayoutItem*> *layoutItems = (*i)->getGeoscapeSoldier()->getEquipmentLayout();

		// clear the previous save
		if (!layoutItems->empty())
		{
			for (std::vector<EquipmentLayoutItem*>::iterator j = layoutItems->begin(); j != layoutItems->end(); ++j)
				delete *j;
			layoutItems->clear();
		}

		// save the soldier's items
		// note: with using getInventory() we are skipping the ammos loaded, (they're not owned) because we handle the loaded-ammos separately (inside)
		for (std::vector<BattleItem*>::iterator j = (*i)->getInventory()->begin(); j != (*i)->getInventory()->end(); ++j)
		{
			std::string ammo;
			if ((*j)->needsAmmo() && 0 != (*j)->getAmmoItem()) ammo = (*j)->getAmmoItem()->getRules()->getType();
			else ammo = "NONE";
			layoutItems->push_back(new EquipmentLayoutItem(
				(*j)->getRules()->getType(),
				(*j)->getSlot()->getId(),
				(*j)->getSlotX(),
				(*j)->getSlotY(),
				ammo,
				(*j)->getFuseTimer()
			));
		}
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool InventoryState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (_modal || !_game->isState(const_cast<InventoryState *>(this)) || !agentPaintHealthy() || !_inv->agentProjectionReady()) return false;
    out = AgentUiState(); out.screen = "inventory"; out.save = _battleGame;
    // The inventory is the final composite in this explicit state. Other states
    // and unrecognized appended layers do not inherit this admission.
    if (_surfaces.empty() || _surfaces.back() != _inv) return false;
    std::vector<Surface *> preceding(_surfaces.begin(), _surfaces.end() - 1);
    const auto include = [&](const AgentUiRect &r) -> bool {
        if (!agentInventoryInside(r)) return false;
        out.regions.push_back(r);
        if (!agentRegionsDisjoint(out)) { out.regions.pop_back(); return false; }
        return true;
    };
    const auto text = [&](const Text *widget) -> std::string {
        const std::string value = agentInventoryTextJson(widget);
        if (value == "null" || !agentWidgetUncovered(widget, preceding)) return "null";
        SDL_Rect ink;
        if (!widget->agentPaintedBounds(ink)) return value == "\"\"" ? value : "null";
        AgentUiRect r = {ink.x, ink.y, ink.w, ink.h};
        if (!agentInventoryClear(_inv, r) || !include(r)) return "null";
        return value;
    };
    const auto button = [&](const Surface *widget, const char *id, const char *label) {
        // Stock Ground arrow allocates one transparent column past x=320.
        // Admit only its actual complete ink, not that padded allocation.
        AgentUiRect r;
        if (!agentInventoryInk(widget, r) || !agentRegionsUncovered({r}, widget, preceding)) return;
        if (agentInventoryClear(_inv, r) && include(r))
            out.actions.push_back(AgentUiAction(id,label,"visible_control_semantics"));
    };
    button(_btnOk, "close_inventory", "Close inventory");
    std::string name="null", tus="null", weight="null", item="null", ammo="null", warning="null", held="null", targets="[]";
    if (_tu)
    {
        name = text(_txtName); tus = text(_txtTus); weight = text(_txtWeight);
        item = text(_txtItem); ammo = text(_txtAmmo);
        const std::string rawMessage = _inv->_warning->agentMessage();
        std::string message;
        if (!rawMessage.empty() && agentInventoryRenderedText(rawMessage, message) && !message.empty())
        {
            const AgentUiRect r = {_inv->_warning->getX(),_inv->_warning->getY(),_inv->_warning->getWidth(),_inv->_warning->getHeight()};
            if (include(r)) { warning = agentJsonString(message); out.displayedMessage = message; }
        }
        AgentUiRect heldInk;
        if (_inv->_agentHeldPresented && agentInventoryInk(_inv->_selection, heldInk) && _inv->agentOverlayClear(heldInk, false) && include(heldInk))
        {
            held = "{\"status\":\"presented\"," + agentInventoryRectJson(heldInk) + "}";
            out.actions.push_back(AgentUiAction("cancel_inventory_hold","Return held item","visible_control_semantics"));
        }
        button(_btnUnload, "unload_inventory", "Unload weapon");
        button(_btnGround, "next_inventory_ground_page", "Next ground page");
        std::vector<AgentUiRect> includedLabels;
        targets = "[";
        for (const Inventory::AgentTarget &target : _inv->_agentTargets)
        {
            if (out.points.size() == 64) break;
            const AgentUiRect &r = target.rect;
            if (!_inv->agentGridVisible(target) || !_inv->agentOverlayClear(r) || !include(r)) continue;
            std::string label = target.label;
            if (label != "null")
            {
                if (!agentInventoryClear(_inv->_items, target.labelInk) || !_inv->agentOverlayClear(target.labelInk)) label = "null";
                else if (std::find(includedLabels.begin(), includedLabels.end(), target.labelInk) == includedLabels.end())
                {
                    if (include(target.labelInk)) includedLabels.push_back(target.labelInk); else label = "null";
                }
            }
            if (!out.points.empty()) targets += ",";
            const int x = r.x + r.width/2, y = r.y + r.height/2;
            targets += "{\"handle\":\"v" + std::to_string(out.points.size()) + "\",\"area\":" + agentJsonString(target.area) +
                ",\"label\":" + label + "," + agentInventoryRectJson(r) + ",\"point\":{\"x\":" + std::to_string(x) +
                ",\"y\":" + std::to_string(y) + "},\"item\":\"" + (_inv->agentItemsVisible(r) ? "presented" : "unknown") + "\"}";
            out.points.push_back(std::make_pair(x,y));
        }
        targets += "]";
        if (!out.points.empty())
        {
            out.actions.push_back(AgentUiAction("inventory_hover","Inspect inventory target","visible_control_semantics","inventory_target"));
            out.actions.push_back(AgentUiAction("inventory_activate","Activate inventory target","visible_control_semantics","inventory_target"));
        }
    }
    out.visible = "{\"close_label\":\"Close inventory\",\"selected_name\":"+name+",\"time_units\":"+tus+
        ",\"weight\":"+weight+",\"item_name\":"+item+",\"ammo\":"+ammo+",\"warning\":"+warning+
        ",\"held\":"+held+",\"targets\":"+targets+"}";
    // Identity/generations only prevent stale dispatch. They never reach DTOs.
    out.actor = _inv->_selUnit;
    out.actorRevision = _inv->_selUnit ? _inv->_selUnit->stanceControlRevision() : 0;
    out.selectionRevision = _battleGame->stanceControlRevision();
    out.widgets.push_back(std::make_pair(_inv, _inv->_agentInventoryRevision));
    out.widgets.push_back(std::make_pair(_inv->_selItem, 0));
    return !out.regions.empty();
}
bool InventoryState::agentBindingCurrent(const Observer::AgentUiState &binding) const
{
    return binding.save == _battleGame && binding.actor == _inv->_selUnit &&
        _inv->_selUnit == _battleGame->getSelectedUnit() &&
        binding.selectionRevision == _battleGame->stanceControlRevision() &&
        binding.actorRevision == (_inv->_selUnit ? _inv->_selUnit->stanceControlRevision() : 0);
}
bool InventoryState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState shown;
    if (!agentUiState(shown) || !shown.offers(request.action)) return false;
    if (request.action == "inventory_hover" || request.action == "inventory_activate")
    {
        const unsigned n = static_cast<unsigned>(std::stoul(request.choice.substr(1)));
        if (n >= shown.points.size()) return false;
        const int x = shown.points[n].first, y = shown.points[n].second;
        if (_inv->movePointer(x,y)) invMouseOver(nullptr);
        if (request.action == "inventory_activate" && _inv->activateButton(SDL_BUTTON_LEFT,x,y,false)) invClick(nullptr);
        return true;
    }
    if (request.action == "cancel_inventory_hold")
    {
        // Only the ordinary held return branch: never prime, close or right-click fallback.
        if (_inv->returnHeldItem()) invClick(nullptr);
        return true;
    }
    return agentDispatch(request.action);
}
bool InventoryState::agentDispatch(const std::string &action)
{
    Observer::AgentUiState shown;
    if (!agentUiState(shown) || !shown.offers(action)) return false;
    if (action == "close_inventory") btnOkClick(nullptr);
    else if (action == "unload_inventory") btnUnloadClick(nullptr);
    else if (action == "next_inventory_ground_page") btnGroundClick(nullptr);
    else return false;
    return true;
}
#endif

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void InventoryState::btnOkClick(Action *)
{
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE
    InventoryCloseControlGate::inventoryCloseEntered(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    InventoryClickControlGate::inventoryCloseEntered(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    InventoryRightClickControlGate::inventoryCloseEntered(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::inventoryCloseEntered(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    InventoryGroundControlGate::inventoryCloseEntered(this);
#endif
	if (_inv->getSelectedItem() != 0)
		return;
	_game->popState();
	Tile *inventoryTile = _battleGame->getSelectedUnit()->getTile();
	if (!_tu)
	{
		saveEquipmentLayout();
		_battleGame->resetUnitTiles();
		if (_battleGame->getTurn() == 1)
		{
			_battleGame->randomizeItemLocations(inventoryTile);
			if (inventoryTile->getUnit())
			{
				// make sure we select the unit closest to the ramp.
				_battleGame->setSelectedUnit(inventoryTile->getUnit());
			}
		}

		// initialize xcom units for battle
		for (std::vector<BattleUnit*>::iterator j = _battleGame->getUnits()->begin(); j != _battleGame->getUnits()->end(); ++j)
		{
			if ((*j)->getOriginalFaction() != FACTION_PLAYER || (*j)->isOut())
			{
				continue;
			}

			(*j)->prepareNewTurn(false);
		}
	}
}

/**
 * Selects the previous soldier.
 * @param action Pointer to an action.
 */
void InventoryState::btnPrevClick(Action *)
{
	if (_inv->getSelectedItem() != 0)
	{
		return;
	}

	if (_parent)
	{
		_parent->selectPreviousPlayerUnit(false, false, true);
	}
	else
	{
		_battleGame->selectPreviousPlayerUnit(false, false, true);
	}
	init();
}

/**
 * Selects the next soldier.
 * @param action Pointer to an action.
 */
void InventoryState::btnNextClick(Action *)
{
	if (_inv->getSelectedItem() != 0)
	{
		return;
	}
	if (_parent)
	{
		_parent->selectNextPlayerUnit(false, false, true);
	}
	else
	{
		_battleGame->selectNextPlayerUnit(false, false, true);
	}
	init();
}

/**
 * Unloads the selected weapon.
 * @param action Pointer to an action.
 */
void InventoryState::btnUnloadClick(Action *action)
{
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::inventoryUnloadButtonEntered(this, action);
#else
    (void)action;
#endif
	if (_inv->unload())
	{
		_txtItem->setText("");
		_txtAmmo->setText("");
		_selAmmo->clear();
		updateStats();
		_game->getMod()->getSoundByDepth(0, Mod::ITEM_DROP)->play();
	}
}

/**
 * Shows more ground items / rearranges them.
 * @param action Pointer to an action.
 */
void InventoryState::btnGroundClick(Action *)
{
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    InventoryGroundControlGate::inventoryGroundEntered(this);
#endif
	_inv->arrangeGround();
}

/**
 * Shows the unit info screen.
 * @param action Pointer to an action.
 */
void InventoryState::btnRankClick(Action *)
{
	_game->pushState(new UnitInfoState(_battleGame->getSelectedUnit(), _parent, true, false));
}

void InventoryState::btnCreateTemplateClick(Action *)
{
	// don't accept clicks when moving items
	if (_inv->getSelectedItem() != 0)
	{
		return;
	}

	// clear current template
	_clearInventoryTemplate(_curInventoryTemplate);

	// copy inventory instead of just keeping a pointer to it.  that way
	// create/apply can be used as an undo button for a single unit and will
	// also work as expected if inventory is modified after 'create' is clicked
	std::vector<BattleItem*> *unitInv = _battleGame->getSelectedUnit()->getInventory();
	for (std::vector<BattleItem*>::iterator j = unitInv->begin(); j != unitInv->end(); ++j)
	{
		if ((*j)->getRules()->isFixed()) {
			// don't copy fixed weapons into the template
			continue;
		}

		std::string ammo;
		if ((*j)->needsAmmo() && (*j)->getAmmoItem())
		{
			ammo = (*j)->getAmmoItem()->getRules()->getType();
		}
		else
		{
			ammo = "NONE";
		}

		_curInventoryTemplate.push_back(new EquipmentLayoutItem(
				(*j)->getRules()->getType(),
				(*j)->getSlot()->getId(),
				(*j)->getSlotX(),
				(*j)->getSlotY(),
				ammo,
				(*j)->getFuseTimer()));
	}

	// give audio feedback
	_game->getMod()->getSoundByDepth(_battleGame->getDepth(), Mod::ITEM_DROP)->play();
	_refreshMouse();
}

static void _clearInventory(Game *game, std::vector<BattleItem*> *unitInv, Tile *groundTile)
{
	RuleInventory *groundRuleInv = game->getMod()->getInventory("STR_GROUND", true);

	// clear unit's inventory (i.e. move everything to the ground)
	for (std::vector<BattleItem*>::iterator i = unitInv->begin(); i != unitInv->end(); )
	{
		if ((*i)->getRules()->isFixed()) {
			// don't drop fixed weapons
			++i;
			continue;
		}

		(*i)->setOwner(NULL);
		groundTile->addItem(*i, groundRuleInv);
		i = unitInv->erase(i);
	}
}

void InventoryState::btnApplyTemplateClick(Action *)
{
	// don't accept clicks when moving items
	// it's ok if the template is empty -- it will just result in clearing the
	// unit's inventory
	if (_inv->getSelectedItem() != 0)
	{
		return;
	}

	BattleUnit               *unit          = _battleGame->getSelectedUnit();
	std::vector<BattleItem*> *unitInv       = unit->getInventory();
	Tile                     *groundTile    = unit->getTile();
	std::vector<BattleItem*> *groundInv     = groundTile->getInventory();
	RuleInventory            *groundRuleInv = _game->getMod()->getInventory("STR_GROUND", true);

	_clearInventory(_game, unitInv, groundTile);

	// attempt to replicate inventory template by grabbing corresponding items
	// from the ground.  if any item is not found on the ground, display warning
	// message, but continue attempting to fulfill the template as best we can
	bool itemMissing = false;
	std::vector<EquipmentLayoutItem*>::iterator templateIt;
	for (templateIt = _curInventoryTemplate.begin(); templateIt != _curInventoryTemplate.end(); ++templateIt)
	{
		// search for template item in ground inventory
		std::vector<BattleItem*>::iterator groundItem;
		const bool needsAmmo = !_game->getMod()->getItem((*templateIt)->getItemType(), true)->getCompatibleAmmo()->empty();
		bool found = false;
		bool rescan = true;
		while (rescan)
		{
			rescan = false;

			const std::string targetAmmo = (*templateIt)->getAmmoItem();
			BattleItem *matchedWeapon = NULL;
			BattleItem *matchedAmmo   = NULL;
			for (groundItem = groundInv->begin(); groundItem != groundInv->end(); ++groundItem)
			{
				// if we find the appropriate ammo, remember it for later for if we find
				// the right weapon but with the wrong ammo
				const std::string groundItemName = (*groundItem)->getRules()->getType();
				if (needsAmmo && targetAmmo == groundItemName)
				{
					matchedAmmo = *groundItem;
				}

				if ((*templateIt)->getItemType() == groundItemName)
				{
					// if the template item would overlap with an existing item (i.e. a fixed
					// weapon that didn't get cleared in _clearInventory() above), skip it
					if (Inventory::overlapItems(unit, *groundItem,
								    _game->getMod()->getInventory((*templateIt)->getSlot(), true),
								    (*templateIt)->getSlotX(), (*templateIt)->getSlotY())) {
						// don't display 'item not found' warning message
						found = true;
						break;
					}

					// if the loaded ammo doesn't match the template item's,
					// remember the weapon for later and continue scanning
					BattleItem *loadedAmmo = (*groundItem)->getAmmoItem();
					if ((needsAmmo && loadedAmmo && targetAmmo != loadedAmmo->getRules()->getType())
					 || (needsAmmo && !loadedAmmo))
					{
						// remember the last matched weapon for simplicity (but prefer empty weapons if any are found)
						if (!matchedWeapon || matchedWeapon->getAmmoItem())
						{
							matchedWeapon = *groundItem;
						}
						continue;
					}

					// move matched item from ground to the appropriate inv slot
					(*groundItem)->setOwner(unit);
					(*groundItem)->setTile(0);
					(*groundItem)->setSlot(_game->getMod()->getInventory((*templateIt)->getSlot(), true));
					(*groundItem)->setSlotX((*templateIt)->getSlotX());
					(*groundItem)->setSlotY((*templateIt)->getSlotY());
					(*groundItem)->setFuseTimer((*templateIt)->getFuseTimer());
					unitInv->push_back(*groundItem);
					groundInv->erase(groundItem);
					found = true;
					break;
				}
			}

			// if we failed to find an exact match, but found unloaded ammo and
			// the right weapon, unload the target weapon, load the right ammo, and use it
			if (!found && matchedWeapon && (!needsAmmo || matchedAmmo))
			{
				// unload the existing ammo (if any) from the weapon
				BattleItem *loadedAmmo = matchedWeapon->getAmmoItem();
				if (loadedAmmo)
				{
					groundTile->addItem(loadedAmmo, groundRuleInv);
					matchedWeapon->setAmmoItem(NULL);
				}

				// load the correct ammo into the weapon
				if (matchedAmmo)
				{
					matchedWeapon->setAmmoItem(matchedAmmo);
					groundTile->removeItem(matchedAmmo);
				}

				// rescan and pick up the newly-loaded/unloaded weapon
				rescan = true;
			}
		}

		if (!found)
		{
			itemMissing = true;
		}
	}

	if (itemMissing)
	{
		_inv->showWarning(tr("STR_NOT_ENOUGH_ITEMS_FOR_TEMPLATE"));
	}

	// refresh ui
	_inv->arrangeGround(false);
	updateStats();
	_refreshMouse();

	// give audio feedback
	_game->getMod()->getSoundByDepth(_battleGame->getDepth(), Mod::ITEM_DROP)->play();
}

void InventoryState::_refreshMouse()
{
	// send a mouse motion event to refresh any hover actions
	int x, y;
	SDL_GetMouseState(&x, &y);
	SDL_WarpMouse(x+1, y);

	// move the mouse back to avoid cursor creep
	SDL_WarpMouse(x, y);
}

void InventoryState::onClearInventory(Action *)
{
	// don't act when moving items
	if (_inv->getSelectedItem() != 0)
	{
		return;
	}

	BattleUnit               *unit       = _battleGame->getSelectedUnit();
	std::vector<BattleItem*> *unitInv    = unit->getInventory();
	Tile                     *groundTile = unit->getTile();

	_clearInventory(_game, unitInv, groundTile);

	// refresh ui
	_inv->arrangeGround(false);
	updateStats();
	_refreshMouse();

	// give audio feedback
	_game->getMod()->getSoundByDepth(_battleGame->getDepth(), Mod::ITEM_DROP)->play();
}

void InventoryState::onAutoequip(Action *)
{
	// don't act when moving items
	if (_inv->getSelectedItem() != 0)
	{
		return;
	}

	BattleUnit               *unit          = _battleGame->getSelectedUnit();
	Tile                     *groundTile    = unit->getTile();
	std::vector<BattleItem*> *groundInv     = groundTile->getInventory();
	Mod                      *mod           = _game->getMod();
	RuleInventory            *groundRuleInv = mod->getInventory("STR_GROUND", true);
	int                       worldShade    = _battleGame->getGlobalShade();

	std::vector<BattleUnit*> units;
	units.push_back(unit);
	BattlescapeGenerator::autoEquip(units, mod, NULL, groundInv, groundRuleInv, worldShade, true, true);

	// refresh ui
	_inv->arrangeGround(false);
	updateStats();
	_refreshMouse();

	// give audio feedback
	_game->getMod()->getSoundByDepth(_battleGame->getDepth(), Mod::ITEM_DROP)->play();
}

/**
 * Updates item info.
 * @param action Pointer to an action.
 */
void InventoryState::invClick(Action *)
{
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    InventoryClickControlGate::inventoryInvClickEntered(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    InventoryRightClickControlGate::inventoryInvClickEntered(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::inventoryInvClickEntered(this);
#endif
	updateStats();
}

/**
 * Shows item info.
 * @param action Pointer to an action.
 */
void InventoryState::invMouseOver(Action *)
{
	if (_inv->getSelectedItem() != 0)
	{
		return;
	}

	BattleItem *item = _inv->getMouseOverItem();
	if (item != 0)
	{
		if (item->getUnit() && item->getUnit()->getStatus() == STATUS_UNCONSCIOUS)
		{
			_txtItem->setText(item->getUnit()->getName(_game->getLanguage()));
		}
		else
		{
			if (_game->getSavedGame()->isResearched(item->getRules()->getRequirements()))
			{
				_txtItem->setText(tr(item->getRules()->getName()));
			}
			else
			{
				_txtItem->setText(tr("STR_ALIEN_ARTIFACT"));
			}
		}
		std::string s;
		if (item->getAmmoItem() != 0 && item->needsAmmo())
		{
			s = tr("STR_AMMO_ROUNDS_LEFT").arg(item->getAmmoItem()->getAmmoQuantity());
			SDL_Rect r;
			r.x = 0;
			r.y = 0;
			r.w = RuleInventory::HAND_W * RuleInventory::SLOT_W;
			r.h = RuleInventory::HAND_H * RuleInventory::SLOT_H;
			_selAmmo->drawRect(&r, _game->getMod()->getInterface("inventory")->getElement("grid")->color);
			r.x++;
			r.y++;
			r.w -= 2;
			r.h -= 2;
			_selAmmo->drawRect(&r, Palette::blockOffset(0)+15);
			item->getAmmoItem()->getRules()->drawHandSprite(_game->getMod()->getSurfaceSet("BIGOBS.PCK"), _selAmmo);
			_updateTemplateButtons(false);
		}
		else
		{
			_selAmmo->clear();
			_updateTemplateButtons(!_tu);
		}
		if (item->getAmmoQuantity() != 0 && item->needsAmmo())
		{
			s = tr("STR_AMMO_ROUNDS_LEFT").arg(item->getAmmoQuantity());
		}
		else if (item->getRules()->getBattleType() == BT_MEDIKIT)
		{
			s = tr("STR_MEDI_KIT_QUANTITIES_LEFT").arg(item->getPainKillerQuantity()).arg(item->getStimulantQuantity()).arg(item->getHealQuantity());
		}
		_txtAmmo->setText(s);
	}
	else
	{
		if (_currentTooltip.empty())
		{
			_txtItem->setText("");
		}
		_txtAmmo->setText("");
		_selAmmo->clear();
		_updateTemplateButtons(!_tu);
	}
}

/**
 * Hides item info.
 * @param action Pointer to an action.
 */
void InventoryState::invMouseOut(Action *)
{
	_txtItem->setText("");
	_txtAmmo->setText("");
	_selAmmo->clear();
	_updateTemplateButtons(!_tu);
}

/**
 * Takes care of any events from the core game engine.
 * @param action Pointer to an action.
 */
void InventoryState::handle(Action *action)
{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
    // Remember ownership before routing: callbacks can retire the overlay.
    // The dismissal gesture must never reach widgets or the X1/X2 tail below.
    if (_inventoryCloseConsumeEvent || inventoryCloseOverlayOwned())
    {
        _inventoryCloseConsumeEvent = false;
        if (inventoryCloseOverlayActive()) State::handle(action);
        return;
    }
    ++_inventoryUiRevision;
#endif
	State::handle(action);


#ifndef __MORPHOS__
	if (action->getDetails()->type == SDL_MOUSEBUTTONDOWN)
	{
		if (action->getDetails()->button.button == SDL_BUTTON_X1)
		{
			btnNextClick(action);
		}
		else if (action->getDetails()->button.button == SDL_BUTTON_X2)
		{
			btnPrevClick(action);
		}
	}
#endif
}

/**
 * Shows a tooltip for the appropriate button.
 * @param action Pointer to an action.
 */
void InventoryState::txtTooltipIn(Action *action)
{
	if (_inv->getSelectedItem() == 0 && Options::battleTooltips)
	{
		_currentTooltip = action->getSender()->getTooltip();
		_txtItem->setText(tr(_currentTooltip));
	}
}

/**
 * Clears the tooltip text.
 * @param action Pointer to an action.
 */
void InventoryState::txtTooltipOut(Action *action)
{
	if (_inv->getSelectedItem() == 0 && Options::battleTooltips)
	{
		if (_currentTooltip == action->getSender()->getTooltip())
		{
			_currentTooltip = "";
			_txtItem->setText("");
		}
	}
}

void InventoryState::_updateTemplateButtons(bool isVisible)
{
	if (isVisible)
	{
		if (_curInventoryTemplate.empty())
		{
			// use "empty template" icons
			_game->getMod()->getSurface("InvCopy")->blit(_btnCreateTemplate);
			_game->getMod()->getSurface("InvPasteEmpty")->blit(_btnApplyTemplate);
			_btnApplyTemplate->setTooltip("STR_CLEAR_INVENTORY");
		}
		else
		{
			// use "active template" icons
			_game->getMod()->getSurface("InvCopyActive")->blit(_btnCreateTemplate);
			_game->getMod()->getSurface("InvPaste")->blit(_btnApplyTemplate);
			_btnApplyTemplate->setTooltip("STR_APPLY_INVENTORY_TEMPLATE");
		}
		_btnCreateTemplate->initSurfaces();
		_btnApplyTemplate->initSurfaces();
	}
	else
	{
		_btnCreateTemplate->clear();
		_btnApplyTemplate->clear();
	}
}

}
