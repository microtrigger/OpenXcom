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
#include "SellState.h"
#include <algorithm>
#include <sstream>
#include <climits>
#include <cmath>
#include <iomanip>
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Interface/ComboBox.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/Craft.h"
#include "../Savegame/ItemContainer.h"
#include "../Mod/RuleItem.h"
#include "../Mod/Armor.h"
#include "../Mod/RuleCraft.h"
#include "../Savegame/CraftWeapon.h"
#include "../Mod/RuleCraftWeapon.h"
#include "../Engine/Timer.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Mod/RuleInterface.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Sell/Sack screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param origin Game section that originated this state.
 */
SellState::SellState(Base *base, OptionsOrigin origin) : _base(base), _sel(0), _total(0), _spaceChange(0), _origin(origin)
{
	bool overfull = Options::storageLimitsEnforced && _base->storesOverfull();

	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(overfull? 288:148, 16, overfull? 16:8, 176);
	_btnCancel = new TextButton(148, 16, 164, 176);
	_txtTitle = new Text(310, 17, 5, 8);
	_txtSales = new Text(150, 9, 10, 24);
	_txtFunds = new Text(150, 9, 160, 24);
	_txtSpaceUsed = new Text(150, 9, 160, 34);
	_txtQuantity = new Text(54, 9, 136, 44);
	_txtSell = new Text(96, 9, 190, 44);
	_txtValue = new Text(40, 9, 270, 44);
	_cbxCategory = new ComboBox(this, 120, 16, 10, 36);
	_lstItems = new TextList(287, 120, 8, 54);

	// Set palette
	setInterface("sellMenu");

	_ammoColor = _game->getMod()->getInterface("sellMenu")->getElement("ammoColor")->color;

	add(_window, "window", "sellMenu");
	add(_btnOk, "button", "sellMenu");
	add(_btnCancel, "button", "sellMenu");
	add(_txtTitle, "text", "sellMenu");
	add(_txtSales, "text", "sellMenu");
	add(_txtFunds, "text", "sellMenu");
	add(_txtSpaceUsed, "text", "sellMenu");
	add(_txtQuantity, "text", "sellMenu");
	add(_txtSell, "text", "sellMenu");
	add(_txtValue, "text", "sellMenu");
	add(_lstItems, "list", "sellMenu");
	add(_cbxCategory, "text", "sellMenu");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnOk->setText(tr("STR_SELL_SACK"));
	_btnOk->onMouseClick((ActionHandler)&SellState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&SellState::btnOkClick, Options::keyOk);

	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&SellState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&SellState::btnCancelClick, Options::keyCancel);

	if (overfull)
	{
		_btnCancel->setVisible(false);
		_btnOk->setVisible(false);
	}

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_SELL_ITEMS_SACK_PERSONNEL"));

	_txtSales->setText(tr("STR_VALUE_OF_SALES").arg(Unicode::formatFunding(_total)));

	_txtFunds->setText(tr("STR_FUNDS").arg(Unicode::formatFunding(_game->getSavedGame()->getFunds())));

	_txtSpaceUsed->setVisible(Options::storageLimitsEnforced);

	std::ostringstream ss;
	ss << _base->getUsedStores() << ":" << _base->getAvailableStores();
	_txtSpaceUsed->setText(ss.str());
	_txtSpaceUsed->setText(tr("STR_SPACE_USED").arg(ss.str()));

	_txtQuantity->setText(tr("STR_QUANTITY_UC"));

	_txtSell->setText(tr("STR_SELL_SACK"));

	_txtValue->setText(tr("STR_VALUE"));

	_lstItems->setArrowColumn(182, ARROW_VERTICAL);
	_lstItems->setColumns(4, 156, 54, 24, 53);
	_lstItems->setSelectable(true);
	_lstItems->setBackground(_window);
	_lstItems->setMargin(2);
	_lstItems->onLeftArrowPress((ActionHandler)&SellState::lstItemsLeftArrowPress);
	_lstItems->onLeftArrowRelease((ActionHandler)&SellState::lstItemsLeftArrowRelease);
	_lstItems->onLeftArrowClick((ActionHandler)&SellState::lstItemsLeftArrowClick);
	_lstItems->onRightArrowPress((ActionHandler)&SellState::lstItemsRightArrowPress);
	_lstItems->onRightArrowRelease((ActionHandler)&SellState::lstItemsRightArrowRelease);
	_lstItems->onRightArrowClick((ActionHandler)&SellState::lstItemsRightArrowClick);
	_lstItems->onMousePress((ActionHandler)&SellState::lstItemsMousePress);

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

	for (std::vector<Soldier*>::iterator i = _base->getSoldiers()->begin(); i != _base->getSoldiers()->end(); ++i)
	{
		if ((*i)->getCraft() == 0)
		{
			TransferRow row = { TRANSFER_SOLDIER, (*i), (*i)->getName(true), 0, 1, 0, 0 };
			_items.push_back(row);
			std::string cat = getCategory(_items.size() - 1);
			if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
			{
				_cats.push_back(cat);
			}
		}
	}
	for (std::vector<Craft*>::iterator i = _base->getCrafts()->begin(); i != _base->getCrafts()->end(); ++i)
	{
		if ((*i)->getStatus() != "STR_OUT")
		{
			TransferRow row = { TRANSFER_CRAFT, (*i), (*i)->getName(_game->getLanguage()), (*i)->getRules()->getSellCost(), 1, 0, 0 };
			_items.push_back(row);
			std::string cat = getCategory(_items.size() - 1);
			if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
			{
				_cats.push_back(cat);
			}
		}
	}
	if (_base->getAvailableScientists() > 0)
	{
		TransferRow row = { TRANSFER_SCIENTIST, 0, tr("STR_SCIENTIST"), 0, _base->getAvailableScientists(), 0, 0 };
		_items.push_back(row);
		std::string cat = getCategory(_items.size() - 1);
		if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
		{
			_cats.push_back(cat);
		}
	}
	if (_base->getAvailableEngineers() > 0)
	{
		TransferRow row = { TRANSFER_ENGINEER, 0, tr("STR_ENGINEER"), 0, _base->getAvailableEngineers(), 0, 0 };
		_items.push_back(row);
		std::string cat = getCategory(_items.size() - 1);
		if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
		{
			_cats.push_back(cat);
		}
	}
	const std::vector<std::string> &items = _game->getMod()->getItemsList();
	for (std::vector<std::string>::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		int qty = _base->getStorageItems()->getItem(*i);
		if (Options::storageLimitsEnforced && _origin == OPT_BATTLESCAPE)
		{
			for (std::vector<Transfer*>::iterator j = _base->getTransfers()->begin(); j != _base->getTransfers()->end(); ++j)
			{
				if ((*j)->getItems() == *i)
				{
					qty += (*j)->getQuantity();
				}
			}
			for (std::vector<Craft*>::iterator j = _base->getCrafts()->begin(); j != _base->getCrafts()->end(); ++j)
			{
				qty += (*j)->getItems()->getItem(*i);
			}
		}
		RuleItem *rule = _game->getMod()->getItem(*i, true);
		if (qty > 0 && (Options::canSellLiveAliens || !rule->isAlien()))
		{
			TransferRow row = { TRANSFER_ITEM, rule, tr(*i), rule->getSellCost(), qty, 0, 0 };
			_items.push_back(row);
			std::string cat = getCategory(_items.size() - 1);
			if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
			{
				_cats.push_back(cat);
			}
		}
	}

	_cbxCategory->setOptions(_cats, true);
	_cbxCategory->onChange((ActionHandler)&SellState::cbxCategoryChange);

	updateList();

	_timerInc = new Timer(250);
	_timerInc->onTimer((StateHandler)&SellState::increase);
	_timerDec = new Timer(250);
	_timerDec->onTimer((StateHandler)&SellState::decrease);
}

