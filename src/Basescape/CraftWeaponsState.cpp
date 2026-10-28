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
#include "CraftWeaponsState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentOutfitting.h"
#include "../Savegame/SavedGame.h"
#include <algorithm>
#endif
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Craft.h"
#include "../Savegame/CraftWeapon.h"
#include "../Mod/RuleCraftWeapon.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/Base.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Craft Weapons window.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param craft ID of the selected craft.
 * @param weapon ID of the selected weapon.
 */
CraftWeaponsState::CraftWeaponsState(Base *base, size_t craft, size_t weapon) : _base(base), _craft(craft), _weapon(weapon)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 220, 160, 50, 20, POPUP_BOTH);
	_btnCancel = new TextButton(140, 16, 90, 156);
	_txtTitle = new Text(208, 17, 56, 28);
	_txtArmament = new Text(76, 9, 66, 52);
	_txtQuantity = new Text(50, 9, 140, 52);
	_txtAmmunition = new Text(68, 17, 200, 44);
	_lstWeapons = new TextList(188, 80, 58, 68);

	// Set palette
	setInterface("craftWeapons");

	add(_window, "window", "craftWeapons");
	add(_btnCancel, "button", "craftWeapons");
	add(_txtTitle, "text", "craftWeapons");
	add(_txtArmament, "text", "craftWeapons");
	add(_txtQuantity, "text", "craftWeapons");
	add(_txtAmmunition, "text", "craftWeapons");
	add(_lstWeapons, "list", "craftWeapons");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK14.SCR"));

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&CraftWeaponsState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&CraftWeaponsState::btnCancelClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_SELECT_ARMAMENT"));

	_txtArmament->setText(tr("STR_ARMAMENT"));

	_txtQuantity->setText(tr("STR_QUANTITY_UC"));

	_txtAmmunition->setText(tr("STR_AMMUNITION_AVAILABLE"));
	_txtAmmunition->setWordWrap(true);
	_txtAmmunition->setVerticalAlign(ALIGN_BOTTOM);

	_lstWeapons->setColumns(3, 94, 50, 36);
	_lstWeapons->setSelectable(true);
	_lstWeapons->setBackground(_window);
	_lstWeapons->setMargin(8);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _lstWeapons->agentEnableOutfitting(3);
#endif

	_lstWeapons->addRow(1, tr("STR_NONE_UC").c_str());
	_weapons.push_back(0);

	const std::vector<std::string> &weapons = _game->getMod()->getCraftWeaponsList();
	for (std::vector<std::string>::const_iterator i = weapons.begin(); i != weapons.end(); ++i)
	{
		RuleCraftWeapon *w = _game->getMod()->getCraftWeapon(*i);
		if (_base->getStorageItems()->getItem(w->getLauncherItem()) > 0)
		{
			_weapons.push_back(w);
			std::ostringstream ss, ss2;
			ss << _base->getStorageItems()->getItem(w->getLauncherItem());
			if (!w->getClipItem().empty())
			{
				ss2 << _base->getStorageItems()->getItem(w->getClipItem());
			}
			else
			{
				ss2 << tr("STR_NOT_AVAILABLE");
			}
			_lstWeapons->addRow(3, tr(w->getType()).c_str(), ss.str().c_str(), ss2.str().c_str());
		}
	}
	_lstWeapons->onMouseClick((ActionHandler)&CraftWeaponsState::lstWeaponsClick);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentCraftBinding=_base->getCrafts()->at(_craft);
    _agentSlotWeaponBinding=_agentCraftBinding->getWeapons()->at(_weapon);
    _agentSlotAmmoBinding=_agentSlotWeaponBinding?_agentSlotWeaponBinding->getAmmo():0;
    if (_agentSlotWeaponBinding)
    {
        RuleCraftWeapon *rule=_agentSlotWeaponBinding->getRules();
        _agentStockBinding.push_back(_base->getStorageItems()->getItem(rule->getLauncherItem()));
        _agentStockBinding.push_back(_base->getStorageItems()->getItem(rule->getClipItem()));
    }
    for (RuleCraftWeapon *rule:_weapons) if (rule)
    {
        _agentStockBinding.push_back(_base->getStorageItems()->getItem(rule->getLauncherItem()));
        _agentStockBinding.push_back(_base->getStorageItems()->getItem(rule->getClipItem()));
    }
#endif
}

/**
 *
 */
