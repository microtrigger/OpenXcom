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
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include <memory>
#endif

namespace OpenXcom
{

class Window;
class Text;
class TextButton;
class Craft;
class Target;
class Texture;

/**
 * Window that allows the player
 * to confirm a craft landing at its destination.
 */
class ConfirmLandingState : public State
{
private:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    Target *_agentLandingDestination = nullptr; // Private constructor operand identity; never serialized.
    std::weak_ptr<const void> _agentLandingLifetime;
    bool _agentDestinationPaintFull = false;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif
	Craft *_craft;
	Window *_window;
	Texture *_texture;
	int _shade;
	Text *_txtMessage, *_txtBegin;
	TextButton *_btnYes, *_btnNo;
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the Confirm Landing state.
	ConfirmLandingState(Craft *craft, Texture *texture, int shade);
	/// Cleans up the Confirm Landing state.
	~ConfirmLandingState();
	/// initialize the state, make a sanity check.
	void init();
	/// Handler for clicking the Yes button.
	void btnYesClick(Action *action);
	/// Handler for clicking the No button.
	void btnNoClick(Action *action);
};

}