/**
 *
 */
SellState::~SellState()
{
	delete _timerInc;
	delete _timerDec;
}

/**
 * Runs the arrow timers.
 */
void SellState::think()
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
std::string SellState::getCategory(int sel) const
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
void SellState::updateList()
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
		ssQty << _items[i].qtySrc - _items[i].amount;
		ssAmount << _items[i].amount;
		_lstItems->addRow(4, name.c_str(), ssQty.str().c_str(), ssAmount.str().c_str(), Unicode::formatFunding(_items[i].cost).c_str());
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
 * Sells the selected items.
 * @param action Pointer to an action.
 */
void SellState::btnOkClick(Action *)
{
	_game->getSavedGame()->setFunds(_game->getSavedGame()->getFunds() + _total);
	Soldier *soldier;
	Craft *craft;
	for (std::vector<TransferRow>::const_iterator i = _items.begin(); i != _items.end(); ++i)
	{
		if (i->amount > 0)
		{
			switch (i->type)
			{
			case TRANSFER_SOLDIER:
				soldier = (Soldier*)i->rule;
				for (std::vector<Soldier*>::iterator s = _base->getSoldiers()->begin(); s != _base->getSoldiers()->end(); ++s)
				{
					if (*s == soldier)
					{
						if ((*s)->getArmor()->getStoreItem() != Armor::NONE)
						{
							_base->getStorageItems()->addItem((*s)->getArmor()->getStoreItem());
						}
						_base->getSoldiers()->erase(s);
						break;
					}
				}
				delete soldier;
				break;
			case TRANSFER_CRAFT:
				craft = (Craft*)i->rule;
				_base->removeCraft(craft, true);
				delete craft;
				break;
			case TRANSFER_SCIENTIST:
				_base->setScientists(_base->getScientists() - i->amount);
				break;
			case TRANSFER_ENGINEER:
				_base->setEngineers(_base->getEngineers() - i->amount);
				break;
			case TRANSFER_ITEM:
				RuleItem *item = (RuleItem*)i->rule;
				if (_base->getStorageItems()->getItem(item->getType()) < i->amount)
				{
					int toRemove = i->amount - _base->getStorageItems()->getItem(item->getType());

					// remove all of said items from base
					_base->getStorageItems()->removeItem(item->getType(), INT_MAX);

					// if we still need to remove any, remove them from the crafts first, and keep a running tally
					for (std::vector<Craft*>::iterator j = _base->getCrafts()->begin(); j != _base->getCrafts()->end() && toRemove; ++j)
					{
						if ((*j)->getItems()->getItem(item->getType()) < toRemove)
						{
							toRemove -= (*j)->getItems()->getItem(item->getType());
							(*j)->getItems()->removeItem(item->getType(), INT_MAX);
						}
						else
						{
							(*j)->getItems()->removeItem(item->getType(), toRemove);
							toRemove = 0;
						}
					}

					// if there are STILL any left to remove, take them from the transfers, and if necessary, delete it.
					for (std::vector<Transfer*>::iterator j = _base->getTransfers()->begin(); j != _base->getTransfers()->end() && toRemove;)
					{
						if ((*j)->getItems() == item->getType())
						{
							if ((*j)->getQuantity() <= toRemove)
							{
								toRemove -= (*j)->getQuantity();
								delete *j;
								j = _base->getTransfers()->erase(j);
							}
							else
							{
								(*j)->setItems((*j)->getItems(), (*j)->getQuantity() - toRemove);
								toRemove = 0;
							}
						}
						else
						{
							++j;
						}
					}
				}
				else
				{
					_base->getStorageItems()->removeItem(item->getType(), i->amount);
				}
				break;
			}
		}
	}
	_game->popState();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void SellState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Starts increasing the item.
 * @param action Pointer to an action.
 */
void SellState::lstItemsLeftArrowPress(Action *action)
{
	_sel = _lstItems->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && !_timerInc->isRunning()) _timerInc->start();
}

