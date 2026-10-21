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
#include "TransferItemsState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#endif
#include <algorithm>
#include <sstream>
#include <climits>
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/Craft.h"
#include "../Savegame/ItemContainer.h"
#include "../Mod/RuleItem.h"
#include "../Engine/Timer.h"
#include "../Menu/ErrorMessageState.h"
#include "TransferConfirmState.h"
#include "../Engine/Options.h"
#include "../fmath.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/RuleCraftWeapon.h"
#include "../Mod/Armor.h"
#include "../Interface/ComboBox.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Transfer screen.
 * @param game Pointer to the core game.
 * @param baseFrom Pointer to the source base.
 * @param baseTo Pointer to the destination base.
 */
TransferItemsState::TransferItemsState(Base *baseFrom, Base *baseTo) : _baseFrom(baseFrom), _baseTo(baseTo), _sel(0), _total(0), _pQty(0), _cQty(0), _aQty(0), _iQty(0.0), _distance(0.0), _ammoColor(0)
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(148, 16, 8, 176);
	_btnCancel = new TextButton(148, 16, 164, 176);
	_txtTitle = new Text(310, 17, 5, 8);
	_txtQuantity = new Text(50, 9, 150, 24);
	_txtAmountTransfer = new Text(60, 17, 200, 24);
	_txtAmountDestination = new Text(60, 17, 260, 24);
	_cbxCategory = new ComboBox(this, 120, 16, 10, 24);
	_lstItems = new TextList(287, 128, 8, 44);

	// Set palette
	setInterface("transferMenu");

	_ammoColor = _game->getMod()->getInterface("transferMenu")->getElement("ammoColor")->color;

	add(_window, "window", "transferMenu");
	add(_btnOk, "button", "transferMenu");
	add(_btnCancel, "button", "transferMenu");
	add(_txtTitle, "text", "transferMenu");
	add(_txtQuantity, "text", "transferMenu");
	add(_txtAmountTransfer, "text", "transferMenu");
	add(_txtAmountDestination, "text", "transferMenu");
	add(_lstItems, "list", "transferMenu");
	add(_cbxCategory, "text", "transferMenu");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnOk->setText(tr("STR_TRANSFER"));
	_btnOk->onMouseClick((ActionHandler)&TransferItemsState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&TransferItemsState::btnOkClick, Options::keyOk);

	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&TransferItemsState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&TransferItemsState::btnCancelClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_TRANSFER"));

	_txtQuantity->setText(tr("STR_QUANTITY_UC"));

	_txtAmountTransfer->setText(tr("STR_AMOUNT_TO_TRANSFER"));
	_txtAmountTransfer->setWordWrap(true);

	_txtAmountDestination->setText(tr("STR_AMOUNT_AT_DESTINATION"));
	_txtAmountDestination->setWordWrap(true);

	_lstItems->setArrowColumn(193, ARROW_VERTICAL);
	_lstItems->setColumns(4, 162, 58, 40, 27);
	_lstItems->setSelectable(true);
	_lstItems->setBackground(_window);
	_lstItems->setMargin(2);
	_lstItems->onLeftArrowPress((ActionHandler)&TransferItemsState::lstItemsLeftArrowPress);
	_lstItems->onLeftArrowRelease((ActionHandler)&TransferItemsState::lstItemsLeftArrowRelease);
	_lstItems->onLeftArrowClick((ActionHandler)&TransferItemsState::lstItemsLeftArrowClick);
	_lstItems->onRightArrowPress((ActionHandler)&TransferItemsState::lstItemsRightArrowPress);
	_lstItems->onRightArrowRelease((ActionHandler)&TransferItemsState::lstItemsRightArrowRelease);
	_lstItems->onRightArrowClick((ActionHandler)&TransferItemsState::lstItemsRightArrowClick);
	_lstItems->onMousePress((ActionHandler)&TransferItemsState::lstItemsMousePress);

	_distance = getDistance();

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

	for (std::vector<Soldier*>::iterator i = _baseFrom->getSoldiers()->begin(); i != _baseFrom->getSoldiers()->end(); ++i)
	{
		if ((*i)->getCraft() == 0)
		{
			TransferRow row = { TRANSFER_SOLDIER, (*i), (*i)->getName(true), (int)(5 * _distance), 1, 0, 0 };
			_items.push_back(row);
			std::string cat = getCategory(_items.size() - 1);
			if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
			{
				_cats.push_back(cat);
			}
		}
	}
	for (std::vector<Craft*>::iterator i = _baseFrom->getCrafts()->begin(); i != _baseFrom->getCrafts()->end(); ++i)
	{
		if ((*i)->getStatus() != "STR_OUT" || (Options::canTransferCraftsWhileAirborne && (*i)->getFuel() >= (*i)->getFuelLimit(_baseTo)))
		{
			TransferRow row = { TRANSFER_CRAFT, (*i), (*i)->getName(_game->getLanguage()), (int)(25 * _distance), 1, 0, 0 };
			_items.push_back(row);
			std::string cat = getCategory(_items.size() - 1);
			if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
			{
				_cats.push_back(cat);
			}
		}
	}
	if (_baseFrom->getAvailableScientists() > 0)
	{
		TransferRow row = { TRANSFER_SCIENTIST, 0, tr("STR_SCIENTIST"), (int)(5 * _distance), _baseFrom->getAvailableScientists(), _baseTo->getAvailableScientists(), 0 };
		_items.push_back(row);
		std::string cat = getCategory(_items.size() - 1);
		if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
		{
			_cats.push_back(cat);
		}
	}
	if (_baseFrom->getAvailableEngineers() > 0)
	{
		TransferRow row = { TRANSFER_ENGINEER, 0, tr("STR_ENGINEER"), (int)(5 * _distance), _baseFrom->getAvailableEngineers(), _baseTo->getAvailableEngineers(), 0 };
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
		int qty = _baseFrom->getStorageItems()->getItem(*i);
		if (qty > 0)
		{
			RuleItem *rule = _game->getMod()->getItem(*i);
			TransferRow row = { TRANSFER_ITEM, rule, tr(*i), (int)(1 * _distance), qty, _baseTo->getStorageItems()->getItem(*i), 0 };
			_items.push_back(row);
			std::string cat = getCategory(_items.size() - 1);
			if (std::find(_cats.begin(), _cats.end(), cat) == _cats.end())
			{
				_cats.push_back(cat);
			}
		}
	}

	_cbxCategory->setOptions(_cats, true);
	_cbxCategory->onChange((ActionHandler)&TransferItemsState::cbxCategoryChange);

	updateList();

	_timerInc = new Timer(250);
	_timerInc->onTimer((StateHandler)&TransferItemsState::increase);
	_timerDec = new Timer(250);
	_timerDec->onTimer((StateHandler)&TransferItemsState::decrease);
}

