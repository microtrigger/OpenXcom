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
#include "ScannerState.h"
#include "ScannerView.h"
#include "../Engine/InteractiveSurface.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/Timer.h"
#include "../Engine/Screen.h"
#include "../Engine/Options.h"
#include "../Savegame/BattleUnit.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

/**
 * Initializes the Scanner State.
 * @param game Pointer to the core game.
 * @param action Pointer to an action.
 */
ScannerState::ScannerState (BattleAction *action) : _action(action)
{
	if (Options::maximizeInfoScreens)
	{
		Options::baseXResolution = Screen::ORIGINAL_WIDTH;
		Options::baseYResolution = Screen::ORIGINAL_HEIGHT;
		_game->getScreen()->resetDisplay(false);
	}
	_bg = new InteractiveSurface(320, 200);
	_scan = new Surface(320, 200);
	_scannerView = new ScannerView(152, 152, 56, 24, _game, _action->actor);

	if (_game->getScreen()->getDY() > 50)
	{
		_screen = false;
	}

	// Set palette
	_game->getSavedGame()->getSavedBattle()->setPaletteByDepth(this);

	add(_scan);
	add(_scannerView);
	add(_bg);

	centerAllSurfaces();

	_game->getMod()->getSurface("DETBORD.PCK")->blit(_bg);
	_game->getMod()->getSurface("DETBORD2.PCK")->blit(_scan);
	_bg->onMouseClick((ActionHandler)&ScannerState::exitClick);
	_bg->onKeyboardPress((ActionHandler)&ScannerState::exitClick, Options::keyCancel);

	_timerAnimate = new Timer(125);
	_timerAnimate->onTimer((StateHandler)&ScannerState::animate);
	_timerAnimate->start();

	update();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
#endif
}

ScannerState::~ScannerState()
{
	delete _timerAnimate;
}

/**
 * Closes the window on right-click.
 * @param action Pointer to an action.
 */
void ScannerState::handle(Action *action)
{
	State::handle(action);
	if (action->getDetails()->type == SDL_MOUSEBUTTONDOWN && action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		exitClick(action);
	}
}

/**
 * Updates scanner state.
 */
void ScannerState::update()
{
	//_scannerView->draw();
}

/**
 * Animation handler. Updates the minimap view animation.
 */
void ScannerState::animate()
{
	_scannerView->animate();
}

/**
 * Handles timers.
 */
void ScannerState::think()
{
	State::think();
	_timerAnimate->think(this, 0);
}

/**
 * Exits the screen.
 * @param action Pointer to an action.
 */
