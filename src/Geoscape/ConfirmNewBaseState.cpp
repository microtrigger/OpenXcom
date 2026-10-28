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
#include "ConfirmNewBaseState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Region.h"
#include "../Mod/RuleRegion.h"
#include "../Savegame/Base.h"
#include "BaseNameState.h"
#include "../Menu/ErrorMessageState.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Mod/RuleInterface.h"

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
 * Initializes all the elements in the Confirm New Base window.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to place.
 * @param globe Pointer to the Geoscape globe.
 */
ConfirmNewBaseState::ConfirmNewBaseState(Base *base, Globe *globe) : _base(base), _globe(globe), _cost(0)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 224, 72, 16, 64);
	_btnOk = new TextButton(54, 12, 68, 104);
	_btnCancel = new TextButton(54, 12, 138, 104);
	_txtCost = new Text(120, 9, 68, 80);
	_txtArea = new Text(120, 9, 68, 90);

	// Set palette
	setInterface("geoscape");

	add(_window, "genericWindow", "geoscape");
	add(_btnOk, "genericButton2", "geoscape");
	add(_btnCancel, "genericButton2", "geoscape");
	add(_txtCost, "genericText", "geoscape");
	add(_txtArea, "genericText", "geoscape");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ConfirmNewBaseState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ConfirmNewBaseState::btnOkClick, Options::keyOk);

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&ConfirmNewBaseState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&ConfirmNewBaseState::btnCancelClick, Options::keyCancel);

	std::string area;
	for (std::vector<Region*>::iterator i = _game->getSavedGame()->getRegions()->begin(); i != _game->getSavedGame()->getRegions()->end(); ++i)
	{
		if ((*i)->getRules()->insideRegion(_base->getLongitude(), _base->getLatitude()))
		{
			_cost = (*i)->getRules()->getBaseCost();
			area = tr((*i)->getRules()->getType());
			break;
		}
	}

	_txtCost->setText(tr("STR_COST_").arg(Unicode::formatFunding(_cost)));

	_txtArea->setText(tr("STR_AREA_").arg(area));
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
ConfirmNewBaseState::~ConfirmNewBaseState()
{

}

/**
 * Go to the Place Access Lift screen.
 * @param action Pointer to an action.
 */
void ConfirmNewBaseState::btnOkClick(Action *)
{
	if (_game->getSavedGame()->getFunds() >= _cost)
	{
		_game->getSavedGame()->setFunds(_game->getSavedGame()->getFunds() - _cost);
		_game->getSavedGame()->getBases()->push_back(_base);
		_game->pushState(new BaseNameState(_base, _globe, false));
	}
	else
	{
		_game->pushState(new ErrorMessageState(tr("STR_NOT_ENOUGH_MONEY"), _palette, _game->getMod()->getInterface("geoscape")->getElement("genericWindow")->color, "BACK01.SCR", _game->getMod()->getInterface("geoscape")->getElement("palette")->color));
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ConfirmNewBaseState::btnCancelClick(Action *)
{
	_globe->onMouseOver(0);
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ConfirmNewBaseState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> ConfirmNewBaseState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    revisions.push_back(_txtCost->agentTextRevision());
    revisions.push_back(_txtArea->agentTextRevision());
    revisions.push_back(_btnOk->agentLabelRevision());
    revisions.push_back(_btnCancel->agentLabelRevision());
    return revisions;
}
void ConfirmNewBaseState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool ConfirmNewBaseState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(ConfirmNewBaseState) || _modal || _window->agentPopupActive() ||
        !_base || !_game->isState(const_cast<ConfirmNewBaseState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState(); out.actor = _base; out.screen = "confirm_new_base";
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
    const auto cost = text(_txtCost), area = text(_txtArea);
    std::string buttons = "[";
    const auto button = [&](TextButton *widget, const char *id, const char *action) {
        const auto label = agentProjectNamedButton(widget, logical, painted, out, allowed);
        if (label.empty() || label.size() > 1024) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back(action, label, "visible_text");
    };
    if (cost != "null" && cost != "\"\"" && area != "null" && area != "\"\"")
        button(_btnOk, "ok", "confirm_new_base");
    button(_btnCancel, "cancel", "cancel_new_base_confirmation");
    buttons += "]";
    out.visible = "{\"cost\":" + cost + ",\"area\":" + area + ",\"buttons\":" + buttons + "}";
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size() <= 32768;
}
bool ConfirmNewBaseState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;
    if (!agentUiState(fresh) || !fresh.offers(request.action)) return false;
    if (request.action == "confirm_new_base") {
        btnOkClick(nullptr); return true;
    }
    if (request.action == "cancel_new_base_confirmation") { btnCancelClick(nullptr); return true; }
    return false;
}
#endif

}
