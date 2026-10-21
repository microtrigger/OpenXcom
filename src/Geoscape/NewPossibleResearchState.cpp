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
#include <algorithm>
#include "NewPossibleResearchState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#endif
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Mod/Mod.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Mod/RuleResearch.h"
#include "../Basescape/ResearchState.h"
#include "../Savegame/SavedGame.h"
#include "../Engine/Options.h"

namespace OpenXcom
{
/**
 * Initializes all the elements in the EndResearch screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param possibilities List of newly possible ResearchProject
 */
NewPossibleResearchState::NewPossibleResearchState(Base * base, const std::vector<RuleResearch *> & possibilities) : _base(base)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 288, 180, 16, 10);
	_btnOk = new TextButton(160, 14, 80, 149);
	_btnResearch = new TextButton(160, 14, 80, 165);
	_txtTitle = new Text(288, 40, 16, 20);
	_lstPossibilities = new TextList(250, 96, 35, 50);

	// Set palette
	setInterface("geoResearch");

	add(_window, "window", "geoResearch");
	add(_btnOk, "button", "geoResearch");
	add(_btnResearch, "button", "geoResearch");
	add(_txtTitle, "text1", "geoResearch");
	add(_lstPossibilities, "text2", "geoResearch");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK05.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&NewPossibleResearchState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&NewPossibleResearchState::btnOkClick, Options::keyCancel);
	_btnResearch->setText(tr("STR_ALLOCATE_RESEARCH"));
	_btnResearch->onMouseClick((ActionHandler)&NewPossibleResearchState::btnResearchClick);
	_btnResearch->onKeyboardPress((ActionHandler)&NewPossibleResearchState::btnResearchClick, Options::keyOk);
	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);

	_lstPossibilities->setColumns(1, 250);
	_lstPossibilities->setBig();
	_lstPossibilities->setAlign(ALIGN_CENTER);
	_lstPossibilities->setScrolling(true, 0);

	bool foundNew = false;
	for (std::vector<RuleResearch *>::const_iterator iter = possibilities.begin(); iter != possibilities.end(); ++iter)
	{
		// Note: ignore all topics with "requires" (same reason as in NewResearchListState::fillProjectList())
		if ((*iter)->getRequirements().empty())
		{
			// Also ignore:
			// 1. things that already popped before
			// 2. things that never popped, but are researched already (can happen for topics that can be researched multiple times)
			if (!_game->getSavedGame()->wasResearchPopped(*iter) && !_game->getSavedGame()->isResearched((*iter)->getName(), false))
			{
				_game->getSavedGame()->addPoppedResearch((*iter));
				_lstPossibilities->addRow(1, tr((*iter)->getName()).c_str());
				foundNew = true;
			}
		}
	}

	if (foundNew)
	{
		_txtTitle->setText(tr("STR_WE_CAN_NOW_RESEARCH"));
	}
}

/**
 * return to the previous screen
 * @param action Pointer to an action.
 */
void NewPossibleResearchState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Open the ResearchState so the player can dispatch available scientist.
 * @param action Pointer to an action.
 */
void NewPossibleResearchState::btnResearchClick(Action *)
{
	_game->popState();
	_game->pushState (new ResearchState(_base));
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool NewPossibleResearchState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> NewPossibleResearchState::agentPaintRevisions() const
{
    return {_txtTitle->agentTextRevision(),
        _btnOk->agentLabelRevision(),
        _btnResearch->agentLabelRevision(),
        _lstPossibilities->agentListRevision()};
}

void NewPossibleResearchState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool NewPossibleResearchState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<NewPossibleResearchState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "new_possible_research";
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
        if (widget->getText().size() > 1024 || value == "null" || !agentWidgetUncovered(widget, painted)) return "null";
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
    button(_btnOk, "ok", "dismiss_new_research");
    button(_btnResearch, "allocate", "open_research_allocation");
    buttons += "]";
    AgentListProjection list;
    if (!agentProjectList(_lstPossibilities, _lstPossibilities, painted, 1, false, out, list, allowed)) return false;
    out.context.push_back(list.up ? 1 : 0);
    out.context.push_back(list.down ? 1 : 0);
    if (list.up || list.down)
        out.actions.emplace_back("scroll_visible_list", "Scroll visible list", "visible_control_semantics", "scroll_direction");
    out.visible = "{\"title\":" + title + ",\"rows\":" + list.rows + ",\"scroll\":" + list.scroll + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    // Match the retained-event ring's conservative non-ASCII byte estimate,
    // leaving room for its envelope without changing shared transport policy.
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool NewPossibleResearchState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "dismiss_new_research") { btnOkClick(nullptr); return true; }
    if (request.action == "open_research_allocation") { btnResearchClick(nullptr); return true; }
    if (request.action == "scroll_visible_list" && current.context.size() == 2)
    {
        if (request.choice == "up" && current.context[0]) { _lstPossibilities->scrollUp(false, false); return true; }
        if (request.choice == "down" && current.context[1]) { _lstPossibilities->scrollDown(false, false); return true; }
    }
    return false;
}
#endif

}
