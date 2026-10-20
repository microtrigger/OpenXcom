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
#include "ResearchRequiredState.h"
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
#include "../Mod/RuleItem.h"
#include "../Engine/Options.h"

namespace OpenXcom
{
/**
 * Initializes all the elements in the Research Required screen.
 * @param game Pointer to the core game.
 * @param item Pointer to the researched weapon.
 */
ResearchRequiredState::ResearchRequiredState(RuleItem *item)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 288, 180, 16, 10);
	_btnOk = new TextButton(160, 18, 80, 150);
	_txtTitle = new Text(288, 80, 16, 50);

	// Set palette
	setInterface("geoResearchRequired");

	add(_window, "window", "geoResearchRequired");
	add(_btnOk, "button", "geoResearchRequired");
	add(_txtTitle, "text1", "geoResearchRequired");

	centerAllSurfaces();

	std::string weapon = item->getType();
	std::string clip = item->getCompatibleAmmo()->front();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK05.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ResearchRequiredState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ResearchRequiredState::btnOkClick, Options::keyCancel);
	_btnOk->onKeyboardPress((ActionHandler)&ResearchRequiredState::btnOkClick, Options::keyOk);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setVerticalAlign(ALIGN_MIDDLE);
	_txtTitle->setText(tr("STR_YOU_NEED_TO_RESEARCH_ITEM_TO_PRODUCE_ITEM")
					   .arg(tr(clip))
					   .arg(tr(weapon)));
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ResearchRequiredState::btnOkClick(Action *)
{
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ResearchRequiredState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> ResearchRequiredState::agentPaintRevisions() const
{
    return {_txtTitle->agentTextRevision(),
        _btnOk->agentLabelRevision()};
}

void ResearchRequiredState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool ResearchRequiredState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<ResearchRequiredState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "research_required";
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
    const auto message = text(_txtTitle);
    button(_btnOk, "ok", "dismiss_research_required");
    buttons += "]";
    out.visible = "{\"message\":" + message + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    // Match the retained-event ring's conservative non-ASCII byte estimate,
    // leaving room for its envelope without changing shared transport policy.
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool ResearchRequiredState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "dismiss_research_required") { btnOkClick(nullptr); return true; }
    return false;
}
#endif

}
