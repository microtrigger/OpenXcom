// SPDX-License-Identifier: GPL-3.0-or-later
// Pure business-rule parity. Model seams do not construct a Game, SDL or widgets.
#include "PurchaseModelTestSeamsV18s.h"
#include "../../src/Savegame/QuantityCommand.h"
#include "../../src/fmath.h"
#include <algorithm>
#include <climits>
#include <functional>
#include <cstdlib>
#include <iostream>
using OpenXcom::QuantityOperation;
namespace Purchases {
#include "PurchaseBaseStoreRule.generated.inc"
#include "PurchaseCommandsHeader.generated.inc"
#include "PurchaseCommands.generated.inc"
struct ErrorMessageState {std::string message;ErrorMessageState(const std::string&s,void*,int,const char*,int):message(s){}};
struct Game {unsigned pops=0;void popState(){++pops;}SavedGame save;Mod mod;std::string error;void pushState(ErrorMessageState*e){error=e->message;delete e;} SavedGame *getSavedGame(){return &save;}Mod*getMod(){return &mod;}};
struct Timer {void stop(){}};
struct LegacyPurchase {Base *_base;Game *_game;TransferRow row;int _total=0,_pQty=0,_cQty=0;double _iQty=0;Timer timer,*_timerInc=&timer;void *_palette=nullptr;
 TransferRow&getRow(){return row;}std::string tr(const char*s){return s;}void updateItemStrings(){}void increaseByValue(int);void decreaseByValue(int);std::vector<TransferRow> _items;void btnOkClick(void*);void btnCancelClick(void*);
};
#include "PurchaseLegacy.generated.inc"
}
namespace {unsigned checks=0;void check(bool ok,const char*why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}}
std::string warning(Purchases::PurchaseWarning w){using Purchases::PurchaseWarning;return w==PurchaseWarning::Money?"STR_NOT_ENOUGH_MONEY":w==PurchaseWarning::Quarters?"STR_NOT_ENOUGH_LIVING_SPACE":w==PurchaseWarning::Hangars?"STR_NO_FREE_HANGARS_FOR_PURCHASE":w==PurchaseWarning::Stores?"STR_NOT_ENOUGH_STORE_SPACE":"";}
void parity(){using namespace Purchases;
 for(auto type:{TRANSFER_SOLDIER,TRANSFER_SCIENTIST,TRANSFER_ENGINEER,TRANSFER_CRAFT,TRANSFER_ITEM})
 for(int money:{0,99,100,350,10000})for(int space:{0,1,3,20})for(double size:{0.0,0.1,1.0,3.0}){
 Base a,b;Game old;a.quarters=b.quarters=space;a.usedQuarters=b.usedQuarters=0;a.hangars=b.hangars=space;a.usedHangars=b.usedHangars=0;a.stores=b.stores=space;a.usedStores=b.usedStores=0;old.save.funds=money;SavedGame save;save.funds=money;RuleItem item;item.size=size;RuleSoldier soldier;RuleCraft craft;void*rule=type==TRANSFER_ITEM?static_cast<void*>(&item):type==TRANSFER_SOLDIER?static_cast<void*>(&soldier):type==TRANSFER_CRAFT?static_cast<void*>(&craft):nullptr;
 LegacyPurchase legacy;legacy._base=&a;legacy._game=&old;legacy.row={type,rule,"private",100,0,0,0};TransferRow row=legacy.row;int total=0,people=0,crafts=0;double stores=0;PurchaseDraft draft(total,people,crafts,stores);
 for(auto op:{QuantityOperation::Adjust,QuantityOperation::Set,QuantityOperation::Maximum,QuantityOperation::Adjust,QuantityOperation::Clear,QuantityOperation::Set}){
  const int value=op==QuantityOperation::Adjust?(row.amount? -2:3):op==QuantityOperation::Set?7:0;old.error.clear();
  if(op==QuantityOperation::Maximum)legacy.increaseByValue(INT_MAX);else if(op==QuantityOperation::Clear)legacy.decreaseByValue(INT_MAX);else if(op==QuantityOperation::Set){if(value>legacy.row.amount)legacy.increaseByValue(value-legacy.row.amount);else legacy.decreaseByValue(legacy.row.amount-value);}else if(value>0)legacy.increaseByValue(value);else legacy.decreaseByValue(-value);
  auto result=PurchaseCommands::quantity(b,save,row,draft,op,value);
  check(warning(result)==old.error,"money-first warning parity with exact legacy helpers");check(row.amount==legacy.row.amount&&total==legacy._total&&people==legacy._pQty&&crafts==legacy._cQty&&std::abs(stores-legacy._iQty)<1e-9,"draft accounting parity with exact legacy helpers");check(save.funds==money&&b.transfers.empty(),"quantity never spends or creates transfers");
 }
 }
}
void commitDiscard(){using namespace Purchases;
 for(int soldierTime:{0,24}) {
 Base b,oldBase;SavedGame save;Game old;RuleItem item;RuleSoldier soldier;soldier.time=soldierTime;RuleCraft craft;std::vector<TransferRow> rows={{TRANSFER_SOLDIER,&soldier,"S",10,0,0,2},{TRANSFER_SCIENTIST,nullptr,"S",20,0,0,3},{TRANSFER_ENGINEER,nullptr,"E",30,0,0,4},{TRANSFER_CRAFT,&craft,"C",40,0,0,2},{TRANSFER_ITEM,&item,"I",50,0,0,5},{TRANSFER_ITEM,&item,"empty",50,0,0,0}};
 int total=530,people=9,crafts=2;double stores=5;const auto funds=save.funds;LegacyPurchase legacy;legacy._base=&oldBase;legacy._game=&old;legacy._total=total;legacy._items=rows;legacy.btnOkClick(nullptr);
 PurchaseCommands::commit(b,save,rows,total,old.mod.getPersonnelTime(),[&](const std::string&type){return old.mod.genSoldier(&save,type);});
 check(save.funds==old.save.funds&&save.funds==funds-total&&old.pops==1&&b.transfers.size()==oldBase.transfers.size()&&b.transfers.size()==7,"commit parity with exact prior ordinary OK body");
 for(size_t n=0;n<b.transfers.size();++n){auto a=b.transfers[n],o=oldBase.transfers[n];check(a->hours==o->hours&&a->type==o->type&&a->amount==o->amount&&a->item==o->item&&a->craftId==o->craftId,"all pending transfer kind/time/amount/identity fields match prior OK");}
 check(b.transfers[0]->hours==(soldierTime?soldierTime:old.mod.getPersonnelTime()),"soldier transfer fallback preserved");
 const auto after=save.funds;const auto beforeOld=old.save.funds;const size_t priorTransfers=oldBase.transfers.size();legacy.btnCancelClick(nullptr);PurchaseCommands::discard(rows,PurchaseDraft(total,people,crafts,stores));
 check(total==0&&people==0&&crafts==0&&stores==0&&save.funds==after&&old.save.funds==beforeOld&&b.transfers.size()==7&&oldBase.transfers.size()==priorTransfers&&old.pops==2,"Cancel parity in persistent model effects; shared discard clears only local draft");for(auto&r:rows)check(r.amount==0,"discard clears every draft amount");
 }
}

}
int main(){parity();commitDiscard();std::cout<<"AgentPurchaseCommandTestV18s PASS "<<checks<<" checks\n";}
