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
#include "MultipleTargetsState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Savegame/Target.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/Ufo.h"
#include "GeoscapeState.h"
#include "ConfirmDestinationState.h"
#include "InterceptState.h"
#include "UfoDetectedState.h"
#include "GeoscapeCraftState.h"
#include "TargetInfoState.h"
#include "../Engine/Options.h"
#include "../Engine/Action.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentOutfitting.h"
#include <algorithm>
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Multiple Targets window.
 * @param game Pointer to the core game.
 * @param targets List of targets to display.
 * @param craft Pointer to craft to retarget (NULL if none).
 * @param state Pointer to the Geoscape state.
 */
MultipleTargetsState::MultipleTargetsState(std::vector<Target*> targets, Craft *craft, GeoscapeState *state) : _targets(targets), _craft(craft), _state(state)
{
	_screen = false;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for(Target *target:_targets) _agentTargetLifetimes.push_back(target ? target->agentLifetime() : std::weak_ptr<const void>());
    if(_craft) _agentCraftLifetime=_craft->agentLifetime();
#endif

	if (_targets.size() > 1)
	{
		int winHeight = BUTTON_HEIGHT * _targets.size() + SPACING * (_targets.size() - 1) + MARGIN * 2;
		int winY = (200 - winHeight) / 2;
		int btnY = winY + MARGIN;

		// Create objects
		_window = new Window(this, 136, winHeight, 60, winY, POPUP_VERTICAL);

		// Set palette
		setInterface("multipleTargets");

		add(_window, "window", "multipleTargets");

		// Set up objects
		_window->setBackground(_game->getMod()->getSurface("BACK15.SCR"));

		int y = btnY;
		for (size_t i = 0; i < _targets.size(); ++i)
		{
			TextButton *button = new TextButton(116, BUTTON_HEIGHT, 70, y);
			button->setText(_targets[i]->getName(_game->getLanguage()));
			button->onMouseClick((ActionHandler)&MultipleTargetsState::btnTargetClick);
			add(button, "button", "multipleTargets");

			_btnTargets.push_back(button);

			y += button->getHeight() + SPACING;
		}
		_btnTargets[0]->onKeyboardPress((ActionHandler)&MultipleTargetsState::btnCancelClick, Options::keyCancel);

		centerAllSurfaces();
	}
}

/**
 *
 */
MultipleTargetsState::~MultipleTargetsState()
{

}

/**
 * Resets the palette and ignores the window
 * if there's only one target.
 */
void MultipleTargetsState::init()
{
	if (_targets.size() == 1)
	{
		popupTarget(*_targets.begin());
	}
	else
	{
		State::init();
	}
}

/**
 * Displays the right popup for a specific target.
 * @param target Pointer to target.
 */
void MultipleTargetsState::popupTarget(Target *target)
{
	_game->popState();
	if (_craft == 0)
	{
		Base* b = dynamic_cast<Base*>(target);
		Craft* c = dynamic_cast<Craft*>(target);
		Ufo* u = dynamic_cast<Ufo*>(target);
		if (b != 0)
		{
			_game->pushState(new InterceptState(_state->getGlobe(), b));
		}
		else if (c != 0)
		{
			_game->pushState(new GeoscapeCraftState(c, _state->getGlobe(), 0));
		}
		else if (u != 0)
		{
			_game->pushState(new UfoDetectedState(u, _state, false, u->getHyperDetected()));
		}
		else
		{
			_game->pushState(new TargetInfoState(target, _state->getGlobe()));
		}
	}
	else
	{
		_game->pushState(new ConfirmDestinationState(_craft, target));
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void MultipleTargetsState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Pick a target to display.
 * @param action Pointer to an action.
 */
void MultipleTargetsState::btnTargetClick(Action *action)
{
	for (size_t i = 0; i < _btnTargets.size(); ++i)
	{
		if (action->getSender() == _btnTargets[i])
		{
			popupTarget(_targets[i]);
			break;
		}
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void MultipleTargetsState::blit()
{
    _agentDestinationPaintFull=false;_agentStoredPaintRevisions.clear();
    const bool before=Observer::agentOutfittingDestinationFull(_game);
    State::blit();
    _agentStoredPaintRevisions=Observer::agentOutfittingPaintRevisions(_surfaces);
    _agentDestinationPaintFull=before && Observer::agentOutfittingDestinationFull(_game) && Observer::agentPaintHealthy();
}
bool MultipleTargetsState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if(_targets.size()<2 || _targets.size()!=_btnTargets.size() || _targets.size()!=_agentTargetLifetimes.size() ||
        !_game->isState(const_cast<MultipleTargetsState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentOutfittingDestinationFull(_game) ||
        _agentStoredPaintRevisions!=agentOutfittingPaintRevisions(_surfaces) || (_craft && _agentCraftLifetime.expired())) return false;
    for(const auto &life:_agentTargetLifetimes) if(life.expired()) return false;
    out=AgentUiState();out.screen="multiple_targets";out.actor=this;out.save=_game->getSavedGame();out.uiRevision=_agentUiRevision;
    out.targetLifetimes=_agentTargetLifetimes;
    if(_craft)out.targetLifetimes.push_back(_agentCraftLifetime);
    AgentOutfittingView view(_game,_surfaces,out);
    std::string rows="[";
    for(size_t i=0;i<_btnTargets.size();++i)
    {
        const auto *button=_btnTargets[i];
        out.widgets.emplace_back(button,button->agentLabelRevision());
        const std::string label=agentButtonLabel(button);
        if(label.empty() || label.size()>1024 || !agentWidgetUncovered(button,view.painted) || !view.include(button))continue;
        if(!out.rows.empty())rows+=",";
        rows+="{\"handle\":\"r"+std::to_string(out.rows.size())+"\",\"cells\":["+agentJsonString(label)+"],\"selected\":false}";
        out.rows.push_back(i);
    }
    rows+="]";
    if(!out.rows.empty())
    {
        out.actions.emplace_back("choose_globe_target","Select target","visible_control_semantics","outfitting_row");
        // Native cancel key is registered on the first target button only.
        if(std::find(out.rows.begin(),out.rows.end(),size_t(0))!=out.rows.end())
            out.actions.emplace_back("cancel_globe_targets","Cancel target selection","visible_control_semantics");
    }
    out.visible="{\"rows\":"+rows+",\"buttons\":[]}";
    return view.finish();
}
bool MultipleTargetsState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;if(!agentUiState(current) || !current.offers(request.action))return false;
    if(request.action=="cancel_globe_targets"){btnCancelClick(nullptr);return true;}
    size_t row=0;
    if(request.action=="choose_globe_target" && Observer::agentOutfittingRow(current,request,row) && row<_btnTargets.size())
    {
        SDL_Event event={};event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;
        Action action(&event,1.0,1.0,0,0);action.setSender(_btnTargets[row]);btnTargetClick(&action);return true;
    }
    return false;
}
#endif

}
