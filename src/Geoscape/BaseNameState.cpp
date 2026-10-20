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
#include "BaseNameState.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextButton.h"
#include "../Savegame/Base.h"
#include "../Basescape/PlaceLiftState.h"
#include "../Engine/Options.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in a Base Name window.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to name.
 * @param globe Pointer to the Geoscape globe.
 * @param first Is this the first base in the game?
 */
BaseNameState::BaseNameState(Base *base, Globe *globe, bool first) : _base(base), _globe(globe), _first(first)
{
	_globe->onMouseOver(0);

	_screen = false;

	// Create objects
	_window = new Window(this, 192, 80, 32, 60, POPUP_BOTH);
	_btnOk = new TextButton(162, 12, 47, 118);
	_txtTitle = new Text(182, 17, 37, 70);
	_edtName = new TextEdit(this, 127, 16, 59, 94);

	// Set palette
	setInterface("baseNaming");

	add(_window, "window", "baseNaming");
	add(_btnOk, "button", "baseNaming");
	add(_txtTitle, "text", "baseNaming");
	add(_edtName, "text", "baseNaming");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&BaseNameState::btnOkClick);
	//_btnOk->onKeyboardPress((ActionHandler)&BaseNameState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&BaseNameState::btnOkClick, Options::keyCancel);

	//something must be in the name before it is acceptable
	_btnOk->setVisible(false);

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_BASE_NAME"));

	_edtName->setBig();
	_edtName->setFocus(true, false);
	_edtName->onChange((ActionHandler)&BaseNameState::edtNameChange);
}

/**
 *
 */
BaseNameState::~BaseNameState()
{

}

/**
 * Updates the base name and disables the OK button
 * if no name is entered.
 * @param action Pointer to an action.
 */
void BaseNameState::edtNameChange(Action *action)
{
	applyNameChange(action->getDetails()->key.keysym.sym == SDLK_RETURN ||
		action->getDetails()->key.keysym.sym == SDLK_KP_ENTER);
}

void BaseNameState::applyNameChange(bool commit)
{
	_base->setName(_edtName->getText());
	if (commit)
	{
		if (!_edtName->getText().empty()) btnOkClick(nullptr);
	}
	else
	{
		_btnOk->setVisible(!_edtName->getText().empty());
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool BaseNameState::agentUiState(Observer::AgentUiState &out) const
{
	// The owned editor is the only modal exception. No unrelated modal can
	// be bypassed, and a modal editor never grants direct button dispatch.
	if (_modal && (_modal != _edtName || !_edtName->isFocused())) return false;
	out = Observer::AgentUiState();
	out.screen = "base_name";
	out.uiRevision = _edtName->agentInputRevision();
	const std::string title = Observer::agentWidgetUncovered(_txtTitle, _surfaces) ?
		Observer::agentTextJson(_txtTitle) : "null";
	if (title != "null") Observer::agentIncludeWidget(out, _txtTitle);
	std::string name, nameJson = "null";
	if (Observer::agentWidgetUncovered(_edtName, _surfaces) && _edtName->agentRenderedText(name))
	{
		nameJson = Observer::agentJsonString(name);
		Observer::agentIncludeWidget(out, _edtName);
		if (_edtName->agentEditable())
			out.actions.push_back(Observer::AgentUiAction("replace_base_name", "Edit base name", "visible_control_semantics", "text"));
	}
	out.visible = "{\"title\":" + title + ",\"name\":" + nameJson + "}";
	if (!_modal && nameJson != "null" && Observer::agentWidgetUncovered(_btnOk, _surfaces))
	{
		const std::string label = Observer::agentButtonLabel(_btnOk);
		if (!label.empty())
		{
			out.actions.push_back(Observer::AgentUiAction("confirm_base_name", label, "visible_text"));
			Observer::agentIncludeWidget(out, _btnOk);
		}
	}
	return true;
}

bool BaseNameState::agentDispatch(const Observer::AgentRequest &request)
{
	const std::string &action = request.action;
	Observer::AgentUiState current;
	if (!agentUiState(current) || !current.offers(action)) return false;
	if (action == "replace_base_name")
	{
		if (!_edtName->agentReplaceText(request.text)) return false;
		applyNameChange(false);
		return true;
	}
	if (action == "confirm_base_name") { btnOkClick(nullptr); return true; }
	return false;
}
#endif

/**
 * Returns to the previous screen
 * @param action Pointer to an action.
 */
void BaseNameState::btnOkClick(Action *)
{
	if (!_edtName->getText().empty())
	{
		_game->popState();
		_game->popState();
		if (!_first || Options::customInitialBase)
		{
			if (!_first)
			{
				_game->popState();
			}
			_game->pushState(new PlaceLiftState(_base, _globe, _first));
		}
	}
}

}
