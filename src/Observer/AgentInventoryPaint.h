// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentUi.h"
#include "../Engine/Surface.h"
#include <algorithm>
namespace OpenXcom { namespace Observer {
// Narrow inventory-composite proof. Reads only already painted widget layers;
// no item, rule, slot, quantity or game object is accessible here.
inline bool agentInventoryLayer(const Surface *s)
{
    if (!s || !s->agentContextVisible() || s->getWidth() < 1 || s->getWidth() > 640 ||
        s->getHeight() < 1 || s->getHeight() > 400) return false;
    const SDL_Surface *p = s->getSurface();
    return p && p->format && p->format->BytesPerPixel == 1 && p->format->palette && p->format->palette->ncolors == 256 &&
        p->pixels && !p->locked && !SDL_MUSTLOCK(p) && p->pitch >= p->w &&
        p->clip_rect.x == 0 && p->clip_rect.y == 0 && p->clip_rect.w == p->w && p->clip_rect.h == p->h &&
        (p->flags & SDL_SRCCOLORKEY) && p->format->colorkey == 0 && !(p->flags & SDL_SRCALPHA);
}
inline unsigned agentInventoryPixel(const Surface *s, int x, int y)
{
    const SDL_Surface *p = s->getSurface();
    return static_cast<const Uint8 *>(p->pixels)[y * p->pitch + x];
}
inline bool agentInventoryInk(const Surface *s, AgentUiRect &out)
{
    if (!agentInventoryLayer(s)) return false;
    int left = s->getWidth(), top = s->getHeight(), right = -1, bottom = -1;
    for (int y = 0; y < s->getHeight(); ++y) for (int x = 0; x < s->getWidth(); ++x)
        if (agentInventoryPixel(s, x, y))
        { left = std::min(left, x); top = std::min(top, y); right = std::max(right, x); bottom = std::max(bottom, y); }
    if (right < left) return false;
    out = {s->getX() + left, s->getY() + top, right - left + 1, bottom - top + 1};
    return true;
}
inline bool agentInventoryClear(const Surface *s, const AgentUiRect &r, int offsetX = 0, int offsetY = 0)
{
    if (!s || !s->getVisible()) return true;
    if (!agentInventoryLayer(s)) return false;
    const int sx = offsetX + s->getX(), sy = offsetY + s->getY();
    for (int y = std::max(r.y, sy); y < std::min(r.y + r.height, sy + s->getHeight()); ++y)
        for (int x = std::max(r.x, sx); x < std::min(r.x + r.width, sx + s->getWidth()); ++x)
            if (agentInventoryPixel(s, x - sx, y - sy)) return false;
    return true;
}
inline bool agentInventoryInside(const AgentUiRect &r)
{
    return r.x >= 0 && r.y >= 0 && r.width > 0 && r.height > 0 &&
        r.x <= 640 - r.width && r.y <= 400 - r.height && agentRegionVisible(r);
}
inline std::string agentInventoryRectJson(const AgentUiRect &r)
{
    return "\"x\":" + std::to_string(r.x) + ",\"y\":" + std::to_string(r.y) +
        ",\"width\":" + std::to_string(r.width) + ",\"height\":" + std::to_string(r.height);
}
} }
