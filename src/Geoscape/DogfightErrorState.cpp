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
#include "DogfightErrorState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Engine/Options.h"
#include "../Savegame/Craft.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in a Dogfight Error window.
 * @param game Pointer to the core game.
 * @param state Pointer to the Geoscape state.
 * @param msg Error message.
 */
DogfightErrorState::DogfightErrorState(Craft *craft, const std::string &msg) : _craft(craft)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 208, 120, 24, 48, POPUP_BOTH);
	_btnIntercept = new TextButton(180, 12, 38, 128);
	_btnBase = new TextButton(180, 12, 38, 144);
	_txtCraft = new Text(198, 16, 29, 63);
	_txtMessage = new Text(198, 20, 29, 94);

	// Set palette
	setInterface("dogfightInfo");

	add(_window, "window", "dogfightInfo");
	add(_btnIntercept, "button", "dogfightInfo");
	add(_btnBase, "button", "dogfightInfo");
	add(_txtCraft, "text", "dogfightInfo");
	add(_txtMessage, "text", "dogfightInfo");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK15.SCR"));

	_btnIntercept->setText(tr("STR_CONTINUE_INTERCEPTION_PURSUIT"));
	_btnIntercept->onMouseClick((ActionHandler)&DogfightErrorState::btnInterceptClick);
	_btnIntercept->onKeyboardPress((ActionHandler)&DogfightErrorState::btnInterceptClick, Options::keyCancel);

	_btnBase->setText(tr("STR_RETURN_TO_BASE"));
	_btnBase->onMouseClick((ActionHandler)&DogfightErrorState::btnBaseClick);
	_btnBase->onKeyboardPress((ActionHandler)&DogfightErrorState::btnBaseClick, Options::keyOk);

	_txtCraft->setAlign(ALIGN_CENTER);
	_txtCraft->setBig();
	_txtCraft->setText(_craft->getName(_game->getLanguage()));

	_txtMessage->setAlign(ALIGN_CENTER);
	_txtMessage->setWordWrap(true);
	_txtMessage->setText(msg);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *widget : std::vector<Surface *>{_window, _txtCraft, _txtMessage, _btnIntercept, _btnBase})
        widget->agentTrackPresentation();
#endif
}

/**
 *
 */
DogfightErrorState::~DogfightErrorState()
{

}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void DogfightErrorState::btnInterceptClick(Action *)
{
	_game->popState();
}

/**
 * Returns the craft to base.
 * @param action Pointer to an action.
 */
void DogfightErrorState::btnBaseClick(Action *)
{
	_craft->returnToBase();
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool DogfightErrorState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> DogfightErrorState::agentPaintRevisions() const
{
    return {_window->agentPresentationRevision(),
        _txtCraft->agentPresentationRevision(), _txtCraft->agentTextRevision(),
        _txtMessage->agentPresentationRevision(), _txtMessage->agentTextRevision(),
        _btnIntercept->agentPresentationRevision(), _btnIntercept->agentLabelRevision(),
        _btnBase->agentPresentationRevision(), _btnBase->agentLabelRevision()};
}

void DogfightErrorState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool DogfightErrorState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<DogfightErrorState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "dogfight_error";
    out.actor = _craft; // Private authority identity, not serialized craft/world data.
    const Surface *destination = _game->getScreen()->getSurface();
    if (!_window->agentPresentationPaintedTo(destination)) return false;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const auto include = [&](const Surface *widget) -> bool {
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
        if (!found || !_game->agentOverlayClear(part)) return false;
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before);
        return false;
    };
    // These are current active UI captions, not a glyph-pixel readability test.
    // Offscreen/hidden/occluded or stale widgets contribute no text or action.
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        if (widget->getText().size() > 1024 || !widget->getFont() ||
            widget->getTextWidth() > widget->getWidth() ||
            widget->getTextHeight() > widget->getHeight() || !include(widget)) return "null";
        return agentJsonString(widget->getText());
    };
    const auto craftName = text(_txtCraft), message = text(_txtMessage);
    std::string buttons = "[";
    const auto button = [&](TextButton *widget, const char *id, const char *action) {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentLabelRevision());
        const std::string label = widget->agentLayoutLabel();
        if (label.empty() || label.size() > 1024 || !include(widget)) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back(action, label, "visible_text");
    };
    button(_btnIntercept, "continue", "continue_pursuit");
    button(_btnBase, "return", "return_interceptor_to_base");
    buttons += "]";
    out.visible = "{\"craft_name\":" + craftName + ",\"message\":" + message + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool DogfightErrorState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "continue_pursuit") { btnInterceptClick(nullptr); return true; }
    if (request.action == "return_interceptor_to_base") { btnBaseClick(nullptr); return true; }
    return false;
}
#endif

}
