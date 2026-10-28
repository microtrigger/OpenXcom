// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentPlayerBases.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "AgentProtocol.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/GameTime.h"
#include "../Savegame/Waypoint.h"
#include "../Engine/Language.h"
#include "../Engine/Options.h"
#include "../Mod/ArticleDefinition.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDiary.h"
#include "../Savegame/SoldierDeath.h"
#include "../Savegame/MissionStatistics.h"
#include "../Savegame/BattleUnitStatistics.h"
#include "../Mod/RuleCommendations.h"
#include "../Engine/LocalizedText.h"
#include "../Savegame/Craft.h"
#include "../Savegame/CraftWeapon.h"
#include "../Savegame/Vehicle.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/Production.h"
#include "../Savegame/Transfer.h"
#include "../Savegame/EquipmentLayoutItem.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleSoldier.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleCraftWeapon.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/RuleManufacture.h"
#include "../Mod/Armor.h"
#include "../Mod/Unit.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace OpenXcom { namespace Observer {
namespace {
const std::size_t MaxRecords = 100000, MaxRecordBytes = 16384, MaxTotalBytes = 16 * 1024 * 1024;
std::string quote(const std::string &value) {
    if (value.size() > 4096) throw std::runtime_error("record_limit");
    return agentJsonString(value);
}
struct Object {
    std::string value = "{";
    Object &raw(const char *key, const std::string &v) { if(value.size()>1)value+=",";value+=quote(key)+":"+v;return *this; }
    Object &s(const char *key,const std::string &v){return raw(key,quote(v));}
    template<typename T> Object &n(const char *key,T v){std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17)<<v;return raw(key,s.str());}
    Object &d(const char *key,double v){if(!std::isfinite(v))throw std::runtime_error("invalid_base_data");return n(key,v);}
    Object &b(const char *key,bool v){return raw(key,v?"true":"false");}
    Object &nullable(const char *key,const std::string &v){return raw(key,v.empty()?"null":quote(v));}
    std::string end()const{return value+"}";}
};
Object record(const char *kind,const std::string &id,const std::string &base){return Object().s("kind",kind).s("id",id).s("base_id",base);}
std::string stats(const UnitStats *s,bool psiStrengthVisible,bool psiSkillVisible){
    if(!s)throw std::runtime_error("invalid_base_data");
    return Object().n("tu",s->tu).n("stamina",s->stamina).n("health",s->health).n("bravery",s->bravery)
        .n("reactions",s->reactions).n("firing",s->firing).n("throwing",s->throwing).n("strength",s->strength)
        .raw("psi_strength",psiStrengthVisible?std::to_string(s->psiStrength):"null")
        .raw("psi_skill",psiSkillVisible?std::to_string(s->psiSkill):"null").n("melee",s->melee).end();
}
std::string quantities(const std::map<std::string,int> &items){
    Object out; if(items.size()>1000)throw std::runtime_error("record_limit");
    for(const auto &item:items)out.raw(item.first.c_str(),std::to_string(item.second));return out.end();
}
std::string capacity(double total,double used){return Object().d("capacity",total).d("used",used).d("free",total-used).end();}
}
void AgentPlayerBases::reset(){_identities.clear();_logicalIdentities.clear();_seen.clear();_save=nullptr;_next=0;_thread=std::thread::id();}
std::string AgentPlayerBases::identify(const std::string &kind,const void *pointer,const std::weak_ptr<const void> &lifetime,bool tracked){
    if(!pointer)throw std::runtime_error("invalid_base_data");
    const Key key(kind,pointer);_seen.insert(key);auto found=_identities.find(key);
    if(found!=_identities.end() && (!tracked || (!found->second.lifetime.expired() &&
        !found->second.lifetime.owner_before(lifetime) && !lifetime.owner_before(found->second.lifetime)))) return found->second.id;
    if(_next>=9007199254740991ULL || _identities.size()>=MaxRecords)throw std::runtime_error("record_limit");
    Identity value;value.id=kind+"_"+std::to_string(++_next);value.lifetime=lifetime;value.tracked=tracked;_identities[key]=value;return value.id;
}
std::string AgentPlayerBases::logicalIdentity(const std::string &kind,const std::string &scope,const std::string &type){
    if(type.empty() || type.size()>4096)throw std::runtime_error("invalid_base_data");
    const auto key=std::make_tuple(kind,scope,type);const auto found=_logicalIdentities.find(key);
    if(found!=_logicalIdentities.end())return found->second;
    if(_next>=9007199254740991ULL || _logicalIdentities.size()>=MaxRecords)throw std::runtime_error("record_limit");
    const auto id=kind+"_"+std::to_string(++_next);_logicalIdentities[key]=id;return id;
}
bool AgentPlayerBases::collect(SavedGame *save,Language *language,std::vector<std::string> &records,std::string &reason){
    return collect(save,language,records,reason,nullptr);
}
bool AgentPlayerBases::collect(SavedGame *save,Language *language,std::vector<std::string> &records,std::string &reason,const Mod *mod){
    records.clear();reason.clear();
    if(_thread!=std::thread::id() && _thread!=std::this_thread::get_id()){reason="wrong_thread";return false;}
    if(!save){reason="no_campaign";return false;}
    if(_save!=save){reset();_save=save;} _thread=std::this_thread::get_id();_seen.clear();
    try {
        if(!language)throw std::runtime_error("invalid_base_data");
        const auto *bases=static_cast<const SavedGame*>(save)->getBases();
        if(!bases || bases->size()>64)throw std::runtime_error("record_limit");
        std::map<Waypoint*,std::string> waypointIds;
        std::map<Base*,std::string> baseIds;std::map<Craft*,std::string> craftIds;std::map<Soldier*,std::string> soldierIds;
        std::map<std::string,Soldier*> nativeSoldiers;
        const auto soldierId=[&](Soldier *s)->std::string {
            if(!s)throw std::runtime_error("invalid_base_data");const auto id="soldier_"+std::to_string(s->getId());
            auto f=nativeSoldiers.find(id);if(f!=nativeSoldiers.end()&&f->second!=s)throw std::runtime_error("invalid_base_data");
            nativeSoldiers[id]=s;soldierIds[s]=id;return id;
        };
        for(Base *base:*bases){
            if(!base || baseIds.count(base))throw std::runtime_error("invalid_base_data");
            baseIds[base]=identify("base",base,base->agentLifetime(),true);
            if(base->getSoldiers()->size()>MaxRecords || base->getCrafts()->size()>MaxRecords || base->getTransfers()->size()>MaxRecords)throw std::runtime_error("record_limit");
            for(Soldier *s:*base->getSoldiers())soldierId(s);
            for(Craft *c:*base->getCrafts()){if(!c)throw std::runtime_error("invalid_base_data");craftIds[c]=identify("craft",c,c->agentLifetime(),true);}
            for(Transfer *t:*base->getTransfers()){
                if(!t)throw std::runtime_error("invalid_base_data");
                if(t->getSoldier())soldierId(t->getSoldier());
                if(Craft*c=t->getCraft())craftIds[c]=identify("craft",c,c->agentLifetime(),true);
            }
        }
        if(save->getWaypoints()->size()>MaxRecords)throw std::runtime_error("record_limit");
        for(Waypoint *waypoint:*save->getWaypoints()){
            if(!waypoint || waypointIds.count(waypoint))throw std::runtime_error("invalid_base_data");
            waypointIds[waypoint]=identify("own_waypoint",waypoint,waypoint->agentLifetime(),true);
        }
        if(save->getDeadSoldiers()->size()>MaxRecords)throw std::runtime_error("record_limit");
        for(Soldier *s:*save->getDeadSoldiers()){
            if(!s || !s->getDeath() || soldierIds.count(s))throw std::runtime_error("invalid_base_data");
            soldierId(s);
        }
        std::size_t total=0, historyWork=0;std::set<std::string> recordIds;
        const auto emit=[&](const std::string &id,const Object &object){
            const auto json=object.end();if(json.size()>MaxRecordBytes || records.size()>=MaxRecords || total+json.size()>MaxTotalBytes)throw std::runtime_error("record_limit");
            if(!recordIds.insert(id).second)throw std::runtime_error("invalid_base_data");total+=json.size();records.push_back(json);
        };
        const auto *time=save->getTime();if(!time)throw std::runtime_error("invalid_base_data");
        Object date;date.n("year",time->getYear()).n("month",time->getMonth()).n("day",time->getDay()).n("hour",time->getHour()).n("minute",time->getMinute()).n("second",time->getSecond());
        emit("campaign_0",record("campaign","campaign_0","").n("funds",save->getFunds()).n("monthly_income",save->getCountryFunding()).n("monthly_maintenance",save->getBaseMaintenance()).raw("time",date.end()));
        if(save->getDiscoveredResearch().size()>MaxRecords)throw std::runtime_error("record_limit");
        std::set<std::string> discoveredTypes;
        for(const RuleResearch *rule:save->getDiscoveredResearch()){
            if(!rule)throw std::runtime_error("invalid_base_data");
            const auto &type=rule->getName();if(!discoveredTypes.insert(type).second)continue;
            const auto id=logicalIdentity("research_knowledge","",type);
            emit(id,record("research_knowledge",id,"").s("type",type).s("name",language->getString(type)));
        }
        for(const auto &waypoint:waypointIds){
            emit(waypoint.second,record("own_waypoint",waypoint.second,"").n("native_id",waypoint.first->getId())
                .s("name",waypoint.first->getName(language)).d("longitude",waypoint.first->getLongitude()).d("latitude",waypoint.first->getLatitude()));
        }
        // Only the completed campaign archive is consulted here, never the
        // current battle, enemy targets, positions or tactical object graph.
        auto *missionStatistics=save->getMissionStatistics();
        if(!missionStatistics || missionStatistics->size()>MaxRecords)throw std::runtime_error("record_limit");
        std::map<int,const MissionStatistics*> completedMissions;
        std::map<int,std::string> missionIds;
        for(const auto *mission:*missionStatistics){
            if(!mission || mission->id<0 || !completedMissions.emplace(mission->id,mission).second)throw std::runtime_error("invalid_base_data");
        }
        const auto historyIdentity=[&](const std::string &kind,const std::string &scope,const std::string &key){
            if(key.size()>4096)throw std::runtime_error("record_limit");
            return logicalIdentity(kind,scope,key);
        };
        for(const auto &entry:completedMissions){
            const auto *mission=entry.second;const auto id=historyIdentity("mission_history","",std::to_string(entry.first));missionIds[entry.first]=id;
            const auto &t=mission->time;
            const auto when=Object().n("year",t.getYear()).n("month",t.getMonth()).n("day",t.getDay()).n("hour",t.getHour()).n("minute",t.getMinute()).n("second",t.getSecond()).end();
            emit(id,record("mission_history",id,"").n("native_id",mission->id).s("title",mission->getMissionName(language)).s("type",mission->type)
                .raw("time",when).s("region",mission->region).s("country",mission->country)
                .raw("ufo",mission->isUfoMission()?quote(language->getString(mission->ufo)):"null")
                .b("success",mission->success).s("rating",mission->getRatingString(language)).n("score",mission->score)
                .raw("alien_race",mission->alienRace=="STR_UNKNOWN"?"null":quote(language->getString(mission->alienRace)))
                .s("daylight",mission->isDarkness()?"night":"day").n("loot_value",mission->lootValue).b("valiant_crux",mission->valiantCrux));
        }
        const Mod *historyMod=bases->empty()?nullptr:bases->front()->getMod();
        std::set<std::string> ownItemTypes;
        const auto ownItem=[&](const std::string &type){
            if(type.empty())return;
            if(type.size()>4096 || ownItemTypes.size()>=MaxRecords)throw std::runtime_error("record_limit");
            ownItemTypes.insert(type);
        };
        const auto stock=[&](ItemContainer *items,const std::string &owner,const char *kind,const std::string &base){
            if(!items || items->getContents()->size()>MaxRecords)throw std::runtime_error("record_limit");
            for(const auto &item:*items->getContents()){
                if(item.second>0)ownItem(item.first);
                const auto id=identify("stock",&item);emit(id,record("stock",id,base).s("owner_id",owner).s("owner_kind",kind).s("type",item.first).n("quantity",item.second));
            }
        };
        const auto vehicles=[&](const std::vector<Vehicle*> &list,const std::string &owner,const char *kind,const std::string &base){
            if(list.size()>MaxRecords)throw std::runtime_error("record_limit");
            for(Vehicle *v:list){if(!v || !v->getRules())throw std::runtime_error("invalid_base_data");const auto id=identify("vehicle",v);
                // Base defense vehicles may alias loaded craft vehicles. Only the
                // authoritative first owner gets a record; no tactical graph.
                if(recordIds.count(id))continue;
                ownItem(v->getRules()->getType());
                emit(id,record("vehicle",id,base).s("owner_id",owner).s("owner_kind",kind).s("type",v->getRules()->getType()).n("ammo",v->getAmmo()).n("size",v->getSize()));
            }
        };
        const auto soldier=[&](Soldier *s,const std::string &base,const std::string &transfer){
            if(!s || !s->getRules() || !s->getArmor())throw std::runtime_error("invalid_base_data");const auto id=soldierIds.at(s);
            // Match SoldierInfoState, including memorial/in-transit records and
            // initial stats: owning a soldier does not reveal untrained psi.
            const bool psiSkillVisible=s->getCurrentStats()->psiSkill>0;
            const bool psiStrengthVisible=psiSkillVisible || (mod && Options::psiStrengthEval && save->isResearched(mod->getPsiRequirements()));
            const auto craft=craftIds.find(s->getCraft());
            const SoldierDeath *death=s->getDeath();
            if((base.empty() && (!death || s->getCraft() || !transfer.empty())) || (!base.empty() && death))throw std::runtime_error("invalid_base_data");
            std::string deathJson="null";
            if(death){
                const auto *t=death->getTime();if(!t)throw std::runtime_error("invalid_base_data");
                Object date;date.n("year",t->getYear()).n("month",t->getMonth()).n("day",t->getDay()).n("hour",t->getHour()).n("minute",t->getMinute()).n("second",t->getSecond());
                // The memorial UI uses presence only for KIA/MIA. Never inspect
                // the cause's enemy identity, tactical ID, mission or turn.
                deathJson=Object().s("status",death->getCause()?"kia":"mia").raw("time",date.end()).end();
            }
            emit(id,record("soldier",id,base).n("native_id",s->getId()).s("type",s->getRules()->getType()).s("name",s->getName(false))
                .s("rank",s->getRankString()).n("rank_value",int(s->getRank())).s("armor",s->getArmor()->getType()).n("gender",int(s->getGender())).n("look",int(s->getLook()))
                .n("missions",s->getMissions()).n("kills",s->getKills()).n("recovery_days",s->getWoundRecovery()).b("psi_training",s->isInPsiTraining())
                .raw("psi_improvement",psiSkillVisible?std::to_string(s->getImprovement()):"null").raw("psi_strength_improvement",psiStrengthVisible?std::to_string(s->getPsiStrImprovement()):"null").nullable("craft_id",craft==craftIds.end()?"":craft->second)
                .s("location",base.empty()?"memorial":(transfer.empty()?"home":"in_transit")).nullable("transfer_id",transfer).raw("initial_stats",stats(s->getInitStats(),psiStrengthVisible,psiSkillVisible)).raw("current_stats",stats(s->getCurrentStats(),psiStrengthVisible,psiSkillVisible))
                .b("recently_promoted",s->getRecentlyPromoted()).raw("death",deathJson));
            SoldierDiary *diary=s->getDiary();if(!diary)throw std::runtime_error("invalid_base_data");
            const auto &kills=diary->getKills();const auto *medals=diary->getSoldierCommendations();
            if(!medals || kills.size()>MaxRecords || medals->size()>MaxRecords || diary->getMissionIdList().size()>MaxRecords)throw std::runtime_error("record_limit");
            const auto work=kills.size()+medals->size()+diary->getMissionIdList().size();
            if(work>MaxRecords || historyWork>MaxRecords-work)throw std::runtime_error("record_limit");historyWork+=work;
            for(const auto *kill:kills)if(!kill)throw std::runtime_error("invalid_base_data");
            // Indexed equivalent of the native archive-by-diary aggregate
            // joins. Count repeated diary references exactly as native getters
            // do, but avoid quadratic work on long completed histories.
            std::set<int> ownMissions;
            std::map<std::string,int> byRegion,byCountry,byType,byUfo;
            long long wins=0,missionScore=0;
            for(int nativeId:diary->getMissionIdList()){
                const auto found=completedMissions.find(nativeId);
                if(found==completedMissions.end())throw std::runtime_error("invalid_base_data");
                ownMissions.insert(nativeId);const auto *mission=found->second;
                wins+=mission->success?1:0;missionScore+=mission->score;
                ++byRegion[mission->region];++byCountry[mission->country];++byType[mission->type];++byUfo[mission->ufo];
            }
            const auto did="soldier_diary_"+std::to_string(s->getId());
            const auto fired=diary->getShotsFiredTotal(),landed=diary->getShotsLandedTotal();
            emit(did,record("soldier_diary",did,base).s("soldier_id",id).n("missions",diary->getMissionTotal()).n("months_service",diary->getMonthsService())
                .n("days_wounded",diary->getDaysWoundedTotal()).n("shots_fired",fired).n("shots_landed",landed).n("accuracy",fired?100LL*landed/fired:0)
                .n("kills",diary->getKillTotal()).n("stuns",diary->getStunTotal()).n("panics",diary->getPanickTotal()).n("mind_controls",diary->getControlTotal())
                .n("wins",wins).n("score",missionScore)
                .raw("neutralizations_by_race",quantities(diary->getAlienRaceTotal())).raw("neutralizations_by_rank",quantities(diary->getAlienRankTotal()))
                .raw("hostile_neutralizations_by_weapon",quantities(diary->getWeaponTotal())).raw("hostile_neutralizations_by_ammo",quantities(diary->getWeaponAmmoTotal()))
                .raw("missions_by_region",quantities(byRegion)).raw("missions_by_country",quantities(byCountry))
                .raw("missions_by_type",quantities(byType)).raw("missions_by_ufo",quantities(byUfo)));
            for(int nativeId:ownMissions){
                const auto found=completedMissions.find(nativeId);
                const auto relation=historyIdentity("soldier_mission",id,std::to_string(nativeId));
                const auto wound=found->second->injuryList.find(s->getId());
                emit(relation,record("soldier_mission",relation,base).s("soldier_id",id).s("mission_id",missionIds.at(nativeId))
                    .n("wounded_days",wound==found->second->injuryList.end()?0:wound->second));
            }
            std::map<std::string,std::size_t> occurrences;
            for(const auto *kill:kills){
                if(!ownMissions.count(kill->mission) || !completedMissions.count(kill->mission))throw std::runtime_error("invalid_base_data");
                // Identity uses only fields that produce the native history
                // caption, never a tactical unit ID, turn, faction or position.
                Object identity;identity.n("mission",kill->mission).s("status",kill->getKillStatusString()).s("weapon",kill->weapon);
                if(!kill->name.empty())identity.s("name",kill->name);
                else if(!kill->type.empty())identity.s("type",kill->type);
                else identity.s("race",kill->race).s("rank",kill->rank);
                const auto key=identity.end();if(key.size()>4096)throw std::runtime_error("record_limit");
                const auto occurrence=occurrences[key]++;
                const auto encounter=historyIdentity("soldier_encounter",id,key+":"+std::to_string(occurrence));
                emit(encounter,record("soldier_encounter",encounter,base).s("soldier_id",id).s("mission_id",missionIds.at(kill->mission))
                    .s("status",language->getString(kill->getKillStatusString())).s("unit_name",kill->getUnitName(language)).s("weapon",language->getString(kill->weapon)));
            }
            for(const SoldierCommendations *medal:*medals){
                if(!medal)throw std::runtime_error("invalid_base_data");const auto mid=identify("soldier_commendation",medal);
                const bool hasNoun=medal->getNoun()!="noNoun";
                const std::string noun=hasNoun?std::string(language->getString(medal->getNoun())):std::string();
                const std::string name=hasNoun?std::string(language->getString(medal->getType()).arg(noun)):std::string(language->getString(medal->getType()));
                const RuleCommendations *rule=nullptr;
                if(historyMod){const auto &rules=historyMod->getCommendationsList();const auto found=rules.find(medal->getType());if(found!=rules.end())rule=found->second;}
                std::string description="null";
                if(rule)description=quote(hasNoun?std::string(language->getString(rule->getDescription()).arg(noun)):std::string(language->getString(rule->getDescription())));
                emit(mid,record("soldier_commendation",mid,base).s("soldier_id",id).s("type",medal->getType()).n("decoration_level",medal->getDecorationLevelInt()).b("is_new",medal->isNew())
                    .raw("noun",hasNoun?quote(noun):"null").s("name",name).raw("description",description).s("decoration_description",language->getString(medal->getDecorationDescription())));
            }
            if(s->getEquipmentLayout()->size()>MaxRecords)throw std::runtime_error("record_limit");
            for(EquipmentLayoutItem *e:*s->getEquipmentLayout()){
                if(!e)throw std::runtime_error("invalid_base_data");const auto eid=identify("equipment_layout",e);
                if(!base.empty()){ownItem(e->getItemType());if(e->getAmmoItem()!="NONE")ownItem(e->getAmmoItem());}
                emit(eid,record("equipment_layout",eid,base).s("soldier_id",id).s("item_type",e->getItemType()).s("slot",e->getSlot()).n("x",e->getSlotX()).n("y",e->getSlotY()).s("ammo_item",e->getAmmoItem()).n("fuse_timer",e->getFuseTimer()));
            }
        };
        const auto craft=[&](Craft *c,const std::string &base,const std::string &transfer){
            if(!c || !c->getRules())throw std::runtime_error("invalid_base_data");const auto id=craftIds.at(c);const auto *r=c->getRules();
            std::vector<std::string> crew;for(const auto &s:soldierIds)if(s.first->getCraft()==c)crew.push_back(s.second);std::sort(crew.begin(),crew.end());
            std::string crewJson="[";for(const auto &s:crew){if(crewJson.size()>1)crewJson+=",";crewJson+=quote(s);}crewJson+="]";
            // Compare target addresses only against explicit player-owned registries.
            // Never ask an unrecognized destination for type, name, position or ID.
            const Target *destination=c->getDestination();std::string destinationKind=destination?"other":"none",destinationBase,destinationWaypoint;
            for(const auto &entry:baseIds)if(destination==entry.first){destinationKind="base";destinationBase=entry.second;break;}
            for(const auto &entry:waypointIds)if(destination==entry.first){destinationKind="waypoint";destinationWaypoint=entry.second;break;}
            const auto route=Object().s("kind",destinationKind).nullable("base_id",destinationBase).nullable("waypoint_id",destinationWaypoint).end();
            const auto home=baseIds.find(c->getBase());std::string homeId,distance="null",fuelHome="null";
            if(home!=baseIds.end()){
                homeId=home->second;
                const double radians=c->getDistance(home->first);
                if(std::isfinite(radians) && radians>=0){
                    std::ostringstream number;number.imbue(std::locale::classic());number<<std::setprecision(17)<<radians;distance=number.str();
                    const double denominator=c->agentMaxSpeedRadian();
                    if(std::isfinite(denominator) && denominator>0){
                        const double estimate=std::floor(c->getFuelConsumption(r->getMaxSpeed())*radians/denominator);
                        if(std::isfinite(estimate) && estimate>=std::numeric_limits<int>::min() && estimate<=std::numeric_limits<int>::max())
                            fuelHome=std::to_string(c->getFuelLimit(home->first));
                    }
                }
            }
            emit(id,record("craft",id,base).n("native_id",c->getId()).s("type",c->getType()).s("name",c->getName(language)).s("status",c->getStatus())
                .s("location",transfer.empty()?"home_roster":"in_transit").nullable("transfer_id",transfer).n("weapon_slot_count",c->getWeapons()->size()).n("weapon_capacity",r->getWeapons()).d("longitude",c->getLongitude()).d("latitude",c->getLatitude())
                .n("speed",c->getSpeed()).n("fuel",c->getFuel()).n("fuel_max",r->getMaxFuel()).n("damage",c->getDamage()).n("damage_max",r->getMaxDamage()).n("speed_max",r->getMaxSpeed())
                .n("soldier_capacity",r->getSoldiers()).n("vehicle_capacity",r->getVehicles()).n("item_capacity",r->getMaxItems()).n("space_used",c->getSpaceUsed()).n("space_available",c->getSpaceAvailable())
                .n("monthly_cost",r->getRentCost()).b("low_fuel",c->getLowFuel()).b("mission_complete",c->getMissionComplete()).raw("crew",crewJson)
                .nullable("home_base_id",homeId).n("fuel_consumption",c->getFuelConsumption()).raw("fuel_to_home",fuelHome).raw("distance_to_home",distance)
                .b("in_dogfight",c->isInDogfight()).b("in_battlescape",c->isInBattlescape()).n("interception_order",c->getInterceptionOrder()).raw("destination",route));
            stock(c->getItems(),id,"craft",base);vehicles(*c->getVehicles(),id,"craft",base);
            if(c->getWeapons()->size()>MaxRecords)throw std::runtime_error("record_limit");
            for(std::size_t i=0;i<c->getWeapons()->size();++i){CraftWeapon *w=c->getWeapons()->at(i);if(!w)continue;if(!w->getRules())throw std::runtime_error("invalid_base_data");const auto wid=identify("weapon",w);const auto *wr=w->getRules();
                ownItem(wr->getLauncherItem());ownItem(wr->getClipItem());
                emit(wid,record("weapon",wid,base).s("craft_id",id).s("type",wr->getType()).n("slot",i).n("ammo",w->getAmmo()).n("ammo_max",wr->getAmmoMax()).n("rearm_rate",wr->getRearmRate()).b("rearming",w->isRearming()).s("launcher_item",wr->getLauncherItem()).s("clip_item",wr->getClipItem()));
            }
        };
        for(Base *base:*bases){const auto bid=baseIds.at(base);
            // Same researched, nonzero-cost rule rows and quantity getters as
            // MonthlyCostsState, independent of the currently selected UI base.
            const Mod *mod=base->getMod();
            if(!mod)throw std::runtime_error("invalid_base_data");
            if(mod->getResearchList().size()>MaxRecords || mod->getManufactureList().size()>MaxRecords)throw std::runtime_error("record_limit");
            // Match the ordinary NewResearchListState query and its final
            // requires filter. Offers are not a promise of resources/capacity.
            std::vector<RuleResearch*> researchOffers;
            save->getAvailableResearchProjects(researchOffers,mod,base,true);
            std::set<std::string> researchTypes;
            for(const RuleResearch *rule:researchOffers){
                if(!rule)throw std::runtime_error("invalid_base_data");
                if(!rule->getRequirements().empty())continue;
                const auto &type=rule->getName();if(!researchTypes.insert(type).second)continue;
                const auto id=logicalIdentity("research_offer",bid,type);
                emit(id,record("research_offer",id,bid).s("type",type).s("name",language->getString(type))
                    .n("base_cost",rule->getCost()).b("needs_item",rule->needItem()).b("destroys_item",rule->destroyItem()));
            }
            std::vector<RuleManufacture*> manufactureOffers;
            save->getAvailableProductions(manufactureOffers,mod,base);
            std::set<std::string> manufactureTypes;
            for(const RuleManufacture *rule:manufactureOffers){
                if(!rule)throw std::runtime_error("invalid_base_data");
                const auto type=rule->getName();if(!manufactureTypes.insert(type).second)continue;
                const auto id=logicalIdentity("manufacture_offer",bid,type);
                emit(id,record("manufacture_offer",id,bid).s("type",type).s("name",language->getString(type)).s("category",rule->getCategory())
                    .n("required_space",rule->getRequiredSpace()).n("hours_per_unit",rule->getManufactureTime()).n("cost_per_unit",rule->getManufactureCost())
                    .raw("required_items",quantities(rule->getRequiredItems())).raw("produced_items",quantities(rule->getProducedItems())));
            }
            std::string craftLedger="[",salaryLedger="[";
            std::set<std::string> craftTypes,salaryTypes;
            const auto appendCost=[&](std::string &list,std::set<std::string> &types,const std::string &type,int unit,int quantity){
                if(types.size()>=256)throw std::runtime_error("record_limit");
                if(type.empty() || quantity<0 || !types.insert(type).second)throw std::runtime_error("invalid_base_data");
                const std::int64_t total=std::int64_t(unit)*quantity;
                if(total>9007199254740991LL || total< -9007199254740991LL)throw std::runtime_error("invalid_base_data");
                if(list.size()>1)list+=",";
                list+=Object().s("type",type).n("unit_cost",unit).n("quantity",quantity).n("total",total).end();
                if(list.size()>MaxRecordBytes)throw std::runtime_error("record_limit");
            };
            if(mod->getCraftsList().size()>MaxRecords || mod->getSoldiersList().size()>MaxRecords)throw std::runtime_error("record_limit");
            for(const auto &type:mod->getCraftsList()){
                const RuleCraft *rule=mod->getCraft(type);
                if(!rule)throw std::runtime_error("invalid_base_data");
                if(rule->getRentCost()!=0 && save->isResearched(rule->getRequirements()))
                    appendCost(craftLedger,craftTypes,type,rule->getRentCost(),base->getCraftCount(type));
            }
            for(const auto &type:mod->getSoldiersList()){
                const RuleSoldier *rule=mod->getSoldier(type);
                if(!rule)throw std::runtime_error("invalid_base_data");
                if(rule->getSalaryCost()!=0 && save->isResearched(rule->getRequirements()))
                    appendCost(salaryLedger,salaryTypes,type,rule->getSalaryCost(),base->getSoldierCount(type));
            }
            appendCost(salaryLedger,salaryTypes,"STR_ENGINEERS",mod->getEngineerCost(),base->getTotalEngineers());
            appendCost(salaryLedger,salaryTypes,"STR_SCIENTISTS",mod->getScientistCost(),base->getTotalScientists());
            const auto ledger=Object().raw("crafts",craftLedger+"]").raw("salaries",salaryLedger+"]").end();
            Object personnel;personnel.n("soldiers",base->getTotalSoldiers()).n("scientists",base->getTotalScientists()).n("engineers",base->getTotalEngineers())
                .n("available_soldiers",base->getAvailableSoldiers()).n("available_scientists",base->getAvailableScientists()).n("available_engineers",base->getAvailableEngineers())
                .n("allocated_scientists",base->getAllocatedScientists()).n("allocated_engineers",base->getAllocatedEngineers());
            Object capacities;capacities.raw("quarters",capacity(base->getAvailableQuarters(),base->getUsedQuarters())).raw("stores",capacity(base->getAvailableStores(),base->getUsedStores()))
                .raw("laboratories",capacity(base->getAvailableLaboratories(),base->getUsedLaboratories())).raw("workshops",capacity(base->getAvailableWorkshops(),base->getUsedWorkshops()))
                .raw("hangars",capacity(base->getAvailableHangars(),base->getUsedHangars())).raw("psi_labs",capacity(base->getAvailablePsiLabs(),base->getUsedPsiLabs())).raw("containment",capacity(base->getAvailableContainment(),base->getUsedContainment()));
            Object maintenance;maintenance.n("craft",base->getCraftMaintenance()).n("personnel",base->getPersonnelMaintenance()).n("facilities",base->getFacilityMaintenance()).n("total",base->getMonthlyMaintenace());
            emit(bid,record("base",bid,bid).s("name",base->getName(language)).d("longitude",base->getLongitude()).d("latitude",base->getLatitude()).n("grid_size",6)
                .raw("expense_ledger",ledger).raw("personnel",personnel.end()).raw("capacities",capacities.end()).raw("maintenance",maintenance.end()).n("defense",base->getDefenseValue()).n("short_range_detection",base->getShortRangeDetection()).n("long_range_detection",base->getLongRangeDetection()));
            if(base->getFacilities()->size()>MaxRecords || base->getResearch().size()>MaxRecords || base->getProductions().size()>MaxRecords)throw std::runtime_error("record_limit");
            for(BaseFacility *f:*base->getFacilities()){
                if(!f || !f->getRules())throw std::runtime_error("invalid_base_data");const auto id=identify("facility",f);const auto *r=f->getRules();
                Object caps;caps.n("quarters",r->getPersonnel()).n("stores",r->getStorage()).n("laboratories",r->getLaboratories()).n("workshops",r->getWorkshops()).n("hangars",r->getCrafts()).n("psi_labs",r->getPsiLaboratories()).n("containment",r->getAliens());
                emit(id,record("facility",id,bid).s("type",r->getType()).n("x",f->getX()).n("y",f->getY()).n("size",r->getSize()).n("build_days_remaining",f->getBuildTime()).n("build_days_total",r->getBuildTime()).n("build_cost",r->getBuildCost()).n("monthly_cost",r->getMonthlyCost()).raw("capacities",caps.end())
                    .n("radar_range",r->getRadarRange()).n("radar_chance",r->getRadarChance()).n("defense",r->getDefenseValue()).n("hit_ratio",r->getHitRatio()).b("lift",r->isLift()).b("hyperwave",r->isHyperwave()).b("mind_shield",r->isMindShield()).b("grav_shield",r->isGravShield()));
            }
            for(Soldier *s:*base->getSoldiers())soldier(s,bid,"");for(Craft *c:*base->getCrafts())craft(c,bid,"");
            stock(base->getStorageItems(),bid,"base",bid);vehicles(*base->getVehicles(),bid,"base",bid);
            for(ResearchProject *p:base->getResearch()){
                if(!p || !p->getRules())throw std::runtime_error("invalid_base_data");const auto id=identify("research",p);
                emit(id,record("research",id,bid).s("type",p->getRules()->getName()).s("progress",p->getResearchProgress()).n("assigned",p->getAssigned()).n("spent",p->getSpent()).n("cost",p->getCost()));
            }
            for(Production *p:base->getProductions()){
                if(!p || !p->getRules())throw std::runtime_error("invalid_base_data");const auto id=identify("production",p);const auto *r=p->getRules();
                emit(id,record("production",id,bid).s("type",r->getName()).s("category",r->getCategory()).n("amount_total",p->getAmountTotal()).n("amount_produced",p->getAmountProduced()).n("time_spent",p->getTimeSpent()).n("assigned_engineers",p->getAssignedEngineers()).n("unit_time",r->getManufactureTime()).n("unit_cost",r->getManufactureCost()).n("required_space",r->getRequiredSpace()).b("infinite",p->getInfiniteAmount()).b("sell",p->getSellItems()).raw("required_items",quantities(r->getRequiredItems())).raw("produced_items",quantities(r->getProducedItems())));
            }
            for(Transfer *t:*base->getTransfers()){
                const auto id=identify("transfer",t);const auto type=t->getType();const char *name=nullptr;
                switch(type){case TRANSFER_ITEM:name="item";break;case TRANSFER_CRAFT:name="craft";break;case TRANSFER_SOLDIER:name="soldier";break;case TRANSFER_SCIENTIST:name="scientist";break;case TRANSFER_ENGINEER:name="engineer";break;default:throw std::runtime_error("invalid_base_data");}
                if(type==TRANSFER_ITEM && t->getQuantity()>0)ownItem(t->getItems());
                emit(id,record("transfer",id,bid).s("transfer_type",name).n("hours",t->getHours()).n("quantity",t->getQuantity()).nullable("item_type",type==TRANSFER_ITEM?t->getItems():"").nullable("craft_id",t->getCraft()?craftIds.at(t->getCraft()):"").nullable("soldier_id",t->getSoldier()?soldierIds.at(t->getSoldier()):""));
                if(t->getSoldier())soldier(t->getSoldier(),bid,id);if(t->getCraft())craft(t->getCraft(),bid,id);
            }
        }
        for(Soldier *s:*save->getDeadSoldiers())soldier(s,"","");
        // Only tangible own stock, gear and transit references establish this
        // bounded item set. Do not recursively expand offers or rule graphs.
        const auto itemArticle=[&](const std::string &type)->const ArticleDefinition*{
            if(!historyMod)return nullptr;
            const auto *article=historyMod->getUfopaediaArticle(type,false);
            return article && article->id==type &&
                (article->getType()==UFOPAEDIA_TYPE_ITEM || article->getType()==UFOPAEDIA_TYPE_TFTD_ITEM) &&
                article->section!=UFOPAEDIA_NOT_AVAILABLE && save->isResearched(article->_requires)?article:nullptr;
        };
        const auto damageLabel=[&](ItemDamageType type)->std::string{
            const char *label="STR_UNKNOWN";
            switch(type){case DT_AP:label="STR_DAMAGE_ARMOR_PIERCING";break;case DT_IN:label="STR_DAMAGE_INCENDIARY";break;
                case DT_HE:label="STR_DAMAGE_HIGH_EXPLOSIVE";break;case DT_LASER:label="STR_DAMAGE_LASER_BEAM";break;
                case DT_PLASMA:label="STR_DAMAGE_PLASMA_BEAM";break;case DT_STUN:label="STR_DAMAGE_STUN";break;
                case DT_MELEE:label="STR_DAMAGE_MELEE";break;case DT_ACID:label="STR_DAMAGE_ACID";break;
                case DT_SMOKE:label="STR_DAMAGE_SMOKE";break;default:break;}
            return language->getString(label);
        };
        for(const auto &type:ownItemTypes){
            RuleItem *rule=historyMod?historyMod->getItem(type,false):nullptr;
            if(!rule)throw std::runtime_error("invalid_base_data");
            const bool identified=save->isResearched(rule->getRequirements());
            const bool purchasable=identified && rule->getBuyCost()!=0;
            const bool sellable=Options::canSellLiveAliens || !rule->isAlien();
            const auto logistics=Object().d("storage_size",rule->getSize())
                .raw("sell_price",sellable?std::to_string(rule->getSellCost()):"null")
                .raw("purchase_price",purchasable?std::to_string(rule->getBuyCost()):"null")
                .raw("purchase_delivery_hours",purchasable?std::to_string(rule->getTransferTime()):"null").end();
            const auto medikit=rule->getBattleType()==BT_MEDIKIT?Object().n("heal",rule->getHealQuantity())
                .n("painkiller",rule->getPainKillerQuantity()).n("stimulant",rule->getStimulantQuantity()).end():std::string("null");
            const auto physical=Object().n("weight",rule->getWeight()).n("width",rule->getInventoryWidth()).n("height",rule->getInventoryHeight())
                .n("clip_capacity",rule->getClipSize()).raw("medikit",medikit).end();
            std::string technical="null";
            if(const ArticleDefinition *article=itemArticle(type)){
                std::string modes="[";
                const auto mode=[&](const char *name,int accuracy,int tu){if(tu<=0)return;if(modes.size()>1)modes+=",";
                    modes+=Object().s("mode",name).n("accuracy",accuracy).n("tu",tu).s("tu_unit",rule->getFlatRate()?"absolute":"percent").end();};
                if(rule->getBattleType()==BT_FIREARM){mode("auto",rule->getAccuracyAuto(),rule->getTUAuto());mode("snap",rule->getAccuracySnap(),rule->getTUSnap());mode("aimed",rule->getAccuracyAimed(),rule->getTUAimed());}
                modes+="]";
                const auto *ammo=rule->getCompatibleAmmo();
                if(!ammo || ammo->size()>MaxRecords)throw std::runtime_error("record_limit");
                const auto battleType=rule->getBattleType();
                const bool ownDamage=(battleType==BT_FIREARM && ammo->empty()) || battleType==BT_AMMO || battleType==BT_GRENADE || battleType==BT_PROXIMITYGRENADE || battleType==BT_MELEE;
                std::string damage="null";
                if(ownDamage){
                    const bool showPellets=article->getType()==UFOPAEDIA_TYPE_ITEM || battleType==BT_FIREARM;
                    damage=Object().s("type",damageLabel(rule->getDamageType())).n("power",rule->getPower())
                        .raw("pellets",showPellets && rule->getShotgunPellets()!=0?std::to_string(rule->getShotgunPellets()):"null").end();
                }
                std::string compatible="[";std::set<std::string> included;
                if(battleType==BT_FIREARM)for(size_t i=0;i<std::min<size_t>(3,ammo->size());++i){const auto &ammoType=ammo->at(i);
                    if(ownItemTypes.count(ammoType) && itemArticle(ammoType) && included.insert(ammoType).second){if(compatible.size()>1)compatible+=",";compatible+=quote(ammoType);}}
                compatible+="]";
                technical=Object().s("article_id",article->id).raw("fire_modes",modes).raw("damage",damage).raw("compatible_ammo",compatible).end();
            }
            const auto id=logicalIdentity("item_definition","",type);
            emit(id,record("item_definition",id,"").s("type",type).s("base_label",language->getString(type))
                .s("inventory_label",language->getString(identified?rule->getName():"STR_ALIEN_ARTIFACT")).b("identified",identified)
                .raw("logistics",logistics).raw("physical",physical).raw("technical",technical));
        }
        std::sort(records.begin(),records.end());
        for(auto i=_identities.begin();i!=_identities.end();)if(!_seen.count(i->first))i=_identities.erase(i);else ++i;
        return true;
    } catch(const std::exception &e){records.clear();reason=std::string(e.what())=="record_limit"?"record_limit":"invalid_base_data";return false;}
}
} }
#endif
