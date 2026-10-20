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
#include "PurchaseState.h"
#include "../Savegame/PurchaseCommands.h"
#include <algorithm>
#include <sstream>
#include <climits>
#include <iomanip>
#include "../fmath.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Timer.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/ComboBox.h"
#include "../Interface/TextList.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Interface/ScrollBar.h"
#endif
#include "../Savegame/SavedGame.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleItem.h"
#include "../Savegame/Base.h"
#include "../Engine/Action.h"
#include "../Savegame/Craft.h"
#include "../Savegame/ItemContainer.h"
#include "../Menu/ErrorMessageState.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/RuleSoldier.h"
#include "../Mod/RuleCraftWeapon.h"
#include "../Mod/Armor.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Purchase/Hire screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 */
PurchaseState::PurchaseState(Base *base) : _base(base), _sel(0), _total(0), _pQty(0), _cQty(0), _iQty(0.0), _ammoColor(0)
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(148, 16, 8, 176);
	_btnCancel = new TextButton(148, 16, 164, 176);
	_txtTitle = new Text(310, 17, 5, 8);
	_txtFunds = new Text(150, 9, 10, 24);
	_txtPurchases = new Text(150, 9, 160, 24);
	_txtSpaceUsed = new Text(150, 9, 160, 34);
	_txtCost = new Text(102, 9, 152, 44);
	_txtQuantity = new Text(60, 9, 256, 44);
	_cbxCategory = new ComboBox(this, 120, 16, 10, 36);
	_lstItems = new TextList(287, 120, 8, 54);

	// Set palette
	setInterface("buyMenu");

	_ammoColor = _game->getMod()->getInterface("buyMenu")->getElement("ammoColor")->color;

	add(_window, "window", "buyMenu");
	add(_btnOk, "button", "buyMenu");
	add(_btnCancel, "button", "buyMenu");
	add(_txtTitle, "text", "buyMenu");
	add(_txtFunds, "text", "buyMenu");
	add(_txtPurchases, "text", "buyMenu");
	add(_txtSpaceUsed, "text", "buyMenu");
	add(_txtCost, "text", "buyMenu");
	add(_txtQuantity, "text", "buyMenu");
	add(_lstItems, "list", "buyMenu");
	add(_cbxCategory, "text", "buyMenu");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&PurchaseState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&PurchaseState::btnOkClick, Options::keyOk);

	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&PurchaseState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&PurchaseState::btnCancelClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_PURCHASE_HIRE_PERSONNEL"));

	_txtFunds->setText(tr("STR_CURRENT_FUNDS").arg(Unicode::formatFunding(_game->getSavedGame()->getFunds())));

	_txtPurchases->setText(tr("STR_COST_OF_PURCHASES").arg(Unicode::formatFunding(_total)));

	_txtSpaceUsed->setVisible(Options::storageLimitsEnforced);
	std::ostringstream ss;
	ss << _base->getUsedStores() << ":" << _base->getAvailableStores();
	_txtSpaceUsed->setText(tr("STR_SPACE_USED").arg(ss.str()));

	_txtCost->setText(tr("STR_COST_PER_UNIT_UC"));

	_txtQuantity->setText(tr("STR_QUANTITY_UC"));

	_lstItems->setArrowColumn(227, ARROW_VERTICAL);
	_lstItems->setColumns(4, 150, 55, 50, 32);
	_lstItems->setSelectable(true);
	_lstItems->setBackground(_window);
	_lstItems->setMargin(2);
	_lstItems->onLeftArrowPress((ActionHandler)&PurchaseState::lstItemsLeftArrowPress);
	_lstItems->onLeftArrowRelease((ActionHandler)&PurchaseState::lstItemsLeftArrowRelease);
	_lstItems->onLeftArrowClick((ActionHandler)&PurchaseState::lstItemsLeftArrowClick);
	_lstItems->onRightArrowPress((ActionHandler)&PurchaseState::lstItemsRightArrowPress);
	_lstItems->onRightArrowRelease((ActionHandler)&PurchaseState::lstItemsRightArrowRelease);
	_lstItems->onRightArrowClick((ActionHandler)&PurchaseState::lstItemsRightArrowClick);
	_lstItems->onMousePress((ActionHandler)&PurchaseState::lstItemsMousePress);

	_cats.push_back("STR_ALL_ITEMS");

	const std::vector<std::string> &cw = _game->getMod()->getCraftWeaponsList();
	for (std::vector<std::string>::const_iterator i = cw.begin(); i != cw.end(); ++i)
	{
		RuleCraftWeapon *rule = _game->getMod()->getCraftWeapon(*i);
		_craftWeapons.insert(rule->getLauncherItem());
		_craftWeapons.insert(rule->getClipItem());
	}
	const std::vector<std::string> &ar = _game->getMod()->getArmorsList();
	for (std::vector<std::string>::const_iterator i = ar.begin(); i != ar.end(); ++i)
	{
		Armor *rule = _game->getMod()->getArmor(*i);
		_armors.insert(rule->getStoreItem());
	}

	const std::vector<std::string> &soldiers = _game->getMod()->getSoldiersList();
	for (std::vector<std::string>::const_iterator i = soldiers.begin(); i != soldiers.end(); ++i)
	{
		RuleSoldier *rule = _game->getMod()->getSoldier(*i);
		if (rule->getBuyCost() != 0 && _game->getSavedGame()->isResearched(rule->getRequirements()))
		{
			TransferRow row = { TRANSFER_SOLDIER, rule, tr(rule->getType()), rule->getBuyCost(), _base->getSoldierCount(rule->getType()), 0, 0 };
			_items.push_back(row);
			std::string cat = getCategory(_items.size() - 1);
			if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
			{
				_cats.push_back(cat);
			}
		}
	}
	{
		TransferRow row = { TRANSFER_SCIENTIST, 0, tr("STR_SCIENTIST"), _game->getMod()->getScientistCost() * 2, _base->getTotalScientists(), 0, 0 };
		_items.push_back(row);
		std::string cat = getCategory(_items.size() - 1);
		if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
		{
			_cats.push_back(cat);
		}
	}
	{
		TransferRow row = { TRANSFER_ENGINEER, 0, tr("STR_ENGINEER"), _game->getMod()->getEngineerCost() * 2, _base->getTotalEngineers(), 0, 0 };
		_items.push_back(row);
		std::string cat = getCategory(_items.size() - 1);
		if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
		{
			_cats.push_back(cat);
		}
	}
	const std::vector<std::string> &crafts = _game->getMod()->getCraftsList();
	for (std::vector<std::string>::const_iterator i = crafts.begin(); i != crafts.end(); ++i)
	{
		RuleCraft *rule = _game->getMod()->getCraft(*i);
		if (rule->getBuyCost() != 0 && _game->getSavedGame()->isResearched(rule->getRequirements()))
		{
			TransferRow row = { TRANSFER_CRAFT, rule, tr(rule->getType()), rule->getBuyCost(), _base->getCraftCount(rule->getType()), 0, 0 };
			_items.push_back(row);
			std::string cat = getCategory(_items.size() - 1);
			if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
			{
				_cats.push_back(cat);
			}
		}
	}
	const std::vector<std::string> &items = _game->getMod()->getItemsList();
	for (std::vector<std::string>::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		RuleItem *rule = _game->getMod()->getItem(*i);
		if (rule->getBuyCost() != 0 && _game->getSavedGame()->isResearched(rule->getRequirements()))
		{
			TransferRow row = { TRANSFER_ITEM, rule, tr(rule->getType()), rule->getBuyCost(), _base->getStorageItems()->getItem(rule->getType()), 0, 0 };
			_items.push_back(row);
			std::string cat = getCategory(_items.size() - 1);
			if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
			{
				_cats.push_back(cat);
			}
		}
	}

	_cbxCategory->setOptions(_cats, true);
	_cbxCategory->onChange((ActionHandler)&PurchaseState::cbxCategoryChange);

	updateList();

	_timerInc = new Timer(250);
	_timerInc->onTimer((StateHandler)&PurchaseState::increase);
	_timerDec = new Timer(250);
	_timerDec->onTimer((StateHandler)&PurchaseState::decrease);
}

