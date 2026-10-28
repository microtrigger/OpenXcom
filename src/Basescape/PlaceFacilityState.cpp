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
#include "PlaceFacilityState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "BaseView.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/SavedGame.h"
#include "../Menu/ErrorMessageState.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Mod/RuleInterface.h"
#include <climits>

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include <typeinfo>
#include <algorithm>
#include "../Engine/Screen.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Place Facility window.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param rule Pointer to the facility ruleset to build.
 */
PlaceFacilityState::PlaceFacilityState(Base *base, RuleBaseFacility *rule) : _base(base), _rule(rule)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 128, 160, 192, 40);
	_view = new BaseView(192, 192, 0, 8);
	_btnCancel = new TextButton(112, 16, 200, 176);
	_txtFacility = new Text(110, 9, 202, 50);
	_txtCost = new Text(110, 9, 202, 62);
	_numCost = new Text(110, 17, 202, 70);
	_txtTime = new Text(110, 9, 202, 90);
	_numTime = new Text(110, 17, 202, 98);
	_txtMaintenance = new Text(110, 9, 202, 118);
	_numMaintenance = new Text(110, 17, 202, 126);

	// Set palette
	setInterface("placeFacility");

	add(_window, "window", "placeFacility");
	add(_view, "baseView", "basescape");
	add(_btnCancel, "button", "placeFacility");
	add(_txtFacility, "text", "placeFacility");
	add(_txtCost, "text", "placeFacility");
	add(_numCost, "numbers", "placeFacility");
	add(_txtTime, "text", "placeFacility");
	add(_numTime, "numbers", "placeFacility");
	add(_txtMaintenance, "text", "placeFacility");
	add(_numMaintenance, "numbers", "placeFacility");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

	_view->setTexture(_game->getMod()->getSurfaceSet("BASEBITS.PCK"));
	_view->setBase(_base);
	_view->setSelectable(rule->getSize());
	_view->onMouseClick((ActionHandler)&PlaceFacilityState::viewClick);

	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&PlaceFacilityState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&PlaceFacilityState::btnCancelClick, Options::keyCancel);

	_txtFacility->setText(tr(_rule->getType()));

	_txtCost->setText(tr("STR_COST_UC"));

	_numCost->setBig();
	_numCost->setText(Unicode::formatFunding(_rule->getBuildCost()));

	_txtTime->setText(tr("STR_CONSTRUCTION_TIME_UC"));

	_numTime->setBig();
	_numTime->setText(tr("STR_DAY", _rule->getBuildTime()));

	_txtMaintenance->setText(tr("STR_MAINTENANCE_UC"));

	_numMaintenance->setBig();
	_numMaintenance->setText(Unicode::formatFunding(_rule->getMonthlyCost()));
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
    _btnCancel->agentEnableNamedControl();
    _view->agentEnablePlacement();
    if (_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if (_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
#endif
}

/**
 *
 */
PlaceFacilityState::~PlaceFacilityState()
{

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void PlaceFacilityState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Processes clicking on facilities.
 * @param action Pointer to an action.
 */
void PlaceFacilityState::viewClick(Action *)
{
	if (!_view->isPlaceable(_rule))
	{
		_game->pushState(new ErrorMessageState(tr("STR_CANNOT_BUILD_HERE"), _palette, _game->getMod()->getInterface("placeFacility")->getElement("errorMessage")->color, "BACK01.SCR", _game->getMod()->getInterface("placeFacility")->getElement("errorPalette")->color));
	}
	else if (_game->getSavedGame()->getFunds() < _rule->getBuildCost())
	{
		_game->popState();
		_game->pushState(new ErrorMessageState(tr("STR_NOT_ENOUGH_MONEY"), _palette, _game->getMod()->getInterface("placeFacility")->getElement("errorMessage")->color, "BACK01.SCR", _game->getMod()->getInterface("placeFacility")->getElement("errorPalette")->color));
	}
	else
	{
		BaseFacility *fac = new BaseFacility(_rule, _base);
		fac->setX(_view->getGridX());
		fac->setY(_view->getGridY());
		fac->setBuildTime(_rule->getBuildTime());
		_base->getFacilities()->push_back(fac);
		if (Options::allowBuildingQueue)
		{
			if (_view->isQueuedBuilding(_rule)) fac->setBuildTime(INT_MAX);
			_view->reCalcQueuedBuildings();
		}
		_game->getSavedGame()->setFunds(_game->getSavedGame()->getFunds() - _rule->getBuildCost());
		_game->popState();
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace {
// Private readback of the known widget's painted ink, never image discovery or
// exported pixels. Compare current indexed RGB against the logical destination.
bool PlaceFacilityStateInkCurrent(const Surface *widget, const Surface *logical,
    const Observer::AgentUiState &part)
{
    if (!widget || !logical || !widget->agentPresentationPaintedTo(logical) || part.regions.empty()) return false;
    SDL_Surface *source = widget->getSurface(), *target = logical->getSurface();
    if (!source || !target || source->format->BytesPerPixel != 1 || target->format->BytesPerPixel != 1 ||
        !source->format->palette || !target->format->palette) return false;
    if (Observer::agentObservePaint(SDL_LockSurface(source)) < 0) return false;
    if (Observer::agentObservePaint(SDL_LockSurface(target)) < 0) { SDL_UnlockSurface(source); return false; }
    bool same = true, ink = false; std::size_t work = 0;
    for (const auto &r : part.regions) {
        if (r.x < widget->getX() || r.y < widget->getY() || r.x+r.width > widget->getX()+source->w ||
            r.y+r.height > widget->getY()+source->h || r.x < 0 || r.y < 0 ||
            r.x+r.width > target->w || r.y+r.height > target->h) { same=false; break; }
        for (int y=r.y; same && y<r.y+r.height; ++y) for (int x=r.x; x<r.x+r.width; ++x) {
            if (++work > 131072) { same=false; break; }
            const Uint8 a=static_cast<const Uint8 *>(source->pixels)[(y-widget->getY())*source->pitch+x-widget->getX()];
            if ((source->flags & SDL_SRCCOLORKEY) && a==source->format->colorkey) continue;
            const Uint8 b=static_cast<const Uint8 *>(target->pixels)[y*target->pitch+x];
            if (a>=source->format->palette->ncolors || b>=target->format->palette->ncolors) { same=false; break; }
            const SDL_Color ca=source->format->palette->colors[a], cb=target->format->palette->colors[b];
            ink=true; if (ca.r!=cb.r || ca.g!=cb.g || ca.b!=cb.b) { same=false; break; }
        }
    }
    SDL_UnlockSurface(target); SDL_UnlockSurface(source);
    return same && ink && Observer::agentPaintHealthy();
}
}
bool PlaceFacilityState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> PlaceFacilityState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    if(const Surface *selector=_view->agentPlacementSelector())revisions.push_back(selector->agentPresentationRevision());
    revisions.push_back(_txtFacility->agentTextRevision());
    revisions.push_back(_txtCost->agentTextRevision());
    revisions.push_back(_numCost->agentTextRevision());
    revisions.push_back(_txtTime->agentTextRevision());
    revisions.push_back(_numTime->agentTextRevision());
    revisions.push_back(_txtMaintenance->agentTextRevision());
    revisions.push_back(_numMaintenance->agentTextRevision());
    revisions.push_back(_btnCancel->agentLabelRevision());
    return revisions;
}
void PlaceFacilityState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool PlaceFacilityState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(PlaceFacilityState) || _modal || _window->agentPopupActive() || !_base || !_game->isState(const_cast<PlaceFacilityState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState(); out.actor = _base; out.screen = "place_facility";
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const Surface *logical = _game->getScreen()->getSurface();
    for (const Surface *surface : painted) out.widgets.emplace_back(surface, surface->agentPresentationRevision());
    if (_game->getCursor()) out.widgets.emplace_back(_game->getCursor(), _game->getCursor()->agentPresentationRevision());
    if (_game->getFpsCounter()) out.widgets.emplace_back(_game->getFpsCounter(), _game->getFpsCounter()->agentPresentationRevision());
    if(const Surface *selector=_view->agentPlacementSelector())out.widgets.emplace_back(selector,selector->agentPresentationRevision());
    const auto allowed = [this](const AgentUiState &part) { return _game->agentOverlayClear(part); };
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        const auto value = agentTextJson(widget);
        if (value == "null" || !agentWidgetUncovered(widget, painted)) return "null";
        AgentUiState part; agentIncludeWidget(part, widget);
        if (part.regions.empty() || !allowed(part) || !PlaceFacilityStateInkCurrent(widget, logical, part)) return "null";
        const auto old = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(out)) return value;
        out.regions.resize(old); return "null";
    };
    const auto facility = text(_txtFacility);
    const auto cost_label = text(_txtCost);
    const auto cost = text(_numCost);
    const auto time_label = text(_txtTime);
    const auto time = text(_numTime);
    const auto maintenance_label = text(_txtMaintenance);
    const auto maintenance = text(_numMaintenance);
    std::string grid="null";
    if(_view->agentPlacementPresented(logical)) {
        std::string cells="[";
        for(int y=0;y<6;++y)for(int x=0;x<6;++x){
            AgentUiState cell;cell.regions.push_back({_view->getX()+32*x,_view->getY()+32*y,32,32});
            if(!agentRegionsUncovered(cell.regions,_view,painted) || !allowed(cell))continue;
            const auto old=out.regions.size();out.regions.insert(out.regions.end(),cell.regions.begin(),cell.regions.end());
            if(!agentRegionsDisjoint(out)){out.regions.resize(old);continue;}
            if(cells.size()>1)cells+=",";
            cells+="{\"x\":"+std::to_string(x)+",\"y\":"+std::to_string(y)+"}";
            out.context.push_back(y*6+x);
        }
        cells+="]";
        if(!out.context.empty()) {
            grid="{\"columns\":6,\"rows\":6,\"origin_x\":"+std::to_string(_view->getX())+",\"origin_y\":"+std::to_string(_view->getY())+",\"cell_size\":32,\"cells\":"+cells+"}";
            out.actions.emplace_back("place_facility_cell","Place facility in visible cell","visible_control_semantics","facility_cell");
        }
    }
    std::string buttons = "[";
    const auto label = agentProjectNamedButton(_btnCancel, logical, painted, out, allowed);
    if (!label.empty() && label.size() <= 1024) {
        buttons += "{\"id\":\"cancel\",\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back("cancel_place_facility", label, "visible_text");
    }
    buttons += "]";
    out.visible = "{\"facility\":" + facility + ",\"cost_label\":" + cost_label + ",\"cost\":" + cost + ",\"time_label\":" + time_label + ",\"time\":" + time + ",\"maintenance_label\":" + maintenance_label + ",\"maintenance\":" + maintenance + ",\"grid\":" + grid + ",\"buttons\":" + buttons + "}";
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size() <= 32768;
}
bool PlaceFacilityState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;
    if (!agentUiState(fresh) || !fresh.offers(request.action)) return false;
    if(request.action=="place_facility_cell") {
        if(request.x<0 || request.y<0 || request.x>5 || request.y>5 ||
            std::find(fresh.context.begin(),fresh.context.end(),request.y*6+request.x)==fresh.context.end())return false;
        return _view->agentPlacementClick(this,request.x,request.y);
    }
    if (request.action == "cancel_place_facility") { btnCancelClick(nullptr); return true; }
    return false;
}
#endif

}
