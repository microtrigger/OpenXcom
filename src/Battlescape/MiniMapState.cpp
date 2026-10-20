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
#include "MiniMapState.h"
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Interface/BattlescapeButton.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Palette.h"
#include "../Interface/Text.h"
#include "MiniMapView.h"
#include "Camera.h"
#include "../Engine/Timer.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{
/**
 * Initializes all the elements in the MiniMapState screen.
 * @param game Pointer to the core game.
 * @param camera The Battlescape camera.
 * @param battleGame The Battlescape save.
 */
MiniMapState::MiniMapState (Camera * camera, SavedBattleGame * battleGame)
{
	if (Options::maximizeInfoScreens)
	{
		Options::baseXResolution = Screen::ORIGINAL_WIDTH;
		Options::baseYResolution = Screen::ORIGINAL_HEIGHT;
		_game->getScreen()->resetDisplay(false);
	}

	_bg = new Surface(320, 200);
	_miniMapView = new MiniMapView(221, 148, 48, 16, _game, camera, battleGame);
	_btnLvlUp = new BattlescapeButton(18, 20, 24, 62);
	_btnLvlDwn = new BattlescapeButton(18, 20, 24, 88);
	_btnOk = new BattlescapeButton(32, 32, 275, 145);
	_txtLevel = new Text(28, 16, 281, 75);

	// Set palette
	battleGame->setPaletteByDepth(this);

	add(_bg);
	_game->getMod()->getSurface("SCANBORD.PCK")->blit(_bg);
	add(_miniMapView);
	add(_btnLvlUp, "buttonUp", "minimap", _bg);
	add(_btnLvlDwn, "buttonDown", "minimap", _bg);
	add(_btnOk, "buttonOK", "minimap", _bg);
	add(_txtLevel, "textLevel", "minimap", _bg);

	centerAllSurfaces();

	if (_game->getScreen()->getDY() > 50)
	{
		_screen = false;
		_bg->drawRect(46, 14, 223, 151, Palette::blockOffset(15)+15);
	}

	_btnLvlUp->onMouseClick((ActionHandler)&MiniMapState::btnLevelUpClick);
	_btnLvlDwn->onMouseClick((ActionHandler)&MiniMapState::btnLevelDownClick);
	_btnOk->onMouseClick((ActionHandler)&MiniMapState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&MiniMapState::btnOkClick, Options::keyCancel);
	_btnOk->onKeyboardPress((ActionHandler)&MiniMapState::btnOkClick, Options::keyBattleMap);
	_txtLevel->setBig();
	_txtLevel->setHighContrast(true);
	_txtLevel->setText(tr("STR_LEVEL_SHORT").arg(camera->getViewLevel()));
	_timerAnimate = new Timer(125);
	_timerAnimate->onTimer((StateHandler)&MiniMapState::animate);
	_timerAnimate->start();
	_miniMapView->draw();
}

/**
 *
 */
MiniMapState::~MiniMapState()
{
	delete _timerAnimate;
}

/**
 * Handles mouse-wheeling.
 * @param action Pointer to an action.
 */
