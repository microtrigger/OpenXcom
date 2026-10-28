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
#include "../../src/Savegame/Waypoint.h"
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
struct EnemyTarget:Target {mutable unsigned reads=0;std::string getType()const override{++reads;return "PRIVATE_ENEMY_TYPE";}int getMarker()const override{++reads;return 99;}std::string getName(Language*)const override{++reads;return "PRIVATE_ENEMY_NAME";}};
static void opportunityAndRouteChecks(const std::string&output);
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
 for(const auto &raw:rows){auto r=YAML::Load(raw);const auto kind=r["kind"].as<std::string>();++kinds[kind];byId[r["id"].as<std::string>()]=r;if(kind=="research")researchId=r["id"].as<std::string>();if(kind=="facility"&&r["build_days_remaining"].as<int>()==7)facilityId=r["id"].as<std::string>();check(raw.find("PRIVATE_ENEMY")==std::string::npos&&raw.find("retaliation")==std::string::npos,"no enemy graph fields");check(raw.find("PRIVATE_HISTORY")==std::string::npos&&raw.find("PRIVATE_MEDAL_NOUN")==std::string::npos&&raw.find("932817")==std::string::npos&&raw.find("782931")==std::string::npos,"history identities and medal noun excluded");}
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
 opportunityAndRouteChecks(argc>1?std::string(argv[1])+".opportunities.json":"");
 std::cout<<"PASS "<<checks<<" native all-player-bases read-only checks; synthetic in-memory rules, no Game/window/player-save execution\n";
}

