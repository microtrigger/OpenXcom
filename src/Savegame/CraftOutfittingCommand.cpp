/*
 * Copyright 2010-2016 OpenXcom Developers.
 * Extracted from the ordinary CraftSoldiersState and CraftEquipmentState rules.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "CraftOutfittingCommand.h"
#include "Base.h"
#include "Craft.h"
#include "Soldier.h"
#include "ItemContainer.h"
#include "Vehicle.h"
#include "SavedGame.h"
#include "../Mod/Mod.h"
#include "../Mod/Armor.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleItem.h"
#include <algorithm>
#include <climits>

namespace OpenXcom { namespace CraftOutfittingCommand {
CraftCrewChange setAssigned(Craft &craft, Soldier &soldier, bool assigned)
{
    if (assigned == (soldier.getCraft() == &craft)) return CraftCrewChange::Unchanged;
    if (!assigned)
    {
        soldier.setCraft(0);
        return CraftCrewChange::Removed;
    }
    if (soldier.getCraft() && soldier.getCraft()->getStatus() == "STR_OUT")
        return CraftCrewChange::OtherCraftAway;
    if (craft.getSpaceAvailable() > 0 && soldier.getWoundRecovery() == 0)
    {
        soldier.setCraft(&craft);
        return CraftCrewChange::Assigned;
    }
    return CraftCrewChange::Refused;
}

static bool decrease(Base &base, Craft &craft, Mod &mod, SavedGame &save,
    const std::string &itemId, int &totalItems, int change)
{
	Craft *c = &craft;
	RuleItem *item = mod.getItem(itemId, true);
	int cQty = 0;
	if (item->isFixed()) cQty = c->getVehicleCount(itemId);
	else cQty = c->getItems()->getItem(itemId);
	if (change <= 0 || cQty <= 0) return false;
	change = std::min(cQty, change);
	// Convert vehicle to item
	if (item->isFixed())
	{
		if (!item->getCompatibleAmmo()->empty())
		{
			// Calculate how much ammo needs to be added to the base.
			RuleItem *ammo = mod.getItem(item->getCompatibleAmmo()->front(), true);
			int ammoPerVehicle;
			if (ammo->getClipSize() > 0 && item->getClipSize() > 0)
			{
				ammoPerVehicle = item->getClipSize() / ammo->getClipSize();
			}
			else
			{
				ammoPerVehicle = ammo->getClipSize();
			}
			// Put the vehicles and their ammo back as separate items.
			if (save.getMonthsPassed() != -1)
			{
				base.getStorageItems()->addItem(itemId, change);
				base.getStorageItems()->addItem(ammo->getType(), ammoPerVehicle * change);
			}
			// now delete the vehicles from the craft.
			for (std::vector<Vehicle*>::iterator i = c->getVehicles()->begin(); i != c->getVehicles()->end() && change > 0; )
			{
				if ((*i)->getRules() == item)
				{
					delete (*i);
					i = c->getVehicles()->erase(i);
					--change;
				}
				else ++i;
			}
		}
		else
		{
			if (save.getMonthsPassed() != -1)
			{
				base.getStorageItems()->addItem(itemId, change);
			}
			for (std::vector<Vehicle*>::iterator i = c->getVehicles()->begin(); i != c->getVehicles()->end() && change > 0; )
			{
				if ((*i)->getRules() == item)
				{
					delete (*i);
					i = c->getVehicles()->erase(i);
					--change;
				}
				else ++i;
			}
		}
	}
	else
	{
		c->getItems()->removeItem(itemId, change);
		totalItems -= change;
		if (save.getMonthsPassed() > -1)
		{
			base.getStorageItems()->addItem(itemId, change);
		}
	}
	return true;
}

static bool increase(Base &base, Craft &craft, Mod &mod, SavedGame &save,
    const std::string &itemId, int &totalItems, int change, const CraftEquipmentWarningSink &warning)
{
	Craft *c = &craft;
	RuleItem *item = mod.getItem(itemId, true);
	int bqty = base.getStorageItems()->getItem(itemId);
	if (save.getMonthsPassed() == -1)
	{
		if (change == INT_MAX)
		{
			change = 10;
		}
		bqty = change;
	}
	if (0 >= change || 0 >= bqty) return false;
	change = std::min(bqty, change);
	// Do we need to convert item to vehicle?
	if (item->isFixed())
	{
		int size = 4;
		if (mod.getUnit(item->getType()))
		{
			size = mod.getArmor(mod.getUnit(item->getType())->getArmor(), true)->getSize();
			size *= size;
		}
		// Check if there's enough room
		int room = std::min(c->getRules()->getVehicles() - c->getNumVehicles(), c->getSpaceAvailable() / size);
		if (room > 0)
		{
			change = std::min(room, change);
			if (!item->getCompatibleAmmo()->empty())
			{
				// And now let's see if we can add the total number of vehicles.
				RuleItem *ammo = mod.getItem(item->getCompatibleAmmo()->front(), true);
				int ammoPerVehicle, clipSize;
				if (ammo->getClipSize() > 0 && item->getClipSize() > 0)
				{
					clipSize = item->getClipSize();
					ammoPerVehicle = clipSize / ammo->getClipSize();
				}
				else
				{
					clipSize = ammo->getClipSize();
					ammoPerVehicle = clipSize;
				}

				int baseQty = base.getStorageItems()->getItem(ammo->getType()) / ammoPerVehicle;
				if (save.getMonthsPassed() == -1)
					baseQty = change;
				int canBeAdded = std::min(change, baseQty);
				if (canBeAdded > 0)
				{
					for (int i = 0; i < canBeAdded; ++i)
					{
						if (save.getMonthsPassed() != -1)
						{
							base.getStorageItems()->removeItem(ammo->getType(), ammoPerVehicle);
							base.getStorageItems()->removeItem(itemId);
						}
						c->getVehicles()->push_back(new Vehicle(item, clipSize, size));
					}
				}
				else
				{
					// So we haven't managed to increase the count of vehicles because of the ammo
					warning(CraftEquipmentWarning::NotEnoughAmmo, ammoPerVehicle, ammo->getType());
				}
			}
			else
				for (int i = 0; i < change; ++i)
				{
					c->getVehicles()->push_back(new Vehicle(item, item->getClipSize(), size));
					if (save.getMonthsPassed() != -1)
					{
						base.getStorageItems()->removeItem(itemId);
					}
				}
		}
	}
	else
	{
		if (c->getRules()->getMaxItems() > 0 && totalItems + change > c->getRules()->getMaxItems())
		{
			warning(CraftEquipmentWarning::MaxItems, c->getRules()->getMaxItems(), std::string());
			change = c->getRules()->getMaxItems() - totalItems;
		}
		c->getItems()->addItem(itemId,change);
		totalItems += change;
		if (save.getMonthsPassed() > -1)
		{
			base.getStorageItems()->removeItem(itemId,change);
		}
	}
	return true;
}

bool quantity(Base &base, Craft &craft, Mod &mod, SavedGame &save,
    const std::string &item, int &totalItems, QuantityOperation operation,
    int value, const CraftEquipmentWarningSink &warning)
{
    switch (operation)
    {
    case QuantityOperation::Maximum:
        return increase(base, craft, mod, save, item, totalItems, INT_MAX, warning);
    case QuantityOperation::Clear:
        return decrease(base, craft, mod, save, item, totalItems, INT_MAX);
    case QuantityOperation::Set:
    {
        if (value < 0) return false;
        RuleItem *rule = mod.getItem(item, true);
        const int current = rule->isFixed() ? craft.getVehicleCount(item) : craft.getItems()->getItem(item);
        // Ordinary stock counts are nonnegative ints. Use a wide local delta so
        // constructing a semantic request never adds signed-overflow reachability.
        const long long delta = static_cast<long long>(value) - current;
        if (delta > 0) return increase(base, craft, mod, save, item, totalItems,
            static_cast<int>(std::min<long long>(INT_MAX, delta)), warning);
        if (delta < 0) return decrease(base, craft, mod, save, item, totalItems,
            static_cast<int>(std::min<long long>(INT_MAX, -delta)));
        return false;
    }
    case QuantityOperation::Adjust:
        if (value > 0) return increase(base, craft, mod, save, item, totalItems, value, warning);
        if (value < 0) return decrease(base, craft, mod, save, item, totalItems,
            value == INT_MIN ? INT_MAX : -value);
        return false;
    }
    return false;
}
} }
