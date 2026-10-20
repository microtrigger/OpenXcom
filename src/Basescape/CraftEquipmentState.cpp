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
#include "CraftEquipmentState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentOutfitting.h"
#endif
#include "../Savegame/CraftOutfittingCommand.h"
#include <climits>
#include <sstream>
#include <algorithm>
#include "../Engine/Screen.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Timer.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Mod/Armor.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Mod/RuleCraft.h"
#include "../Savegame/ItemContainer.h"
#include "../Mod/RuleItem.h"
#include "../Savegame/Vehicle.h"
#include "../Savegame/SavedGame.h"
#include "../Menu/ErrorMessageState.h"
#include "../Battlescape/InventoryState.h"
#include "../Battlescape/BattlescapeGenerator.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Mod/RuleInterface.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Craft Equipment screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param craft ID of the selected craft.
 */
CraftEquipmentState::CraftEquipmentState(Base *base, size_t craft) : _sel(0), _craft(craft), _base(base), _totalItems(0), _ammoColor(0)
{
	Craft *c = _base->getCrafts()->at(_craft);
	bool craftHasACrew = c->getNumSoldiers() > 0;
	bool isNewBattle = _game->getSavedGame()->getMonthsPassed() == -1;

	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton((craftHasACrew || isNewBattle)? 148:288, 16, (craftHasACrew || isNewBattle)? 164:16, 176);
	_btnClear = new TextButton(148, 16, 8, 176);
	_btnInventory = new TextButton(148, 16, 8, 176);
	_txtTitle = new Text(300, 17, 16, 7);
	_txtItem = new Text(144, 9, 16, 32);
	_txtStores = new Text(150, 9, 160, 32);
	_txtAvailable = new Text(110, 9, 16, 24);
	_txtUsed = new Text(110, 9, 130, 24);
	_txtCrew = new Text(71, 9, 244, 24);
	_lstEquipment = new TextList(288, 128, 8, 40);

	// Set palette
	setInterface("craftEquipment");

	_ammoColor = _game->getMod()->getInterface("craftEquipment")->getElement("ammoColor")->color;

	add(_window, "window", "craftEquipment");
	add(_btnOk, "button", "craftEquipment");
	add(_btnClear, "button", "craftEquipment");
	add(_btnInventory, "button", "craftEquipment");
	add(_txtTitle, "text", "craftEquipment");
	add(_txtItem, "text", "craftEquipment");
	add(_txtStores, "text", "craftEquipment");
	add(_txtAvailable, "text", "craftEquipment");
	add(_txtUsed, "text", "craftEquipment");
	add(_txtCrew, "text", "craftEquipment");
	add(_lstEquipment, "list", "craftEquipment");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK04.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&CraftEquipmentState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&CraftEquipmentState::btnOkClick, Options::keyCancel);

	_btnClear->setText(tr("STR_UNLOAD_CRAFT"));
	_btnClear->onMouseClick((ActionHandler)&CraftEquipmentState::btnClearClick);
	_btnClear->setVisible(isNewBattle);

	_btnInventory->setText(tr("STR_INVENTORY"));
	_btnInventory->onMouseClick((ActionHandler)&CraftEquipmentState::btnInventoryClick);
	_btnInventory->setVisible(craftHasACrew && !isNewBattle);

	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_EQUIPMENT_FOR_CRAFT").arg(c->getName(_game->getLanguage())));

	_txtItem->setText(tr("STR_ITEM"));

	_txtStores->setText(tr("STR_STORES"));

	_txtAvailable->setText(tr("STR_SPACE_AVAILABLE").arg(c->getSpaceAvailable()));

	_txtUsed->setText(tr("STR_SPACE_USED").arg(c->getSpaceUsed()));

	std::ostringstream ss3;
	ss3 << tr("STR_SOLDIERS_UC") << ">" << Unicode::TOK_COLOR_FLIP << c->getNumSoldiers();
	_txtCrew->setText(ss3.str());

	_lstEquipment->setArrowColumn(203, ARROW_HORIZONTAL);
	_lstEquipment->setColumns(3, 156, 83, 41);
	_lstEquipment->setSelectable(true);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	_lstEquipment->agentEnableOutfitting(3, true, false);
