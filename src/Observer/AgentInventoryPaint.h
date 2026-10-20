// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentUi.h"
#include "../Engine/Surface.h"
#include "../Engine/Unicode.h"
#include "../Interface/Text.h"
#include <algorithm>
namespace OpenXcom { namespace Observer {
// Narrow inventory-composite proof. Reads only already painted widget layers;
// no item, rule, slot, quantity or game object is accessible here.
inline bool agentInventoryReadable(const Surface *s)
{
    if (!s || !s->agentContextVisible() || s->getWidth() < 1 || s->getWidth() > 640 ||
        s->getHeight() < 1 || s->getHeight() > 400) return false;
    const SDL_Surface *p = s->getSurface();
    return p && p->format && p->format->BytesPerPixel == 1 && p->format->palette && p->format->palette->ncolors == 256 &&
        p->pixels && !p->locked && !SDL_MUSTLOCK(p) && p->pitch >= p->w &&
        p->clip_rect.x == 0 && p->clip_rect.y == 0 && p->clip_rect.w == p->w && p->clip_rect.h == p->h &&
        !(p->flags & SDL_SRCALPHA);
}
// Source layers use key0 transparency; Screen's logical destination explicitly
// disables color key. Its opaque zero index is still a readable background.
inline bool agentInventoryLayer(const Surface *s)
{
    return agentInventoryReadable(s) && (s->getSurface()->flags & SDL_SRCCOLORKEY) &&
        s->getSurface()->format->colorkey == 0;
}
inline bool agentInventoryDestination(const Surface *s)
{
    return agentInventoryReadable(s) && !(s->getSurface()->flags & SDL_SRCCOLORKEY) &&
        s->getX() == 0 && s->getY() == 0;
}
// Called only after the ordinary widget's complete rendered-text proof.
// These two tokens are explicitly consumed by Text::draw, never glyphs.
inline bool agentInventoryRenderedText(const std::string &raw, std::string &shown)
{
    UString out;
    for (UCode c : Unicode::convUtf8ToUtf32(raw))
    {
        if (c == Unicode::TOK_COLOR_FLIP) continue;
        if (c == Unicode::TOK_NL_SMALL) c = '\n';
        if ((c < 32 && c != '\n') || (c >= 127 && c <= 159)) return false;
        out.push_back(c);
    }
    shown = Unicode::convUtf32ToUtf8(out);
    return shown.size() <= 1024;
}
inline std::string agentInventoryTextJson(const Text *text)
{
    if (agentTextJson(text) == "null") return "null";
    std::string shown;
    return agentInventoryRenderedText(text->getText(), shown) ? agentJsonString(shown) : "null";
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
