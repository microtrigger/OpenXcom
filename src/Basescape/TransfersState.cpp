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
#include "TransfersState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/Transfer.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Transfers window.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 */
TransfersState::TransfersState(Base *base) : _base(base)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 320, 184, 0, 8, POPUP_BOTH);
	_btnOk = new TextButton(288, 16, 16, 166);
	_txtTitle = new Text(278, 17, 21, 18);
	_txtItem = new Text(114, 9, 16, 34);
	_txtQuantity = new Text(54, 9, 152, 34);
	_txtArrivalTime = new Text(112, 9, 212, 34);
	_lstTransfers = new TextList(273, 112, 14, 50);

	// Set palette
	setInterface("transferInfo");

	add(_window, "window", "transferInfo");
	add(_btnOk, "button", "transferInfo");
	add(_txtTitle, "text", "transferInfo");
	add(_txtItem, "text", "transferInfo");
	add(_txtQuantity, "text", "transferInfo");
	add(_txtArrivalTime, "text", "transferInfo");
	add(_lstTransfers, "list", "transferInfo");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&TransfersState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&TransfersState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&TransfersState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_TRANSFERS"));

	_txtItem->setText(tr("STR_ITEM"));

	_txtQuantity->setText(tr("STR_QUANTITY_UC"));

	_txtArrivalTime->setText(tr("STR_ARRIVAL_TIME_HOURS"));

	_lstTransfers->setColumns(3, 155, 75, 46);
	_lstTransfers->setSelectable(true);
	_lstTransfers->setBackground(_window);
	_lstTransfers->setMargin(2);

	for (std::vector<Transfer*>::iterator i = _base->getTransfers()->begin(); i != _base->getTransfers()->end(); ++i)
	{
		std::ostringstream ss, ss2;
		ss << (*i)->getQuantity();
		ss2 << (*i)->getHours();
		_lstTransfers->addRow(3, (*i)->getName(_game->getLanguage()).c_str(), ss.str().c_str(), ss2.str().c_str());
	}
}

/**
 *
 */
TransfersState::~TransfersState()
{

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void TransfersState::btnOkClick(Action *)
{
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool TransfersState::agentUiState(Observer::AgentUiState &out) const
{
    if (!_game->isState(const_cast<TransfersState *>(this)) || _modal || !Observer::agentPaintHealthy()) return false;
    out = Observer::AgentUiState();
    out.screen = "base_transfers";
    const std::vector<Surface *> painted = Observer::agentFlattenPaintOrder(_surfaces);
    const auto regionAllowed = [this](const Observer::AgentUiState &candidate) -> bool
    { return _game->agentOverlayClear(candidate); };
    const auto include = [&](const Surface *widget) -> bool
    {
        Observer::AgentUiState candidate;
        Observer::agentIncludeWidget(candidate, widget);
        if (!regionAllowed(candidate)) return false;
        const std::size_t before = out.regions.size();
        out.regions.insert(out.regions.end(), candidate.regions.begin(), candidate.regions.end());
        if (Observer::agentRegionsDisjoint(out)) return true;
        out.regions.resize(before);
        return false;
    };
    const auto text = [&](const Text *widget) -> std::string
    {
        if (!Observer::agentWidgetUncovered(widget, painted)) return "null";
        const std::string value = Observer::agentTextJson(widget);
        return value != "null" && include(widget) ? value : "null";
    };
    const std::string title = text(_txtTitle);
    const std::string item = text(_txtItem), quantity = text(_txtQuantity), last = text(_txtArrivalTime);
    Observer::AgentListProjection list;
    // The ordinary hover selector remains enabled. This table is read-only:
    // no row activation is offered and every public row has selected=false.
    if (!Observer::agentProjectList(_lstTransfers, _lstTransfers, painted, 3, false, out, list, regionAllowed)) return false;
    out.visible = "{\"title\":" + title + ",\"headings\":[" + item + "," + quantity + "," + last +
        "],\"rows\":" + list.rows + ",\"scroll\":" + list.scroll + "}";
    // Keep dispatch tied to exactly the arrows admitted with the full table,
    // including disjointness with its title/headings, not a broader reprojection.
    out.context.push_back(list.up ? 1 : 0);
    out.context.push_back(list.down ? 1 : 0);
    if (list.up || list.down)
        out.actions.push_back(Observer::AgentUiAction("scroll_visible_list", "Scroll visible list", "visible_control_semantics", "scroll_direction"));
    if (Observer::agentWidgetUncovered(_btnOk, painted))
    {
        const std::string label = Observer::agentButtonLabel(_btnOk);
        if (!label.empty() && label.size() <= 1024 && include(_btnOk))
            out.actions.push_back(Observer::AgentUiAction("close_base_transfers", label, "visible_text"));
    }
    return true;
}

bool TransfersState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "close_base_transfers") { btnOkClick(nullptr); return true; }
    if (request.action == "scroll_visible_list" && current.context.size() == 2)
    {
        if (request.choice == "up" && current.context[0]) { _lstTransfers->scrollUp(false, false); return true; }
        if (request.choice == "down" && current.context[1]) { _lstTransfers->scrollDown(false, false); return true; }
    }
    return false;
}
#endif

}
