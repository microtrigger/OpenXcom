// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
// Digit patterns adapted from src/Interface/NumberText.cpp;
// Copyright 2010-2016 OpenXcom Developers; GPL-3.0-or-later.
#include "PresentedHud.h"
#include "PresentedCursorSurface.h"
#include <locale>
#include <sstream>
namespace OpenXcom { namespace Observer {
bool decodeHudDigits(const std::vector<unsigned char> &mask, int width, unsigned int &value)
{
    // NumberText's fixed unbordered font, row-major, including all holes.
    static const char *glyphs[10] = {
        "111101101101111", "010110010010111", "111001111100111", "111001111001111",
        "101101111001001", "111100111001111", "111100111101111", "111001001001001",
        "111101111101111", "111101111001111"
    };
    if (width < 7 || width > 64 || mask.size() != static_cast<std::size_t>(width * 5)) return false;
    value = 0;
    int digits = 0;
    for (int left = 0; left + 3 <= width; left += 4)
    {
        bool blank = true;
        for (int y = 0; y < 5; ++y)
            for (int x = left; x < width; ++x) if (mask[y * width + x]) blank = false;
        if (blank) return digits > 0;
        if (digits == 3) return false;
        int match = -1;
        for (int n = 0; n < 10; ++n)
        {
            bool equal = true;
            for (int y = 0; y < 5; ++y)
                for (int x = 0; x < 3; ++x)
                    if (mask[y * width + left + x] != static_cast<unsigned char>(glyphs[n][y * 3 + x] == '1')) equal = false;
            if (equal) { if (match >= 0) return false; match = n; }
        }
        if (match < 0 || (digits > 0 && value == 0)) return false;
        if (left + 3 < width)
            for (int y = 0; y < 5; ++y) if (mask[y * width + left + 3]) return false;
        value = value * 10 + match; ++digits;
    }
    // A blank complete glyph slot is required: clipped longer numerals and
    // edge-touching numerals must be indistinguishable and both unsupported.
    return false;
}
std::string provePresentedHud(SDL_Surface *screen, int bw, int bh, const HudFrame &frame, bool allowed)
{
    if (!supportedPresentedSurface(screen, bw, bh, allowed)) return "null";
    const int scale = screen->w / bw;
    for (const HudDigits &d : frame)
    {
        if (d.width < 7 || d.width > 64 || d.height != 5 ||
            d.x < 0 || d.y < 0 || d.x > bw - d.width || d.y > bh - d.height || !d.foreground)
            return "null";
    }
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
            if (frame[i].x < frame[j].x + frame[j].width && frame[j].x < frame[i].x + frame[i].width &&
                frame[i].y < frame[j].y + frame[j].height && frame[j].y < frame[i].y + frame[i].height)
                return "null";
    std::array<std::vector<unsigned char>, 4> masks;
    for (int i = 0; i < 4; ++i) masks[i].resize(frame[i].width * 5);
    if (SDL_LockSurface(screen) != 0) return "null";
    bool valid = true;
    for (int i = 0; i < 4; ++i)
    {
        const HudDigits &d = frame[i];
        const SDL_Color &fg = screen->format->palette->colors[d.foreground];
        for (int y = 0; y < d.height * scale && valid; ++y)
        {
            const unsigned char *row = static_cast<const unsigned char *>(screen->pixels) + (d.y * scale + y) * screen->pitch;
            for (int x = 0; x < d.width * scale; ++x)
            {
                const SDL_Color &actual = screen->format->palette->colors[row[d.x * scale + x]];
                const unsigned char ink = actual.r == fg.r && actual.g == fg.g && actual.b == fg.b;
                const int at = (y / scale) * d.width + x / scale;
                if (x % scale == 0 && y % scale == 0) masks[i][at] = ink;
                else if (masks[i][at] != ink) { valid = false; break; }
            }
        }
    }
    SDL_UnlockSurface(screen);
    std::array<unsigned int, 4> values;
    for (int i = 0; i < 4; ++i)
        if (!valid || !decodeHudDigits(masks[i], frame[i].width, values[i])) return "null";
    std::ostringstream out; out.imbue(std::locale::classic());
    out << "{\"time_units\":" << values[0] << ",\"energy\":" << values[1]
        << ",\"health\":" << values[2] << ",\"morale\":" << values[3] << "}";
    return out.str();
}
} }
