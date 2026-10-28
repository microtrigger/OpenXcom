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
#include "../Engine/State.h"

namespace OpenXcom
{

class Window;
class Text;
class TextButton;
class SavedBattleGame;
class BattlescapeState;

/**
 * Screen which asks for confirmation to abort mission.
 */
class AbortMissionState : public State
{
private:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull = false;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif
	Window *_window;
	Text *_txtInEntrance, *_txtInExit, *_txtOutside, *_txtAbort;
	TextButton *_btnOk, *_btnCancel;
	SavedBattleGame *_battleGame;
	BattlescapeState *_state;
	int _inEntrance, _inExit, _outside;
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the Abort Mission state.
	AbortMissionState(SavedBattleGame *battleGame, BattlescapeState *state);
	/// Cleans up the Abort Mission state.
	~AbortMissionState();
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action *action);

};

}
