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
#include "ManageAlienContainmentState.h"
#include <climits>
#include <sstream>
#include <algorithm>
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/ItemContainer.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/Armor.h"
#include "../Engine/Timer.h"
#include "../Engine/Options.h"
#include "../Menu/ErrorMessageState.h"
#include "SellState.h"
#include "../Mod/RuleInterface.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Engine/Screen.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#include <typeinfo>
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Manage Alien Containment screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param origin Game section that originated this state.
 */
ManageAlienContainmentState::ManageAlienContainmentState(Base *base, OptionsOrigin origin) : _base(base), _origin(origin), _sel(0), _aliensSold(0)
{
	bool overCrowded = Options::storageLimitsEnforced && _base->getFreeContainment() < 0;
	std::vector<std::string> researchList;
	for (std::vector<ResearchProject*>::const_iterator iter = _base->getResearch().begin(); iter != _base->getResearch().end(); ++iter)
	{
		const RuleResearch *research = (*iter)->getRules();
		RuleItem *item = _game->getMod()->getItem(research->getName());
		if (item && item->isAlien())
		{
			researchList.push_back(research->getName());
		}
	}

	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(overCrowded ? 288:148, 16, overCrowded ? 16:8, 176);
	_btnCancel = new TextButton(148, 16, 164, 176);
	_txtTitle = new Text(310, 17, 5, 8);
	_txtAvailable =  new Text(190, 9, 10, 24);
	_txtUsed = new Text(110, 9, 136, 24);
	_txtItem = new Text(120, 9, 10, 41);
	_txtLiveAliens = new Text(54, 18, 153, 32);
	_txtDeadAliens = new Text(54, 18, 207, 32);
	_txtInterrogatedAliens = new Text(54, 18, 261, 32);
	_lstAliens = new TextList(286, 112, 8, 53);

	// Set palette
	setInterface("manageContainment");

	add(_window, "window", "manageContainment");
	add(_btnOk, "button", "manageContainment");
	add(_btnCancel, "button", "manageContainment");
	add(_txtTitle, "text", "manageContainment");
	add(_txtAvailable, "text", "manageContainment");
	add(_txtUsed, "text", "manageContainment");
	add(_txtItem, "text", "manageContainment");
	add(_txtLiveAliens, "text", "manageContainment");
	add(_txtDeadAliens, "text", "manageContainment");
	add(_txtInterrogatedAliens, "text", "manageContainment");
	add(_lstAliens, "list", "manageContainment");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface((origin == OPT_BATTLESCAPE)? "BACK01.SCR" : "BACK05.SCR"));

	_btnOk->setText(tr("STR_REMOVE_SELECTED"));
	_btnOk->onMouseClick((ActionHandler)&ManageAlienContainmentState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ManageAlienContainmentState::btnOkClick, Options::keyOk);

	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&ManageAlienContainmentState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&ManageAlienContainmentState::btnCancelClick, Options::keyCancel);

	if (overCrowded)
	{
		_btnCancel->setVisible(false);
		_btnOk->setVisible(false);
	}

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_MANAGE_CONTAINMENT"));

	_txtItem->setText(tr("STR_ALIEN"));

	_txtLiveAliens->setText(tr("STR_LIVE_ALIENS"));
	_txtLiveAliens->setWordWrap(true);
	_txtLiveAliens->setVerticalAlign(ALIGN_BOTTOM);

	_txtDeadAliens->setText(tr("STR_DEAD_ALIENS"));
	_txtDeadAliens->setWordWrap(true);
	_txtDeadAliens->setVerticalAlign(ALIGN_BOTTOM);

	_txtInterrogatedAliens->setText(tr("STR_UNDER_INTERROGATION"));
	_txtInterrogatedAliens->setWordWrap(true);
	_txtInterrogatedAliens->setVerticalAlign(ALIGN_BOTTOM);

	_txtAvailable->setText(tr("STR_SPACE_AVAILABLE").arg(_base->getFreeContainment()));

	_txtUsed->setText(tr("STR_SPACE_USED").arg( _base->getUsedContainment()));

	_lstAliens->setArrowColumn(184, ARROW_HORIZONTAL);
	_lstAliens->setColumns(4, 160, 64, 46, 46);
	_lstAliens->setSelectable(true);
	_lstAliens->setBackground(_window);
	_lstAliens->setMargin(2);
	_lstAliens->onLeftArrowPress((ActionHandler)&ManageAlienContainmentState::lstItemsLeftArrowPress);
	_lstAliens->onLeftArrowRelease((ActionHandler)&ManageAlienContainmentState::lstItemsLeftArrowRelease);
	_lstAliens->onLeftArrowClick((ActionHandler)&ManageAlienContainmentState::lstItemsLeftArrowClick);
	_lstAliens->onRightArrowPress((ActionHandler)&ManageAlienContainmentState::lstItemsRightArrowPress);
	_lstAliens->onRightArrowRelease((ActionHandler)&ManageAlienContainmentState::lstItemsRightArrowRelease);
	_lstAliens->onRightArrowClick((ActionHandler)&ManageAlienContainmentState::lstItemsRightArrowClick);
	_lstAliens->onMousePress((ActionHandler)&ManageAlienContainmentState::lstItemsMousePress);

	const std::vector<std::string> &items = _game->getMod()->getItemsList();
	for (std::vector<std::string>::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		int qty = _base->getStorageItems()->getItem(*i);
		if (qty > 0 && _game->getMod()->getItem(*i, true)->isAlien())
		{
			_qtys.push_back(0);
			_aliens.push_back(*i);
			std::ostringstream ss;
			ss << qty;
			std::string rqty;
			std::vector<std::string>::iterator research = std::find(researchList.begin(), researchList.end(), *i);
			if (research != researchList.end())
			{
				rqty = "1";
				researchList.erase(research);
			}
			else
			{
				rqty = "0";
			}
			_lstAliens->addRow(4, tr(*i).c_str(), ss.str().c_str(), "0", rqty.c_str());
		}
	}

	for (std::vector<std::string>::const_iterator i = researchList.begin(); i != researchList.end(); ++i)
	{
		_aliens.push_back(*i);
		_qtys.push_back(0);
		_lstAliens->addRow(4, tr(*i).c_str(), "0", "0", "1");
		_lstAliens->setRowColor(_qtys.size() -1, _lstAliens->getSecondaryColor());
	}
	_timerInc = new Timer(250);
	_timerInc->onTimer((StateHandler)&ManageAlienContainmentState::increase);
	_timerDec = new Timer(250);
	_timerDec->onTimer((StateHandler)&ManageAlienContainmentState::decrease);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
    _btnOk->agentEnableNamedControl(); _btnCancel->agentEnableNamedControl();
    _lstAliens->agentEnableContainmentCells();
    if (_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if (_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
#endif
}

/**
 *
 */
ManageAlienContainmentState::~ManageAlienContainmentState()
{
	delete _timerInc;
	delete _timerDec;
}

/**
 * Runs the arrow timers.
 */
void ManageAlienContainmentState::think()
{
	State::think();

	_timerInc->think(this, 0);
	_timerDec->think(this, 0);
}

/**
 * Deals with the selected aliens.
 * @param action Pointer to an action.
 */
void ManageAlienContainmentState::btnOkClick(Action *)
{
	for (size_t i = 0; i < _qtys.size(); ++i)
	{
		if (_qtys[i] > 0)
		{
			// remove the aliens
			_base->getStorageItems()->removeItem(_aliens[i], _qtys[i]);

			if (Options::canSellLiveAliens)
			{
				_game->getSavedGame()->setFunds(_game->getSavedGame()->getFunds() + _game->getMod()->getItem(_aliens[i], true)->getSellCost() * _qtys[i]);
			}
			else
			{
				// add the corpses
				_base->getStorageItems()->addItem(
					_game->getMod()->getArmor(
						_game->getMod()->getUnit(
							_aliens[i], true
						)->getArmor(), true
					)->getCorpseGeoscape(), _qtys[i]
				); // ;)
			}
		}
	}
	_game->popState();

	if (Options::storageLimitsEnforced && _base->storesOverfull())
	{
		_game->pushState(new SellState(_base, _origin));
		if (_origin == OPT_BATTLESCAPE)
			_game->pushState(new ErrorMessageState(tr("STR_STORAGE_EXCEEDED").arg(_base->getName()), _palette, _game->getMod()->getInterface("manageContainment")->getElement("errorMessage")->color, "BACK01.SCR", _game->getMod()->getInterface("manageContainment")->getElement("errorPalette")->color));
		else
			_game->pushState(new ErrorMessageState(tr("STR_STORAGE_EXCEEDED").arg(_base->getName()), _palette, _game->getMod()->getInterface("manageContainment")->getElement("errorMessage")->color, "BACK13.SCR", _game->getMod()->getInterface("manageContainment")->getElement("errorPalette")->color));
 	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ManageAlienContainmentState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Starts increasing the alien count.
 * @param action Pointer to an action.
 */
void ManageAlienContainmentState::lstItemsRightArrowPress(Action *action)
{
	_sel = _lstAliens->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && !_timerInc->isRunning()) _timerInc->start();
}

/**
 * Stops increasing the alien count.
 * @param action Pointer to an action.
 */
void ManageAlienContainmentState::lstItemsRightArrowRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerInc->stop();
	}
}

