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

#include "UfopaediaStartState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentOutfitting.h"
#endif
#include "UfopaediaSelectState.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/ArrowButton.h"
#include "../Mod/Mod.h"
#include "../Engine/Timer.h"
#include "../fmath.h"

namespace OpenXcom
{
	const size_t UfopaediaStartState::CAT_MIN_BUTTONS = 9;
	const size_t UfopaediaStartState::CAT_MAX_BUTTONS = 10;

	UfopaediaStartState::UfopaediaStartState() : _offset(0), _scroll(0), _cats(_game->getMod()->getUfopaediaCategoryList())
	{
		_screen = false;

		// set background window
		_window = new Window(this, 256, 180, 32, 10, POPUP_BOTH);

		// set title
		_txtTitle = new Text(220, 17, 50, 33);

		// Set palette
		setInterface("ufopaedia");

		add(_window, "window", "ufopaedia");
		add(_txtTitle, "text", "ufopaedia");

		_btnOk = new TextButton(220, 12, 50, 167);
		add(_btnOk, "button1", "ufopaedia");

		// set buttons
		int y = 50;
		size_t numButtons = std::min(_cats.size(), CAT_MAX_BUTTONS);
		if (numButtons > CAT_MIN_BUTTONS)
			y -= 13 * (numButtons - CAT_MIN_BUTTONS);

		_btnScrollUp = new ArrowButton(ARROW_BIG_UP, 13, 14, 270, y);
		add(_btnScrollUp, "button1", "ufopaedia");
		_btnScrollDown = new ArrowButton(ARROW_BIG_DOWN, 13, 14, 270, 152);
		add(_btnScrollDown, "button1", "ufopaedia");

		for (size_t i = 0; i < numButtons; ++i)
		{
			TextButton *button = new TextButton(220, 12, 50, y);
			y += 13;

			add(button, "button1", "ufopaedia");

			button->onMouseClick((ActionHandler)&UfopaediaStartState::btnSectionClick);
			button->onMousePress((ActionHandler)&UfopaediaStartState::btnScrollUpClick, SDL_BUTTON_WHEELUP);
			button->onMousePress((ActionHandler)&UfopaediaStartState::btnScrollDownClick, SDL_BUTTON_WHEELDOWN);

			_btnSections.push_back(button);
		}
		updateButtons();
		if (!_btnSections.empty())
			_txtTitle->setY(_btnSections.front()->getY() - _txtTitle->getHeight());

		centerAllSurfaces();

		_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

		_txtTitle->setBig();
		_txtTitle->setAlign(ALIGN_CENTER);
		_txtTitle->setText(tr("STR_UFOPAEDIA"));

		_btnOk->setText(tr("STR_OK"));
		_btnOk->onMouseClick((ActionHandler)&UfopaediaStartState::btnOkClick);
		_btnOk->onKeyboardPress((ActionHandler)&UfopaediaStartState::btnOkClick, Options::keyCancel);
		_btnOk->onKeyboardPress((ActionHandler)&UfopaediaStartState::btnOkClick, Options::keyGeoUfopedia);

		_btnScrollUp->setVisible(_cats.size() > CAT_MAX_BUTTONS);
		_btnScrollUp->onMousePress((ActionHandler)&UfopaediaStartState::btnScrollUpPress);
		_btnScrollUp->onMouseRelease((ActionHandler)&UfopaediaStartState::btnScrollRelease);
		_btnScrollDown->setVisible(_cats.size() > CAT_MAX_BUTTONS);
		_btnScrollDown->onMousePress((ActionHandler)&UfopaediaStartState::btnScrollDownPress);
		_btnScrollDown->onMouseRelease((ActionHandler)&UfopaediaStartState::btnScrollRelease);

		_timerScroll = new Timer(50);
		_timerScroll->onTimer((StateHandler)&UfopaediaStartState::scroll);
	}

	/**
	 * Deletes timers.
	 */
	UfopaediaStartState::~UfopaediaStartState()
	{
		delete _timerScroll;
	}

	/**
	 * Run timers.
	 */
	void UfopaediaStartState::think()
	{
		State::think();
		_timerScroll->think(this, 0);
	}

	/**
	 * Returns to the previous screen.
	 * @param action Pointer to an action.
	 */
	void UfopaediaStartState::btnOkClick(Action *)
	{
		_game->popState();
	}

	/**
	 * Displays the list of articles for this section.
	 * @param action Pointer to an action.
	 */
	void UfopaediaStartState::btnSectionClick(Action *action)
	{
		for (size_t i = 0; i < _btnSections.size(); ++i)
		{
			if (action->getSender() == _btnSections[i])
			{
				_game->pushState(new UfopaediaSelectState(_cats[_offset + i]));
				break;
			}
		}
	}

	/**
	 * Starts scrolling the section buttons up.
	 * @param action Pointer to an action.
	 */
	void UfopaediaStartState::btnScrollUpPress(Action *)
	{
		_scroll = -1;
		_timerScroll->start();
	}

	/**
	 * Scrolls the section buttons up.
	 * @param action Pointer to an action.
	 */
	void UfopaediaStartState::btnScrollUpClick(Action *)
	{
		_scroll = -1;
		scroll();
	}

