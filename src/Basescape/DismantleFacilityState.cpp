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
#include "DismantleFacilityState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "BaseView.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/SavedGame.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Engine/Screen.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#include <algorithm>
#include <typeinfo>
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in a Dismantle Facility window.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param view Pointer to the baseview to update.
 * @param fac Pointer to the facility to dismantle.
 */
DismantleFacilityState::DismantleFacilityState(Base *base, BaseView *view, BaseFacility *fac) : _base(base), _view(view), _fac(fac)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 152, 80, 20, 60);
	_btnOk = new TextButton(44, 16, 36, 115);
	_btnCancel = new TextButton(44, 16, 112, 115);
	_txtTitle = new Text(142, 9, 25, 75);
	_txtFacility = new Text(142, 9, 25, 85);

	// Set palette
	setInterface("dismantleFacility");

	add(_window, "window", "dismantleFacility");
	add(_btnOk, "button", "dismantleFacility");
	add(_btnCancel, "button", "dismantleFacility");
	add(_txtTitle, "text", "dismantleFacility");
	add(_txtFacility, "text", "dismantleFacility");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&DismantleFacilityState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&DismantleFacilityState::btnOkClick, Options::keyOk);

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&DismantleFacilityState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&DismantleFacilityState::btnCancelClick, Options::keyCancel);

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_DISMANTLE"));

	_txtFacility->setAlign(ALIGN_CENTER);
	_txtFacility->setText(tr(_fac->getRules()->getType()));
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
    _btnOk->agentEnableNamedControl();
    _btnCancel->agentEnableNamedControl();
    if (_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if (_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
#endif
}

/**
 *
 */
DismantleFacilityState::~DismantleFacilityState()
{

}

/**
 * Dismantles the facility and returns
 * to the previous screen.
 * @param action Pointer to an action.
 */
void DismantleFacilityState::btnOkClick(Action *)
{
	if (!_fac->getRules()->isLift())
	{
		// Give refund if this is an unstarted, queued build.
		if (_fac->getBuildTime() > _fac->getRules()->getBuildTime())
		{
			_game->getSavedGame()->setFunds(_game->getSavedGame()->getFunds() + _fac->getRules()->getBuildCost());
		}

		for (std::vector<BaseFacility*>::iterator i = _base->getFacilities()->begin(); i != _base->getFacilities()->end(); ++i)
		{
			if (*i == _fac)
			{
				_base->getFacilities()->erase(i);
				_view->resetSelectedFacility();
				delete _fac;
				if (Options::allowBuildingQueue) _view->reCalcQueuedBuildings();
				break;
			}
		}
	}
	// Remove whole base if it's the access lift
	else
	{
		for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
		{
			if (*i == _base)
			{
				_game->getSavedGame()->getBases()->erase(i);
				delete _base;
				break;
			}
		}
	}
	_game->popState();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void DismantleFacilityState::btnCancelClick(Action *)
{
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool DismantleFacilityState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> DismantleFacilityState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    revisions.push_back(_txtTitle->agentTextRevision());
    revisions.push_back(_txtFacility->agentTextRevision());
    revisions.push_back(_btnOk->agentLabelRevision());
    revisions.push_back(_btnCancel->agentLabelRevision());
    return revisions;
}
void DismantleFacilityState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool DismantleFacilityState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(DismantleFacilityState) || _modal || _window->agentPopupActive() ||
        !_base || !_fac || !_game->isState(const_cast<DismantleFacilityState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    if (std::find(_base->getFacilities()->begin(), _base->getFacilities()->end(), _fac) == _base->getFacilities()->end()) return false;
    out = AgentUiState(); out.actor = _fac; out.screen = "dismantle_facility";
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const Surface *logical = _game->getScreen()->getSurface();
    for (const Surface *surface : painted) out.widgets.emplace_back(surface, surface->agentPresentationRevision());
    if (_game->getCursor()) out.widgets.emplace_back(_game->getCursor(), _game->getCursor()->agentPresentationRevision());
    if (_game->getFpsCounter()) out.widgets.emplace_back(_game->getFpsCounter(), _game->getFpsCounter()->agentPresentationRevision());
    const auto allowed = [this](const AgentUiState &part) { return _game->agentOverlayClear(part); };
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        const auto value = agentTextJson(widget);
        if (value == "null" || !widget->agentPresentationPaintedTo(logical) || !agentWidgetUncovered(widget, painted)) return "null";
        AgentUiState part; agentIncludeWidget(part, widget);
        if (part.regions.empty() || !allowed(part)) return "null";
        const auto old = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(out)) return value;
        out.regions.resize(old); return "null";
    };
    const auto title = text(_txtTitle), facility = text(_txtFacility);
    // This bounded capability never authorizes abandoning an entire base.
    // The own-facility rule is an explicit safety gate, not inferred from a
    // translated caption or used to prefilter ordinary grid-click legality.
    const bool confirmationAllowed = !_fac->getRules()->isLift();
    out.context.push_back(confirmationAllowed ? 1 : 0);
    std::string buttons = "[";
    const auto button = [&](TextButton *widget, const char *id, const char *action) {
        const auto label = agentProjectNamedButton(widget, logical, painted, out, allowed);
        if (label.empty() || label.size() > 1024) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back(action, label, "visible_text");
    };
    if (confirmationAllowed && title != "null" && title != "\"\"" && facility != "null" && facility != "\"\"")
        button(_btnOk, "ok", "confirm_dismantle_facility");
    button(_btnCancel, "cancel", "cancel_dismantle_facility");
    buttons += "]";
    out.visible = "{\"title\":" + title + ",\"facility\":" + facility +
        ",\"confirmation_allowed\":" + (confirmationAllowed ? "true" : "false") + ",\"buttons\":" + buttons + "}";
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size() <= 32768;
}
bool DismantleFacilityState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;
    if (!agentUiState(fresh) || !fresh.offers(request.action)) return false;
    if (request.action == "confirm_dismantle_facility") {
        if (_fac->getRules()->isLift()) return false;
        btnOkClick(nullptr); return true;
    }
    if (request.action == "cancel_dismantle_facility") { btnCancelClick(nullptr); return true; }
    return false;
}
#endif

}
