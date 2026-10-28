#pragma once
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
 * along with OpenXcom.  If not, see <http:///www.gnu.org/licenses/>.
 */
#include "../Engine/State.h"

namespace OpenXcom
{

class Base;
class Window;
class Text;
class TextButton;
class Globe;

/**
 * Screen that allows the player
 * to confirm a new base on the globe.
 * Note: This is different from the starting base screen, BaseNameState
 */
class ConfirmNewBaseState : public State
{
private:
	Base *_base;
	Globe *_globe;
	Window *_window;
	Text *_txtCost, *_txtArea;
	TextButton *_btnOk, *_btnCancel;
	int _cost;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull = false;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the Confirm New Base state.
	ConfirmNewBaseState(Base *base, Globe *globe);
	/// Cleans up the Confirm New Base state.
	~ConfirmNewBaseState();
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action *action);
};

}