	/**
	 * Starts scrolling the section buttons down.
	 * @param action Pointer to an action.
	 */
	void UfopaediaStartState::btnScrollDownPress(Action *)
	{
		_scroll = 1;
		_timerScroll->start();
	}

	/**
	 * Scrolls the section buttons down.
	 * @param action Pointer to an action.
	 */
	void UfopaediaStartState::btnScrollDownClick(Action *)
	{
		_scroll = 1;
		scroll();
	}

	/**
	 * Stops scrolling the section buttons.
	 * @param action Pointer to an action.
	 */
	void UfopaediaStartState::btnScrollRelease(Action *)
	{
		_timerScroll->stop();
	}

	/**
	 * Offsets the list of section buttons.
	 */
	void UfopaediaStartState::scroll()
	{
		if (_cats.size() > CAT_MAX_BUTTONS)
		{
			_offset = Clamp(_offset + _scroll, 0, int(_cats.size() - CAT_MAX_BUTTONS));
			updateButtons();
		}
	}

	/**
	 * Updates the section button labels based on scroll.
	 */
	void UfopaediaStartState::updateButtons()
	{
		for (size_t i = 0; i < _btnSections.size(); ++i)
		{
			_btnSections[i]->setText(tr(_cats[_offset + i]));
		}
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void UfopaediaStartState::blit()
{
    _agentDestinationPaintFull=false;_agentStoredPaintRevisions.clear();
    const bool before=Observer::agentOutfittingDestinationFull(_game);
    State::blit();
    _agentStoredPaintRevisions=Observer::agentOutfittingPaintRevisions(_surfaces);
    _agentDestinationPaintFull=before && Observer::agentOutfittingDestinationFull(_game) && Observer::agentPaintHealthy();
}
bool UfopaediaStartState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<UfopaediaStartState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentOutfittingDestinationFull(_game) ||
        _agentStoredPaintRevisions!=agentOutfittingPaintRevisions(_surfaces)) return false;
    if (_offset<0 || size_t(_offset)>_cats.size() || _btnSections.size()>_cats.size()-size_t(_offset)) return false;
    out=AgentUiState();out.screen="ufopaedia_start";out.actor=this;out.save=_game->getSavedGame();out.uiRevision=_agentUiRevision;
    out.context.push_back(_offset);
    AgentOutfittingView view(_game,_surfaces,out);
    const auto title=view.text(_txtTitle);
    std::string rows="[";
    for (size_t i=0;i<_btnSections.size();++i)
    {
        TextButton *button=_btnSections[i];
        out.widgets.emplace_back(button,button->agentLabelRevision());
        const std::string label=agentButtonLabel(button);
        if (label.empty() || label.size()>1024 || !agentWidgetUncovered(button,view.painted) || !view.include(button)) continue;
        if (!out.rows.empty()) rows+=",";
        rows+="{\"handle\":"+agentJsonString("r"+std::to_string(out.rows.size()))+",\"cells\":["+agentJsonString(label)+"],\"selected\":false}";
        out.rows.push_back(i);
    }
    rows+="]";
    if (!out.rows.empty()) out.actions.emplace_back("select_ufopaedia_category","Select category","visible_control_semantics","outfitting_row");
    const bool up=_offset>0 && agentWidgetUncovered(_btnScrollUp,view.painted) && view.include(_btnScrollUp);
    const bool down=_cats.size()>CAT_MAX_BUTTONS && size_t(_offset)<_cats.size()-CAT_MAX_BUTTONS &&
        agentWidgetUncovered(_btnScrollDown,view.painted) && view.include(_btnScrollDown);
    out.context.push_back(up);out.context.push_back(down);
    if (up || down) out.actions.emplace_back("scroll_visible_list","Scroll visible list","visible_control_semantics","scroll_direction");
    view.button(_btnOk,"close","close_ufopaedia");
    out.visible="{\"title\":"+title+",\"rows\":"+rows+",\"scroll\":{\"up\":"+(up?"true":"false")+",\"down\":"+(down?"true":"false")+"},\"buttons\":"+view.buttons+"]}";
    return view.finish();
}
bool UfopaediaStartState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if(request.action=="close_ufopaedia") { btnOkClick(nullptr);return true; }
    if(request.action=="scroll_visible_list" && current.context.size()>=2)
    {
        const size_t n=current.context.size();
        if(request.choice=="up" && current.context[n-2]) { btnScrollUpClick(nullptr);btnScrollRelease(nullptr);return true; }
        if(request.choice=="down" && current.context[n-1]) { btnScrollDownClick(nullptr);btnScrollRelease(nullptr);return true; }
        return false;
    }
    size_t row=0;
    if(request.action=="select_ufopaedia_category" && Observer::agentOutfittingRow(current,request,row) && row<_btnSections.size())
    {
        SDL_Event event={};event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;
        Action action(&event,1.0,1.0,0,0);action.setSender(_btnSections[row]);
        btnSectionClick(&action);return true;
    }
    return false;
}
#endif

}
