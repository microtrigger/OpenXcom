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
#include "InterceptState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#endif
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"
#include "../Savegame/SavedGame.h"
#include "../Engine/Options.h"
#include "Globe.h"
#include "SelectDestinationState.h"
#include "ConfirmDestinationState.h"
#include "../Basescape/BasescapeState.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Intercept window.
 * @param game Pointer to the core game.
 * @param globe Pointer to the Geoscape globe.
 * @param base Pointer to base to show contained crafts (NULL to show all crafts).
 * @param target Pointer to target to intercept (NULL to ask user for target).
 */
InterceptState::InterceptState(Globe *globe, Base *base, Target *target) : _globe(globe), _base(base), _target(target)
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    , _agentTargetless(target == nullptr)
#endif
{
	_screen = false;

	// Create objects
	_window = new Window(this, 320, 140, 0, 30, POPUP_HORIZONTAL);
	_btnCancel = new TextButton(_base ? 142 : 288, 16, 16, 146);
	_btnGotoBase = new TextButton(142, 16, 162, 146);
	_txtTitle = new Text(300, 17, 10, 46);
	_txtCraft = new Text(86, 9, 14, 70);
	_txtStatus = new Text(70, 9, 100, 70);
	_txtBase = new Text(80, 9, 170, 70);
	_txtWeapons = new Text(80, 17, 238, 62);
	_lstCrafts = new TextList(288, 64, 8, 78);

	// Set palette
	setInterface("intercept");

	add(_window, "window", "intercept");
	add(_btnCancel, "button", "intercept");
	add(_btnGotoBase, "button", "intercept");
	add(_txtTitle, "text1", "intercept");
	add(_txtCraft, "text2", "intercept");
	add(_txtStatus, "text2", "intercept");
	add(_txtBase, "text2", "intercept");
	add(_txtWeapons, "text2", "intercept");
	add(_lstCrafts, "list", "intercept");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK12.SCR"));

	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&InterceptState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&InterceptState::btnCancelClick, Options::keyCancel);
	_btnCancel->onKeyboardPress((ActionHandler)&InterceptState::btnCancelClick, Options::keyGeoIntercept);

	_btnGotoBase->setText(tr("STR_GO_TO_BASE"));
	_btnGotoBase->onMouseClick((ActionHandler)&InterceptState::btnGotoBaseClick);
	_btnGotoBase->setVisible(_base != 0);

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_LAUNCH_INTERCEPTION"));

	_txtCraft->setText(tr("STR_CRAFT"));

	_txtStatus->setText(tr("STR_STATUS"));

	_txtBase->setText(tr("STR_BASE"));

	_txtWeapons->setText(tr("STR_WEAPONS_CREW_HWPS"));

	_lstCrafts->setColumns(4, 86, 70, 80, 46);
	_lstCrafts->setSelectable(true);
	_lstCrafts->setBackground(_window);
	_lstCrafts->setMargin(6);
	_lstCrafts->onMouseClick((ActionHandler)&InterceptState::lstCraftsLeftClick);
	_lstCrafts->onMouseClick((ActionHandler)&InterceptState::lstCraftsRightClick, SDL_BUTTON_RIGHT);

	int row = 0;
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		if (_base != 0 && (*i) != _base)
			continue;
		for (std::vector<Craft*>::iterator j = (*i)->getCrafts()->begin(); j != (*i)->getCrafts()->end(); ++j)
		{
			std::ostringstream ss;
			if ((*j)->getNumWeapons() > 0)
			{
				ss << Unicode::TOK_COLOR_FLIP << (*j)->getNumWeapons() << Unicode::TOK_COLOR_FLIP;
			}
			else
			{
				ss << 0;
			}
			ss << "/";
			if ((*j)->getNumSoldiers() > 0)
			{
				ss << Unicode::TOK_COLOR_FLIP << (*j)->getNumSoldiers() << Unicode::TOK_COLOR_FLIP;
			}
			else
			{
				ss << 0;
			}
			ss << "/";
			if ((*j)->getNumVehicles() > 0)
			{
				ss << Unicode::TOK_COLOR_FLIP << (*j)->getNumVehicles() << Unicode::TOK_COLOR_FLIP;
			}
			else
			{
				ss << 0;
			}
			_crafts.push_back(*j);
			_lstCrafts->addRow(4, (*j)->getName(_game->getLanguage()).c_str(), tr((*j)->getStatus()).c_str(), (*i)->getName().c_str(), ss.str().c_str());
			if ((*j)->getStatus() == "STR_READY")
			{
				_lstCrafts->setCellColor(row, 1, _lstCrafts->getSecondaryColor());
			}
			row++;
		}
	}
}

/**
 *
 */
InterceptState::~InterceptState()
{

}

/**
 * Closes the window.
 * @param action Pointer to an action.
 */
void InterceptState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Goes to the base for the respective craft.
 * @param action Pointer to an action.
 */
void InterceptState::btnGotoBaseClick(Action *)
{
	_game->popState();
	_game->pushState(new BasescapeState(_base, _globe));
}