void ScannerState::exitClick(Action *)
{
	if (Options::maximizeInfoScreens)
	{
		Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, true);
		_game->getScreen()->resetDisplay(false);
	}
	_game->popState();
}

}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProjectileOutput.h"
#include <algorithm>
namespace OpenXcom {
std::vector<std::uint64_t> ScannerState::agentRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : {static_cast<const Surface *>(_scan), static_cast<const Surface *>(_scannerView), static_cast<const Surface *>(_bg)})
        revisions.push_back(surface->agentPresentationRevision());
    return revisions;
}
void ScannerState::blit()
{
    _agentPaintFull = false;
    const SDL_Surface *before = _game->getScreen()->getSurface()->getSurface();
    const bool full = before && !before->clip_rect.x && !before->clip_rect.y &&
        before->clip_rect.w == before->w && before->clip_rect.h == before->h;
    State::blit();
    _agentPaintRevisions = agentRevisions();
    const SDL_Surface *after = _game->getScreen()->getSurface()->getSurface();
    _agentPaintFull = full && after && !after->clip_rect.x && !after->clip_rect.y &&
        after->clip_rect.w == after->w && after->clip_rect.h == after->h && Observer::agentPaintHealthy();
}
bool ScannerState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<ScannerState *>(this)) || _modal || !_agentPaintFull ||
        !agentPaintHealthy() || _agentPaintRevisions != agentRevisions()) return false;
    const Surface *destination = _game->getScreen()->getSurface();
    if (!_bg->agentPresentationPaintedTo(destination) || !agentWidgetUncovered(_bg, _surfaces)) return false;
    out = AgentUiState(); out.screen = "scanner";
    // Native right-click/Cancel closes this whole visible panel. No unit or
    // scanner model identities are needed in its public or private binding.
    agentIncludeWidget(out, _bg);
    if (out.regions.empty() || !_game->agentNonCursorOverlayClear(out)) return false;
    out.actions.emplace_back("close_scanner", "Close scanner", "visible_control_semantics");
    for (const Surface *surface : {static_cast<const Surface *>(_scan), static_cast<const Surface *>(_scannerView), static_cast<const Surface *>(_bg)})
        out.widgets.emplace_back(surface, surface->agentPresentationRevision());
    // Closing is independent of the 125ms animated scan. Preserve every
    // widget/layout/visibility/palette generation and native window guard,
    // but do not turn changing read-only pixels into close-action authority.
    out.controlVisible = "{\"close_panel\":["+std::to_string(_bg->getX())+","+
        std::to_string(_bg->getY())+","+std::to_string(_bg->getWidth())+","+
        std::to_string(_bg->getHeight())+"],\"viewport_geometry\":["+
        std::to_string(_scannerView->getX())+","+std::to_string(_scannerView->getY())+","+
        std::to_string(_scannerView->getWidth())+","+std::to_string(_scannerView->getHeight())+"]}";
    std::string viewport = "null", palette = "[]", pixels = "";
    bool ready = false;
    const int ox = _scannerView->getX(), oy = _scannerView->getY();
    const int width = _scannerView->getWidth(), height = _scannerView->getHeight();
    if (agentWidgetVisible(_scannerView) && _scannerView->agentPresentationPaintedTo(destination) &&
        ox >= 0 && oy >= 0 && width > 0 && height > 0 && width <= 152 && height <= 152 &&
        ox + width <= 640 && oy + height <= 400)
    {
        viewport = "{\"x\":"+std::to_string(ox)+",\"y\":"+std::to_string(oy)+
            ",\"width\":"+std::to_string(width)+",\"height\":"+std::to_string(height)+"}";
        // The known stock frame is a transparent overlay, not blanket authority
        // to ignore later widgets. Every candidate still needs its transparent
        // frame pixel AND matching logical/final output RGB.
        std::vector<Surface *> order;
        for (Surface *surface : _surfaces) if (surface != _bg) order.push_back(surface);
        AgentProjectileOutput output;
        ready = agentWidgetUncovered(_scannerView, order) && _game->getScreen()->agentCaptureOutput(_scannerView, output);
        std::vector<std::uint32_t> colors;
        std::string raster;
        raster.reserve(width*height);
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        const auto pixel = [&](int x, int y, std::uint32_t &rgb) -> bool {
            const unsigned index = _scannerView->getPixel(x, y);
            const int bx = ox+x-_bg->getX(), by = oy+y-_bg->getY();
            if (!index || bx < 0 || by < 0 || bx >= _bg->getWidth() || by >= _bg->getHeight() || _bg->getPixel(bx,by)) return false;
            rgb = output.palette[index];
            return output.matches(ox+x, oy+y, rgb);
        };
        for (int y=0; ready && y<height; ++y)
            for (int x=0; ready && x<width; ++x)
            {
                std::uint32_t rgb;
                if (!pixel(x,y,rgb)) { raster += 'A'; continue; }
                auto found=std::find(colors.begin(),colors.end(),rgb);
                const unsigned color=static_cast<unsigned>(found-colors.begin());
                if (found==colors.end()) {
                    if (colors.size()==63) { ready=false; break; }
                    colors.push_back(rgb);
                }
                raster += alphabet[color+1];
            }
        if (ready)
        {
            pixels=raster; palette="[";
            for (unsigned i=0;i<colors.size();++i) {
                if(i)palette+=",";const std::uint32_t rgb=colors[i];
                palette+="["+std::to_string((rgb>>16)&255)+","+std::to_string((rgb>>8)&255)+","+std::to_string(rgb&255)+"]";
            }
            palette+="]";
        }
    }
    out.visible="{\"viewport\":"+viewport+",\"palette\":"+palette+",\"pixels\":"+agentJsonString(pixels)+
        ",\"content_status\":\""+(ready?"bounded":"unavailable")+"\"}";
    return out.visible.size() <= 24576 && agentRegionsDisjoint(out);
}
bool ScannerState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;
    if (!agentUiState(fresh) || !fresh.offers(request.action)) return false;
    if (request.action == "close_scanner") { exitClick(nullptr); return true; }
    return false;
}
}
#endif
