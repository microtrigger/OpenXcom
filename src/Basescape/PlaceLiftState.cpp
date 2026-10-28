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
#include "PlaceLiftState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Text.h"
#include "BaseView.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#include "BasescapeState.h"
#include "SelectStartFacilityState.h"
#include "../Savegame/SavedGame.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Engine/Screen.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#include <typeinfo>
#include <algorithm>
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Place Lift screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param globe Pointer to the Geoscape globe.
 * @param first Is this a custom starting base?
 */
PlaceLiftState::PlaceLiftState(Base *base, Globe *globe, bool first) : _base(base), _globe(globe), _first(first)
{
	// Create objects
	_view = new BaseView(192, 192, 0, 8);
	_txtTitle = new Text(320, 9, 0, 0);

	// Set palette
	setInterface("placeFacility");

	add(_view, "baseView", "basescape");
	add(_txtTitle, "text", "placeFacility");

	centerAllSurfaces();

	// Set up objects
	_view->setTexture(_game->getMod()->getSurfaceSet("BASEBITS.PCK"));
	_view->setBase(_base);
	for (std::vector<std::string>::const_iterator i = _game->getMod()->getBaseFacilitiesList().begin(); i != _game->getMod()->getBaseFacilitiesList().end(); ++i)
	{
		if (_game->getMod()->getBaseFacility(*i)->isLift())
		{
			_lift = _game->getMod()->getBaseFacility(*i);
			break;
		}
	}
	_view->setSelectable(_lift->getSize());
	_view->onMouseClick((ActionHandler)&PlaceLiftState::viewClick);

	_txtTitle->setText(tr("STR_SELECT_POSITION_FOR_ACCESS_LIFT"));
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
    _view->agentEnablePlacement();
    if (_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if (_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
#endif
}

/**
 *
 */
PlaceLiftState::~PlaceLiftState()
{

}

/**
 * Processes clicking on facilities.
 * @param action Pointer to an action.
 */
void PlaceLiftState::viewClick(Action *)
{
	BaseFacility *fac = new BaseFacility(_lift, _base);
	fac->setX(_view->getGridX());
	fac->setY(_view->getGridY());
	_base->getFacilities()->push_back(fac);
	_game->popState();
	BasescapeState *bState = new BasescapeState(_base, _globe);
	_game->getSavedGame()->setSelectedBase(_game->getSavedGame()->getBases()->size() - 1);
	_game->pushState(bState);
	if (_first)
	{
		_game->pushState(new SelectStartFacilityState(_base, bState, _globe));
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool PlaceLiftState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> PlaceLiftState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    revisions.push_back(_txtTitle->agentTextRevision());
    if (const Surface *selector = _view->agentPlacementSelector()) revisions.push_back(selector->agentPresentationRevision());
    return revisions;
}
void PlaceLiftState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool PlaceLiftState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(PlaceLiftState) || _first || !_lift || _lift->getSize() != 1 || _modal ||
        !_base || !_game->isState(const_cast<PlaceLiftState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState(); out.actor = _base; out.screen = "place_lift";
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const Surface *logical = _game->getScreen()->getSurface();
    for (const Surface *surface : painted) out.widgets.emplace_back(surface, surface->agentPresentationRevision());
    if (_game->getCursor()) out.widgets.emplace_back(_game->getCursor(), _game->getCursor()->agentPresentationRevision());
    if (_game->getFpsCounter()) out.widgets.emplace_back(_game->getFpsCounter(), _game->getFpsCounter()->agentPresentationRevision());
    if (const Surface *selector = _view->agentPlacementSelector()) out.widgets.emplace_back(selector, selector->agentPresentationRevision());
    out.widgets.emplace_back(_txtTitle, _txtTitle->agentTextRevision());
    const auto allowed = [this](const AgentUiState &part) { return _game->agentOverlayClear(part); };
    std::string title = "null";
    const auto value = agentTextJson(_txtTitle);
    if (value != "null" && _txtTitle->agentPresentationPaintedTo(logical) && agentWidgetUncovered(_txtTitle, painted)) {
        AgentUiState part; agentIncludeWidget(part, _txtTitle);
        if (!part.regions.empty() && allowed(part)) {
            out.regions = part.regions;
            if (agentRegionsDisjoint(out)) title = value;
            else out.regions.clear();
        }
    }
    std::string grid = "null";
    if (_view->agentPlacementPresented(logical)) {
        std::string cells = "[";
        for (int y = 0; y < 6; ++y) for (int x = 0; x < 6; ++x) {
            // Native header overlaps the grid allocation's top pixel, but not
            // its interactive cell centers. Authorize the actual click target.
            AgentUiState cell; cell.regions.push_back({_view->getX()+32*x+16, _view->getY()+32*y+16, 1, 1});
            if (!agentRegionsUncovered(cell.regions, _view, painted) || !allowed(cell)) continue;
            const auto old = out.regions.size(); out.regions.insert(out.regions.end(), cell.regions.begin(), cell.regions.end());
            if (!agentRegionsDisjoint(out)) { out.regions.resize(old); continue; }
            if (cells.size() > 1) cells += ",";
            cells += "{\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y) + "}";
            out.context.push_back(y*6+x);
        }
        cells += "]";
        if (!out.context.empty()) {
            grid = "{\"columns\":6,\"rows\":6,\"origin_x\":" + std::to_string(_view->getX()) + ",\"origin_y\":" + std::to_string(_view->getY()) + ",\"cell_size\":32,\"cells\":" + cells + "}";
            out.actions.emplace_back("place_access_lift_cell", "Place access lift in visible cell", "visible_control_semantics", "facility_cell");
        }
    }
    out.visible = "{\"title\":" + title + ",\"grid\":" + grid + ",\"buttons\":[]}";
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size() <= 32768;
}
bool PlaceLiftState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;
    if (!agentUiState(fresh) || !fresh.offers(request.action)) return false;
    if (request.action == "place_access_lift_cell") {
        if (request.x < 0 || request.y < 0 || request.x > 5 || request.y > 5 ||
            std::find(fresh.context.begin(), fresh.context.end(), request.y*6+request.x) == fresh.context.end()) return false;
        return _view->agentPlacementClick(this, request.x, request.y);
    }
    return false;
}
#endif

}
