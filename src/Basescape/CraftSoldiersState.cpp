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
#include "CraftSoldiersState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentOutfitting.h"
#endif
#include "../Savegame/CraftOutfittingCommand.h"
#include <algorithm>
#include <functional>
#include <climits>
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/ComboBox.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/Craft.h"
#include "../Savegame/SavedGame.h"
#include "SoldierInfoState.h"
#include "../Mod/RuleInterface.h"

namespace OpenXcom
{

typedef int (*getStatFn_t)(Game *, Soldier *);

struct SortFunctor : public std::binary_function<Soldier *, Soldier *, bool>
{
	Game *_game;
	getStatFn_t _getStatFn;
	SortFunctor(Game *game, getStatFn_t getStatFn)
		: _game(game), _getStatFn(getStatFn) { }
	bool operator()(Soldier *a, Soldier *b)
	{
		bool ret = _getStatFn(_game, a) < _getStatFn(_game, b);
		return ret;
	}
};

#define GET_ATTRIB_STAT_FN(attrib) \
	int attrib##Stat(Game *, Soldier *s) { return s->getCurrentStats()->attrib; }
GET_ATTRIB_STAT_FN(tu)
GET_ATTRIB_STAT_FN(stamina)
GET_ATTRIB_STAT_FN(health)
GET_ATTRIB_STAT_FN(bravery)
GET_ATTRIB_STAT_FN(reactions)
GET_ATTRIB_STAT_FN(firing)
GET_ATTRIB_STAT_FN(throwing)
GET_ATTRIB_STAT_FN(strength)
int psiStrengthStat(Game *game, Soldier *s)
{
	// don't reveal (relative) psi strength before it would otherwise be known
	if (s->getCurrentStats()->psiSkill > 0
	 || (Options::psiStrengthEval
	     && game->getSavedGame()->isResearched(game->getMod()->getPsiRequirements())))
	{
		return s->getCurrentStats()->psiStrength;
	}
	return 0;
}
int psiSkillStat(Game *, Soldier *s)
{
	// protect against negative psiSkill (possible when Options::anytimePsiTraining
	// is enabled)
	return (s->getCurrentStats()->psiSkill > 0) ? s->getCurrentStats()->psiSkill : 0;
}
GET_ATTRIB_STAT_FN(melee)
#undef GET_ATTRIB_STAT_FN
#define GET_SOLDIER_STAT_FN(attrib, camelCaseAttrib) \
	int attrib##Stat(Game *, Soldier *s) { return s->get##camelCaseAttrib(); }
GET_SOLDIER_STAT_FN(rank, Rank)
GET_SOLDIER_STAT_FN(missions, Missions)
GET_SOLDIER_STAT_FN(kills, Kills)
GET_SOLDIER_STAT_FN(woundRecovery, WoundRecovery)
#undef GET_SOLDIER_STAT_FN

/**
 * Initializes all the elements in the Craft Soldiers screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param craft ID of the selected craft.
 */
CraftSoldiersState::CraftSoldiersState(Base *base, size_t craft)
		:  _base(base), _craft(craft), _otherCraftColor(0), _origSoldierOrder(*_base->getSoldiers())
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(148, 16, 164, 176);
	_txtTitle = new Text(300, 17, 16, 7);
	_txtName = new Text(114, 9, 16, 32);
	_txtRank = new Text(102, 9, 122, 32);
	_txtCraft = new Text(84, 9, 224, 32);
	_txtAvailable = new Text(110, 9, 16, 24);
	_txtUsed = new Text(110, 9, 122, 24);
	_cbxSortBy = new ComboBox(this, 148, 16, 8, 176, true);
	_lstSoldiers = new TextList(288, 128, 8, 40);

	// Set palette
	setInterface("craftSoldiers");

	add(_window, "window", "craftSoldiers");
	add(_btnOk, "button", "craftSoldiers");
	add(_txtTitle, "text", "craftSoldiers");
	add(_txtName, "text", "craftSoldiers");
	add(_txtRank, "text", "craftSoldiers");
	add(_txtCraft, "text", "craftSoldiers");
	add(_txtAvailable, "text", "craftSoldiers");
	add(_txtUsed, "text", "craftSoldiers");
	add(_lstSoldiers, "list", "craftSoldiers");
	add(_cbxSortBy, "button", "craftSoldiers");