/**
 *
 */
PurchaseState::~PurchaseState()
{
	delete _timerInc;
	delete _timerDec;
}

/**
* Runs the arrow timers.
*/
void PurchaseState::think()
{
	State::think();

	_timerInc->think(this, 0);
	_timerDec->think(this, 0);
}

/**
 * Determines the category a row item belongs in.
 * @param sel Selected row.
 * @returns Item category.
 */
std::string PurchaseState::getCategory(int sel) const
{
	RuleItem *rule = 0;
	switch (_items[sel].type)
	{
	case TRANSFER_SOLDIER:
	case TRANSFER_SCIENTIST:
	case TRANSFER_ENGINEER:
		return "STR_PERSONNEL";
	case TRANSFER_CRAFT:
		return "STR_CRAFT_ARMAMENT";
	case TRANSFER_ITEM:
		rule = (RuleItem*)_items[sel].rule;
		if (rule->getBattleType() == BT_CORPSE || rule->isAlien())
		{
			return "STR_ALIENS";
		}
		if (rule->getBattleType() == BT_NONE)
		{
			if (_craftWeapons.find(rule->getType()) != _craftWeapons.end())
			{
				return "STR_CRAFT_ARMAMENT";
			}
			if (_armors.find(rule->getType()) != _armors.end())
			{
				return "STR_EQUIPMENT";
			}
			return "STR_COMPONENTS";
		}
		return "STR_EQUIPMENT";
	}
	return "STR_ALL_ITEMS";
}

