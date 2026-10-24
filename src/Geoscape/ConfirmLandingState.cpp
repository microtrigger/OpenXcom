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
#include "ConfirmLandingState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Target.h"
#include "../Savegame/Ufo.h"
#include "../Savegame/Base.h"
#include "../Savegame/MissionSite.h"
#include "../Savegame/AlienBase.h"
#include "../Battlescape/BriefingState.h"
#include "../Battlescape/BattlescapeGenerator.h"
#include "../Engine/Exception.h"
#include "../Engine/Options.h"
#include "../Mod/AlienDeployment.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Confirm Landing window.
 * @param game Pointer to the core game.
 * @param craft Pointer to the craft to confirm.
 * @param texture Texture of the landing site.
 * @param shade Shade of the landing site.
 */
ConfirmLandingState::ConfirmLandingState(Craft *craft, Texture *texture, int shade) : _craft(craft), _texture(texture), _shade(shade)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 216, 160, 20, 20, POPUP_BOTH);
	_btnYes = new TextButton(80, 20, 40, 150);
	_btnNo = new TextButton(80, 20, 136, 150);
	_txtMessage = new Text(206, 80, 25, 40);
	_txtBegin = new Text(206, 17, 25, 130);

	// Set palette
	setInterface("confirmLanding");

	add(_window, "window", "confirmLanding");
	add(_btnYes, "button", "confirmLanding");
	add(_btnNo, "button", "confirmLanding");
	add(_txtMessage, "text", "confirmLanding");
	add(_txtBegin, "text", "confirmLanding");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK15.SCR"));

	_btnYes->setText(tr("STR_YES"));
	_btnYes->onMouseClick((ActionHandler)&ConfirmLandingState::btnYesClick);
	_btnYes->onKeyboardPress((ActionHandler)&ConfirmLandingState::btnYesClick, Options::keyOk);

	_btnNo->setText(tr("STR_NO"));
	_btnNo->onMouseClick((ActionHandler)&ConfirmLandingState::btnNoClick);
	_btnNo->onKeyboardPress((ActionHandler)&ConfirmLandingState::btnNoClick, Options::keyCancel);

	_txtMessage->setBig();
	_txtMessage->setAlign(ALIGN_CENTER);
	_txtMessage->setWordWrap(true);
	_txtMessage->setText(tr("STR_CRAFT_READY_TO_LAND_NEAR_DESTINATION")
						 .arg(_craft->getName(_game->getLanguage()))
						 .arg(_craft->getDestination()->getName(_game->getLanguage())));

	_txtBegin->setBig();
	_txtBegin->setAlign(ALIGN_CENTER);
	std::ostringstream ss;
	ss << Unicode::TOK_COLOR_FLIP << tr("STR_BEGIN_MISSION");
	_txtBegin->setText(ss.str());
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentLandingDestination = _craft->getDestination();
    if (_agentLandingDestination) _agentLandingLifetime = _agentLandingDestination->agentLifetime();
    for (Surface *widget : std::vector<Surface *>{_window, _txtMessage, _txtBegin, _btnYes, _btnNo})
        widget->agentTrackPresentation();
#endif
}

/**
 *
 */
ConfirmLandingState::~ConfirmLandingState()
{

}

/*
 * Make sure we aren't returning to base.
 */
void ConfirmLandingState::init()
{
	State::init();
	Base* b = dynamic_cast<Base*>(_craft->getDestination());
	if (b == _craft->getBase())
		_game->popState();
}

/**
 * Enters the mission.
 * @param action Pointer to an action.
 */
void ConfirmLandingState::btnYesClick(Action *)
{
	_game->popState();
	Ufo* u = dynamic_cast<Ufo*>(_craft->getDestination());
	MissionSite* m = dynamic_cast<MissionSite*>(_craft->getDestination());
	AlienBase* b = dynamic_cast<AlienBase*>(_craft->getDestination());

	SavedBattleGame *bgame = new SavedBattleGame();
	_game->getSavedGame()->setBattleGame(bgame);
	BattlescapeGenerator bgen(_game);
	bgen.setWorldTexture(_texture);
	bgen.setWorldShade(_shade);
	bgen.setCraft(_craft);
	if (u != 0)
	{
		if (u->getStatus() == Ufo::CRASHED)
			bgame->setMissionType("STR_UFO_CRASH_RECOVERY");
		else
			bgame->setMissionType("STR_UFO_GROUND_ASSAULT");
		bgen.setUfo(u);
		bgen.setAlienRace(u->getAlienRace());
	}
	else if (m != 0)
	{
		bgame->setMissionType(m->getDeployment()->getType());
		bgen.setMissionSite(m);
		bgen.setAlienRace(m->getAlienRace());
	}
	else if (b != 0)
	{
		bgame->setMissionType(b->getDeployment()->getType());
		bgen.setAlienBase(b);
		bgen.setAlienRace(b->getAlienRace());
		bgen.setWorldTexture(0);
	}
	else
	{
		throw Exception("No mission available!");
	}
	bgen.run();
	_game->pushState(new BriefingState(_craft));
}

/**
 * Returns the craft to base and closes the window.
 * @param action Pointer to an action.
 */
void ConfirmLandingState::btnNoClick(Action *)
{
	_craft->returnToBase();
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ConfirmLandingState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> ConfirmLandingState::agentPaintRevisions() const
{
    return {_window->agentPresentationRevision(),
        _txtMessage->agentPresentationRevision(), _txtMessage->agentTextRevision(),
        _txtBegin->agentPresentationRevision(), _txtBegin->agentTextRevision(),
        _btnYes->agentPresentationRevision(), _btnYes->agentLabelRevision(),
        _btnNo->agentPresentationRevision(), _btnNo->agentLabelRevision()};
}

void ConfirmLandingState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool ConfirmLandingState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<ConfirmLandingState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "landing_confirmation";
    out.actor = _craft; // Private callback target identity, never serialized.
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
    const auto message = text(_txtMessage);
    const auto prompt = text(_txtBegin);
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
    // A replacement native destination must not reuse the displayed landing choice.
    // Identity only: no mission eligibility or hidden target attributes are inspected.
    if (!_agentLandingLifetime.expired() && _agentLandingDestination && _craft->getDestination() == _agentLandingDestination)
    {
        button(_btnYes, "yes", "accept_landing_begin_mission");
        if (out.offers("accept_landing_begin_mission"))
            out.targetLifetimes.push_back(_agentLandingLifetime);
    }
    button(_btnNo, "no", "decline_landing_return_to_base");
    buttons += "]";
    out.visible = "{\"message\":" + message + ",\"prompt\":" + prompt + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool ConfirmLandingState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "accept_landing_begin_mission") { btnYesClick(nullptr); return true; }
    if (request.action == "decline_landing_return_to_base") { btnNoClick(nullptr); return true; }
    return false;
}
#endif


}