/**
 * Stops increasing the item.
 * @param action Pointer to an action.
 */
void SellState::lstItemsLeftArrowRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerInc->stop();
	}
}

/**
 * Increases the selected item;
 * by one on left-click, to max on right-click.
 * @param action Pointer to an action.
 */
void SellState::lstItemsLeftArrowClick(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT) changeByValue(INT_MAX, 1);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		changeByValue(1,1);
		_timerInc->setInterval(250);
		_timerDec->setInterval(250);
	}
}

/**
 * Starts decreasing the item.
 * @param action Pointer to an action.
 */
void SellState::lstItemsRightArrowPress(Action *action)
{
	_sel = _lstItems->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && !_timerDec->isRunning()) _timerDec->start();
}

/**
 * Stops decreasing the item.
 * @param action Pointer to an action.
 */
void SellState::lstItemsRightArrowRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerDec->stop();
	}
}

/**
 * Decreases the selected item;
 * by one on left-click, to 0 on right-click.
 * @param action Pointer to an action.
 */
void SellState::lstItemsRightArrowClick(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT) changeByValue(INT_MAX, -1);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		changeByValue(1,-1);
		_timerInc->setInterval(250);
		_timerDec->setInterval(250);
	}
}

/**
 * Handles the mouse-wheels on the arrow-buttons.
 * @param action Pointer to an action.
 */
