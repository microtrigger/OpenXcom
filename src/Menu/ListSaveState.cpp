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
#include "ListSaveState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Interface/Text.h"
#endif
#include "../Engine/CrossPlatform.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Unicode.h"
#include "../Engine/Options.h"
#include "../Interface/TextList.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextButton.h"
#include "SaveGameState.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Save Game screen.
 * @param game Pointer to the core game.
 * @param origin Game section that originated this state.
 */
ListSaveState::ListSaveState(OptionsOrigin origin) : ListGamesState(origin, 1, false), _previousSelectedRow(-1), _selectedRow(-1)
{
	// Create objects
	_edtSave = new TextEdit(this, 168, 9, 0, 0);
	_btnSaveGame = new TextButton(_game->getSavedGame()->isIronman() ? 200 : 80, 16, 60, 172);

	add(_edtSave);
	add(_btnSaveGame, "button", "saveMenus");

	// Set up objects
	_txtTitle->setText(tr("STR_SELECT_SAVE_POSITION"));

	if (_game->getSavedGame()->isIronman())
	{
		_btnCancel->setVisible(false);
	}
	else
	{
		_btnCancel->setX(180);
	}

	_btnSaveGame->setText(tr("STR_SAVE_GAME"));
	_btnSaveGame->onMouseClick((ActionHandler)&ListSaveState::btnSaveGameClick);

	_edtSave->setColor(_lstSaves->getSecondaryColor());
	_edtSave->setVisible(false);
	_edtSave->onKeyboardPress((ActionHandler)&ListSaveState::edtSaveKeyPress);

	centerAllSurfaces();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _edtSave->agentEnableSaveName(); _btnSaveGame->agentTrackPresentation();
#endif
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool ListSaveState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    bool popupPresentation=false;
    if (!agentListUiState(out,"save_list",popupPresentation)) return false;
    if(popupPresentation)
    {out.visible.pop_back();out.visible+=",\"new_save_name\":null}";return true;}
    out.context.push_back(static_cast<std::uint64_t>(_selectedRow+1));
    out.widgets.emplace_back(_edtSave,_edtSave->agentInputRevision());
    out.widgets.emplace_back(_edtSave,_edtSave->agentPresentationRevision());
    std::string name="null";
    // Hidden Cancel is the native ironman presentation; do not expose manual save.
    if (_btnCancel->getVisible())
    {
        if (_selectedRow == -1)
        {
            for(size_t n=0;n<out.rows.size();++n) if(out.rows[n]==0)
            {
                std::vector<AgentOutfittingRow> rows;
                if(!_lstSaves->agentSaveListRows(rows))return false;
                for(const auto &row:rows)if(row.logicalIndex==0 && !row.cells.empty() && row.cells[0].known)
                {
                    const AgentUiRect rect={row.bounds.x,row.bounds.y,row.bounds.w,row.bounds.h};
                    if(agentListRegion(_lstSaves,rect,false))
                    { out.actions.emplace_back("select_new_save_slot",row.cells[0].text,"visible_text");out.regions.push_back(rect); }
                }
            }
        }
        if (_selectedRow == 0 && Options::keyboardMode == KEYBOARD_ON)
        {
            std::string shown; SDL_Rect rect={}; std::uint64_t epoch=0;
            if(_edtSave->agentSaveName(_game->getScreen()->getSurface(),shown,rect,epoch) &&
                agentListRegion(_edtSave,{rect.x,rect.y,rect.w,rect.h},true))
            {
                if(!_edtSave->agentSaveNameBinding(_game->getScreen()->getSurface(),out))return false;
                name=agentJsonString(shown);
                if(agentListRegion(_edtSave,{rect.x,rect.y,rect.w,rect.h},false))
                {
                    out.actions.emplace_back("replace_new_save_name","Replace save name","visible_control_semantics","text");
                    out.regions.push_back({rect.x,rect.y,rect.w,rect.h});
                }
                if(!shown.empty() && shown.size()<=128 && shown.find_first_of("/\\") == std::string::npos)
                    agentListButton(out,_btnSaveGame,"save_new_game");
            }
        }
    }
    out.visible.pop_back(); out.visible+=",\"new_save_name\":"+name+"}";
    return agentRegionsDisjoint(out);
}

