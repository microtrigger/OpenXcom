// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentProjectileWitness.h"
#include <SDL.h>
namespace OpenXcom { namespace Observer {
// Readback proof only for known writes; never a candidate/image classifier.
class AgentProjectileOutput
{
    const SDL_Surface *_logical = nullptr, *_final = nullptr;
    int _scaleX = 0, _scaleY = 0, _left = 0, _top = 0;
    static bool layout(const SDL_Surface *surface);
    static bool rgb(const SDL_Surface *surface, int x, int y, std::uint32_t &out);
public:
    AgentProjectileWitness::Palette palette;
    bool capture(bool successfulFlip, bool filtered, const SDL_Surface *logical,
        const SDL_Surface *final, const SDL_Surface *map, int top, int bottom, int left, int right);
    bool matches(int logicalX, int logicalY, std::uint32_t expected) const;
};
} }