/**
 *
 */
TransferItemsState::~TransferItemsState()
{
	delete _timerInc;
	delete _timerDec;
}

/**
 * Runs the arrow timers.
 */
void TransferItemsState::think()
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
std::string TransferItemsState::getCategory(int sel) const
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
void TransferItemsState::updateList()
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
		std::ostringstream ssQtySrc, ssQtyDst, ssAmount;
		ssQtySrc << _items[i].qtySrc - _items[i].amount;
		ssQtyDst << _items[i].qtyDst;
		ssAmount << _items[i].amount;
		_lstItems->addRow(4, name.c_str(), ssQtySrc.str().c_str(), ssAmount.str().c_str(), ssQtyDst.str().c_str());
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
 * Transfers the selected items.
 * @param action Pointer to an action.
 */
void TransferItemsState::btnOkClick(Action *)
{
	_game->pushState(new TransferConfirmState(_baseTo, this));
}

/**
 * Completes the transfer between bases.
 */
void TransferItemsState::completeTransfer()
{
	int time = (int)floor(6 + _distance / 10.0);
	_game->getSavedGame()->setFunds(_game->getSavedGame()->getFunds() - _total);
	for (std::vector<TransferRow>::const_iterator i = _items.begin(); i != _items.end(); ++i)
	{
		if (i->amount > 0)
		{
			Transfer *t = 0;
			Craft *craft = 0;
			switch (i->type)
			{
			case TRANSFER_SOLDIER:
				for (std::vector<Soldier*>::iterator s = _baseFrom->getSoldiers()->begin(); s != _baseFrom->getSoldiers()->end(); ++s)
				{
					if (*s == i->rule)
					{
						(*s)->setPsiTraining(false);
						t = new Transfer(time);
						t->setSoldier(*s);
						_baseTo->getTransfers()->push_back(t);
						_baseFrom->getSoldiers()->erase(s);
						break;
					}
				}
				break;
			case TRANSFER_CRAFT:
				craft = (Craft*)i->rule;
				// Transfer soldiers inside craft
				for (std::vector<Soldier*>::iterator s = _baseFrom->getSoldiers()->begin(); s != _baseFrom->getSoldiers()->end();)
				{
					if ((*s)->getCraft() == craft)
					{
						(*s)->setPsiTraining(false);
						if (craft->getStatus() == "STR_OUT")
						{
							_baseTo->getSoldiers()->push_back(*s);
						}
						else
						{
							t = new Transfer(time);
							t->setSoldier(*s);
							_baseTo->getTransfers()->push_back(t);
						}
						s = _baseFrom->getSoldiers()->erase(s);
					}
					else
					{
						++s;
					}
				}

				// Transfer craft
				_baseFrom->removeCraft(craft, false);
				if (craft->getStatus() == "STR_OUT")
				{
					bool returning = (craft->getDestination() == (Target*)craft->getBase());
					_baseTo->getCrafts()->push_back(craft);
					craft->setBase(_baseTo, false);
					if (craft->getFuel() <= craft->getFuelLimit(_baseTo))
					{
						craft->setLowFuel(true);
						craft->returnToBase();
					}
					else if (returning)
					{
						craft->setLowFuel(false);
						craft->returnToBase();
					}
				}
				else
				{
					t = new Transfer(time);
					t->setCraft(craft);
					_baseTo->getTransfers()->push_back(t);
				}
				break;
			case TRANSFER_SCIENTIST:
				_baseFrom->setScientists(_baseFrom->getScientists() - i->amount);
				t = new Transfer(time);
				t->setScientists(i->amount);
				_baseTo->getTransfers()->push_back(t);
				break;
			case TRANSFER_ENGINEER:
				_baseFrom->setEngineers(_baseFrom->getEngineers() - i->amount);
				t = new Transfer(time);
				t->setEngineers(i->amount);
				_baseTo->getTransfers()->push_back(t);
				break;
			case TRANSFER_ITEM:
				_baseFrom->getStorageItems()->removeItem(((RuleItem*)i->rule)->getType(), i->amount);
				t = new Transfer(time);
				t->setItems(((RuleItem*)i->rule)->getType(), i->amount);
				_baseTo->getTransfers()->push_back(t);
				break;
			}
		}
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void TransferItemsState::btnCancelClick(Action *)
{
	_game->popState();
	_game->popState();
}

/**
 * Starts increasing the item.
 * @param action Pointer to an action.
 */
void TransferItemsState::lstItemsLeftArrowPress(Action *action)
{
	_sel = _lstItems->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && !_timerInc->isRunning()) _timerInc->start();
}

/**
 * Stops increasing the item.
 * @param action Pointer to an action.
 */
void TransferItemsState::lstItemsLeftArrowRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerInc->stop();
	}
}

