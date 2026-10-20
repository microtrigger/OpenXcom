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
#include "NewManufactureListState.h"
#include "../Interface/Window.h"
#include "../Interface/TextButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Interface/ComboBox.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleManufacture.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "ManufactureStartState.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the productions list screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 */
NewManufactureListState::NewManufactureListState(Base *base) : _base(base)
{
	_screen = false;

	_window = new Window(this, 320, 156, 0, 22, POPUP_BOTH);
	_btnOk = new TextButton(304, 16, 8, 154);
	_txtTitle = new Text(320, 17, 0, 30);
	_txtItem = new Text(156, 9, 10, 62);
	_txtCategory = new Text(130, 9, 166, 62);
	_lstManufacture = new TextList(288, 80, 8, 70);
	_cbxCategory = new ComboBox(this, 146, 16, 166, 46);

	// Set palette
	setInterface("selectNewManufacture");

	add(_window, "window", "selectNewManufacture");
	add(_btnOk, "button", "selectNewManufacture");
	add(_txtTitle, "text", "selectNewManufacture");
	add(_txtItem, "text", "selectNewManufacture");
	add(_txtCategory, "text", "selectNewManufacture");
	add(_lstManufacture, "list", "selectNewManufacture");
	add(_cbxCategory, "catBox", "selectNewManufacture");

	centerAllSurfaces();

	_window->setBackground(_game->getMod()->getSurface("BACK17.SCR"));

	_txtTitle->setText(tr("STR_PRODUCTION_ITEMS"));
	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);

	_txtItem->setText(tr("STR_ITEM"));

	_txtCategory->setText(tr("STR_CATEGORY"));

	_lstManufacture->setColumns(2, 156, 130);
	_lstManufacture->setSelectable(true);
	_lstManufacture->setBackground(_window);
	_lstManufacture->setMargin(2);
	_lstManufacture->onMouseClick((ActionHandler)&NewManufactureListState::lstProdClick);

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&NewManufactureListState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&NewManufactureListState::btnOkClick, Options::keyCancel);

	_possibleProductions.clear();
	_game->getSavedGame()->getAvailableProductions(_possibleProductions, _game->getMod(), _base);
	_catStrings.push_back("STR_ALL_ITEMS");

	for (std::vector<RuleManufacture *>::iterator it = _possibleProductions.begin(); it != _possibleProductions.end(); ++it)
	{
		bool addCategory = true;
		for (size_t x = 0; x < _catStrings.size(); ++x)
		{
			if ((*it)->getCategory() == _catStrings[x])
			{
				addCategory = false;
				break;
			}
		}
		if (addCategory)
		{
			_catStrings.push_back((*it)->getCategory());
		}
	}

	_cbxCategory->setOptions(_catStrings, true);
	_cbxCategory->onChange((ActionHandler)&NewManufactureListState::cbxCategoryChange);

}

/**
 * Initializes state (fills list of possible productions).
 */
void NewManufactureListState::init()
{
	State::init();
	fillProductionList();
}

/**
 * Returns to the previous screen.
 * @param action A pointer to an Action.
 */
void NewManufactureListState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Opens the Production settings screen.
 * @param action A pointer to an Action.
 */
void NewManufactureListState::lstProdClick(Action *)
{
	selectProduction(_lstManufacture->getSelectedRow());
}

void NewManufactureListState::selectProduction(size_t row)
{
	RuleManufacture *rule = 0;
	for (std::vector<RuleManufacture *>::iterator it = _possibleProductions.begin(); it != _possibleProductions.end(); ++it)
	{
		if ((*it)->getName() == _displayedStrings[row])
		{
			rule = (*it);
			break;
		}
	}
	_game->pushState(new ManufactureStartState(_base, rule));
}

/**
 * Updates the production list to match the category filter
 */

void NewManufactureListState::cbxCategoryChange(Action *)
{
	fillProductionList();
}

/**
 * Fills the list of possible productions.
 */
