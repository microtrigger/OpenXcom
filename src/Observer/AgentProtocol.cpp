// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentProtocol.h"
#include <sstream>
#include <vector>

namespace OpenXcom { namespace Observer {
const char *agentProtocolSchema() { return "openxcom-agent-control/47"; }
std::string agentEnvelope()
{
    return "{\"schema\":\"openxcom-agent-control/47\",\"mode\":\"agent-control\"";
}
std::string agentJsonString(const std::string &value)
{
    const char *hex = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char c : value)
    {
        if (c == '\"' || c == '\\') { out += '\\'; out += c; }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += c;
    }
    return out + "\"";
}
bool agentToken(const std::string &value)
{
    return value.size() == 32 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}
bool agentActionId(const std::string &value)
{
    return value == "acknowledge_cannot_reequip" || value == "open_abort_mission" || value == "confirm_abort_mission" || value == "cancel_abort_mission" || value == "move_cursor" || value == "choose_dogfight_mode" || value == "open_interception" || value == "open_contact_interception" || value == "cancel_interception" ||
        value == "open_intercept_base" || value == "confirm_interception" || value == "cancel_interception_confirmation" ||
        value == "open_transfer" || value == "cancel_transfer_destination" ||
        value == "change_transfer_quantity" || value == "review_transfer" ||
        value == "cancel_transfer_items" || value == "confirm_transfer" || value == "cancel_transfer_confirmation" ||
        value == "next_soldier" || value == "previous_soldier" || value == "kneel" ||
        value == "stand" || value == "reserve_none" || value == "reserve_snap" || value == "reserve_aimed" || value == "reserve_auto" || value == "toggle_kneel_reserve" || value == "acknowledge_crash_recovery" || value == "continue_next_turn" || value == "new_battle" ||
        value == "start_battle" || value == "continue_briefing" || value == "close_inventory" ||
        value == "open_new_game" || value == "open_options" || value == "open_load_game" ||
        value == "open_pause_menu" || value == "open_save_game" || value == "resume_game" ||
        value == "cancel_menu" || value == "dismiss_error" || value == "select_difficulty" ||
        value == "set_ironman" || value == "start_new_game" || value == "place_base_site" || value == "open_new_base" || value == "cancel_new_base" || value == "confirm_new_base" || value == "cancel_new_base_confirmation" || value == "place_access_lift_cell" ||
        value == "replace_base_name" || value == "confirm_base_name" ||
        value == "load_saved_game" || value == "select_new_save_slot" || value == "replace_new_save_name" || value == "save_new_game" ||
        value == "open_new_battle_choice" || value == "choose_visible_row" || value == "open_psi_training_base" || value == "choose_visible_craft_destination" || value == "scroll_visible_list" ||
        value == "close_dropdown" || value == "open_funding" || value == "close_funding" || value == "end_turn" || value == "acknowledge_message" || value == "map_primary" || value == "map_secondary" ||
        value == "center_enemy" || value == "center_selected_unit" || value == "open_hand_action_menu" ||
        value == "choose_weapon_action" || value == "choose_grenade_timer" || value == "cancel_grenade_timer" || value == "cancel_weapon_action" || value == "close_debriefing" || value == "toggle_debriefing_stats" || value == "open_statistics" ||
        value == "open_inventory" || value == "inventory_hover" || value == "inventory_activate" ||
        value == "cancel_inventory_hold" || value == "unload_inventory" || value == "next_inventory_ground_page" ||
        value == "view_level_up" || value == "view_level_down" || value == "toggle_layers" ||
        value == "open_minimap" || value == "close_minimap" || value == "unit_up" || value == "unit_down" ||
        value == "camera_pan" || value == "minimap_center" ||
        value == "choose_geoscape_speed" || value == "select_base" || value == "open_bases" ||
        value == "return_geoscape" || value == "open_base_info" || value == "close_base_info" ||
        value == "open_base_stores" || value == "close_base_stores" ||
        value == "open_base_transfers" || value == "close_base_transfers" ||
        value == "right_click_base_facility_cell" || value == "increase_containment_removal" || value == "decrease_containment_removal" || value == "remove_selected_aliens" || value == "cancel_containment" ||
        value == "open_diary_combat" || value == "open_diary_awards" || value == "select_diary_mission" || value == "previous_diary_mission" || value == "next_diary_mission" || value == "close_diary_mission" ||
        value == "open_memorial" || value == "select_memorial_soldier" || value == "close_memorial" ||
        value == "open_soldier_diary" || value == "open_diary_performance" || value == "close_soldier_diary" || value == "close_diary_performance" ||
        value == "open_monthly_costs" || value == "close_monthly_costs" ||
        value == "open_research" || value == "open_new_research" || value == "select_research_project" ||
        value == "adjust_scientists" || value == "accept_research" || value == "cancel_research_project" ||
        value == "close_research_list" || value == "open_manufacture" || value == "open_new_production" ||
        value == "select_production_project" || value == "start_production" || value == "cancel_production_start" ||
        value == "adjust_engineers" || value == "accept_production" || value == "stop_production" ||
        value == "close_production_list" || value == "change_production_quantity" ||
        value == "set_production_sell" || value == "open_purchase" || value == "change_purchase_quantity" ||
        value == "accept_purchase" || value == "cancel_purchase" || value == "open_purchase_category" ||
        value == "open_build_facilities" || value == "select_build_facility" || value == "close_build_facilities" || value == "cancel_place_facility" || value == "place_facility_cell" || value == "click_base_facility_cell" || value == "confirm_dismantle_facility" || value == "cancel_dismantle_facility" ||
        value == "open_sell" || value == "change_sale_quantity" || value == "accept_sell" || value == "cancel_sell" || value == "open_sell_category" || value == "scroll_sell" ||
        value == "scroll_purchase" || value == "position_purchase_list" ||
        value == "dismiss_items_arriving" || value == "open_arrival_base" ||
        value == "dismiss_production_complete" || value == "open_production_destination" ||
        value == "accept_landing_begin_mission" || value == "decline_landing_return_to_base" || value == "toggle_psi_training" || value == "close_psi_training_allocation" || value == "close_psi_training" || value == "close_commendations" || value == "acknowledge_lost_in_service" || value == "close_promotions" || value == "acknowledge_alien_base_discovery" || value == "acknowledge_ufo_lost" || value == "acknowledge_monthly_report" || value == "acknowledge_monthly_failure" || value == "advance_slideshow" || value == "close_statistics" || value == "acknowledge_craft_error" || value == "acknowledge_craft_error_5_seconds" ||
        value == "open_craft_destination" || value == "cancel_craft_destination" ||
        value == "redirect_patrolling_craft" || value == "cancel_craft_navigation" || value == "return_craft_to_base" ||
        value == "patrol_craft_current_position" || value == "go_to_craft_last_known_position" ||
        value == "acknowledge_craft_patrol" || value == "acknowledge_low_fuel" || value == "acknowledge_low_fuel_5_seconds" ||
        value == "dismiss_ufo_contact" || value == "continue_pursuit" || value == "return_interceptor_to_base" ||
        value == "open_soldiers" ||
        value == "select_soldier" ||
        value == "close_soldiers" ||
        value == "previous_personnel" ||
        value == "next_personnel" ||
        value == "open_soldier_armor" ||
        value == "close_soldier_info" ||
        value == "open_crafts" ||
        value == "select_craft" ||
        value == "close_crafts" ||
        value == "open_craft_crew" ||
        value == "open_craft_equipment" ||
        value == "open_ufopaedia" ||
        value == "select_ufopaedia_category" ||
        value == "close_ufopaedia" ||
        value == "select_ufopaedia_article" ||
        value == "close_ufopaedia_select" ||
        value == "open_craft_armor" ||
        value == "open_craft_weapon_1" ||
        value == "open_craft_weapon_2" ||
        value == "select_craft_weapon" ||
        value == "cancel_craft_weapons" ||
        value == "close_craft_info" ||
        value == "set_craft_soldier" ||
        value == "open_craft_soldier_info" ||
        value == "close_craft_crew" ||
        value == "change_craft_equipment" ||
        value == "close_craft_equipment" ||
        value == "select_craft_armor_soldier" ||
        value == "close_craft_armor" ||
        value == "select_soldier_armor" ||
        value == "cancel_soldier_armor" ||
        value == "dismiss_research_complete" ||
        value == "view_research_reports" ||
        value == "dismiss_new_research" ||
        value == "open_research_allocation" ||
        value == "dismiss_new_manufacture" ||
        value == "open_manufacture_allocation" ||
        value == "dismiss_research_required" ||
        value == "intercept_mission_alert" ||
        value == "center_mission_alert" ||
        value == "dismiss_mission_alert" ||
        value == "close_ufopaedia_article" ||
        value == "previous_ufopaedia_article" ||
        value == "next_ufopaedia_article" ||
        value == "scroll_article_text";
}
const char *agentArgumentKind(const std::string &action)
{
    if (action == "move_cursor") return "cursor_position";
    if (action == "choose_dogfight_mode") return "dogfight_mode";
    if (action == "set_craft_soldier") return "crew_assignment";
    if (action == "change_craft_equipment") return "equipment_quantity";
    if (action == "select_ufopaedia_category" || action == "select_ufopaedia_article" || action == "select_craft_weapon" || action == "toggle_psi_training" || action == "select_soldier" || action == "select_craft" || action == "open_craft_soldier_info" || action == "select_craft_armor_soldier" || action == "select_soldier_armor") return "outfitting_row";
    if (action == "change_production_quantity") return "production_quantity";
    if (action == "change_transfer_quantity") return "transfer_quantity";
    if (action == "select_build_facility") return "facility_row";
    if (action == "right_click_base_facility_cell" || action == "place_facility_cell" || action == "click_base_facility_cell" || action == "place_access_lift_cell") return "facility_cell";
    if (action == "change_sale_quantity") return "sale_quantity";
    if (action == "scroll_sell") return "sale_scroll";
    if (action == "change_purchase_quantity") return "purchase_quantity";
    if (action == "set_production_sell") return "boolean_value";
    if (action == "scroll_purchase") return "purchase_scroll";
    if (action == "position_purchase_list") return "purchase_position";
    if (action == "select_memorial_soldier" || action == "increase_containment_removal" || action == "decrease_containment_removal" || action == "select_diary_mission" || action == "select_research_project" || action == "select_production_project") return "project_row";
    if (action == "adjust_scientists" || action == "adjust_engineers") return "staff_direction";
    if (action == "choose_geoscape_speed") return "geoscape_speed";
    if (action == "select_base") return "base_target";
    if (action == "map_primary" || action == "map_secondary") return "grid_cell";
    if (action == "center_enemy") return "enemy_button";
    if (action == "open_hand_action_menu") return "hand";
    if (action == "choose_weapon_action") return "weapon_action";
    if (action == "choose_grenade_timer") return "grenade_timer";
    if (action == "inventory_hover" || action == "inventory_activate") return "inventory_target";
    if (action == "camera_pan") return "camera_direction";
    if (action == "minimap_center") return "minimap_target";
    if (action == "open_new_battle_choice") return "choice_control";
    if (action == "choose_visible_row" || action == "open_psi_training_base" || action == "choose_visible_craft_destination" || action == "load_saved_game") return "visible_row";
    if (action == "scroll_visible_list" || action == "scroll_article_text") return "scroll_direction";
    if (action == "select_difficulty") return "difficulty";
    if (action == "set_ironman") return "boolean";
    if (action == "replace_base_name" || action == "replace_new_save_name") return "text";
    if (action == "place_base_site") return "point";
    return agentActionId(action) ? "none" : "unsupported";
}
bool agentInputText(const std::string &value)
{
    if (value.size() > 128) return false;
    for (std::size_t i = 0; i < value.size();)
    {
        const unsigned char first = static_cast<unsigned char>(value[i++]);
        unsigned code = 0, remaining = 0, minimum = 0;
        if (first < 0x80) code = first;
        else if (first >= 0xc2 && first <= 0xdf) { code = first & 31; remaining = 1; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { code = first & 15; remaining = 2; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; remaining = 3; minimum = 0x10000; }
        else return false;
        if (i + remaining > value.size()) return false;
        for (unsigned j = 0; j < remaining; ++j)
        {
            const unsigned char next = static_cast<unsigned char>(value[i++]);
            if ((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 63);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
            code < 32 || (code >= 127 && code <= 159)) return false;
    }
    return true;
}
namespace {
bool decimal(const std::string &text, std::uint64_t maximum, std::uint64_t &out)
{
    if (text.empty() || text.size() > 16 || (text.size() > 1 && text[0] == '0') ||
        text.find_first_not_of("0123456789") != std::string::npos) return false;
    out = 0;
    for (char c : text) out = out * 10 + (c - '0');
    return out <= maximum;
}
}
bool parseAgentRequest(const std::string &packet, AgentRequest &request)
{
    request = AgentRequest();
    if (packet.empty() || packet.size() > 512 || packet.find_first_of("\r\n\t") != std::string::npos ||
        packet.find('\0') != std::string::npos) return false;
    if (packet == std::string("H ") + agentProtocolSchema()) { request.kind = 'H'; return true; }
    if (packet.size() == 34 && packet[1] == ' ' && packet[0] == 'R')
    {
        const std::string value = packet.substr(2);
        if (!agentToken(value)) return false;
        request.kind = packet[0];
        if (request.kind == 'R') request.challenge = value; else request.id = value;
        return true;
    }
    std::istringstream input(packet);
    std::vector<std::string> parts; std::string part, canonical;
    while (input >> part)
    {
        if (!canonical.empty()) canonical += " ";
        canonical += part; parts.push_back(part);
    }
    if (canonical == packet && parts.size() == 3 && parts[0] == "S")
    {
        if (!decimal(parts[1], 9007199254740991ULL, request.commandGeneration) || !request.commandGeneration || !agentToken(parts[2])) return false;
        request.kind='S'; request.id=parts[2]; return true;
    }
    if (canonical == packet && (parts[0] == "L" || parts[0] == "K"))
    {
        const bool retire=parts[0]=="L";
        if (parts.size() != (retire ? 5u : 4u) || !agentToken(parts[1]) || !agentToken(parts[2]) ||
            !decimal(parts[3],9007199254740991ULL,request.commandGeneration) || !request.commandGeneration) return false;
        if (retire && (!decimal(parts[4],256,request.acknowledgedCount) || !request.acknowledgedCount)) return false;
        request.kind=retire?'L':'K'; request.challenge=parts[1]; request.epoch=parts[2]; return true;
    }
    if (canonical == packet && parts.size() >= 7 && parts[0] == "E")
    {
        if (!decimal(parts[2],9007199254740991ULL,request.commandGeneration) || !request.commandGeneration) return false;
        parts.erase(parts.begin()+2); // Existing action-specific argument grammar stays unchanged.
    }
    if (canonical == packet && parts.size() == 5 && parts[0] == "B")
    {
        std::uint64_t limit = 0;
        if (!agentToken(parts[1]) || (parts[2]!="-" && !agentToken(parts[2])) ||
            !decimal(parts[3], 100000, request.after) || !decimal(parts[4], 16, limit) || !limit ||
            (parts[2]=="-" && request.after)) return false;
        request.kind='B'; request.challenge=parts[1]; request.snapshot=parts[2];
        request.limit=static_cast<unsigned>(limit); return true;
    }
    if (canonical == packet && parts.size() == 5 && parts[0] == "V")
    {
        std::uint64_t limit = 0;
        if (!agentToken(parts[1]) || !agentToken(parts[2]) ||
            !decimal(parts[3], 9007199254740991ULL, request.after) ||
            !decimal(parts[4], 16, limit) || !limit) return false;
        request.kind = 'V'; request.challenge = parts[1]; request.epoch = parts[2];
        request.limit = static_cast<unsigned>(limit); return true;
    }
    if (!request.commandGeneration || canonical != packet || parts.size() < 6 || parts.size() > 9 || parts[0] != "E") return false;
    request.id = parts[1]; request.epoch = parts[2]; request.capability = parts[3]; request.action = parts[5];
    if (!agentToken(request.id) || !agentToken(request.epoch) || !agentToken(request.capability) ||
        !agentActionId(request.action) || !decimal(parts[4], 9007199254740991ULL, request.revision)) return false;
    const std::string kind = agentArgumentKind(request.action);
    if (kind == "none") { if (parts.size() != 6) return false; }
    else if (kind == "dogfight_mode")
    {
        if (parts.size() != 7 || (parts[6] != "standoff" && parts[6] != "cautious" &&
            parts[6] != "standard" && parts[6] != "aggressive" && parts[6] != "disengage")) return false;
        request.choice = parts[6];
    }
    else if (kind == "sale_quantity")
    {
        std::uint64_t row;
        if (parts.size()!=9 || parts[6].size()<2 || parts[6][0]!='r' || !decimal(parts[6].substr(1),31,row) ||
            parts[7]!="adjust" || (parts[8]!="1" && parts[8]!="-1")) return false;
        request.choice=parts[6]; request.operation=parts[7]; request.value=parts[8]=="1"?1:-1;
    }
    else if (kind == "sale_scroll")
    {
        if (parts.size()!=8 || (parts[6]!="up" && parts[6]!="down") || parts[7]!="step") return false;
        request.direction=parts[6]; request.amount=parts[7];
    }
    else if (kind == "production_quantity" || kind == "purchase_quantity" || kind == "equipment_quantity" || kind == "transfer_quantity")
    {
        if (parts.size()!=8 && parts.size()!=9) return false;
        if (kind=="production_quantity") { if (parts[6]!="engineers" && parts[6]!="units") return false; request.control=parts[6]; }
        else { std::uint64_t row; if (parts[6].size()<2 || parts[6][0]!='r' || !decimal(parts[6].substr(1),31,row)) return false; request.choice=parts[6]; }
        request.operation=parts[7];
        if (request.operation=="max" || request.operation=="clear") { if(parts.size()!=8) return false; }
        else if (request.operation=="adjust" || request.operation=="set")
        {
            if (parts.size()!=9) return false;
            const bool negative=!parts[8].empty() && parts[8][0]=='-';
            std::uint64_t value;
            if (!decimal(negative?parts[8].substr(1):parts[8],request.operation=="adjust"?1000:100000,value) ||
                (negative && (!value || request.operation=="set")) || (!value && request.operation=="adjust")) return false;
            request.value=negative?-static_cast<int>(value):static_cast<int>(value);
        }
        else return false;
    }
    else if (kind == "crew_assignment")
    {
        std::uint64_t row;
        if (parts.size()!=8 || parts[6].size()<2 || parts[6][0]!='r' || !decimal(parts[6].substr(1),31,row) ||
            (parts[7]!="true" && parts[7]!="false")) return false;
        request.choice=parts[6];request.desired=parts[7]=="true";
    }
    else if (kind == "boolean_value")
    {
        if (parts.size()!=7 || (parts[6]!="true" && parts[6]!="false")) return false;
        request.desired=parts[6]=="true";
    }
    else if (kind == "purchase_scroll")
    {
        if (parts.size()!=8 || (parts[6]!="up" && parts[6]!="down") ||
            (parts[7]!="step" && parts[7]!="page" && parts[7]!="end")) return false;
        request.direction=parts[6]; request.amount=parts[7];
    }
    else if (kind == "purchase_position")
    {
        std::uint64_t target;
        if (parts.size()!=7 || parts[6].size()<2 || parts[6][0]!='t' || !decimal(parts[6].substr(1),2,target)) return false;
        request.choice=parts[6];
    }
    else if (kind == "difficulty")
    {
        if (parts.size() != 7) return false;
        request.choice = parts[6];
        if (request.choice != "beginner" && request.choice != "experienced" && request.choice != "veteran" &&
            request.choice != "genius" && request.choice != "superhuman") return false;
    }
    else if (kind == "choice_control")
    {
        if (parts.size() != 7) return false;
        request.choice = parts[6];
        if (request.choice != "mission" && request.choice != "craft" && request.choice != "terrain" &&
            request.choice != "difficulty" && request.choice != "alien_race") return false;
    }
    else if (kind == "grid_cell" || kind == "enemy_button" || kind == "weapon_action" || kind == "grenade_timer")
    {
        const char prefix = kind == "grid_cell" ? 'g' : (kind == "enemy_button" ? 'e' : (kind == "grenade_timer" ? 't' : 'a'));
        const unsigned maximum = kind == "grid_cell" ? 127 : (kind == "enemy_button" ? 9 : (kind == "grenade_timer" ? 23 : 5));
        std::uint64_t index;
        if (parts.size() != 7 || parts[6].empty() || parts[6][0] != prefix ||
            !decimal(parts[6].substr(1), maximum, index)) return false;
        request.choice = parts[6];
    }
    else if (kind == "geoscape_speed" || kind == "base_target")
    {
        const char prefix = kind == "geoscape_speed" ? 's' : 'b';
        const unsigned maximum = kind == "geoscape_speed" ? 5 : 7;
        std::uint64_t index;
        if (parts.size() != 7 || parts[6].empty() || parts[6][0] != prefix ||
            !decimal(parts[6].substr(1), maximum, index)) return false;
        request.choice = parts[6];
    }
    else if (kind == "hand")
    {
        if (parts.size() != 7 || (parts[6] != "left" && parts[6] != "right")) return false;
        request.choice = parts[6];
    }
    else if (kind == "inventory_target")
    {
        std::uint64_t target;
        if (parts.size() != 7 || parts[6].empty() || parts[6][0] != 'v' ||
            !decimal(parts[6].substr(1), 95, target)) return false;
        request.choice = parts[6];
    }
    else if (kind == "minimap_target")
    {
        std::uint64_t target;
        if (parts.size() != 7 || parts[6].empty() || parts[6][0] != 'm' ||
            !decimal(parts[6].substr(1), 159, target)) return false;
        request.choice = parts[6];
    }
    else if (kind == "camera_direction")
    {
        if (parts.size() != 7 || (parts[6] != "left" && parts[6] != "right" &&
            parts[6] != "up" && parts[6] != "down")) return false;
        request.choice = parts[6];
    }
    else if (kind == "facility_row" || kind == "visible_row" || kind == "project_row" || kind == "outfitting_row")
    {
        std::uint64_t row;
        if (parts.size() != 7 || parts[6].empty() || parts[6][0] != 'r' ||
            !decimal(parts[6].substr(1), 31, row)) return false;
        request.choice = parts[6];
    }
    else if (kind == "staff_direction")
    {
        if (parts.size() != 7 || (parts[6] != "increase" && parts[6] != "decrease")) return false;
        request.choice = parts[6];
    }
    else if (kind == "scroll_direction")
    {
        if (parts.size() != 7 || (parts[6] != "up" && parts[6] != "down")) return false;
        request.choice = parts[6];
    }
    else if (kind == "boolean")
    {
        if (parts.size() != 7 || (parts[6] != "0" && parts[6] != "1")) return false;
        request.desired = parts[6] == "1";
    }
    else if (kind == "text")
    {
        if (parts.size() != 7) return false;
        const std::string &hex = parts[6];
        if (hex != "-")
        {
            if (hex.empty() || hex.size() > 256 || hex.size() % 2 ||
                hex.find_first_not_of("0123456789abcdef") != std::string::npos) return false;
            const std::string digits = "0123456789abcdef";
            for (std::size_t i = 0; i < hex.size(); i += 2)
                request.text += static_cast<char>(digits.find(hex[i]) * 16 + digits.find(hex[i+1]));
        }
        if (!agentInputText(request.text)) return false;
    }
    else if(kind=="facility_cell")
    {
        std::uint64_t x,y;
        if(parts.size()!=8 || !decimal(parts[6],5,x) || !decimal(parts[7],5,y))return false;
        request.x=static_cast<int>(x);request.y=static_cast<int>(y);
    }
    else if (kind == "point" || kind == "cursor_position")
    {
        std::uint64_t x, y;
        if (parts.size() != 8 || !decimal(parts[6], 4095, x) || !decimal(parts[7], 4095, y)) return false;
        request.x = static_cast<int>(x); request.y = static_cast<int>(y);
    }
    else return false;
    request.kind = 'E';
    return true;
}
AgentLedger::Admission AgentLedger::begin(const std::string &id, const std::string &request)
{
    auto found = _entries.find(id);
    if (found != _entries.end()) return found->second.request == request ? Duplicate : Conflict;
    if (_entries.size() >= 256) return Full;
    _entries[id].request = request;
    return Fresh;
}
bool AgentLedger::retireAcknowledged(std::size_t count)
{
    if (!count || count != _entries.size()) return false;
    for (const auto &entry : _entries) if (!entry.second.terminalDispatched) return false;
    _entries.clear(); return true;
}
AgentLedger::Entry *AgentLedger::find(const std::string &id)
{
    auto found = _entries.find(id);
    return found == _entries.end() ? 0 : &found->second;
}
} }
