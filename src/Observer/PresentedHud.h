// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include <SDL.h>
#include <array>
#include <string>
#include <vector>
namespace OpenXcom { namespace Observer {
struct HudDigits
{
    int x = 0, y = 0, width = 0, height = 0;
    unsigned char foreground = 0;
};
typedef std::array<HudDigits, 4> HudFrame;
bool decodeHudDigits(const std::vector<unsigned char> &mask, int width, unsigned int &value);
// Validate complete widget masks against the final displayed RGB pixels.
std::string provePresentedHud(SDL_Surface *screen, int baseWidth, int baseHeight,
                             const HudFrame &frame, bool allowed);
} }
