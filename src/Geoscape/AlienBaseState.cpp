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
#include "AlienBaseState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "GeoscapeState.h"
#include "Globe.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Region.h"
#include "../Savegame/Country.h"
#include "../Mod/RuleRegion.h"
#include "../Mod/RuleCountry.h"
#include "../Savegame/AlienBase.h"
#include "../Engine/Options.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Aliens Base discovered window.
 * @param game Pointer to the core game.
 * @param base Pointer to the alien base to get info from.
 * @param state Pointer to the Geoscape.
 */
AlienBaseState::AlienBaseState(AlienBase *base, GeoscapeState *state) : _state(state), _base(base)
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(50, 12, 135, 180);
	_txtTitle = new Text(308, 60, 6, 60);

	setInterface("alienBase");

	add(_window, "window", "alienBase");
	add(_btnOk, "text", "alienBase");
	add(_txtTitle, "button", "alienBase");

	centerAllSurfaces();


	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&AlienBaseState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&AlienBaseState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&AlienBaseState::btnOkClick, Options::keyCancel);

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setWordWrap(true);

	// Check location of base
	std::string region, country;
	for (std::vector<Country*>::iterator i = _game->getSavedGame()->getCountries()->begin(); i != _game->getSavedGame()->getCountries()->end(); ++i)
	{
		if ((*i)->getRules()->insideCountry(_base->getLongitude(), _base->getLatitude()))
		{
			country = tr((*i)->getRules()->getType());
			break;
		}
	}
	for (std::vector<Region*>::iterator i = _game->getSavedGame()->getRegions()->begin(); i != _game->getSavedGame()->getRegions()->end(); ++i)
	{
		if ((*i)->getRules()->insideRegion(_base->getLongitude(), _base->getLatitude()))
		{
			region = tr((*i)->getRules()->getType());
			break;
		}
	}
	std::string location;
	if (!country.empty())
	{
		location = tr("STR_COUNTRIES_COMMA").arg(country).arg(region);
	}
	else if (!region.empty())
	{
		location = region;
	}
	else
	{
		location = tr("STR_UNKNOWN");
	}
	_txtTitle->setText(tr("STR_XCOM_AGENTS_HAVE_LOCATED_AN_ALIEN_BASE_IN_REGION").arg(location));
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *widget : std::vector<Surface *>{_window, _txtTitle, _btnOk})
        widget->agentTrackPresentation();
#endif
}

/**
 *
 */
AlienBaseState::~AlienBaseState()
{

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void AlienBaseState::btnOkClick(Action *)
{
	_state->timerReset();
	_state->getGlobe()->center(_base->getLongitude(), _base->getLatitude());
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool AlienBaseState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> AlienBaseState::agentPaintRevisions() const
{
    return {_window->agentPresentationRevision(),
        _txtTitle->agentPresentationRevision(), _txtTitle->agentTextRevision(),
        _btnOk->agentPresentationRevision(), _btnOk->agentLabelRevision()};
}

void AlienBaseState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool AlienBaseState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<AlienBaseState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "alien_base_discovered";
    out.actor = _state; // Private native callback ownership, never serialized.
    out.context.push_back(reinterpret_cast<std::uintptr_t>(_base));
    const Surface *destination = _game->getScreen()->getSurface();
    if (!_window->agentPresentationPaintedTo(destination)) return false;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const auto include = [&](const Surface *widget, bool observationOnly) -> bool {
        if (!widget->agentPresentationPaintedTo(destination) || !agentWidgetVisible(widget)) return false;
        AgentUiState part;
        part.regions.push_back({widget->getX(), widget->getY(), widget->getWidth(), widget->getHeight()});
        // Allocation geometry only: do not require glyph-ink evidence for text.
        bool found = false;
        for (const Surface *later : painted)
        {
            if (later == widget) { found = true; continue; }
            if (!found || !later || !later->getVisible()) continue;
            const auto &rect = part.regions.front();
            if (rect.x < later->getX() + later->getWidth() && rect.x + rect.width > later->getX() &&
                rect.y < later->getY() + later->getHeight() && rect.y + rect.height > later->getY()) return false;
        }
        if (!found || !(observationOnly ? _game->agentNonCursorOverlayClear(part) : _game->agentOverlayClear(part))) return false;
        if (observationOnly) return true;
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before);
        return false;
    };
    // These are current active UI captions, not a glyph-pixel readability test.
    // Hidden, geometrically absent, occluded or stale widgets contribute no value.
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        out.context.insert(out.context.end(), {
            static_cast<std::uint64_t>(widget->getX()), static_cast<std::uint64_t>(widget->getY()),
            static_cast<std::uint64_t>(widget->getWidth()), static_cast<std::uint64_t>(widget->getHeight())});
        // Active logical captions: glyph/ink completeness and pointer overlap
        // are not text admission criteria. Resource bound is not a layout test.
        if (widget->getText().size() > 1024 || !include(widget, true)) return "null";
        return agentJsonString(widget->getText());
    };
    const auto message = text(_txtTitle);
    std::string buttons = "[";
    const auto button = [&](TextButton *widget, const char *id, const char *action) {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentLabelRevision());
        const std::string label = widget->getText();
        if (label.empty() || label.size() > 1024 || !include(widget, false)) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back(action, label, "visible_text");
    };
    button(_btnOk, "ok", "acknowledge_alien_base_discovery");
    buttons += "]";
    out.visible = "{\"message\":" + message + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool AlienBaseState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "acknowledge_alien_base_discovery") { btnOkClick(nullptr); return true; }
    return false;
}
#endif

}