/**
 * Increases the selected alien count;
 * by one on left-click, to max on right-click.
 * @param action Pointer to an action.
 */
void ManageAlienContainmentState::lstItemsRightArrowClick(Action *action)
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
 * Starts decreasing the alien count.
 * @param action Pointer to an action.
 */
void ManageAlienContainmentState::lstItemsLeftArrowPress(Action *action)
{
	_sel = _lstAliens->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && !_timerDec->isRunning()) _timerDec->start();
}

/**
 * Stops decreasing the alien count.
 * @param action Pointer to an action.
 */
void ManageAlienContainmentState::lstItemsLeftArrowRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerDec->stop();
	}
}

/**
 * Decreases the selected alien count;
 * by one on left-click, to 0 on right-click.
 * @param action Pointer to an action.
 */
void ManageAlienContainmentState::lstItemsLeftArrowClick(Action *action)
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
void ManageAlienContainmentState::lstItemsMousePress(Action *action)
{
	_sel = _lstAliens->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
	{
		_timerInc->stop();
		_timerDec->stop();
		if (action->getAbsoluteXMouse() >= _lstAliens->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstAliens->getArrowsRightEdge())
		{
			increaseByValue(Options::changeValueByMouseWheel);
		}
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
	{
		_timerInc->stop();
		_timerDec->stop();
		if (action->getAbsoluteXMouse() >= _lstAliens->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstAliens->getArrowsRightEdge())
		{
			decreaseByValue(Options::changeValueByMouseWheel);
		}
	}
}

/**
 * Gets the quantity of the currently selected alien on the base.
 * @return Quantity of selected alien on the base.
 */
int ManageAlienContainmentState::getQuantity()
{
	return _base->getStorageItems()->getItem(_aliens[_sel]);
}

/**
 * Increases the quantity of the selected alien to exterminate by one.
 */
void ManageAlienContainmentState::increase()
{
	_timerDec->setInterval(50);
	_timerInc->setInterval(50);
	increaseByValue(1);
}

/**
 * Increases the quantity of the selected alien to exterminate by "change".
 * @param change How much we want to add.
 */
void ManageAlienContainmentState::increaseByValue(int change)
{
	int qty = getQuantity() - _qtys[_sel];
	if (change <= 0 || qty <= 0) return;

	change = std::min(qty, change);
	_qtys[_sel] += change;
	_aliensSold += change;
	updateStrings();
}

/**
 * Decreases the quantity of the selected alien to exterminate by one.
 */
void ManageAlienContainmentState::decrease()
{
	_timerInc->setInterval(50);
	_timerDec->setInterval(50);
	decreaseByValue(1);
}

/**
 * Decreases the quantity of the selected alien to exterminate by "change".
 * @param change How much we want to remove.
 */
void ManageAlienContainmentState::decreaseByValue(int change)
{
	if (change <= 0 || _qtys[_sel] <= 0) return;
	change = std::min(_qtys[_sel], change);
	_qtys[_sel] -= change;
	_aliensSold -= change;
	updateStrings();
}

/**
 * Updates the quantity-strings of the selected alien.
 */
void ManageAlienContainmentState::updateStrings()
{
	std::ostringstream ss, ss2;
	int qty = getQuantity() - _qtys[_sel];
	ss << qty;
	ss2 << _qtys[_sel];

	_lstAliens->setRowColor(_sel, (qty == 0)? _lstAliens->getSecondaryColor() : _lstAliens->getColor());
	_lstAliens->setCellText(_sel, 1, ss.str());
	_lstAliens->setCellText(_sel, 2, ss2.str());

	int aliens = _base->getUsedContainment() - _aliensSold;
	int spaces = _base->getAvailableContainment() - aliens;
	if (Options::storageLimitsEnforced)
	{
		_btnOk->setVisible(spaces >= 0);
	}
	_txtAvailable->setText(tr("STR_SPACE_AVAILABLE").arg(spaces));
	_txtUsed->setText(tr("STR_SPACE_USED").arg(aliens));
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ManageAlienContainmentState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> ManageAlienContainmentState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    for (const Text *text : {_txtTitle, _txtUsed, _txtAvailable, _txtItem, _txtLiveAliens, _txtDeadAliens, _txtInterrogatedAliens})
        revisions.push_back(text->agentTextRevision());
    revisions.push_back(_lstAliens->agentListRevision());
    revisions.push_back(_btnOk->agentLabelRevision()); revisions.push_back(_btnCancel->agentLabelRevision());
    return revisions;
}
void ManageAlienContainmentState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool ManageAlienContainmentState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(ManageAlienContainmentState) || _modal || _window->agentPopupActive() ||
        _timerInc->isRunning() || _timerDec->isRunning() ||
        !_game->isState(const_cast<ManageAlienContainmentState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState(); out.screen = "manage_containment"; out.actor = _base;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const Surface *logical = _game->getScreen()->getSurface();
    if (!agentWidgetVisible(_window) || !_window->agentPresentationPaintedTo(logical)) return false;
    for (const Surface *surface : painted) out.widgets.emplace_back(surface, surface->agentPresentationRevision());
    out.widgets.emplace_back(_lstAliens, _lstAliens->agentListRevision());
    if (_game->getCursor()) out.widgets.emplace_back(_game->getCursor(), _game->getCursor()->agentPresentationRevision());
    if (_game->getFpsCounter()) out.widgets.emplace_back(_game->getFpsCounter(), _game->getFpsCounter()->agentPresentationRevision());
    const auto allowed = [this](const AgentUiState &part) { return _game->agentOverlayClear(part); };
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        const auto value = agentTextJson(widget);
        if (value == "null" || !widget->agentPresentationPaintedTo(logical) || !agentWidgetUncovered(widget, painted)) return "null";
        AgentUiState part; agentIncludeWidget(part, widget);
        return !part.regions.empty() && allowed(part) ? value : "null";
    };
    const auto title=text(_txtTitle), available=text(_txtAvailable), used=text(_txtUsed);
    const auto item=text(_txtItem), live=text(_txtLiveAliens), dead=text(_txtDeadAliens), interrogated=text(_txtInterrogatedAliens);
    const auto control = [&](const Surface *widget) -> bool {
        if (!widget || !widget->agentPresentationPaintedTo(logical) || !agentWidgetUncovered(widget, painted)) return false;
        AgentUiState part; agentIncludeWidget(part, widget);
        if (part.regions.empty() || !allowed(part)) return false;
        const auto old=out.regions.size();out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(old);return false;
    };
    std::vector<AgentOutfittingRow> rows;std::uint64_t epoch=0;std::string values="[";
    bool anyIncrease=false,anyDecrease=false;
    if (_lstAliens->agentContainmentRows(logical,rows,epoch) && rows.size()<=32) for (const auto &row:rows) {
        if(row.cells.size()!=4)continue;
        std::string cells="[";bool witnessed=false;
        for(size_t col=0;col<4;++col){
            if(col)cells+=",";const auto &cell=row.cells[col];bool known=cell.known && cell.text.size()<=1024 && !cell.regions.empty();
            for(const auto &r:cell.regions){AgentUiState part;part.regions.push_back({r.x,r.y,r.w,r.h});
                // Native arrows composite over their list; only foreign state surfaces occlude logical cells.
                if(!agentRegionsUncovered(part.regions,_lstAliens,_surfaces) || !allowed(part))known=false;}
            cells+=known?agentJsonString(cell.text):"null";witnessed=witnessed||known;
        }cells+="]";
        const bool increase=control(row.increase),decrease=control(row.decrease);
        if(!witnessed && !increase && !decrease)continue;
        if(values.size()>1)values+=",";
        values+="{\"handle\":\"r"+std::to_string(out.rows.size())+"\",\"cells\":"+cells+",\"selected\":"+(row.selected?"true":"false")+",\"controls\":[";
        if(increase)values+="\"increase\"";if(decrease)values+=increase?",\"decrease\"":"\"decrease\"";values+="]}";
        out.rows.push_back(row.logicalIndex);out.context.push_back((increase?1:0)|(decrease?2:0));anyIncrease|=increase;anyDecrease|=decrease;
    }
    values+="]";
    if(anyIncrease)out.actions.emplace_back("increase_containment_removal","Increase selected alien removal","visible_control_semantics","project_row");
    if(anyDecrease)out.actions.emplace_back("decrease_containment_removal","Decrease selected alien removal","visible_control_semantics","project_row");
    const Surface *up=nullptr,*down=nullptr;_lstAliens->agentScrollControls(up,down);
    const bool canUp=control(up),canDown=control(down);out.context.push_back(canUp);out.context.push_back(canDown);out.context.push_back(epoch);
    if(canUp||canDown)out.actions.emplace_back("scroll_visible_list","Scroll visible list","visible_control_semantics","scroll_direction");
    std::string buttons="[";
    const auto button=[&](TextButton *widget,const char *id,const char *action){
        const auto label=agentProjectNamedButton(widget,logical,painted,out,allowed);if(label.empty()||label.size()>1024)return;
        if(buttons.size()>1)buttons+=",";buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";out.actions.emplace_back(action,label,"visible_text");};
    button(_btnOk,"remove","remove_selected_aliens");button(_btnCancel,"cancel","cancel_containment");buttons+="]";
    out.visible="{\"title\":"+title+",\"space_available\":"+available+",\"space_used\":"+used+",\"headings\":["+item+","+live+","+dead+","+interrogated+"],\"rows\":"+values+
        ",\"scroll\":{\"up\":"+(canUp?"true":"false")+",\"down\":"+(canDown?"true":"false")+"},\"buttons\":"+buttons+"}";
    return out.visible.size()<=32768 && agentRegionsDisjoint(out) && (!out.actions.empty() || !out.rows.empty() || title!="null");
}
bool ManageAlienContainmentState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;if(!agentUiState(fresh)||!fresh.offers(request.action))return false;
    if(request.action=="remove_selected_aliens"){_timerInc->stop();_timerDec->stop();btnOkClick(nullptr);return true;}
    if(request.action=="cancel_containment"){_timerInc->stop();_timerDec->stop();btnCancelClick(nullptr);return true;}
    if(request.action=="scroll_visible_list" && fresh.context.size()==fresh.rows.size()+3){
        _timerInc->stop();_timerDec->stop();
        if(request.choice=="up"&&fresh.context[fresh.rows.size()]){_lstAliens->scrollUp(false,false);return true;}
        if(request.choice=="down"&&fresh.context[fresh.rows.size()+1]){_lstAliens->scrollDown(false,false);return true;}return false;
    }
    const bool increase=request.action=="increase_containment_removal";
    if(increase||request.action=="decrease_containment_removal")for(size_t i=0;i<fresh.rows.size();++i)if(request.choice=="r"+std::to_string(i)){
        if(!(fresh.context[i]&(increase?1:2)))return false;
        _timerInc->stop();_timerDec->stop();
        return _lstAliens->agentContainmentArrowClick(_game->getScreen()->getSurface(),fresh.rows[i],increase,this);
    }
    return false;
}
#endif

}
