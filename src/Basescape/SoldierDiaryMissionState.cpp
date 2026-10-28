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
#include "SoldierDiaryMissionState.h"
#include "../Mod/Mod.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDiary.h"
#include "../Savegame/MissionStatistics.h"
#include "../Savegame/BattleUnitStatistics.h"

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
 * Initializes all the elements in the Soldier Diary Mission window.
 * @param soldier Pointer to the selected soldier.
 * @param rowEntry number to get mission info from.
 */
SoldierDiaryMissionState::SoldierDiaryMissionState(Soldier *soldier, int rowEntry) : _soldier(soldier), _rowEntry(rowEntry)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 300, 128, 10, 36, POPUP_HORIZONTAL);
	_btnOk = new TextButton(240, 16, 40, 140);
	_btnPrev = new TextButton(28, 14, 18, 44);
	_btnNext = new TextButton(28, 14, 274, 44);
	_txtTitle = new Text(262, 9, 29, 44);
	_txtUFO = new Text(262, 9, 29, 52);
	_txtScore = new Text(180, 9, 29, 68);
	_txtKills = new Text(120, 9, 169, 68);
	_txtLocation = new Text(180, 9, 29, 76);
	_txtRace = new Text(120, 9, 169, 76);
	_txtDaylight = new Text(120, 9, 169, 84);
	_txtDaysWounded = new Text(180, 9, 29, 84);
	_txtNoRecord = new Text(240, 9, 29, 100);
	_lstKills = new TextList(270, 32, 20, 100);

	// Set palette
	setInterface("soldierDiaryMission");

	add(_window, "window", "soldierDiaryMission");
	add(_btnOk, "button", "soldierDiaryMission");
	add(_btnPrev, "button", "soldierDiaryMission");
	add(_btnNext, "button", "soldierDiaryMission");
	add(_txtTitle, "text", "soldierDiaryMission");
	add(_txtUFO, "text", "soldierDiaryMission");
	add(_txtScore, "text", "soldierDiaryMission");
	add(_txtKills, "text", "soldierDiaryMission");
	add(_txtLocation, "text", "soldierDiaryMission");
	add(_txtRace, "text", "soldierDiaryMission");
	add(_txtDaylight, "text", "soldierDiaryMission");
	add(_txtDaysWounded, "text", "soldierDiaryMission");
	add(_txtNoRecord, "text", "soldierDiaryMission");
	add(_lstKills, "list", "soldierDiaryMission");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK16.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&SoldierDiaryMissionState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&SoldierDiaryMissionState::btnOkClick, Options::keyCancel);

	_btnPrev->setText("<<");
	_btnPrev->onMouseClick((ActionHandler)&SoldierDiaryMissionState::btnPrevClick);
	_btnPrev->onKeyboardPress((ActionHandler)&SoldierDiaryMissionState::btnPrevClick, Options::keyBattleNextUnit);

	_btnNext->setText(">>");
	_btnNext->onMouseClick((ActionHandler)&SoldierDiaryMissionState::btnNextClick);
	_btnNext->onKeyboardPress((ActionHandler)&SoldierDiaryMissionState::btnNextClick, Options::keyBattlePrevUnit);

	_txtTitle->setAlign(ALIGN_CENTER);

	_txtUFO->setAlign(ALIGN_CENTER);

	_lstKills->setColumns(3, 60, 110, 100);

	init(); // Populate the list
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
    for (TextButton *button : {_btnPrev, _btnNext, _btnOk}) button->agentEnableNamedControl();
    _lstKills->agentEnableReportCells(3, false, true);
    if (_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if (_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
#endif
}

/**
 *
 */
SoldierDiaryMissionState::~SoldierDiaryMissionState()
{

}

/**
 *  Clears all the variables and reinitializes the stats for the mission.
 *
 */
void SoldierDiaryMissionState::init()
{
	State::init();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentMissionValid = false;
    _agentMissionRowEntry = -1;
    _agentMissionIds.clear();
    _agentMissionRecords.clear();
    _agentDestinationPaintFull = false;
#endif
	if (_soldier->getDiary()->getMissionIdList().empty())
	{
		_game->popState();
		return;
	}
	std::vector<MissionStatistics*> *missionStatistics = _game->getSavedGame()->getMissionStatistics();
	unsigned int missionId = _soldier->getDiary()->getMissionIdList().at(_rowEntry);
	if (missionId > missionStatistics->size())
	{
		missionId = 0;
	}
    const auto &injuries = missionStatistics->at(missionId)->injuryList;
    const auto injury = injuries.find(_soldier->getId());
    const int daysWounded = injury == injuries.end() ? 0 : injury->second;

	_lstKills->clearList();
	_txtTitle->setText(tr(missionStatistics->at(missionId)->type));
	if (missionStatistics->at(missionId)->isUfoMission())
	{
		_txtUFO->setText(tr(missionStatistics->at(missionId)->ufo));
	}
	_txtUFO->setVisible(missionStatistics->at(missionId)->isUfoMission());
	_txtScore->setText(tr("STR_SCORE_VALUE").arg(missionStatistics->at(missionId)->score));
	_txtLocation->setText(tr("STR_LOCATION").arg(tr(missionStatistics->at(missionId)->getLocationString())));
	_txtRace->setText(tr("STR_RACE_TYPE").arg(tr(missionStatistics->at(missionId)->alienRace)));
	_txtRace->setVisible(missionStatistics->at(missionId)->alienRace != "STR_UNKNOWN");
	_txtDaylight->setText(tr("STR_DAYLIGHT_TYPE").arg(tr(missionStatistics->at(missionId)->getDaylightString())));
	_txtDaysWounded->setText(tr("STR_DAYS_WOUNDED").arg(daysWounded));
	_txtDaysWounded->setVisible(daysWounded != 0);

	int kills = 0;
	bool stunOrKill = false;

	for (std::vector<BattleUnitKills*>::iterator i = _soldier->getDiary()->getKills().begin() ; i != _soldier->getDiary()->getKills().end() ; ++i)
	{
		if ((unsigned int)(*i)->mission != missionId) continue;

		switch ((*i)->status)
		{
		case STATUS_DEAD:
			kills++;
		//Fall-through
		case STATUS_UNCONSCIOUS:
		case STATUS_PANICKING:
		case STATUS_TURNING:
			stunOrKill = true;
		default:
			break;
		}

		_lstKills->addRow(3, tr((*i)->getKillStatusString()).c_str(),
							 (*i)->getUnitName(_game->getLanguage()).c_str(),
							 tr((*i)->weapon).c_str());
	}

	_txtNoRecord->setAlign(ALIGN_CENTER);
	_txtNoRecord->setText(tr("STR_NO_RECORD"));
	_txtNoRecord->setVisible(!stunOrKill);

	_txtKills->setText(tr("STR_KILLS").arg(kills));
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    // Bind this initialized presentation to the exact private history sequence,
    // not merely its ordinal. A later repaint cannot bless changed model data.
    _agentMissionIds = _soldier->getDiary()->getMissionIdList();
    bool bindingValid = _rowEntry >= 0 && static_cast<size_t>(_rowEntry) < _agentMissionIds.size();
    for (int id : _agentMissionIds) {
        if (id < 0 || static_cast<size_t>(id) >= missionStatistics->size() ||
            !missionStatistics->at(id) || missionStatistics->at(id)->id != id) {
            bindingValid = false;
            break;
        }
        _agentMissionRecords.push_back(missionStatistics->at(id));
    }
    _agentMissionRowEntry = _rowEntry;
    _agentMissionValid = bindingValid;
#endif
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void SoldierDiaryMissionState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Goes to the previous mission.
 * @param action Pointer to an action.
 */
void SoldierDiaryMissionState::btnPrevClick(Action *)
{
	if (_rowEntry == 0)
		_rowEntry = _soldier->getDiary()->getMissionTotal() - 1;
	else
		_rowEntry--;
	init();
}

/**
 * Goes to the next mission.
 * @param action Pointer to an action.
 */
void SoldierDiaryMissionState::btnNextClick(Action *)
{
	_rowEntry++;
	if (_rowEntry >= _soldier->getDiary()->getMissionTotal())
		_rowEntry = 0;
	init();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool SoldierDiaryMissionState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> SoldierDiaryMissionState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    for (const Text *text : {_txtTitle, _txtUFO, _txtScore, _txtKills, _txtLocation, _txtRace, _txtDaylight, _txtDaysWounded, _txtNoRecord}) revisions.push_back(text->agentTextRevision());
    revisions.push_back(_lstKills->agentListRevision());
    revisions.push_back(static_cast<std::uint64_t>(_rowEntry));
    for (const TextButton *button : {_btnPrev, _btnNext, _btnOk}) revisions.push_back(button->agentLabelRevision());
    return revisions;
}
void SoldierDiaryMissionState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool SoldierDiaryMissionState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(SoldierDiaryMissionState) || !_agentMissionValid || _rowEntry != _agentMissionRowEntry || _modal || _window->agentPopupActive() ||
        !_soldier ||
        !_game->isState(const_cast<SoldierDiaryMissionState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out=AgentUiState();out.screen="soldier_diary_mission";out.actor=_soldier;out.context.push_back(_rowEntry);
    // MCP navigation must not reach the native legacy malformed-ID path.
    // Direct constructor lifecycle is deliberately unchanged.
    const auto &ids=_soldier->getDiary()->getMissionIdList();
    const auto *statistics=_game->getSavedGame()->getMissionStatistics();
    if (ids != _agentMissionIds || _agentMissionRecords.size() != ids.size()) return false;
    for (size_t i = 0; i < ids.size(); ++i) {
        const int id = ids[i];
        if (id < 0 || static_cast<size_t>(id) >= statistics->size() ||
            !statistics->at(id) || statistics->at(id)->id != id ||
            statistics->at(id) != _agentMissionRecords[i]) return false;
    }
    bool navigationAllowed=_rowEntry>=0 && static_cast<size_t>(_rowEntry)<ids.size();
    for(int id:ids) if(id<0 || static_cast<size_t>(id)>=statistics->size() || !statistics->at(id) || statistics->at(id)->id!=id) navigationAllowed=false;
    out.context.push_back(navigationAllowed);

    const auto painted=agentFlattenPaintOrder(_surfaces);const Surface *logical=_game->getScreen()->getSurface();
    if(!agentWidgetVisible(_window)||!_window->agentPresentationPaintedTo(logical))return false;
    for(const Surface *surface:painted)out.widgets.emplace_back(surface,surface->agentPresentationRevision());
    if(_game->getCursor())out.widgets.emplace_back(_game->getCursor(),_game->getCursor()->agentPresentationRevision());
    if(_game->getFpsCounter())out.widgets.emplace_back(_game->getFpsCounter(),_game->getFpsCounter()->agentPresentationRevision());
    const auto allowed=[this](const AgentUiState &part){return _game->agentOverlayClear(part);};
    const auto text=[&](const Text *widget)->std::string{
        out.widgets.emplace_back(widget,widget->agentTextRevision());const auto value=agentTextJson(widget);
        if(value=="null"||!widget->agentPresentationPaintedTo(logical))return "null";
        // The native empty kill list is painted after the "No record" caption.
        // Its cleared transparent surface is not an occluding window. Exempt
        // only this exact allocation after certifying its current empty paint;
        // retain its controls and every other later surface in the witness.
        std::vector<Surface *> captionPainted;
        const std::vector<Surface *> *captionOrder=&painted;
        if(widget==_txtNoRecord && _lstKills->getRows()==0 && _lstKills->agentPresentationPaintedTo(logical)) {
            std::vector<AgentOutfittingRow> rows;std::uint64_t epoch=0;
            if(_lstKills->agentReportRows(rows,epoch) && rows.empty()) {
                out.context.push_back(epoch);
                for(Surface *surface:painted)if(surface!=_lstKills)captionPainted.push_back(surface);
                captionOrder=&captionPainted;
            }
        }
        if(!agentWidgetUncovered(widget,*captionOrder))return "null";
        AgentUiState part;agentIncludeWidget(part,widget);return !part.regions.empty()&&allowed(part)?value:"null";
    };
    const auto title=text(_txtTitle), ufo=text(_txtUFO), score=text(_txtScore), kills=text(_txtKills);
    const auto location=text(_txtLocation), race=text(_txtRace), daylight=text(_txtDaylight), daysWounded=text(_txtDaysWounded), noRecord=text(_txtNoRecord);
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
            }value+="]}";if(!witnessed)continue;if(values.size()>1)values+=",";values+=value;if(values.size()>16000)return "[]";
        }return values+"]";
    };
    const auto rows=table(_lstKills,3);
    const Surface *up=nullptr,*down=nullptr;_lstKills->agentScrollControls(up,down);
    const auto control=[&](const Surface *widget)->bool{
        if(!widget||!agentWidgetUncovered(widget,painted))return false;AgentUiState part;agentIncludeWidget(part,widget);if(part.regions.empty()||!allowed(part))return false;
        const auto old=out.regions.size();out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());if(agentRegionsDisjoint(out))return true;out.regions.resize(old);return false;
    };
    const bool canUp=control(up),canDown=control(down);out.context.push_back(canUp);out.context.push_back(canDown);
    if(canUp||canDown)out.actions.emplace_back("scroll_visible_list","Scroll visible list","visible_control_semantics","scroll_direction");
    std::string buttons="[";
    const auto button=[&](TextButton *widget,const char *id,const char *action){
        const auto label=agentProjectNamedButton(widget,logical,painted,out,allowed);if(label.empty()||label.size()>1024)return;
        if(buttons.size()>1)buttons+=",";buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";out.actions.emplace_back(action,label,"visible_text");
    };
    if(navigationAllowed){button(_btnPrev,"previous","previous_diary_mission");button(_btnNext,"next","next_diary_mission");}
    
    button(_btnOk,"ok","close_diary_mission");buttons+="]";
    out.visible="{\"title\":"+title+",\"ufo\":"+ufo+",\"score\":"+score+",\"kills\":"+kills+
        ",\"location\":"+location+",\"race\":"+race+",\"daylight\":"+daylight+",\"days_wounded\":"+daysWounded+
        ",\"no_record\":"+noRecord+",\"rows\":"+rows+",\"scroll\":{\"up\":"+(canUp?"true":"false")+",\"down\":"+(canDown?"true":"false")+"},\"buttons\":"+buttons+"}";
    return out.visible.size()<=32768&&agentRegionsDisjoint(out)&&(!out.actions.empty()||title!="null"||rows!="[]");
}
bool SoldierDiaryMissionState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;if(!agentUiState(fresh)||!fresh.offers(request.action))return false;
    if(request.action=="previous_diary_mission"){btnPrevClick(nullptr);return true;}
    if(request.action=="next_diary_mission"){btnNextClick(nullptr);return true;}
    
    if(request.action=="close_diary_mission"){btnOkClick(nullptr);return true;}
    if(request.action=="scroll_visible_list"&&fresh.context.size()>=3){
        if(request.choice=="up"&&fresh.context[fresh.context.size()-2]){_lstKills->scrollUp(false,false);return true;}
        if(request.choice=="down"&&fresh.context.back()){_lstKills->scrollDown(false,false);return true;}
    }return false;
}
#endif

}
