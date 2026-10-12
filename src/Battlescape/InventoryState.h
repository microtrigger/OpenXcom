// Fork modification 2026-10-12: default-off capless presented inventory view; GPL-3.0-or-later.
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
#include <cstdint>
#include "../Interface/TextButton.h"
#include "../Savegame/EquipmentLayoutItem.h"


namespace OpenXcom
{

class Surface;
class Text;
class InteractiveSurface;
class Inventory;
class SavedBattleGame;
class BattlescapeState;
class BattleUnit;
class BattlescapeButton;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
namespace Observer { class StanceController; class InventoryCloseOverlay; struct InventoryCloseContext; }
#endif

/**
 * Screen which displays soldier's inventory.
 */
class InventoryState : public State
{
private:
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    friend class Observer::StanceController;
    bool _inventoryInitialized = false;
    bool inventoryViewAllowed() const;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE
    friend struct InventoryCloseGateAccess;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    friend struct InventoryClickGateAccess;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    friend struct InventoryGroundGateAccess;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE
    Observer::InventoryCloseOverlay *_inventoryCloseOverlay = 0;
    std::uint64_t _inventoryUiRevision = 0;
    BattleUnit *_inventoryCloseShownActor = 0;
    bool _inventoryCloseConsumeEvent = false;
    bool inventoryCloseContext(Observer::InventoryCloseContext &context, bool overlay = false) const;
    bool inventoryControlContext(Observer::InventoryCloseContext &context, bool overlay, bool allowHeld) const;
    bool showInventoryCloseOverlay(Observer::StanceController *controller);
    void dismissInventoryCloseOverlay();
    bool inventoryCloseOverlayOwned() const;
    bool inventoryCloseOverlayActive() const;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND
    bool inventoryGroundControlVisible() const;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK
    // Exact owned-down tails survive proposal retirement, never state lifetime.
    std::uint8_t _inventoryClickConsumedButtons = 0;
    friend class Observer::InventoryCloseOverlay;
    bool inventoryClickContext(Observer::InventoryCloseContext &context, bool overlay = false) const;
    int inventoryClickBannerY() const;
    void inventoryControlClick(int x, int y);
#endif
	Surface *_bg, *_soldier;
	Text *_txtName, *_txtItem, *_txtAmmo, *_txtWeight, *_txtTus, *_txtFAcc, *_txtReact, *_txtPSkill, *_txtPStr;
	BattlescapeButton *_btnOk, *_btnPrev, *_btnNext, *_btnUnload, *_btnGround, *_btnRank;
	BattlescapeButton *_btnCreateTemplate, *_btnApplyTemplate;
	Surface *_selAmmo;
	Inventory *_inv;
	std::vector<EquipmentLayoutItem*> _curInventoryTemplate;
	SavedBattleGame *_battleGame;
	const bool _tu;
	BattlescapeState *_parent;
	std::string _currentTooltip;
public:
	/// Creates the Inventory state.
	InventoryState(bool tu, BattlescapeState *parent);
	/// Cleans up the Inventory state.
	~InventoryState();
	/// Updates all soldier info.
	void init();
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    /// Observe only after the ordinary inventory has actually blitted.
    void blit() override;
#endif
	/// Updates the soldier info (Weight, TU).
	void updateStats();
	/// Saves the soldiers' equipment-layout.
	void saveEquipmentLayout();
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Previous button.
	void btnPrevClick(Action *action);
	/// Handler for clicking the Next button.
	void btnNextClick(Action *action);
	/// Handler for clicking the Unload button.
	void btnUnloadClick(Action *action);
	/// Handler for clicking on the Ground -> button.
	void btnGroundClick(Action *action);
	/// Handler for clicking the Rank button.
	void btnRankClick(Action *action);
	/// Handler for clicking on the Create Template button.
	void btnCreateTemplateClick(Action *action);
	/// Handler for clicking the Apply Template button.
	void btnApplyTemplateClick(Action *action);
	/// Handler for hitting the Clear Inventory hotkey.
	void onClearInventory(Action *action);
	/// Handler for hitting the Autoequip hotkey.
	void onAutoequip(Action *action);
	/// Handler for clicking on the inventory.
	void invClick(Action *action);
	/// Handler for showing item info.
	void invMouseOver(Action *action);
	/// Handler for hiding item info.
	void invMouseOut(Action *action);
	/// Handles keypresses.
	void handle(Action *action);
	/// Handler for showing tooltip.
	void txtTooltipIn(Action *action);
	/// Handler for hiding tooltip.
	void txtTooltipOut(Action *action);

private:
	/// Update the visibility and icons for the template buttons
	void _updateTemplateButtons(bool isVisible);
	/// Refresh the hover status of the mouse
	void _refreshMouse();
};

}
