// SPDX-License-Identifier: GPL-3.0-or-later
#include "ProductionCommands.h"
#include "Base.h"
#include "Production.h"
#include "../Mod/RuleManufacture.h"
#include <algorithm>
#include <climits>
namespace OpenXcom { namespace ProductionCommands {
ProductionCommandResult rawEngineers(Base &base, Production &production, int delta)
{
    if (delta > 0)
    {
        const int available=base.getAvailableEngineers(), space=base.getFreeWorkshops();
        if (available > 0 && space > 0)
        {
            const int change=std::min(std::min(available,space),delta);
            production.setAssignedEngineers(production.getAssignedEngineers()+change);
            base.setEngineers(base.getEngineers()-change);
            return {true};
        }
    }
    else if (delta < 0 && production.getAssignedEngineers() > 0)
    {
        const int requested=delta==INT_MIN?INT_MAX:-delta;
        const int change=std::min(production.getAssignedEngineers(),requested);
        production.setAssignedEngineers(production.getAssignedEngineers()-change);
        base.setEngineers(base.getEngineers()+change);
        return {true};
    }
    return {};
}
ProductionCommandResult rawUnits(Base &base, Production &production, int delta)
{
    if (delta > 0)
    {
        const bool craft=production.getRules()->getCategory()=="STR_CRAFT";
        if (craft && base.getAvailableHangars()-base.getUsedHangars()<=0)
            return {false,ProductionWarning::NoFreeHangars};
        const int units=production.getAmountTotal();
        int change=std::min(INT_MAX-units,delta);
        if (craft) change=std::min(base.getAvailableHangars()-base.getUsedHangars(),change);
        production.setAmountTotal(units+change);
        return {true};
    }
    if (delta < 0)
    {
        const int units=production.getAmountTotal();
        const int requested=delta==INT_MIN?INT_MAX:-delta;
        const int change=std::min(units-(production.getAmountProduced()+1),requested);
        production.setAmountTotal(units-change);
        return {true};
    }
    return {};
}
ProductionCommandResult quantity(Base &base, Production &production, ProductionControl control, QuantityOperation operation, int value)
{
    if (control==ProductionControl::Engineers)
    {
        if (operation==QuantityOperation::Maximum) return rawEngineers(base,production,INT_MAX);
        if (operation==QuantityOperation::Clear) return rawEngineers(base,production,-INT_MAX);
        if (operation==QuantityOperation::Set)
        {
            if (value<0) return {};
            return rawEngineers(base,production,value-production.getAssignedEngineers());
        }
        return rawEngineers(base,production,value);
    }
    if (operation==QuantityOperation::Maximum)
    {
        if (production.getInfiniteAmount()) return {};
        if (production.getRules()->getCategory()=="STR_CRAFT") return rawUnits(base,production,INT_MAX);
        production.setInfiniteAmount(true); return {true};
    }
    if (operation==QuantityOperation::Clear)
    {
        production.setInfiniteAmount(false);
        production.setAmountTotal(production.getAmountProduced()+1);
        return {true};
    }
    if (operation==QuantityOperation::Set)
    {
        if (value<0) return {};
        const bool wasInfinite=production.getInfiniteAmount();
        production.setInfiniteAmount(false);
        const int target=std::max(value,production.getAmountProduced()+1);
        const int delta=target-production.getAmountTotal();
        if (!delta) return {true};
        auto result=rawUnits(base,production,delta);
        // Even a capacity refusal may leave the requested finite mode changed.
        // Refresh that draft separately from the ordinary warning outcome.
        result.refresh=result.refresh || wasInfinite;
        return result;
    }
    if (value>0 && production.getInfiniteAmount()) return {};
    if (value<0)
    {
        production.setInfiniteAmount(false);
        if (production.getAmountTotal()<=production.getAmountProduced())
        {
            production.setAmountTotal(production.getAmountProduced()+1);
            auto result=rawUnits(base,production,value);
            ++result.refresh;
            return result;
        }
    }
    return rawUnits(base,production,value);
}
void setSell(ProductionDraft &draft, bool enabled) { draft.sell=enabled; }
void accept(Base &base, Production &production, SavedGame &save, const Mod *mod, const ProductionDraft &draft, bool isNew)
{
    if (isNew) production.startItem(&base,&save,mod);
    production.setSellItems(draft.sell);
}
void stop(Base &base, Production &production) { base.removeProduction(&production); }
} }
