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

#include "Ufopaedia.h"
#include "UfopaediaSelectState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentOutfitting.h"
#endif
#include "../Mod/ArticleDefinition.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{
	UfopaediaSelectState::UfopaediaSelectState(const std::string &section) : _section(section)
	{
		_screen = false;

		// set background window
		_window = new Window(this, 256, 180, 32, 10, POPUP_NONE);

		// set title
		_txtTitle = new Text(224, 17, 48, 26);

		// set buttons
		_btnOk = new TextButton(224, 16, 48, 166);
		_lstSelection = new TextList(224, 104, 40, 50);

		// Set palette
		setInterface("ufopaedia");

		add(_window, "window", "ufopaedia");
		add(_txtTitle, "text", "ufopaedia");
		add(_btnOk, "button2", "ufopaedia");
		add(_lstSelection, "list", "ufopaedia");

		centerAllSurfaces();

		_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

		_txtTitle->setBig();
		_txtTitle->setAlign(ALIGN_CENTER);
		_txtTitle->setText(tr("STR_SELECT_ITEM"));

		_btnOk->setText(tr("STR_OK"));
		_btnOk->onMouseClick((ActionHandler)&UfopaediaSelectState::btnOkClick);
		_btnOk->onKeyboardPress((ActionHandler)&UfopaediaSelectState::btnOkClick,Options::keyCancel);

		_lstSelection->setColumns(1, 206);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        _lstSelection->agentEnablePaintedSingleColumn();
#endif
		_lstSelection->setSelectable(true);
		_lstSelection->setBackground(_window);
		_lstSelection->setMargin(18);
		_lstSelection->setAlign(ALIGN_CENTER);
		_lstSelection->onMouseClick((ActionHandler)&UfopaediaSelectState::lstSelectionClick);

		loadSelectionList();
	}

	UfopaediaSelectState::~UfopaediaSelectState()
	{}

	/**
	 * Initializes the state.
	 */
	void UfopaediaSelectState::init()
	{
		State::init();
	}

	/**
	 * Returns to the previous screen.
	 * @param action Pointer to an action.
	 */
	void UfopaediaSelectState::btnOkClick(Action *)
	{
		_game->popState();
	}

	/**
	 *
	 * @param action Pointer to an action.
	 */
	void UfopaediaSelectState::lstSelectionClick(Action *)
	{
		Ufopaedia::openArticle(_game, _article_list[_lstSelection->getSelectedRow()]);
	}

	void UfopaediaSelectState::loadSelectionList()
	{
		ArticleDefinitionList::iterator it;

		_article_list.clear();
		Ufopaedia::list(_game->getSavedGame(), _game->getMod(), _section, _article_list);
		for (it = _article_list.begin(); it!=_article_list.end(); ++it)
		{
			_lstSelection->addRow(1, tr((*it)->title).c_str());
		}
	}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void UfopaediaSelectState::blit()
{
    _agentDestinationPaintFull=false;_agentStoredPaintRevisions.clear();
    const bool before=Observer::agentOutfittingDestinationFull(_game);
    State::blit();
    _agentStoredPaintRevisions=Observer::agentOutfittingPaintRevisions(_surfaces);
    _agentDestinationPaintFull=before && Observer::agentOutfittingDestinationFull(_game) && Observer::agentPaintHealthy();
}
bool UfopaediaSelectState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<UfopaediaSelectState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentOutfittingDestinationFull(_game) ||
        _agentStoredPaintRevisions!=agentOutfittingPaintRevisions(_surfaces)) return false;
    // Revalidate only the same player-known filter used by the ordinary UI.
    // No undiscovered catalog or research requirements enter the projection.
    ArticleDefinitionList available;
    Ufopaedia::list(_game->getSavedGame(),_game->getMod(),_section,available);
    if (available!=_article_list) return false;
    out=AgentUiState();out.screen="ufopaedia_select";out.actor=this;out.save=_game->getSavedGame();out.uiRevision=_agentUiRevision;
    AgentOutfittingView view(_game,_surfaces,out);
    const auto title=view.text(_txtTitle);
    AgentListProjection list;if (!view.list(_lstSelection,1,false,list)) return false;
    for (size_t row:out.rows)
    {
        if (row>=_article_list.size()) return false;
        out.widgets.emplace_back(_article_list[row],0);
    }
    if (!out.rows.empty()) out.actions.emplace_back("select_ufopaedia_article","Select article","visible_control_semantics","outfitting_row");
    view.button(_btnOk,"close","close_ufopaedia_select");
    out.visible="{\"title\":"+title+",\"rows\":"+list.rows+",\"scroll\":"+list.scroll+",\"buttons\":"+view.buttons+"]}";
    return view.finish();
}
bool UfopaediaSelectState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if(request.action=="close_ufopaedia_select") { btnOkClick(nullptr);return true; }
    if(Observer::agentOutfittingScroll(_lstSelection,current,request)) return true;
    size_t row=0;
    if(request.action=="select_ufopaedia_article" && Observer::agentOutfittingRow(current,request,row) &&
        row<_article_list.size() && _lstSelection->agentSelectOutfittingRow(row))
    { lstSelectionClick(nullptr);return true; }
    return false;
}
#endif

}
