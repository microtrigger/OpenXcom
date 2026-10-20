// SPDX-License-Identifier: GPL-3.0-or-later
// Raster/provenance regression against real SDL_gfx on 8-bit off-screen surfaces.
// No Game, world model, video subsystem, window, or SDL event queue is used.
#include "../../src/Observer/AgentGlobeProjection.h"
#include <SDL.h>
#include <SDL_gfxPrimitives.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
using OpenXcom::Observer::AgentGlobeProjection;
namespace {
unsigned checks = 0, cases = 0, certainCovered = 0, edgeExcluded = 0, shadowRejected = 0;
const int Width = 96, Height = 80;
void check(bool value, const char *message)
{
    ++checks;
    if (!value) { std::cerr << "FAIL " << message << " (case " << cases << ")\n"; std::exit(1); }
}
SDL_Surface *surface(int width, int height)
{
    SDL_Surface *result = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 8, 0, 0, 0, 0);
    check(result != nullptr, "create paletted software surface");
    SDL_Color colors[256];
    for (int i = 0; i < 256; ++i) colors[i] = {Uint8(i), Uint8(i), Uint8(i), 0};
    check(SDL_SetColors(result, colors, 0, 256) == 1, "set deterministic grayscale palette");
    check(SDL_FillRect(result, nullptr, 0) == 0, "clear software surface");
    return result;
}
Uint8 &pixel(SDL_Surface *surface, int x, int y)
{
    return static_cast<Uint8 *>(surface->pixels)[y * surface->pitch + x];
}
struct Polygon { std::vector<Sint16> x, y; };
void rasterCase(SDL_Surface *texture, const Polygon &original, int offset, bool reverse)
{
    ++cases;
    Polygon polygon = original;
    for (auto &x : polygon.x) x += offset;
    for (auto &y : polygon.y) y -= offset;
    if (reverse) { std::reverse(polygon.x.begin(), polygon.x.end()); std::reverse(polygon.y.begin(), polygon.y.end()); }
    SDL_Surface *scene = surface(Width, Height), *survival = surface(Width, Height);
    // Match the ordinary ocean primitive before land, with a deliberately
    // overlarge radius; the later survival mask is independent of land fill.
    check(filledCircleColor(scene, 49, 40, 55, 0x292929ff) == 0, "real filled ocean circle");
    const int texturedResult = texturedPolygon(scene, polygon.x.data(), polygon.y.data(), polygon.x.size(), texture, 0, 0);
    if (texturedResult < 0) std::cerr << "texturedPolygon result=" << texturedResult << " error=" << SDL_GetError() << '\n';
    check(texturedResult >= 0, "real textured triangle/quad raster");
    AgentGlobeProjection projection;
    projection.begin(Width, Height);
    projection.polygon(polygon.x.data(), polygon.y.data(), polygon.x.size(), true);
    check(SDL_LockSurface(scene) == 0, "lock raster evidence");
    unsigned positive = 0;
    for (const auto &sample : projection.samples)
    {
        check(!projection.visible(sample), "polygon coverage alone lacks final shadow proof");
        if (sample.textured && !sample.uncertain)
        {
            check(pixel(scene, sample.x, sample.y) == 113, "every certain textured sample has actual SDL_gfx coverage");
            ++positive; ++certainCovered;
        }
        for (unsigned i = 0, j = polygon.x.size() - 1; i < polygon.x.size(); j = i++)
        {
            if (AgentGlobeProjection::distance(sample.x, sample.y, polygon.x[j], polygon.y[j], polygon.x[i], polygon.y[i]) <= 4.)
            {
                check(sample.uncertain, "two-pixel boundary is excluded regardless of raster fill");
                ++edgeExcluded;
            }
        }
    }
    SDL_UnlockSurface(scene);
    check(positive > 0, "each translated/reversed fixture exercises certain textured interior");

    check(filledCircleColor(survival, 48, 40, 31, 0xffffffff) == 0, "independent final sphere survival circle");
    check(filledCircleColor(survival, 40, 40, 9, 0x000000ff) == 0, "independent transparent survival hole");
    check(SDL_LockSurface(scene) == 0 && SDL_LockSurface(survival) == 0, "lock final masks");
    for (int y = 0; y < Height; ++y)
        for (int x = 0; x < Width; ++x)
        {
            if (!pixel(survival, x, y)) pixel(scene, x, y) = 0;
            projection.shadow(x, y, pixel(scene, x, y) != 0);
        }
    for (const auto &sample : projection.samples)
    {
        check(sample.sphere == (pixel(scene, sample.x, sample.y) != 0), "shadow hook tracks independently masked final pixel survival");
        if (projection.visible(sample))
        {
            check(pixel(scene, sample.x, sample.y) != 0, "every visible sample survives final raster mask");
            if (sample.textured)
                check(pixel(scene, sample.x, sample.y) == 113, "surviving textured sample retains texture provenance");
        }
        if (sample.textured && !sample.uncertain && !sample.sphere)
        {
            check(!projection.visible(sample), "shadow-cleared textured interior is excluded");
            ++shadowRejected;
        }
    }
    SDL_UnlockSurface(survival); SDL_UnlockSurface(scene);
    SDL_FreeSurface(survival); SDL_FreeSurface(scene);
}
void transparentCase(SDL_Surface *opaque)
{
    ++cases;
    SDL_Surface *scene = surface(Width, Height), *transparent = surface(7, 5);
    check(SDL_SetColorKey(transparent, SDL_SRCCOLORKEY, 0) == 0, "transparent texture has a real zero color key");
    const Sint16 fullX[] = {-4, 100, 100, -4}, fullY[] = {-4, -4, 84, 84};
    const Sint16 overX[] = {0, 64, 64, 0}, overY[] = {0, 0, 64, 64};
    check(texturedPolygon(scene, fullX, fullY, 4, opaque, 0, 0) >= 0, "opaque baseline quad");
    check(texturedPolygon(scene, overX, overY, 4, transparent, 0, 0) >= 0, "real transparent textured overlay");
    AgentGlobeProjection projection; projection.begin(Width, Height);
    projection.polygon(fullX, fullY, 4, true);
    projection.polygon(overX, overY, 4, false);
    check(SDL_LockSurface(scene) == 0, "lock transparent raster evidence");
    unsigned excluded = 0, retained = 0;
    for (const auto &sample : projection.samples) projection.shadow(sample.x, sample.y, pixel(scene, sample.x, sample.y) != 0);
    for (const auto &sample : projection.samples)
    {
        check(pixel(scene, sample.x, sample.y) == 113, "transparent keyed overlay leaves baseline texture unchanged");
        if (sample.x < 64 && sample.y < 64)
        {
            check(sample.uncertain && !projection.visible(sample), "unknown-opacity primitive excludes its whole interior");
            ++excluded;
        }
        else if (!sample.uncertain)
        {
            check(projection.visible(sample) && sample.textured, "uncovered certain opaque baseline remains available");
            ++retained;
        }
    }
    check(excluded > 0 && retained > 0, "transparency fixture checks excluded and retained samples");
    SDL_UnlockSurface(scene);
    SDL_FreeSurface(transparent); SDL_FreeSurface(scene);
}
}
int main()
{
    check(SDL_Init(0) == 0, "initialize SDL without video/audio");
    check(SDL_GetVideoSurface() == nullptr && !(SDL_WasInit(SDL_INIT_VIDEO)), "no GUI before raster checks");
    SDL_Surface *opaque = surface(7, 5);
    check(SDL_FillRect(opaque, nullptr, 113) == 0, "opaque nonzero texture");
    const Polygon polygons[] = {
        {{4, 88, 40}, {4, 12, 76}},           // Triangle
        {{-28, 108, 40}, {8, 12, 106}},       // Triangle crosses viewport edges
        {{0, 80, 8}, {0, 72, 76}},           // Shallow/steep diagonal edges
        {{4, 84, 92, 12}, {12, 4, 68, 76}},  // Convex skew quad
        {{-32, 112, 104, -8}, {-20, 8, 92, 72}}, // Quad spans the viewport
        {{4, 92, 36, 4}, {4, 4, 36, 76}},    // Concave quad
        {{-12, 100, 40, 4}, {8, -16, 32, 92}}, // Concave/crossing viewport
        {{8, 88, 88, 8}, {8, 8, 72, 72}},    // Exact sample-aligned edges
    };
    for (const auto &polygon : polygons)
        for (int offset : {-3, -1, 0, 1, 3})
            for (bool reverse : {false, true}) rasterCase(opaque, polygon, offset, reverse);
    transparentCase(opaque);
    check(certainCovered > 0 && edgeExcluded > 0 && shadowRejected > 0, "all provenance rejection categories exercised");
    SDL_FreeSurface(opaque);
    check(SDL_GetVideoSurface() == nullptr && !(SDL_WasInit(SDL_INIT_VIDEO)), "no GUI after raster checks");
    std::cout << "AgentGlobeRasterTest PASS " << checks << " checks; " << cases << " cases; "
              << certainCovered << " certain covered samples; " << edgeExcluded << " edge exclusions; "
              << shadowRejected << " shadow-rejected textured samples\n";
    SDL_Quit();
}
