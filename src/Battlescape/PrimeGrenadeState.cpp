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
#include "PrimeGrenadeState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Action.h"
#include "../Interface/Text.h"
#include "../Interface/Frame.h"
#include "../Engine/InteractiveSurface.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleInterface.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Prime Grenade window.
 * @param game Pointer to the core game.
 * @param action Pointer to the action.
 * @param inInventoryView Called from inventory?
 * @param grenadeInInventory Pointer to associated grenade.
 */
PrimeGrenadeState::PrimeGrenadeState(BattleAction *action, bool inInventoryView, BattleItem *grenadeInInventory) : _action(action), _inInventoryView(inInventoryView), _grenadeInInventory(grenadeInInventory)
{
	_screen = false;

	// Create objects
	_title = new Text(192, 24, 65, 44);
	_frame = new Frame(192, 27, 65, 37);
	_bg = new Surface(192, 93, 65, 45);

	int x = 67;
	int y = 68;
	for (int i = 0; i < 24; ++i)
	{
		_button[i] = new InteractiveSurface(22, 22, x-1+((i%8)*24), y-4+((i/8)*25));
		_number[i] = new Text(20, 20, x+((i%8)*24), y-1+((i/8)*25));
	}

	// Set palette
	if (inInventoryView)
	{
		setPalette("PAL_BATTLESCAPE");
	}
	else
	{
		_game->getSavedGame()->getSavedBattle()->setPaletteByDepth(this);
	}

	Element *grenadeBackground = _game->getMod()->getInterface("battlescape")->getElement("grenadeBackground");

	// Set up objects
	add(_bg);
	_bg->drawRect(0, 0, _bg->getWidth(), _bg->getHeight(), grenadeBackground->color);

	add(_frame, "grenadeMenu", "battlescape");
	_frame->setThickness(3);
	_frame->setHighContrast(true);

	add(_title, "grenadeMenu", "battlescape");
	_title->setAlign(ALIGN_CENTER);
	_title->setBig();
	_title->setText(tr("STR_SET_TIMER"));
	_title->setHighContrast(true);

	for (int i = 0; i < 24; ++i)
	{
		SDL_Rect square;
		add(_button[i]);
		_button[i]->onMouseClick((ActionHandler)&PrimeGrenadeState::btnClick);
		square.x = 0;
		square.y = 0;
		square.w = _button[i]->getWidth();
		square.h = _button[i]->getHeight();
		_button[i]->drawRect(&square, grenadeBackground->border);
		square.x++;
		square.y++;
		square.w -= 2;
		square.h -= 2;
		_button[i]->drawRect(&square, grenadeBackground->color2);

		std::ostringstream ss;
		ss << i;
		add(_number[i], "grenadeMenu", "battlescape");
		_number[i]->setBig();
		_number[i]->setText(ss.str());
		_number[i]->setHighContrast(true);
		_number[i]->setAlign(ALIGN_CENTER);
		_number[i]->setVerticalAlign(ALIGN_MIDDLE);
	}

	centerAllSurfaces();
	lowerAllSurfaces();
}

/**
 *
 */
PrimeGrenadeState::~PrimeGrenadeState()
{

}

/**
 * Closes the window on right-click.
 * @param action Pointer to an action.
 */
void PrimeGrenadeState::handle(Action *action)
{
	State::handle(action);
	if (action->getDetails()->type == SDL_MOUSEBUTTONDOWN && action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		cancelTimer();
	}
}


/**
 * Executes the action corresponding to this action menu item.
 * @param action Pointer to an action.
 */
void PrimeGrenadeState::btnClick(Action *action)
{
	int btnID = -1;

	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		cancelTimer();
		return;
	}

	// got to find out which button was pressed
	for (int i = 0; i < 24 && btnID == -1; ++i)
	{
		if (action->getSender() == _button[i])
		{
			btnID = i;
		}
	}

	if (btnID != -1) selectTimer(btnID);
}

void PrimeGrenadeState::cancelTimer()
{
	if (!_inInventoryView) _action->value = -1;
	_game->popState();
}