#endif
	_lstEquipment->setBackground(_window);
	_lstEquipment->setMargin(8);
	_lstEquipment->onLeftArrowPress((ActionHandler)&CraftEquipmentState::lstEquipmentLeftArrowPress);
	_lstEquipment->onLeftArrowRelease((ActionHandler)&CraftEquipmentState::lstEquipmentLeftArrowRelease);
	_lstEquipment->onLeftArrowClick((ActionHandler)&CraftEquipmentState::lstEquipmentLeftArrowClick);
	_lstEquipment->onRightArrowPress((ActionHandler)&CraftEquipmentState::lstEquipmentRightArrowPress);
	_lstEquipment->onRightArrowRelease((ActionHandler)&CraftEquipmentState::lstEquipmentRightArrowRelease);
	_lstEquipment->onRightArrowClick((ActionHandler)&CraftEquipmentState::lstEquipmentRightArrowClick);
	_lstEquipment->onMousePress((ActionHandler)&CraftEquipmentState::lstEquipmentMousePress);

	int row = 0;
	const std::vector<std::string> &items = _game->getMod()->getItemsList();
	for (std::vector<std::string>::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		RuleItem *rule = _game->getMod()->getItem(*i);
		int cQty = 0;
		if (rule->isFixed())
		{
			cQty = c->getVehicleCount(*i);
		}
		else
		{
			cQty = c->getItems()->getItem(*i);
			_totalItems += cQty;
		}
		if (rule->getBigSprite() > -1 && rule->getBattleType() != BT_NONE && rule->getBattleType() != BT_CORPSE &&
			_game->getSavedGame()->isResearched(rule->getRequirements()) &&
			(_base->getStorageItems()->getItem(*i) > 0 || cQty > 0))
		{
			_items.push_back(*i);
			std::ostringstream ss, ss2;
			if (_game->getSavedGame()->getMonthsPassed() > -1)
			{
				ss << _base->getStorageItems()->getItem(*i);
			}
			else
			{
				ss << "-";
			}
			ss2 << cQty;

			std::string s = tr(*i);
			if (rule->getBattleType() == BT_AMMO)
			{
				s.insert(0, "  ");
			}
			_lstEquipment->addRow(3, s.c_str(), ss.str().c_str(), ss2.str().c_str());

			Uint8 color;
			if (cQty == 0)
			{
				if (rule->getBattleType() == BT_AMMO)
				{
					color = _ammoColor;
				}
				else
				{
					color = _lstEquipment->getColor();
				}
			}
			else
			{
					color = _lstEquipment->getSecondaryColor();
			}
			_lstEquipment->setRowColor(row, color);

			++row;
		}
	}

	_timerLeft = new Timer(250);
	_timerLeft->onTimer((StateHandler)&CraftEquipmentState::moveLeft);
	_timerRight = new Timer(250);
	_timerRight->onTimer((StateHandler)&CraftEquipmentState::moveRight);
}

/**
 *
 */
CraftEquipmentState::~CraftEquipmentState()
{
	delete _timerLeft;
	delete _timerRight;
}

/**
 * Resets the savegame when coming back from the inventory.
 */
void CraftEquipmentState::init()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentUiRevision;
#endif
	State::init();

	_game->getSavedGame()->setBattleGame(0);

	Craft *c = _base->getCrafts()->at(_craft);
	c->setInBattlescape(false);
}

/**
 * Runs the arrow timers.
 */
void CraftEquipmentState::think()
{
	State::think();

	_timerLeft->think(this, 0);
	_timerRight->think(this, 0);
}


/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void CraftEquipmentState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Starts moving the item to the base.
 * @param action Pointer to an action.
 */
void CraftEquipmentState::lstEquipmentLeftArrowPress(Action *action)
{
	_sel = _lstEquipment->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && !_timerLeft->isRunning()) _timerLeft->start();
}

/**
 * Stops moving the item to the base.
 * @param action Pointer to an action.
 */
void CraftEquipmentState::lstEquipmentLeftArrowRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerLeft->stop();
	}
}

/**
 * Moves all the items to the base on right-click.
 * @param action Pointer to an action.
 */
void CraftEquipmentState::lstEquipmentLeftArrowClick(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT) moveLeftByValue(INT_MAX);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		moveLeftByValue(1);
		_timerRight->setInterval(250);
		_timerLeft->setInterval(250);
	}
}

/**
 * Starts moving the item to the craft.
 * @param action Pointer to an action.
 */
void CraftEquipmentState::lstEquipmentRightArrowPress(Action *action)
{
	_sel = _lstEquipment->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && !_timerRight->isRunning()) _timerRight->start();
}

/**
 * Stops moving the item to the craft.
 * @param action Pointer to an action.
 */
void CraftEquipmentState::lstEquipmentRightArrowRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerRight->stop();
	}
}

