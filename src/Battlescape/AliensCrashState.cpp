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
#include "AliensCrashState.h"
#include "DebriefingState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Engine/Options.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Aliens Crash screen.
 * @param game Pointer to the core game.
 */
AliensCrashState::AliensCrashState()
{
	// Create objects
	_window = new Window(this, 256, 160, 32, 20);
	_btnOk = new TextButton(120, 18, 100, 154);
	_txtTitle = new Text(246, 80, 37, 50);

	// Set palette
	setPalette("PAL_BATTLESCAPE");

	add(_window, "messageWindowBorder", "battlescape");
	add(_btnOk, "messageWindowButtons", "battlescape");
	add(_txtTitle, "messageWindows", "battlescape");

	centerAllSurfaces();

	// Set up objects
	_window->setHighContrast(true);
	_window->setBackground(_game->getMod()->getSurface("TAC00.SCR"));

	_btnOk->setHighContrast(true);
	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&AliensCrashState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&AliensCrashState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&AliensCrashState::btnOkClick, Options::keyCancel);

	_txtTitle->setHighContrast(true);
	_txtTitle->setText(tr("STR_ALL_ALIENS_KILLED_IN_CRASH"));
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setVerticalAlign(ALIGN_MIDDLE);
	_txtTitle->setBig();
	_txtTitle->setWordWrap(true);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _window->agentTrackPresentation();
    _txtTitle->agentTrackPresentation();
    _btnOk->agentTrackPresentation();
#endif
}

/**
 *
 */
AliensCrashState::~AliensCrashState()
{
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void AliensCrashState::btnOkClick(Action *)
{
	_game->popState();
	_game->pushState(new DebriefingState);
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
std::vector<std::uint64_t> AliensCrashState::agentPaintRevisions() const
{
    return {_window->agentPresentationRevision(), _txtTitle->agentPresentationRevision(),
        _txtTitle->agentTextRevision(), _btnOk->agentPresentationRevision(), _btnOk->agentLabelRevision()};
}
void AliensCrashState::blit()
{
    _agentPainted = false;
    const SDL_Surface *target = _game->getScreen()->getSurface()->getSurface();
    const auto full = [](const SDL_Surface *s) {
        return s && s->clip_rect.x == 0 && s->clip_rect.y == 0 &&
            s->clip_rect.w == s->w && s->clip_rect.h == s->h;
    };
    const bool before = full(target);
    State::blit();
    _agentPaintRevisions = agentPaintRevisions();
    _agentPainted = before && full(target) && Observer::agentPaintHealthy();
}
bool AliensCrashState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<AliensCrashState *>(this)) || _modal || !_agentPainted ||
        !agentPaintHealthy() || _agentPaintRevisions != agentPaintRevisions()) return false;
    const Surface *destination = _game->getScreen()->getSurface();
    if (!_window->agentPresentationPaintedTo(destination)) return false;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    // Current active UI captions only, never crash outcomes read from battle/save data.
    const auto admitted = [&](const Surface *widget) {
        if (!widget->agentPresentationPaintedTo(destination) || !agentWidgetVisible(widget)) return false;
        const AgentUiRect region = {widget->getX(), widget->getY(), widget->getWidth(), widget->getHeight()};
        bool found = false;
        for (const Surface *later : painted)
        {
            if (later == widget) { found = true; continue; }
            if (!found || !later || !later->getVisible()) continue;
            if (region.x < later->getX() + later->getWidth() && region.x + region.width > later->getX() &&
                region.y < later->getY() + later->getHeight() && region.y + region.height > later->getY()) return false;
        }
        return found;
    };
    if (!admitted(_txtTitle) || !admitted(_btnOk)) return false;
    const std::string message = _txtTitle->getText(), label = _btnOk->getText();
    if (message.empty() || message.size() > 1024 || label.empty() || label.size() > 1024) return false;
    out = AgentUiState(); out.screen = "crash_recovery";
    out.visible = "{\"message\":" + agentJsonString(message) + "}";
    out.displayedMessage = message;
    out.actions.emplace_back("acknowledge_crash_recovery", label, "visible_text");
    for (const Surface *widget : {static_cast<const Surface *>(_txtTitle), static_cast<const Surface *>(_btnOk)})
        out.regions.push_back({widget->getX(), widget->getY(), widget->getWidth(), widget->getHeight()});
    out.widgets.emplace_back(_window, _window->agentPresentationRevision());
    out.widgets.emplace_back(_txtTitle, _txtTitle->agentTextRevision());
    out.widgets.emplace_back(_btnOk, _btnOk->agentLabelRevision());
    out.context = _agentPaintRevisions;
    return agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}
bool AliensCrashState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (request.action != "acknowledge_crash_recovery" || !agentUiState(current) || !current.offers(request.action)) return false;
    btnOkClick(nullptr);
    return true;
}
#endif

}