/**
 * Increases the selected item;
 * by one on left-click; to max on right-click.
 * @param action Pointer to an action.
 */
void TransferItemsState::lstItemsLeftArrowClick(Action *action)
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
void TransferItemsState::lstItemsRightArrowPress(Action *action)
{
	_sel = _lstItems->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && !_timerDec->isRunning()) _timerDec->start();
}

/**
 * Stops decreasing the item.
 * @param action Pointer to an action.
 */
void TransferItemsState::lstItemsRightArrowRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerDec->stop();
	}
}

/**
 * Decreases the selected item;
 * by one on left-click; to 0 on right-click.
 * @param action Pointer to an action.
 */
void TransferItemsState::lstItemsRightArrowClick(Action *action)
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
void TransferItemsState::lstItemsMousePress(Action *action)
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
 * Increases the quantity of the selected item to transfer by one.
 */
void TransferItemsState::increase()
{
	_timerDec->setInterval(50);
	_timerInc->setInterval(50);
	increaseByValue(1);
}

/**
 * Increases the quantity of the selected item to transfer by "change".
 * @param change How much we want to add.
 */
void TransferItemsState::increaseByValue(int change)
{
	if (0 >= change || getRow().qtySrc <= getRow().amount) return;
	std::string errorMessage;
	RuleItem *selItem = 0;
	Craft *craft = 0;

	switch (getRow().type)
	{
	case TRANSFER_SOLDIER:
	case TRANSFER_SCIENTIST:
	case TRANSFER_ENGINEER:
		if (_pQty + 1 > _baseTo->getAvailableQuarters() - _baseTo->getUsedQuarters())
		{
			errorMessage = tr("STR_NO_FREE_ACCOMODATION");
		}
		break;
	case TRANSFER_CRAFT:
		craft = (Craft*)getRow().rule;
		if (_cQty + 1 > _baseTo->getAvailableHangars() - _baseTo->getUsedHangars())
		{
			errorMessage = tr("STR_NO_FREE_HANGARS_FOR_TRANSFER");
		}
		else if (_pQty + craft->getNumSoldiers() > _baseTo->getAvailableQuarters() - _baseTo->getUsedQuarters())
		{
			errorMessage = tr("STR_NO_FREE_ACCOMODATION_CREW");
		}
		else if (Options::storageLimitsEnforced && _baseTo->storesOverfull(_iQty + craft->getItems()->getTotalSize(_game->getMod())))
		{
			errorMessage = tr("STR_NOT_ENOUGH_STORE_SPACE_FOR_CRAFT");
		}
		break;
	case TRANSFER_ITEM:
		selItem = (RuleItem*)getRow().rule;
		if (!selItem->isAlien() && _baseTo->storesOverfull(selItem->getSize() + _iQty))
		{
			errorMessage = tr("STR_NOT_ENOUGH_STORE_SPACE");
		}
		else if (selItem->isAlien() && Options::storageLimitsEnforced * _aQty + 1 > _baseTo->getAvailableContainment() - Options::storageLimitsEnforced * _baseTo->getUsedContainment())
		{
			errorMessage = tr("STR_NO_ALIEN_CONTAINMENT_FOR_TRANSFER");
		}
		break;
	}

	if (errorMessage.empty())
	{
		int freeQuarters = _baseTo->getAvailableQuarters() - _baseTo->getUsedQuarters() - _pQty;
		switch (getRow().type)
		{
		case TRANSFER_SOLDIER:
		case TRANSFER_SCIENTIST:
		case TRANSFER_ENGINEER:
			change = std::min(std::min(freeQuarters, getRow().qtySrc - getRow().amount), change);
			_pQty += change;
			getRow().amount += change;
			_total += getRow().cost * change;
			break;
		case TRANSFER_CRAFT:
			_cQty++;
			_pQty += craft->getNumSoldiers();
			_iQty += craft->getItems()->getTotalSize(_game->getMod());
			getRow().amount++;
			if (!Options::canTransferCraftsWhileAirborne || craft->getStatus() != "STR_OUT")
				_total += getRow().cost;
			break;
		case TRANSFER_ITEM:
			if (!selItem->isAlien())
			{
				double storesNeededPerItem = ((RuleItem*)getRow().rule)->getSize();
				double freeStores = _baseTo->getAvailableStores() - _baseTo->getUsedStores() - _iQty;
				double freeStoresForItem = (double)(INT_MAX);
				if (!AreSame(storesNeededPerItem, 0.0))
				{
					freeStoresForItem = (freeStores + 0.05) / storesNeededPerItem;
				}
				change = std::min(std::min((int)freeStoresForItem, getRow().qtySrc - getRow().amount), change);
				_iQty += change * storesNeededPerItem;
				getRow().amount += change;
				_total += getRow().cost * change;
			}
			else
			{
				int freeContainment = Options::storageLimitsEnforced ? _baseTo->getAvailableContainment() - _baseTo->getUsedContainment() - _aQty : INT_MAX;
				change = std::min(std::min(freeContainment, getRow().qtySrc - getRow().amount), change);
				_aQty += change;
				getRow().amount += change;
				_total += getRow().cost * change;
			}
			break;
		}
		updateItemStrings();
	}
	else
	{
		_timerInc->stop();
		RuleInterface *menuInterface = _game->getMod()->getInterface("transferMenu");
		_game->pushState(new ErrorMessageState(errorMessage, _palette, menuInterface->getElement("errorMessage")->color, "BACK13.SCR", menuInterface->getElement("errorPalette")->color));
	}
}

