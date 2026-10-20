// SPDX-License-Identifier: GPL-3.0-or-later
// Pure stock-rule/YAML serialization tests. No Game, arrangement, UI or assets.
#include "../agent-report-fixture/AgentInventoryFixture.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>
using namespace OpenXcom::AgentReportFixture;
static unsigned checks=0;
static void check(bool value)
{
    ++checks;
    if (!value) { std::cerr<<"Check failed: "<<checks<<'\n'; std::exit(1); }
}
static YAML::Node rule(const YAML::Node &items,const std::string &type)
{
    for (const auto &item:items) if (item["type"].as<std::string>()==type) return item;
    std::cerr<<"Missing stock rule: "<<type<<'\n'; std::exit(2);
}
static void onlyKeys(const YAML::Node &node,const std::set<std::string> &keys)
{
    check(node.IsMap()); check(node.size()==keys.size());
    for (const auto &entry:node) check(keys.count(entry.first.as<std::string>())==1);
}
static void ground(const YAML::Node &item)
{
    check(!item["owner"] && !item["previousOwner"] && !item["unit"]);
    check(item["inventoryslot"].as<std::string>()=="STR_GROUND");
    check(item["position"].as<std::vector<int> >()==std::vector<int>({2,2,0}));
    check(!item["inventoryX"] && !item["inventoryY"]); // Normal arrangement chooses these.
    check(item["XCOMProperty"].as<bool>());
}
int main(int argc,char **argv)
{
    check(argc==2);
    for (int game=1;game<=2;++game)
    {
        const bool underwater=game==2;
        const InventoryStockTypes types=inventoryStockTypes(underwater);
        const YAML::Node rules=YAML::LoadFile(std::string(argv[1])+"/bin/standard/xcom"+std::to_string(game)+"/items.rul")["items"];
        const YAML::Node rifle=rule(rules,types.rifle), rifleClip=rule(rules,types.rifleClip),
            pistolClip=rule(rules,types.pistolClip), grenade=rule(rules,types.grenade),
            gun=rule(rules,types.pagingGun), ammo=rule(rules,types.pagingAmmo);
        const std::vector<std::string> rifleAmmo=rifle["compatibleAmmo"].as<std::vector<std::string> >();
        check(std::find(rifleAmmo.begin(),rifleAmmo.end(),types.rifleClip)!=rifleAmmo.end());
        check(std::find(rifleAmmo.begin(),rifleAmmo.end(),types.pistolClip)==rifleAmmo.end());
        check(rifleClip["battleType"].as<int>()==2 && pistolClip["battleType"].as<int>()==2);
        check(rifleClip["clipSize"].as<int>()>0 && pistolClip["clipSize"].as<int>()>0);
        check(grenade["battleType"].as<int>()==4);
        for (const YAML::Node &small : {rifleClip,pistolClip,grenade})
        {
            check(small["invWidth"].as<int>(1)==1); check(small["invHeight"].as<int>(1)==1);
        }
        check(!gun["fixedWeapon"].as<bool>(false)); check(gun["battleType"].as<int>()==1);
        check(gun["invWidth"].as<int>()==2 && gun["invHeight"].as<int>()==3);
        const std::vector<std::string> gunAmmo=gun["compatibleAmmo"].as<std::vector<std::string> >();
        check(std::find(gunAmmo.begin(),gunAmmo.end(),types.pagingAmmo)!=gunAmmo.end());
        check(ammo["battleType"].as<int>()==2 && ammo["clipSize"].as<int>()==14);
        for (bool paging : {false,true})
        {
            YAML::Node battle;
            battle["selectedUnit"]=1; battle["turn"]=1; battle["depth"]=underwater?1:0;
            battle["units"][0]["id"]=1; battle["units"][0]["morale"]=100;
            const std::vector<int> priorIds={10,11,20,21,22,30,31};
            for (int id:priorIds) { YAML::Node item; item["id"]=id; item["type"]="prior"; battle["items"].push_back(item); }
            const YAML::Node before=YAML::Clone(battle);
            appendInventoryStock(battle,underwater,paging,rifleClip["clipSize"].as<int>(),pistolClip["clipSize"].as<int>());
            check(battle.size()==before.size());
            for (const auto &entry:before)
                if (entry.first.as<std::string>()!="items")
                    check(YAML::Dump(battle[entry.first.as<std::string>()])==YAML::Dump(entry.second));
            const std::size_t start=priorIds.size();
            check(battle["items"].size()==start+(paging?23:3));
            for (std::size_t i=0;i<start;++i) check(YAML::Dump(battle["items"][i])==YAML::Dump(before["items"][i]));
            std::set<int> ids,quantities;
            for (const auto &item:battle["items"]) check(ids.insert(item["id"].as<int>()).second);
            const YAML::Node compatible=battle["items"][start], incompatible=battle["items"][start+1], ordinary=battle["items"][start+2];
            check(compatible["type"].as<std::string>()==types.rifleClip && compatible["id"].as<int>()==40);
            check(compatible["ammoqty"].as<int>()==rifleClip["clipSize"].as<int>());
            check(incompatible["type"].as<std::string>()==types.pistolClip && incompatible["id"].as<int>()==41);
            check(incompatible["ammoqty"].as<int>()==pistolClip["clipSize"].as<int>());
            check(ordinary["type"].as<std::string>()==types.grenade && ordinary["id"].as<int>()==42);
            check(!ordinary["fuseTimer"] && !ordinary["ammoItem"] && !ordinary["ammoqty"]);
            for (const YAML::Node &item : {compatible,incompatible})
                onlyKeys(item,{"type","id","inventoryslot","position","XCOMProperty","ammoqty"});
            onlyKeys(ordinary,{"type","id","inventoryslot","position","XCOMProperty"});
            for (const YAML::Node &item : {compatible,incompatible,ordinary}) ground(item);
            for (int quantity=1;paging && quantity<=10;++quantity)
            {
                const std::size_t index=start+3+2*(quantity-1);
                const YAML::Node w=battle["items"][index],a=battle["items"][index+1];
                check(w["type"].as<std::string>()==types.pagingGun); ground(w);
                onlyKeys(w,{"type","id","inventoryslot","position","XCOMProperty","ammoItem"});
                check(w["id"].as<int>()==50+2*(quantity-1));
                check(w["ammoItem"].as<int>()==a["id"].as<int>() && a["id"].as<int>()==w["id"].as<int>()+1);
                check(a["type"].as<std::string>()==types.pagingAmmo);
                check(a["ammoqty"].as<int>()==quantity && quantity<=ammo["clipSize"].as<int>());
                check(quantities.insert(a["ammoqty"].as<int>()).second);
                check(a["inventoryslot"].as<std::string>()=="STR_RIGHT_HAND");
                check(!a["owner"] && !a["position"] && !a["inventoryX"] && !a["inventoryY"]);
                check(a["XCOMProperty"].as<bool>());
                onlyKeys(a,{"type","id","inventoryslot","XCOMProperty","ammoqty"});
            }
            check(quantities.size()==(paging?10u:0u));
            const YAML::Node roundtrip=YAML::Load(YAML::Dump(battle));
            check(YAML::Dump(roundtrip)==YAML::Dump(battle));
            for (int invalid : {0,-1})
            {
                for (bool rifleInvalid : {false,true})
                {
                    YAML::Node empty; const std::string beforeInvalid=YAML::Dump(empty); bool refused=false;
                    try { appendInventoryStock(empty,underwater,paging,rifleInvalid?invalid:1,rifleInvalid?1:invalid); }
                    catch (const std::invalid_argument &) { refused=true; }
                    check(refused); check(YAML::Dump(empty)==beforeInvalid);
                }
            }
            for (int collision : {40,41,42,50,69})
            {
                if (!paging && collision>=50) continue;
                YAML::Node colliding; colliding["items"][0]["id"]=collision;
                const std::string beforeCollision=YAML::Dump(colliding); bool refused=false;
                try { appendInventoryStock(colliding,underwater,paging,1,1); }
                catch (const std::invalid_argument &) { refused=true; }
                check(refused); check(YAML::Dump(colliding)==beforeCollision);
            }
        }
    }
    std::cout<<"AgentInventoryFixtureTest PASS "<<checks<<" checks (serialization only)\n";
}