	_otherCraftColor = _game->getMod()->getInterface("craftSoldiers")->getElement("otherCraft")->color;

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK02.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&CraftSoldiersState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&CraftSoldiersState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();
	Craft *c = _base->getCrafts()->at(_craft);
	_txtTitle->setText(tr("STR_SELECT_SQUAD_FOR_CRAFT").arg(c->getName(_game->getLanguage())));

	_txtName->setText(tr("STR_NAME_UC"));

	_txtRank->setText(tr("STR_RANK"));

	_txtCraft->setText(tr("STR_CRAFT"));

	// populate sort options
	std::vector<std::string> sortOptions;
	sortOptions.push_back(tr("STR_ORIGINAL_ORDER"));
	_sortFunctors.push_back(NULL);

#define PUSH_IN(strId, functor) \
	sortOptions.push_back(tr(strId)); \
	_sortFunctors.push_back(new SortFunctor(_game, functor));

	PUSH_IN("STR_RANK", rankStat);
	PUSH_IN("STR_MISSIONS2", missionsStat);
	PUSH_IN("STR_KILLS2", killsStat);
	PUSH_IN("STR_WOUND_RECOVERY2", woundRecoveryStat);
	PUSH_IN("STR_TIME_UNITS", tuStat);
	PUSH_IN("STR_STAMINA", staminaStat);
	PUSH_IN("STR_HEALTH", healthStat);
	PUSH_IN("STR_BRAVERY", braveryStat);
	PUSH_IN("STR_REACTIONS", reactionsStat);
	PUSH_IN("STR_FIRING_ACCURACY", firingStat);
	PUSH_IN("STR_THROWING_ACCURACY", throwingStat);
	PUSH_IN("STR_STRENGTH", strengthStat);

	// don't show psionic sort options until they actually have data they can use
	bool showPsiStrength = Options::psiStrengthEval
			&& _game->getSavedGame()->isResearched(_game->getMod()->getPsiRequirements());
	bool showPsiSkill = false;
	for (std::vector<Soldier*>::iterator i = _base->getSoldiers()->begin(); i != _base->getSoldiers()->end(); ++i)
	{
		if ((*i)->getCurrentStats()->psiSkill > 0)
		{
			showPsiStrength = true;
			showPsiSkill = true;
			break;
		}
	}
	if (showPsiStrength)
	{
		PUSH_IN("STR_PSIONIC_STRENGTH", psiStrengthStat);
	}
	if (showPsiSkill)
	{
		PUSH_IN("STR_PSIONIC_SKILL", psiSkillStat);
	}

	PUSH_IN("STR_MELEE_ACCURACY", meleeStat);

#undef PUSH_IN

	_cbxSortBy->setOptions(sortOptions);
	_cbxSortBy->setSelected(0);
	_cbxSortBy->onChange((ActionHandler)&CraftSoldiersState::cbxSortByChange);
	_cbxSortBy->setText(tr("STR_SORT_BY"));

	_lstSoldiers->setArrowColumn(192, ARROW_VERTICAL);
	_lstSoldiers->setColumns(3, 106, 102, 72);
	_lstSoldiers->setSelectable(true);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	_lstSoldiers->agentEnableOutfitting(3, false, true);
#endif
	_lstSoldiers->setBackground(_window);
	_lstSoldiers->setMargin(8);
	_lstSoldiers->onLeftArrowClick((ActionHandler)&CraftSoldiersState::lstItemsLeftArrowClick);
	_lstSoldiers->onRightArrowClick((ActionHandler)&CraftSoldiersState::lstItemsRightArrowClick);
	_lstSoldiers->onMouseClick((ActionHandler)&CraftSoldiersState::lstSoldiersClick, 0);
	_lstSoldiers->onMousePress((ActionHandler)&CraftSoldiersState::lstSoldiersMousePress);
}

