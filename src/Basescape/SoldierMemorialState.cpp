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
#include "SoldierMemorialState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDeath.h"
#include "../Savegame/GameTime.h"
#include "SoldierInfoState.h"
#include "../Menu/StatisticsState.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Engine/Screen.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#include <set>
#include <typeinfo>
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Soldier Memorial screen.
 * @param game Pointer to the core game.
 */
SoldierMemorialState::SoldierMemorialState()
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(148, 16, 164, 176);
	_btnStatistics = new TextButton(148, 16, 8, 176);
	_txtTitle = new Text(310, 17, 5, 8);
	_txtName = new Text(114, 9, 16, 36);
	_txtRank = new Text(102, 9, 130, 36);
	_txtDate = new Text(90, 9, 218, 36);
	_txtRecruited = new Text(150, 9, 16, 24);
	_txtLost = new Text(150, 9, 160, 24);
	_lstSoldiers = new TextList(288, 120, 8, 44);

	// Set palette
	setInterface("soldierMemorial");

	add(_window, "window", "soldierMemorial");
	add(_btnOk, "button", "soldierMemorial");
	add(_btnStatistics, "button", "soldierMemorial");
	add(_txtTitle, "text", "soldierMemorial");
	add(_txtName, "text", "soldierMemorial");
	add(_txtRank, "text", "soldierMemorial");
	add(_txtDate, "text", "soldierMemorial");
	add(_txtRecruited, "text", "soldierMemorial");
	add(_txtLost, "text", "soldierMemorial");
	add(_lstSoldiers, "list", "soldierMemorial");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK02.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&SoldierMemorialState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&SoldierMemorialState::btnOkClick, Options::keyCancel);

	_btnStatistics->setText(tr("STR_STATISTICS"));
	_btnStatistics->onMouseClick((ActionHandler)&SoldierMemorialState::btnStatisticsClick);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_MEMORIAL"));

	_txtName->setText(tr("STR_NAME_UC"));

	_txtRank->setText(tr("STR_RANK"));

	_txtDate->setText(tr("STR_DATE_UC"));

	size_t lost = _game->getSavedGame()->getDeadSoldiers()->size();
	size_t recruited = lost;
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		recruited += (*i)->getTotalSoldiers();
	}

	_txtRecruited->setText(tr("STR_SOLDIERS_RECRUITED_UC").arg(recruited));

	_txtLost->setText(tr("STR_SOLDIERS_LOST_UC").arg(lost));

	_lstSoldiers->setColumns(5, 114, 88, 30, 25, 35);
	_lstSoldiers->setSelectable(true);
	_lstSoldiers->setBackground(_window);
	_lstSoldiers->setMargin(8);
	_lstSoldiers->onMouseClick((ActionHandler)&SoldierMemorialState::lstSoldiersClick);

	for (std::vector<Soldier*>::reverse_iterator i = _game->getSavedGame()->getDeadSoldiers()->rbegin(); i != _game->getSavedGame()->getDeadSoldiers()->rend(); ++i)
	{
		SoldierDeath *death = (*i)->getDeath();

		std::ostringstream saveDay, saveMonth, saveYear;
		saveDay << death->getTime()->getDayString(_game->getLanguage());
		saveMonth << tr(death->getTime()->getMonthString());
		saveYear << death->getTime()->getYear();
		_lstSoldiers->addRow(5, (*i)->getName().c_str(), tr((*i)->getRankString()).c_str(), saveDay.str().c_str(), saveMonth.str().c_str(), saveYear.str().c_str());
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
    _btnOk->agentEnableNamedControl();
    _lstSoldiers->agentEnableReportCells(5, false, true);
    _lstSoldiers->agentEnableReportSelection();
    if (_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if (_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
    // Keep constructor order privately. Native rows reverse this order and
    // SoldierInfo performs the visual-ordinal reversal itself, exactly once.
    _agentMemorialSave = _game->getSavedGame();
    for (const Soldier *soldier : *_game->getSavedGame()->getDeadSoldiers()) {
        _agentDeadSoldiers.push_back(soldier);
        _agentDeadIds.push_back(soldier ? soldier->getId() : -1);
        const SoldierDeath *death = soldier ? soldier->getDeath() : nullptr;
        _agentDeaths.push_back(death);
        const GameTime *date = death ? death->getTime() : nullptr;
        _agentDeathDates.push_back(date ? date->getYear() : -1);
        _agentDeathDates.push_back(date ? date->getMonth() : -1);
        _agentDeathDates.push_back(date ? date->getDay() : -1);
    }
#endif
}

/**
 *
 */
SoldierMemorialState::~SoldierMemorialState()
{

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void SoldierMemorialState::btnOkClick(Action *)
{
	_game->popState();
	_game->getMod()->playMusic("GMGEO");
}

/**
* Shows the Statistics screen.
* @param action Pointer to an action.
*/
void SoldierMemorialState::btnStatisticsClick(Action *)
{
	_game->pushState(new StatisticsState);
}

/**
 * Shows the selected soldier's info.
 * @param action Pointer to an action.
 */
void SoldierMemorialState::lstSoldiersClick(Action *)
{
	_game->pushState(new SoldierInfoState(0, _lstSoldiers->getSelectedRow()));
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool SoldierMemorialState::agentMemorialBindingCurrent() const
{
    if (_game->getSavedGame() != _agentMemorialSave) return false;
    const auto *dead = _game->getSavedGame()->getDeadSoldiers();
    if (!dead || dead->size() != _agentDeadSoldiers.size() || _agentDeadIds.size() != dead->size() ||
        _agentDeaths.size() != dead->size() || _agentDeathDates.size() != dead->size()*3 || _lstSoldiers->getRows() != dead->size()) return false;
    std::set<int> ids;
    for (size_t i=0; i<dead->size(); ++i) {
        const Soldier *soldier=dead->at(i);
        if (!soldier || soldier != _agentDeadSoldiers[i] || soldier->getId()<0 ||
            soldier->getId() != _agentDeadIds[i] || !ids.insert(soldier->getId()).second) return false;
        const SoldierDeath *death=soldier->getDeath();
        if (!death || death != _agentDeaths[i] || !death->getTime()) return false;
        const GameTime *date=death->getTime();
        if (date->getYear()!=_agentDeathDates[i*3] || date->getMonth()!=_agentDeathDates[i*3+1] || date->getDay()!=_agentDeathDates[i*3+2]) return false;
    }
    return true;
}
bool SoldierMemorialState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> SoldierMemorialState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    for (const Text *text : {_txtTitle, _txtName, _txtRank, _txtDate, _txtRecruited, _txtLost}) revisions.push_back(text->agentTextRevision());
    revisions.push_back(_lstSoldiers->agentListRevision());
    revisions.push_back(_btnOk->agentLabelRevision());
    return revisions;
}
void SoldierMemorialState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool SoldierMemorialState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(SoldierMemorialState) || _modal || _window->agentPopupActive() ||
        !agentMemorialBindingCurrent() ||
        !_game->isState(const_cast<SoldierMemorialState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out=AgentUiState();out.screen="soldier_memorial";out.actor=_agentMemorialSave;out.save=_agentMemorialSave;out.context.push_back(_agentDeadSoldiers.size());
    const auto painted=agentFlattenPaintOrder(_surfaces);const Surface *logical=_game->getScreen()->getSurface();
    if(!agentWidgetVisible(_window)||!_window->agentPresentationPaintedTo(logical))return false;
    for(const Surface *surface:painted)out.widgets.emplace_back(surface,surface->agentPresentationRevision());
    if(_game->getCursor())out.widgets.emplace_back(_game->getCursor(),_game->getCursor()->agentPresentationRevision());
    if(_game->getFpsCounter())out.widgets.emplace_back(_game->getFpsCounter(),_game->getFpsCounter()->agentPresentationRevision());
    const auto allowed=[this](const AgentUiState &part){return _game->agentOverlayClear(part);};
    const auto text=[&](const Text *widget)->std::string{
        out.widgets.emplace_back(widget,widget->agentTextRevision());const auto value=agentTextJson(widget);
        if(value=="null"||!widget->agentPresentationPaintedTo(logical)||!agentWidgetUncovered(widget,painted))return "null";
        AgentUiState part;agentIncludeWidget(part,widget);return !part.regions.empty()&&allowed(part)?value:"null";
    };
    const auto title=text(_txtTitle);
    bool tableHealthy=true;
    const auto table=[&](const TextList *list,unsigned columns)->std::string{
        out.widgets.emplace_back(list,list->agentListRevision());std::vector<AgentOutfittingRow> rows;std::uint64_t epoch=0;
        if(!list->agentPresentationPaintedTo(logical)||!list->agentReportRows(rows,epoch)||rows.size()>32)return "[]";
        out.context.push_back(epoch);std::string values="[";
        for(const auto &row:rows){
            if(row.cells.size()!=columns){tableHealthy=false;return "[]";}
            bool witnessed=false;std::string value="{\"cells\":[";
            for(unsigned col=0;col<columns;++col){
                if(col)value+=",";const auto &cell=row.cells[col];bool known=cell.known&&cell.text.size()<=1024&&!cell.regions.empty();bool visible=!cell.regions.empty();
                for(const auto &r:cell.regions){AgentUiState part;part.regions.push_back({r.x,r.y,r.w,r.h});if(!agentRegionsUncovered(part.regions,list,painted)||!allowed(part)){known=false;visible=false;}}
                witnessed=witnessed||visible;value+=known?agentJsonString(cell.text):"null";
            }value+="]";
            SDL_Rect point={}; const bool target=list->agentReportRowTarget(row.logicalIndex,point);
            AgentUiState hit; if(target)hit.regions.push_back({point.x,point.y,1,1});
            const bool shown=target && agentRegionsUncovered(hit.regions,list,painted) && allowed(hit);
            if(!witnessed&&!shown)continue;
            bool actionable=shown && row.logicalIndex<_agentDeadSoldiers.size();
            value+=",\"handle\":";
            if(actionable){value+=agentJsonString("r"+std::to_string(out.rows.size()));out.rows.push_back(row.logicalIndex);out.regions.insert(out.regions.end(),hit.regions.begin(),hit.regions.end());}
            else value+="null";
            value+=",\"selected\":";value+=shown&&list->agentReportRowSelected(row.logicalIndex)?"true":"false";value+="}";
            if(values.size()>1)values+=",";values+=value;if(values.size()>16000){tableHealthy=false;return "[]";}
        }return values+"]";
    };
    const auto name=text(_txtName),rank=text(_txtRank),date=text(_txtDate),recruited=text(_txtRecruited),lost=text(_txtLost);const auto rows=table(_lstSoldiers,5);
    const Surface *up=nullptr,*down=nullptr;_lstSoldiers->agentScrollControls(up,down);
    const auto control=[&](const Surface *widget)->bool{
        if(!widget||!agentWidgetUncovered(widget,painted))return false;AgentUiState part;agentIncludeWidget(part,widget);if(part.regions.empty()||!allowed(part))return false;
        const auto old=out.regions.size();out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());if(agentRegionsDisjoint(out))return true;out.regions.resize(old);return false;
    };
    const bool canUp=control(up),canDown=control(down);out.context.push_back(canUp);out.context.push_back(canDown);
    if(canUp||canDown)out.actions.emplace_back("scroll_visible_list","Scroll visible list","visible_control_semantics","scroll_direction");
    if(!out.rows.empty())out.actions.emplace_back("select_memorial_soldier","Open visible memorial soldier","visible_control_semantics","project_row");
    std::string buttons="[";
    const auto button=[&](TextButton *widget,const char *id,const char *action){
        const auto label=agentProjectNamedButton(widget,logical,painted,out,allowed);if(label.empty()||label.size()>1024)return;
        if(buttons.size()>1)buttons+=",";buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";out.actions.emplace_back(action,label,"visible_text");
    };
    button(_btnOk,"ok","close_memorial");buttons+="]";
    out.visible="{\"title\":"+title+",\"recruited\":"+recruited+",\"lost\":"+lost+",\"headings\":["+name+","+rank+","+date+"],\"rows\":"+rows+",\"scroll\":{\"up\":"+(canUp?"true":"false")+",\"down\":"+(canDown?"true":"false")+"},\"buttons\":"+buttons+"}";
    return tableHealthy&&out.visible.size()<=32768&&agentRegionsDisjoint(out)&&(!out.actions.empty()||title!="null"||rows!="[]");
}
bool SoldierMemorialState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;if(!agentUiState(fresh)||!fresh.offers(request.action))return false;
    if(request.action=="select_memorial_soldier"){
        for(size_t index=0;index<fresh.rows.size();++index)if(request.choice=="r"+std::to_string(index)){
            const size_t row=fresh.rows[index];return row<_agentDeadSoldiers.size()&&_lstSoldiers->agentReportRowClick(row,this);
        }return false;
    }
    if(request.action=="close_memorial"){btnOkClick(nullptr);return true;}
    if(request.action=="scroll_visible_list"&&fresh.context.size()>=3){
        if(request.choice=="up"&&fresh.context[fresh.context.size()-2]){_lstSoldiers->scrollUp(false,false);return true;}
        if(request.choice=="down"&&fresh.context.back()){_lstSoldiers->scrollDown(false,false);return true;}
    }return false;
}
#endif

}
