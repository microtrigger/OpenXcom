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
#include <sstream>
#include "TransferConfirmState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#endif
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Savegame/Base.h"
#include "TransferItemsState.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Confirm Transfer window.
 * @param game Pointer to the core game.
 * @param base Pointer to the destination base.
 * @param state Pointer to the Transfer state.
 */
TransferConfirmState::TransferConfirmState(Base *base, TransferItemsState *state) : _base(base), _state(state)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 320, 80, 0, 60);
	_btnCancel = new TextButton(128, 16, 176, 115);
	_btnOk = new TextButton(128, 16, 16, 115);
	_txtTitle = new Text(310, 17, 5, 75);
	_txtCost = new Text(60, 17, 110, 95);
	_txtTotal = new Text(100, 17, 170, 95);

	// Set palette
	setInterface("transferConfirm");

	add(_window, "window", "transferConfirm");
	add(_btnCancel, "button", "transferConfirm");
	add(_btnOk, "button", "transferConfirm");
	add(_txtTitle, "text", "transferConfirm");
	add(_txtCost, "text", "transferConfirm");
	add(_txtTotal, "text", "transferConfirm");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&TransferConfirmState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&TransferConfirmState::btnCancelClick, Options::keyCancel);

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&TransferConfirmState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&TransferConfirmState::btnOkClick, Options::keyOk);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_TRANSFER_ITEMS_TO").arg(_base->getName()));

	_txtCost->setBig();
	_txtCost->setText(tr("STR_COST"));

	std::ostringstream ss;
	ss << Unicode::TOK_COLOR_FLIP << Unicode::formatFunding(_state->getTotal());

	_txtTotal->setBig();
	_txtTotal->setText(ss.str());
}

/**
 *
 */
TransferConfirmState::~TransferConfirmState()
{
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void TransferConfirmState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Completes the transfer.
 * @param action Pointer to an action.
 */
void TransferConfirmState::btnOkClick(Action *)
{
	_state->completeTransfer();
	_game->popState();
	_game->popState();
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool TransferConfirmState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> TransferConfirmState::agentPaintRevisions() const
{
    return {_txtTitle->agentTextRevision(), _txtCost->agentTextRevision(), _txtTotal->agentTextRevision(), _btnOk->agentLabelRevision(), _btnCancel->agentLabelRevision()};
}

void TransferConfirmState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool TransferConfirmState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<TransferConfirmState *>(this)) || _modal || !agentPaintHealthy() || !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState(); out.actor = _base;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const auto allowed = [this](const AgentUiState &part) { return _game->agentOverlayClear(part); };
    const auto include = [&](const Surface *widget) -> bool {
        if (!agentWidgetUncovered(widget,painted)) return false;
        AgentUiState part; agentIncludeWidget(part,widget);
        if (part.regions.empty() || !allowed(part)) return false;
        const auto before=out.regions.size();
        out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before); return false;
    };
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        const auto value=agentTextJson(widget);
        return value!="null" && (widget->getText().empty() || include(widget)) ? value : "null";
    };
    std::string buttons="[";
    const auto button = [&](TextButton *widget,const char *id,const char *action) {
        out.widgets.emplace_back(widget, widget->agentLabelRevision());
        const auto label=agentButtonLabel(widget);
        if (label.empty() || label.size()>1024 || !include(widget)) return;
        if (buttons.size()>1) buttons+=",";
        buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";
        out.actions.emplace_back(action,label,"visible_text");
    };
    out.screen="transfer_confirm"; out.save=_state;
    const auto title=text(_txtTitle), cost=text(_txtCost), total=text(_txtTotal);
    button(_btnOk,"confirm","confirm_transfer");
    button(_btnCancel,"cancel","cancel_transfer_confirmation"); buttons+="]";
    out.visible="{\"title\":"+title+",\"cost\":"+cost+",\"total\":"+total+",\"buttons\":"+buttons+"}";
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size()<=9000;
}
bool TransferConfirmState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action=="confirm_transfer") { btnOkClick(nullptr); return true; }
    if (request.action=="cancel_transfer_confirmation") { btnCancelClick(nullptr); return true; }
    return false;
}
#endif

}
