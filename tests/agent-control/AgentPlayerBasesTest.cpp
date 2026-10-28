// SPDX-License-Identifier: GPL-3.0-or-later
// Real native in-memory own-base models and collector. No Game, SDL window,
// campaign loading, resource discovery or player save files are used.
#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <sstream>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/RNG.h"
#include "../../src/Mod/Unit.h"
#include "../../src/Mod/RuleAlienMission.h"
#include "../../src/Savegame/GameTime.h"
// Synthetic rules injected in test-only access; getters/models stay native.
#define private public
#include "../../src/Mod/Mod.h"
#undef private
#include "../../src/Observer/AgentPlayerBases.h"
#include "../../src/Savegame/SavedGame.h"
#include "../../src/Savegame/Base.h"
#include "../../src/Savegame/BaseFacility.h"
#include "../../src/Savegame/Craft.h"
#include "../../src/Savegame/CraftWeapon.h"
#include "../../src/Savegame/Soldier.h"
#include "../../src/Savegame/ItemContainer.h"
#include "../../src/Savegame/ResearchProject.h"
#include "../../src/Savegame/Production.h"
#include "../../src/Savegame/Transfer.h"
#include "../../src/Savegame/EquipmentLayoutItem.h"
#include "../../src/Savegame/Vehicle.h"
#include "../../src/Mod/RuleBaseFacility.h"
#include "../../src/Mod/RuleCraft.h"
#include "../../src/Mod/RuleCraftWeapon.h"
#include "../../src/Mod/RuleSoldier.h"
#include "../../src/Mod/RuleItem.h"
#include "../../src/Mod/RuleResearch.h"
#include "../../src/Mod/RuleManufacture.h"
#include "../../src/Mod/Armor.h"
using namespace OpenXcom;using namespace OpenXcom::Observer;
static unsigned checks=0;
void check(bool yes,const char *label){++checks;if(!yes){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}}
struct EnemyTarget:Target {mutable unsigned reads=0;std::string getType()const override{++reads;return "PRIVATE_ENEMY_TYPE";}int getMarker()const override{++reads;return 99;}};
int main(int argc,char **argv){
 Mod mod;Language language;EnemyTarget enemy;SavedGame save;AgentPlayerBases collector;
 std::vector<std::string> rows,again;std::string reason;
 check(!collector.collect(nullptr,&language,rows,reason)&&reason=="no_campaign"&&rows.empty(),"no campaign does not start one");
 auto *item=new RuleItem("OWN_ITEM");item->load(YAML::Load("size: 0.5"),&mod,0);mod._items["OWN_ITEM"]=item;
 RuleBaseFacility facilityRule("OWN_LAB");facilityRule.load(YAML::Load("size: 2\nbuildTime: 20\nbuildCost: 300000\nmonthlyCost: 12000\npersonnel: 50\nstorage: 100\nlaboratories: 30\nworkshops: 20\ncrafts: 3\npsiLabs: 10\naliens: 12"),&mod,0);
 RuleCraft craftRule("OWN_CRAFT");craftRule.load(YAML::Load("fuelMax: 1000\ndamageMax: 100\nspeedMax: 2000\nweapons: 2\nsoldiers: 10\nvehicles: 2\ncostRent: 10000\nmaxItems: 80"),&mod,0);
 RuleSoldier soldierRule("OWN_SOLDIER");Armor armor("OWN_ARMOR");RuleResearch researchRule("OWN_RESEARCH");researchRule.load(YAML::Load("cost: 200"),0);
 RuleManufacture manufactureRule("OWN_PRODUCTION");manufactureRule.load(YAML::Load("time: 10\ncost: 100\nspace: 2\nrequiredItems: {OWN_ITEM: 1}\nproducedItems: {OWN_ITEM: 2}"),0);
 Base *a=new Base(&mod),*b=new Base(&mod);a->setName("FIRST BASE");b->setName("SECOND BASE");a->setLongitude(0.2);b->setLatitude(-0.5);save.getBases()->push_back(a);save.getBases()->push_back(b);
 BaseFacility *f=new BaseFacility(&facilityRule,a);f->setX(1);f->setY(2);f->setBuildTime(7);a->getFacilities()->push_back(f);BaseFacility *complete=new BaseFacility(&facilityRule,b);b->getFacilities()->push_back(complete);
 a->setScientists(4);a->setEngineers(6);a->getStorageItems()->addItem("OWN_ITEM",11);b->getStorageItems()->addItem("OWN_ITEM",3);
 Craft *c=new Craft(&craftRule,a,17);c->setName("OWN CRAFT NAME");c->setFuel(500);c->setDamage(8);c->setDestination(&enemy);a->getCrafts()->push_back(c);c->getItems()->addItem("OWN_ITEM",2);
 // id0 avoids random generation; all properties are native mutable setup only.
 Soldier *s=new Soldier(&soldierRule,&armor,0);s->setName("OWN SOLDIER NAME");s->setCraft(c);s->getCurrentStats()->psiStrength=81;s->promoteRank();s->promoteRank();a->getSoldiers()->push_back(s);s->getEquipmentLayout()->push_back(new EquipmentLayoutItem("OWN_ITEM","STR_RIGHT_HAND",0,0,"",-1));
 c->getVehicles()->push_back(new Vehicle(item,8,4));
 RuleCraftWeapon weaponRule("OWN_WEAPON");weaponRule.load(YAML::Load("ammoMax: 20\nrearmRate: 4\nlauncher: OWN_ITEM\nclip: OWN_ITEM"),&mod);c->getWeapons()->at(0)=new CraftWeapon(&weaponRule,9);
 auto *research=new ResearchProject(&researchRule,250);research->setAssigned(3);research->setSpent(12);a->addResearch(research);
 auto *production=new Production(&manufactureRule,5);production->setAssignedEngineers(2);production->setTimeSpent(13);a->addProduction(production);
 Transfer *items=new Transfer(9);items->setItems("OWN_ITEM",5);b->getTransfers()->push_back(items);Transfer *staff=new Transfer(12);staff->setScientists(7);b->getTransfers()->push_back(staff);
 Craft *incoming=new Craft(&craftRule,b,19);incoming->setName("INCOMING CRAFT");Transfer *craftTransfer=new Transfer(14);craftTransfer->setCraft(incoming);b->getTransfers()->push_back(craftTransfer);
 Soldier *incomingSoldier=new Soldier(&soldierRule,&armor,1);incomingSoldier->setName("INCOMING SOLDIER");incomingSoldier->setCraft(incoming);Transfer *soldierTransfer=new Transfer(14);soldierTransfer->setSoldier(incomingSoldier);b->getTransfers()->push_back(soldierTransfer);
 Transfer *engineers=new Transfer(5);engineers->setEngineers(3);b->getTransfers()->push_back(engineers);
 const auto seed=RNG::getSeed();const auto funds=save.getFunds();enemy.reads=0;
 check(collector.collect(&save,&language,rows,reason),"native two-base snapshot");
 if(argc>1){std::ofstream output(argv[1]);output<<"[";for(std::size_t i=0;i<rows.size();++i){if(i)output<<",";output<<rows[i];}output<<"]\n";}
 std::map<std::string,int> kinds;std::map<std::string,YAML::Node> byId;std::string researchId,facilityId;
 for(const auto &raw:rows){auto r=YAML::Load(raw);const auto kind=r["kind"].as<std::string>();++kinds[kind];byId[r["id"].as<std::string>()]=r;if(kind=="research")researchId=r["id"].as<std::string>();if(kind=="facility"&&r["build_days_remaining"].as<int>()==7)facilityId=r["id"].as<std::string>();check(raw.find("PRIVATE_ENEMY")==std::string::npos&&raw.find("retaliation")==std::string::npos&&raw.find("destination")==std::string::npos,"no enemy graph fields");}
 for(const auto &kind:{"campaign","base","facility","soldier","craft","research","production","transfer","stock","vehicle","weapon","equipment_layout"})check(kinds[kind]>0,"all allowlisted record kinds populated");
 check(kinds["base"]==2&&kinds["craft"]==2&&kinds["transfer"]==5,"all bases and in-transit craft captured");
 check(byId.at(facilityId)["build_days_remaining"].as<int>()==7,"construction time model read");
 check(collector.collect(&save,&language,again,reason)&&again==rows,"unchanged repeated reads exact stable");
 save.setSelectedBase(1);std::reverse(save.getBases()->begin(),save.getBases()->end());std::reverse(a->getFacilities()->begin(),a->getFacilities()->end());std::reverse(b->getTransfers()->begin(),b->getTransfers()->end());
 check(collector.collect(&save,&language,again,reason)&&again==rows,"selected base and row reorder do not change snapshot");
 check(enemy.reads==0,"enemy target getters never traversed");check(RNG::getSeed()==seed&&save.getFunds()==funds,"read does not mutate RNG or funds");
 check(f->getBuildTime()==7&&research->getSpent()==12&&production->getTimeSpent()==13&&items->getHours()==9&&c->getFuel()==500&&s->getCurrentStats()->psiStrength==81,"read preserves native model state");
 check(s->isPromoted(),"read did not consume promotion flag");
 bool wrongThread=false;std::thread other([&]{std::vector<std::string> r;std::string why;wrongThread=!collector.collect(&save,&language,r,why)&&why=="wrong_thread";});other.join();check(wrongThread,"non-owner thread refused");
 a->removeResearch(research);check(collector.collect(&save,&language,again,reason),"removed project captured");for(const auto &raw:again)check(YAML::Load(raw)["id"].as<std::string>()!=researchId,"removed identity absent");
 auto *replacement=new ResearchProject(&researchRule,250);a->addResearch(replacement);check(collector.collect(&save,&language,again,reason),"new project captured");for(const auto &raw:again){auto r=YAML::Load(raw);if(r["kind"].as<std::string>()=="research")check(r["id"].as<std::string>()!=researchId,"retired project identity not reused");}
 b->setName(std::string(5000,'x'));check(!collector.collect(&save,&language,again,reason)&&again.empty()&&reason=="record_limit","oversized field fails whole snapshot without truncation");b->setName("SECOND BASE");
 collector.reset();check(collector.collect(&save,&language,again,reason),"explicit lifecycle reset accepts campaign");
 std::cout<<"PASS "<<checks<<" native all-player-bases read-only checks; synthetic in-memory rules, no Game/window/player-save execution\n";
}
