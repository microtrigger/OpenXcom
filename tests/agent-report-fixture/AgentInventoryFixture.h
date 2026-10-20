// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <yaml-cpp/yaml.h>
#include <stdexcept>
#include <string>
namespace OpenXcom { namespace AgentReportFixture {
// Setup-only stock identities. Never included by a controller or UI provider.
struct InventoryStockTypes
{
    const char *rifle;
    const char *rifleClip;
    const char *pistolClip;
    const char *grenade;
    const char *pagingGun;
    const char *pagingAmmo;
};
inline InventoryStockTypes inventoryStockTypes(bool underwater)
{
    const InventoryStockTypes ufo = {"STR_RIFLE", "STR_RIFLE_CLIP", "STR_PISTOL_CLIP",
        "STR_GRENADE", "STR_AUTO_CANNON", "STR_AC_AP_AMMO"};
    const InventoryStockTypes tftd = {"STR_GAUSS_RIFLE", "STR_GAUSS_RIFLE_CLIP", "STR_GAUSS_PISTOL_CLIP",
        "STR_MAGNA_BLAST_GRENADE", "STR_HYDRO_JET_CANNON", "STR_HJC_AP_AMMO"};
    return underwater ? tftd : ufo;
}
inline YAML::Node inventoryGroundItem(const char *type, int id)
{
    YAML::Node item;
    item["type"]=type; item["id"]=id; item["inventoryslot"]="STR_GROUND";
    // The unchanged gameplay-mission-v1 soldier begins on this tile.
    item["position"].push_back(2); item["position"].push_back(2); item["position"].push_back(0);
    item["XCOMProperty"]=true;
    return item;
}
// Pure serialization before ordinary SavedBattleGame::load. No runtime writes,
// forced arrangement coordinates, controller fields, rule edits or RNG access.
inline void appendInventoryStock(YAML::Node &battle, bool underwater, bool paging,
    int rifleClipQuantity, int pistolClipQuantity)
{
    if (rifleClipQuantity <= 0 || pistolClipQuantity <= 0)
        throw std::invalid_argument("Invalid isolated inventory ammunition");
    // Reserve private setup IDs without replacing any existing item.
    for (const auto &item : battle["items"])
    {
        const int id=item["id"].as<int>();
        if ((id>=40 && id<=42) || (paging && id>=50 && id<=69))
            throw std::invalid_argument("Isolated inventory item ID collision");
    }
    const InventoryStockTypes types=inventoryStockTypes(underwater);
    YAML::Node rifleClip=inventoryGroundItem(types.rifleClip,40);
    rifleClip["ammoqty"]=rifleClipQuantity;
    YAML::Node pistolClip=inventoryGroundItem(types.pistolClip,41);
    pistolClip["ammoqty"]=pistolClipQuantity;
    battle["items"].push_back(rifleClip); battle["items"].push_back(pistolClip);
    // Omitted fuseTimer retains the normal BattleItem constructor's -1.
    battle["items"].push_back(inventoryGroundItem(types.grenade,42));
    if (!paging) return;
    for (int quantity=1; quantity<=10; ++quantity)
    {
        const int gunId=50+2*(quantity-1), ammoId=gunId+1;
        YAML::Node gun=inventoryGroundItem(types.pagingGun,gunId);
        gun["ammoItem"]=ammoId;
        YAML::Node ammo; ammo["type"]=types.pagingAmmo; ammo["id"]=ammoId;
        ammo["ammoqty"]=quantity; ammo["XCOMProperty"]=true;
        // Match BattlescapeGenerator::loadWeapons: a linked magazine has the
        // right-hand slot but no owner and no separate ground-tile position.
        ammo["inventoryslot"]="STR_RIGHT_HAND";
        battle["items"].push_back(gun); battle["items"].push_back(ammo);
    }
}
} }