/**
 * cleans up dynamic state
 */
CraftSoldiersState::~CraftSoldiersState()
{
	for (std::vector<SortFunctor *>::iterator it = _sortFunctors.begin();
		it != _sortFunctors.end(); ++it)
	{
		delete(*it);
	}
}

/**
 * Sorts the soldiers list by the selected criterion
 * @param action Pointer to an action.
 */
void CraftSoldiersState::cbxSortByChange(Action *)
{
	size_t selIdx = _cbxSortBy->getSelected();
	if (selIdx == (size_t)-1)
	{
		return;
	}

	SortFunctor *compFunc = _sortFunctors[selIdx];
	if (compFunc)
	{
		std::stable_sort(_base->getSoldiers()->begin(), _base->getSoldiers()->end(), *compFunc);
	}
	else
	{
		// restore original ordering, ignoring (of course) those
		// soldiers that have been sacked since this state started
		for (std::vector<Soldier *>::const_iterator it = _origSoldierOrder.begin();
			it != _origSoldierOrder.end(); ++it)
		{
			std::vector<Soldier *>::iterator soldierIt =
				std::find(_base->getSoldiers()->begin(), _base->getSoldiers()->end(), *it);
			if (soldierIt != _base->getSoldiers()->end())
			{
				Soldier *s = *soldierIt;
				_base->getSoldiers()->erase(soldierIt);
				_base->getSoldiers()->insert(_base->getSoldiers()->end(), s);
			}
		}
	}

	initList();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void CraftSoldiersState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Shows the soldiers in a list
 */
void CraftSoldiersState::initList()
{
	size_t originalScrollPos = _lstSoldiers->getScroll();
	int row = 0;
	_lstSoldiers->clearList();
	Craft *c = _base->getCrafts()->at(_craft);
	for (std::vector<Soldier*>::iterator i = _base->getSoldiers()->begin(); i != _base->getSoldiers()->end(); ++i)
	{
		_lstSoldiers->addRow(3, (*i)->getName(true, 19).c_str(), tr((*i)->getRankString()).c_str(), (*i)->getCraftString(_game->getLanguage()).c_str());

		Uint8 color;
		if ((*i)->getCraft() == c)
		{
			color = _lstSoldiers->getSecondaryColor();
		}
		else if ((*i)->getCraft() != 0)
		{
			color = _otherCraftColor;
		}
		else
		{
			color = _lstSoldiers->getColor();
		}
		_lstSoldiers->setRowColor(row, color);
		row++;
	}

	_lstSoldiers->draw();
	_lstSoldiers->scrollTo(originalScrollPos);

	_txtAvailable->setText(tr("STR_SPACE_AVAILABLE").arg(c->getSpaceAvailable()));
	_txtUsed->setText(tr("STR_SPACE_USED").arg(c->getSpaceUsed()));
}

/**
 * Shows the soldiers in a list.
 */
void CraftSoldiersState::init()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentUiRevision;
#endif
	State::init();
	initList();
}

/**
 * Reorders a soldier up.
 * @param action Pointer to an action.
 */
void CraftSoldiersState::lstItemsLeftArrowClick(Action *action)
{
	unsigned int row = _lstSoldiers->getSelectedRow();
	if (row > 0)
	{
		if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
		{
			moveSoldierUp(action, row);
		}
		else if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
		{
			moveSoldierUp(action, row, true);
		}
	}
	_cbxSortBy->setText(tr("STR_SORT_BY"));
	_cbxSortBy->setSelected(-1);
}

/**
 * Moves a soldier up on the list.
 * @param action Pointer to an action.
 * @param row Selected soldier row.
 * @param max Move the soldier to the top?
 */