/**
 * Decreases the quantity of the selected item to transfer by one.
 */
void TransferItemsState::decrease()
{
	_timerInc->setInterval(50);
	_timerDec->setInterval(50);
	decreaseByValue(1);
}

/**
 * Decreases the quantity of the selected item to transfer by "change".
 * @param change How much we want to remove.
 */
void TransferItemsState::decreaseByValue(int change)
{
	if (0 >= change || 0 >= getRow().amount) return;
	Craft *craft = 0;
	change = std::min(getRow().amount, change);

	switch (getRow().type)
	{
	case TRANSFER_SOLDIER:
	case TRANSFER_SCIENTIST:
	case TRANSFER_ENGINEER:
		_pQty -= change;
		break;
	case TRANSFER_CRAFT:
		craft = (Craft*)getRow().rule;
		_cQty--;
		_pQty -= craft->getNumSoldiers();
		_iQty -= craft->getItems()->getTotalSize(_game->getMod());
		break;
	case TRANSFER_ITEM:
		const RuleItem *selItem = (RuleItem*)getRow().rule;
		if (!selItem->isAlien())
		{
			_iQty -= selItem->getSize() * change;
		}
		else
		{
			_aQty -= change;
		}
		break;
	}
	getRow().amount -= change;
	if (!Options::canTransferCraftsWhileAirborne || 0 == craft || craft->getStatus() != "STR_OUT")
		_total -= getRow().cost * change;
	updateItemStrings();
}