/**
 * Filters the current list of items.
 */
void PurchaseState::updateList()
{
	_lstItems->clearList();
	_rows.clear();
	for (size_t i = 0; i < _items.size(); ++i)
	{
		std::string cat = _cats[_cbxCategory->getSelected()];
		if (cat != "STR_ALL_ITEMS" && cat != getCategory(i))
		{
			continue;
		}
		std::string name = _items[i].name;
		bool ammo = false;
		if (_items[i].type == TRANSFER_ITEM)
		{
			RuleItem *rule = (RuleItem*)_items[i].rule;
			ammo = (rule->getBattleType() == BT_AMMO || (rule->getBattleType() == BT_NONE && rule->getClipSize() > 0));
			if (ammo)
			{
				name.insert(0, "  ");
			}
		}
		std::ostringstream ssQty, ssAmount;
		ssQty << _items[i].qtySrc;
		ssAmount << _items[i].amount;
		_lstItems->addRow(4, name.c_str(), Unicode::formatFunding(_items[i].cost).c_str(), ssQty.str().c_str(), ssAmount.str().c_str());
		_rows.push_back(i);
		if (_items[i].amount > 0)
		{
			_lstItems->setRowColor(_rows.size() - 1, _lstItems->getSecondaryColor());
		}
		else if (ammo)
		{
			_lstItems->setRowColor(_rows.size() - 1, _ammoColor);
		}
	}
}

/**
 * Purchases the selected items.
 * @param action Pointer to an action.
 */
void PurchaseState::btnOkClick(Action *)
{
    PurchaseCommands::commit(*_base, *_game->getSavedGame(), _items, _total, _game->getMod()->getPersonnelTime(),
        [this](const std::string &type) { return _game->getMod()->genSoldier(_game->getSavedGame(), type); });
    _game->popState();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void PurchaseState::btnCancelClick(Action *)
{
    PurchaseCommands::discard(_items, PurchaseDraft(_total, _pQty, _cQty, _iQty));
    _game->popState();
}

/**
 * Starts increasing the item.
 * @param action Pointer to an action.
 */
void PurchaseState::lstItemsLeftArrowPress(Action *action)
{
	_sel = _lstItems->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && !_timerInc->isRunning()) _timerInc->start();
}