void SellState::lstItemsMousePress(Action *action)
{
	_sel = _lstItems->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
	{
		_timerInc->stop();
		_timerDec->stop();
		if (action->getAbsoluteXMouse() >= _lstItems->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstItems->getArrowsRightEdge())
		{
			changeByValue(Options::changeValueByMouseWheel, 1);
		}
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
	{
		_timerInc->stop();
		_timerDec->stop();
		if (action->getAbsoluteXMouse() >= _lstItems->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstItems->getArrowsRightEdge())
		{
			changeByValue(Options::changeValueByMouseWheel, -1);
		}
	}
}

/**
 * Increases the quantity of the selected item to sell by one.
 */
void SellState::increase()
{
	_timerDec->setInterval(50);
	_timerInc->setInterval(50);
	changeByValue(1,1);
}

/**
 * Increases or decreases the quantity of the selected item to sell.
 * @param change How much we want to add or remove.
 * @param dir Direction to change, +1 to increase or -1 to decrease.
 */
void SellState::changeByValue(int change, int dir)
{
	if (dir > 0)
	{
		if (0 >= change || getRow().qtySrc <= getRow().amount) return;
		change = std::min(getRow().qtySrc - getRow().amount, change);
	}
	else
	{
		if (0 >= change || 0 >= getRow().amount) return;
		change = std::min(getRow().amount, change);
	}
	getRow().amount += dir * change;
	_total += dir * getRow().cost * change;

	// Calculate the change in storage space.
	Craft *craft;
	Soldier *soldier;
	RuleItem *armor, *item, *weapon, *ammo;
	double total = 0.0;
	switch (getRow().type)
	{
	case TRANSFER_SOLDIER:
		soldier = (Soldier*)getRow().rule;
		if (soldier->getArmor()->getStoreItem() != Armor::NONE)
		{
			armor = _game->getMod()->getItem(soldier->getArmor()->getStoreItem(), true);
			_spaceChange += dir * armor->getSize();
		}
		break;
	case TRANSFER_CRAFT:
		craft = (Craft*)getRow().rule;
		for (std::vector<CraftWeapon*>::iterator w = craft->getWeapons()->begin(); w != craft->getWeapons()->end(); ++w)
		{
			if (*w)
			{
				weapon = _game->getMod()->getItem((*w)->getRules()->getLauncherItem(), true);
				total += weapon->getSize();
				ammo = _game->getMod()->getItem((*w)->getRules()->getClipItem());
				if (ammo)
					total += ammo->getSize() * (*w)->getClipsLoaded(_game->getMod());
			}
		}
		_spaceChange += dir * total;
		break;
	case TRANSFER_ITEM:
		item = (RuleItem*)getRow().rule;
		_spaceChange -= dir * change * item->getSize();
		break;
	default:
		//TRANSFER_SCIENTIST and TRANSFER_ENGINEER do not own anything that takes storage
		break;
	}

	updateItemStrings();
}

/**
 * Decreases the quantity of the selected item to sell by one.
 */
void SellState::decrease()
{
	_timerInc->setInterval(50);
	_timerDec->setInterval(50);
	changeByValue(1,-1);
}

/**
 * Updates the quantity-strings of the selected item.
 */
void SellState::updateItemStrings()
{
	std::ostringstream ss, ss2, ss3;
	ss << getRow().amount;
	_lstItems->setCellText(_sel, 2, ss.str());
	ss2 << getRow().qtySrc - getRow().amount;
	_lstItems->setCellText(_sel, 1, ss2.str());
	_txtSales->setText(tr("STR_VALUE_OF_SALES").arg(Unicode::formatFunding(_total)));

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

	ss3 << _base->getUsedStores();
	if (std::abs(_spaceChange) > 0.05)
	{
		ss3 << "(";
		if (_spaceChange > 0.05)
			ss3 << "+";
		ss3 << std::fixed << std::setprecision(1) << _spaceChange << ")";
	}
	ss3 << ":" << _base->getAvailableStores();
	_txtSpaceUsed->setText(tr("STR_SPACE_USED").arg(ss3.str()));
	if (Options::storageLimitsEnforced)
	{
		_btnOk->setVisible(!_base->storesOverfull(_spaceChange));
	}
}

/**
* Updates the production list to match the category filter.
*/
void SellState::cbxCategoryChange(Action *)
{
	updateList();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool SellState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<SellState *>(this)) || !agentPaintHealthy() ||
        _timerInc->isRunning() || _timerDec->isRunning()) return false;
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
        out.screen = "sell_category";
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
    out.screen = "sell";
    const auto text = [&](const Text *widget) -> std::string {
        const auto value = agentTextJson(widget);
        if (value == "null" || !agentWidgetUncovered(widget, painted)) return "null";
        return widget->getText().empty() || include(widget) ? value : "null";
    };
    const auto title = text(_txtTitle), funds = text(_txtFunds), total = text(_txtSales), space = text(_txtSpaceUsed);
    const auto headings = "[" + text(_txtQuantity) + "," + text(_txtSell) + "," + text(_txtValue) + "]";
    std::string category = "null", buttons = "[";
    const auto label = _cbxCategory->agentLabel();
    if (!label.empty() && label.size() <= 1024 && includeCombo())
    {
        category = "{\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back("open_sell_category", label, "visible_text");
    }
    const auto button = [&](TextButton *widget, const char *id, const char *action, const char *kind) {
        const auto shown = agentButtonLabel(widget);
        if (shown.empty() || shown.size() > 1024 || !include(widget)) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(shown) + "}";
        out.actions.emplace_back(action, shown, "visible_text", kind);
    };
    button(_btnOk, "accept", "accept_sell", "none");
    button(_btnCancel, "cancel", "cancel_sell", "none"); buttons += "]";
    const Surface *up = nullptr, *down = nullptr;
    _lstItems->agentScrollControls(up, down);
    const bool canUp = up && include(up), canDown = down && include(down);
    out.context = {canUp ? 1u : 0u, canDown ? 1u : 0u};
    const std::string scroll = std::string("{\"up\":") + (canUp ? "true" : "false") + ",\"down\":" + (canDown ? "true" : "false") + "}";
    if (canUp || canDown) out.actions.emplace_back("scroll_sell", "Scroll sell list", "visible_control_semantics", "sale_scroll");
    std::string scrollbar = "null";
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
    if (!out.rows.empty()) out.actions.emplace_back("change_sale_quantity", "Change sale quantity", "visible_control_semantics", "sale_quantity");
    out.visible = prefix + shownRows + suffix;
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size() <= 9000;
}