CraftWeaponsState::~CraftWeaponsState()
{

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void CraftWeaponsState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Equips the weapon on the craft and returns to the previous screen.
 * @param action Pointer to an action.
 */
void CraftWeaponsState::lstWeaponsClick(Action *)
{
	CraftWeapon *current = _base->getCrafts()->at(_craft)->getWeapons()->at(_weapon);
	// Remove current weapon
	if (current != 0)
	{
		_base->getStorageItems()->addItem(current->getRules()->getLauncherItem());
		_base->getStorageItems()->addItem(current->getRules()->getClipItem(), current->getClipsLoaded(_game->getMod()));
		delete current;
		_base->getCrafts()->at(_craft)->getWeapons()->at(_weapon) = 0;
	}

	// Equip new weapon
	if (_weapons[_lstWeapons->getSelectedRow()] != 0)
	{
		CraftWeapon *sel = new CraftWeapon(_weapons[_lstWeapons->getSelectedRow()], 0);
		sel->setRearming(true);
		_base->getStorageItems()->removeItem(sel->getRules()->getLauncherItem());
		_base->getCrafts()->at(_craft)->getWeapons()->at(_weapon) = sel;
		if (_base->getCrafts()->at(_craft)->getStatus() == "STR_READY")
		{
			_base->getCrafts()->at(_craft)->setStatus("STR_REARMING");
		}
	}

	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool CraftWeaponsState::agentBindingsCurrent() const
{
    // The list is a constructor-time projection. Never dispatch its cached
    // choices after an owning object, slot or stock balance has changed.
    const auto *save=_game->getSavedGame();
    if (!save || !_base || std::find(save->getBases()->begin(),save->getBases()->end(),_base)==save->getBases()->end() ||
        _craft>=_base->getCrafts()->size() || _base->getCrafts()->at(_craft)!=_agentCraftBinding ||
        !_agentCraftBinding || _weapon>=_agentCraftBinding->getWeapons()->size() ||
        _agentCraftBinding->getWeapons()->at(_weapon)!=_agentSlotWeaponBinding ||
        (_agentSlotWeaponBinding && _agentSlotWeaponBinding->getAmmo()!=_agentSlotAmmoBinding)) return false;
    std::vector<int> stocks;
    if (_agentSlotWeaponBinding)
    {
        RuleCraftWeapon *rule=_agentSlotWeaponBinding->getRules();
        stocks.push_back(_base->getStorageItems()->getItem(rule->getLauncherItem()));
        stocks.push_back(_base->getStorageItems()->getItem(rule->getClipItem()));
    }
    for (RuleCraftWeapon *rule:_weapons) if (rule)
    {
        stocks.push_back(_base->getStorageItems()->getItem(rule->getLauncherItem()));
        stocks.push_back(_base->getStorageItems()->getItem(rule->getClipItem()));
    }
    return stocks==_agentStockBinding;
}
void CraftWeaponsState::blit()
{
    _agentDestinationPaintFull=false;_agentStoredPaintRevisions.clear();
    const bool before=Observer::agentOutfittingDestinationFull(_game);
    State::blit();
    _agentStoredPaintRevisions=Observer::agentOutfittingPaintRevisions(_surfaces);
    _agentDestinationPaintFull=before && Observer::agentOutfittingDestinationFull(_game) && Observer::agentPaintHealthy();
}
bool CraftWeaponsState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<CraftWeaponsState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentOutfittingDestinationFull(_game) ||
        _agentStoredPaintRevisions!=agentOutfittingPaintRevisions(_surfaces) || !agentBindingsCurrent()) return false;
    out=AgentUiState();out.screen="craft_weapons";out.actor=_agentCraftBinding;out.save=_game->getSavedGame();out.uiRevision=_agentUiRevision;
    out.widgets.emplace_back(_base,0);out.widgets.emplace_back(_agentSlotWeaponBinding,0);
    out.context.push_back(_craft);out.context.push_back(_weapon);
    AgentOutfittingView view(_game,_surfaces,out);
    const auto title=view.text(_txtTitle);
    const std::string headings="["+view.text(_txtArmament)+","+view.text(_txtQuantity)+","+view.text(_txtAmmunition)+"]";
    AgentListProjection list;if (!view.list(_lstWeapons,3,false,list)) return false;
    for (size_t row:out.rows)
    {
        if (row>=_weapons.size()) return false;
        out.widgets.emplace_back(_weapons[row],0);
    }
    if (!out.rows.empty()) out.actions.emplace_back("select_craft_weapon","Select armament","visible_control_semantics","outfitting_row");
    view.button(_btnCancel,"cancel","cancel_craft_weapons");
    out.visible="{\"title\":"+title+",\"headings\":"+headings+",\"rows\":"+list.rows+",\"scroll\":"+list.scroll+",\"buttons\":"+view.buttons+"]}";
    return view.finish();
}
bool CraftWeaponsState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if(request.action=="cancel_craft_weapons") { btnCancelClick(nullptr);return true; }
    if(Observer::agentOutfittingScroll(_lstWeapons,current,request)) return true;
    size_t row=0;
    if(request.action=="select_craft_weapon" && Observer::agentOutfittingRow(current,request,row) &&
        row<_weapons.size() && _lstWeapons->agentSelectOutfittingRow(row))
    { lstWeaponsClick(nullptr);return true; }
    return false;
}
#endif

}
