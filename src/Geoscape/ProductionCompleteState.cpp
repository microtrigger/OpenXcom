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
#include <assert.h>
#include "ProductionCompleteState.h"
#include "../Engine/Game.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#endif
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "GeoscapeState.h"
#include "../Engine/Options.h"
#include "../Basescape/BasescapeState.h"
#include "../Basescape/ManufactureState.h"
#include "../Savegame/Base.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in a Production Complete window.
 * @param game Pointer to the core game.
 * @param base Pointer to base the production belongs to.
 * @param item Item that finished producing.
 * @param state Pointer to the Geoscape state.
 * @param endType What ended the production.
 */
ProductionCompleteState::ProductionCompleteState(Base *base, const std::string &item, GeoscapeState *state, productionProgress_e endType) : _base(base), _state(state), _endType(endType)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 256, 160, 32, 20, POPUP_BOTH);
	_btnOk = new TextButton(118, 18, 40, 154);
	_btnGotoBase = new TextButton(118, 18, 162, 154);
	_txtMessage = new Text(246, 110, 37, 35);

	// Set palette
	setInterface("geoManufactureComplete");

	add(_window, "window", "geoManufactureComplete");
	add(_btnOk, "button", "geoManufactureComplete");
	add(_btnGotoBase, "button", "geoManufactureComplete");
	add(_txtMessage, "text1", "geoManufactureComplete");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK17.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ProductionCompleteState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ProductionCompleteState::btnOkClick, Options::keyCancel);

	if (_endType != PROGRESS_CONSTRUCTION)
	{
		_btnGotoBase->setText(tr("STR_ALLOCATE_MANUFACTURE"));
	}
	else
	{
		_btnGotoBase->setText(tr("STR_GO_TO_BASE"));
	}
	_btnGotoBase->onMouseClick((ActionHandler)&ProductionCompleteState::btnGotoBaseClick);

	_txtMessage->setAlign(ALIGN_CENTER);
	_txtMessage->setVerticalAlign(ALIGN_MIDDLE);
	_txtMessage->setBig();
	_txtMessage->setWordWrap(true);
	std::string s;
	switch(_endType)
	{
	case PROGRESS_CONSTRUCTION:
		s = tr("STR_CONSTRUCTION_OF_FACILITY_AT_BASE_IS_COMPLETE").arg(item).arg(base->getName());
		break;
	case PROGRESS_COMPLETE:
		s = tr("STR_PRODUCTION_OF_ITEM_AT_BASE_IS_COMPLETE").arg(item).arg(base->getName());
		break;
	case PROGRESS_NOT_ENOUGH_MONEY:
		s = tr("STR_NOT_ENOUGH_MONEY_TO_PRODUCE_ITEM_AT_BASE").arg(item).arg(base->getName());
		break;
	case PROGRESS_NOT_ENOUGH_MATERIALS:
		s = tr("STR_NOT_ENOUGH_SPECIAL_MATERIALS_TO_PRODUCE_ITEM_AT_BASE").arg(item).arg(base->getName());
		break;
	default:
		assert(false);
	}
	_txtMessage->setText(s);
}

/**
 *
 */
ProductionCompleteState::~ProductionCompleteState()
{

}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void ProductionCompleteState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Goes to the base for the respective production.
 * @param action Pointer to an action.
 */
void ProductionCompleteState::btnGotoBaseClick(Action *)
{
	_state->timerReset();
	_game->popState();
	if (_endType != PROGRESS_CONSTRUCTION)
	{
		_game->pushState(new ManufactureState(_base));
	}
	else
	{
		_game->pushState(new BasescapeState(_base, _state->getGlobe()));
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ProductionCompleteState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> ProductionCompleteState::agentPaintRevisions() const
{
    return {_txtMessage->agentTextRevision(),
        _btnOk->agentLabelRevision(),
        _btnGotoBase->agentLabelRevision()};
}

void ProductionCompleteState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool ProductionCompleteState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<ProductionCompleteState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "production_complete";
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const auto allowed = [this](const AgentUiState &part) { return _game->agentOverlayClear(part); };
    const auto include = [&](const Surface *widget) -> bool {
        AgentUiState part;
        agentIncludeWidget(part, widget);
        if (part.regions.empty() || !agentRegionsUncovered(part.regions, widget, painted) || !allowed(part)) return false;
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before);
        return false;
    };
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        const auto value = agentTextJson(widget);
        if (value == "null" || !agentWidgetUncovered(widget, painted)) return "null";
        return widget->getText().empty() || include(widget) ? value : "null";
    };
    std::string buttons = "[";
    const auto button = [&](TextButton *widget, const char *id, const char *action) {
        out.widgets.emplace_back(widget, widget->agentLabelRevision());
        const auto label = agentButtonLabel(widget);
        if (label.empty() || label.size() > 1024 || !include(widget)) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(label) + "}";
        if (action) out.actions.emplace_back(action, label, "visible_text");
    };
    const auto message = text(_txtMessage);
    button(_btnOk, "ok", "dismiss_production_complete");
    button(_btnGotoBase, "destination", "open_production_destination");
    buttons += "]";
    // The label is the drawn caption. No production enum or hidden reason
    // is projected; the ordinary callback retains destination policy.
    out.visible = "{\"message\":" + message + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    // Match the retained-event ring's conservative non-ASCII byte estimate,
    // leaving room for its envelope without changing shared transport policy.
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool ProductionCompleteState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "dismiss_production_complete") { btnOkClick(nullptr); return true; }
    if (request.action == "open_production_destination") { btnGotoBaseClick(nullptr); return true; }
    return false;
}
#endif

}
