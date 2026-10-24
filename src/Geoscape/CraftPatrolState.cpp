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
#include "CraftPatrolState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Target.h"
#include "GeoscapeCraftState.h"
#include "../Engine/Options.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Craft Patrol window.
 * @param game Pointer to the core game.
 * @param craft Pointer to the craft to display.
 * @param globe Pointer to the Geoscape globe.
 */
CraftPatrolState::CraftPatrolState(Craft *craft, Globe *globe) : _craft(craft), _globe(globe)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 224, 168, 16, 16, POPUP_BOTH);
	_btnOk = new TextButton(140, 12, 58, 144);
	_btnRedirect = new TextButton(140, 12, 58, 160);
	_txtDestination = new Text(224, 64, 16, 48);
	_txtPatrolling = new Text(224, 17, 16, 120);

	// Set palette
	setInterface("craftPatrol");

	add(_window, "window", "craftPatrol");
	add(_btnOk, "button", "craftPatrol");
	add(_btnRedirect, "button", "craftPatrol");
	add(_txtDestination, "text1", "craftPatrol");
	add(_txtPatrolling, "text1", "craftPatrol");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK12.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&CraftPatrolState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&CraftPatrolState::btnOkClick, Options::keyCancel);

	_btnRedirect->setText(tr("STR_REDIRECT_CRAFT"));
	_btnRedirect->onMouseClick((ActionHandler)&CraftPatrolState::btnRedirectClick);
	_btnRedirect->onKeyboardPress((ActionHandler)&CraftPatrolState::btnRedirectClick, Options::keyOk);

	_txtDestination->setBig();
	_txtDestination->setAlign(ALIGN_CENTER);
	_txtDestination->setWordWrap(true);
	_txtDestination->setText(tr("STR_CRAFT_HAS_REACHED_DESTINATION")
							 .arg(_craft->getName(_game->getLanguage()))
							 .arg(_craft->getDestination()->getName(_game->getLanguage())));

	_txtPatrolling->setBig();
	_txtPatrolling->setAlign(ALIGN_CENTER);
	_txtPatrolling->setText(tr("STR_NOW_PATROLLING"));
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *widget : std::vector<Surface *>{_window, _txtDestination, _txtPatrolling, _btnOk, _btnRedirect})
        widget->agentTrackPresentation();
#endif
}

/**
 *
 */
CraftPatrolState::~CraftPatrolState()
{

}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void CraftPatrolState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Opens up the Craft window.
 * @param action Pointer to an action.
 */
void CraftPatrolState::btnRedirectClick(Action *)
{
	_game->popState();
	_game->pushState(new GeoscapeCraftState(_craft, _globe, 0));
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool CraftPatrolState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> CraftPatrolState::agentPaintRevisions() const
{
    return {_window->agentPresentationRevision(),
        _txtDestination->agentPresentationRevision(), _txtDestination->agentTextRevision(),
        _txtPatrolling->agentPresentationRevision(), _txtPatrolling->agentTextRevision(),
        _btnOk->agentPresentationRevision(), _btnOk->agentLabelRevision(),
        _btnRedirect->agentPresentationRevision(), _btnRedirect->agentLabelRevision()};
}

void CraftPatrolState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool CraftPatrolState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<CraftPatrolState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "craft_patrol";
    out.actor = _globe; // Private callback target identity, never serialized.
    out.context.push_back(reinterpret_cast<std::uintptr_t>(_craft));
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
    const auto title = text(_txtDestination), message = text(_txtPatrolling);
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
    button(_btnOk, "ok", "acknowledge_craft_patrol");
    button(_btnRedirect, "redirect", "redirect_patrolling_craft");
    buttons += "]";
    out.visible = "{\"message\":" + title + ",\"patrolling\":" + message + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool CraftPatrolState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "acknowledge_craft_patrol") { btnOkClick(nullptr); return true; }
    if (request.action == "redirect_patrolling_craft") { btnRedirectClick(nullptr); return true; }
    return false;
}
#endif

}
