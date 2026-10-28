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
#include <vector>
#include "../Engine/State.h"

namespace OpenXcom
{

class Base;
class Craft;
class CraftWeapon;
class TextButton;
class Window;
class Text;
class TextList;
class RuleCraftWeapon;

/**
 * Select Armament window for
 * changing the weapon equipped on a craft.
 */
class CraftWeaponsState : public State
{
private:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull=false;
    std::uint64_t _agentUiRevision=0;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    Craft *_agentCraftBinding=nullptr;
    CraftWeapon *_agentSlotWeaponBinding=nullptr;
    int _agentSlotAmmoBinding=0;
    std::vector<int> _agentStockBinding;
    bool agentBindingsCurrent() const;
#endif
	Base *_base;
	size_t _craft, _weapon;

	TextButton *_btnCancel;
	Window *_window;
	Text *_txtTitle, *_txtArmament, *_txtQuantity, *_txtAmmunition;
	TextList *_lstWeapons;
	std::vector<RuleCraftWeapon*> _weapons;
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the Craft Weapons state.
	CraftWeaponsState(Base *base, size_t craft, size_t weapon);
	/// Cleans up the Craft Weapons state.
	~CraftWeaponsState();
	/// Handler for clicking the Cancel button.
	void btnCancelClick(Action *action);
	/// Handler for clicking the Weapons list.
	void lstWeaponsClick(Action *action);
};

}