/**
 * Pick a target for the selected craft.
 * @param action Pointer to an action.
 */
void InterceptState::lstCraftsLeftClick(Action *)
{
	Craft* c = _crafts[_lstCrafts->getSelectedRow()];
	if (c->getStatus() == "STR_READY" || ((c->getStatus() == "STR_OUT" || Options::craftLaunchAlways) && !c->getLowFuel() && !c->getMissionComplete()))
	{
		_game->popState();
		if (_target == 0)
		{
			_game->pushState(new SelectDestinationState(c, _globe));
		}
		else
		{
			_game->pushState(new ConfirmDestinationState(c, _target));
		}
	}
}

/**
 * Centers on the selected craft.
 * @param action Pointer to an action.
 */
void InterceptState::lstCraftsRightClick(Action *)
{
	Craft* c = _crafts[_lstCrafts->getSelectedRow()];
	if (c->getStatus() == "STR_OUT")
	{
		_globe->center(c->getLongitude(), c->getLatitude());
		_game->popState();
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool InterceptState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> InterceptState::agentPaintRevisions() const
{
    return {_txtTitle->agentTextRevision(), _txtCraft->agentTextRevision(), _txtStatus->agentTextRevision(), _txtBase->agentTextRevision(), _txtWeapons->agentTextRevision(), _btnCancel->agentLabelRevision(), _btnGotoBase->agentLabelRevision(), _lstCrafts->agentListRevision()};
}

void InterceptState::blit()
{
    _agentDestinationPaintFull = false;
    _agentStoredPaintRevisions.clear();
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool InterceptState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<InterceptState *>(this)) || _modal || (_agentTargetless != (_target == nullptr)) || !agentPaintHealthy() || !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState(); out.actor = _base; out.save = _target;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const auto allowed = [this](const AgentUiState &part) { return _game->agentOverlayClear(part); };
    const auto include = [&](const Surface *widget) -> bool {
        if (!agentWidgetUncovered(widget,painted)) return false;
        AgentUiState part; agentIncludeWidget(part,widget);
        if (part.regions.empty() || !allowed(part)) return false;
        const auto before=out.regions.size();
        out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before); return false;
    };
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        const auto value=agentTextJson(widget);
        return value!="null" && (widget->getText().empty() || include(widget)) ? value : "null";
    };
    std::string buttons="[";
    const auto button = [&](TextButton *widget,const char *id,const char *action) {
        out.widgets.emplace_back(widget, widget->agentLabelRevision());
        const auto label=agentButtonLabel(widget);
        if (label.empty() || label.size()>1024 || !include(widget)) return;
        if (buttons.size()>1) buttons+=",";
        buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";
        out.actions.emplace_back(action,label,"visible_text");
    };
    out.screen="intercept_craft";
    out.widgets.emplace_back(_lstCrafts,_lstCrafts->agentListRevision());
    const auto title=text(_txtTitle), craft=text(_txtCraft), status=text(_txtStatus), base=text(_txtBase), weapons=text(_txtWeapons);
    button(_btnCancel,"cancel","cancel_interception");
    button(_btnGotoBase,"base","open_intercept_base"); buttons+="]";
    AgentListProjection list;
    if (!agentProjectInterceptList(_lstCrafts,_lstCrafts,painted,out,list,allowed)) return false;
    out.context={list.up?1u:0u,list.down?1u:0u,_agentTargetless?1u:0u};
    out.visible="{\"title\":"+title+",\"headings\":["+craft+","+status+","+base+","+weapons+"],\"rows\":"+list.rows+",\"scroll\":"+list.scroll+",\"buttons\":"+buttons+"}";
    if (!out.rows.empty()) out.actions.emplace_back("choose_visible_row","Choose visible row","visible_control_semantics","visible_row");
    if (list.up || list.down) out.actions.emplace_back("scroll_visible_list","Scroll visible list","visible_control_semantics","scroll_direction");
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size()<=9000;
}
bool InterceptState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action=="cancel_interception") { btnCancelClick(nullptr); return true; }
    if (request.action=="open_intercept_base") { btnGotoBaseClick(nullptr); return true; }
    if (request.action=="scroll_visible_list" && current.context.size()==3) {
        if (request.choice=="up" && current.context[0]) { _lstCrafts->scrollUp(false,false); return true; }
        if (request.choice=="down" && current.context[1]) { _lstCrafts->scrollDown(false,false); return true; }
    }
    if (request.action!="choose_visible_row" || request.choice.size()<2 || request.choice.size()>3 || request.choice[0]!='r' || (request.choice.size()==3 && request.choice[1]=='0')) return false;
    unsigned index=0;
    for (size_t i=1;i<request.choice.size();++i) { if(request.choice[i]<'0'||request.choice[i]>'9') return false; index=index*10+request.choice[i]-'0'; }
    if (index>=current.rows.size() || current.rows[index]>=_crafts.size() || !_lstCrafts->agentSelectProjectRow(current.rows[index])) return false;
    lstCraftsLeftClick(nullptr); return true;
}
#endif

}
