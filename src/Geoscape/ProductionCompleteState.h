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
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <string>
#include "../Engine/State.h"
#include "../Savegame/Production.h"

namespace OpenXcom
{

class TextButton;
class Window;
class Text;
class Base;
class GeoscapeState;

/**
 * Window used to notify the player when
 * a production is completed.
 */
class ProductionCompleteState : public State
{
private:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull = false;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif
	Base *_base;
	GeoscapeState *_state;

	TextButton *_btnOk, *_btnGotoBase;
	Window *_window;
	Text *_txtMessage;
	productionProgress_e _endType;
public:
	/// Creates the Production Complete state.
	ProductionCompleteState(Base *base, const std::string &item, GeoscapeState *state, productionProgress_e endType = PROGRESS_COMPLETE);
	/// Cleans up the Production Complete state.
	~ProductionCompleteState();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Go To Base button.
	void btnGotoBaseClick(Action *action);
};

}