void MiniMapState::handle(Action *action)
{
	State::handle(action);
	if (action->getDetails()->type == SDL_MOUSEBUTTONDOWN)
	{
		if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
		{
			btnLevelUpClick(action);
		}
		else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
		{
			btnLevelDownClick(action);
		}
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void MiniMapState::btnOkClick(Action *)
{
	if (Options::maximizeInfoScreens)
	{
		Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, true);
		_game->getScreen()->resetDisplay(false);
	}
	_game->popState();
}

/**
 * Changes the currently displayed minimap level.
 * @param action Pointer to an action.
 */
void MiniMapState::btnLevelUpClick(Action *)
{
	_txtLevel->setText(tr("STR_LEVEL_SHORT").arg(_miniMapView->up()));
}

/**
 * Changes the currently displayed minimap level.
 * @param action Pointer to an action.
 */
void MiniMapState::btnLevelDownClick(Action *)
{
	_txtLevel->setText(tr("STR_LEVEL_SHORT").arg(_miniMapView->down()));
}

/**
 * Animation handler. Updates the minimap view animation.
 */
void MiniMapState::animate()
{
	_miniMapView->animate();
}

/**
 * Handles timers.
 */
void MiniMapState::think()
{
	State::think();
	_timerAnimate->think(this, 0);
}

}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProjectileOutput.h"
#include <set>
#include <algorithm>
namespace OpenXcom {
bool MiniMapState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (_modal || !_game->isState(const_cast<MiniMapState *>(this))) return false;
    out = AgentUiState(); out.screen = "minimap";
    if (!_miniMapView) return false;
    out.widgets.push_back({_miniMapView, _miniMapView->agentPresentation().revision()});
    out.context.push_back(_miniMapView->agentCameraRevision());
    const auto include = [&](const Surface *widget) -> bool
    {
        if (!agentWidgetUncovered(widget, _surfaces)) return false;
        AgentUiState part; agentIncludeWidget(part, widget);
        if (part.regions.empty() || !_game->agentOverlayClear(part)) return false;
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        return true;
    };
    if (include(_btnLvlUp)) out.actions.emplace_back("view_level_up", "View level up", "visible_control_semantics");
    if (include(_btnLvlDwn)) out.actions.emplace_back("view_level_down", "View level down", "visible_control_semantics");
    if (include(_btnOk)) out.actions.emplace_back("close_minimap", "Close minimap", "visible_control_semantics");
    std::string label = agentTextJson(_txtLevel);
    if (label != "null" && !include(_txtLevel)) label = "null";
    std::string viewport = "null", terrain = "[]", markers = "[]", targets = "[]";
    bool bounded = false, truncated = false;
    if (agentWidgetVisible(_miniMapView) && _miniMapView->agentReady() &&
        _miniMapView->getX() + _miniMapView->getWidth() <= 640 &&
        _miniMapView->getY() + _miniMapView->getHeight() <= 400)
    {
        const int ox = _miniMapView->getX(), oy = _miniMapView->getY();
        const int width = _miniMapView->getWidth(), height = _miniMapView->getHeight();
        AgentProjectileOutput output;
        const bool finalReady = _game->getScreen()->agentCaptureOutput(_miniMapView, output);
        const auto exposed = [&](int x, int y) -> bool
        {
            AgentUiState part; part.regions.push_back({ox+x, oy+y, 4, 4});
            return agentRegionsUncovered(part.regions, _miniMapView, _surfaces) && _game->agentOverlayClear(part);
        };
        const auto proof = [&](int x, int y, std::uint32_t rgb) -> bool
        { return finalReady && output.matches(ox+x, oy+y, rgb); };
        std::vector<AgentMiniMapGlyph> glyphs;
        if (finalReady) glyphs = _miniMapView->agentPresentation().project(output.palette, proof, exposed);
        std::set<std::pair<int,int> > terrainCells, regionCells;
        std::vector<AgentMiniMapGlyph> markerGlyphs;
        for (const auto &glyph : glyphs)
            if (glyph.kind == AgentMiniMapKind::Terrain) terrainCells.insert({glyph.y, glyph.x});
            else markerGlyphs.push_back(glyph);
        std::sort(markerGlyphs.begin(), markerGlyphs.end(), [](const AgentMiniMapGlyph &a, const AgentMiniMapGlyph &b)
        { if (a.y != b.y) return a.y < b.y; if (a.x != b.x) return a.x < b.x;
          return a.kind == AgentMiniMapKind::Item && b.kind == AgentMiniMapKind::Unit; });
        terrain = "[";
        unsigned rectangles = 0;
        for (auto it = terrainCells.begin(); it != terrainCells.end(); )
        {
            const int y = it->first, x = it->second; int end = x + 4; ++it;
            while (it != terrainCells.end() && it->first == y && it->second == end) { end += 4; ++it; }
            if (rectangles == 512) { truncated = true; continue; }
            if (rectangles++) terrain += ",";
            terrain += "[" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(end-x) + ",4]";
            for (int px = x; px < end; px += 4) regionCells.insert({y, px});
        }
        terrain += "]";
        markers = "["; unsigned count = 0;
        std::vector<std::pair<int,int> > points;
        const auto addPoint = [&](int x, int y)
        {
            const std::pair<int,int> point(x,y);
            if (std::find(points.begin(), points.end(), point) == points.end()) points.push_back(point);
        };
        for (const auto &glyph : markerGlyphs)
        {
            if (count == 128) { truncated = true; continue; }
            if (count++) markers += ",";
            markers += std::string("{\"kind\":\"") + (glyph.kind == AgentMiniMapKind::Unit ? "unit_marker" : "item_marker") +
                "\",\"x\":" + std::to_string(glyph.x) + ",\"y\":" + std::to_string(glyph.y) + ",\"width\":4,\"height\":4}";
            regionCells.insert({glyph.y, glyph.x}); addPoint(glyph.x+2, glyph.y+2);
        }
        markers += "]"; bounded = rectangles || count;
        if (bounded)
        {
            // Navigation anchors are geometry, never occupancy or passability.
            // Each complete anchor cell must survive later layers/final output.
            for (int yi = 1; yi <= 3; ++yi) for (int xi = 1; xi <= 3; ++xi)
            {
                const int x = ((width * xi / 4) / 4) * 4, y = ((height * yi / 4) / 4) * 4;
                if (x + 4 > width || y + 4 > height || !exposed(x,y)) continue;
                bool shown = true;
                for (int py=y; py<y+4; ++py) for (int px=x; px<x+4; ++px)
                {
                    const Uint8 color = _miniMapView->getPixel(px,py);
                    shown = shown && color && proof(px,py,output.palette[color]);
                }
                if (shown) { addPoint(x+2,y+2); regionCells.insert({y,x}); }
            }
            targets = "[";
            for (const auto &point : points)
            {
                if (out.points.size() == 160) { truncated = true; break; }
                if (!out.points.empty()) targets += ",";
                targets += "{\"handle\":\"m" + std::to_string(out.points.size()) + "\",\"x\":" +
                    std::to_string(point.first) + ",\"y\":" + std::to_string(point.second) + "}";
                out.points.push_back(point);
            }
            targets += "]";
            // Union cells, coalesced by row: no duplicate/overlapping global
            // guard regions when transparent marker and terrain ink coexist.
            for (auto it = regionCells.begin(); it != regionCells.end(); )
            {
                const int y=it->first, x=it->second; int end=x+4; ++it;
                while (it != regionCells.end() && it->first==y && it->second==end) { end+=4; ++it; }
                out.regions.push_back({ox+x,oy+y,end-x,4});
            }
            viewport = "{\"x\":" + std::to_string(ox) + ",\"y\":" + std::to_string(oy) +
                ",\"width\":" + std::to_string(width) + ",\"height\":" + std::to_string(height) + "}";
            if (!out.points.empty()) out.actions.emplace_back("minimap_center", "Center minimap", "visible_control_semantics", "minimap_target");
        }
        else { terrain = markers = targets = "[]"; truncated = false; }
    }
    out.visible = "{\"level_label\":" + label + ",\"viewport\":" + viewport + ",\"terrain\":" + terrain +
        ",\"markers\":" + markers + ",\"targets\":" + targets + ",\"content_status\":\"" +
        (bounded ? "bounded" : "unavailable") + "\",\"truncated\":" + (truncated ? "true" : "false") + "}";
    return !out.regions.empty() && agentRegionsDisjoint(out);
}
bool MiniMapState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;
    if (!agentUiState(fresh) || !fresh.offers(request.action)) return false;
    if (request.action == "view_level_up") { btnLevelUpClick(nullptr); return true; }
    if (request.action == "view_level_down") { btnLevelDownClick(nullptr); return true; }
    if (request.action == "close_minimap") { btnOkClick(nullptr); return true; }
    if (request.action == "minimap_center")
    {
        if (request.choice.size() < 2 || request.choice[0] != 'm') return false;
        unsigned index = 0;
        for (std::size_t i=1; i<request.choice.size(); ++i)
        { if (request.choice[i]<'0' || request.choice[i]>'9') return false; index=index*10+request.choice[i]-'0'; if(index>=160) return false; }
        if (request.choice != "m"+std::to_string(index) || index >= fresh.points.size()) return false;
        _miniMapView->agentActivatePoint(fresh.points[index].first, fresh.points[index].second); return true;
    }
    return false;
}
}
#endif
