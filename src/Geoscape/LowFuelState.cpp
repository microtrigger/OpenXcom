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
#include "LowFuelState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Savegame/Craft.h"
#include "GeoscapeState.h"
#include "../Engine/Options.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Low Fuel window.
 * @param game Pointer to the core game.
 * @param craft Pointer to the craft to display.
 * @param state Pointer to the Geoscape.
 */
LowFuelState::LowFuelState(Craft *craft, GeoscapeState *state) : _craft(craft), _state(state)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 224, 120, 16, 40, POPUP_BOTH);
	_btnOk = new TextButton(90, 18, 30, 120);
	_btnOk5Secs = new TextButton(90, 18, 136, 120);
	_txtTitle = new Text(214, 17, 21, 60);
	_txtMessage = new Text(214, 17, 21, 90);

	// Set palette
	setInterface("lowFuel");

	add(_window, "window", "lowFuel");
	add(_btnOk, "button", "lowFuel");
	add(_btnOk5Secs, "button", "lowFuel");
	add(_txtTitle, "text", "lowFuel");
	add(_txtMessage, "text", "lowFuel");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK12.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&LowFuelState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&LowFuelState::btnOkClick, Options::keyCancel);

	_btnOk5Secs->setText(tr("STR_OK_5_SECONDS"));
	_btnOk5Secs->onMouseClick((ActionHandler)&LowFuelState::btnOk5SecsClick);
	_btnOk5Secs->onKeyboardPress((ActionHandler)&LowFuelState::btnOk5SecsClick, Options::keyOk);

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(_craft->getName(_game->getLanguage()));

	_txtMessage->setAlign(ALIGN_CENTER);
	_txtMessage->setText(tr("STR_IS_LOW_ON_FUEL_RETURNING_TO_BASE"));
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *widget : std::vector<Surface *>{_window, _txtTitle, _txtMessage, _btnOk, _btnOk5Secs})
        widget->agentTrackPresentation();
#endif
}

/**
 *
 */
LowFuelState::~LowFuelState()
{

}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void LowFuelState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Closes the window and sets the timer to 5 Secs.
 * @param action Pointer to an action.
 */
void LowFuelState::btnOk5SecsClick(Action *)
{
	_state->timerReset();
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool LowFuelState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> LowFuelState::agentPaintRevisions() const
{
    return {_window->agentPresentationRevision(),
        _txtTitle->agentPresentationRevision(), _txtTitle->agentTextRevision(),
        _txtMessage->agentPresentationRevision(), _txtMessage->agentTextRevision(),
        _btnOk->agentPresentationRevision(), _btnOk->agentLabelRevision(),
        _btnOk5Secs->agentPresentationRevision(), _btnOk5Secs->agentLabelRevision()};
}

void LowFuelState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool LowFuelState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<LowFuelState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "low_fuel";
    out.actor = _state; // Private callback target identity, never serialized.
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
    const auto title = text(_txtTitle), message = text(_txtMessage);
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
    button(_btnOk, "ok", "acknowledge_low_fuel");
    button(_btnOk5Secs, "ok_5_seconds", "acknowledge_low_fuel_5_seconds");
    buttons += "]";
    out.visible = "{\"title\":" + title + ",\"message\":" + message + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool LowFuelState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "acknowledge_low_fuel") { btnOkClick(nullptr); return true; }
    if (request.action == "acknowledge_low_fuel_5_seconds") { btnOk5SecsClick(nullptr); return true; }
    return false;
}
#endif

}
