// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentProjectileWitness.h"
#include <new>
#include <limits>

namespace OpenXcom { namespace Observer {
namespace {
bool inside(int x, int y, const AgentProjectileRect &r)
{
    // int64 avoids overflow even for an untrusted malformed coverage fixture.
    return x >= r.x && y >= r.y && std::int64_t(x) < std::int64_t(r.x) + r.width &&
        std::int64_t(y) < std::int64_t(r.y) + r.height;
}
bool validCover(const AgentProjectileRect &r)
{
    return r.width > 0 && r.height > 0;
}
}

bool AgentProjectileWitness::begin(int width, int height)
{
    _valid = false; _sealed = false; _width = _height = 0; _pixels.clear();
    if (width <= 0 || height <= 0 || width > MaxWidth || height > MaxHeight) return false;
    try { _pixels.assign(static_cast<std::size_t>(width) * height, Pixel()); }
    catch (const std::bad_alloc &) { return false; }
    _width = width; _height = height; _valid = true;
    return true;
}

void AgentProjectileWitness::wrote(int x, int y, std::uint8_t before,
    std::uint8_t after, bool projectileBody)
{
    if (!_valid) return;
    if (_sealed || x < 0 || y < 0 || x >= _width || y >= _height)
    { invalidate(); return; }
    Pixel &pixel = _pixels[static_cast<std::size_t>(y) * _width + x];
    if (!projectileBody) { pixel.candidate = false; return; }
    if (pixel.candidate && before != pixel.after) { invalidate(); return; }
    // Map's final zero/color-key pixels do not contribute to its destination.
    if (!after) { pixel.candidate = false; return; }
    // A transparent predecessor has no proven opaque Map background color.
    if (!pixel.candidate && !before) { invalidate(); return; }
    if (!pixel.candidate) pixel.before = before;
    pixel.after = after;
    pixel.candidate = true;
}

void AgentProjectileWitness::invalidate() { _valid = false; _sealed = false; }
void AgentProjectileWitness::seal(bool healthy)
{
    if (!healthy) { invalidate(); return; }
    if (_valid) _sealed = true;
}

AgentProjectileFact AgentProjectileWitness::evaluate(bool admitted,
    const AgentProjectileRect &viewport, const std::vector<AgentProjectileRect> &covers,
    const Palette &palette, const FinalPixelProof &proof,
    const std::function<bool(int, int)> &pixelAdmitted) const
{
    if (!admitted || !_valid || !_sealed || !proof || viewport.x < 0 || viewport.y < 0 ||
        viewport.width <= 0 || viewport.height <= 0 || viewport.width > _width || viewport.height > _height ||
        viewport.x > _width - viewport.width || viewport.y > _height - viewport.height || covers.size() > MaxOccluders)
        return AgentProjectileFact::Unknown;
    for (const AgentProjectileRect &cover : covers)
        if (!validCover(cover)) return AgentProjectileFact::Unknown;
    const auto covered = [&](int x, int y) -> bool
    {
        for (const AgentProjectileRect &cover : covers)
            if (inside(x, y, cover)) return true;
        return false;
    };
    // A fully covered viewport cannot publicly establish this renderer mode or
    // episode absence. This bounded geometry check reads no image pixels.
    bool mapExposed = false;
    for (int y = viewport.y; y < viewport.y + viewport.height && !mapExposed; ++y)
        for (int x = viewport.x; x < viewport.x + viewport.width; ++x)
            if (!covered(x, y)) { mapExposed = true; break; }
    if (!mapExposed) return AgentProjectileFact::Unknown;
    bool found = false, unproven = false;
    for (int y = viewport.y; y < viewport.y + viewport.height; ++y)
        for (int x = viewport.x; x < viewport.x + viewport.width; ++x)
        {
            const Pixel &pixel = _pixels[static_cast<std::size_t>(y) * _width + x];
            if (!pixel.candidate) continue;
            const std::uint32_t before = palette[pixel.before], after = palette[pixel.after];
            if (before > 0xffffff || after > 0xffffff) return AgentProjectileFact::Unknown;
            if (before == after) continue;
            if (covered(x, y)) continue;
            if (pixelAdmitted && !pixelAdmitted(x, y)) { unproven = true; continue; }
            if (!proof(x, y, after)) return AgentProjectileFact::Unknown;
            found = true;
        }
    return found ? AgentProjectileFact::Presented : (unproven ? AgentProjectileFact::Unknown : AgentProjectileFact::KnownAbsent);
}
bool AgentProjectileRaster::begin(AgentProjectileWitness &witness, void *bytes, int width, int height, int pitch)
{
    _witness = &witness; _body = false; _begin = 0; _span = 0;
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(bytes);
    if (!bytes || width <= 0 || height <= 0 || width > AgentProjectileWitness::MaxWidth ||
        height > AgentProjectileWitness::MaxHeight || pitch < width || pitch > 4096 ||
        begin > std::numeric_limits<std::uintptr_t>::max() - std::size_t(pitch) * height ||
        !witness.begin(width, height)) { witness.invalidate(); return false; }
    _begin = begin; _span = std::size_t(pitch) * height;
    _width = width; _height = height; _pitch = pitch; return true;
}
void AgentProjectileRaster::wrote(const void *address, std::uint8_t before, std::uint8_t after)
{
    if (!_witness) return;
    const std::uintptr_t p = reinterpret_cast<std::uintptr_t>(address);
    if (!_begin || p < _begin || p - _begin >= _span) { unknownWrite(); return; }
    const std::size_t offset = p - _begin;
    const int x = offset % _pitch, y = offset / _pitch;
    if (x >= _width || y >= _height) { unknownWrite(); return; }
    _witness->wrote(x, y, before, after, _body);
}
} }
