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
#include "PurchaseCommands.h"
#include "Base.h"
#include "SavedGame.h"
#include "Craft.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleSoldier.h"
#include "../fmath.h"
#include <algorithm>
#include <climits>

namespace OpenXcom
{
PurchaseWarning PurchaseCommands::increase(Base &base, SavedGame &save, TransferRow &row, PurchaseDraft draft, int change)
{
	if (0 >= change) return PurchaseWarning::None;
	PurchaseWarning warning = PurchaseWarning::None;

	if (draft.total + row.cost > save.getFunds())
	{
		warning = PurchaseWarning::Money;
	}
	else
	{
		RuleItem *rule = 0;
		switch (row.type)
		{
		case TRANSFER_SOLDIER:
		case TRANSFER_SCIENTIST:
		case TRANSFER_ENGINEER:
			if (draft.personnel + 1 > base.getAvailableQuarters() - base.getUsedQuarters())
			{
				warning = PurchaseWarning::Quarters;
			}
			break;
		case TRANSFER_CRAFT:
			if (draft.crafts + 1 > base.getAvailableHangars() - base.getUsedHangars())
			{
				warning = PurchaseWarning::Hangars;
			}
			break;
		case TRANSFER_ITEM:
			rule = (RuleItem*)row.rule;
			if (base.storesOverfull(draft.stores + rule->getSize()))
			{
				warning = PurchaseWarning::Stores;
			}
			break;
		}
	}

	if (warning == PurchaseWarning::None)
	{
		int maxByMoney = (save.getFunds() - draft.total) / row.cost;
		if (maxByMoney >= 0)
			change = std::min(maxByMoney, change);
		switch (row.type)
		{
		case TRANSFER_SOLDIER:
		case TRANSFER_SCIENTIST:
		case TRANSFER_ENGINEER:
			{
				int maxByQuarters = base.getAvailableQuarters() - base.getUsedQuarters() - draft.personnel;
				change = std::min(maxByQuarters, change);
				draft.personnel += change;
			}
			break;
		case TRANSFER_CRAFT:
			{
				int maxByHangars = base.getAvailableHangars() - base.getUsedHangars() - draft.crafts;
				change = std::min(maxByHangars, change);
				draft.crafts += change;
			}
			break;
		case TRANSFER_ITEM:
		{
			RuleItem *rule = (RuleItem*)row.rule;
			double storesNeededPerItem = rule->getSize();
			double freeStores = base.getAvailableStores() - base.getUsedStores() - draft.stores;
			double maxByStores = (double)(INT_MAX);
			if (!AreSame(storesNeededPerItem, 0.0))
			{
				maxByStores = (freeStores + 0.05) / storesNeededPerItem;
			}
			change = std::min((int)maxByStores, change);
			draft.stores += change * storesNeededPerItem;
		}
			break;
		}
		row.amount += change;
		draft.total += row.cost * change;
	}
	return warning;
}

void PurchaseCommands::decrease(TransferRow &row, PurchaseDraft draft, int change)
{
	if (0 >= change || 0 >= row.amount) return;
	change = std::min(row.amount, change);

	RuleItem *rule = 0;
	switch (row.type)
	{
	case TRANSFER_SOLDIER:
	case TRANSFER_SCIENTIST:
	case TRANSFER_ENGINEER:
		draft.personnel -= change;
		break;
	case TRANSFER_CRAFT:
		draft.crafts -= change;
		break;
	case TRANSFER_ITEM:
		rule = (RuleItem*)row.rule;
		draft.stores -= rule->getSize() * change;
		break;
	}
	row.amount -= change;
	draft.total -= row.cost * change;
}

PurchaseWarning PurchaseCommands::quantity(Base &base, SavedGame &save, TransferRow &row, PurchaseDraft draft,
    QuantityOperation operation, int value)
{
    switch (operation)
    {
    case QuantityOperation::Maximum: return increase(base, save, row, draft, INT_MAX);
    case QuantityOperation::Clear: decrease(row, draft, INT_MAX); return PurchaseWarning::None;
    case QuantityOperation::Set:
        if (value < 0) return PurchaseWarning::None;
        if (value > row.amount) return increase(base, save, row, draft, value - row.amount);
        decrease(row, draft, row.amount - value); return PurchaseWarning::None;
    case QuantityOperation::Adjust:
        if (value > 0) return increase(base, save, row, draft, value);
        if (value < 0) decrease(row, draft, value == INT_MIN ? INT_MAX : -value);
        return PurchaseWarning::None;
    }
    return PurchaseWarning::None;
}

void PurchaseCommands::commit(Base &base, SavedGame &save, const std::vector<TransferRow> &items, int total,
    int personnelTime, const std::function<Soldier *(const std::string &)> &generateSoldier)
{
	save.setFunds(save.getFunds() - total);
	for (std::vector<TransferRow>::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		if (i->amount > 0)
		{
			Transfer *t = 0;
			switch (i->type)
			{
			case TRANSFER_SOLDIER:
				for (int s = 0; s < i->amount; s++)
				{
					RuleSoldier *rule = (RuleSoldier*)i->rule;
					int time = rule->getTransferTime();
					if (time == 0)
						time = personnelTime;
					t = new Transfer(time);
					t->setSoldier(generateSoldier(rule->getType()));
					base.getTransfers()->push_back(t);
				}
				break;
			case TRANSFER_SCIENTIST:
				t = new Transfer(personnelTime);
				t->setScientists(i->amount);
				base.getTransfers()->push_back(t);
				break;
			case TRANSFER_ENGINEER:
				t = new Transfer(personnelTime);
				t->setEngineers(i->amount);
				base.getTransfers()->push_back(t);
				break;
			case TRANSFER_CRAFT:
				for (int c = 0; c < i->amount; c++)
				{
					RuleCraft *rule = (RuleCraft*)i->rule;
					t = new Transfer(rule->getTransferTime());
					Craft *craft = new Craft(rule, &base, save.getId(rule->getType()));
					craft->setStatus("STR_REFUELLING");
					t->setCraft(craft);
					base.getTransfers()->push_back(t);
				}
				break;
			case TRANSFER_ITEM:
				{
					RuleItem *rule = (RuleItem*)i->rule;
					t = new Transfer(rule->getTransferTime());
					t->setItems(rule->getType(), i->amount);
					base.getTransfers()->push_back(t);
				}
				break;
			}
		}
	}
}

void PurchaseCommands::discard(std::vector<TransferRow> &items, PurchaseDraft draft)
{
    for (TransferRow &row : items) row.amount = 0;
    draft.total = draft.personnel = draft.crafts = 0;
    draft.stores = 0.0;
}
}
