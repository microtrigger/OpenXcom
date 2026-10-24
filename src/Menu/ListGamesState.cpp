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
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#endif
#include "ListGamesState.h"
#include <algorithm>
#include <functional>
#include "../Engine/Logger.h"
#include "../Savegame/SavedGame.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/Exception.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Interface/ArrowButton.h"
#include "DeleteGameState.h"

namespace OpenXcom
{

struct compareSaveName : public std::binary_function<SaveInfo&, SaveInfo&, bool>
{
	bool _reverse;

	compareSaveName(bool reverse) : _reverse(reverse) {}

	bool operator()(const SaveInfo &a, const SaveInfo &b) const
	{
		if (a.reserved == b.reserved)
		{
			return Unicode::naturalCompare(a.displayName, b.displayName);
		}
		else
		{
			return _reverse ? b.reserved : a.reserved;
		}
	}
};

struct compareSaveTimestamp : public std::binary_function<SaveInfo&, SaveInfo&, bool>
{
	bool _reverse;

	compareSaveTimestamp(bool reverse) : _reverse(reverse) {}

	bool operator()(const SaveInfo &a, const SaveInfo &b) const
	{
		if (a.reserved == b.reserved)
		{
			return a.timestamp < b.timestamp;
		}
		else
		{
			return _reverse ? b.reserved : a.reserved;
		}
	}
};

/**
 * Initializes all the elements in the Saved Game screen.
 * @param game Pointer to the core game.
 * @param origin Game section that originated this state.
 * @param firstValidRow First row containing saves.
 * @param autoquick Show auto/quick saved games?
 */
ListGamesState::ListGamesState(OptionsOrigin origin, int firstValidRow, bool autoquick) : _origin(origin), _firstValidRow(firstValidRow), _autoquick(autoquick), _sortable(true)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 320, 200, 0, 0, POPUP_BOTH);
	_btnCancel = new TextButton(80, 16, 120, 172);
	_txtTitle = new Text(310, 17, 5, 7);
	_txtDelete = new Text(310, 9, 5, 23);
	_txtName = new Text(150, 9, 16, 32);
	_txtDate = new Text(110, 9, 204, 32);
	_lstSaves = new TextList(288, 112, 8, 42);
	_txtDetails = new Text(288, 16, 16, 156);
	_sortName = new ArrowButton(ARROW_NONE, 11, 8, 16, 32);
	_sortDate = new ArrowButton(ARROW_NONE, 11, 8, 204, 32);

	// Set palette
	setInterface("geoscape", true, _game->getSavedGame() ? _game->getSavedGame()->getSavedBattle() : 0);

	add(_window, "window", "saveMenus");
	add(_btnCancel, "button", "saveMenus");
	add(_txtTitle, "text", "saveMenus");
	add(_txtDelete, "text", "saveMenus");
	add(_txtName, "text", "saveMenus");
	add(_txtDate, "text", "saveMenus");
	add(_lstSaves, "list", "saveMenus");
	add(_txtDetails, "text", "saveMenus");
	add(_sortName, "text", "saveMenus");
	add(_sortDate, "text", "saveMenus");

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&ListGamesState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&ListGamesState::btnCancelClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);

	_txtDelete->setAlign(ALIGN_CENTER);
	_txtDelete->setText(tr("STR_RIGHT_CLICK_TO_DELETE"));

	_txtName->setText(tr("STR_NAME"));

	_txtDate->setText(tr("STR_DATE"));

	_lstSaves->setColumns(3, 188, 60, 40);
	_lstSaves->setSelectable(true);
	_lstSaves->setBackground(_window);
	_lstSaves->setMargin(8);
	_lstSaves->onMouseOver((ActionHandler)&ListGamesState::lstSavesMouseOver);
	_lstSaves->onMouseOut((ActionHandler)&ListGamesState::lstSavesMouseOut);
	_lstSaves->onMousePress((ActionHandler)&ListGamesState::lstSavesPress);

	_txtDetails->setWordWrap(true);
	_txtDetails->setText(tr("STR_DETAILS").arg(""));

	_sortName->setX(_sortName->getX() + _txtName->getTextWidth() + 5);
	_sortName->onMouseClick((ActionHandler)&ListGamesState::sortNameClick);

	_sortDate->setX(_sortDate->getX() + _txtDate->getTextWidth() + 5);
	_sortDate->onMouseClick((ActionHandler)&ListGamesState::sortDateClick);

	updateArrows();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _lstSaves->agentEnableSaveListCells(firstValidRow == 1);
    for (Surface *w : std::vector<Surface *>{_window,_btnCancel,_txtTitle,_txtName,_txtDate,_txtDetails}) w->agentTrackPresentation();
