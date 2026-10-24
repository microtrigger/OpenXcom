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

class TextButton;
class Window;
class Text;
class Craft;
class Globe;

/**
 * Window displayed when a craft
 * starts patrolling a waypoint.
 */
class CraftPatrolState : public State
{
private:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull = false;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif
	Craft *_craft;
	Globe *_globe;

	TextButton *_btnOk, *_btnRedirect;
	Window *_window;
	Text *_txtDestination, *_txtPatrolling;
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the Geoscape Craft state.
	CraftPatrolState(Craft *craft, Globe *globe);
	/// Cleans up the Geoscape Craft state.
	~CraftPatrolState();
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Redirect Craft button.
	void btnRedirectClick(Action *action);
};

}
