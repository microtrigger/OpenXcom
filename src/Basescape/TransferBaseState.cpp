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
#include "TransferBaseState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#endif
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/Region.h"
#include "../Mod/RuleRegion.h"
#include "TransferItemsState.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Select Destination Base window.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 */
TransferBaseState::TransferBaseState(Base *base) : _base(base)
{
	// Create objects
	_window = new Window(this, 280, 140, 20, 30);
	_btnCancel = new TextButton(264, 16, 28, 146);
	_txtTitle = new Text(270, 17, 25, 38);
	_txtFunds = new Text(250, 9, 30, 54);
	_txtName = new Text(130, 17, 28, 64);
	_txtArea = new Text(130, 17, 160, 64);
	_lstBases = new TextList(248, 64, 28, 80);

	// Set palette
	setInterface("transferBaseSelect");

	add(_window, "window", "transferBaseSelect");
	add(_btnCancel, "button", "transferBaseSelect");
	add(_txtTitle, "text", "transferBaseSelect");
	add(_txtFunds, "text", "transferBaseSelect");
	add(_txtName, "text", "transferBaseSelect");
	add(_txtArea, "text", "transferBaseSelect");
	add(_lstBases, "list", "transferBaseSelect");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&TransferBaseState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&TransferBaseState::btnCancelClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_SELECT_DESTINATION_BASE"));

	_txtFunds->setText(tr("STR_CURRENT_FUNDS").arg(Unicode::formatFunding(_game->getSavedGame()->getFunds())));

	_txtName->setText(tr("STR_NAME"));
	_txtName->setBig();

	_txtArea->setText(tr("STR_AREA"));
	_txtArea->setBig();

	_lstBases->setColumns(2, 130, 116);
	_lstBases->setSelectable(true);
	_lstBases->setBackground(_window);
	_lstBases->setMargin(2);
	_lstBases->onMouseClick((ActionHandler)&TransferBaseState::lstBasesClick);

	int row = 0;
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		if ((*i) != _base)
		{
			// Get area
			std::string area;
			for (std::vector<Region*>::iterator j = _game->getSavedGame()->getRegions()->begin(); j != _game->getSavedGame()->getRegions()->end(); ++j)
			{
				if ((*j)->getRules()->insideRegion((*i)->getLongitude(), (*i)->getLatitude()))
				{
					area = tr((*j)->getRules()->getType());
					break;
				}
			}
			std::ostringstream ss;
			ss << Unicode::TOK_COLOR_FLIP << area;
			_lstBases->addRow(2, (*i)->getName().c_str(), ss.str().c_str());
			_bases.push_back(*i);
			row++;
		}
	}
}

/**
 *
 */
TransferBaseState::~TransferBaseState()
{
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void TransferBaseState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Shows the Transfer screen for the selected base.
 * @param action Pointer to an action.
 */
void TransferBaseState::lstBasesClick(Action *)
{
	_game->pushState(new TransferItemsState(_base, _bases[_lstBases->getSelectedRow()]));
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool TransferBaseState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> TransferBaseState::agentPaintRevisions() const
{
    return {_txtTitle->agentTextRevision(), _txtFunds->agentTextRevision(), _txtName->agentTextRevision(), _txtArea->agentTextRevision(), _btnCancel->agentLabelRevision(), _lstBases->agentListRevision()};
}

void TransferBaseState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool TransferBaseState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<TransferBaseState *>(this)) || _modal || !agentPaintHealthy() || !_agentDestinationPaintFull || !agentDestinationFull() ||
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
    out.screen="transfer_destination";
    out.widgets.emplace_back(_lstBases,_lstBases->agentListRevision());
    const auto title=text(_txtTitle), funds=text(_txtFunds), name=text(_txtName), area=text(_txtArea);
    button(_btnCancel,"cancel","cancel_transfer_destination"); buttons+="]";
    AgentListProjection list;
    if (!agentProjectList(_lstBases,_lstBases,painted,2,true,out,list,allowed,true)) return false;
    out.context={list.up?1u:0u,list.down?1u:0u};
    out.visible="{\"title\":"+title+",\"funds\":"+funds+",\"headings\":["+name+","+area+"],\"rows\":"+list.rows+",\"scroll\":"+list.scroll+",\"buttons\":"+buttons+"}";
    if (!out.rows.empty()) out.actions.emplace_back("choose_visible_row","Choose visible row","visible_control_semantics","visible_row");
    if (list.up || list.down) out.actions.emplace_back("scroll_visible_list","Scroll visible list","visible_control_semantics","scroll_direction");
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size()<=9000;
}
bool TransferBaseState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action=="cancel_transfer_destination") { btnCancelClick(nullptr); return true; }
    if (request.action=="scroll_visible_list" && current.context.size()==2) {
        if (request.choice=="up" && current.context[0]) { _lstBases->scrollUp(false,false); return true; }
        if (request.choice=="down" && current.context[1]) { _lstBases->scrollDown(false,false); return true; }
    }
    if (request.action!="choose_visible_row" || request.choice.size()!=2 || request.choice[0]!='r' || request.choice[1]<'0' || request.choice[1]>'7') return false;
    const unsigned index=request.choice[1]-'0';
    if (index>=current.rows.size() || current.rows[index]>=_bases.size() || !_lstBases->agentSelectProjectRow(current.rows[index])) return false;
    lstBasesClick(nullptr); return true;
}
#endif

}
