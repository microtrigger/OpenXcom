// SPDX-License-Identifier: GPL-3.0-or-later
// Fresh production command object test. Every class is declared by its actual
// production header, including Vehicle allocation size and virtual base layout.
// Only dependencies are explicit model-method/constructor seams. No Game/UI.
#include "../../src/Savegame/CraftOutfittingCommand.h"
#include "../../src/Savegame/Base.h"
#include "../../src/Savegame/Craft.h"
#include "../../src/Savegame/Soldier.h"
#include "../../src/Savegame/ItemContainer.h"
#include "../../src/Savegame/Vehicle.h"
#include "../../src/Savegame/SavedGame.h"
#include "../../src/Mod/Mod.h"
#include "../../src/Mod/RuleItem.h"
#include "../../src/Mod/RuleCraft.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Mod/Unit.h"
#include <climits>
#include <cstdlib>
#include <iostream>
#include <map>
namespace ObjectSeams {
struct RuleValues {bool fixed=false,unit=true;int clip=30,ammoClip=1,armorSize=2,seats=14,slots=4,maxItems=0;std::vector<std::string> ammo;};
std::map<const OpenXcom::RuleItem*,RuleValues> items;
std::map<const OpenXcom::RuleCraft*,RuleValues> crafts;
std::map<const OpenXcom::Armor*,int> armorSizes;
std::map<const OpenXcom::SavedGame*,int> months;
struct ModValues {std::map<std::string,OpenXcom::RuleItem*> items;OpenXcom::Unit *unit=nullptr;OpenXcom::Armor *armor=nullptr;};
std::map<const OpenXcom::Mod*,ModValues> mods;
}
namespace OpenXcom {
Target::Target():_lon(0),_lat(0),_id(0){}Target::~Target(){}
void Target::load(const YAML::Node&){}YAML::Node Target::save()const{return YAML::Node();}
std::string Target::getName(Language*)const{return _name;}std::string Target::getDefaultName(Language*)const{return "target";}
std::string Target::getMarkerName()const{return "marker";}int Target::getMarkerId()const{return 0;}
MovingTarget::MovingTarget():_dest(nullptr),_speedLon(0),_speedLat(0),_speedRadian(0),_meetPointLon(0),_meetPointLat(0),_speed(0),_meetCalculated(false){}
MovingTarget::~MovingTarget(){}void MovingTarget::load(const YAML::Node&){}YAML::Node MovingTarget::save()const{return YAML::Node();}
void MovingTarget::setDestination(Target *t){_dest=t;}void MovingTarget::calculateSpeed(){}
Base::Base(const Mod *m):_mod(m),_items(new ItemContainer){}Base::~Base(){delete _items;}
YAML::Node Base::save()const{return YAML::Node();}std::string Base::getType()const{return "base";}std::string Base::getName(Language*)const{return "base";}int Base::getMarker()const{return 0;}
ItemContainer *Base::getStorageItems(){return _items;}std::vector<Soldier*> *Base::getSoldiers(){return &_soldiers;}
Craft::Craft(RuleCraft *r,Base *b,int):_rules(r),_base(b),_items(new ItemContainer),_status("STR_READY"){}
Craft::~Craft(){for(auto *v:_vehicles)delete v;delete _items;}
YAML::Node Craft::save()const{return YAML::Node();}std::string Craft::getType()const{return "craft";}std::string Craft::getDefaultName(Language*)const{return "craft";}int Craft::getMarker()const{return 0;}void Craft::setDestination(Target *t){_dest=t;}
RuleCraft *Craft::getRules()const{return _rules;}std::string Craft::getStatus()const{return _status;}void Craft::setStatus(const std::string &s){_status=s;}
ItemContainer *Craft::getItems(){return _items;}std::vector<Vehicle*> *Craft::getVehicles(){return &_vehicles;}int Craft::getNumVehicles()const{return _vehicles.size();}
Soldier::Soldier(RuleSoldier *r,Armor *a,int):_rules(r),_craft(nullptr),_recovery(0),_armor(a){}Soldier::~Soldier(){}
Craft *Soldier::getCraft()const{return _craft;}void Soldier::setCraft(Craft *c){_craft=c;}int Soldier::getWoundRecovery()const{return _recovery;}void Soldier::setWoundRecovery(int n){_recovery=n;if(n>0)_craft=nullptr;}
SavedGame::SavedGame(){}SavedGame::~SavedGame(){}int SavedGame::getMonthsPassed()const{return ObjectSeams::months[this];}
ItemContainer::ItemContainer(){}ItemContainer::~ItemContainer(){}
Vehicle::Vehicle(RuleItem *r,int a,int s):_rules(r),_ammo(a),_size(s){}Vehicle::~Vehicle(){}
RuleItem *Vehicle::getRules()const{return _rules;}int Vehicle::getAmmo()const{return _ammo;}int Vehicle::getSize()const{return _size;}
RuleCraft::RuleCraft(const std::string &s):_type(s){}RuleCraft::~RuleCraft(){}
int RuleCraft::getSoldiers()const{return ObjectSeams::crafts[this].seats;}int RuleCraft::getVehicles()const{return ObjectSeams::crafts[this].slots;}int RuleCraft::getMaxItems()const{return ObjectSeams::crafts[this].maxItems;}
RuleItem::RuleItem(const std::string &s):_type(s){}RuleItem::~RuleItem(){}
std::string RuleItem::getType()const{return _type;}bool RuleItem::isFixed()const{return ObjectSeams::items[this].fixed;}int RuleItem::getClipSize()const{return ObjectSeams::items[this].clip;}
std::vector<std::string> *RuleItem::getCompatibleAmmo(){return &ObjectSeams::items[this].ammo;}
Armor::Armor(const std::string &s):_type(s){}Armor::~Armor(){}int Armor::getSize()const{return ObjectSeams::armorSizes[this];}
Unit::Unit(const std::string &s):_type(s){}Unit::~Unit(){}std::string Unit::getArmor()const{return "armor";}
GameTime::GameTime(int,int,int,int,int,int,int){}GameTime::~GameTime(){}
Mod::Mod():_startingTime(6,1,1,1999,12,0,0){}Mod::~Mod(){}
RuleItem *Mod::getItem(const std::string &s,bool)const{return ObjectSeams::mods[this].items.at(s);}Unit *Mod::getUnit(const std::string &,bool)const{return ObjectSeams::mods[this].unit;}Armor *Mod::getArmor(const std::string &,bool)const{return ObjectSeams::mods[this].armor;}
// These dependency bodies are independently extracted from frozen /20. They
// access real class members through their unchanged production declarations.
#include "OutfittingRealModel.generated.inc"
}
using namespace OpenXcom;
namespace {
unsigned checks=0,cases=0;
void check(bool ok,const char *why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<"\n";std::exit(1);}}
struct World {
 Mod mod;SavedGame save;RuleCraft rule;RuleItem item,ammo,otherItem;Armor armor;Unit unit;Base base;Craft craft,other;Soldier soldier;int total=0;std::vector<CraftEquipmentWarning> warnings;std::vector<int> before;CraftEquipmentWarningSink sink;
 World():rule("craft"),item("item"),ammo("ammo"),otherItem("other"),armor("armor"),unit("item"),base(&mod),craft(&rule,&base),other(&rule,&base),soldier(nullptr,&armor),sink([this](CraftEquipmentWarning w,int,const std::string &){warnings.push_back(w);before.push_back(total);}){
  ObjectSeams::items[&item]=ObjectSeams::RuleValues();ObjectSeams::items[&ammo]=ObjectSeams::RuleValues();ObjectSeams::items[&ammo].clip=1;ObjectSeams::crafts[&rule]=ObjectSeams::RuleValues();ObjectSeams::armorSizes[&armor]=2;ObjectSeams::months[&save]=0;ObjectSeams::mods[&mod].items={{"item",&item},{"ammo",&ammo},{"other",&otherItem}};ObjectSeams::mods[&mod].armor=&armor;ObjectSeams::mods[&mod].unit=&unit;base.getSoldiers()->push_back(&soldier);
 }
 bool change(QuantityOperation op,int value=0){return CraftOutfittingCommand::quantity(base,craft,mod,save,"item",total,op,value,sink);}
 void fixed(){ObjectSeams::items[&item].fixed=true;ObjectSeams::items[&item].ammo={"ammo"};}
};
void items(){for(int stock:{0,1,1000,INT_MAX})for(int aboard:{0,1,7})for(int limit:{0,8,20}){
 if((long long)stock+aboard>INT_MAX)continue;World w;w.base.getStorageItems()->addItem("item",stock);w.craft.getItems()->addItem("item",aboard);w.total=aboard;ObjectSeams::crafts[&w.rule].maxItems=limit;++cases;
 check(!w.change(QuantityOperation::Adjust,0),"zero is ordinary no-refresh");check(!w.change(QuantityOperation::Set,-1),"negative set no-refresh");
 const int desired=limit?std::min(limit,stock+aboard):stock+aboard;w.change(QuantityOperation::Maximum);check(w.total==desired&&w.craft.getItems()->getItem("item")==desired,"maximum native object stock/capacity");
 if(limit&&stock+aboard>limit&&stock>0){check(w.warnings.size()==1&&w.before[0]==aboard,"native object warning before partial add");}
 w.change(QuantityOperation::Set,1);check(w.total==std::min(1,stock+aboard),"native object set exact or bounded");
 w.change(QuantityOperation::Adjust,INT_MIN);check(w.total==0&&w.base.getStorageItems()->getItem("item")==stock+aboard,"native object saturating unload conservation");
}}
void vehicles(){for(int stock:{0,1,1000,INT_MAX})for(int ammo:{0,29,30,45,120})for(int room:{0,4,14}){
 World w;w.fixed();w.base.getStorageItems()->addItem("item",stock);w.base.getStorageItems()->addItem("ammo",ammo);ObjectSeams::crafts[&w.rule].seats=room;++cases;w.change(QuantityOperation::Maximum);
 const int count=std::min(std::min(stock,ammo/30),std::min(4,room/4));check(w.craft.getNumVehicles()==count,"native object HWP stock/ammo/room clamp");check(w.base.getStorageItems()->getItem("ammo")==ammo-30*count,"native object ammo debit");
 check(w.warnings.size()==((room>=4&&stock>0&&ammo<30)?1u:0u),"native object warns only zero canBeAdded");
 for(auto *v:*w.craft.getVehicles())check(v->getAmmo()==30&&v->getSize()==4&&v->getRules()==&w.item,"real ABI Vehicle allocation/constructor");
 w.change(QuantityOperation::Clear);check(w.craft.getNumVehicles()==0&&w.base.getStorageItems()->getItem("item")==stock&&w.base.getStorageItems()->getItem("ammo")==ammo,"native object HWP unload conservation");
}}
void crew(){for(int owner:{0,1,2})for(int wound:{0,1})for(int seats:{0,1,14})for(bool away:{false,true}){
 World w;ObjectSeams::crafts[&w.rule].seats=seats;w.soldier.setWoundRecovery(wound);w.soldier.setCraft(owner==1?&w.craft:owner==2?&w.other:nullptr);w.other.setStatus(away?"STR_OUT":"STR_READY");++cases;
 Craft *before=w.soldier.getCraft();auto result=CraftOutfittingCommand::setAssigned(w.craft,w.soldier,true);
 bool can=owner==1||(owner!=2||!away)&&(seats>0&&wound==0);
 check(w.soldier.getCraft()==(can?&w.craft:before),"native object crew constraints");
 if(owner==1)check(result==CraftCrewChange::Unchanged,"native object desired true idempotent");
 CraftOutfittingCommand::setAssigned(w.craft,w.soldier,false);check(w.soldier.getCraft()==(can?nullptr:before),"native object desired false never steals another craft assignment");
 auto snapshot=w.soldier.getCraft();CraftOutfittingCommand::setAssigned(w.craft,w.soldier,false);check(w.soldier.getCraft()==snapshot,"native object desired false idempotent");
}}
}
int main(){items();vehicles();crew();std::cout<<"AgentCraftOutfittingObjectTestV21 PASS "<<checks<<" checks in "<<cases<<" cases; real production class declarations, explicit model seams\n";}
