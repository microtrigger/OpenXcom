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
#pragma once
#include "QuantityCommand.h"
#include "Transfer.h"
#include <functional>
#include <vector>

namespace OpenXcom
{
class Base;
class SavedGame;
class Soldier;

// Local purchase draft, shared by ordinary GUI and semantic command adapters.
// References preserve the screen's existing storage and accounting lifetime.
struct PurchaseDraft
{
    int &total, &personnel, &crafts;
    double &stores;
    PurchaseDraft(int &t, int &p, int &c, double &s) : total(t), personnel(p), crafts(c), stores(s) {}
};
enum class PurchaseWarning { None, Money, Quarters, Hangars, Stores };

class PurchaseCommands
{
    static PurchaseWarning increase(Base &base, SavedGame &save, TransferRow &row, PurchaseDraft draft, int change);
    static void decrease(TransferRow &row, PurchaseDraft draft, int change);
public:
    static PurchaseWarning quantity(Base &base, SavedGame &save, TransferRow &row, PurchaseDraft draft,
        QuantityOperation operation, int value = 0);
    static void commit(Base &base, SavedGame &save, const std::vector<TransferRow> &items, int total,
        int personnelTime, const std::function<Soldier *(const std::string &)> &generateSoldier);
    static void discard(std::vector<TransferRow> &items, PurchaseDraft draft);
};
}