bool SellState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    const auto slot = [](const std::string &value, char prefix, unsigned limit) -> int {
        if (value.size() < 2 || value.size() > 3 || value[0] != prefix || (value.size() == 3 && value[1] == '0')) return -1;
        unsigned result = 0;
        for (size_t i = 1; i < value.size(); ++i) { if (value[i] < '0' || value[i] > '9') return -1; result = result * 10 + value[i] - '0'; }
        return result < limit ? static_cast<int>(result) : -1;
    };
    if (current.screen == "sell_category")
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
    if (request.action == "open_sell_category") return _cbxCategory->agentOpen();
    if (request.action == "accept_sell") { btnOkClick(nullptr); return true; }
    if (request.action == "cancel_sell") { btnCancelClick(nullptr); return true; }
    if (request.action == "change_sale_quantity")
    {
        const int index = slot(request.choice, 'r', current.rows.size());
        if (index < 0 || current.context.size() != current.rows.size()+2) return false;
        if (request.operation != "adjust" || (request.value != 1 && request.value != -1)) return false;
        const unsigned required = request.value > 0 ? 1u : 2u;
        if ((current.context[index+2] & required) != required) return false;
        const size_t row = current.rows[index];
        if (row >= _rows.size() || _rows[row] < 0 || static_cast<size_t>(_rows[row]) >= _items.size()) return false;
        // Route the admitted displayed row exactly as the native arrow-press
        // selection does, then use the ordinary one-step left-click handler.
        _sel = row;
        SDL_Event event = {}; event.type = SDL_MOUSEBUTTONUP; event.button.button = SDL_BUTTON_LEFT;
        Action action(&event, 1.0, 1.0, 0, 0);
        if (request.value > 0) lstItemsLeftArrowClick(&action);
        else lstItemsRightArrowClick(&action);
        return true;
    }
    if (request.action == "scroll_sell")
    {
        const unsigned direction = request.direction == "up" ? 0 : request.direction == "down" ? 1 : 2;
        if (direction > 1 || current.context.size() < 2 || !current.context[direction]) return false;
        if (request.amount != "step") return false;
        if (direction) _lstItems->scrollDown(false, false);
        else _lstItems->scrollUp(false, false);
        return true;
    }
    return false;
}
#endif

}
