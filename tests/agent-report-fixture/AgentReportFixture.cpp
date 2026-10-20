// SPDX-License-Identifier: GPL-3.0-or-later
// Artificial isolated setup only. Never called by MCP or after setup completes.
#include "AgentReportFixture.h"
#include "AgentVehicleFixture.h"
#include "AgentInventoryFixture.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Screen.h"
#include "../../src/Engine/Exception.h"
#include "../../src/Savegame/SavedGame.h"
#include "../../src/Savegame/SavedBattleGame.h"
#include "../../src/Savegame/Base.h"
#include "../../src/Savegame/Craft.h"
#include "../../src/Savegame/Soldier.h"
#include "../../src/Savegame/BattleUnit.h"
#include "../../src/Savegame/BattleItem.h"
#include "../../src/Savegame/Tile.h"
#include "../../src/Mod/Mod.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Mod/RuleSoldier.h"
#include "../../src/Mod/RuleItem.h"
#include "../../src/Mod/Unit.h"
#include "../../src/Mod/MapData.h"
#include "../../src/Mod/MapDataSet.h"
#include "../../src/Battlescape/BattlescapeState.h"
#include "../../src/Battlescape/Pathfinding.h"
#include "../../src/Battlescape/TileEngine.h"
#include "../../src/Geoscape/GeoscapeState.h"
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
namespace OpenXcom { namespace AgentReportFixture {
namespace {
bool requested = false, finished = false, gameplay = false, armored = false, equippedVehicle = false, missionContext = false;
bool inventoryBody = false, inventoryPaging = false;
void require(bool condition) { if (!condition) throw Exception("Isolated report fixture setup unavailable"); }
YAML::Node position(int x,int y,int z=0)
{ YAML::Node n; n.push_back(x);n.push_back(y);n.push_back(z);return n; }
YAML::Node unitNode(int id,int faction,const std::string &type,const std::string &armor,int x,int y,int direction)
{
    YAML::Node n;
    n["id"]=id;n["faction"]=faction;n["originalFaction"]=faction;
    n["status"]=0;n["position"]=position(x,y);n["direction"]=direction;
    n["genUnitType"]=type;n["genUnitArmor"]=armor;
    // Ordinary load defaults provide health/TUs/armor from unchanged rules.
    n["armor"]=YAML::Node(YAML::NodeType::Sequence);
    n["fatalWounds"]=YAML::Node(YAML::NodeType::Sequence);
    n["turnsSinceSpotted"]=0;
    return n;
}
}
void prepare(int &argc,char **argv)
{
    for(int i=1;i<argc;++i)
    {
        if(std::string(argv[i])!="--agent-report-fixture") continue;
        require(!requested && i+1<argc && (std::string(argv[i+1])=="alien-control-v1" || std::string(argv[i+1])=="gameplay-v1" || std::string(argv[i+1])=="gameplay-armored-v1" || std::string(argv[i+1])=="gameplay-vehicle-v1" || std::string(argv[i+1])=="gameplay-mission-v1" || std::string(argv[i+1])=="inventory-body-v1" || std::string(argv[i+1])=="inventory-paging-v1"));
        const char *mode=std::getenv("OPENXCOM_AGENT_CONTROL");
        require(mode && std::string(mode)=="1");
        inventoryPaging=std::string(argv[i+1])=="inventory-paging-v1";
        inventoryBody=inventoryPaging || std::string(argv[i+1])=="inventory-body-v1";
        missionContext=std::string(argv[i+1])=="gameplay-mission-v1";
        missionContext=missionContext || inventoryBody;
        equippedVehicle=missionContext || std::string(argv[i+1])=="gameplay-vehicle-v1";
        armored=equippedVehicle || std::string(argv[i+1])=="gameplay-armored-v1";
        gameplay=armored || std::string(argv[i+1])=="gameplay-v1";
        requested=true;
        for(int j=i;j+2<argc;++j)argv[j]=argv[j+2];
        argc-=2;argv[argc]=nullptr;--i;
    }
}
bool waiting() { return requested && !finished; }
bool start(Game *game)
{
    if(!requested || finished) return false;
    require(game && !game->getSavedGame() && game->getMod());
    require(!Options::autosave && !Options::debug && !Options::traceAI && Options::getActiveMods().size()==1);
    const std::string master=Options::getActiveMaster();
    require(master=="xcom1" || master=="xcom2");
    const bool underwater=master=="xcom2";
    Mod *mod=game->getMod();
    const std::string dataset=underwater?"SAND":"CULTIVAT";
    MapDataSet *terrain=mod->getMapDataSet(dataset);require(terrain);
    terrain->loadData(mod->getMCDPatch(dataset));
    // Select a stock flat floor; never edit MCD, mod rules or retail assets.
    int floor=-1;
    for(std::size_t i=0;i<terrain->getSize();++i)
    {
        MapData *m=terrain->getObject(i);
        if(m->getObjectType()==O_FLOOR && !m->isNoFloor() && !m->isGravLift() &&
            m->getTerrainLevel()==0 && m->getTUCost(MT_WALK)==4 && m->getTUCost(MT_FLY)==4)
        {floor=static_cast<int>(i);break;}
    }
    require(floor>=0);
    // gameplay-v1 includes the normal terrain-destruction fallback resources.
    MapDataSet *obstacles = nullptr; int obstacle = -1;
    const std::string obstacleSet = underwater ? "ROCKS" : "CULTIVAT";
    if (gameplay)
    {
        mod->getMapDataSet("BLANKS")->loadData(mod->getMCDPatch("BLANKS"));
        obstacles = mod->getMapDataSet(obstacleSet); require(obstacles);
        obstacles->loadData(mod->getMCDPatch(obstacleSet));
        for (std::size_t i=0;i<obstacles->getSize();++i)
        {
            MapData *m=obstacles->getObject(i);
            if (m->getObjectType()==O_OBJECT && m->getTUCost(MT_WALK)==255 && !m->isDoor() && !m->isUFODoor())
            { obstacle=static_cast<int>(i); break; }
        }
        require(obstacle>=0);
    }
    std::unique_ptr<SavedGame> save(new SavedGame());save->setDifficulty(DIFF_BEGINNER);
    Base *base=new Base(mod);save->getBases()->push_back(base);
    require(!mod->getSoldiersList().empty());
    RuleSoldier *rules=mod->getSoldier(mod->getSoldiersList().front(),true);
    // A separately named fixture uses ordinary stock armor. Original aliases
    // retain their original equipment; no base-stat override or forced outcome.
    const std::string soldierArmor=armored?(underwater?"STR_MAGNETIC_ION_ARMOR_UC":"STR_POWER_SUIT_UC"):rules->getArmor();
    // id=0 constructs a blank soldier: no random name/stat generation or seed manipulation.
    std::unique_ptr<Soldier> soldier(new Soldier(rules,mod->getArmor(soldierArmor,true),0));
    YAML::Node person;person["id"]=1;person["name"]="MCP Test";person["rank"]=0;person["gender"]=0;person["look"]=0;person["armor"]=soldierArmor;
    UnitStats stats=rules->getMinStats();require(stats.psiStrength==0 && stats.psiSkill==0);
    person["initialStats"]=stats;person["currentStats"]=stats;
    soldier->load(person,mod,save.get());base->getSoldiers()->push_back(soldier.release());
    if (missionContext)
    {
        // Ordinary skirmish ownership context, needed by normal debriefing.
        // No target, outcome, AI, RNG or soldier-stat mutation is introduced.
        std::unique_ptr<Craft> craft(new Craft(mod->getCraft(underwater?"STR_TRITON":"STR_SKYRANGER",true),base,1));
        craft->setInBattlescape(true);
        base->getSoldiers()->front()->setCraft(craft.get());
        base->getCrafts()->push_back(craft.release());
        require(save->getMonthsPassed()==-1 && !base->isInBattlescape() && base->getCrafts()->size()==1);
        require(base->getCrafts()->front()->getBase()==base && base->getCrafts()->front()->isInBattlescape() &&
            base->getSoldiers()->front()->getCraft()==base->getCrafts()->front());
    }
    const int extent=gameplay?6:4;
    YAML::Node battle;battle["width"]=extent;battle["length"]=extent;battle["height"]=1;
    battle["missionType"]="STR_UFO_CRASH_RECOVERY";battle["globalshade"]=0;battle["turn"]=1;
    battle["depth"]=underwater?1:0;battle["selectedUnit"]=1;
    if(gameplay)battle["mapdatasets"].push_back("BLANKS");
    battle["mapdatasets"].push_back(dataset);
    if(gameplay && obstacleSet!=dataset)battle["mapdatasets"].push_back(obstacleSet);
    for(int y=0;y<extent;++y)for(int x=0;x<extent;++x)
    {
        YAML::Node tile;tile["position"]=position(x,y);
        for(int p=0;p<4;++p)
        {
            const bool blocked=gameplay && x==3 && y==2 && p==O_OBJECT;
            tile["mapDataID"].push_back(p==0?floor:(blocked?obstacle:-1));
            tile["mapDataSetID"].push_back(p==0?(gameplay?1:0):(blocked?(obstacleSet==dataset?1:2):-1));
        }
        for(int p=0;p<3;++p)tile["discovered"].push_back(true);
        battle["tiles"].push_back(tile);
    }
    // Ordinary AI always visits setupPatrol even after choosing psi. Its
    // fallback requires a real node; an empty node vector is not a valid map.
    YAML::Node route;route["id"]=0;route["position"]=position(3,3);route["rank"]=0;
    route["type"]=0;route["flags"]=1;route["dummy"]=false;
    for(int i=0;i<5;++i)route["links"].push_back(-1);
    battle["nodes"].push_back(route);
    const std::string tankType=underwater?"STR_COELACANTH_GAS_CANNON":"STR_TANK_CANNON";
    const std::string alienType=gameplay?(underwater?"STR_LOBSTERMAN_SOLDIER":"STR_MUTON_SOLDIER"):(underwater?"STR_AQUATOID_COMMANDER":"STR_ETHEREAL_COMMANDER");
    Unit *tankRule=mod->getUnit(tankType,true),*alienRule=mod->getUnit(alienType,true);
    const int tankId=BattleUnit::MAX_SOLDIER_ID,alienId=tankId+1;
    YAML::Node victim=unitNode(1,FACTION_PLAYER,"",soldierArmor,2,gameplay?2:1,4);victim["morale"]=gameplay?100:0;
    battle["units"].push_back(victim);
    YAML::Node vehicle=unitNode(tankId,FACTION_PLAYER,tankType,tankRule->getArmor(),0,0,4);
    if (equippedVehicle)
    {
        // Match the stock generator's fixed hand weapon, compatible magazine
        // and turret. Serialize before ordinary load; never equip during play.
        RuleItem *gun=mod->getItem(tankType,true);
        require(gun->isFixed() && gun->getBattleType()==BT_FIREARM && !gun->getCompatibleAmmo()->empty());
        RuleItem *rounds=mod->getItem(gun->getCompatibleAmmo()->front(),true);
        require(rounds->getBattleType()==BT_AMMO && rounds->getClipSize()>0 && gun->getTurretType()>=0);
        appendVehicleArmament(battle,vehicle,gun->getType(),rounds->getType(),rounds->getClipSize(),gun->getTurretType());
    }
    battle["units"].push_back(vehicle);
    battle["units"].push_back(unitNode(alienId,FACTION_HOSTILE,alienType,alienRule->getArmor(),2,gameplay?4:2,0));
    const std::string pistol=underwater?"STR_SONIC_PISTOL":"STR_PLASMA_PISTOL";
    const std::string clip=underwater?"STR_PISTOL_POWER_CLIP":"STR_PLASMA_PISTOL_CLIP";
    YAML::Node weapon;weapon["type"]=pistol;weapon["id"]=10;weapon["owner"]=alienId;weapon["inventoryslot"]="STR_RIGHT_HAND";weapon["ammoItem"]=11;
    YAML::Node ammo;ammo["type"]=clip;ammo["id"]=11;ammo["ammoqty"]=mod->getItem(clip,true)->getClipSize();
    battle["items"].push_back(weapon);battle["items"].push_back(ammo);
    if (gameplay)
    {
        const std::string gun=underwater?"STR_GAUSS_RIFLE":"STR_RIFLE";
        const std::string rounds=underwater?"STR_GAUSS_RIFLE_CLIP":"STR_RIFLE_CLIP";
        YAML::Node loaded;loaded["type"]=gun;loaded["id"]=20;loaded["owner"]=1;
        loaded["inventoryslot"]="STR_RIGHT_HAND";loaded["ammoItem"]=21;
        YAML::Node magazine;magazine["type"]=rounds;magazine["id"]=21;
        magazine["ammoqty"]=mod->getItem(rounds,true)->getClipSize();
        YAML::Node empty;empty["type"]=gun;empty["id"]=22;empty["owner"]=1;empty["inventoryslot"]="STR_LEFT_HAND";
        battle["items"].push_back(loaded);battle["items"].push_back(magazine);battle["items"].push_back(empty);
    }
    if (inventoryBody)
    {
        const InventoryStockTypes types=inventoryStockTypes(underwater);
        RuleItem *rifle=mod->getItem(types.rifle,true);
        RuleItem *rifleClip=mod->getItem(types.rifleClip,true);
        RuleItem *pistolClip=mod->getItem(types.pistolClip,true);
        RuleItem *grenade=mod->getItem(types.grenade,true);
        require(rifleClip->getBattleType()==BT_AMMO && pistolClip->getBattleType()==BT_AMMO &&
            rifleClip->getClipSize()>0 && pistolClip->getClipSize()>0 && grenade->getBattleType()==BT_GRENADE);
        for (RuleItem *small : {rifleClip,pistolClip,grenade})
            require(small->getInventoryWidth()==1 && small->getInventoryHeight()==1);
        const std::vector<std::string> &compatible=*rifle->getCompatibleAmmo();
        require(std::find(compatible.begin(),compatible.end(),types.rifleClip)!=compatible.end() &&
            std::find(compatible.begin(),compatible.end(),types.pistolClip)==compatible.end());
        if (inventoryPaging)
        {
            RuleItem *gun=mod->getItem(types.pagingGun,true), *ammo=mod->getItem(types.pagingAmmo,true);
            require(!gun->isFixed() && gun->getBattleType()==BT_FIREARM &&
                gun->getInventoryWidth()==2 && gun->getInventoryHeight()==3 &&
                ammo->getBattleType()==BT_AMMO && ammo->getClipSize()>=10);
            const std::vector<std::string> &gunAmmo=*gun->getCompatibleAmmo();
            require(std::find(gunAmmo.begin(),gunAmmo.end(),types.pagingAmmo)!=gunAmmo.end());
        }
        appendInventoryStock(battle,underwater,inventoryPaging,rifleClip->getClipSize(),pistolClip->getClipSize());
    }
    std::unique_ptr<SavedBattleGame> tactical(new SavedBattleGame());
    tactical->load(battle,mod,save.get());tactical->loadMapResources(mod);
    require(tactical->getUnits()->size()==3 && tactical->getNodes()->size()==1);
    if (equippedVehicle)
    {
        BattleUnit *vehicleUnit=(*tactical->getUnits())[1];
        BattleItem *gun=vehicleUnit->getItem("STR_RIGHT_HAND");
        require(gun && gun->getRules()==mod->getItem(tankType,true) && gun->getAmmoItem());
        require(gun->getAmmoItem()->getAmmoQuantity()==gun->getAmmoItem()->getRules()->getClipSize() &&
            vehicleUnit->getTurretType()==gun->getRules()->getTurretType());
    }
    BattleUnit *target=(*tactical->getUnits())[0],*alien=(*tactical->getUnits())[2];
    require(tactical->getTurn()==1 && tactical->getSide()==FACTION_PLAYER && target->getStatus()==STATUS_STANDING && target->getMorale()==(gameplay?100:0));
    // Setup assertions only, completed before the controller accepts a command.
    // No replay, mutation or hidden diagnostics are available to its consumer.
    tactical->getTileEngine()->calculateFOV(alien);
    require(std::find(alien->getVisibleUnits()->begin(),alien->getVisibleUnits()->end(),target)!=alien->getVisibleUnits()->end());
    require(alien->getMainHandWeapon(false) && alien->getMainHandWeapon(false)->getAmmoItem());
    if (!gameplay)
    {
    BattleItem *psi=alien->getSpecialWeapon(BT_PSIAMP);require(psi);
    require(target->getBaseStats()->psiStrength==0 && target->getBaseStats()->psiSkill==0 &&
        target->getArmor()->getSize()==1 && (*tactical->getUnits())[1]->getArmor()->getSize()==2);
    require(psi->getRules()->getFlatRate() && psi->getRules()->getTUUse()==25);
    const int distance=tactical->getTileEngine()->distance(target->getPosition(),alien->getPosition());
    const int psiStrength=alien->getBaseStats()->psiStrength*alien->getBaseStats()->psiSkill/50;
    require(distance==1 && psiStrength-distance>30 &&
        psiStrength-distance+55>alien->getMainHandWeapon(false)->getAmmoItem()->getRules()->getPower());
    Pathfinding *path=tactical->getPathfinding();
    const std::vector<int> reachable=path->findReachable(alien,alien->getTimeUnits());
    for(int index:reachable)
    {
        Position destination; tactical->getTileCoords(index,&destination.x,&destination.y,&destination.z);
        path->calculate(alien,destination);
        require(destination==alien->getPosition() || (path->getStartDirection()!=-1 && path->getTotalTUCost()<=24));
        path->abortPath();
    }
    require(alien->getTimeUnits()>49);
    }
    else
    {
        require(alien->getBaseStats()->psiSkill==0);
        tactical->getTileEngine()->calculateFOV(target);
        require(std::find(target->getVisibleUnits()->begin(),target->getVisibleUnits()->end(),alien)!=target->getVisibleUnits()->end());
    }
    save->setBattleGame(tactical.release());game->setSavedGame(save.release());
    // Standard loaded-battle UI construction, never a queued attack or report.
    game->setState(new GeoscapeState);
    Options::baseXResolution=Options::baseXBattlescape;Options::baseYResolution=Options::baseYBattlescape;
    game->getScreen()->resetDisplay(false);
    BattlescapeState *state=new BattlescapeState;game->pushState(state);
    game->getSavedGame()->getSavedBattle()->setBattleState(state);
    finished=true;
    return true;
}
} }
