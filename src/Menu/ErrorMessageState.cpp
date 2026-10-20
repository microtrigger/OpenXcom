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
#include "ErrorMessageState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Palette.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Engine/Options.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in an error window.
 * @param game Pointer to the core game.
 * @param msg Text string for the message to display.
 * @param palette Parent state palette.
 * @param color Color of the UI controls.
 * @param bg Background image.
 * @param bgColor Background color (-1 for Battlescape).
 */
ErrorMessageState::ErrorMessageState(const std::string &msg, SDL_Color *palette, Uint8 color, const std::string &bg, int bgColor)
{
	create(msg, palette, color, bg, bgColor);
}

/**
 *
 */
ErrorMessageState::~ErrorMessageState()
{

}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ErrorMessageState::agentDestinationFull() const
{
    const auto screen = _game->getScreen();
    const auto canvas = screen ? screen->getSurface() : nullptr;
    const auto surface = canvas ? canvas->getSurface() : nullptr;
    return surface && surface->w > 0 && surface->h > 0 &&
        surface->clip_rect.x == 0 && surface->clip_rect.y == 0 &&
        surface->clip_rect.w == surface->w && surface->clip_rect.h == surface->h;
}

std::vector<std::uint64_t> ErrorMessageState::agentPaintTextRevisions() const
{
    return {_txtMessage->agentTextRevision(), _btnOk->agentLabelRevision()};
}

void ErrorMessageState::blit()
{
    _agentDestinationPaintFull = false;
    const bool full = agentDestinationFull();
    State::blit();
    _agentPaintTextRevisions = agentPaintTextRevisions();
    _agentDestinationPaintFull = full && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool ErrorMessageState::agentUiState(Observer::AgentUiState &out) const
{
    if (!_game->isState(const_cast<ErrorMessageState *>(this)) || _modal || !Observer::agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentPaintTextRevisions != agentPaintTextRevisions()) return false;
    out = Observer::AgentUiState();
    out.screen = "error_message";
    out.widgets.push_back(std::make_pair(_txtMessage, _txtMessage->agentTextRevision()));
    out.widgets.push_back(std::make_pair(_btnOk, _btnOk->agentLabelRevision()));
    const auto include = [&](const std::vector<Observer::AgentUiRect> &regions) -> bool
    {
        Observer::AgentUiState candidate; candidate.regions=regions;
        if (!Observer::agentRegionsUncovered(regions, _txtMessage, _surfaces) ||
            !_game->agentOverlayClear(candidate)) return false;
        const size_t old=out.regions.size();
        out.regions.insert(out.regions.end(),regions.begin(),regions.end());
        if (Observer::agentRegionsDisjoint(out)) return true;
        out.regions.resize(old); return false;
    };
    std::string message="null", heading="null", shownHeading; SDL_Rect ink={};
    const bool hasHeading=Observer::agentWidgetVisible(_txtMessage) && _txtMessage->agentErrorHeading(shownHeading,ink);
    if (Observer::agentWidgetUncovered(_txtMessage,_surfaces))
    {
        const std::string full=Observer::agentTextJson(_txtMessage);
        Observer::AgentUiState region; Observer::agentIncludeWidget(region,_txtMessage);
        if (full!="null" && include(region.regions)) message=full;
    }
    if (hasHeading)
    {
        const std::vector<Observer::AgentUiRect> region={{ink.x,ink.y,ink.w,ink.h}};
        // The full-message region already contains the heading. Do not add a
        // duplicate overlapping region; still independently prove the prefix.
        if (message!="null") heading=Observer::agentJsonString(shownHeading);
        else if (include(region)) heading=Observer::agentJsonString(shownHeading);
    }
    out.visible="{\"message\":"+message+",\"heading\":"+heading+"}";
    if (Observer::agentWidgetUncovered(_btnOk,_surfaces))
    {
        const std::string label=Observer::agentButtonLabel(_btnOk);
        Observer::AgentUiState button; Observer::agentIncludeWidget(button,_btnOk);
        if (!label.empty() && label.size()<=1024 && _game->agentOverlayClear(button))
        {
            const size_t old=out.regions.size();
            out.regions.insert(out.regions.end(),button.regions.begin(),button.regions.end());
            if (Observer::agentRegionsDisjoint(out))
                out.actions.push_back(Observer::AgentUiAction("dismiss_error",label,"visible_text"));
            else out.regions.resize(old);
        }
    }
    const auto payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return Observer::agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool ErrorMessageState::agentDispatch(const std::string &action)
{
	Observer::AgentUiState current;
	if (!agentUiState(current) || !current.offers(action)) return false;
	if (action != "dismiss_error") return false;
	btnOkClick(nullptr);
	return true;
}
#endif

/**
 * Creates the elements in an error window.
 * @param str Text string for the message to display.
 * @param palette Parent state palette.
 * @param color Color of the UI controls.
 * @param bg Background image.
 * @param bgColor Background color (-1 for Battlescape).
 */
void ErrorMessageState::create(const std::string &str, SDL_Color *palette, Uint8 color, const std::string &bg, int bgColor)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 256, 160, 32, 20, POPUP_BOTH);
	_btnOk = new TextButton(120, 18, 100, 154);
	_txtMessage = new Text(246, 80, 37, 50);

	// Set palette
	setPalette(palette);
	if (bgColor != -1)
		setPalette(_game->getMod()->getPalette("BACKPALS.DAT")->getColors(Palette::blockOffset(bgColor)), Palette::backPos, 16);

	add(_window);
	add(_btnOk);
	add(_txtMessage);

	centerAllSurfaces();

	// Set up objects
	_window->setColor(color);
	_window->setBackground(_game->getMod()->getSurface(bg));

	_btnOk->setColor(color);
	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ErrorMessageState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ErrorMessageState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&ErrorMessageState::btnOkClick, Options::keyCancel);

	_txtMessage->setColor(color);
	_txtMessage->setAlign(ALIGN_CENTER);
	_txtMessage->setVerticalAlign(ALIGN_MIDDLE);
	_txtMessage->setBig();
	_txtMessage->setWordWrap(true);
	_txtMessage->setText(str);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _txtMessage->agentEnableErrorHeading();
#endif

	if (bgColor == -1)
	{
		_window->setHighContrast(true);
		_btnOk->setHighContrast(true);
		_txtMessage->setHighContrast(true);
	}
}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void ErrorMessageState::btnOkClick(Action *)
{
	_game->popState();
}

}
