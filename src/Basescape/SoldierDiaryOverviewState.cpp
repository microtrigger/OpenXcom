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
#include "SoldierDiaryOverviewState.h"
#include "SoldierDiaryPerformanceState.h"
#include "SoldierDiaryMissionState.h"
#include "SoldierInfoState.h"
#include <sstream>
#include "../Mod/Mod.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDiary.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/MissionStatistics.h"

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
 * Initializes all the elements in the Soldier Diary screen.
 * @param base Pointer to the base to get info from.
 * @param soldierId ID of the selected soldier.
 * @param soldierInfoState Pointer to the Soldier Info screen.
 */
SoldierDiaryOverviewState::SoldierDiaryOverviewState(Base *base, size_t soldierId, SoldierInfoState *soldierInfoState) : _base(base), _soldierId(soldierId), _soldierInfoState(soldierInfoState)
{
	if (_base == 0)
	{
		_list = _game->getSavedGame()->getDeadSoldiers();
	}
	else
	{
		_list = _base->getSoldiers();
	}

	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnKills = new TextButton(70, 16, 8, 176);
	_btnMissions = new TextButton(70, 16, 86, 176);
	_btnCommendations = new TextButton(70, 16, 164, 176);
	_btnOk = new TextButton(70, 16, 242, 176);
	_btnPrev = new TextButton(28, 14, 8, 8);
	_btnNext = new TextButton(28, 14, 284, 8);
	_txtTitle = new Text(310, 16, 5, 8);
	_txtMission = new Text(114, 9, 16, 36);
	_txtRating = new Text(102, 9, 120, 36);
	_txtDate = new Text(90, 9, 218, 36);
	_lstDiary = new TextList(288, 120, 8, 44);

	// Set palette
	setInterface("soldierDiary");

	add(_window, "window", "soldierDiary");
	add(_btnOk, "button", "soldierDiary");
	add(_btnKills, "button", "soldierDiary");
	add(_btnMissions, "button", "soldierDiary");
	add(_btnCommendations, "button", "soldierDiary");
	add(_btnPrev, "button", "soldierDiary");
	add(_btnNext, "button", "soldierDiary");
	add(_txtTitle, "text1", "soldierDiary");
	add(_txtMission, "text2", "soldierDiary");
	add(_txtRating, "text2", "soldierDiary");
	add(_txtDate, "text2", "soldierDiary");
	add(_lstDiary, "list", "soldierDiary");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK02.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&SoldierDiaryOverviewState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&SoldierDiaryOverviewState::btnOkClick, Options::keyCancel);

	_btnKills->setText(tr("STR_COMBAT"));
	_btnKills->onMouseClick((ActionHandler)&SoldierDiaryOverviewState::btnKillsClick);

	_btnMissions->setText(tr("STR_PERFORMANCE"));
	_btnMissions->onMouseClick((ActionHandler)&SoldierDiaryOverviewState::btnMissionsClick);

	_btnCommendations->setText(tr("STR_AWARDS"));
	_btnCommendations->onMouseClick((ActionHandler)&SoldierDiaryOverviewState::btnCommendationsClick);
	_btnCommendations->setVisible(!_game->getMod()->getCommendationsList().empty());

	_btnPrev->setText("<<");
	if (_base == 0)
	{
		_btnPrev->onMouseClick((ActionHandler)&SoldierDiaryOverviewState::btnNextClick);
		_btnPrev->onKeyboardPress((ActionHandler)&SoldierDiaryOverviewState::btnNextClick, Options::keyBattlePrevUnit);
	}
	else
	{
		_btnPrev->onMouseClick((ActionHandler)&SoldierDiaryOverviewState::btnPrevClick);
		_btnPrev->onKeyboardPress((ActionHandler)&SoldierDiaryOverviewState::btnPrevClick, Options::keyBattlePrevUnit);
	}

	_btnNext->setText(">>");
	if (_base == 0)
	{
		_btnNext->onMouseClick((ActionHandler)&SoldierDiaryOverviewState::btnPrevClick);
		_btnNext->onKeyboardPress((ActionHandler)&SoldierDiaryOverviewState::btnPrevClick, Options::keyBattleNextUnit);
	}
	else
	{
		_btnNext->onMouseClick((ActionHandler)&SoldierDiaryOverviewState::btnNextClick);
		_btnNext->onKeyboardPress((ActionHandler)&SoldierDiaryOverviewState::btnNextClick, Options::keyBattleNextUnit);
	}

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	// Text is decided in init()

	_txtMission->setText(tr("STR_MISSION"));

	_txtRating->setText(tr("STR_RATING_UC"));

	_txtDate->setText(tr("STR_DATE_UC"));

	_lstDiary->setColumns(5, 104, 98, 30, 25, 35);
	_lstDiary->setSelectable(true);
	_lstDiary->setBackground(_window);
	_lstDiary->setMargin(8);
	_lstDiary->onMouseClick((ActionHandler)&SoldierDiaryOverviewState::lstDiaryInfoClick);

	init(); // Populate the list
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
    for (TextButton *button : {_btnPrev, _btnNext, _btnKills, _btnCommendations, _btnOk}) button->agentEnableNamedControl();
    _lstDiary->agentEnableReportCells(5, false, true);
    _lstDiary->agentEnableReportSelection();
    _btnMissions->agentEnableNamedControl();
    if (_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if (_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
#endif
}

/**
 *
 */
SoldierDiaryOverviewState::~SoldierDiaryOverviewState()
{

}

/**
 *  Clears all the variables and reinitializes the list of medals for the soldier.
 *
 */
void SoldierDiaryOverviewState::init()
{
	State::init();
	if (_list->empty())
	{
		_game->popState();
		return;
	}
	if (_soldierId >= _list->size())
	{
		_soldierId = 0;
	}
	_soldier = _list->at(_soldierId);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentDeadRoster.clear();_agentDeadOrdinal=_soldierId;
    if(!_base)for(Soldier *dead:*_list){
        if(!dead || !dead->getDeath()){_agentDeadRoster.clear();break;}
        _agentDeadRoster.emplace_back(dead,dead->getId());
    }
#endif
	_txtTitle->setText(_soldier->getName());
	_lstDiary->clearList();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentMissionRows.clear();
#endif

	std::vector<MissionStatistics*> *missionStatistics = _game->getSavedGame()->getMissionStatistics();

	unsigned int row = 0;
	for (std::vector<MissionStatistics*>::iterator j = missionStatistics->begin() ; j != missionStatistics->end() ; ++j)
	{
		int missionId = (*j)->id;
		bool wasOnMission = false;

		// See if this mission is part of the soldier's vector of missions
		for (std::vector<int>::const_iterator k = _soldier->getDiary()->getMissionIdList().begin(); k != _soldier->getDiary()->getMissionIdList().end(); ++k)
		{
			if (missionId == (*k))
			{
				wasOnMission = true;
				break;
			}
		}
		if (!wasOnMission)
		{
			continue;
		}

		std::ostringstream ss;
		ss << (*j)->time.getYear();

		_lstDiary->addRow(5, (*j)->getMissionName(_game->getLanguage()).c_str(),
							 (*j)->getRatingString(_game->getLanguage()).c_str(),
							 (*j)->time.getDayString(_game->getLanguage()).c_str(),
							 tr((*j)->time.getMonthString()).c_str(),
							 ss.str().c_str());
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        _agentMissionRows.push_back(*j);
#endif
		row++;
	}
	if (row > 0 && _lstDiary->getScroll() >= row)
	{
		_lstDiary->scrollTo(0);
	}
}

/**
 * Set the soldier's Id.
 */
void SoldierDiaryOverviewState::setSoldierId(size_t soldier)
{
	_soldierId = soldier;
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void SoldierDiaryOverviewState::btnOkClick(Action *)
{
	_soldierInfoState->setSoldierId(_soldierId);
	_game->popState();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void SoldierDiaryOverviewState::btnKillsClick(Action *)
{
	_game->pushState(new SoldierDiaryPerformanceState(_base, _soldierId, this, DIARY_KILLS));
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void SoldierDiaryOverviewState::btnMissionsClick(Action *)
{
	_game->pushState(new SoldierDiaryPerformanceState(_base, _soldierId, this, DIARY_MISSIONS));
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void SoldierDiaryOverviewState::btnCommendationsClick(Action *)
{
	_game->pushState(new SoldierDiaryPerformanceState(_base, _soldierId, this, DIARY_COMMENDATIONS));
}


/**
 * Goes to the previous soldier.
 * @param action Pointer to an action.
 */
void SoldierDiaryOverviewState::btnPrevClick(Action *)
{
	if (_soldierId == 0)
		_soldierId = _list->size() - 1;
	else
		_soldierId--;
	init();
}

/**
 * Goes to the next soldier.
 * @param action Pointer to an action.
 */
void SoldierDiaryOverviewState::btnNextClick(Action *)
{
	_soldierId++;
	if (_soldierId >= _list->size())
		_soldierId = 0;
	init();
}

/**
 * Shows the selected soldier's info.
 * @param action Pointer to an action.
 */
void SoldierDiaryOverviewState::lstDiaryInfoClick(Action *)
{
	int absoluteRowEntry = _lstDiary->getSelectedRow();
	_game->pushState(new SoldierDiaryMissionState(_soldier, absoluteRowEntry));
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool SoldierDiaryOverviewState::agentMissionRowValid(size_t row) const
{
    if (!_soldier || row>=_agentMissionRows.size()) return false;
    const auto &ids=_soldier->getDiary()->getMissionIdList();
    const auto *stats=_game->getSavedGame()->getMissionStatistics();
    if(row>=ids.size() || ids[row]<0 || static_cast<size_t>(ids[row])>=stats->size())return false;
    const MissionStatistics *record=stats->at(ids[row]);
    return record && record==_agentMissionRows[row] && record->id==ids[row];
}
bool SoldierDiaryOverviewState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> SoldierDiaryOverviewState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    for (const Text *text : {_txtTitle, _txtMission, _txtRating, _txtDate}) revisions.push_back(text->agentTextRevision());
    revisions.push_back(_lstDiary->agentListRevision());
    revisions.push_back(_btnMissions->agentLabelRevision());
    for (const TextButton *button : {_btnPrev, _btnNext, _btnKills, _btnCommendations, _btnOk}) revisions.push_back(button->agentLabelRevision());
    return revisions;
}
void SoldierDiaryOverviewState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool SoldierDiaryOverviewState::agentDeadRosterValid() const
{
    if(_base || !_game->getSavedGame() || _list!=_game->getSavedGame()->getDeadSoldiers() ||
        !_list || _list->empty() || _agentDeadRoster.size()!=_list->size() || _agentDeadOrdinal!=_soldierId ||
        _soldierId>=_list->size() || _list->at(_soldierId)!=_soldier)return false;
    for(size_t i=0;i<_list->size();++i){
        Soldier *current=_list->at(i);
        if(!current || current!=_agentDeadRoster[i].first || current->getId()!=_agentDeadRoster[i].second || !current->getDeath())return false;
    }
    return true;
}
bool SoldierDiaryOverviewState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(SoldierDiaryOverviewState) || _modal || _window->agentPopupActive() ||
        (_base ? (!_list || _list != _base->getSoldiers()) : !agentDeadRosterValid()) || !_soldier || _soldierId >= _list->size() || _list->at(_soldierId) != _soldier ||
        !_game->isState(const_cast<SoldierDiaryOverviewState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out=AgentUiState();out.screen=_base?"soldier_diary":"memorial_soldier_diary";out.actor=_soldier;out.save=_base?static_cast<const void*>(_base):static_cast<const void*>(_game->getSavedGame());out.context.push_back(_soldierId);
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
    const auto table=[&](const TextList *list,unsigned columns)->std::string{
        out.widgets.emplace_back(list,list->agentListRevision());std::vector<AgentOutfittingRow> rows;std::uint64_t epoch=0;
        if(!list->agentPresentationPaintedTo(logical)||!list->agentReportRows(rows,epoch)||rows.size()>32)return "[]";
        out.context.push_back(epoch);std::string values="[";
        for(const auto &row:rows){
            if(row.cells.size()!=columns)return "[]";
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
            bool actionable=shown && agentMissionRowValid(row.logicalIndex);
            value+=",\"handle\":";
            if(actionable){value+=agentJsonString("r"+std::to_string(out.rows.size()));out.rows.push_back(row.logicalIndex);out.regions.insert(out.regions.end(),hit.regions.begin(),hit.regions.end());}
            else value+="null";
            value+=",\"selected\":";value+=shown&&list->agentReportRowSelected(row.logicalIndex)?"true":"false";value+="}";
            if(values.size()>1)values+=",";values+=value;if(values.size()>16000)return "[]";
        }return values+"]";
    };
    const auto mission=text(_txtMission),rating=text(_txtRating),date=text(_txtDate);const auto rows=table(_lstDiary,5);
    const Surface *up=nullptr,*down=nullptr;_lstDiary->agentScrollControls(up,down);
    const auto control=[&](const Surface *widget)->bool{
        if(!widget||!agentWidgetUncovered(widget,painted))return false;AgentUiState part;agentIncludeWidget(part,widget);if(part.regions.empty()||!allowed(part))return false;
        const auto old=out.regions.size();out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());if(agentRegionsDisjoint(out))return true;out.regions.resize(old);return false;
    };
    const bool canUp=control(up),canDown=control(down);out.context.push_back(canUp);out.context.push_back(canDown);
    if(canUp||canDown)out.actions.emplace_back("scroll_visible_list","Scroll visible list","visible_control_semantics","scroll_direction");
    if(!out.rows.empty())out.actions.emplace_back("select_diary_mission","Open visible diary mission","visible_control_semantics","project_row");
    std::string buttons="[";
    const auto button=[&](TextButton *widget,const char *id,const char *action){
        const auto label=agentProjectNamedButton(widget,logical,painted,out,allowed);if(label.empty()||label.size()>1024)return;
        if(buttons.size()>1)buttons+=",";buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";out.actions.emplace_back(action,label,"visible_text");
    };
    button(_btnPrev,"previous","previous_personnel");button(_btnNext,"next","next_personnel");
    button(_btnKills,"combat","open_diary_combat");
    button(_btnMissions,"performance","open_diary_performance");
    button(_btnCommendations,"awards","open_diary_awards");
    button(_btnOk,"ok","close_soldier_diary");buttons+="]";
    out.visible="{\"title\":"+title+",\"headings\":["+mission+","+rating+","+date+"],\"rows\":"+rows+",\"scroll\":{\"up\":"+(canUp?"true":"false")+",\"down\":"+(canDown?"true":"false")+"},\"buttons\":"+buttons+"}";
    return out.visible.size()<=32768&&agentRegionsDisjoint(out)&&(!out.actions.empty()||title!="null"||rows!="[]");
}
bool SoldierDiaryOverviewState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;if(!agentUiState(fresh)||!fresh.offers(request.action))return false;
    if(request.action=="previous_personnel"){if(_base)btnPrevClick(nullptr);else btnNextClick(nullptr);return true;}
    if(request.action=="next_personnel"){if(_base)btnNextClick(nullptr);else btnPrevClick(nullptr);return true;}
    if(request.action=="open_diary_combat"){btnKillsClick(nullptr);return true;}
    if(request.action=="open_diary_awards"){btnCommendationsClick(nullptr);return true;}
    if(request.action=="select_diary_mission"){
        for(size_t index=0;index<fresh.rows.size();++index)if(request.choice=="r"+std::to_string(index)){
            const size_t row=fresh.rows[index];return agentMissionRowValid(row)&&_lstDiary->agentReportRowClick(row,this);
        }return false;
    }
    if(request.action=="open_diary_performance"){btnMissionsClick(nullptr);return true;}
    if(request.action=="close_soldier_diary"){btnOkClick(nullptr);return true;}
    if(request.action=="scroll_visible_list"&&fresh.context.size()>=3){
        if(request.choice=="up"&&fresh.context[fresh.context.size()-2]){_lstDiary->scrollUp(false,false);return true;}
        if(request.choice=="down"&&fresh.context.back()){_lstDiary->scrollDown(false,false);return true;}
    }return false;
}
#endif

}