#endif
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void ListGamesState::blit()
{
    _agentListPaintHealthy = false;
    const auto previousCatalog = _agentPaintCatalog;
    _agentPaintCatalog.clear();
    State::blit();
    std::vector<AgentOutfittingRow> rows;
    if (!_lstSaves->agentSaveListRows(rows) || _agentCatalogEpoch == UINT64_MAX) return;
    for (const auto &row : rows)
    {
        if (row.logicalIndex < _firstValidRow) continue;
        const size_t index = row.logicalIndex - _firstValidRow;
        if (index >= _saves.size()) return;
        _agentPaintCatalog.emplace_back(index, _saves[index].fileName);
    }
    if (!_agentCatalogEpoch || previousCatalog != _agentPaintCatalog) ++_agentCatalogEpoch;
    _agentListPaintHealthy = true;
}

bool ListGamesState::agentListRegion(const Surface *widget, const Observer::AgentUiRect &rect, bool observation) const
{
    using namespace Observer;
    const Surface *destination = _game->getScreen()->getSurface();
    if (!widget || !widget->agentPresentationPaintedTo(destination) || !agentWidgetVisible(widget) ||
        !agentRegionVisible(rect)) return false;
    const auto paint = agentFlattenPaintOrder(_surfaces);
    bool found = false;
    for (const Surface *later : paint)
    {
        if (later == widget) { found = true; continue; }
        if (!found || !later || !later->getVisible()) continue;
        if (rect.x < later->getX()+later->getWidth() && rect.x+rect.width > later->getX() &&
            rect.y < later->getY()+later->getHeight() && rect.y+rect.height > later->getY()) return false;
    }
    AgentUiState part; part.regions.push_back(rect);
    return found && (observation ? _game->agentNonCursorOverlayClear(part) : _game->agentOverlayClear(part));
}

bool ListGamesState::agentListButton(Observer::AgentUiState &out, TextButton *button, const char *action, const char *kind) const
{
    using namespace Observer;
    out.widgets.emplace_back(button,button->agentPresentationRevision());
    out.widgets.emplace_back(button,button->agentLabelRevision());
    const AgentUiRect r={button->getX(),button->getY(),button->getWidth(),button->getHeight()};
    if (!agentListRegion(button,r,false) || button->getText().empty() || button->getText().size()>1024) return false;
    out.actions.emplace_back(action,button->getText(),"visible_text",kind);
    out.regions.push_back(r); return true;
}

