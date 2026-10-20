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
#include "ResearchCompleteState.h"
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
#include "../Mod/RuleResearch.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Engine/Options.h"

namespace OpenXcom
{
/**
 * Initializes all the elements in the EndResearch screen.
 * @param game Pointer to the core game.
 * @param newResearch Pointer to the completed research (or 0, if the ufopedia article shouldn't popup again).
 * @param bonus Pointer to bonus unlocked research.
 * @param research Pointer to the research project.
 */
ResearchCompleteState::ResearchCompleteState(const RuleResearch *newResearch, const RuleResearch *bonus, const RuleResearch *research) : _research(newResearch), _bonus(bonus)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 230, 140, 45, 30, POPUP_BOTH);
	_btnOk = new TextButton(80, 16, 64, 146);
	_btnReport = new TextButton(80, 16, 176, 146);
	_txtTitle = new Text(230, 17, 45, 70);
	_txtResearch = new Text(230, 32, 45, 96);

	// Set palette
	setInterface("geoResearchComplete");

	add(_window, "window", "geoResearchComplete");
	add(_btnOk, "button", "geoResearchComplete");
	add(_btnReport, "button", "geoResearchComplete");
	add(_txtTitle, "text1", "geoResearchComplete");
	add(_txtResearch, "text2", "geoResearchComplete");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK05.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ResearchCompleteState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ResearchCompleteState::btnOkClick, Options::keyCancel);

	_btnReport->setText(tr("STR_VIEW_REPORTS"));
	_btnReport->onMouseClick((ActionHandler)&ResearchCompleteState::btnReportClick);
	_btnReport->onKeyboardPress((ActionHandler)&ResearchCompleteState::btnReportClick, Options::keyOk);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_RESEARCH_COMPLETED"));

	_txtResearch->setAlign(ALIGN_CENTER);
	_txtResearch->setBig();
	_txtResearch->setWordWrap(true);
	if (research)
	{
		_txtResearch->setText(tr(research->getName()));
	}
}

/**
 * return to the previous screen
 * @param action Pointer to an action.
 */
void ResearchCompleteState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * open the Ufopaedia to the entry about the Research.
 * @param action Pointer to an action.
 */
void ResearchCompleteState::btnReportClick(Action *)
{
	_game->popState();
	std::string name;
	std::string bonusName;
	if (_bonus)
	{
		if (_bonus->getLookup().empty())
			bonusName = _bonus->getName();
		else
			bonusName = _bonus->getLookup();
		Ufopaedia::openArticle(_game, bonusName);
	}
	if (_research)
	{
		if (_research->getLookup().empty())
			name = _research->getName();
		else
			name = _research->getLookup();
		Ufopaedia::openArticle(_game, name);
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ResearchCompleteState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> ResearchCompleteState::agentPaintRevisions() const
{
    return {_txtTitle->agentTextRevision(),
        _txtResearch->agentTextRevision(),
        _btnOk->agentLabelRevision(),
        _btnReport->agentLabelRevision()};
}

void ResearchCompleteState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool ResearchCompleteState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<ResearchCompleteState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "research_complete";
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
    const auto topic = text(_txtResearch);
    button(_btnOk, "ok", "dismiss_research_complete");
    button(_btnReport, "reports", "view_research_reports");
    buttons += "]";
    out.visible = "{\"title\":" + title + ",\"topic\":" + topic + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    // Match the retained-event ring's conservative non-ASCII byte estimate,
    // leaving room for its envelope without changing shared transport policy.
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool ResearchCompleteState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "dismiss_research_complete") { btnOkClick(nullptr); return true; }
    if (request.action == "view_research_reports") { btnReportClick(nullptr); return true; }
    return false;
}
#endif

}