/**
 * Stops increasing the item.
 * @param action Pointer to an action.
 */
void PurchaseState::lstItemsLeftArrowRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerInc->stop();
	}
}

/**
 * Increases the item by one on left-click,
 * to max on right-click.
 * @param action Pointer to an action.
 */
void PurchaseState::lstItemsLeftArrowClick(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT) increaseByValue(INT_MAX);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		increaseByValue(1);
		_timerInc->setInterval(250);
		_timerDec->setInterval(250);
	}
}

/**
 * Starts decreasing the item.
 * @param action Pointer to an action.
 */
void PurchaseState::lstItemsRightArrowPress(Action *action)
{
	_sel = _lstItems->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && !_timerDec->isRunning()) _timerDec->start();
}

/**
 * Stops decreasing the item.
 * @param action Pointer to an action.
 */
void PurchaseState::lstItemsRightArrowRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerDec->stop();
	}
}

/**
 * Decreases the item by one on left-click,
 * to 0 on right-click.
 * @param action Pointer to an action.
 */
void PurchaseState::lstItemsRightArrowClick(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT) decreaseByValue(INT_MAX);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		decreaseByValue(1);
		_timerInc->setInterval(250);
		_timerDec->setInterval(250);
	}
}

/**
 * Handles the mouse-wheels on the arrow-buttons.
 * @param action Pointer to an action.
 */