/**
 * Updates the quantity-strings of the selected item.
 */
void TransferItemsState::updateItemStrings()
{
	std::ostringstream ss1, ss2;
	ss1 << getRow().qtySrc - getRow().amount;
	ss2 << getRow().amount;
	_lstItems->setCellText(_sel, 1, ss1.str());
	_lstItems->setCellText(_sel, 2, ss2.str());

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
}

/**
 * Gets the total cost of the current transfer.
 * @return Total cost.
 */
int TransferItemsState::getTotal() const
{
	return _total;
}

/**
 * Gets the shortest distance between the two bases.
 * @return Distance.
 */
double TransferItemsState::getDistance() const
{
	double x[3], y[3], z[3], r = 51.2;
	Base *base = _baseFrom;
	for (int i = 0; i < 2; ++i) {
		x[i] = r * cos(base->getLatitude()) * cos(base->getLongitude());
		y[i] = r * cos(base->getLatitude()) * sin(base->getLongitude());
		z[i] = r * -sin(base->getLatitude());
		base = _baseTo;
	}
	x[2] = x[1] - x[0];
	y[2] = y[1] - y[0];
	z[2] = z[1] - z[0];
	return sqrt(x[2] * x[2] + y[2] * y[2] + z[2] * z[2]);
}

/**
* Updates the production list to match the category filter.
*/
void TransferItemsState::cbxCategoryChange(Action *)
{
	updateList();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool TransferItemsState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> TransferItemsState::agentPaintRevisions() const
{
    return {_txtTitle->agentTextRevision(), _txtQuantity->agentTextRevision(), _txtAmountTransfer->agentTextRevision(), _txtAmountDestination->agentTextRevision(), _btnOk->agentLabelRevision(), _btnCancel->agentLabelRevision(), _lstItems->agentListRevision(), _cbxCategory->agentUiRevision()};
}

void TransferItemsState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool TransferItemsState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<TransferItemsState *>(this)) || _modal || _cbxCategory->agentPopupOpen() || _timerInc->isRunning() || _timerDec->isRunning() || !agentPaintHealthy() || !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    out = AgentUiState(); out.actor = _baseFrom; out.save = _baseTo;
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
    out.screen = "transfer_items";
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        const auto value = agentTextJson(widget);
        if (value == "null" || !agentWidgetUncovered(widget, painted)) return "null";
        return widget->getText().empty() || include(widget) ? value : "null";
    };
    const auto title = text(_txtTitle);
    const auto headings = "[null," + text(_txtQuantity) + "," + text(_txtAmountTransfer) + "," + text(_txtAmountDestination) + "]";
    std::string buttons = "[";
    const auto button = [&](TextButton *widget, const char *id, const char *action, const char *kind) {
        out.widgets.emplace_back(widget, widget->agentLabelRevision());
        const auto shown = agentButtonLabel(widget);
        if (shown.empty() || shown.size() > 1024 || !include(widget)) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(shown) + "}";
        out.actions.emplace_back(action, shown, "visible_text", kind);
    };
    button(_btnOk, "review", "review_transfer", "none");
    button(_btnCancel, "cancel", "cancel_transfer_items", "none"); buttons += "]";
    const Surface *up = nullptr, *down = nullptr;
    _lstItems->agentScrollControls(up, down);
    const bool canUp = up && include(up), canDown = down && include(down);
    out.context = {canUp ? 1u : 0u, canDown ? 1u : 0u};
    const std::string scroll = std::string("{\"up\":") + (canUp ? "true" : "false") + ",\"down\":" + (canDown ? "true" : "false") + "}";
    if (canUp || canDown) out.actions.emplace_back("scroll_visible_list", "Scroll visible list", "visible_control_semantics", "scroll_direction");
    const std::string prefix = "{\"title\":" + title + ",\"headings\":" + headings + ",\"rows\":";
    const std::string suffix = ",\"scroll\":" + scroll + ",\"buttons\":" + buttons + "}";
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
    if (!out.rows.empty()) out.actions.emplace_back("change_transfer_quantity", "Change transfer quantity", "visible_control_semantics", "transfer_quantity");
    out.visible = prefix + shownRows + suffix;
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size() <= 9000;
}