bool ListGamesState::agentListUiState(Observer::AgentUiState &out, const char *screen, bool &popupPresentation) const
{
    using namespace Observer;
    popupPresentation = false;
    if (!_game->isState(const_cast<ListGamesState*>(this)) || _modal || !agentPaintHealthy() ||
        !_window->agentPresentationPaintedTo(_game->getScreen()->getSurface())) return false;
    if (_window->agentPopupActive())
    {
        // A currently painted ordinary popup is presentation, not catalog authority.
        const AgentUiRect r={_window->getX(),_window->getY(),_window->getWidth(),_window->getHeight()};
        if (!agentWidgetVisible(_window) || !agentRegionVisible(r)) return false;
        bool found=false;
        for (const Surface *later:agentFlattenPaintOrder(_surfaces))
        {
            if(later==_window){found=true;continue;}
            // Exact native blit visibility: hidden popup children do not occlude.
            if(!found || !later || !later->agentBlitEnabled())continue;
            if(r.x<later->getX()+later->getWidth() && r.x+r.width>later->getX() &&
               r.y<later->getY()+later->getHeight() && r.y+r.height>later->getY())return false;
        }
        AgentUiState footprint;footprint.regions.push_back(r);
        if(!found || !_game->agentNonCursorOverlayClear(footprint))return false;
        out=AgentUiState();out.screen=screen;out.actor=this;
        out.widgets.emplace_back(_window,_window->agentPresentationRevision());
        out.visible="{\"title\":null,\"name_heading\":null,\"date_heading\":null,\"details\":null,\"rows\":[]}";
        popupPresentation=true;return true;
    }
    if (!_agentListPaintHealthy) return false;
    for (const auto &pair : _agentPaintCatalog)
        if (pair.first >= _saves.size() || _saves[pair.first].fileName != pair.second) return false;
    out=AgentUiState(); out.screen=screen; out.actor=this;
    out.context.push_back(_agentCatalogEpoch);
    out.widgets.emplace_back(_lstSaves,_lstSaves->agentListRevision());
    out.widgets.emplace_back(_lstSaves,_lstSaves->agentPresentationRevision());
    const auto caption=[&](Text *w) {
        out.widgets.emplace_back(w,w->agentPresentationRevision());
        out.widgets.emplace_back(w,w->agentTextRevision());
        const AgentUiRect rect={w->getX(),w->getY(),w->getWidth(),w->getHeight()};
        return agentListRegion(w,rect,true) && w->getText().size()<=1024 ? agentJsonString(w->getText()) : std::string("null");
    };
    out.visible="{\"title\":"+caption(_txtTitle)+",\"name_heading\":"+caption(_txtName)+
        ",\"date_heading\":"+caption(_txtDate)+",\"details\":"+caption(_txtDetails)+",\"rows\":[";
    std::vector<AgentOutfittingRow> rows;
    if (!_lstSaves->agentPresentationPaintedTo(_game->getScreen()->getSurface()) || !_lstSaves->agentSaveListRows(rows)) return false;
    for (const auto &row : rows)
    {
        bool hasName=false; std::string cells="[";
        for (size_t c=0;c<row.cells.size();++c)
        {
            const auto &cell=row.cells[c]; bool known=cell.known && !cell.regions.empty();
            for (const auto &r:cell.regions)
                known=known && agentListRegion(_lstSaves,{r.x,r.y,r.w,r.h},true);
            if(c) cells+=",";
            cells+=known ? agentJsonString(cell.text) : "null";
            if(c==0) hasName=known;
        }
        cells+="]";
        if (!hasName) continue; // No target from an entirely occluded/absent caption.
        if(!out.rows.empty())out.visible+=",";
        out.visible+="{\"handle\":"+agentJsonString("r"+std::to_string(out.rows.size()))+",\"cells\":"+cells+"}";
        out.rows.push_back(row.logicalIndex);
    }
    out.visible+="]}";
    agentListButton(out,_btnCancel,"cancel_menu");
    return agentRegionsDisjoint(out);
}
#endif

/**
 *
 */
ListGamesState::~ListGamesState()
{

}

/**
 * Refreshes the saves list.
 */
void ListGamesState::init()
{
	State::init();

	if (_origin == OPT_BATTLESCAPE)
	{
		applyBattlescapeTheme();
	}

	try
	{
		_saves = SavedGame::getList(_game->getLanguage(), _autoquick);
		_lstSaves->clearList();
		sortList(Options::saveOrder);
	}
	catch (Exception &e)
	{
		Log(LOG_ERROR) << e.what();
	}
}

/**
 * Updates the sorting arrows based
 * on the current setting.
 */
