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
#include "GeoscapeCraftState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Mod/RuleCraft.h"
#include "../Savegame/CraftWeapon.h"
#include "../Mod/RuleCraftWeapon.h"
#include "../Savegame/Target.h"
#include "../Savegame/Ufo.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Waypoint.h"
#include "SelectDestinationState.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "Globe.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Geoscape Craft window.
 * @param game Pointer to the core game.
 * @param craft Pointer to the craft to display.
 * @param globe Pointer to the Geoscape globe.
 * @param waypoint Pointer to the last UFO position (if redirecting the craft).
 */
GeoscapeCraftState::GeoscapeCraftState(Craft *craft, Globe *globe, Waypoint *waypoint) : _craft(craft), _globe(globe), _waypoint(waypoint)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 240, 184, 8, 8, POPUP_BOTH);
	_btnBase = new TextButton(212, 12, 22, 124);
	_btnTarget = new TextButton(212, 12, 22, 140);
	_btnPatrol = new TextButton(212, 12, 22, 156);
	_btnCancel = new TextButton(212, 12, 22, 172);
	_txtTitle = new Text(210, 17, 32, 20);
	_txtStatus = new Text(210, 17, 32, 36);
	_txtBase = new Text(210, 9, 32, 52);
	_txtSpeed = new Text(210, 9, 32, 60);
	_txtMaxSpeed = new Text(210, 9, 32, 68);
	_txtAltitude = new Text(210, 9, 32, 76);
	_txtFuel = new Text(130, 9, 32, 84);
	_txtDamage = new Text(80, 9, 164, 84);
	_txtW1Name = new Text(130, 9, 32, 92);
	_txtW1Ammo = new Text(80, 9, 164, 92);
	_txtW2Name = new Text(130, 9, 32, 100);
	_txtW2Ammo = new Text(80, 9, 164, 100);
	_txtRedirect = new Text(230, 17, 13, 108);
	_txtSoldier = new Text(80, 9, 164, 68);
	_txtHWP = new Text(80, 9, 164, 76);

	// Set palette
	setInterface("geoCraft");

	add(_window, "window", "geoCraft");
	add(_btnBase, "button", "geoCraft");
	add(_btnTarget, "button", "geoCraft");
	add(_btnPatrol, "button", "geoCraft");
	add(_btnCancel, "button", "geoCraft");
	add(_txtTitle, "text1", "geoCraft");
	add(_txtStatus, "text1", "geoCraft");
	add(_txtBase, "text3", "geoCraft");
	add(_txtSpeed, "text3", "geoCraft");
	add(_txtMaxSpeed, "text3", "geoCraft");
	add(_txtAltitude, "text3", "geoCraft");
	add(_txtFuel, "text3", "geoCraft");
	add(_txtDamage, "text3", "geoCraft");
	add(_txtW1Name, "text3", "geoCraft");
	add(_txtW1Ammo, "text3", "geoCraft");
	add(_txtW2Name, "text3", "geoCraft");
	add(_txtW2Ammo, "text3", "geoCraft");
	add(_txtRedirect, "text3", "geoCraft");
	add(_txtSoldier, "text3", "geoCraft");
	add(_txtHWP, "text3", "geoCraft");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK12.SCR"));

	_btnBase->setText(tr("STR_RETURN_TO_BASE"));
	_btnBase->onMouseClick((ActionHandler)&GeoscapeCraftState::btnBaseClick);

	_btnTarget->setText(tr("STR_SELECT_NEW_TARGET"));
	_btnTarget->onMouseClick((ActionHandler)&GeoscapeCraftState::btnTargetClick);

	_btnPatrol->setText(tr("STR_PATROL"));
	_btnPatrol->onMouseClick((ActionHandler)&GeoscapeCraftState::btnPatrolClick);

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&GeoscapeCraftState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&GeoscapeCraftState::btnCancelClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setText(_craft->getName(_game->getLanguage()));

	_txtStatus->setWordWrap(true);
	std::string status;
	if (_waypoint != 0)
	{
		status = tr("STR_INTERCEPTING_UFO").arg(_waypoint->getId());
	}
	else if (_craft->getLowFuel())
	{
		status = tr("STR_LOW_FUEL_RETURNING_TO_BASE");
	}
	else if (_craft->getMissionComplete())
	{
		status = tr("STR_MISSION_COMPLETE_RETURNING_TO_BASE");
	}
	else if (_craft->getDestination() == 0)
	{
		status = tr("STR_PATROLLING");
	}
	else if (_craft->getDestination() == (Target*)_craft->getBase())
	{
		status = tr("STR_RETURNING_TO_BASE");
	}
	else
	{
		Ufo *u = dynamic_cast<Ufo*>(_craft->getDestination());
		if (u != 0)
		{
			if (_craft->isInDogfight())
			{
				status = tr("STR_TAILING_UFO");
			}
			else if (u->getStatus() == Ufo::FLYING)
			{
				status = tr("STR_INTERCEPTING_UFO").arg(u->getId());
			}
			else
			{
				status = tr("STR_DESTINATION_UC_").arg(u->getName(_game->getLanguage()));
			}
		}
		else
		{
			status = tr("STR_DESTINATION_UC_").arg(_craft->getDestination()->getName(_game->getLanguage()));
		}
	}
	_txtStatus->setText(tr("STR_STATUS_").arg(status));

	_txtBase->setText(tr("STR_BASE_UC").arg(_craft->getBase()->getName()));

	int speed = _craft->getSpeed();
	if (_craft->isInDogfight())
	{
		Ufo *ufo = dynamic_cast<Ufo*>(_craft->getDestination());
		if (ufo)
		{
			speed = ufo->getSpeed();
		}
	}
	_txtSpeed->setText(tr("STR_SPEED_").arg(Unicode::formatNumber(speed)));

	_txtMaxSpeed->setText(tr("STR_MAXIMUM_SPEED_UC").arg(Unicode::formatNumber(_craft->getRules()->getMaxSpeed())));

	std::string altitude = _craft->getAltitude() == "STR_GROUND" ? "STR_GROUNDED" : _craft->getAltitude();
	if (_craft->getRules()->isWaterOnly() && !_globe->insideLand(_craft->getLongitude(), _craft->getLatitude()))
	{
		altitude = "STR_AIRBORNE";
	}
	_txtAltitude->setText(tr("STR_ALTITUDE_").arg(tr(altitude)));

	_txtFuel->setText(tr("STR_FUEL").arg(Unicode::formatPercentage(_craft->getFuelPercentage())));

	_txtDamage->setText(tr("STR_DAMAGE_UC_").arg(Unicode::formatPercentage(_craft->getDamagePercentage())));

	if (_craft->getRules()->getWeapons() > 0 && _craft->getWeapons()->at(0) != 0)
	{
		CraftWeapon *w1 = _craft->getWeapons()->at(0);
		_txtW1Name->setText(tr("STR_WEAPON_ONE").arg(tr(w1->getRules()->getType())));
		_txtW1Ammo->setText(tr("STR_ROUNDS_").arg(w1->getAmmo()));
	}
	else
	{
		_txtW1Name->setText(tr("STR_WEAPON_ONE").arg(tr("STR_NONE_UC")));
		_txtW1Ammo->setVisible(false);
	}

	if (_craft->getRules()->getWeapons() > 1 && _craft->getWeapons()->at(1) != 0)
	{
		CraftWeapon *w2 = _craft->getWeapons()->at(1);
		_txtW2Name->setText(tr("STR_WEAPON_TWO").arg(tr(w2->getRules()->getType())));
		_txtW2Ammo->setText(tr("STR_ROUNDS_").arg(w2->getAmmo()));
	}
	else
	{
		_txtW2Name->setText(tr("STR_WEAPON_TWO").arg(tr("STR_NONE_UC")));
		_txtW2Ammo->setVisible(false);
	}

	_txtRedirect->setBig();
	_txtRedirect->setAlign(ALIGN_CENTER);
	_txtRedirect->setText(tr("STR_REDIRECT_CRAFT"));

	std::ostringstream ss11;
	ss11 << tr("STR_SOLDIERS_UC") << ">" << Unicode::TOK_COLOR_FLIP << _craft->getNumSoldiers();
	_txtSoldier->setText(ss11.str());

	std::ostringstream ss12;
	ss12 << tr("STR_HWPS") << ">" << Unicode::TOK_COLOR_FLIP << _craft->getNumVehicles();
	_txtHWP->setText(ss12.str());

	if (_waypoint == 0)
	{
		_txtRedirect->setVisible(false);
	}
	else
	{
		_btnCancel->setText(tr("STR_GO_TO_LAST_KNOWN_UFO_POSITION"));
	}

	if (_craft->getLowFuel() || _craft->getMissionComplete())
	{
		_btnBase->setVisible(false);
		_btnTarget->setVisible(false);
		_btnPatrol->setVisible(false);
	}

	if (_craft->getRules()->getSoldiers() == 0)
		_txtSoldier->setVisible(false);
	if (_craft->getRules()->getVehicles() == 0)
		_txtHWP->setVisible(false);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentWaypoint = _waypoint;
    if (_waypoint) _agentWaypointLifetime = _waypoint->agentLifetime();
    for (Surface *widget : std::vector<Surface *>{_window, _txtTitle, _txtStatus, _txtRedirect, _btnBase, _btnPatrol, _btnTarget, _btnCancel})
        widget->agentTrackPresentation();
