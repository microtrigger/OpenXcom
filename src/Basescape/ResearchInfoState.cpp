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
#include "ResearchInfoState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Savegame/Base.h"
#include "../Mod/RuleResearch.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/ResearchProject.h"
#include "../Interface/ArrowButton.h"
#include "../Engine/Timer.h"
#include "../Engine/RNG.h"
#include <climits>
#include <algorithm>

namespace OpenXcom
{

/**
 * Initializes all the elements in the ResearchProject screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param rule A RuleResearch which will be used to create a new ResearchProject
 */
ResearchInfoState::ResearchInfoState(Base *base, RuleResearch *rule) : _base(base), _project(new ResearchProject(rule, int(rule->getCost() * OpenXcom::RNG::generate(50, 150)/100))), _rule(rule)
{
	buildUi();
}

/**
 * Initializes all the elements in the ResearchProject screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param project A ResearchProject to modify
 */
ResearchInfoState::ResearchInfoState(Base *base, ResearchProject *project) : _base(base), _project(project), _rule(0)
{
	buildUi();
}

/**
 * Builds dialog.
 */
void ResearchInfoState::buildUi()
{
	_screen = false;

	_window = new Window(this, 230, 140, 45, 30);
	_txtTitle = new Text(210, 17, 61, 40);

	_txtAvailableScientist = new Text(210, 9, 61, 60);
	_txtAvailableSpace = new Text(210, 9, 61, 70);
	_txtAllocatedScientist = new Text(210, 17, 61, 80);
	_txtMore = new Text(110, 17, 85, 100);
	_txtLess = new Text(110, 17, 85, 120);
	_btnCancel = new TextButton(90, 16, 61, 145);
	_btnOk = new TextButton(90, 16, 169, 145);

	_btnMore = new ArrowButton(ARROW_BIG_UP, 13, 14, 195, 100);
	_btnLess = new ArrowButton(ARROW_BIG_DOWN, 13, 14, 195, 120);

	_surfaceScientists = new InteractiveSurface(230, 140, 45, 30);
	_surfaceScientists->onMouseClick((ActionHandler)&ResearchInfoState::handleWheel, 0);

	// Set palette
	setInterface("allocateResearch");

	add(_surfaceScientists);
	add(_window, "window", "allocateResearch");
	add(_btnOk, "button2", "allocateResearch");
	add(_btnCancel, "button2", "allocateResearch");
	add(_txtTitle, "text", "allocateResearch");
	add(_txtAvailableScientist, "text", "allocateResearch");
	add(_txtAvailableSpace, "text", "allocateResearch");
	add(_txtAllocatedScientist, "text", "allocateResearch");
	add(_txtMore, "text", "allocateResearch");
	add(_txtLess, "text", "allocateResearch");
	add(_btnMore, "button1", "allocateResearch");
	add(_btnLess, "button1", "allocateResearch");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK05.SCR"));

	_txtTitle->setBig();

	_txtTitle->setText(_rule ? tr(_rule->getName()) : tr(_project->getRules()->getName()));

	_txtAllocatedScientist->setBig();

	_txtMore->setText(tr("STR_INCREASE"));
	_txtLess->setText(tr("STR_DECREASE"));

	_txtMore->setBig();
	_txtLess->setBig();

	if (_rule)
	{
		_base->addResearch(_project);
		if (_rule->needItem() && _rule->destroyItem())
		{
			_base->getStorageItems()->removeItem(_rule->getName(), 1);
		}
	}
	setAssignedScientist();
	_btnMore->onMousePress((ActionHandler)&ResearchInfoState::morePress);
	_btnMore->onMouseRelease((ActionHandler)&ResearchInfoState::moreRelease);
	_btnMore->onMouseClick((ActionHandler)&ResearchInfoState::moreClick, 0);
	_btnLess->onMousePress((ActionHandler)&ResearchInfoState::lessPress);
	_btnLess->onMouseRelease((ActionHandler)&ResearchInfoState::lessRelease);
	_btnLess->onMouseClick((ActionHandler)&ResearchInfoState::lessClick, 0);

	_timerMore = new Timer(250);
	_timerMore->onTimer((StateHandler)&ResearchInfoState::more);
	_timerLess = new Timer(250);
	_timerLess->onTimer((StateHandler)&ResearchInfoState::less);

	_btnOk->onMouseClick((ActionHandler)&ResearchInfoState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ResearchInfoState::btnOkClick, Options::keyOk);
	if (_rule)
	{
		_btnOk->setText(tr("STR_START_PROJECT"));
		_btnCancel->setText(tr("STR_CANCEL_UC"));
		_btnCancel->onKeyboardPress((ActionHandler)&ResearchInfoState::btnCancelClick, Options::keyCancel);
	}
	else
	{
		_btnOk->setText(tr("STR_OK"));
		_btnCancel->setText(tr("STR_CANCEL_PROJECT"));
		_btnOk->onKeyboardPress((ActionHandler)&ResearchInfoState::btnOkClick, Options::keyCancel);
	}
	_btnCancel->onMouseClick((ActionHandler)&ResearchInfoState::btnCancelClick);
}

/**
 * Frees up memory that's not automatically cleaned on exit
 */
ResearchInfoState::~ResearchInfoState()
{
	delete _timerLess;
	delete _timerMore;
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ResearchInfoState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Returns to the previous screen, removing the current project from the active
 * research list.
 * @param action Pointer to an action.
 */
void ResearchInfoState::btnCancelClick(Action *)
{
	_base->removeResearch(_project);
	_game->popState();
}

/**
 * Updates count of assigned/free scientists and available lab space.
 */
void ResearchInfoState::setAssignedScientist()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	++_agentProjectRevision;
#endif
	_txtAvailableScientist->setText(tr("STR_SCIENTISTS_AVAILABLE_UC").arg(_base->getAvailableScientists()));
	_txtAvailableSpace->setText(tr("STR_LABORATORY_SPACE_AVAILABLE_UC").arg(_base->getFreeLaboratories()));
	_txtAllocatedScientist->setText(tr("STR_SCIENTISTS_ALLOCATED").arg(_project->getAssigned()));
}

/**
 * Increases or decreases the scientists according the mouse-wheel used.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::handleWheel(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP) moreByValue(Options::changeValueByMouseWheel);
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN) lessByValue(Options::changeValueByMouseWheel);
}

/**
 * Starts the timeMore timer.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::morePress(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT) _timerMore->start();
}

/**
 * Stops the timeMore timer.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::moreRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerMore->setInterval(250);
		_timerMore->stop();
	}
}

/**
 * Allocates scientists to the current project;
 * one scientist on left-click, all scientists on right-click.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::moreClick(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
		moreByValue(INT_MAX);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
		moreByValue(1);
}

/**
 * Starts the timeLess timer.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::lessPress(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT) _timerLess->start();
}

/**
 * Stops the timeLess timer.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::lessRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerLess->setInterval(250);
		_timerLess->stop();
	}
}

/**
 * Removes scientists from the current project;
 * one scientist on left-click, all scientists on right-click.
 * @param action Pointer to an Action.
 */
void ResearchInfoState::lessClick(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
		lessByValue(INT_MAX);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
		lessByValue(1);
}

/**
 * Adds one scientist to the project if possible.
 */
void ResearchInfoState::more()
{
	_timerMore->setInterval(50);
	moreByValue(1);
}

/**
 * Adds the given number of scientists to the project if possible.
 * @param change Number of scientists to add.
 */
void ResearchInfoState::moreByValue(int change)
{
	if (0 >= change) return;
	int freeScientist = _base->getAvailableScientists();
	int freeSpaceLab = _base->getFreeLaboratories();
	if (freeScientist > 0 && freeSpaceLab > 0)
	{
		change = std::min(std::min(freeScientist, freeSpaceLab), change);
		_project->setAssigned(_project->getAssigned()+change);
		_base->setScientists(_base->getScientists()-change);
		setAssignedScientist();
	}
}

/**
 * Removes one scientist from the project if possible.
 */
void ResearchInfoState::less()
{
	_timerLess->setInterval(50);
	lessByValue(1);
}

/**
 * Removes the given number of scientists from the project if possible.
 * @param change Number of scientists to subtract.
 */
void ResearchInfoState::lessByValue(int change)
{
	if (0 >= change) return;
	int assigned = _project->getAssigned();
	if (assigned > 0)
	{
		change = std::min(assigned, change);
		_project->setAssigned(assigned-change);
		_base->setScientists(_base->getScientists()+change);
		setAssignedScientist();
	}
}

/**
 * Runs state functionality every cycle (used to update the timer).
 */
void ResearchInfoState::think()
{
	State::think();

	_timerLess->think (this, 0);
	_timerMore->think (this, 0);
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ResearchInfoState::agentUiState(Observer::AgentUiState &out) const
{
    if (!_game->isState(const_cast<ResearchInfoState *>(this)) || _modal || !Observer::agentPaintHealthy()) return false;
    if (!_base || !_project) return false;
    // Bind only identity and membership privately; all public values come from
    // the complete painted widgets, never the underlying project or rule.
    const std::vector<ResearchProject *> &projects = _base->getResearch();
    if (std::find(projects.begin(), projects.end(), _project) == projects.end()) return false;
    out = Observer::AgentUiState();
    out.screen = "research_info";
    out.actor = _base;
    out.save = _project;
    out.actorRevision = _agentProjectRevision;
    out.widgets.push_back(std::make_pair(_rule, 0));
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
    const std::string title = text(_txtTitle), available = text(_txtAvailableScientist),
        space = text(_txtAvailableSpace), allocated = text(_txtAllocatedScientist);
    std::string buttons = "[", firstDirection;
    const auto appendButton = [&](const char *id, const std::string &label)
    {
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + Observer::agentJsonString(id) + ",\"label\":" + Observer::agentJsonString(label) + "}";
    };
    const auto direction = [&](const ArrowButton *arrow, const Text *label, const char *id) -> bool
    {
        if (!Observer::agentWidgetUncovered(arrow, painted) || !Observer::agentWidgetUncovered(label, painted) ||
            Observer::agentTextJson(label) == "null" || label->getText().empty()) return false;
        const std::size_t before = out.regions.size();
        if (!include(label) || !include(arrow)) { out.regions.resize(before); return false; }
        appendButton(id, label->getText());
        if (firstDirection.empty()) firstDirection = label->getText();
        return true;
    };
    out.context.push_back(direction(_btnMore, _txtMore, "increase") ? 1 : 0);
    out.context.push_back(direction(_btnLess, _txtLess, "decrease") ? 1 : 0);
    if (title != "null" && allocated != "null" && !firstDirection.empty())
        out.actions.push_back(Observer::AgentUiAction("adjust_scientists", firstDirection, "visible_text", "staff_direction"));
    const auto button = [&](const TextButton *widget, const char *id, const char *action)
    {
        if (!Observer::agentWidgetUncovered(widget, painted)) return;
        const std::string label = Observer::agentButtonLabel(widget);
        if (label.empty() || label.size() > 1024 || !include(widget)) return;
        appendButton(id, label);
        if (title != "null") out.actions.push_back(Observer::AgentUiAction(action, label, "visible_text"));
    };
    button(_btnOk, "accept", "accept_research");
    button(_btnCancel, "cancel", "cancel_research_project");
    out.visible = "{\"title\":" + title + ",\"available\":" + available + ",\"space\":" + space +
        ",\"allocated\":" + allocated + ",\"buttons\":" + buttons + "]}";
    return !out.regions.empty() && Observer::agentRegionsDisjoint(out) && out.visible.size() <= 32768;
}

bool ResearchInfoState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "accept_research") { btnOkClick(nullptr); return true; }
    if (request.action == "cancel_research_project") { btnCancelClick(nullptr); return true; }
    if (request.action == "adjust_scientists" && current.context.size() == 2)
    {
        // Deliberately one ordinary increment only: no timer, hold, wheel or
        // right-click behavior is synthesized by this semantic operation.
        if (request.choice == "increase" && current.context[0]) { moreByValue(1); return true; }
        if (request.choice == "decrease" && current.context[1]) { lessByValue(1); return true; }
    }
    return false;
}
#endif

}
