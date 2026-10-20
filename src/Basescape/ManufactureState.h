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
class TextList;
class Base;
class Production;

/**
 * Manufacture screen that lets the player manage
 * all the manufacturing operations of a base.
 */
class ManufactureState : public State
{
private:
	void selectProduction(size_t row);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	std::uint64_t _agentMappingRevision = 0;
	std::vector<Production *> _agentProjects;
#endif
	Base *_base;
	TextButton *_btnNew, *_btnOk;
	Window *_window;
	Text *_txtTitle, *_txtAvailable, *_txtAllocated, *_txtSpace, *_txtFunds, *_txtItem, *_txtEngineers, *_txtProduced, *_txtCost, *_txtTimeLeft;
	TextList *_lstManufacture;
	void lstManufactureClick(Action * action);
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	bool agentUiState(Observer::AgentUiState &out) const override;
	bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the Manufacture state.
	ManufactureState(Base *base);
	/// Cleans up the Manufacture state.
	~ManufactureState();
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Updates the production list.
	void init();
	/// Handler for the New Production button.
	void btnNewProductionClick(Action * action);
	/// Fills the list of base productions.
	void fillProductionList();
};

}