bool TransferItemsState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "review_transfer") { btnOkClick(nullptr); return true; }
    if (request.action == "cancel_transfer_items") { btnCancelClick(nullptr); return true; }
    if (request.action == "scroll_visible_list" && current.context.size() >= 2)
    {
        if (request.choice == "up" && current.context[0]) { _lstItems->scrollUp(false,false); return true; }
        if (request.choice == "down" && current.context[1]) { _lstItems->scrollDown(false,false); return true; }
        return false;
    }
    if (request.action != "change_transfer_quantity" || request.choice.size() < 2 || request.choice.size() > 3 || request.choice[0] != 'r' ||
        (request.choice.size() == 3 && request.choice[1] == '0')) return false;
    unsigned index = 0;
    for (size_t i=1; i<request.choice.size(); ++i)
    { if (request.choice[i]<'0' || request.choice[i]>'9') return false; index=index*10+request.choice[i]-'0'; }
    if (index >= current.rows.size() || current.context.size() != current.rows.size()+2 || current.rows[index] >= _rows.size()) return false;
    unsigned required;
    if (request.operation == "adjust" && request.value && request.value >= -1000 && request.value <= 1000) required=request.value>0 ? 1:2;
    else if (request.operation == "set" && request.value >= 0 && request.value <= 100000) required=3;
    else if (request.operation == "max") required=1;
    else if (request.operation == "clear") required=2;
    else return false;
    if ((current.context[index+2] & required) != required) return false;
    _sel = current.rows[index];
    // Delegate to ordinary UI controller rules: this is a request, not a promise
    // that all units were accepted. The next rendered state proves the result.
    if (request.operation == "max") increaseByValue(INT_MAX);
    else if (request.operation == "clear") decreaseByValue(INT_MAX);
    else {
        const int delta = request.operation == "set" ? request.value - getRow().amount : request.value;
        if (delta > 0) increaseByValue(delta);
        else if (delta < 0) decreaseByValue(-delta);
    }
    return true;
}
#endif

}