// Independent native model fixture for authorized own research/catalogue/route
// reads. All setup mutators below are test setup, never part of collection.
static void opportunityAndRouteChecks(const std::string&output)
{
 Mod mod;Language language;EnemyTarget enemy;SavedGame save;AgentPlayerBases collector;std::vector<std::string> rows,again;std::string reason;
 const auto researchRule=[&](const std::string&id,const std::string&yaml){auto*r=new RuleResearch(id);r->load(YAML::Load(yaml),0);mod._research[id]=r;mod._researchIndex.push_back(id);return r;};
 const auto productionRule=[&](const std::string&id,const std::string&yaml){auto*r=new RuleManufacture(id);r->load(YAML::Load(yaml),0);mod._manufacture[id]=r;mod._manufactureIndex.push_back(id);return r;};
 for(const auto&id:{"OWN_SAMPLE","OWN_ITEM"}){auto*r=new RuleItem(id);r->load(YAML::Load("size: 0.5"),&mod,0);mod._items[id]=r;mod._itemsIndex.push_back(id);}
 auto*known=researchRule("OWN_KNOWN","cost: 100");auto*helper=researchRule("OWN_HELPER_FLAG","cost: 0\nrequires: [OWN_KNOWN]");
 researchRule("OWN_SAMPLE","cost: 250\nneedItem: true\ndestroyItem: true\ndependencies: [OWN_KNOWN]");
 auto*active=researchRule("OWN_ACTIVE_RESEARCH","cost: 75\ndependencies: [OWN_KNOWN]");
 researchRule("OWN_OPEN_RESEARCH","cost: 200\ndependencies: [OWN_KNOWN]");
 researchRule("PRIVATE_LOCKED_TECH","cost: 999\ndependencies: [UNKNOWN_PREREQUISITE]");
 researchRule("PRIVATE_UNOFFERED_HELPER","cost: 0\nrequires: [OWN_KNOWN]");
 auto*manufacture=productionRule("OWN_MAKE","category: STR_CRAFT_ARMAMENT\nrequires: [OWN_KNOWN]\nspace: 2000\ntime: 80\ncost: 9999999\nrequiredItems: {OWN_ITEM: 20}\nproducedItems: {OWN_SAMPLE: 2}");
 productionRule("OWN_OPEN_MAKE","category: STR_EQUIPMENT\nrequires: [OWN_KNOWN]\nspace: 3\ntime: 4\ncost: 17\nproducedItems: {OWN_ITEM: 1}");
 productionRule("PRIVATE_LOCKED_MAKE","requires: [UNKNOWN_PREREQUISITE]\nspace: 1\ntime: 2\ncost: 3");
 save.addFinishedResearchSimple(known);save.addFinishedResearchSimple(helper);save.setFunds(0);
 auto*a=new Base(&mod);auto*b=new Base(&mod);a->setName("OFFER A");b->setName("OFFER B");a->setLongitude(0.2);b->setLongitude(0.4);save.getBases()->push_back(a);save.getBases()->push_back(b);a->getStorageItems()->addItem("OWN_SAMPLE",2);
 auto*project=new ResearchProject(active,75);project->setAssigned(1);project->setSpent(7);b->addResearch(project);auto*production=new Production(manufacture,3);production->setTimeSpent(2);b->addProduction(production);
 auto*point=new Waypoint;point->setId(44);point->setName("OWN WAYPOINT");point->setLongitude(0.8);point->setLatitude(0.3);save.getWaypoints()->push_back(point);
 auto*secondPoint=new Waypoint;secondPoint->setId(45);secondPoint->setName("SECOND WAYPOINT");secondPoint->setLongitude(0.9);save.getWaypoints()->push_back(secondPoint);
 RuleCraft rule("OWN_ROUTE_CRAFT");rule.load(YAML::Load("speedMax: 2000\nfuelMax: 1000\ncostRent: 0"),&mod,0);RuleCraft zeroRule("OWN_ZERO_SPEED");zeroRule.load(YAML::Load("speedMax: 0\nfuelMax: 1000"),&mod,0);
 auto*c=new Craft(&rule,a,71);c->setLongitude(0.25);c->setLatitude(0.1);c->setFuel(700);c->setSpeed(1500);c->setInDogfight(true);c->setInBattlescape(true);c->setInterceptionOrder(7);c->setDestination(&enemy);a->getCrafts()->push_back(c);
 auto*orphan=new Craft(&rule,nullptr,72);orphan->setFuel(400);a->getCrafts()->push_back(orphan);auto*zero=new Craft(&zeroRule,a,73);zero->setFuel(50);a->getCrafts()->push_back(zero);
 const auto seed=RNG::getSeed();enemy.reads=0;
 const auto collect=[&](){check(collector.collect(&save,&language,rows,reason),"native own opportunities and safe routes snapshot");};collect();
 std::map<std::string,YAML::Node> records;std::string aid,bid,wid,cid,orphanId,zeroId;for(const auto&raw:rows){auto row=YAML::Load(raw);records[row["id"].as<std::string>()]=row;const auto kind=row["kind"].as<std::string>();if(kind=="base"){if(row["name"].as<std::string>()=="OFFER A")aid=row["id"].as<std::string>();else bid=row["id"].as<std::string>();}if(kind=="own_waypoint"&&row["native_id"].as<int>()==44)wid=row["id"].as<std::string>();if(kind=="craft"){const int id=row["native_id"].as<int>();if(id==71)cid=row["id"].as<std::string>();if(id==72)orphanId=row["id"].as<std::string>();if(id==73)zeroId=row["id"].as<std::string>();}check(raw.find("PRIVATE_")==std::string::npos&&raw.find("UNKNOWN_PREREQUISITE")==std::string::npos,"unavailable tech and foreign target details absent");}
 check(!aid.empty()&&!bid.empty()&&!wid.empty()&&!cid.empty()&&!orphanId.empty()&&!zeroId.empty(),"owned stable references resolved for both bases waypoints and craft");
 std::set<std::string>knowledge;for(const auto&r:records)if(r.second["kind"].as<std::string>()=="research_knowledge"){knowledge.insert(r.second["type"].as<std::string>());check(r.second["base_id"].as<std::string>().empty(),"research knowledge has campaign scope");}
 check(knowledge==std::set<std::string>({"OWN_KNOWN","OWN_HELPER_FLAG"}),"actual discovered helper flags included without unknown research catalogue");
 for(const auto pair:{std::make_pair(a,aid),std::make_pair(b,bid)}){
  std::vector<RuleResearch*>nativeResearch;save.getAvailableResearchProjects(nativeResearch,&mod,pair.first,true);std::set<std::string>expectedResearch;for(auto*r:nativeResearch)if(r->getRequirements().empty())expectedResearch.insert(r->getName());
  std::vector<RuleManufacture*>nativeManufacture;save.getAvailableProductions(nativeManufacture,&mod,pair.first);std::set<std::string>expectedManufacture;for(auto*r:nativeManufacture)expectedManufacture.insert(r->getName());
  std::set<std::string>shownResearch,shownManufacture;for(const auto&r:records){auto row=r.second;if(row["base_id"].as<std::string>()!=pair.second)continue;const auto kind=row["kind"].as<std::string>();if(kind=="research_offer")shownResearch.insert(row["type"].as<std::string>());if(kind=="manufacture_offer")shownManufacture.insert(row["type"].as<std::string>());}
  check(shownResearch==expectedResearch&&shownManufacture==expectedManufacture,"per-base offers exactly match native UI availability queries and hidden-helper filter");
  if(pair.first==a)check(shownResearch.count("OWN_SAMPLE")&&shownResearch.count("OWN_ACTIVE_RESEARCH")&&shownManufacture.count("OWN_MAKE"),"base A has item research and opportunities absent from base B active projects");else check(!shownResearch.count("OWN_SAMPLE")&&!shownResearch.count("OWN_ACTIVE_RESEARCH")&&!shownManufacture.count("OWN_MAKE"),"base B missing item and active research/manufacture are excluded natively");
 }
 bool manufactureFound=false,researchFound=false;for(const auto&r:records){auto row=r.second;const auto kind=row["kind"].as<std::string>();if(kind=="manufacture_offer"&&row["type"].as<std::string>()=="OWN_MAKE"){manufactureFound=true;check(row["required_space"].as<int>()==2000&&row["hours_per_unit"].as<int>()==80&&row["cost_per_unit"].as<int>()==9999999&&row["required_items"]["OWN_ITEM"].as<int>()==20&&row["produced_items"]["OWN_SAMPLE"].as<int>()==2,"offer includes pure rule requirements despite zero funds materials and workshop capacity");}if(kind=="research_offer"&&row["type"].as<std::string>()=="OWN_SAMPLE"){researchFound=true;check(row["base_cost"].as<int>()==250&&row["needs_item"].as<bool>()&&row["destroys_item"].as<bool>(),"research offer exact native item and cost requirements");}}
 check(manufactureFound&&researchFound,"both typed opportunity requirement records present");
 auto craftRow=records.at(cid);check(craftRow["home_base_id"].as<std::string>()==aid&&craftRow["fuel_consumption"].as<int>()==c->getFuelConsumption()&&craftRow["fuel_to_home"].as<int>()==c->getFuelLimit()&&craftRow["distance_to_home"].as<double>()==c->getDistanceFromBase(),"own craft native fuel and distance telemetry matches getters");
 check(craftRow["in_dogfight"].as<bool>()&&craftRow["in_battlescape"].as<bool>()&&craftRow["interception_order"].as<int>()==7,"own craft engagement flags and order allowlisted");check(craftRow["destination"]["kind"].as<std::string>()=="other"&&craftRow["destination"]["base_id"].IsNull()&&craftRow["destination"]["waypoint_id"].IsNull()&&enemy.reads==0,"foreign destination classified by pointer only without target getters");
 check(records.at(orphanId)["home_base_id"].IsNull()&&records.at(orphanId)["fuel_to_home"].IsNull()&&records.at(orphanId)["distance_to_home"].IsNull(),"null home craft avoids unsafe native distance and fuel estimation");check(records.at(zeroId)["fuel_to_home"].IsNull(),"zero max-speed craft avoids division-by-zero native fuel estimate");
 const auto original=rows;auto&discovered=const_cast<std::vector<const RuleResearch*>&>(save.getDiscoveredResearch());std::reverse(discovered.begin(),discovered.end());std::reverse(mod._researchIndex.begin(),mod._researchIndex.end());std::reverse(mod._manufactureIndex.begin(),mod._manufactureIndex.end());std::reverse(save.getBases()->begin(),save.getBases()->end());std::reverse(save.getWaypoints()->begin(),save.getWaypoints()->end());collect();check(rows==original,"knowledge rule base and waypoint reorder retain exact stable IDs and serialized snapshot");
 check(records.at(wid)["base_id"].as<std::string>().empty()&&records.at(wid)["name"].as<std::string>()=="OWN WAYPOINT"&&records.at(wid)["longitude"].as<double>()==point->getLongitude()&&records.at(wid)["latitude"].as<double>()==point->getLatitude(),"own waypoint record contains only own scoped name and coordinates");
 a->getStorageItems()->removeItem("OWN_SAMPLE",2);collect();bool sampleAbsent=true;for(const auto&raw:rows){auto row=YAML::Load(raw);if(row["kind"].as<std::string>()=="research_offer"&&row["type"].as<std::string>()=="OWN_SAMPLE")sampleAbsent=false;}check(sampleAbsent,"needed-item research opportunity disappears with missing owned item");a->getStorageItems()->addItem("OWN_SAMPLE",2);collect();std::set<std::string> beforeOffers,afterOffers;for(const auto&raw:original){auto row=YAML::Load(raw);if(row["kind"].as<std::string>()=="research_offer"||row["kind"].as<std::string>()=="manufacture_offer")beforeOffers.insert(raw);}for(const auto&raw:rows){auto row=YAML::Load(raw);if(row["kind"].as<std::string>()=="research_offer"||row["kind"].as<std::string>()=="manufacture_offer")afterOffers.insert(raw);}check(afterOffers==beforeOffers,"reappearing opportunity retains same logical identity and exact offer records (recreated stock has new identity)");
 const auto routed=[&](Target*target,const std::string&kind,const std::string&reference){c->setDestination(target);enemy.reads=0;collect();for(const auto&raw:rows){auto row=YAML::Load(raw);if(row["id"].as<std::string>()!=cid)continue;auto dest=row["destination"];check(dest["kind"].as<std::string>()==kind,"destination enum from owned pointer membership");if(kind=="base")check(dest["base_id"].as<std::string>()==reference&&dest["waypoint_id"].IsNull(),"own base route binds known base ID only");if(kind=="waypoint")check(dest["waypoint_id"].as<std::string>()==reference&&dest["base_id"].IsNull(),"own waypoint route binds known waypoint ID only");if(kind=="none")check(dest["base_id"].IsNull()&&dest["waypoint_id"].IsNull(),"no route has nullable references");}check(enemy.reads==0,"route capture never invokes foreign target getters");};
 routed(b,"base",bid);routed(point,"waypoint",wid);routed(nullptr,"none","");routed(&enemy,"other","");
 check(save.getFunds()==0&&RNG::getSeed()==seed&&a->getStorageItems()->getItem("OWN_SAMPLE")==2&&project->getSpent()==7&&production->getTimeSpent()==2&&c->getFuel()==700,"catalogue telemetry reads preserve funds RNG stocks research production and fuel");
 check(collector.collect(&save,&language,again,reason)&&again==rows,"repeated final offer/route reads exact stable");
 if(!output.empty()){std::ofstream file(output);file<<"[";for(size_t i=0;i<rows.size();++i)file<<(i?",":"")<<rows[i];file<<"]\n";}
}