void CraftSoldiersState::moveSoldierUp(Action *action, unsigned int row, bool max)
{
	Soldier *s = _base->getSoldiers()->at(row);
	if (max)
	{
		_base->getSoldiers()->erase(_base->getSoldiers()->begin() + row);
		_base->getSoldiers()->insert(_base->getSoldiers()->begin(), s);
	}
	else
	{
		_base->getSoldiers()->at(row) = _base->getSoldiers()->at(row - 1);
		_base->getSoldiers()->at(row - 1) = s;
		if (row != _lstSoldiers->getScroll())
		{
			SDL_WarpMouse(action->getLeftBlackBand() + action->getXMouse(), action->getTopBlackBand() + action->getYMouse() - static_cast<Uint16>(8 * action->getYScale()));
		}
		else
		{
			_lstSoldiers->scrollUp(false);
		}
	}
	initList();
}

/**
 * Reorders a soldier down.
 * @param action Pointer to an action.
 */
void CraftSoldiersState::lstItemsRightArrowClick(Action *action)
{
	unsigned int row = _lstSoldiers->getSelectedRow();
	size_t numSoldiers = _base->getSoldiers()->size();
	if (0 < numSoldiers && INT_MAX >= numSoldiers && row < numSoldiers - 1)
	{
		if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
		{
			moveSoldierDown(action, row);
		}
		else if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
		{
			moveSoldierDown(action, row, true);
		}
	}
	_cbxSortBy->setText(tr("STR_SORT_BY"));
	_cbxSortBy->setSelected(-1);
}

/**
 * Moves a soldier down on the list.
 * @param action Pointer to an action.
 * @param row Selected soldier row.
 * @param max Move the soldier to the bottom?
 */
void CraftSoldiersState::moveSoldierDown(Action *action, unsigned int row, bool max)
{
	Soldier *s = _base->getSoldiers()->at(row);
	if (max)
	{
		_base->getSoldiers()->erase(_base->getSoldiers()->begin() + row);
		_base->getSoldiers()->insert(_base->getSoldiers()->end(), s);
	}
	else
	{
		_base->getSoldiers()->at(row) = _base->getSoldiers()->at(row + 1);
		_base->getSoldiers()->at(row + 1) = s;
		if (row != _lstSoldiers->getVisibleRows() - 1 + _lstSoldiers->getScroll())
		{
			SDL_WarpMouse(action->getLeftBlackBand() + action->getXMouse(), action->getTopBlackBand() + action->getYMouse() + static_cast<Uint16>(8 * action->getYScale()));
		}
		else
		{
			_lstSoldiers->scrollDown(false);
		}
	}
	initList();
}

/**
 * Shows the selected soldier's info.
 * @param action Pointer to an action.
 */
void CraftSoldiersState::lstSoldiersClick(Action *action)
{
    double mx = action->getAbsoluteXMouse();
    if (mx >= _lstSoldiers->getArrowsLeftEdge() && mx < _lstSoldiers->getArrowsRightEdge()) return;
    const size_t row = _lstSoldiers->getSelectedRow();
    if (row >= _base->getSoldiers()->size()) return;
    if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
    {
        Craft *craft = _base->getCrafts()->at(_craft);
        setSoldierAssigned(row, _base->getSoldiers()->at(row)->getCraft() != craft);
    }
    else if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
        inspectSoldier(row);
}

/** Applies a desired assignment without changing the GUI's ordinary toggle. */
void CraftSoldiersState::setSoldierAssigned(size_t row, bool assigned)
{
    if (row >= _base->getSoldiers()->size()) return;
    Craft *craft = _base->getCrafts()->at(_craft);
    Soldier *soldier = _base->getSoldiers()->at(row);
    const CraftCrewChange result = CraftOutfittingCommand::setAssigned(*craft, *soldier, assigned);
    if (result == CraftCrewChange::Unchanged) return;
    Uint8 color = _lstSoldiers->getColor();
    if (result == CraftCrewChange::Removed)
        _lstSoldiers->setCellText(row, 2, tr("STR_NONE_UC"));
    else if (result == CraftCrewChange::OtherCraftAway)
        color = _otherCraftColor;
    else if (result == CraftCrewChange::Assigned)
    {
        _lstSoldiers->setCellText(row, 2, craft->getName(_game->getLanguage()));
        color = _lstSoldiers->getSecondaryColor();
    }
    _lstSoldiers->setRowColor(row, color);
    _txtAvailable->setText(tr("STR_SPACE_AVAILABLE").arg(craft->getSpaceAvailable()));
    _txtUsed->setText(tr("STR_SPACE_USED").arg(craft->getSpaceUsed()));
}

