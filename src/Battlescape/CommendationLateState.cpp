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
#include "CommendationLateState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDiary.h"
#include "../Engine/Options.h"
#include "../Mod/RuleCommendations.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Medals screen.
 * @param soldiersMedalled List of soldiers with medals.
 */
CommendationLateState::CommendationLateState(std::vector<Soldier*> soldiersMedalled)
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(288, 16, 16, 176);
	_txtTitle = new Text(300, 16, 10, 8);
	_lstSoldiers = new TextList(288, 128, 8, 32);

	// Set palette
	setInterface("commendationsLate");

	add(_window, "window", "commendationsLate");
	add(_btnOk, "button", "commendationsLate");
	add(_txtTitle, "text", "commendationsLate");
	add(_lstSoldiers, "list", "commendationsLate");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK02.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&CommendationLateState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&CommendationLateState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&CommendationLateState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_LOST_IN_SERVICE"));

	_lstSoldiers->setColumns(5, 51, 51, 51, 51, 84);
	_lstSoldiers->setSelectable(true);
	_lstSoldiers->setBackground(_window);
	_lstSoldiers->setMargin(8);
	_lstSoldiers->setFlooding(true);

    /***

                                            LOST IN SERVICE

    SOLDIER NAME, RANK: ___, SCORE: ___, KILLS: ___, NUMBER OF MISSIONS: ___, DAYS WOUNDED: ___, TIMES HIT: ___
      COMMENDATION
      COMMENDATION
      COMMENDATION
      CAUSE OF DEATH: KILLED BY ALIEN_RACE ALIEN_RANK, USING WEAPON


    ***/

	const std::map<std::string, RuleCommendations *> commendationsList = _game->getMod()->getCommendationsList();
	bool modularCommendation;
	std::string noun;



	// Loop over dead soldiers
	for (std::vector<Soldier*>::iterator s = soldiersMedalled.begin() ; s != soldiersMedalled.end(); ++s)
	{
		// Establish some base information
		_lstSoldiers->addRow(5, (*s)->getName().c_str(),
								L"",
								tr((*s)->getRankString()).c_str(),
								L"",
								tr("STR_KILLS").arg((*s)->getDiary()->getKillTotal()).c_str());

		// Loop over all commendations
		for (std::map<std::string, RuleCommendations *>::const_iterator commList = commendationsList.begin(); commList != commendationsList.end();)
		{
			std::ostringstream wssCommendation;
			modularCommendation = false;
			noun = "noNoun";

			// Loop over soldier's commendations
			for (std::vector<SoldierCommendations*>::const_iterator soldierComm = (*s)->getDiary()->getSoldierCommendations()->begin(); soldierComm != (*s)->getDiary()->getSoldierCommendations()->end(); ++soldierComm)
			{
				if ((*soldierComm)->getType() == (*commList).first && (*soldierComm)->isNew() && noun == "noNoun")
				{
					(*soldierComm)->makeOld();

					if ((*soldierComm)->getNoun() != "noNoun")
					{
						noun = (*soldierComm)->getNoun();
						modularCommendation = true;
					}
					// Decoration level name
					int skipCounter = 0;
					int lastInt = -2;
					int thisInt = -1;
					int vectorIterator = 0;
					for (std::vector<int>::const_iterator k = (*commList).second->getCriteria()->begin()->second.begin(); k != (*commList).second->getCriteria()->begin()->second.end(); ++k)
					{
						if (vectorIterator == (*soldierComm)->getDecorationLevelInt() + 1)
						{
							break;
						}
						thisInt = *k;
						if (k != (*commList).second->getCriteria()->begin()->second.begin())
						{
							--k;
							lastInt = *k;
							++k;
						}
						if (thisInt == lastInt)
						{
							skipCounter++;
						}
						vectorIterator++;
					}
					// Establish comms name
					// Medal name
					wssCommendation << "   ";
					if (modularCommendation)
					{
						wssCommendation << tr((*commList).first).arg(tr(noun));
					}
					else
					{
						wssCommendation << tr((*commList).first);
					}
					_lstSoldiers->addRow(5, wssCommendation.str().c_str(), "", "", "", tr((*soldierComm)->getDecorationLevelName(skipCounter)).c_str());
					break;
				}
			} // END SOLDIER COMMS LOOP

			if (noun == "noNoun")
			{
				++commList;
			}
		} // END COMMS LOOPS
	} // END SOLDIER LOOP
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *widget : std::vector<Surface *>{_window, _txtTitle, _btnOk, _lstSoldiers})
        widget->agentTrackPresentation();
