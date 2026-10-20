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
#include "MissionDetectedState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#endif
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "GeoscapeState.h"
#include "Globe.h"
#include "../Savegame/MissionSite.h"
#include "../Engine/Options.h"
#include "InterceptState.h"
#include "../Mod/AlienDeployment.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Mission Detected window.
 * @param game Pointer to the core game.
 * @param mission Pointer to the respective Mission Site.
 * @param state Pointer to the Geoscape.
 */
MissionDetectedState::MissionDetectedState(MissionSite *mission, GeoscapeState *state) : _mission(mission), _state(state)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 256, 200, 0, 0, POPUP_BOTH);
	_btnIntercept = new TextButton(200, 16, 28, 130);
	_btnCenter = new TextButton(200, 16, 28, 150);
	_btnCancel = new TextButton(200, 16, 28, 170);
	_txtTitle = new Text(246, 32, 5, 48);
	_txtCity = new Text(246, 17, 5, 80);

	// Set palette
	setInterface("terrorSite");

	add(_window, "window", "terrorSite");
	add(_btnIntercept, "button", "terrorSite");
	add(_btnCenter, "button", "terrorSite");
	add(_btnCancel, "button", "terrorSite");
	add(_txtTitle, "text", "terrorSite");
	add(_txtCity, "text", "terrorSite");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface(mission->getDeployment()->getAlertBackground()));

	_btnIntercept->setText(tr("STR_INTERCEPT"));
	_btnIntercept->onMouseClick((ActionHandler)&MissionDetectedState::btnInterceptClick);

	_btnCenter->setText(tr("STR_CENTER_ON_SITE_TIME_5_SECONDS"));
	_btnCenter->onMouseClick((ActionHandler)&MissionDetectedState::btnCenterClick);

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&MissionDetectedState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&MissionDetectedState::btnCancelClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setWordWrap(true);
	_txtTitle->setText(tr(mission->getDeployment()->getAlertMessage()));

	_txtCity->setBig();
	_txtCity->setAlign(ALIGN_CENTER);
	_txtCity->setText(tr(mission->getCity()));
}

/**
 *
 */
MissionDetectedState::~MissionDetectedState()
{

}

/**
 * Picks a craft to intercept the mission site.
 * @param action Pointer to an action.
 */
void MissionDetectedState::btnInterceptClick(Action *)
{
	_state->timerReset();
	_state->getGlobe()->center(_mission->getLongitude(), _mission->getLatitude());
	_game->pushState(new InterceptState(_state->getGlobe(), 0, _mission));
}

/**
 * Centers on the mission site and returns to the previous screen.
 * @param action Pointer to an action.
 */
void MissionDetectedState::btnCenterClick(Action *)
{
	_state->timerReset();
	_state->getGlobe()->center(_mission->getLongitude(), _mission->getLatitude());
	_game->popState();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void MissionDetectedState::btnCancelClick(Action *)
{
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool MissionDetectedState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> MissionDetectedState::agentPaintRevisions() const
{
    return {_txtTitle->agentTextRevision(),
        _txtCity->agentTextRevision(),
        _btnIntercept->agentLabelRevision(),
        _btnCenter->agentLabelRevision(),
        _btnCancel->agentLabelRevision()};
}

void MissionDetectedState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool MissionDetectedState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<MissionDetectedState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "mission_alert";
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
    const auto city = text(_txtCity);
    button(_btnIntercept, "intercept", nullptr);
    button(_btnCenter, "center", nullptr);
    button(_btnCancel, "cancel", "dismiss_mission_alert");
    buttons += "]";
    out.visible = "{\"title\":" + title + ",\"city\":" + city + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    // Match the retained-event ring's conservative non-ASCII byte estimate,
    // leaving room for its envelope without changing shared transport policy.
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool MissionDetectedState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "dismiss_mission_alert") { btnCancelClick(nullptr); return true; }
    return false;
}
#endif

}
