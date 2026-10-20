// SPDX-License-Identifier: GPL-3.0-or-later
// Shared ordinary craft-outfitting rules, 2026-10-19.
#pragma once
#include "QuantityCommand.h"
#include <functional>
#include <string>
namespace OpenXcom {
class Base;
class Craft;
class Soldier;
class Mod;
class SavedGame;
// Private business results; never part of an observer or public-state DTO.
enum class CraftCrewChange { Unchanged, Removed, Assigned, OtherCraftAway, Refused };
enum class CraftEquipmentWarning { MaxItems, NotEnoughAmmo };
// The UI supplies presentation. The core invokes this at the original warning
// point, before any capacity-limited partial item addition.
typedef std::function<void(CraftEquipmentWarning, int, const std::string &)> CraftEquipmentWarningSink;
namespace CraftOutfittingCommand {
CraftCrewChange setAssigned(Craft &craft, Soldier &soldier, bool assigned);
// Return whether the ordinary transfer reached its quantity-display refresh.
// A false result is an ordinary no-effect, not an eligibility/public outcome.
bool quantity(Base &base, Craft &craft, Mod &mod, SavedGame &save,
    const std::string &item, int &totalItems, QuantityOperation operation,
    int value, const CraftEquipmentWarningSink &warning);
}
}