void PurchaseState::lstItemsMousePress(Action *action)
{
	_sel = _lstItems->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
	{
		_timerInc->stop();
		_timerDec->stop();
		if (action->getAbsoluteXMouse() >= _lstItems->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstItems->getArrowsRightEdge())
		{
			increaseByValue(Options::changeValueByMouseWheel);
		}
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
	{
		_timerInc->stop();
		_timerDec->stop();
		if (action->getAbsoluteXMouse() >= _lstItems->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstItems->getArrowsRightEdge())
		{
			decreaseByValue(Options::changeValueByMouseWheel);
		}
	}
}

/**
 * Increases the quantity of the selected item to buy by one.
 */
void PurchaseState::increase()
{
	_timerDec->setInterval(50);
	_timerInc->setInterval(50);
	increaseByValue(1);
}

/**
 * Increases the quantity of the selected item to buy by "change".
 * @param change How much we want to add.
 */
void PurchaseState::increaseByValue(int change)
{
    if (change > 0) changeQuantity(_sel, QuantityOperation::Adjust, change);
}

/**
 * Decreases the quantity of the selected item to buy by one.
 */
void PurchaseState::decrease()
{
	_timerInc->setInterval(50);
	_timerDec->setInterval(50);
	decreaseByValue(1);
}

/**
 * Decreases the quantity of the selected item to buy by "change".
 * @param change how much we want to add.
 */
void PurchaseState::decreaseByValue(int change)
{
    if (change > 0) changeQuantity(_sel, QuantityOperation::Adjust, -change);
}

/**
 * Updates the quantity-strings of the selected item.
 */
void PurchaseState::changeQuantity(size_t row, QuantityOperation operation, int value)
{
    if (row >= _rows.size() || _rows[row] < 0 || static_cast<size_t>(_rows[row]) >= _items.size()) return;
    _sel = row;
    const int previousAmount = getRow().amount;
    const PurchaseWarning warning = PurchaseCommands::quantity(*_base, *_game->getSavedGame(), getRow(),
        PurchaseDraft(_total, _pQty, _cQty, _iQty), operation, value);
    if (warning == PurchaseWarning::None) { if (getRow().amount != previousAmount) updateItemStrings(); return; }
    const char *message = warning == PurchaseWarning::Money ? "STR_NOT_ENOUGH_MONEY" :
        warning == PurchaseWarning::Quarters ? "STR_NOT_ENOUGH_LIVING_SPACE" :
        warning == PurchaseWarning::Hangars ? "STR_NO_FREE_HANGARS_FOR_PURCHASE" : "STR_NOT_ENOUGH_STORE_SPACE";
    _timerInc->stop();
    RuleInterface *menuInterface = _game->getMod()->getInterface("buyMenu");
    _game->pushState(new ErrorMessageState(tr(message), _palette, menuInterface->getElement("errorMessage")->color,
        "BACK13.SCR", menuInterface->getElement("errorPalette")->color));
}

void PurchaseState::updateItemStrings()
{
	_txtPurchases->setText(tr("STR_COST_OF_PURCHASES").arg(Unicode::formatFunding(_total)));
	std::ostringstream ss, ss5;
	ss << getRow().amount;
	_lstItems->setCellText(_sel, 3, ss.str());
	if (getRow().amount > 0)
	{
		_lstItems->setRowColor(_sel, _lstItems->getSecondaryColor());
	}
	else
	{
		_lstItems->setRowColor(_sel, _lstItems->getColor());
		if (getRow().type == TRANSFER_ITEM)
		{
			RuleItem *rule = (RuleItem*)getRow().rule;
			if (rule->getBattleType() == BT_AMMO || (rule->getBattleType() == BT_NONE && rule->getClipSize() > 0))
			{
				_lstItems->setRowColor(_sel, _ammoColor);
			}
		}
	}
	ss5 << _base->getUsedStores();
	if (std::abs(_iQty) > 0.05)
	{
		ss5 << "(";
		if (_iQty > 0.05)
			ss5 << "+";
		ss5 << std::fixed << std::setprecision(1) << _iQty << ")";
	}
	ss5 << ":" << _base->getAvailableStores();
	_txtSpaceUsed->setText(tr("STR_SPACE_USED").arg(ss5.str()));
}

/**
 * Updates the production list to match the category filter.
 */
void PurchaseState::cbxCategoryChange(Action *)
{
	updateList();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool PurchaseState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<PurchaseState *>(this)) || !agentPaintHealthy()) return false;
    const bool dropdown = _cbxCategory->agentPopupOpen();
    if ((_modal && (!dropdown || _modal != _cbxCategory)) || (dropdown && _modal != _cbxCategory)) return false;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    out = AgentUiState(); out.actor = _base;
    out.widgets.emplace_back(_lstItems, _lstItems->agentListRevision());
    out.widgets.emplace_back(_cbxCategory, _cbxCategory->agentUiRevision());
    const auto allowed = [this](const AgentUiState &part) { return _game->agentOverlayClear(part); };
    const auto includeRegions = [&](const std::vector<AgentUiRect> &regions, const Surface *owner) -> bool {
        if (regions.empty() || !agentRegionsUncovered(regions, owner, painted)) return false;
        AgentUiState part; part.regions = regions;
        if (!allowed(part)) return false;
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), regions.begin(), regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before); return false;
    };
    // Complete Purchase cells can share unchanged ink (stock '$' spans the
    // next row). Check each FULL mask against later widgets and overlays;
    // only then union already certified cell ink for the global disjoint gate.
    // This never deduplicates an overlap with a button, arrow or other widget.
    std::vector<AgentUiRect> purchaseInk;
    size_t purchaseWork = 0;
    bool purchaseHealthy = true;
    const auto inkOrder = [](const AgentUiRect &a, const AgentUiRect &b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    };
    const auto includeCell = [&](const std::vector<AgentUiRect> &regions) -> bool {
        if (!purchaseHealthy || regions.empty() || regions.size() > 8192 ||
            !agentRegionsUncovered(regions, _lstItems, painted)) return false;
        AgentUiState part; part.regions = regions;
        if (!allowed(part)) return false;
        std::vector<AgentUiRect> additions;
        for (const AgentUiRect &run : regions)
        {
            if (++purchaseWork > 2 * 1024 * 1024 || run.height != 1 || run.width <= 0 ||
                run.x < 0 || run.y < 0 || run.x + run.width > 640 || run.y >= 400)
            { purchaseHealthy = false; return false; }
            int left = run.x, right = run.x + run.width;
            auto prior = std::lower_bound(purchaseInk.begin(), purchaseInk.end(), run.y,
                [](const AgentUiRect &r, int y) { return r.y < y; });
            for (; prior != purchaseInk.end() && prior->y == run.y && left < right; ++prior)
            {
                if (++purchaseWork > 2 * 1024 * 1024) { purchaseHealthy = false; return false; }
                if (prior->x + prior->width <= left) continue;
                if (prior->x >= right) break;
                if (prior->x > left) additions.push_back({left, run.y, prior->x-left, 1});
                left = std::max(left, prior->x + prior->width);
            }
            if (left < right) additions.push_back({left, run.y, right-left, 1});
            if (additions.size() + purchaseInk.size() > 8192) { purchaseHealthy = false; return false; }
        }
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), additions.begin(), additions.end());
        if (!agentRegionsDisjoint(out)) { out.regions.resize(before); return false; }
        purchaseInk.insert(purchaseInk.end(), additions.begin(), additions.end());
        std::sort(purchaseInk.begin(), purchaseInk.end(), inkOrder);
        return true;
    };
    const auto include = [&](const Surface *widget) -> bool {
        AgentUiState part; agentIncludeWidget(part, widget);
        return includeRegions(part.regions, widget);
    };
    const auto includeCombo = [&]() -> bool {
        if (!_cbxCategory->agentToggleUncovered(painted)) return false;
        AgentUiState part; agentIncludeWidget(part, _cbxCategory);
        if (part.regions.empty() || !allowed(part)) return false;
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before); return false;
    };
    if (dropdown)
    {
        out.screen = "purchase_category";
        if (!includeCombo()) return false;
        AgentListProjection list;
        if (!agentProjectList(_cbxCategory->agentList(), _cbxCategory->agentList(), painted, 1, true, out, list, allowed)) return false;
        out.context = {list.up ? 1u : 0u, list.down ? 1u : 0u};
        out.visible = "{\"control\":\"category\",\"label\":null,\"rows\":" + list.rows + ",\"scroll\":" + list.scroll + "}";
        if (!out.rows.empty()) out.actions.emplace_back("choose_visible_row", "Choose visible row", "visible_control_semantics", "visible_row");
        if (list.up || list.down) out.actions.emplace_back("scroll_visible_list", "Scroll visible list", "visible_control_semantics", "scroll_direction");
        out.actions.emplace_back("close_dropdown", "Close choices", "visible_control_semantics");
        return agentRegionsDisjoint(out) && out.visible.size() <= 9000;
    }
    out.screen = "purchase";
    const auto text = [&](const Text *widget) -> std::string {
        const auto value = agentTextJson(widget);
        if (value == "null" || !agentWidgetUncovered(widget, painted)) return "null";
        return widget->getText().empty() || include(widget) ? value : "null";
    };
    const auto title = text(_txtTitle), funds = text(_txtFunds), total = text(_txtPurchases), space = text(_txtSpaceUsed);
    const auto headings = "[" + text(_txtCost) + "," + text(_txtQuantity) + "]";
    std::string category = "null", buttons = "[";
    const auto label = _cbxCategory->agentLabel();
    if (!label.empty() && label.size() <= 1024 && includeCombo())
    {
        category = "{\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back("open_purchase_category", label, "visible_text");
    }
    const auto button = [&](TextButton *widget, const char *id, const char *action, const char *kind) {
        const auto shown = agentButtonLabel(widget);
        if (shown.empty() || shown.size() > 1024 || !include(widget)) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(shown) + "}";
        out.actions.emplace_back(action, shown, "visible_text", kind);
    };
    button(_btnOk, "accept", "accept_purchase", "none");
    button(_btnCancel, "cancel", "cancel_purchase", "none"); buttons += "]";
    const Surface *up = nullptr, *down = nullptr;
    _lstItems->agentScrollControls(up, down);
    const bool canUp = up && include(up), canDown = down && include(down);
    out.context = {canUp ? 1u : 0u, canDown ? 1u : 0u};
    const std::string scroll = std::string("{\"up\":") + (canUp ? "true" : "false") + ",\"down\":" + (canDown ? "true" : "false") + "}";
    if (canUp || canDown) out.actions.emplace_back("scroll_purchase", "Scroll purchase list", "visible_control_semantics", "purchase_scroll");
    std::string scrollbar = "null";
    ScrollBar *bar = _lstItems->agentPurchaseScrollbar(); SDL_Rect track = {}, thumb = {};
    if (bar && bar->agentPaintGeometry(track, thumb) && includeRegions({{track.x,track.y,track.w,track.h}},bar))
    {
        out.points.emplace_back(thumb.x + thumb.w / 2, thumb.y + thumb.h / 2);
        const int targets[] = {track.y + thumb.h / 2, track.y + track.h / 2, track.y + track.h - 1 - (thumb.h - 1) / 2};
        const char *names[] = {"top", "middle", "bottom"};
        scrollbar = "{\"targets\":[";
        for (unsigned i = 0; i < 3; ++i)
        {
            if (i && targets[i] == targets[i-1]) continue;
            const auto slot = out.points.size() - 1;
            if (slot) scrollbar += ",";
            scrollbar += "{\"handle\":\"t" + std::to_string(slot) + "\",\"position\":" + agentJsonString(names[i]) + "}";
            out.points.emplace_back(i, targets[i]);
        }
        scrollbar += "]}";
        out.actions.emplace_back("position_purchase_list", "Position purchase list", "visible_control_semantics", "purchase_position");
    }
    const std::string prefix = "{\"title\":" + title + ",\"funds\":" + funds + ",\"total\":" + total + ",\"space_used\":" + space + ",\"headings\":" + headings + ",\"category\":" + category + ",\"rows\":";
    const std::string suffix = ",\"scroll\":" + scroll + ",\"scrollbar\":" + scrollbar + ",\"buttons\":" + buttons + "}";
    std::vector<AgentPurchaseRow> rows;
    if (!_lstItems->agentPurchaseRows(rows)) return false;
    std::string shownRows = "[";
    for (const AgentPurchaseRow &row : rows)
    {
        if (out.rows.size() == 32) break;
        const auto before = out.regions.size();
        const bool inc = row.increase && include(row.increase), dec = row.decrease && include(row.decrease);
        if (!inc && !dec) continue;
        std::string cells = "[";
        for (unsigned i = 0; i < 4; ++i)
        {
            if (i) cells += ",";
            std::vector<AgentUiRect> regions;
            for (const SDL_Rect &r : row.cells[i].regions) regions.push_back({r.x,r.y,r.w,r.h});
            const bool known = row.cells[i].known && (regions.empty() || includeCell(regions));
            cells += known ? agentJsonString(row.cells[i].text) : "null";
        }
        cells += "]";
        // A selector is useful only if its whole painted band is uncovered.
        // This separate check never grants a row control or a cell string.
        AgentUiState selected; selected.regions = {{row.bounds.x,row.bounds.y,row.bounds.w,row.bounds.h}};
        bool isSelected = row.selected && allowed(selected);
        bool afterList = false;
        for (const Surface *later : _surfaces)
        {
            if (later == _lstItems) { afterList = true; continue; }
            if (afterList && later && later->getVisible() &&
                later->getY() < row.bounds.y + row.bounds.h && later->getY()+later->getHeight() > row.bounds.y &&
                later->getX() < row.bounds.x+row.bounds.w && later->getX()+later->getWidth() > row.bounds.x) isSelected = false;
        }
        std::string controls = inc ? "[\"increase\"" : "[";
        if (dec) controls += inc ? ",\"decrease\"" : "\"decrease\"";
        controls += "]";
        const std::string entry = (out.rows.empty() ? "" : ",") + std::string("{\"handle\":\"r") + std::to_string(out.rows.size()) + "\",\"cells\":" + cells + ",\"selected\":" + (isSelected ? "true" : "false") + ",\"controls\":" + controls + "}";
        if (prefix.size() + shownRows.size() + entry.size() + suffix.size() + 1 > 9000) { out.regions.resize(before); break; }
        shownRows += entry; out.rows.push_back(row.logicalIndex); out.context.push_back((inc ? 1u : 0u) | (dec ? 2u : 0u));
    }
    shownRows += "]";
    if (!out.rows.empty()) out.actions.emplace_back("change_purchase_quantity", "Change purchase quantity", "visible_control_semantics", "purchase_quantity");
    out.visible = prefix + shownRows + suffix;
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size() <= 9000;
}

