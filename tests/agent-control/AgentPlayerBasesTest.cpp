// SPDX-License-Identifier: GPL-3.0-or-later
// Real native in-memory own-base models and collector. No Game, SDL window,
// campaign loading, resource discovery or player save files are used.
#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <sstream>
#include <set>
#include <thread>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/RNG.h"
#include "../../src/Mod/Unit.h"
#include "../../src/Mod/RuleAlienMission.h"
#include "../../src/Savegame/GameTime.h"
#include "../../src/Savegame/Country.h"
#include "../../src/Mod/RuleCountry.h"
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
#include "../../src/Savegame/SoldierDiary.h"
#include "../../src/Savegame/SoldierDeath.h"
#include "../../src/Savegame/BattleUnitStatistics.h"
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
 s->getDiary()->load(YAML::Load("missionIdList: [932817, 932818]\nmonthsService: 6\ndaysWoundedTotal: 13\nshotsFiredCounterTotal: 20\nshotsLandedCounterTotal: 7"),&mod);
 for(const auto status:{STATUS_DEAD,STATUS_UNCONSCIOUS,STATUS_PANICKING,STATUS_TURNING}){auto*k=new BattleUnitKills();k->name="PRIVATE_HISTORY_NAME";k->race="PRIVATE_HISTORY_RACE";k->rank="PRIVATE_HISTORY_RANK";k->type="PRIVATE_HISTORY_TYPE";k->weapon="PRIVATE_HISTORY_WEAPON";k->id=782931;k->mission=932817;k->turn=293817;k->status=status;s->getDiary()->getKills().push_back(k);}
 auto *friendly=new BattleUnitKills();friendly->faction=FACTION_PLAYER;friendly->status=STATUS_DEAD;s->getDiary()->getKills().push_back(friendly);
 auto *medal=new SoldierCommendations("OWN_MEDAL","PRIVATE_MEDAL_NOUN");medal->addDecoration();s->getDiary()->getSoldierCommendations()->push_back(medal);
 auto *secondMedal=new SoldierCommendations("OWN_SECOND_MEDAL","PRIVATE_MEDAL_NOUN_2");s->getDiary()->getSoldierCommendations()->push_back(secondMedal);
 Soldier *mia=new Soldier(&soldierRule,&armor,2);mia->setName("MEMORIAL MIA");a->getSoldiers()->push_back(mia);save.killSoldier(mia,nullptr);
 // MonthlyCosts ledger uses native catalog order, researched filters and
 // quantities including incoming transfers. No external rules/resources.
 mod._costEngineer=30000;mod._costScientist=25000;
 soldierRule.load(YAML::Load("costSalary: 20000"),&mod);
 const auto catalogCraft=[&](const std::string &type,int cost,const std::string &requires){auto*r=new RuleCraft(type);r->load(YAML::Load("costRent: "+std::to_string(cost)+"\nrequires: "+requires),&mod,0);mod._crafts[type]=r;mod._craftsIndex.push_back(type);};
 const auto catalogSoldier=[&](const std::string &type,int cost,const std::string &requires){auto*r=new RuleSoldier(type);r->load(YAML::Load("costSalary: "+std::to_string(cost)+"\nrequires: "+requires),&mod);mod._soldiers[type]=r;mod._soldiersIndex.push_back(type);};
 catalogCraft("OWN_CRAFT",10000,"[]");catalogCraft("AVAILABLE_ZERO_CRAFT",12000,"[OWN_RESEARCH]");catalogCraft("PRIVATE_LOCKED_CRAFT",999,"[PRIVATE_UNRESEARCHED]");catalogCraft("ZERO_COST_CRAFT",0,"[]");
 catalogSoldier("OWN_SOLDIER",20000,"[]");catalogSoldier("AVAILABLE_ZERO_SOLDIER",21000,"[OWN_RESEARCH]");catalogSoldier("PRIVATE_LOCKED_SOLDIER",999,"[PRIVATE_UNRESEARCHED]");catalogSoldier("ZERO_COST_SOLDIER",0,"[]");
 save.addFinishedResearchSimple(&researchRule);
 RuleCountry countryRule("PRIVATE_COUNTRY");auto *country=new Country(&countryRule,false);country->getFunding().push_back(123456);save.getCountries()->push_back(country);
 const auto seed=RNG::getSeed();const auto funds=save.getFunds();enemy.reads=0;
 check(collector.collect(&save,&language,rows,reason),"native two-base snapshot");
 if(argc>1){std::ofstream output(argv[1]);output<<"[";for(std::size_t i=0;i<rows.size();++i){if(i)output<<",";output<<rows[i];}output<<"]\n";}
 std::map<std::string,int> kinds;std::map<std::string,YAML::Node> byId;std::string researchId,facilityId;
 for(const auto &raw:rows){auto r=YAML::Load(raw);const auto kind=r["kind"].as<std::string>();++kinds[kind];byId[r["id"].as<std::string>()]=r;if(kind=="research")researchId=r["id"].as<std::string>();if(kind=="facility"&&r["build_days_remaining"].as<int>()==7)facilityId=r["id"].as<std::string>();check(raw.find("PRIVATE_ENEMY")==std::string::npos&&raw.find("retaliation")==std::string::npos&&raw.find("destination")==std::string::npos,"no enemy graph fields");check(raw.find("PRIVATE_HISTORY")==std::string::npos&&raw.find("PRIVATE_MEDAL_NOUN")==std::string::npos&&raw.find("932817")==std::string::npos&&raw.find("782931")==std::string::npos,"history identities and medal noun excluded");}
 for(const auto &kind:{"campaign","base","facility","soldier","craft","research","production","transfer","stock","vehicle","weapon","equipment_layout","soldier_diary","soldier_commendation"})check(kinds[kind]>0,"all allowlisted record kinds populated");
 check(kinds["base"]==2&&kinds["craft"]==2&&kinds["transfer"]==5,"all bases and in-transit craft captured");
 check(byId.at("campaign_0")["monthly_income"].as<int>()==123456 && byId.at("campaign_0")["monthly_maintenance"].as<int>()==save.getBaseMaintenance(),"campaign own-income and all-base native maintenance");
 for(const auto &entry:byId){const auto &row=entry.second;if(row["kind"].as<std::string>()!="base")continue;
  const bool first=row["name"].as<std::string>()=="FIRST BASE";Base*owned=first?a:b;const auto ledger=row["expense_ledger"],crafts=ledger["crafts"],salaries=ledger["salaries"];
  check(crafts.size()==2&&salaries.size()==4,"researched nonzero catalog only, plus both staff rows");
  check(crafts[0]["type"].as<std::string>()=="OWN_CRAFT"&&crafts[1]["type"].as<std::string>()=="AVAILABLE_ZERO_CRAFT","craft native catalog order retained");
  check(salaries[0]["type"].as<std::string>()=="OWN_SOLDIER"&&salaries[1]["type"].as<std::string>()=="AVAILABLE_ZERO_SOLDIER"&&salaries[2]["type"].as<std::string>()=="STR_ENGINEERS"&&salaries[3]["type"].as<std::string>()=="STR_SCIENTISTS","salary rule IDs and explicit staff ordering");
  check(crafts[0]["quantity"].as<int>()==1&&crafts[0]["unit_cost"].as<int>()==10000&&crafts[0]["total"].as<int>()==10000,"home and incoming craft quantities charged natively");
  check(crafts[1]["quantity"].as<int>()==0&&crafts[1]["total"].as<int>()==0&&salaries[1]["quantity"].as<int>()==0,"available zero-quantity rows included");
  check(salaries[0]["quantity"].as<int>()==1&&salaries[0]["total"].as<int>()==20000,"home and incoming soldier salaries");
  check(salaries[2]["quantity"].as<int>()==owned->getTotalEngineers()&&salaries[3]["quantity"].as<int>()==owned->getTotalScientists(),"allocated and incoming native staff totals");
  long long sum=0;for(const auto &x:crafts)sum+=x["total"].as<long long>();for(const auto &x:salaries)sum+=x["total"].as<long long>();sum+=row["maintenance"]["facilities"].as<int>();
  check(sum==row["maintenance"]["total"].as<int>(),"expense rows reconcile native base maintenance");
 }
 for(const auto &raw:rows)check(raw.find("PRIVATE_LOCKED")==std::string::npos&&raw.find("PRIVATE_UNRESEARCHED")==std::string::npos&&raw.find("ZERO_COST_")==std::string::npos&&raw.find("PRIVATE_COUNTRY")==std::string::npos,"ledger omits locked and zero-cost rules and country identity");
 check(byId.at(facilityId)["build_days_remaining"].as<int>()==7,"construction time model read");
 check(byId.at("soldier_0")["recently_promoted"].as<bool>()&&byId.at("soldier_0")["death"].IsNull(),"live pure promotion and null death");
 const auto diary=byId.at("soldier_diary_0");check(diary["missions"].as<int>()==2&&diary["months_service"].as<int>()==6&&diary["days_wounded"].as<int>()==13&&diary["shots_fired"].as<int>()==20&&diary["shots_landed"].as<int>()==7&&diary["accuracy"].as<int>()==35,"exact native diary aggregates");
 check(diary["kills"].as<int>()==1&&diary["stuns"].as<int>()==1&&diary["panics"].as<int>()==1&&diary["mind_controls"].as<int>()==1,"aggregate enemy outcomes omit friendly entry and identities");
 const auto memorial=byId.at("soldier_2");check(memorial["location"].as<std::string>()=="memorial"&&memorial["base_id"].as<std::string>().empty()&&memorial["craft_id"].IsNull()&&memorial["transfer_id"].IsNull()&&memorial["death"]["status"].as<std::string>()=="mia", "campaign memorial unknown base and cause-presence MIA");
 check(memorial["death"]["time"]["year"].as<int>()==save.getTime()->getYear(),"native death timestamp");
 check(collector.collect(&save,&language,again,reason)&&again==rows,"unchanged repeated reads exact stable");
 mod._costEngineer+=100;check(collector.collect(&save,&language,again,reason)&&again!=rows,"changed salary yields changed snapshot content for controller generation");mod._costEngineer-=100;
 check(collector.collect(&save,&language,again,reason)&&again==rows,"restored salary yields exact original records");
 mod._craftsIndex.push_back("OWN_CRAFT");check(!collector.collect(&save,&language,again,reason)&&again.empty()&&reason=="invalid_base_data","duplicate ledger types fail closed");mod._craftsIndex.pop_back();
 const auto originalCatalog=mod._craftsIndex;
 for(int i=0;i<255;++i)catalogCraft("BOUND_CRAFT_"+std::to_string(i),1,"[]");
 check(!collector.collect(&save,&language,again,reason)&&again.empty()&&reason=="record_limit","ledger cardinality/bytes bounded no truncation");mod._craftsIndex=originalCatalog;
 check(collector.collect(&save,&language,again,reason)&&again==rows,"limit failure does not alter owned state or stable records");
 std::reverse(s->getDiary()->getSoldierCommendations()->begin(),s->getDiary()->getSoldierCommendations()->end());
 check(collector.collect(&save,&language,again,reason)&&again==rows,"medal reorder stable identity and snapshot");
 check(medal->isNew()&&secondMedal->isNew()&&medal->getDecorationLevelInt()==1&&s->getRecentlyPromoted(),"reads do not consume medals or promotion");
 save.setSelectedBase(1);std::reverse(save.getBases()->begin(),save.getBases()->end());std::reverse(a->getFacilities()->begin(),a->getFacilities()->end());std::reverse(b->getTransfers()->begin(),b->getTransfers()->end());
 check(collector.collect(&save,&language,again,reason)&&again==rows,"selected base and row reorder do not change snapshot");
 check(enemy.reads==0,"enemy target getters never traversed");check(RNG::getSeed()==seed&&save.getFunds()==funds,"read does not mutate RNG or funds");
 check(f->getBuildTime()==7&&research->getSpent()==12&&production->getTimeSpent()==13&&items->getHours()==9&&c->getFuel()==500&&s->getCurrentStats()->psiStrength==81,"read preserves native model state");
 check(s->isPromoted(),"read did not consume promotion flag");check(!s->getRecentlyPromoted()&&!s->isPromoted(),"ordinary UI still consumes flag exactly once");
 bool wrongThread=false;std::thread other([&]{std::vector<std::string> r;std::string why;wrongThread=!collector.collect(&save,&language,r,why)&&why=="wrong_thread";});other.join();check(wrongThread,"non-owner thread refused");
 a->removeResearch(research);check(collector.collect(&save,&language,again,reason),"removed project captured");for(const auto &raw:again)check(YAML::Load(raw)["id"].as<std::string>()!=researchId,"removed identity absent");
 auto *replacement=new ResearchProject(&researchRule,250);a->addResearch(replacement);check(collector.collect(&save,&language,again,reason),"new project captured");for(const auto &raw:again){auto r=YAML::Load(raw);if(r["kind"].as<std::string>()=="research")check(r["id"].as<std::string>()!=researchId,"retired project identity not reused");}
 b->setName(std::string(5000,'x'));check(!collector.collect(&save,&language,again,reason)&&again.empty()&&reason=="record_limit","oversized field fails whole snapshot without truncation");b->setName("SECOND BASE");
 // Native death transition is explicit fixture setup, never a collector operation.
 auto *cause=new BattleUnitKills();cause->name="PRIVATE_DEATH_KILLER";cause->race="PRIVATE_DEATH_RACE";cause->mission=932817;cause->id=782931;
 save.killSoldier(s,cause);check(collector.collect(&save,&language,again,reason),"native live-to-memorial transition");
 bool deadFound=false,diaryFound=false;std::set<std::string> originalMedals;for(const auto&r:byId)if(r.second["kind"].as<std::string>()=="soldier_commendation")originalMedals.insert(r.first);
 for(const auto &raw:again){auto r=YAML::Load(raw);check(raw.find("PRIVATE_")==std::string::npos&&raw.find("932817")==std::string::npos&&raw.find("782931")==std::string::npos,"death source secrets never serialized");const auto id=r["id"].as<std::string>();
 if(id=="soldier_0"){deadFound=true;check(r["location"].as<std::string>()=="memorial"&&r["base_id"].as<std::string>().empty()&&r["death"]["status"].as<std::string>()=="kia"&&r["craft_id"].IsNull()&&r["transfer_id"].IsNull(),"native ID persists with KIA and unknown base");}
 if(id=="soldier_diary_0"){diaryFound=true;check(r["base_id"].as<std::string>().empty()&&r["missions"].as<int>()==2,"diary ID persists across death");}
 if(r["kind"].as<std::string>()=="soldier_commendation")check(originalMedals.count(id)&&r["base_id"].as<std::string>().empty()&&r["is_new"].as<bool>(),"commendation ID and new flag persist into memorial");}
 check(deadFound&&diaryFound,"dead soldier and diary both captured");
 const auto deadRows=again;std::reverse(save.getDeadSoldiers()->begin(),save.getDeadSoldiers()->end());check(collector.collect(&save,&language,again,reason)&&again==deadRows,"memorial reorder stable");
 check(cause->name=="PRIVATE_DEATH_KILLER"&&cause->mission==932817&&medal->isNew(),"reads preserve cause and medal state");
 if(argc>1){std::ofstream output(std::string(argv[1])+".memorial.json");output<<"[";for(std::size_t i=0;i<again.size();++i){if(i)output<<",";output<<again[i];}output<<"]\n";}
 collector.reset();check(collector.collect(&save,&language,again,reason),"explicit lifecycle reset accepts campaign");
 std::cout<<"PASS "<<checks<<" native all-player-bases read-only checks; synthetic in-memory rules, no Game/window/player-save execution\n";
}
