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
#include "SoldierDiaryPerformanceState.h"
#include "SoldierDiaryOverviewState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/SurfaceSet.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDiary.h"
#include "../Mod/RuleCommendations.h"
#include "../Engine/Action.h"

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
 * Initializes all the elements in the Soldier Diary Totals screen.
 * @param base Pointer to the base to get info from.
 * @param soldier ID of the selected soldier.
 * @param soldierInfoState Pointer to the Soldier Diary screen.
 * @param display Type of totals to display.
 */
SoldierDiaryPerformanceState::SoldierDiaryPerformanceState(Base *base, size_t soldierId, SoldierDiaryOverviewState *soldierDiaryOverviewState, SoldierDiaryDisplay display) : _base(base), _soldierId(soldierId), _soldierDiaryOverviewState(soldierDiaryOverviewState), _display(display), _lastScrollPos(0)
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
	_btnPrev = new TextButton(28, 14, 8, 8);
	_btnNext = new TextButton(28, 14, 284, 8);
	_btnKills = new TextButton(70, 16, 8, 176);
	_btnMissions = new TextButton(70, 16, 86, 176);
	_btnCommendations = new TextButton(70, 16, 164, 176);
	_btnOk = new TextButton(70, 16, 242, 176);
	_txtTitle = new Text(310, 16, 5, 8);
	_lstPerformance = new TextList(288, 128, 8, 28);
	_lstKillTotals = new TextList(302, 9, 8, 164);
	_lstMissionTotals = new TextList(302, 9, 8, 164);
	// Commendation stats
	_txtMedalName = new Text(120, 18, 16, 36);
	_txtMedalLevel = new Text(120, 18, 186, 36);
	_txtMedalInfo = new Text(280, 32, 20, 135);
	_lstCommendations = new TextList(240, 80, 48, 52);
	for (int i = 0; i != 10; ++i)
	{
		_commendations.push_back(new Surface(31, 8, 16, 52 + 8*i));
		_commendationDecorations.push_back(new Surface(31, 8, 16, 52 + 8*i));
	}

	// Set palette
	setInterface("soldierDiaryPerformance");

	add(_window, "window", "soldierDiaryPerformance");
	add(_btnOk, "button", "soldierDiaryPerformance");
	add(_btnKills, "button", "soldierDiaryPerformance");
	add(_btnMissions, "button", "soldierDiaryPerformance");
	add(_btnCommendations, "button", "soldierDiaryPerformance");
	add(_btnPrev, "button", "soldierDiaryPerformance");
	add(_btnNext, "button", "soldierDiaryPerformance");
	add(_txtTitle, "text1", "soldierDiaryPerformance");
	add(_lstPerformance, "list", "soldierDiaryPerformance");
	add(_lstKillTotals, "text2", "soldierDiaryPerformance");
	add(_lstMissionTotals, "text2", "soldierDiaryPerformance");
	add(_txtMedalName, "text2", "soldierDiaryPerformance");
	add(_txtMedalLevel, "text2", "soldierDiaryPerformance");
	add(_txtMedalInfo, "text2", "soldierDiaryPerformance");
	add(_lstCommendations, "list", "soldierDiaryPerformance");
	for (int i = 0; i != 10; ++i)
	{
		add(_commendations[i]);
		add(_commendationDecorations[i]);
	}

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK02.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&SoldierDiaryPerformanceState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&SoldierDiaryPerformanceState::btnOkClick, Options::keyCancel);

	_btnKills->setText(tr("STR_COMBAT"));
	_btnKills->onMouseClick((ActionHandler)&SoldierDiaryPerformanceState::btnKillsToggle);

	_btnMissions->setText(tr("STR_PERFORMANCE"));
	_btnMissions->onMouseClick((ActionHandler)&SoldierDiaryPerformanceState::btnMissionsToggle);

	_btnCommendations->setText(tr("STR_AWARDS"));
	_btnCommendations->onMouseClick((ActionHandler)&SoldierDiaryPerformanceState::btnCommendationsToggle);

	_btnPrev->setText("<<");
	if (_base == 0)
	{
		_btnPrev->onMouseClick((ActionHandler)&SoldierDiaryPerformanceState::btnNextClick);
		_btnPrev->onKeyboardPress((ActionHandler)&SoldierDiaryPerformanceState::btnNextClick, Options::keyBattlePrevUnit);
	}
	else
	{
		_btnPrev->onMouseClick((ActionHandler)&SoldierDiaryPerformanceState::btnPrevClick);
		_btnPrev->onKeyboardPress((ActionHandler)&SoldierDiaryPerformanceState::btnPrevClick, Options::keyBattlePrevUnit);
	}

	_btnNext->setText(">>");
	if (_base == 0)
	{
		_btnNext->onMouseClick((ActionHandler)&SoldierDiaryPerformanceState::btnPrevClick);
		_btnNext->onKeyboardPress((ActionHandler)&SoldierDiaryPerformanceState::btnPrevClick, Options::keyBattleNextUnit);
	}
	else
	{
		_btnNext->onMouseClick((ActionHandler)&SoldierDiaryPerformanceState::btnNextClick);
		_btnNext->onKeyboardPress((ActionHandler)&SoldierDiaryPerformanceState::btnNextClick, Options::keyBattleNextUnit);
	}

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);

	// Text is decided in init()
	_lstPerformance->setColumns(2, 273, 15);
	_lstPerformance->setDot(true);

	_lstKillTotals->setColumns(4, 72, 72, 72, 86);

	_lstMissionTotals->setColumns(4, 72, 72, 72, 86);

	_txtMedalName->setText(tr("STR_MEDAL_NAME"));

	_txtMedalLevel->setText(tr("STR_MEDAL_DECOR_LEVEL"));

	_txtMedalInfo->setWordWrap(true);

	_lstCommendations->setColumns(2, 138, 100);
	_lstCommendations->setSelectable(true);
	_lstCommendations->setBackground(_window);
	_lstCommendations->onMouseOver((ActionHandler)&SoldierDiaryPerformanceState::lstInfoMouseOver);
	_lstCommendations->onMouseOut((ActionHandler)&SoldierDiaryPerformanceState::lstInfoMouseOut);
	_lstCommendations->onMousePress((ActionHandler)&SoldierDiaryPerformanceState::handle);

	if (_display == DIARY_KILLS)
	{
		_group = _btnKills;
	}
	else if (_display == DIARY_MISSIONS)
	{
		_group = _btnMissions;
	}
	else if (_display == DIARY_COMMENDATIONS)
	{
		_group = _btnCommendations;
	}
	_btnKills->setGroup(&_group);
	_btnMissions->setGroup(&_group);
	_btnCommendations->setGroup(&_group);

	init(); // Populate the list
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
    for (TextButton *button : {_btnPrev, _btnNext, _btnKills, _btnMissions, _btnCommendations, _btnOk}) button->agentEnableNamedControl();
    _lstPerformance->agentEnableReportCells(2, true, true);
    _lstMissionTotals->agentEnableReportCells(4, false, true);
    _lstKillTotals->agentEnableReportCells(4, true, true);
    _lstCommendations->agentEnableReportCells(2, false, true);
    if (_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if (_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
#endif
}

/**
 *
 */
SoldierDiaryPerformanceState::~SoldierDiaryPerformanceState()
{

}

/**
 *  Clears all the variables and reinitializes the list of kills or missions for the soldier.
 */
void SoldierDiaryPerformanceState::init()
{
	State::init();
	// Clear sprites
	for (int i = 0; i != 10; ++i)
	{
		_commendations[i]->clear();
		_commendationDecorations[i]->clear();
	}
	// Reset scroll depth for lists
	_lstPerformance->scrollTo(0);
	_lstKillTotals->scrollTo(0);
	_lstMissionTotals->scrollTo(0);
	_lstCommendations->scrollTo(0);
	_lastScrollPos = 0;
	_lstPerformance->setVisible(_display != DIARY_COMMENDATIONS);
	// Set visibility for kills
	_lstKillTotals->setVisible(_display == DIARY_KILLS);
	// Set visibility for missions
	_lstMissionTotals->setVisible(_display == DIARY_MISSIONS);
	// Set visibility for commendations
	_txtMedalName->setVisible(_display == DIARY_COMMENDATIONS);
	_txtMedalLevel->setVisible(_display == DIARY_COMMENDATIONS);
	_txtMedalInfo->setVisible(_display == DIARY_COMMENDATIONS);
	_lstCommendations->setVisible(_display == DIARY_COMMENDATIONS);
	_btnCommendations->setVisible(!_game->getMod()->getCommendationsList().empty());

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
	_lstKillTotals->clearList();
	_lstMissionTotals->clearList();
	_commendationsListEntry.clear();
	_txtTitle->setText(_soldier->getName());
	_lstPerformance->clearList();
	_lstCommendations->clearList();
	if (_display == DIARY_KILLS)
	{
		std::map<std::string, int> mapArray[] = { _soldier->getDiary()->getAlienRaceTotal(), _soldier->getDiary()->getAlienRankTotal(), _soldier->getDiary()->getWeaponTotal() };
		std::string titleArray[] = { "STR_NEUTRALIZATIONS_BY_RACE", "STR_NEUTRALIZATIONS_BY_RANK", "STR_NEUTRALIZATIONS_BY_WEAPON" };

		for (int i = 0; i != 3; ++i)
		{
			_lstPerformance->addRow(1, tr(titleArray[i]).c_str());
			_lstPerformance->setRowColor(_lstPerformance->getRows() - 1, _lstPerformance->getSecondaryColor());
			for (std::map<std::string, int>::const_iterator j = mapArray[i].begin(); j != mapArray[i].end(); ++j)
			{
				std::ostringstream ss;
				ss << (*j).second;
				_lstPerformance->addRow(2, tr((*j).first).c_str(), ss.str().c_str());
			}
			if (i != 2)
			{
				_lstPerformance->addRow(1, "");
			}
		}

		if (_soldier->getCurrentStats()->psiSkill > 0 || (Options::psiStrengthEval && _game->getSavedGame()->isResearched(_game->getMod()->getPsiRequirements())))
		{
			_lstKillTotals->addRow(4, tr("STR_KILLS").arg(_soldier->getDiary()->getKillTotal()).c_str(),
										tr("STR_STUNS").arg(_soldier->getDiary()->getStunTotal()).c_str(),
										tr("STR_DIARY_ACCURACY").arg(_soldier->getDiary()->getAccuracy()).c_str(),
										tr("STR_MINDCONTROLS").arg(_soldier->getDiary()->getControlTotal()).c_str());
		}
		else
		{
			_lstKillTotals->addRow(3, tr("STR_KILLS").arg(_soldier->getDiary()->getKillTotal()).c_str(),
										tr("STR_STUNS").arg(_soldier->getDiary()->getStunTotal()).c_str(),
										tr("STR_DIARY_ACCURACY").arg(_soldier->getDiary()->getAccuracy()).c_str());
		}

	}
	else if (_display == DIARY_MISSIONS)
	{
		std::map<std::string, int> mapArray[] = { _soldier->getDiary()->getRegionTotal(_game->getSavedGame()->getMissionStatistics()), _soldier->getDiary()->getTypeTotal(_game->getSavedGame()->getMissionStatistics()), _soldier->getDiary()->getUFOTotal(_game->getSavedGame()->getMissionStatistics()) };
		std::string titleArray[] = { "STR_MISSIONS_BY_LOCATION", "STR_MISSIONS_BY_TYPE", "STR_MISSIONS_BY_UFO" };

		for (int i = 0; i != 3; ++i)
		{
			_lstPerformance->addRow(1, tr(titleArray[i]).c_str());
			_lstPerformance->setRowColor(_lstPerformance->getRows() - 1, _lstPerformance->getSecondaryColor());
			for (std::map<std::string, int>::const_iterator j = mapArray[i].begin(); j != mapArray[i].end(); ++j)
			{
				if ((*j).first == "NO_UFO") continue;
				std::ostringstream ss;
				ss << (*j).second;
				_lstPerformance->addRow(2, tr((*j).first).c_str(), ss.str().c_str());
			}
			if (i != 2)
			{
				_lstPerformance->addRow(1, "");
			}
		}

		_lstMissionTotals->addRow(4, tr("STR_MISSIONS").arg(_soldier->getDiary()->getMissionTotal()).c_str(),
									tr("STR_WINS").arg(_soldier->getDiary()->getWinTotal(_game->getSavedGame()->getMissionStatistics())).c_str(),
									tr("STR_SCORE_VALUE").arg(_soldier->getDiary()->getScoreTotal(_game->getSavedGame()->getMissionStatistics())).c_str(),
									tr("STR_DAYS_WOUNDED").arg(_soldier->getDiary()->getDaysWoundedTotal()).c_str());
	}
	else if (_display == DIARY_COMMENDATIONS && !_game->getMod()->getCommendationsList().empty())
	{
		for (std::vector<SoldierCommendations*>::const_iterator i = _soldier->getDiary()->getSoldierCommendations()->begin(); i != _soldier->getDiary()->getSoldierCommendations()->end(); ++i)
		{
			RuleCommendations *commendation = _game->getMod()->getCommendation((*i)->getType());
			if ((*i)->getNoun() != "noNoun")
			{
				_lstCommendations->addRow(2, tr((*i)->getType()).arg(tr((*i)->getNoun())).c_str(), tr((*i)->getDecorationDescription()).c_str());
				_commendationsListEntry.push_back(tr(commendation->getDescription()).arg(tr((*i)->getNoun())));
			}
			else
			{
				_lstCommendations->addRow(2, tr((*i)->getType()).c_str(), tr((*i)->getDecorationDescription()).c_str());
				_commendationsListEntry.push_back(tr(commendation->getDescription()));
			}
		}
		drawSprites();
	}
}

/**
 * Draws sprites
 *
 */
void SoldierDiaryPerformanceState::drawSprites()
{
	if (_display != DIARY_COMMENDATIONS) return;

	// Commendation sprites
	_commendationSprite = _game->getMod()->getSurfaceSet("Commendations");
	_commendationDecoration = _game->getMod()->getSurfaceSet("CommendationDecorations");

	// Clear sprites
	for (int i = 0; i != 10; ++i)
	{
		_commendations[i]->clear();
		_commendationDecorations[i]->clear();
	}

	int vectorIterator = 0; // Where we are currently located in the vector
	int scrollDepth = _lstCommendations->getScroll(); // So we know where to start

	for (std::vector<SoldierCommendations*>::const_iterator i = _list->at(_soldierId)->getDiary()->getSoldierCommendations()->begin() ; i != _list->at(_soldierId)->getDiary()->getSoldierCommendations()->end() ; ++i)
	{
		RuleCommendations *commendation = _game->getMod()->getCommendation((*i)->getType());
		// Skip commendations that are not visible in the textlist
		if ( vectorIterator < scrollDepth || vectorIterator - scrollDepth >= (int)_commendations.size())
		{
			vectorIterator++;
			continue;
		}

		int _sprite = commendation->getSprite();
		int _decorationSprite = (*i)->getDecorationLevelInt();

		// Handle commendation sprites
		_commendationSprite->getFrame(_sprite)->setX(0);
		_commendationSprite->getFrame(_sprite)->setY(0);
		_commendationSprite->getFrame(_sprite)->blit(_commendations[vectorIterator - scrollDepth]);

		// Handle commendation decoration sprites
		if (_decorationSprite != 0)
		{
			_commendationDecoration->getFrame(_decorationSprite)->setX(0);
			_commendationDecoration->getFrame(_decorationSprite)->setY(0);
			_commendationDecoration->getFrame(_decorationSprite)->blit(_commendationDecorations[vectorIterator - scrollDepth]);
		}

		vectorIterator++;
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void SoldierDiaryPerformanceState::btnOkClick(Action *)
{
	_soldierDiaryOverviewState->setSoldierId(_soldierId);
	_game->popState();
}

/**
 * Goes to the previous soldier.
 * @param action Pointer to an action.
 */
void SoldierDiaryPerformanceState::btnPrevClick(Action *)
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
void SoldierDiaryPerformanceState::btnNextClick(Action *)
{
	_soldierId++;
	if (_soldierId >= _list->size())
		_soldierId = 0;
	init();
}

/**
 * Display Kills totals.
 */
void SoldierDiaryPerformanceState::btnKillsToggle(Action *)
{
	_display = DIARY_KILLS;
	init();
}

/**
 * Display Missions totals.
 */
void SoldierDiaryPerformanceState::btnMissionsToggle(Action *)
{
	_display = DIARY_MISSIONS;
	init();
}

/**
 * Display Commendations.
 */
void SoldierDiaryPerformanceState::btnCommendationsToggle(Action *)
{
	_display = DIARY_COMMENDATIONS;
	init();
}

/*
 *
 */
void SoldierDiaryPerformanceState::lstInfoMouseOver(Action *)
{
	size_t _sel;
	_sel = _lstCommendations->getSelectedRow();

	if ( _commendationsListEntry.empty() || _sel > _commendationsListEntry.size() - 1)
	{
		_txtMedalInfo->setText("");
	}
	else
	{
		_txtMedalInfo->setText(_commendationsListEntry[_sel]);
	}
}

/*
 *  Clears the Medal information
 */
void SoldierDiaryPerformanceState::lstInfoMouseOut(Action *)
{
	_txtMedalInfo->setText("");
}

/**
 * Runs state functionality every cycle.
 * Used to update sprite vector
 */
void SoldierDiaryPerformanceState::think()
{
	State::think();

	if ((unsigned int)_lastScrollPos != _lstCommendations->getScroll())
	{
		drawSprites();
		_lastScrollPos = _lstCommendations->getScroll();
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool SoldierDiaryPerformanceState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> SoldierDiaryPerformanceState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    for(const Text *text:{_txtTitle,_txtMedalName,_txtMedalLevel,_txtMedalInfo})revisions.push_back(text->agentTextRevision());
    revisions.push_back(_lstPerformance->agentListRevision());
    revisions.push_back(_lstMissionTotals->agentListRevision());
    revisions.push_back(_lstKillTotals->agentListRevision());
    revisions.push_back(_lstCommendations->agentListRevision());
    for (const TextButton *button : {_btnPrev, _btnNext, _btnKills, _btnMissions, _btnCommendations, _btnOk}) revisions.push_back(button->agentLabelRevision());
    return revisions;
}
void SoldierDiaryPerformanceState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool SoldierDiaryPerformanceState::agentDeadRosterValid() const
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
bool SoldierDiaryPerformanceState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(SoldierDiaryPerformanceState) || (_display != DIARY_MISSIONS && _display != DIARY_KILLS && _display != DIARY_COMMENDATIONS) || _modal || _window->agentPopupActive() ||
        (_base ? (!_list || _list != _base->getSoldiers()) : !agentDeadRosterValid()) || !_soldier || _soldierId >= _list->size() || _list->at(_soldierId) != _soldier ||
        !_game->isState(const_cast<SoldierDiaryPerformanceState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out=AgentUiState();out.screen=_base?"soldier_diary_performance":"memorial_soldier_diary_performance";out.actor=_soldier;out.save=_base?static_cast<const void*>(_base):static_cast<const void*>(_game->getSavedGame());out.context.push_back(_soldierId);
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
            }value+="]}";if(!witnessed)continue;if(values.size()>1)values+=",";values+=value;if(values.size()>16000)return "[]";
        }return values+"]";
    };
    const bool awards=_display==DIARY_COMMENDATIONS;
    const auto rows=table(awards?_lstCommendations:_lstPerformance,2);
    const auto totals=awards?std::string("[]"):table(_display==DIARY_KILLS?_lstKillTotals:_lstMissionTotals,4);
    const auto headingName=awards?text(_txtMedalName):std::string("null"),headingLevel=awards?text(_txtMedalLevel):std::string("null");
    const auto description=awards?text(_txtMedalInfo):std::string("null");
    const Surface *up=nullptr,*down=nullptr;(awards?_lstCommendations:_lstPerformance)->agentScrollControls(up,down);
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
    button(_btnPrev,"previous","previous_personnel");button(_btnNext,"next","next_personnel");
    button(_btnKills,"combat","open_diary_combat");button(_btnMissions,"performance","open_diary_performance");button(_btnCommendations,"awards","open_diary_awards");
    
    button(_btnOk,"ok","close_diary_performance");buttons+="]";
    out.visible="{\"mode\":"+agentJsonString(_display==DIARY_KILLS?"combat":(awards?"awards":"missions"))+",\"headings\":["+headingName+","+headingLevel+"],\"description\":"+description+",\"title\":"+title+",\"rows\":"+rows+",\"totals\":"+totals+",\"scroll\":{\"up\":"+(canUp?"true":"false")+",\"down\":"+(canDown?"true":"false")+"},\"buttons\":"+buttons+"}";
    return out.visible.size()<=32768&&agentRegionsDisjoint(out)&&(!out.actions.empty()||title!="null"||rows!="[]");
}
bool SoldierDiaryPerformanceState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;if(!agentUiState(fresh)||!fresh.offers(request.action))return false;
    if(request.action=="previous_personnel"){if(_base)btnPrevClick(nullptr);else btnNextClick(nullptr);return true;}
    if(request.action=="next_personnel"){if(_base)btnNextClick(nullptr);else btnPrevClick(nullptr);return true;}
    
    if(request.action=="open_diary_combat" || request.action=="open_diary_performance" || request.action=="open_diary_awards") {
        TextButton *button=request.action=="open_diary_combat"?_btnKills:(request.action=="open_diary_awards"?_btnCommendations:_btnMissions);
        const int x=button->getX()+button->getWidth()/2,y=button->getY()+button->getHeight()/2;
        SDL_Event event={};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;event.button.x=x;event.button.y=y;
        Action press(&event,1.0,1.0,0,0);press.setSender(button);press.setMouseAction(x,y,button->getX(),button->getY());
        button->mousePress(&press,this); // Ordinary native radio selection precedes the registered toggle.
        event.type=SDL_MOUSEBUTTONUP;
        Action release(&event,1.0,1.0,0,0);release.setSender(button);release.setMouseAction(x,y,button->getX(),button->getY());
        button->mouseRelease(&release,this);button->mouseClick(&release,this);return true;
    }
    if(request.action=="close_diary_performance"){btnOkClick(nullptr);return true;}
    if(request.action=="scroll_visible_list"&&fresh.context.size()>=3){
        if(request.choice=="up"&&fresh.context[fresh.context.size()-2]){(_display==DIARY_COMMENDATIONS?_lstCommendations:_lstPerformance)->scrollUp(false,false);return true;}
        if(request.choice=="down"&&fresh.context.back()){(_display==DIARY_COMMENDATIONS?_lstCommendations:_lstPerformance)->scrollDown(false,false);return true;}
    }return false;
}
#endif

}
