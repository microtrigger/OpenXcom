// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include <stdexcept>
namespace OpenXcom { namespace AgentReportFixture {
// Isolated setup serialization only. No Game, controller, RNG or live mutation.
inline void appendVehicleArmament(YAML::Node &battle, YAML::Node &vehicle,
    const std::string &weaponType, const std::string &ammoType, int quantity, int turret)
{
    if (weaponType.empty() || ammoType.empty() || quantity <= 0 || turret < 0)
        throw std::invalid_argument("Invalid isolated vehicle equipment");
    YAML::Node gun; gun["type"]=weaponType; gun["id"]=30;
    gun["owner"]=vehicle["id"].as<int>(); gun["inventoryslot"]="STR_RIGHT_HAND";
    gun["ammoItem"]=31; gun["XCOMProperty"]=true;
    YAML::Node ammo; ammo["type"]=ammoType; ammo["id"]=31;
    ammo["ammoqty"]=quantity; ammo["XCOMProperty"]=true;
    ammo["inventoryslot"]="STR_RIGHT_HAND"; // Generator's loaded magazine slot, without a separate owner.
    battle["items"].push_back(gun); battle["items"].push_back(ammo);
    vehicle["turretType"]=turret;
}
} }
