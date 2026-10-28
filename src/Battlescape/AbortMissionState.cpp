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
#include "AbortMissionState.h"
#include <vector>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Engine/Action.h"
#include "../Savegame/SavedBattleGame.h"
#include "BattlescapeState.h"
#include "../Engine/Options.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/MapScript.h"
#include "../Savegame/Tile.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Abort Mission window.
 * @param game Pointer to the core game.
 * @param battleGame Pointer to the saved game.
 * @param state Pointer to the Battlescape state.
 */
AbortMissionState::AbortMissionState(SavedBattleGame *battleGame, BattlescapeState *state) : _battleGame(battleGame), _state(state), _inEntrance(0), _inExit(0), _outside(0)
{
	// Create objects
	_screen = false;
	_window = new Window(this, 320, 144, 0, 0);
	_txtInEntrance = new Text(304, 17, 16, 20);
	_txtInExit = new Text(304, 17, 16, 40);
	_txtOutside = new Text(304, 17, 16, 60);
	_txtAbort = new Text(320, 17, 0, 80);
	_btnOk = new TextButton(120, 16, 16, 110);
	_btnCancel = new TextButton(120, 16, 184, 110);

	// Set palette
	_battleGame->setPaletteByDepth(this);

	add(_window, "messageWindowBorder", "battlescape");
	add(_txtInEntrance, "messageWindows", "battlescape");
	add(_txtInExit, "messageWindows", "battlescape");
	add(_txtOutside, "messageWindows", "battlescape");
	add(_txtAbort, "messageWindows", "battlescape");
	add(_btnOk, "messageWindowButtons", "battlescape");
	add(_btnCancel, "messageWindowButtons", "battlescape");

	// Check available areas (maybe should be cached somewhere)
	bool exit = false, craft = true;
	AlienDeployment *deployment = _game->getMod()->getDeployment(_battleGame->getMissionType());
	if (deployment != 0)
	{
		exit = !deployment->getNextStage().empty() || deployment->getEscapeType() == ESCAPE_EXIT || deployment->getEscapeType() == ESCAPE_EITHER;
		const std::vector<MapScript*> *scripts = _game->getMod()->getMapScript(deployment->getScript());
		if (scripts != 0)
		{
			craft = false;
			for (std::vector<MapScript*>::const_iterator i = scripts->begin(); i != scripts->end(); ++i)
			{
				if ((*i)->getType() == MSC_ADDCRAFT)
				{
					craft = true;
					break;
				}
			}
		}
	}
	if (exit)
	{
		exit = false;
		for (int i = 0; i < _battleGame->getMapSizeXYZ(); ++i)
		{
			Tile *tile = _battleGame->getTiles()[i];
			if (tile && tile->getMapData(O_FLOOR) && tile->getMapData(O_FLOOR)->getSpecialType() == END_POINT)
			{
				exit = true;
				break;
			}
		}
	}

	// Calculate values
	for (std::vector<BattleUnit*>::iterator i = _battleGame->getUnits()->begin(); i != _battleGame->getUnits()->end(); ++i)
	{
		if ((*i)->getOriginalFaction() == FACTION_PLAYER)
		{
			if ((*i)->getStatus() != STATUS_DEAD && (*i)->getStatus() != STATUS_IGNORE_ME)
			{
				Tile *unitTile = _battleGame->getTile((*i)->getPosition());
				if (unitTile)
				{
					MapData *floor = unitTile->getMapData(O_FLOOR);
					if (floor)
					{
						if (floor->getSpecialType() == START_POINT)
						{
							_inEntrance++;
							continue;
						}
						else if (floor->getSpecialType() == END_POINT)
						{
							_inExit++;
							continue;
						}
					}
				}
				_outside++;
			}
		}
	}

	// Set up objects
	_window->setHighContrast(true);
	_window->setBackground(_game->getMod()->getSurface("TAC00.SCR"));

	_txtInEntrance->setBig();
	_txtInEntrance->setHighContrast(true);
	if (craft)
	{
		_txtInEntrance->setText(tr("STR_UNITS_IN_CRAFT", _inEntrance));
	}
	else
	{
		_txtInEntrance->setText(tr("STR_UNITS_IN_ENTRANCE", _inEntrance));
	}

	_txtInExit->setBig();
	_txtInExit->setHighContrast(true);
	_txtInExit->setText(tr("STR_UNITS_IN_EXIT", _inExit));

	_txtOutside->setBig();
	_txtOutside->setHighContrast(true);
	_txtOutside->setText(tr("STR_UNITS_OUTSIDE", _outside));


	if (_battleGame->getMissionType() == "STR_BASE_DEFENSE")
	{
		_txtInEntrance->setVisible(false);
		_txtInExit->setVisible(false);
		_txtOutside->setVisible(false);
	}
	else if (!exit)
	{
		_txtInEntrance->setY(26);
		_txtOutside->setY(54);
		_txtInExit->setVisible(false);
	}

	_txtAbort->setBig();
	_txtAbort->setAlign(ALIGN_CENTER);
	_txtAbort->setHighContrast(true);
	_txtAbort->setText(tr("STR_ABORT_MISSION_QUESTION"));


	_btnOk->setText(tr("STR_OK"));
	_btnOk->setHighContrast(true);
	_btnOk->onMouseClick((ActionHandler)&AbortMissionState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&AbortMissionState::btnOkClick, Options::keyOk);


	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->setHighContrast(true);
	_btnCancel->onMouseClick((ActionHandler)&AbortMissionState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&AbortMissionState::btnCancelClick, Options::keyCancel);
	_btnCancel->onKeyboardPress((ActionHandler)&AbortMissionState::btnCancelClick, Options::keyBattleAbort);

	centerAllSurfaces();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *widget : std::vector<Surface *>{_window, _txtInEntrance, _txtInExit, _txtOutside, _txtAbort, _btnOk, _btnCancel})
        widget->agentTrackPresentation();
