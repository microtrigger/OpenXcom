// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "QuantityCommand.h"
namespace OpenXcom {
class Base; class Production; class SavedGame; class Mod;
enum class ProductionControl { Engineers, Units };
enum class ProductionWarning { None, NoFreeHangars };
struct ProductionCommandResult {
    unsigned refresh;
    ProductionWarning warning;
    ProductionCommandResult(unsigned value=0, ProductionWarning problem=ProductionWarning::None):refresh(value),warning(problem){}
};
struct ProductionDraft { bool sell=false; };
namespace ProductionCommands {
// Raw deltas preserve the ordinary wheel/timer infinity-flag behavior.
ProductionCommandResult rawEngineers(Base &, Production &, int delta);
ProductionCommandResult rawUnits(Base &, Production &, int delta);
// Visible quantity intent follows the ordinary click/max/min semantics.
ProductionCommandResult quantity(Base &, Production &, ProductionControl, QuantityOperation, int value=0);
void setSell(ProductionDraft &, bool enabled);
void accept(Base &, Production &, SavedGame &, const Mod *, const ProductionDraft &, bool isNew);
void stop(Base &, Production &);
}
}