void PrimeGrenadeState::selectTimer(int timer)
{
	if (timer < 0 || timer >= 24) return;
	if (_inInventoryView) _grenadeInInventory->setFuseTimer(timer);
	else _action->value = timer;
	_game->popState();
	if (!_inInventoryView) _game->popState();
}

}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Savegame/BattleUnit.h"
namespace OpenXcom {
bool PrimeGrenadeState::agentUiState(Observer::AgentUiState &out) const
{
    // The inventory/prebattle timer has different ordinary effects and is not
    // part of this tactical provider. No fuse or eligibility enters the DTO.
    if (_inInventoryView || !_action || _modal ||
        !_game->isState(const_cast<PrimeGrenadeState *>(this))) return false;
    out = Observer::AgentUiState(); out.screen = "prime_grenade";
    const std::string title = Observer::agentWidgetUncovered(_title, _surfaces)
        ? Observer::agentTextJson(_title) : "null";
    if (title != "null") Observer::agentIncludeWidget(out, _title);
    out.visible = "{\"title\":" + title + ",\"timers\":[";
    std::string first;
    for (unsigned i = 0; i < 24; ++i)
    {
        // The number is intentionally painted over its own button. All other
        // later layers must leave the complete control and number uncovered.
        std::vector<Surface *> withoutNumber;
        for (Surface *surface : _surfaces) if (surface != _number[i]) withoutNumber.push_back(surface);
        if (!Observer::agentWidgetUncovered(_button[i], withoutNumber) ||
            !Observer::agentWidgetUncovered(_number[i], _surfaces)) continue;
        const std::string label = Observer::agentTextJson(_number[i]);
        SDL_Rect ink;
        if (label == "null" || _number[i]->getText().empty() || !_number[i]->agentPaintedBounds(ink) ||
            ink.x < _button[i]->getX() || ink.y < _button[i]->getY() ||
            ink.x + ink.w > _button[i]->getX() + _button[i]->getWidth() ||
            ink.y + ink.h > _button[i]->getY() + _button[i]->getHeight()) continue;
        const std::size_t oldSize = out.regions.size();
        Observer::agentIncludeWidget(out, _button[i]);
        if (!Observer::agentRegionsDisjoint(out)) { out.regions.resize(oldSize); continue; }
        if (!out.rows.empty()) out.visible += ",";
        out.visible += "{\"handle\":\"t" + std::to_string(out.rows.size()) + "\",\"label\":" + label + "}";
        out.rows.push_back(i);
        if (first.empty()) first = _number[i]->getText();
    }
    out.visible += "]}";
    if (!first.empty()) out.actions.push_back(Observer::AgentUiAction("choose_grenade_timer", first, "visible_text", "grenade_timer"));
    if (out.regions.empty()) return false;
    out.actions.push_back(Observer::AgentUiAction("cancel_grenade_timer", "Cancel timer", "visible_control_semantics"));
    out.actor = _action->actor; out.save = _game->getSavedGame()->getSavedBattle();
    out.widgets.push_back(std::make_pair(_action, 0));
    out.widgets.push_back(std::make_pair(_action->weapon, 0));
    out.context.push_back(static_cast<std::uint64_t>(_action->type));
    out.context.push_back(static_cast<std::uint64_t>(_action->value));
    out.context.push_back(static_cast<std::uint64_t>(_action->TU));
    out.context.push_back(_action->targeting ? 1 : 0);
    if (_action->actor) { out.actorRevision = _action->actor->stanceControlRevision();
        out.context.push_back(_action->actor->turnControlPoseRevision()); }
    return true;
}
bool PrimeGrenadeState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState shown;
    if (!agentUiState(shown) || !shown.offers(request.action)) return false;
    if (request.action == "cancel_grenade_timer") { cancelTimer(); return true; }
    if (request.action != "choose_grenade_timer" || request.choice.size() < 2 || request.choice[0] != 't') return false;
    const unsigned timer = static_cast<unsigned>(std::stoul(request.choice.substr(1)));
    if (timer >= shown.rows.size()) return false;
    selectTimer(static_cast<int>(shown.rows[timer])); return true;
}
}
#endif
