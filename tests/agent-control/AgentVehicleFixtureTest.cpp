// SPDX-License-Identifier: GPL-3.0-or-later
// Pure YAML setup regression using repository stock rules. No Game or assets.
#include "../agent-report-fixture/AgentVehicleFixture.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::AgentReportFixture;
static unsigned checks=0;
static void check(bool value) { ++checks; if(!value) std::exit(1); }
static YAML::Node item(const YAML::Node &items,const std::string &type)
{
    for(const auto &node:items) if(node["type"].as<std::string>()==type) return node;
    std::exit(2);
}
int main(int argc,char **argv)
{
    check(argc==2);
    for(int game=1;game<=2;++game)
    {
        const std::string type=game==1?"STR_TANK_CANNON":"STR_COELACANTH_GAS_CANNON";
        YAML::Node rules=YAML::LoadFile(std::string(argv[1])+"/bin/standard/xcom"+std::to_string(game)+"/items.rul")["items"];
        YAML::Node gun=item(rules,type),ammo=item(rules,gun["compatibleAmmo"][0].as<std::string>());
        check(gun["fixedWeapon"].as<bool>()); check(gun["battleType"].as<int>()==1);
        check(ammo["battleType"].as<int>()==2); check(ammo["clipSize"].as<int>()>0);
        check(gun["accuracySnap"].as<int>()>0); check(gun["accuracyAimed"].as<int>()>0);
        YAML::Node battle,vehicle,existing;existing["id"]=10;existing["type"]="existing";
        battle["items"].push_back(existing);battle["selectedUnit"]=1;battle["turn"]=1;
        vehicle["id"]=1000000;vehicle["genUnitType"]=type;vehicle["direction"]=4;
        appendVehicleArmament(battle,vehicle,type,ammo["type"].as<std::string>(),ammo["clipSize"].as<int>(),gun["turretType"].as<int>());
        check(battle["items"].size()==3);check(battle["items"][0]["id"].as<int>()==10);
        check(battle["selectedUnit"].as<int>()==1);check(battle["turn"].as<int>()==1);
        check(vehicle["genUnitType"].as<std::string>()==type);check(vehicle["direction"].as<int>()==4);
        check(vehicle["turretType"].as<int>()==gun["turretType"].as<int>());
        YAML::Node w=battle["items"][1],a=battle["items"][2];
        check(w["type"].as<std::string>()==type);check(w["owner"].as<int>()==vehicle["id"].as<int>());
        check(w["inventoryslot"].as<std::string>()=="STR_RIGHT_HAND");check(w["ammoItem"].as<int>()==a["id"].as<int>());
        check(w["id"].as<int>()==30 && a["id"].as<int>()==31);
        check(a["type"].as<std::string>()==ammo["type"].as<std::string>());
        check(a["ammoqty"].as<int>()==ammo["clipSize"].as<int>());
        check(w["XCOMProperty"].as<bool>() && a["XCOMProperty"].as<bool>());
        check(!a["owner"]); // Ammo is linked inside weapon, not separately owned in the unit inventory.
        check(a["inventoryslot"].as<std::string>()=="STR_RIGHT_HAND"); // Ordinary generator metadata.
        YAML::Node roundtrip=YAML::Load(YAML::Dump(battle));
        check(roundtrip["items"][1]["ammoItem"].as<int>()==roundtrip["items"][2]["id"].as<int>());
        check(!vehicle["health"] && !vehicle["tu"] && !vehicle["faction"]);
    }
    for(int invalid=0;invalid<4;++invalid)
    {
        YAML::Node battle,vehicle;vehicle["id"]=1;bool refused=false;
        try { appendVehicleArmament(battle,vehicle,invalid==0?"":"gun",invalid==1?"":"ammo",invalid==2?0:30,invalid==3?-1:0); }
        catch(const std::invalid_argument &) { refused=true; }
        check(refused);check(!battle["items"] && !vehicle["turretType"]);
    }
    std::cout<<"AgentVehicleFixtureTest PASS "<<checks<<" checks\n";
}
