// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
namespace OpenXcom { namespace Observer {
// Compiled stock UI geometry only. No item, actor, cost, fit or page queries.
inline unsigned agentInventoryBodyCount(const std::string &id)
{
    if (id == "STR_BACK_PACK") return 9;
    if (id == "STR_BELT") return 6;
    if (id == "STR_RIGHT_LEG" || id == "STR_LEFT_LEG" ||
        id == "STR_RIGHT_SHOULDER" || id == "STR_LEFT_SHOULDER") return 2;
    return 0;
}
inline bool agentInventoryBodyCell(const std::string &id, int x, int y, int sx, int sy)
{
    if (id == "STR_BACK_PACK") return x == 192 && y == 37 && sx >= 0 && sx < 3 && sy >= 0 && sy < 3;
    if (id == "STR_BELT") return x == 192 && y == 104 && sx >= 0 && sx < 4 &&
        (sy == 0 || (sy == 1 && (sx == 0 || sx == 3)));
    if (sx < 0 || sx > 1 || sy != 0) return false;
    return (id == "STR_RIGHT_LEG" && x == 0 && y == 120) ||
        (id == "STR_LEFT_LEG" && x == 128 && y == 120) ||
        (id == "STR_RIGHT_SHOULDER" && x == 16 && y == 37) ||
        (id == "STR_LEFT_SHOULDER" && x == 112 && y == 37);
}
template<typename Slots> bool agentInventoryBodyLayout(const std::string &id, int x, int y, const Slots &slots)
{
    if (!agentInventoryBodyCount(id) || slots.size() != agentInventoryBodyCount(id)) return false;
    for (unsigned i = 0; i < slots.size(); ++i)
    {
        if (!agentInventoryBodyCell(id, x, y, slots[i].x, slots[i].y)) return false;
        for (unsigned j = 0; j < i; ++j)
            if (slots[j].x == slots[i].x && slots[j].y == slots[i].y) return false;
    }
    return true;
}
inline bool agentInventoryBodyRect(int x, int y, int w, int h)
{
    if (w != 16 || h != 16) return false;
    if (x >= 192 && x < 240 && (x - 192) % 16 == 0 && y >= 37 && y < 85 && (y - 37) % 16 == 0) return true;
    if (x >= 192 && x < 256 && (x - 192) % 16 == 0 && (y == 104 || (y == 120 && (x == 192 || x == 240)))) return true;
    if (y == 120 && (x == 0 || x == 16 || x == 128 || x == 144)) return true;
    return y == 37 && (x == 16 || x == 32 || x == 112 || x == 128);
}
inline bool agentInventoryBodyIntersects(int x, int y, int w, int h)
{
    // Interior overlap, not the shared single-pixel grid border. Bounded 23 cells.
    const int cells[][2] = {{192,37},{208,37},{224,37},{192,53},{208,53},{224,53},
        {192,69},{208,69},{224,69},{192,104},{208,104},{224,104},{240,104},{192,120},{240,120},
        {0,120},{16,120},{128,120},{144,120},{16,37},{32,37},{112,37},{128,37}};
    for (const auto &cell : cells)
        if (x < cell[0]+16 && x+w > cell[0] && y < cell[1]+16 && y+h > cell[1]) return true;
    return false;
}
} }