#endif
}

/**
 *
 */
GeoscapeCraftState::~GeoscapeCraftState()
{

}

/**
 * Returns the craft back to its base.
 * @param action Pointer to an action.
 */
void GeoscapeCraftState::btnBaseClick(Action *)
{
	_game->popState();
	_craft->returnToBase();
	delete _waypoint;
}

/**
 * Changes the craft's target.
 * @param action Pointer to an action.
 */
void GeoscapeCraftState::btnTargetClick(Action *)
{
	_game->popState();
	_game->pushState(new SelectDestinationState(_craft, _globe));
	delete _waypoint;
}

/**
 * Sets the craft to patrol the current location.
 * @param action Pointer to an action.
 */
void GeoscapeCraftState::btnPatrolClick(Action *)
{
	_game->popState();
	_craft->setDestination(0);
	delete _waypoint;
}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void GeoscapeCraftState::btnCancelClick(Action *)
{
	// Go to the last known UFO position
	if (_waypoint != 0)
	{
		_waypoint->setId(_game->getSavedGame()->getId("STR_WAY_POINT"));
		_game->getSavedGame()->getWaypoints()->push_back(_waypoint);
		_craft->setDestination(_waypoint);
	}
	// Cancel
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool GeoscapeCraftState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> GeoscapeCraftState::agentPaintRevisions() const
{
    return {_window->agentPresentationRevision(),
        _txtTitle->agentPresentationRevision(), _txtTitle->agentTextRevision(),
        _txtStatus->agentPresentationRevision(), _txtStatus->agentTextRevision(),
        _txtRedirect->agentPresentationRevision(), _txtRedirect->agentTextRevision(),
        _btnBase->agentPresentationRevision(), _btnBase->agentLabelRevision(),
        _btnPatrol->agentPresentationRevision(), _btnPatrol->agentLabelRevision(),
        _btnTarget->agentPresentationRevision(), _btnTarget->agentLabelRevision(),
        _btnCancel->agentPresentationRevision(), _btnCancel->agentLabelRevision()};
}

void GeoscapeCraftState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool GeoscapeCraftState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    // Pair the visible native mode with its constructor-owned callback target.
    // No target coordinates or eligibility are inspected or serialized.
    if (_waypoint != _agentWaypoint) return false;
    const bool redirect = _waypoint != nullptr;
    if (redirect && _agentWaypointLifetime.expired()) return false;
    if (_txtRedirect->getVisible() != redirect ||
        _btnCancel->getText() != std::string(_game->getLanguage()->getString(
            redirect ? "STR_GO_TO_LAST_KNOWN_UFO_POSITION" : "STR_CANCEL_UC"))) return false;
    if (redirect && _txtRedirect->getText() != std::string(_game->getLanguage()->getString("STR_REDIRECT_CRAFT"))) return false;
    if (!_game->isState(const_cast<GeoscapeCraftState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "geoscape_craft_navigation";
    if (redirect) out.targetLifetimes.push_back(_agentWaypointLifetime);
    out.actor = _globe; // Private callback target identity, never serialized.
    out.context.push_back(reinterpret_cast<std::uintptr_t>(_craft));
    out.context.push_back(reinterpret_cast<std::uintptr_t>(_waypoint));
    const Surface *destination = _game->getScreen()->getSurface();
    if (!_window->agentPresentationPaintedTo(destination)) return false;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const auto include = [&](const Surface *widget, bool observationOnly) -> bool {
        if (!widget->agentPresentationPaintedTo(destination) || !agentWidgetVisible(widget)) return false;
        AgentUiState part;
        part.regions.push_back({widget->getX(), widget->getY(), widget->getWidth(), widget->getHeight()});
        // Native REDIRECT text allocation covers the first row of the Base
        // button. Admit only the remaining actual unobscured hitbox; keep the
        // text occluder's full rectangle (no ink/transparency inference).
        if (!observationOnly && redirect && widget == _btnBase &&
            widget->getX() == 22 && widget->getY() == 124 &&
            widget->getWidth() == 212 && widget->getHeight() == 12 &&
            _txtRedirect->getX() == 13 && _txtRedirect->getY() == 108 &&
            _txtRedirect->getWidth() == 230 && _txtRedirect->getHeight() == 17)
        {
            ++part.regions.front().y;
            --part.regions.front().height;
        }
        // Both native caption allocations are 17px with a shared spare row.
        // Use their fixed 16px structural strip, not the selected font: Text
        // may auto-switch font according to content. Never inspect fitting,
        // glyph ink, line count or character completeness for caption admission.
        if (observationOnly && widget->getHeight() == 17)
            part.regions.front().height = 16;
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
    const auto title = text(_txtTitle), message = text(_txtStatus);
    const auto redirectCaption = redirect ? text(_txtRedirect) : std::string("null");
    if (redirect && redirectCaption == "null") return false;
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
    if (redirect)
    {
        button(_btnBase, "base", "return_craft_to_base");
        button(_btnPatrol, "patrol", "patrol_craft_current_position");
    }
    button(_btnTarget, "target", "open_craft_destination");
    if (redirect) button(_btnCancel, "last_known", "go_to_craft_last_known_position");
    else button(_btnCancel, "cancel", "cancel_craft_navigation");
    buttons += "]";
    out.visible = "{\"title\":" + title + ",\"status\":" + message + ",\"redirect\":" + redirectCaption + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool GeoscapeCraftState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "open_craft_destination") { btnTargetClick(nullptr); return true; }
    if (request.action == "return_craft_to_base") { btnBaseClick(nullptr); return true; }
    if (request.action == "patrol_craft_current_position") { btnPatrolClick(nullptr); return true; }
    if (request.action == "go_to_craft_last_known_position") { btnCancelClick(nullptr); return true; }
    if (request.action == "cancel_craft_navigation") { btnCancelClick(nullptr); return true; }
    return false;
}
#endif

}