#endif
}

/**
 *
 */
AbortMissionState::~AbortMissionState()
{

}

/**
 * Confirms mission abort.
 * @param action Pointer to an action.
 */
void AbortMissionState::btnOkClick(Action *)
{
	_game->popState();
	_battleGame->setAborted(true);
	_state->finishBattle(true, _inExit);
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void AbortMissionState::btnCancelClick(Action *)
{
	_game->popState();
}


#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool AbortMissionState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> AbortMissionState::agentPaintRevisions() const
{
    return {_window->agentPresentationRevision(),
        _txtAbort->agentPresentationRevision(), _txtAbort->agentTextRevision(),
        _txtInEntrance->agentPresentationRevision(), _txtInEntrance->agentTextRevision(),
        _txtInExit->agentPresentationRevision(), _txtInExit->agentTextRevision(),
        _txtOutside->agentPresentationRevision(), _txtOutside->agentTextRevision(),
        _btnOk->agentPresentationRevision(), _btnOk->agentLabelRevision(),
        _btnCancel->agentPresentationRevision(), _btnCancel->agentLabelRevision()};
}

void AbortMissionState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool AbortMissionState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<AbortMissionState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "abort_mission";
    out.actor = _state; // Private callback target identity, never serialized.
    out.context.push_back(reinterpret_cast<std::uintptr_t>(_battleGame));
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
    // Captions are the only count evidence. Never reconstruct from private unit totals.
    const auto prompt = text(_txtAbort), entrance = text(_txtInEntrance),
        exit = text(_txtInExit), outside = text(_txtOutside);
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
    button(_btnOk, "ok", "confirm_abort_mission");
    button(_btnCancel, "cancel", "cancel_abort_mission");
    buttons += "]";
    out.visible = "{\"prompt\":" + prompt + ",\"entrance\":" + entrance +
        ",\"exit\":" + exit + ",\"outside\":" + outside + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool AbortMissionState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "confirm_abort_mission") { btnOkClick(nullptr); return true; }
    if (request.action == "cancel_abort_mission") { btnCancelClick(nullptr); return true; }
    return false;
}
#endif

}