bool PurchaseState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    const auto slot = [](const std::string &value, char prefix, unsigned limit) -> int {
        if (value.size() < 2 || value.size() > 3 || value[0] != prefix || (value.size() == 3 && value[1] == '0')) return -1;
        unsigned result = 0;
        for (size_t i = 1; i < value.size(); ++i) { if (value[i] < '0' || value[i] > '9') return -1; result = result * 10 + value[i] - '0'; }
        return result < limit ? static_cast<int>(result) : -1;
    };
    if (current.screen == "purchase_category")
    {
        if (request.action == "close_dropdown") return _cbxCategory->agentClose(nullptr, this);
        if (request.action == "choose_visible_row")
        {
            const int index = slot(request.choice, 'r', current.rows.size());
            return index >= 0 && _cbxCategory->agentChooseVisibleRow(current.rows[index], nullptr, this);
        }
        if (request.action == "scroll_visible_list" && current.context.size() == 2)
        {
            if (request.choice == "up" && current.context[0]) { _cbxCategory->agentList()->scrollUp(false,false); return true; }
            if (request.choice == "down" && current.context[1]) { _cbxCategory->agentList()->scrollDown(false,false); return true; }
        }
        return false;
    }
    if (request.action == "open_purchase_category") return _cbxCategory->agentOpen();
    if (request.action == "accept_purchase") { btnOkClick(nullptr); return true; }
    if (request.action == "cancel_purchase") { btnCancelClick(nullptr); return true; }
    if (request.action == "change_purchase_quantity")
    {
        const int index = slot(request.choice, 'r', current.rows.size());
        if (index < 0 || current.context.size() != current.rows.size()+2) return false;
        QuantityOperation operation;
        unsigned required;
        if (request.operation == "adjust" && request.value && request.value >= -1000 && request.value <= 1000)
        { operation = QuantityOperation::Adjust; required = request.value > 0 ? 1 : 2; }
        else if (request.operation == "set" && request.value >= 0 && request.value <= 100000)
        { operation = QuantityOperation::Set; required = 3; }
        else if (request.operation == "max") { operation = QuantityOperation::Maximum; required = 1; }
        else if (request.operation == "clear") { operation = QuantityOperation::Clear; required = 2; }
        else return false;
        if ((current.context[index+2] & required) != required) return false;
        changeQuantity(current.rows[index], operation, request.value);
        return true;
    }
    if (request.action == "scroll_purchase")
    {
        const unsigned direction = request.direction == "up" ? 0 : request.direction == "down" ? 1 : 2;
        if (direction > 1 || current.context.size() < 2 || !current.context[direction]) return false;
        if (request.amount == "step" || request.amount == "end")
        {
            if (direction) _lstItems->scrollDown(request.amount == "end", false);
            else _lstItems->scrollUp(request.amount == "end", false);
            return true;
        }
        if (request.amount == "page")
        {
            const size_t at = _lstItems->getScroll(), page = _lstItems->getVisibleRows();
            _lstItems->scrollTo(direction ? at + page : at - std::min(at, page));
            return true;
        }
        return false;
    }
    if (request.action == "position_purchase_list" && current.points.size() > 1)
    {
        const int index = slot(request.choice,'t',current.points.size()-1);
        if (index < 0 || _lstItems->getRows() <= _lstItems->getVisibleRows()) return false;
        const size_t last = _lstItems->getRows() - _lstItems->getVisibleRows();
        const int position = current.points[index+1].first;
        _lstItems->scrollTo(position == 0 ? 0 : position == 1 ? last/2 : last);
        return true;
    }
    return false;
}
#endif

}