/** Opens the same detail state as the ordinary secondary row click. */
void CraftSoldiersState::inspectSoldier(size_t row)
{
    if (row < _base->getSoldiers()->size())
        _game->pushState(new SoldierInfoState(_base, row));
}

/**
 * Handles the mouse-wheels on the arrow-buttons.
 * @param action Pointer to an action.
 */
void CraftSoldiersState::lstSoldiersMousePress(Action *action)
{
	if (Options::changeValueByMouseWheel == 0)
		return;
	unsigned int row = _lstSoldiers->getSelectedRow();
	size_t numSoldiers = _base->getSoldiers()->size();
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP &&
		row > 0)
	{
		if (action->getAbsoluteXMouse() >= _lstSoldiers->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstSoldiers->getArrowsRightEdge())
		{
			moveSoldierUp(action, row);
		}
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN &&
			 0 < numSoldiers && INT_MAX >= numSoldiers && row < numSoldiers - 1)
	{
		if (action->getAbsoluteXMouse() >= _lstSoldiers->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstSoldiers->getArrowsRightEdge())
		{
			moveSoldierDown(action, row);
		}
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void CraftSoldiersState::blit()
{
    _agentDestinationPaintFull=false;_agentStoredPaintRevisions.clear();
    const bool before=Observer::agentOutfittingDestinationFull(_game);
    State::blit();
    _agentStoredPaintRevisions=Observer::agentOutfittingPaintRevisions(_surfaces);
    _agentDestinationPaintFull=before && Observer::agentOutfittingDestinationFull(_game) && Observer::agentPaintHealthy();
}
bool CraftSoldiersState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<CraftSoldiersState *>(this)) || _modal || !_base || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentOutfittingDestinationFull(_game) ||
        _agentStoredPaintRevisions!=agentOutfittingPaintRevisions(_surfaces)) return false;
    if (_craft>=_base->getCrafts()->size()) return false;
    out=AgentUiState();out.screen="craft_soldiers";out.actor=_base->getCrafts()->at(_craft);out.save=_game->getSavedGame();out.uiRevision=_agentUiRevision;
    out.widgets.emplace_back(_base,0);
    AgentOutfittingView view(_game,_surfaces,out);
    const auto title=view.text(_txtTitle),available=view.text(_txtAvailable),used=view.text(_txtUsed);
    const std::string headings="["+view.text(_txtName)+","+view.text(_txtRank)+","+view.text(_txtCraft)+"]";
    AgentListProjection list;if(!view.list(_lstSoldiers,3,false,list))return false;
    for(size_t row:out.rows){if(row>=_base->getSoldiers()->size())return false;out.widgets.emplace_back(_base->getSoldiers()->at(row),0);}
    if(!out.rows.empty()) {
        out.actions.emplace_back("set_craft_soldier","Set craft assignment","visible_control_semantics","crew_assignment");
        out.actions.emplace_back("open_craft_soldier_info","Inspect soldier","visible_control_semantics","outfitting_row");
    }
    view.button(_btnOk,"close","close_craft_crew");
    out.visible="{\"title\":"+title+",\"headings\":"+headings+",\"available\":"+available+",\"used\":"+used+",\"rows\":"+list.rows+",\"scroll\":"+list.scroll+",\"buttons\":"+view.buttons+"]}";

    return view.finish();
}
bool CraftSoldiersState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if(request.action=="close_craft_crew"){btnOkClick(nullptr);return true;}
    if(Observer::agentOutfittingScroll(_lstSoldiers,current,request))return true;
    size_t row=0;
    if((request.action=="set_craft_soldier" || request.action=="open_craft_soldier_info") &&
        Observer::agentOutfittingRow(current,request,row) && _lstSoldiers->agentSelectOutfittingRow(row)) {
        if(request.action=="set_craft_soldier")setSoldierAssigned(row,request.desired);else inspectSoldier(row);
        return true;
    }

    return false;
}
#endif

}