void ListGamesState::updateArrows()
{
	_sortName->setShape(ARROW_NONE);
	_sortDate->setShape(ARROW_NONE);
	switch (Options::saveOrder)
	{
	case SORT_NAME_ASC:
		_sortName->setShape(ARROW_SMALL_UP);
		break;
	case SORT_NAME_DESC:
		_sortName->setShape(ARROW_SMALL_DOWN);
		break;
	case SORT_DATE_ASC:
		_sortDate->setShape(ARROW_SMALL_UP);
		break;
	case SORT_DATE_DESC:
		_sortDate->setShape(ARROW_SMALL_DOWN);
		break;
	}
}

/**
 * Sorts the save game list.
 * @param sort Order to sort the games in.
 */
void ListGamesState::sortList(SaveSort sort)
{
	switch (sort)
	{
	case SORT_NAME_ASC:
		std::sort(_saves.begin(), _saves.end(), compareSaveName(false));
		break;
	case SORT_NAME_DESC:
		std::sort(_saves.rbegin(), _saves.rend(), compareSaveName(true));
		break;
	case SORT_DATE_ASC:
		std::sort(_saves.begin(), _saves.end(), compareSaveTimestamp(false));
		break;
	case SORT_DATE_DESC:
		std::sort(_saves.rbegin(), _saves.rend(), compareSaveTimestamp(true));
		break;
	}
	updateList();
}

/**
 * Updates the save game list with the current list
 * of available savegames.
 */
void ListGamesState::updateList()
{
	int row = 0;
	int color = _lstSaves->getSecondaryColor();
	for (std::vector<SaveInfo>::const_iterator i = _saves.begin(); i != _saves.end(); ++i)
	{
		_lstSaves->addRow(3, i->displayName.c_str(), i->isoDate.c_str(), i->isoTime.c_str());
		if (i->reserved && _origin != OPT_BATTLESCAPE)
		{
			_lstSaves->setRowColor(row, color);
		}
		row++;
	}
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ListGamesState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Shows the details of the currently hovered save.
 * @param action Pointer to an action.
 */
void ListGamesState::lstSavesMouseOver(Action *)
{
	int sel = _lstSaves->getSelectedRow() - _firstValidRow;
	std::string wstr;
	if (sel >= 0 && sel < (int)_saves.size())
	{
		wstr = _saves[sel].details;
	}
	_txtDetails->setText(tr("STR_DETAILS").arg(wstr));
}

/**
 * Clears the details.
 * @param action Pointer to an action.
 */
void ListGamesState::lstSavesMouseOut(Action *)
{
	_txtDetails->setText(tr("STR_DETAILS").arg(""));
}

/**
 * Deletes the selected save.
 * @param action Pointer to an action.
 */
void ListGamesState::lstSavesPress(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT && _lstSaves->getSelectedRow() >= _firstValidRow)
	{
		_game->pushState(new DeleteGameState(_origin, _saves[_lstSaves->getSelectedRow() - _firstValidRow].fileName));
	}
}

/**
 * Sorts the saves by name.
 * @param action Pointer to an action.
 */
void ListGamesState::sortNameClick(Action *)
{
	if (_sortable)
	{
		if (Options::saveOrder == SORT_NAME_ASC)
		{
			Options::saveOrder = SORT_NAME_DESC;
		}
		else
		{
			Options::saveOrder = SORT_NAME_ASC;
		}
		updateArrows();
		_lstSaves->clearList();
		sortList(Options::saveOrder);
	}
}

/**
 * Sorts the saves by date.
 * @param action Pointer to an action.
 */
void ListGamesState::sortDateClick(Action *)
{
	if (_sortable)
	{
		if (Options::saveOrder == SORT_DATE_ASC)
		{
			Options::saveOrder = SORT_DATE_DESC;
		}
		else
		{
			Options::saveOrder = SORT_DATE_ASC;
		}
		updateArrows();
		_lstSaves->clearList();
		sortList(Options::saveOrder);
	}
}

void ListGamesState::disableSort()
{
	_sortable = false;
}

}
