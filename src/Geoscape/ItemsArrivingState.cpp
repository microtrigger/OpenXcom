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
#include "ItemsArrivingState.h"
#include <sstream>
#include <algorithm>
#include "../Engine/Game.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#endif
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/Transfer.h"
#include "../Savegame/Craft.h"
#include "../Savegame/CraftWeapon.h"
#include "../Savegame/Vehicle.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleCraftWeapon.h"
#include "GeoscapeState.h"
#include "../Engine/Options.h"
#include "../Basescape/BasescapeState.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Items Arriving window.
 * @param game Pointer to the core game.
 * @param state Pointer to the Geoscape state.
 */
ItemsArrivingState::ItemsArrivingState(GeoscapeState *state) : _state(state), _base(0)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 320, 184, 0, 8, POPUP_BOTH);
	_btnOk = new TextButton(142, 16, 16, 166);
	_btnGotoBase = new TextButton(142, 16, 162, 166);
	_txtTitle = new Text(310, 17, 5, 18);
	_txtItem = new Text(114, 9, 16, 34);
	_txtQuantity = new Text(54, 9, 152, 34);
	_txtDestination = new Text(112, 9, 212, 34);
	_lstTransfers = new TextList(271, 112, 14, 50);

	// Set palette
	setInterface("itemsArriving");

	add(_window, "window", "itemsArriving");
	add(_btnOk, "button", "itemsArriving");
	add(_btnGotoBase, "button", "itemsArriving");
	add(_txtTitle, "text1", "itemsArriving");
	add(_txtItem, "text1", "itemsArriving");
	add(_txtQuantity, "text1", "itemsArriving");
	add(_txtDestination, "text1", "itemsArriving");
	add(_lstTransfers, "text2", "itemsArriving");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ItemsArrivingState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ItemsArrivingState::btnOkClick, Options::keyCancel);

	_btnGotoBase->setText(tr("STR_GO_TO_BASE"));
	_btnGotoBase->onMouseClick((ActionHandler)&ItemsArrivingState::btnGotoBaseClick);
	_btnGotoBase->onKeyboardPress((ActionHandler)&ItemsArrivingState::btnGotoBaseClick, Options::keyOk);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_ITEMS_ARRIVING"));

	_txtItem->setText(tr("STR_ITEM"));

	_txtQuantity->setText(tr("STR_QUANTITY_UC"));

	_txtDestination->setText(tr("STR_DESTINATION_UC"));

	_lstTransfers->setColumns(3, 155, 41, 98);
	_lstTransfers->setSelectable(true);
	_lstTransfers->setBackground(_window);
	_lstTransfers->setMargin(2);

	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		for (std::vector<Transfer*>::iterator j = (*i)->getTransfers()->begin(); j != (*i)->getTransfers()->end();)
		{
			if ((*j)->getHours() == 0)
			{
				_base = (*i);

				// Check if we have an automated use for an item
				if ((*j)->getType() == TRANSFER_ITEM)
				{
					RuleItem *item = _game->getMod()->getItem((*j)->getItems(), true);
					if (item->getBattleType() == BT_NONE)
					{
						for (std::vector<Craft*>::iterator c = (*i)->getCrafts()->begin(); c != (*i)->getCrafts()->end(); ++c)
						{
							(*c)->reuseItem((*j)->getItems());
						}
					}
				}

				// Remove transfer
				std::ostringstream ss;
				ss << (*j)->getQuantity();
				_lstTransfers->addRow(3, (*j)->getName(_game->getLanguage()).c_str(), ss.str().c_str(), (*i)->getName().c_str());
				delete *j;
				j = (*i)->getTransfers()->erase(j);
			}
			else
			{
				++j;
			}
		}
	}
}

/**
 *
 */
ItemsArrivingState::~ItemsArrivingState()
{

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ItemsArrivingState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Goes to the base for the respective transfer.
 * @param action Pointer to an action.
 */
void ItemsArrivingState::btnGotoBaseClick(Action *)
{
	_state->timerReset();
	_game->popState();
	_game->pushState(new BasescapeState(_base, _state->getGlobe()));
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ItemsArrivingState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> ItemsArrivingState::agentPaintRevisions() const
{
    return {_txtTitle->agentTextRevision(),
        _txtItem->agentTextRevision(),
        _txtQuantity->agentTextRevision(),
        _txtDestination->agentTextRevision(),
        _btnOk->agentLabelRevision(),
        _btnGotoBase->agentLabelRevision(),
        _lstTransfers->agentListRevision()};
}

void ItemsArrivingState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool ItemsArrivingState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<ItemsArrivingState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "items_arriving";
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
    const auto title = text(_txtTitle);
    const auto item = text(_txtItem), quantity = text(_txtQuantity), destination = text(_txtDestination);
    AgentListProjection list;
    // Only already painted UI rows are read. Arrivals were applied by the
    // ordinary constructor; projection never traverses or replays transfers.
    if (!agentProjectList(_lstTransfers, _lstTransfers, painted, 3, false, out, list, allowed)) return false;
    out.context = {list.up ? 1u : 0u, list.down ? 1u : 0u};
    if (list.up || list.down)
        out.actions.emplace_back("scroll_visible_list", "Scroll visible list", "visible_control_semantics", "scroll_direction");
    button(_btnOk, "ok", "dismiss_items_arriving");
    button(_btnGotoBase, "go_to_base", "open_arrival_base");
    buttons += "]";
    out.visible = "{\"title\":" + title + ",\"headings\":[" + item + "," + quantity + "," + destination +
        "],\"rows\":" + list.rows + ",\"scroll\":" + list.scroll + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    // Match the retained-event ring's conservative non-ASCII byte estimate,
    // leaving room for its envelope without changing shared transport policy.
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool ItemsArrivingState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "dismiss_items_arriving") { btnOkClick(nullptr); return true; }
    if (request.action == "open_arrival_base") { btnGotoBaseClick(nullptr); return true; }
    if (request.action == "scroll_visible_list" && current.context.size() == 2)
    {
        if (request.choice == "up" && current.context[0]) { _lstTransfers->scrollUp(false, false); return true; }
        if (request.choice == "down" && current.context[1]) { _lstTransfers->scrollDown(false, false); return true; }
    }
    return false;
}
#endif

}
