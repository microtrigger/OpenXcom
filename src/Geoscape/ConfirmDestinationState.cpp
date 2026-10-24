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
#include "ConfirmDestinationState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#endif
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Target.h"
#include "../Savegame/Waypoint.h"
#include "../Savegame/Base.h"
#include "../Engine/Options.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Confirm Destination window.
 * @param game Pointer to the core game.
 * @param craft Pointer to the craft to retarget.
 * @param target Pointer to the selected target (NULL if it's just a point on the globe).
 */
ConfirmDestinationState::ConfirmDestinationState(Craft *craft, Target *target) : _craft(craft), _target(target)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    if (_target) _agentTargetLifetime=_target->agentLifetime();
#endif
	Waypoint *w = dynamic_cast<Waypoint*>(_target);
	_screen = false;

	// Create objects
	_window = new Window(this, 244, 72, 6, 64);
	_btnOk = new TextButton(50, 12, 68, 104);
	_btnCancel = new TextButton(50, 12, 138, 104);
	_txtTarget = new Text(232, 32, 12, 72);

	// Set palette
	setInterface("confirmDestination", w != 0 && w->getId() == 0);

	add(_window, "window", "confirmDestination");
	add(_btnOk, "button", "confirmDestination");
	add(_btnCancel, "button", "confirmDestination");
	add(_txtTarget, "text", "confirmDestination");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK12.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ConfirmDestinationState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ConfirmDestinationState::btnOkClick, Options::keyOk);

	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&ConfirmDestinationState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&ConfirmDestinationState::btnCancelClick, Options::keyCancel);

	_txtTarget->setBig();
	_txtTarget->setAlign(ALIGN_CENTER);
	_txtTarget->setVerticalAlign(ALIGN_MIDDLE);
	_txtTarget->setWordWrap(true);
	if (w != 0 && w->getId() == 0)
	{
		_txtTarget->setText(tr("STR_TARGET").arg(tr("STR_WAY_POINT")));
	}
	else
	{
		_txtTarget->setText(tr("STR_TARGET").arg(_target->getName(_game->getLanguage())));
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *widget:std::vector<Surface*>{_window,_txtTarget,_btnOk,_btnCancel})
        widget->agentTrackPresentation();
#endif
}

/**
 *
 */
ConfirmDestinationState::~ConfirmDestinationState()
{

}

/**
 * Confirms the selected target for the craft.
 * @param action Pointer to an action.
 */
void ConfirmDestinationState::btnOkClick(Action *)
{
	Waypoint *w = dynamic_cast<Waypoint*>(_target);
	if (w != 0 && w->getId() == 0)
	{
		w->setId(_game->getSavedGame()->getId("STR_WAY_POINT"));
		_game->getSavedGame()->getWaypoints()->push_back(w);
	}
	_craft->setDestination(_target);
	_craft->setStatus("STR_OUT");
	_game->popState();
	_game->popState();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ConfirmDestinationState::btnCancelClick(Action *)
{
	Waypoint *w = dynamic_cast<Waypoint*>(_target);
	if (w != 0 && w->getId() == 0)
	{
		delete w;
	}
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ConfirmDestinationState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> ConfirmDestinationState::agentPaintRevisions() const
{
    return {_window->agentPresentationRevision(), _txtTarget->agentPresentationRevision(), _txtTarget->agentTextRevision(),
        _btnOk->agentPresentationRevision(), _btnOk->agentLabelRevision(),
        _btnCancel->agentPresentationRevision(), _btnCancel->agentLabelRevision()};
}

void ConfirmDestinationState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool ConfirmDestinationState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<ConfirmDestinationState *>(this)) || _modal || _agentTargetLifetime.expired() || !agentPaintHealthy() || !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState(); out.actor = _craft;
    out.targetLifetimes.push_back(_agentTargetLifetime);
    const Surface *destination=_game->getScreen()->getSurface();
    if (!_window->agentPresentationPaintedTo(destination)) return false;
    const auto painted=agentFlattenPaintOrder(_surfaces);
    const auto include=[&](const Surface *widget,bool observationOnly)->bool {
        if (!widget->agentPresentationPaintedTo(destination) || !agentWidgetVisible(widget)) return false;
        AgentUiState part; part.regions.push_back({widget->getX(),widget->getY(),widget->getWidth(),widget->getHeight()});
        bool found=false;
        for (const Surface *later:painted)
        {
            if (later==widget) { found=true; continue; }
            if (!found || !later || !later->getVisible()) continue;
            const auto &r=part.regions.front();
            if (r.x<later->getX()+later->getWidth() && r.x+r.width>later->getX() &&
                r.y<later->getY()+later->getHeight() && r.y+r.height>later->getY()) return false;
        }
        if (!found || !(observationOnly ? _game->agentNonCursorOverlayClear(part) : _game->agentOverlayClear(part))) return false;
        if (observationOnly) return true;
        const auto old=out.regions.size();out.regions.push_back(part.regions.front());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(old);return false;
    };
    const auto text=[&](const Text *widget)->std::string {
        out.widgets.emplace_back(widget,widget->agentPresentationRevision());
        out.widgets.emplace_back(widget,widget->agentTextRevision());
        out.context.insert(out.context.end(),{static_cast<std::uint64_t>(widget->getX()),static_cast<std::uint64_t>(widget->getY()),
            static_cast<std::uint64_t>(widget->getWidth()),static_cast<std::uint64_t>(widget->getHeight())});
        return widget->getText().size()<=1024 && include(widget,true) ? agentJsonString(widget->getText()) : "null";
    };
    std::string buttons="[";
    const auto button=[&](TextButton *widget,const char *id,const char *action) {
        out.widgets.emplace_back(widget,widget->agentPresentationRevision());
        out.widgets.emplace_back(widget,widget->agentLabelRevision());
        const std::string label=widget->getText();
        if (label.empty() || label.size()>1024 || !include(widget,false)) return;
        if (buttons.size()>1) buttons+=",";
        buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";
        out.actions.emplace_back(action,label,"visible_text");
    };
    out.screen="interception_confirm"; out.save=_target;
    const auto target=text(_txtTarget);
    button(_btnOk,"confirm","confirm_interception");
    button(_btnCancel,"cancel","cancel_interception_confirmation"); buttons+="]";
    out.visible="{\"target\":"+target+",\"buttons\":"+buttons+"}";
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size()<=9000;
}
bool ConfirmDestinationState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action=="confirm_interception") { btnOkClick(nullptr); return true; }
    if (request.action=="cancel_interception_confirmation") { btnCancelClick(nullptr); return true; }
    return false;
}
#endif

}