#endif
}

/**
 *
 */
CommendationLateState::~CommendationLateState()
{
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void CommendationLateState::btnOkClick(Action *)
{
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool CommendationLateState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> CommendationLateState::agentPaintRevisions() const
{
    return {_window->agentPresentationRevision(),
        _txtTitle->agentPresentationRevision(), _txtTitle->agentTextRevision(),
        _btnOk->agentPresentationRevision(), _btnOk->agentLabelRevision(),
        _lstSoldiers->agentPresentationRevision(), _lstSoldiers->agentListRevision()};
}

void CommendationLateState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool CommendationLateState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<CommendationLateState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "lost_in_service";
    out.actor = this; // Private active-popup identity, never serialized.
    const Surface *destination = _game->getScreen()->getSurface();
    if (!_window->agentPresentationPaintedTo(destination)) return false;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const auto include = [&](const Surface *widget, bool observationOnly) -> bool {
        if (!widget->agentPresentationPaintedTo(destination) || !agentWidgetVisible(widget)) return false;
        AgentUiState part;
        part.regions.push_back({widget->getX(), widget->getY(), widget->getWidth(), widget->getHeight()});
        // Allocation geometry only: do not require glyph-ink evidence for text.
        bool found = false;
        for (const Surface *later : painted)
        {
            if (later == widget) { found = true; continue; }
            if (!found || !later || !later->getVisible()) continue;
            const auto &rect = part.regions.front();
            if (rect.x < later->getX() + later->getWidth() && rect.x + rect.width > later->getX() &&
                rect.y < later->getY() + later->getHeight() && rect.y + rect.height > later->getY()) return false;
        }
        if (!found || !(observationOnly ? _game->agentNonCursorOverlayClear(part) : _game->agentOverlayClear(part))) return false;
        if (observationOnly) return true;
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before);
        return false;
    };
    // These are current active UI captions, not a glyph-pixel readability test.
    // Hidden, geometrically absent, occluded or stale widgets contribute no value.
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        out.context.insert(out.context.end(), {
            static_cast<std::uint64_t>(widget->getX()), static_cast<std::uint64_t>(widget->getY()),
            static_cast<std::uint64_t>(widget->getWidth()), static_cast<std::uint64_t>(widget->getHeight())});
        // Active logical captions: glyph/ink completeness and pointer overlap
        // are not text admission criteria. Resource bound is not a layout test.
        if (widget->getText().size() > 1024 || !include(widget, true)) return "null";
        return agentJsonString(widget->getText());
    };
    const auto title = text(_txtTitle);
    out.widgets.emplace_back(_lstSoldiers, _lstSoldiers->agentPresentationRevision());
    out.widgets.emplace_back(_lstSoldiers, _lstSoldiers->agentListRevision());
    out.context.insert(out.context.end(), {static_cast<std::uint64_t>(_lstSoldiers->getX()),
        static_cast<std::uint64_t>(_lstSoldiers->getY()), static_cast<std::uint64_t>(_lstSoldiers->getWidth()),
        static_cast<std::uint64_t>(_lstSoldiers->getHeight())});
    // This increment exposes only the painted title and ordinary OK control.
    // The five-column memorial/medals list is deliberately not reconstructed
    // from Soldier or Diary objects. Constructor makeOld effects are unchanged.
    std::string buttons = "[";
    const auto button = [&](TextButton *widget, const char *id, const char *action) {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentLabelRevision());
        const std::string label = widget->getText();
        if (label.empty() || label.size() > 1024 || !include(widget, false)) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back(action, label, "visible_text");
    };
    button(_btnOk, "ok", "acknowledge_lost_in_service");
    buttons += "]";
    out.visible = "{\"title\":" + title + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool CommendationLateState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "acknowledge_lost_in_service") { btnOkClick(nullptr); return true; }
    return false;
}
#endif

}