void NewManufactureListState::fillProductionList()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	++_agentMappingRevision;
#endif
	_lstManufacture->clearList();
	_possibleProductions.clear();
	_game->getSavedGame()->getAvailableProductions(_possibleProductions, _game->getMod(), _base);
	_displayedStrings.clear();

	for (std::vector<RuleManufacture *>::iterator it = _possibleProductions.begin(); it != _possibleProductions.end(); ++it)
	{
		if (((*it)->getCategory() == _catStrings[_cbxCategory->getSelected()]) || (_catStrings[_cbxCategory->getSelected()] == "STR_ALL_ITEMS"))
		{
			_lstManufacture->addRow(2, tr((*it)->getName()).c_str(), tr((*it)->getCategory()).c_str());
			_displayedStrings.push_back((*it)->getName());
		}
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool NewManufactureListState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (_modal || !_game->isState(const_cast<NewManufactureListState *>(this)) || !agentPaintHealthy()) return false;
    out=AgentUiState(); out.screen="new_manufacture"; out.actor=_base; out.uiRevision=_agentMappingRevision;
    const auto painted=agentFlattenPaintOrder(_surfaces);
    const auto allowed=[this](const AgentUiState &part)->bool { return _game->agentOverlayClear(part); };
    const auto include=[&](const Surface *widget)->bool
    {
        if (!agentWidgetUncovered(widget,painted)) return false;
        AgentUiState part; agentIncludeWidget(part,widget);
        if (part.regions.empty() || !allowed(part)) return false;
        const auto before=out.regions.size();
        out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before); return false;
    };
    const auto text=[&](const Text *widget)->std::string
    {
        const auto value=agentTextJson(widget);
        // Empty shown text is public but has no ink footprint to include.
        if (value=="null" || !agentWidgetUncovered(widget,painted)) return "null";
        if (widget->getText().empty()) return value;
        return include(widget)?value:"null";
    };
    std::string buttons="[";
    const auto button=[&](TextButton *widget,const char *id,const char *action,bool authority)->bool
    {
        const auto label=agentButtonLabel(widget);
        if (label.empty() || label.size()>1024 || !include(widget)) return false;
        if (buttons.size()>1) buttons+=",";
        buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";
        if (authority) out.actions.emplace_back(action,label,"visible_text");
        return true;
    };
    const auto title=text(_txtTitle);
    const std::string summary="[]";
    const std::string headings="["+text(_txtItem)+","+text(_txtCategory)+"]";
    AgentListProjection list;
    if (!agentProjectList(_lstManufacture,_lstManufacture,painted,2,true,out,list,allowed,true)) return false;
    out.context.push_back(list.up?1:0); out.context.push_back(list.down?1:0);
    if (list.up || list.down) out.actions.emplace_back("scroll_visible_list","Scroll visible list","visible_control_semantics","scroll_direction");
    if (title!="null" && !list.firstLabel.empty())
        out.actions.emplace_back("select_production_project",list.firstLabel,"visible_text","project_row");
    button(_btnOk,"close","close_production_list",true);
    buttons+="]";
    out.visible="{\"title\":"+title+",\"summary\":"+summary+",\"headings\":"+headings+",\"rows\":"+list.rows+",\"scroll\":"+list.scroll+",\"buttons\":"+buttons+"}";
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size()<=32768;
}

bool NewManufactureListState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action=="close_production_list") { btnOkClick(nullptr); return true; }
    if (request.action=="scroll_visible_list" && current.context.size()==2)
    {
        if (request.choice=="up" && current.context[0]) { _lstManufacture->scrollUp(false,false); return true; }
        if (request.choice=="down" && current.context[1]) { _lstManufacture->scrollDown(false,false); return true; }
    }
    if (request.action=="select_production_project")
    {
        if (request.choice.size()<2 || request.choice.size()>3 || request.choice[0]!='r' ||
            (request.choice.size()==3 && request.choice[1]=='0')) return false;
        unsigned index=0;
        for (std::size_t i=1;i<request.choice.size();++i) { if (request.choice[i]<'0' || request.choice[i]>'9') return false; index=index*10+request.choice[i]-'0'; }
        if (index>=current.rows.size()) return false;
        const auto row=current.rows[index];
        if (row>=_displayedStrings.size()) return false;
        if (!_lstManufacture->agentSelectProjectRow(row)) return false;
        selectProduction(row); return true;
    }
    return false;
}
#endif

}