/**
 * Moves all the items (as much as possible) to the craft on right-click.
 * @param action Pointer to an action.
 */
void CraftEquipmentState::lstEquipmentRightArrowClick(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT) moveRightByValue(INT_MAX);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		moveRightByValue(1);
		_timerRight->setInterval(250);
		_timerLeft->setInterval(250);
	}
}

/**
 * Handles the mouse-wheels on the arrow-buttons.
 * @param action Pointer to an action.
 */
void CraftEquipmentState::lstEquipmentMousePress(Action *action)
{
	_sel = _lstEquipment->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
	{
		_timerRight->stop();
		_timerLeft->stop();
		if (action->getAbsoluteXMouse() >= _lstEquipment->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstEquipment->getArrowsRightEdge())
		{
			moveRightByValue(Options::changeValueByMouseWheel);
		}
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
	{
		_timerRight->stop();
		_timerLeft->stop();
		if (action->getAbsoluteXMouse() >= _lstEquipment->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstEquipment->getArrowsRightEdge())
		{
			moveLeftByValue(Options::changeValueByMouseWheel);
		}
	}
}

/**
 * Updates the displayed quantities of the
 * selected item on the list.
 */
void CraftEquipmentState::updateQuantity()
{
	Craft *c = _base->getCrafts()->at(_craft);
	RuleItem *item = _game->getMod()->getItem(_items[_sel], true);
	int cQty = 0;
	if (item->isFixed())
	{
		cQty = c->getVehicleCount(_items[_sel]);
	}
	else
	{
		cQty = c->getItems()->getItem(_items[_sel]);
	}
	std::ostringstream ss, ss2;
	if (_game->getSavedGame()->getMonthsPassed() > -1)
	{
		ss << _base->getStorageItems()->getItem(_items[_sel]);
	}
	else
	{
		ss << "-";
	}
	ss2 << cQty;

	Uint8 color;
	if (cQty == 0)
	{
		RuleItem *rule = _game->getMod()->getItem(_items[_sel], true);
		if (rule->getBattleType() == BT_AMMO)
		{
			color = _ammoColor;
		}
		else
		{
			color = _lstEquipment->getColor();
		}
	}
	else
	{
		color = _lstEquipment->getSecondaryColor();
	}
	_lstEquipment->setRowColor(_sel, color);
	_lstEquipment->setCellText(_sel, 1, ss.str());
	_lstEquipment->setCellText(_sel, 2, ss2.str());

	_txtAvailable->setText(tr("STR_SPACE_AVAILABLE").arg(c->getSpaceAvailable()));
	_txtUsed->setText(tr("STR_SPACE_USED").arg(c->getSpaceUsed()));
}

/**
 * Moves the selected item to the base.
 */
void CraftEquipmentState::moveLeft()
{
	_timerLeft->setInterval(50);
	_timerRight->setInterval(50);
	moveLeftByValue(1);
}

/**
 * Moves the given number of items (selected) to the base.
 * @param change Item difference.
 */
void CraftEquipmentState::moveLeftByValue(int change)
{
    if (change > 0) changeEquipmentQuantity(_sel, QuantityOperation::Adjust, -change);
}

/**
 * Moves the selected item to the craft.
 */
void CraftEquipmentState::moveRight()
{
	_timerLeft->setInterval(50);
	_timerRight->setInterval(50);
	moveRightByValue(1);
}

/**
 * Moves the given number of items (selected) to the craft.
 * @param change Item difference.
 */
void CraftEquipmentState::moveRightByValue(int change)
{
    if (change > 0) changeEquipmentQuantity(_sel, QuantityOperation::Adjust, change);
}

/**
 * Applies an immediate ordinary transfer intent, shared by the GUI and semantic
 * adapter. The warning sink preserves the original popup-before-partial-load
 * sequencing; OK remains only a close operation.
 */
void CraftEquipmentState::changeEquipmentQuantity(size_t row, QuantityOperation operation, int value)
{
    if (row >= _items.size()) return;
    _sel = row;
    Craft *craft = _base->getCrafts()->at(_craft);
    const CraftEquipmentWarningSink warning = [this](CraftEquipmentWarning kind, int amount, const std::string &ammo) {
        _timerRight->stop();
        LocalizedText message = kind == CraftEquipmentWarning::MaxItems
            ? tr("STR_NO_MORE_EQUIPMENT_ALLOWED", amount)
            : tr("STR_NOT_ENOUGH_AMMO_TO_ARM_HWP").arg(amount).arg(tr(ammo));
        _game->pushState(new ErrorMessageState(message, _palette,
            _game->getMod()->getInterface("craftEquipment")->getElement("errorMessage")->color,
            "BACK04.SCR", _game->getMod()->getInterface("craftEquipment")->getElement("errorPalette")->color));
    };
    if (CraftOutfittingCommand::quantity(*_base, *craft, *_game->getMod(), *_game->getSavedGame(),
            _items[row], _totalItems, operation, value, warning))
        updateQuantity();
}

/**
 * Empties the contents of the craft, moving all of the items back to the base.
 */
void CraftEquipmentState::btnClearClick(Action *)
{
	for (_sel = 0; _sel != _items.size(); ++_sel)
	{
		moveLeftByValue(INT_MAX);
	}
}

/**
 * Displays the inventory screen for the soldiers
 * inside the craft.
 * @param action Pointer to an action.
 */
void CraftEquipmentState::btnInventoryClick(Action *)
{
	Craft *craft = _base->getCrafts()->at(_craft);
	if (craft->getNumSoldiers() != 0)
	{
		SavedBattleGame *bgame = new SavedBattleGame();
		_game->getSavedGame()->setBattleGame(bgame);

		BattlescapeGenerator bgen = BattlescapeGenerator(_game);
		bgen.runInventory(craft);

		_game->getScreen()->clear();
		_game->pushState(new InventoryState(false, 0));
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void CraftEquipmentState::blit()
{
    _agentDestinationPaintFull=false;_agentStoredPaintRevisions.clear();
    const bool before=Observer::agentOutfittingDestinationFull(_game);
    State::blit();
    _agentStoredPaintRevisions=Observer::agentOutfittingPaintRevisions(_surfaces);
    _agentDestinationPaintFull=before && Observer::agentOutfittingDestinationFull(_game) && Observer::agentPaintHealthy();
}
bool CraftEquipmentState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<CraftEquipmentState *>(this)) || _modal || !_base || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentOutfittingDestinationFull(_game) ||
        _agentStoredPaintRevisions!=agentOutfittingPaintRevisions(_surfaces)) return false;
    if (_craft>=_base->getCrafts()->size()) return false;
    out=AgentUiState();out.screen="craft_equipment";out.actor=_base->getCrafts()->at(_craft);out.save=_game->getSavedGame();out.uiRevision=_agentUiRevision;
    out.widgets.emplace_back(_base,0);
    AgentOutfittingView view(_game,_surfaces,out);
    const auto title=view.text(_txtTitle),available=view.text(_txtAvailable),used=view.text(_txtUsed);
    const auto crew=view.text(_txtCrew);
    const std::string headings="["+view.text(_txtItem)+","+view.text(_txtStores)+"]";
    AgentListProjection list;if(!view.list(_lstEquipment,3,true,list))return false;
    for(size_t row:out.rows){if(row>=_items.size())return false;out.widgets.emplace_back(&_items[row],0);}
    if(!out.rows.empty())out.actions.emplace_back("change_craft_equipment","Change craft equipment","visible_control_semantics","equipment_quantity");
    view.button(_btnOk,"close","close_craft_equipment");
    out.visible="{\"title\":"+title+",\"headings\":"+headings+",\"available\":"+available+",\"used\":"+used+",\"crew\":"+crew+",\"rows\":"+list.rows+",\"scroll\":"+list.scroll+",\"buttons\":"+view.buttons+"]}";

    return view.finish();
}
bool CraftEquipmentState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if(request.action=="close_craft_equipment"){btnOkClick(nullptr);return true;}
    if(Observer::agentOutfittingScroll(_lstEquipment,current,request))return true;
    size_t row=0,slot=0;
    if(request.action=="change_craft_equipment" && Observer::agentOutfittingRow(current,request,row,&slot) && slot+2<current.context.size()) {
        const unsigned mask=current.context[slot];
        QuantityOperation operation;
        if(request.operation=="adjust") {if(!(mask&(request.value>0?1:2)))return false;operation=QuantityOperation::Adjust;}
        else if(request.operation=="set") {if(mask!=3)return false;operation=QuantityOperation::Set;}
        else if(request.operation=="max") {if(!(mask&1))return false;operation=QuantityOperation::Maximum;}
        else if(request.operation=="clear") {if(!(mask&2))return false;operation=QuantityOperation::Clear;}
        else return false;
        if(!_lstEquipment->agentSelectOutfittingRow(row))return false;
        changeEquipmentQuantity(row,operation,request.value);return true;
    }

    return false;
}
#endif

}
