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
#include "FundingState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Unicode.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Country.h"
#include "../Mod/RuleCountry.h"
#include "../Savegame/SavedGame.h"
#include "../Engine/Options.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Funding screen.
 * @param game Pointer to the core game.
 */
FundingState::FundingState()
{
	_screen = false;

	// Create objects
	_window = new Window(this, 320, 200, 0, 0, POPUP_BOTH);
	_btnOk = new TextButton(50, 12, 135, 180);
	_txtTitle = new Text(320, 17, 0, 8);
	_txtCountry = new Text(100, 9, 32, 30);
	_txtFunding = new Text(100, 9, 140, 30);
	_txtChange = new Text(72, 9, 240, 30);
	_lstCountries = new TextList(260, 136, 32, 40);

	// Set palette
	setInterface("fundingWindow");

	add(_window, "window", "fundingWindow");
	add(_btnOk, "button", "fundingWindow");
	add(_txtTitle, "text1", "fundingWindow");
	add(_txtCountry, "text2", "fundingWindow");
	add(_txtFunding, "text2", "fundingWindow");
	add(_txtChange, "text2", "fundingWindow");
	add(_lstCountries, "list", "fundingWindow");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&FundingState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&FundingState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&FundingState::btnOkClick, Options::keyCancel);
	_btnOk->onKeyboardPress((ActionHandler)&FundingState::btnOkClick, Options::keyGeoFunding);

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_INTERNATIONAL_RELATIONS"));

	_txtCountry->setText(tr("STR_COUNTRY"));

	_txtFunding->setText(tr("STR_FUNDING"));

	_txtChange->setText(tr("STR_CHANGE"));

	_lstCountries->setColumns(3, 108, 100, 52);
	_lstCountries->setDot(true);
	for (std::vector<Country*>::iterator i = _game->getSavedGame()->getCountries()->begin(); i != _game->getSavedGame()->getCountries()->end(); ++i)
	{
		std::ostringstream ss, ss2;
		ss << Unicode::TOK_COLOR_FLIP << Unicode::formatFunding((*i)->getFunding().at((*i)->getFunding().size()-1)) << Unicode::TOK_COLOR_FLIP;
		if ((*i)->getFunding().size() > 1)
		{
			ss2 << Unicode::TOK_COLOR_FLIP;
			int change = (*i)->getFunding().back() - (*i)->getFunding().at((*i)->getFunding().size()-2);
			if (change > 0)
				ss2 << '+';
			ss2 << Unicode::formatFunding(change);
			ss2 << Unicode::TOK_COLOR_FLIP;
		}
		else
		{
			ss2 << Unicode::formatFunding(0);
		}
		_lstCountries->addRow(3, tr((*i)->getRules()->getType()).c_str(), ss.str().c_str(), ss2.str().c_str());
	}
	_lstCountries->addRow(2, tr("STR_TOTAL_UC").c_str(), Unicode::formatFunding(_game->getSavedGame()->getCountryFunding()).c_str());
	_lstCountries->setRowColor(_game->getSavedGame()->getCountries()->size(), _txtCountry->getColor());
}

/**
 *
 */
FundingState::~FundingState()
{

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void FundingState::btnOkClick(Action *)
{
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool FundingState::agentUiState(Observer::AgentUiState &out) const
{
    if (_modal) return false;
    out = Observer::AgentUiState();
    out.screen = "funding";
    const std::vector<Surface *> painted = Observer::agentFlattenPaintOrder(_surfaces);
    const auto text = [&](const Text *widget) -> std::string
    {
        if (!Observer::agentWidgetUncovered(widget, painted)) return "null";
        const std::string value = Observer::agentTextJson(widget);
        if (value != "null") Observer::agentIncludeWidget(out, widget);
        return value;
    };
    const std::string title = text(_txtTitle);
    const std::string country = text(_txtCountry), funding = text(_txtFunding), change = text(_txtChange);
    Observer::AgentListProjection list;
    if (!Observer::agentProjectList(_lstCountries, _lstCountries, painted, 3, false, out, list)) return false;
    out.visible = "{\"title\":" + title + ",\"headings\":[" + country + "," + funding + "," + change +
        "],\"rows\":" + list.rows + ",\"scroll\":" + list.scroll + "}";
    if (list.up || list.down)
        out.actions.push_back(Observer::AgentUiAction("scroll_visible_list", "Scroll visible list", "visible_control_semantics", "scroll_direction"));
    if (Observer::agentWidgetUncovered(_btnOk, painted))
    {
        const std::string label = Observer::agentButtonLabel(_btnOk);
        if (!label.empty())
        {
            out.actions.push_back(Observer::AgentUiAction("close_funding", label, "visible_text"));
            Observer::agentIncludeWidget(out, _btnOk);
        }
    }
    return true;
}

bool FundingState::agentDispatch(const Observer::AgentRequest &request)
{
    const std::string &action = request.action;
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(action)) return false;
    if (action == "close_funding") { btnOkClick(nullptr); return true; }
    if (action == "scroll_visible_list")
    {
        Observer::AgentUiState projection;
        Observer::AgentListProjection list;
        if (!Observer::agentProjectList(_lstCountries, _lstCountries, Observer::agentFlattenPaintOrder(_surfaces), 3, false, projection, list)) return false;
        if (request.choice == "up" && list.up) { _lstCountries->scrollUp(false, false); return true; }
        if (request.choice == "down" && list.down) { _lstCountries->scrollDown(false, false); return true; }
    }
    return false;
}
#endif

}
