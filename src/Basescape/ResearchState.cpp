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
#include "ResearchState.h"
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
#include "NewResearchListState.h"
#include "../Savegame/ResearchProject.h"
#include "../Mod/RuleResearch.h"
#include "ResearchInfoState.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Research screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 */
ResearchState::ResearchState(Base *base) : _base(base)
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnNew = new TextButton(148, 16, 8, 176);
	_btnOk = new TextButton(148, 16, 164, 176);
	_txtTitle = new Text(310, 17, 5, 8);
	_txtAvailable = new Text(150, 9, 10, 24);
	_txtAllocated = new Text(150, 9, 160, 24);
	_txtSpace = new Text(300, 9, 10, 34);
	_txtProject = new Text(110, 17, 10, 44);
	_txtScientists = new Text(106, 17, 120, 44);
	_txtProgress = new Text(84, 9, 226, 44);
	_lstResearch = new TextList(288, 112, 8, 62);

	// Set palette
	setInterface("researchMenu");

	add(_window, "window", "researchMenu");
	add(_btnNew, "button", "researchMenu");
	add(_btnOk, "button", "researchMenu");
	add(_txtTitle, "text", "researchMenu");
	add(_txtAvailable, "text", "researchMenu");
	add(_txtAllocated, "text", "researchMenu");
	add(_txtSpace, "text", "researchMenu");
	add(_txtProject, "text", "researchMenu");
	add(_txtScientists, "text", "researchMenu");
	add(_txtProgress, "text", "researchMenu");
	add(_lstResearch, "list", "researchMenu");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK05.SCR"));

	_btnNew->setText(tr("STR_NEW_PROJECT"));
	_btnNew->onMouseClick((ActionHandler)&ResearchState::btnNewClick);

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ResearchState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ResearchState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_CURRENT_RESEARCH"));

	_txtProject->setWordWrap(true);
	_txtProject->setText(tr("STR_RESEARCH_PROJECT"));

	_txtScientists->setWordWrap(true);
	_txtScientists->setText(tr("STR_SCIENTISTS_ALLOCATED_UC"));

	_txtProgress->setText(tr("STR_PROGRESS"));

	_lstResearch->setColumns(3, 158, 58, 70);
	_lstResearch->setSelectable(true);
	_lstResearch->setBackground(_window);
	_lstResearch->setMargin(2);
	_lstResearch->setWordWrap(true);
	_lstResearch->onMouseClick((ActionHandler)&ResearchState::onSelectProject);
}

/**
 *
 */
ResearchState::~ResearchState()
{
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ResearchState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ResearchState::btnNewClick(Action *)
{
	_game->pushState(new NewResearchListState(_base));
}

/**
 * Displays the list of possible ResearchProjects.
 * @param action Pointer to an action.
 */
void ResearchState::onSelectProject(Action *)
{
	selectProject(_lstResearch->getSelectedRow());
}

void ResearchState::selectProject(std::size_t row)
{
	const std::vector<ResearchProject *> & baseProjects(_base->getResearch());
	if (row < baseProjects.size())
		_game->pushState(new ResearchInfoState(_base, baseProjects[row]));
}

/**
 * Updates the research list
 * after going to other screens.
 */
void ResearchState::init()
{
	State::init();
	fillProjectList();
}

/**
 * Fills the list with Base ResearchProjects. Also updates count of available lab space and available/allocated scientists.
 */
void ResearchState::fillProjectList()
{
	const std::vector<ResearchProject *> & baseProjects(_base->getResearch());
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	++_agentMappingRevision;
	_agentProjectMapping = baseProjects;
#endif
	_lstResearch->clearList();
	for (std::vector<ResearchProject *>::const_iterator iter = baseProjects.begin(); iter != baseProjects.end(); ++iter)
	{
		std::ostringstream sstr;
		sstr << (*iter)->getAssigned();
		const RuleResearch *r = (*iter)->getRules();

		std::string wstr = tr(r->getName());
		_lstResearch->addRow(3, wstr.c_str(), sstr.str().c_str(), tr((*iter)->getResearchProgress()).c_str());
	}
	_txtAvailable->setText(tr("STR_SCIENTISTS_AVAILABLE").arg(_base->getAvailableScientists()));
	_txtAllocated->setText(tr("STR_SCIENTISTS_ALLOCATED").arg(_base->getAllocatedScientists()));
	_txtSpace->setText(tr("STR_LABORATORY_SPACE_AVAILABLE").arg(_base->getFreeLaboratories()));
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ResearchState::agentUiState(Observer::AgentUiState &out) const
{
    if (!_game->isState(const_cast<ResearchState *>(this)) || _modal || !Observer::agentPaintHealthy()) return false;
    // Identity only: never project model values into the public DTO. A stale
    // painted list may not select a different current model row.
    if (!_base || _base->getResearch() != _agentProjectMapping) return false;
    out = Observer::AgentUiState();
    out.screen = "research";
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
    const std::string available = text(_txtAvailable), allocated = text(_txtAllocated), space = text(_txtSpace);
    const std::string project = text(_txtProject), scientists = text(_txtScientists), progress = text(_txtProgress);
    Observer::AgentListProjection list;
    if (!Observer::agentProjectList(_lstResearch, _lstResearch, painted, 3, true, out, list, regionAllowed, true)) return false;
    for (const std::size_t row : out.rows)
    {
        if (row >= _agentProjectMapping.size()) return false;
        out.widgets.push_back(std::make_pair(_agentProjectMapping[row], 0));
    }
    if (title != "null" && !out.rows.empty() && !list.firstLabel.empty())
        out.actions.push_back(Observer::AgentUiAction("select_research_project", list.firstLabel, "visible_text", "project_row"));
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
    button(_btnNew, "new", "open_new_research");
    button(_btnOk, "close", "close_research_list");
    out.visible = "{\"title\":" + title + ",\"summary\":[" + available + "," + allocated + "," + space +
        "],\"headings\":[" + project + "," + scientists + "," + progress + "],\"rows\":" + list.rows +
        ",\"scroll\":" + list.scroll + ",\"buttons\":" + buttons + "]}";
    return !out.regions.empty() && Observer::agentRegionsDisjoint(out) && out.visible.size() <= 32768;
}

bool ResearchState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "open_new_research") { btnNewClick(nullptr); return true; }
    if (request.action == "close_research_list") { btnOkClick(nullptr); return true; }
    if (request.action == "select_research_project")
    {
        for (std::size_t slot = 0; slot < current.rows.size(); ++slot)
            if (request.choice == "r" + std::to_string(slot))
            {
                const std::size_t row = current.rows[slot];
                if (row >= _agentProjectMapping.size() || !_lstResearch->agentSelectProjectRow(row)) return false;
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
