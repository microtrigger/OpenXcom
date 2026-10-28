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

class Base;
class RuleBaseFacility;
class BaseView;
class TextButton;
class Window;
class Text;

/**
 * Window shown when the player tries to
 * build a facility.
 */
class PlaceFacilityState : public State
{
protected:
	Base *_base;
	RuleBaseFacility *_rule;

	BaseView *_view;
	TextButton *_btnCancel;
	Window *_window;
	Text *_txtFacility, *_txtCost, *_numCost, *_txtTime, *_numTime, *_txtMaintenance, *_numMaintenance;
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
	/// Creates the Place Facility state.
	PlaceFacilityState(Base *base, RuleBaseFacility *rule);
	/// Cleans up the Place Facility state.
	~PlaceFacilityState();
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action *action);
	/// Handler for clicking the base view.
	void viewClick(Action *action);
};

}
