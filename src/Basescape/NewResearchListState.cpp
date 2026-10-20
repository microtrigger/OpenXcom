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
#include "NewResearchListState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Mod/RuleResearch.h"
#include "ResearchInfoState.h"

namespace OpenXcom
{
/**
 * Initializes all the elements in the Research list screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 */
NewResearchListState::NewResearchListState(Base *base) : _base(base)
{
	_screen = false;

	_window = new Window(this, 230, 140, 45, 30, POPUP_BOTH);
	_btnOK = new TextButton(214, 16, 53, 146);
	_txtTitle = new Text(214, 16, 53, 38);
	_lstResearch = new TextList(198, 88, 53, 54);

	// Set palette
	setInterface("selectNewResearch");

	add(_window, "window", "selectNewResearch");
	add(_btnOK, "button", "selectNewResearch");
	add(_txtTitle, "text", "selectNewResearch");
	add(_lstResearch, "list", "selectNewResearch");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK05.SCR"));

	_btnOK->setText(tr("STR_OK"));
	_btnOK->onMouseClick((ActionHandler)&NewResearchListState::btnOKClick);
	_btnOK->onKeyboardPress((ActionHandler)&NewResearchListState::btnOKClick, Options::keyCancel);

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_NEW_RESEARCH_PROJECTS"));

	_lstResearch->setColumns(1, 190);
	_lstResearch->setSelectable(true);
	_lstResearch->setBackground(_window);
	_lstResearch->setMargin(8);
	_lstResearch->setAlign(ALIGN_CENTER);
	_lstResearch->onMouseClick((ActionHandler)&NewResearchListState::onSelectProject);
}

/**
 * Initializes the screen (fills the list).
 */
void NewResearchListState::init()
{
	State::init();
	fillProjectList();
}

/**
 * Selects the RuleResearch to work on.
 * @param action Pointer to an action.
 */
void NewResearchListState::onSelectProject(Action *)
{
	selectProject(_lstResearch->getSelectedRow());
}

void NewResearchListState::selectProject(std::size_t row)
{
	if (row < _projects.size())
		_game->pushState(new ResearchInfoState(_base, _projects[row]));
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void NewResearchListState::btnOKClick(Action *)
{
	_game->popState();
}

/**
 * Fills the list with possible ResearchProjects.
 */
void NewResearchListState::fillProjectList()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	++_agentMappingRevision;
#endif
	_projects.clear();
	_lstResearch->clearList();
	// Note: this is the *only* place where this method is called with considerDebugMode = true
	_game->getSavedGame()->getAvailableResearchProjects(_projects, _game->getMod() , _base, true);
	std::vector<RuleResearch*>::iterator it = _projects.begin();
	while (it != _projects.end())
	{
		// EXPLANATION
		// -----------
		// Projects with "requires" can only be discovered/researched indirectly
		//  - this is because we can't reliably determine if they are unlocked or not
		// Example:
		//  - Alien Origins + Alien Leader => ALIEN_LEADER_PLUS is discovered
		//  - Alien Leader + Alien Origins => ALIEN_LEADER_PLUS is NOT discovered (you need to research another alien leader/commander)
		// If we wanted to allow also direct research of projects with "requires",
		// we would need to implement a slightly more complicated unlocking algorithm
		// and more importantly, we would need to remember the list of unlocked topics
		// in the save file (currently this is not done, the list is calculated on-the-fly).
		// Summary:
		//  - it would be possible to remove this condition, but more refactoring would be needed
		//  - for now, handling "requires" via zero-cost helpers (e.g. STR_LEADER_PLUS)... is enough
		if ((*it)->getRequirements().empty())
		{
			_lstResearch->addRow(1, tr((*it)->getName()).c_str());
			++it;
		}
		else
		{
			it = _projects.erase(it);
		}
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool NewResearchListState::agentUiState(Observer::AgentUiState &out) const
{
    if (!_game->isState(const_cast<NewResearchListState *>(this)) || _modal || !Observer::agentPaintHealthy()) return false;
    if (!_base) return false;
    out = Observer::AgentUiState();
    out.screen = "new_research";
    out.actor = _base;
    out.actorRevision = _agentMappingRevision;
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
    Observer::AgentListProjection list;
    if (!Observer::agentProjectList(_lstResearch, _lstResearch, painted, 1, true, out, list, regionAllowed, true, true)) return false;
    for (const std::size_t row : out.rows)
    {
        if (row >= _projects.size()) return false;
        out.widgets.push_back(std::make_pair(_projects[row], 0));
    }
    if (title != "null" && !out.rows.empty())
        out.actions.push_back(Observer::AgentUiAction("select_research_project", "Select research project", "visible_control_semantics", "project_row"));
    out.context.push_back(list.up ? 1 : 0);
    out.context.push_back(list.down ? 1 : 0);
    if (list.up || list.down)
        out.actions.push_back(Observer::AgentUiAction("scroll_visible_list", "Scroll visible list", "visible_control_semantics", "scroll_direction"));
    std::string buttons = "[";
    const auto button = [&](const TextButton *widget, const char *id, const char *action)
    {
        if (!Observer::agentWidgetUncovered(widget, painted)) return;
        const std::string label = Observer::agentButtonLabel(widget);
        if (label.empty() || label.size() > 1024 || !include(widget)) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + Observer::agentJsonString(id) + ",\"label\":" + Observer::agentJsonString(label) + "}";
        out.actions.push_back(Observer::AgentUiAction(action, label, "visible_text"));
    };
    button(_btnOK, "close", "close_research_list");
    out.visible = "{\"title\":" + title + ",\"summary\":[" + std::string() +
        "],\"headings\":[" + std::string("null") + "],\"rows\":" + list.rows +
        ",\"scroll\":" + list.scroll + ",\"buttons\":" + buttons + "]}";
    return !out.regions.empty() && Observer::agentRegionsDisjoint(out) && out.visible.size() <= 32768;
}

bool NewResearchListState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "close_research_list") { btnOKClick(nullptr); return true; }
    if (request.action == "select_research_project")
    {
        for (std::size_t slot = 0; slot < current.rows.size(); ++slot)
            if (request.choice == "r" + std::to_string(slot))
            {
                const std::size_t row = current.rows[slot];
                if (row >= _projects.size() || !_lstResearch->agentSelectProjectRow(row, true)) return false;
                selectProject(row);
                return true;
            }
    }
    if (request.action == "scroll_visible_list" && current.context.size() == 2)
    {
        if (request.choice == "up" && current.context[0]) { _lstResearch->scrollUp(false, false); return true; }
        if (request.choice == "down" && current.context[1]) { _lstResearch->scrollDown(false, false); return true; }
    }
    return false;
}
#endif

}
