// SPDX-License-Identifier: GPL-3.0-or-later
// Exact frozen /20 handlers are a separate executable reference, not wrappers
// around the new core. Generated inclusions are hash-bound by the root runner.
#include "CraftOutfittingModelSeamsV21.h"
#include "../../src/Savegame/CraftOutfittingCommand.h"
namespace OpenXcom {
namespace Options {int changeValueByMouseWheel=1;}
#include "OutfittingFrozenModel.generated.inc"
#define EQUIPMENT_METHODS \
 void updateQuantity();void moveLeft();void moveRight();void moveLeftByValue(int);void moveRightByValue(int); \
 void lstEquipmentLeftArrowPress(Action*);void lstEquipmentRightArrowPress(Action*); \
 void lstEquipmentLeftArrowRelease(Action*);void lstEquipmentRightArrowRelease(Action*); \
 void lstEquipmentLeftArrowClick(Action*);void lstEquipmentRightArrowClick(Action*);void lstEquipmentMousePress(Action*); \
 void btnClearClick(Action*);void btnOkClick(Action*);
struct LegacyCraftEquipmentState:EquipmentUi {EQUIPMENT_METHODS};
struct CraftEquipmentState:EquipmentUi {EQUIPMENT_METHODS void changeEquipmentQuantity(size_t,QuantityOperation,int=0);};
struct LegacyCraftSoldiersState:CrewUi {void lstSoldiersClick(Action*);};
struct CraftSoldiersState:CrewUi {void lstSoldiersClick(Action*);void setSoldierAssigned(size_t,bool);void inspectSoldier(size_t);};
#include "OutfittingLegacy.generated.inc"
#include "OutfittingStateAdapters.generated.inc"
}
#include "OutfittingCore.generated.inc"
using namespace OpenXcom;
namespace {
unsigned long checks=0,cases=0;
void check(bool ok,const std::string &why){++checks;if(!ok){std::cerr<<"FAIL case "<<cases<<": "<<why<<"\n";std::exit(1);}}
struct World {
 Base base;Craft craft,other;Soldier soldier;Game game;
 World():craft(&base,"target"),other(&base,"other"){base.crafts={&craft,&other};base.soldiers={&soldier};}
 template<class S>void bind(S &s){s._base=&base;s._game=&game;}
};
std::string model(const World &w){std::ostringstream s;for(const auto &v:w.base.storage._qty)s<<"b"<<v.first<<":"<<v.second<<";";for(const auto &v:w.craft.items._qty)s<<"c"<<v.first<<":"<<v.second<<";";for(auto *v:w.craft.vehicles)s<<"v"<<v->getRules()->getType()<<":"<<v->getAmmo()<<":"<<v->getSize()<<";";s<<"s"<<(w.soldier.craft?w.soldier.craft->name:"none")<<":"<<w.craft.getSpaceUsed()<<":"<<w.craft.getSpaceAvailable();return s.str();}
void compare(World &old,World &now,LegacyCraftEquipmentState &a,CraftEquipmentState &b,const std::string &context){
 check(model(old)==model(now),context+" exact model");check(a._totalItems==b._totalItems,context+" total");
 check(old.game.warnings==now.game.warnings,context+" warnings");check(old.game.warningTotals==now.game.warningTotals,context+" pre-mutation warning point");
 check(a.list.cells==b.list.cells&&a.list.colors==b.list.colors&&a.available.value==b.available.value&&a.used.value==b.used.value,context+" display refresh");
 check(a.right.stops==b.right.stops&&a.left.stops==b.left.stops&&a.right.interval==b.right.interval&&a.left.interval==b.left.interval,context+" timer presentation");
}
void configure(World &w,int stock,int aboard,int cap,int months,bool fixed=false,int ammo=0,int slots=4,int seats=14,int crew=0,int unitSize=2,int clip=30,int ammoClip=1,bool hasAmmo=true,bool hasUnit=true){
 w.base.storage._qty["item"]=stock;w.game.save.months=months;w.craft.rule.maxItems=cap;w.craft.rule.vehicles=slots;w.craft.rule.soldiers=seats;
 w.soldier.craft=crew?&w.craft:nullptr;w.game.mod.item.fixed=fixed;w.game.mod.item.clip=clip;w.game.mod.ammo.clip=ammoClip;w.game.mod.hasUnit=hasUnit;w.game.mod.armor.size=unitSize;
 if(fixed){if(hasAmmo)w.game.mod.item.ammo={"ammo"};w.base.storage._qty["ammo"]=ammo;for(int n=0;n<aboard;++n)w.craft.vehicles.push_back(new Vehicle(&w.game.mod.item,clip,hasUnit?unitSize*unitSize:4));}
 else w.craft.items._qty["item"]=aboard;
}
void legacyIntent(LegacyCraftEquipmentState &s,QuantityOperation op,int value){
 if(op==QuantityOperation::Maximum)s.moveRightByValue(INT_MAX);
 else if(op==QuantityOperation::Clear)s.moveLeftByValue(INT_MAX);
 else if(op==QuantityOperation::Set){if(value<0)return;auto *c=s._base->crafts[0];int current=s._game->mod.item.fixed?c->getVehicleCount("item"):c->items.getItem("item");if(value>current)s.moveRightByValue(value-current);else s.moveLeftByValue(current-value);}
 else if(value>0)s.moveRightByValue(value);else if(value<0)s.moveLeftByValue(value==INT_MIN?INT_MAX:-value);
}
void equipmentItemParity(){
 for(int months:{-1,0})for(int stock:{0,1,17,1000,INT_MAX})for(int aboard:{0,1,7})for(int cap:{0,1,8,20})
 for(auto op:{QuantityOperation::Adjust,QuantityOperation::Set,QuantityOperation::Maximum,QuantityOperation::Clear})for(int value:{INT_MIN,-1000,-1,0,1,1000,100000,INT_MAX}){
  // The frozen ordinary ItemContainer and total arithmetic use signed int.
  // Deliberately exclude already-undefined worlds rather than inventing parity.
  if((long long)stock+aboard>INT_MAX)continue;
  if(months==-1 && ((op==QuantityOperation::Adjust&&value==100000)||(op==QuantityOperation::Set&&value==INT_MAX)))continue;
  if(months==-1 && op==QuantityOperation::Adjust&&value==INT_MAX){} // ordinary sentinel becomes 10
  World old,now;configure(old,stock,aboard,cap,months);configure(now,stock,aboard,cap,months);
  LegacyCraftEquipmentState a;CraftEquipmentState b;old.bind(a);now.bind(b);a._totalItems=b._totalItems=aboard;old.game.total=&a._totalItems;now.game.total=&b._totalItems;
  ++cases;legacyIntent(a,op,value);b.changeEquipmentQuantity(0,op,value);compare(old,now,a,b,"item intent");
 }
}
void equipmentVehicleParity(){
 for(int months:{-1,0})for(int stock:{0,1,7,1000,INT_MAX})for(int aboard:{0,1,3})for(int ammo:{0,1,29,30,45,60,120,INT_MAX-120})for(int slots:{0,1,4})for(int seats:{0,3,4,14})
 for(int variant:{0,1,2,3}){
  const bool hasAmmo=variant!=0,hasUnit=variant!=1;const int clip=variant==3?0:30,ammoClip=variant==3?8:1;
  for(auto op:{QuantityOperation::Adjust,QuantityOperation::Set,QuantityOperation::Maximum,QuantityOperation::Clear}){
   if((long long)stock+aboard>INT_MAX)continue;
   World old,now;configure(old,stock,aboard,0,months,true,ammo,slots,seats,1,2,clip,ammoClip,hasAmmo,hasUnit);configure(now,stock,aboard,0,months,true,ammo,slots,seats,1,2,clip,ammoClip,hasAmmo,hasUnit);
   LegacyCraftEquipmentState a;CraftEquipmentState b;old.bind(a);now.bind(b);old.game.total=&a._totalItems;now.game.total=&b._totalItems;
   ++cases;int value=op==QuantityOperation::Set?1:1000;legacyIntent(a,op,value);b.changeEquipmentQuantity(0,op,value);compare(old,now,a,b,"vehicle intent");
  }
 }
}
void guiAdapters(){
 for(int button:{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT,SDL_BUTTON_WHEELUP,SDL_BUTTON_WHEELDOWN})for(int wheel:{-1,0,1,1000,INT_MAX})for(int variant:{0,1,2,3,4,5}){
  World old,now;configure(old,100,3,8,0);configure(now,100,3,8,0);LegacyCraftEquipmentState a;CraftEquipmentState b;old.bind(a);now.bind(b);a._totalItems=b._totalItems=3;old.game.total=&a._totalItems;now.game.total=&b._totalItems;
  Options::changeValueByMouseWheel=wheel;Action event;event.details.button.button=button;event.x=variant==5?99:110;++cases;
  switch(variant){case 0:a.lstEquipmentLeftArrowPress(&event);b.lstEquipmentLeftArrowPress(&event);a.lstEquipmentLeftArrowClick(&event);b.lstEquipmentLeftArrowClick(&event);a.lstEquipmentLeftArrowRelease(&event);b.lstEquipmentLeftArrowRelease(&event);break;
   case 1:a.lstEquipmentRightArrowPress(&event);b.lstEquipmentRightArrowPress(&event);a.lstEquipmentRightArrowClick(&event);b.lstEquipmentRightArrowClick(&event);a.lstEquipmentRightArrowRelease(&event);b.lstEquipmentRightArrowRelease(&event);break;
   case 2:a.moveLeft();b.moveLeft();break;case 3:a.moveRight();b.moveRight();break;default:a.lstEquipmentMousePress(&event);b.lstEquipmentMousePress(&event);}
  compare(old,now,a,b,"GUI adapter");check(a.left.running==b.left.running&&a.right.running==b.right.running,"press/release timer state");
  auto before=model(now);a.btnOkClick(nullptr);b.btnOkClick(nullptr);check(old.game.pops==1&&now.game.pops==1&&before==model(now),"OK only closes");
 }
 // Whole-craft New Battle clear still calls the same selected-row rule.
 World old,now;configure(old,0,7,0,-1);configure(now,0,7,0,-1);LegacyCraftEquipmentState a;CraftEquipmentState b;old.bind(a);now.bind(b);a._totalItems=b._totalItems=7;++cases;a.btnClearClick(nullptr);b.btnClearClick(nullptr);compare(old,now,a,b,"New Battle clear");
}
void crewParity(){
 for(int owner:{0,1,2})for(int wound:{0,1,5})for(int capacity:{0,1,14})for(bool away:{false,true})for(bool ownAway:{false,true})for(int button:{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT})for(int mx:{20,100,119,120}){
  World old,now;for(auto *w:{&old,&now}){w->craft.rule.soldiers=capacity;w->soldier.wound=wound;w->soldier.craft=owner==0?nullptr:owner==1?&w->craft:&w->other;w->other.status=away?"STR_OUT":"STR_READY";w->craft.status=ownAway?"STR_OUT":"STR_READY";}
  LegacyCraftSoldiersState a;CraftSoldiersState b;old.bind(a);now.bind(b);a.list.colors[0]=b.list.colors[0]=owner==1?2:owner==2?3:1;a.list.cells[{0,2}]=b.list.cells[{0,2}]=owner==0?"none":owner==1?"target":"other";
  Action action;action.details.button.button=button;action.x=mx;++cases;a.lstSoldiersClick(&action);b.lstSoldiersClick(&action);
  check(model(old)==model(now),"crew GUI business parity");check(a.list.cells==b.list.cells&&a.list.colors==b.list.colors&&a.available.value==b.available.value&&a.used.value==b.used.value,"crew GUI colors/text refusal parity");check(old.game.inspected==now.game.inspected,"secondary inspect parity and reorder-band exclusion");
  const bool desired=now.soldier.craft==&now.craft;const auto before=model(now);const auto cells=b.list.cells;const auto colors=b.list.colors;b.setSoldierAssigned(0,desired);check(model(now)==before&&b.list.cells==cells&&b.list.colors==colors,"desired crew bool is idempotent");
 }
}
void warningAndCapacityCases(){
 World item;configure(item,1000,7,8,0);CraftEquipmentState s;item.bind(s);s._totalItems=7;item.game.total=&s._totalItems;++cases;
 s.changeEquipmentQuantity(0,QuantityOperation::Maximum);
 check(item.craft.items.getItem("item")==8&&item.base.storage.getItem("item")==999,"capacity warning accompanies partial addition");
 check(item.game.warnings.size()==1&&item.game.warningTotals[0]==7&&s._totalItems==8,"max-items warning sink precedes mutation");
 World hwp;configure(hwp,3,0,0,0,true,45);CraftEquipmentState v;hwp.bind(v);++cases;
 v.changeEquipmentQuantity(0,QuantityOperation::Maximum);
 check(hwp.craft.getNumVehicles()==1&&hwp.base.storage.getItem("item")==2&&hwp.base.storage.getItem("ammo")==15&&hwp.game.warnings.empty(),"partial HWP ammo load emits no warning");
 v.changeEquipmentQuantity(0,QuantityOperation::Adjust,1);
 check(hwp.craft.getNumVehicles()==1&&hwp.game.warnings.size()==1,"zero-add HWP ammo refusal warns once");
 v.changeEquipmentQuantity(0,QuantityOperation::Clear);
 check(hwp.craft.getNumVehicles()==0&&hwp.base.storage.getItem("item")==3&&hwp.base.storage.getItem("ammo")==45,"HWP clear returns vehicle and full ordinary ammo clips");
 World full;configure(full,1,0,0,0,true,0,0);CraftEquipmentState f;full.bind(f);++cases;f.changeEquipmentQuantity(0,QuantityOperation::Maximum);
 check(full.craft.getNumVehicles()==0&&full.game.warnings.empty(),"no vehicle room has no ammo warning");
 World other;configure(other,3,0,0,0,true,120);other.game.mod.other.fixed=true;other.craft.vehicles.push_back(new Vehicle(&other.game.mod.other,5,4));other.craft.vehicles.push_back(new Vehicle(&other.game.mod.item,17,4));CraftEquipmentState mixed;other.bind(mixed);++cases;
 mixed.changeEquipmentQuantity(0,QuantityOperation::Clear);check(other.craft.getNumVehicles()==1&&other.craft.vehicles[0]->getRules()->getType()=="other"&&other.base.storage.getItem("ammo")==150,"HWP unload matches selected rules and preserves other vehicle");
}

void directBoundaries(){
 World w;configure(w,7,3,0,0);CraftEquipmentState s;w.bind(s);s._totalItems=3;const auto initial=model(w);++cases;
 s.changeEquipmentQuantity(9,QuantityOperation::Clear);check(model(w)==initial,"out-of-range equipment row no effect");
 s.changeEquipmentQuantity(0,QuantityOperation::Set,-1);s.changeEquipmentQuantity(0,QuantityOperation::Adjust,0);check(model(w)==initial,"negative set/zero adjust no effect");
 for(int n:{-1000,-1,0}){s.moveRightByValue(n);s.moveLeftByValue(n);}check(model(w)==initial,"raw nonpositive moves no effect");
 s.changeEquipmentQuantity(0,QuantityOperation::Set,1);check(w.craft.items.getItem("item")==1&&w.base.storage.getItem("item")==9,"set unload exact target");
 s.changeEquipmentQuantity(0,QuantityOperation::Set,100000);check(w.craft.items.getItem("item")==10&&w.base.storage.getItem("item")==0,"set bounded by stock");
 s.changeEquipmentQuantity(0,QuantityOperation::Adjust,INT_MIN);check(w.craft.items.getItem("item")==0&&w.base.storage.getItem("item")==10,"INT_MIN safely saturates unload");
 CraftSoldiersState crew;w.bind(crew);crew.setSoldierAssigned(9,true);crew.inspectSoldier(9);check(!w.soldier.craft&&w.game.inspected.empty(),"out-of-range crew row no effect");
}
}
int main(){equipmentItemParity();equipmentVehicleParity();guiAdapters();crewParity();warningAndCapacityCases();directBoundaries();std::cout<<"AgentCraftOutfittingCommandTestV21 PASS "<<checks<<" checks in "<<cases<<" cases\n";}