bool ListSaveState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if(!agentUiState(current) || !current.offers(request.action))return false;
    if(request.action=="cancel_menu"){btnCancelClick(nullptr);return true;}
    if(request.action=="select_new_save_slot")
    {
        if(_selectedRow!=-1 || !_lstSaves->agentSelectSaveListRow(0))return false;
        SDL_Event event={};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;
        Action action(&event,1.,1.,0,0);action.setSender(_lstSaves);lstSavesPress(&action);return true;
    }
    if(_selectedRow!=0)return false;
    if(request.action=="replace_new_save_name")
    {
        if(request.text.size()>128 || request.text.find_first_of("/\\")!=std::string::npos)return false;
        return _edtSave->agentReplaceSaveName(_game->getScreen()->getSurface(), request.text);
    }
    if(request.action=="save_new_game"){btnSaveGameClick(nullptr);return true;}
    return false;
}
#endif

/**
 *
 */
ListSaveState::~ListSaveState()
{

}

/**
 * Updates the save game list with the current list
 * of available savegames.
 */
void ListSaveState::updateList()
{
	_lstSaves->addRow(1, tr("STR_NEW_SAVED_GAME_SLOT").c_str());
	if (_origin != OPT_BATTLESCAPE)
		_lstSaves->setRowColor(0, _lstSaves->getSecondaryColor());
	ListGamesState::updateList();
}

/**
 * Names the selected save.
 * @param action Pointer to an action.
 */
void ListSaveState::lstSavesPress(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT && _edtSave->isFocused())
	{
		_edtSave->setText("");
		_edtSave->setVisible(false);
		_edtSave->setFocus(false, false);
		_lstSaves->setScrolling(true);
	}
	ListGamesState::lstSavesPress(action);
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_previousSelectedRow = _selectedRow;
		_selectedRow = _lstSaves->getSelectedRow();

		switch (_previousSelectedRow)
		{
			case -1:	// first click on the savegame list
				break;
			case 0:
				_lstSaves->setCellText(_previousSelectedRow	, 0, tr("STR_NEW_SAVED_GAME_SLOT"));
				break;
			default:
				_lstSaves->setCellText(_previousSelectedRow	, 0, _selected);
		}

		_selected = _lstSaves->getCellText(_lstSaves->getSelectedRow(), 0);
		_lstSaves->setCellText(_lstSaves->getSelectedRow(), 0, "");
		if (_lstSaves->getSelectedRow() == 0)
		{
			_edtSave->setText("");
			_selected = "";
		}
		else
		{
			_edtSave->setText(_selected);
		}
		_edtSave->setX(_lstSaves->getColumnX(0));
		_edtSave->setY(_lstSaves->getRowY(_selectedRow));
		_edtSave->setVisible(true);
		_edtSave->setFocus(true, false);
		_lstSaves->setScrolling(false);
		ListGamesState::disableSort();
	}
}

/**
 * Saves the selected save.
 * @param action Pointer to an action.
 */
void ListSaveState::edtSaveKeyPress(Action *action)
{
	if (action->getDetails()->key.keysym.sym == SDLK_RETURN ||
		action->getDetails()->key.keysym.sym == SDLK_KP_ENTER)
	{
		saveGame();
	}
}

/**
 * Saves the selected save.
 * @param action Pointer to an action.
 */
void ListSaveState::btnSaveGameClick(Action *)
{
	if (_selectedRow != -1)
	{
		saveGame();
	}
}

/**
 * Saves the selected save.
 */
void ListSaveState::saveGame()
{
	_game->getSavedGame()->setName(_edtSave->getText());
	std::string oldFilename, newFilename;
	newFilename = CrossPlatform::sanitizeFilename(Unicode::convUtf8ToPath(_edtSave->getText()));
	if (_selectedRow > 0)
	{
		oldFilename = _saves[_selectedRow - 1].fileName;
		if (oldFilename != newFilename + ".sav")
		{
			while (CrossPlatform::fileExists(Options::getMasterUserFolder() + newFilename + ".sav"))
			{
				newFilename += "_";
			}
			std::string oldPath = Options::getMasterUserFolder() + oldFilename;
			std::string newPath = Options::getMasterUserFolder() + newFilename + ".sav";
			CrossPlatform::moveFile(oldPath, newPath);
		}
	}
	else
	{
		while (CrossPlatform::fileExists(Options::getMasterUserFolder() + newFilename + ".sav"))
		{
			newFilename += "_";
		}
	}
	newFilename += ".sav";
	_game->pushState(new SaveGameState(_origin, newFilename, _palette));
}

}
