// SPDX-License-Identifier: GPL-3.0-or-later
// Pure bounded write witness. Renderer integration records actual Map writes;
// final-output admission remains separate from this identity-free core.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace OpenXcom { namespace Observer {

enum class AgentProjectileFact { Unknown, KnownAbsent, Presented };

struct AgentProjectileRect
{
    int x, y, width, height;
};

class AgentProjectileWitness
{
public:
    enum { MaxWidth = 640, MaxHeight = 400, MaxOccluders = 128 };
    typedef std::array<std::uint32_t, 256> Palette;
    // Called only for tracked, surviving contributions. The integration must
    // verify this RGB at the corresponding successfully presented final pixels.
    // False is Unknown, never evidence that an ordinary event did not happen.
    typedef std::function<bool(int, int, std::uint32_t)> FinalPixelProof;

    bool begin(int width, int height);
    // Record actual writes AFTER ordinary clipping/transparency decisions.
    // An ordinary write retires a witness even when its color is identical.
    void wrote(int x, int y, std::uint8_t before, std::uint8_t after, bool projectileBody);
    void invalidate();
    void seal(bool paintHealthy);

    // fullPresentationAdmitted requires supported final copy/palette/flip and
    // current top-state/map/viewport proof. A fully covered or unproven viewport
    // is not KnownAbsent. Coverage rectangles are private, never public targets.
    AgentProjectileFact evaluate(bool fullPresentationAdmitted,
        const AgentProjectileRect &viewport, const std::vector<AgentProjectileRect> &covers,
        const Palette &finalPalette, const FinalPixelProof &proof,
        const std::function<bool(int, int)> &pixelAdmitted = std::function<bool(int, int)>()) const;
    // Complete known 4x4 glyph admission. Every original ink pixel must retain
    // this role's write and match successful final output; at least one must
    // contribute contrast. This does not classify arbitrary screen pixels.
    bool completeGlyph(int x, int y, std::uint16_t ink, const Palette &palette,
        const FinalPixelProof &proof) const;
    std::size_t storageCells() const { return _pixels.size(); }

private:
    struct Pixel { std::uint8_t before = 0, after = 0; bool candidate = false; };
    int _width = 0, _height = 0;
    bool _valid = false, _sealed = false;
    std::vector<Pixel> _pixels;
};

// Map-local actual-write adapter. Addresses are checked numerically against a
// bounded byte layout; no source surface, object identity or draw IDs survive.
enum class AgentRendererRole { Ordinary = -1, Projectile = 0, Impact = 1, Collapse = 2, ThrownItem = 3, LargeExplosion = 4 };
class AgentProjectileRaster
{
    std::array<AgentProjectileWitness *, 5> _witnesses = {{nullptr, nullptr, nullptr, nullptr, nullptr}};
    std::uintptr_t _begin = 0;
    std::size_t _span = 0;
    int _width = 0, _height = 0, _pitch = 0;
    AgentRendererRole _role = AgentRendererRole::Ordinary;
public:
    bool begin(AgentProjectileWitness &witness, void *bytes, int width, int height, int pitch);
    bool begin(const std::array<AgentProjectileWitness *, 5> &witnesses,
        void *bytes, int width, int height, int pitch);
    void role(AgentRendererRole value) { _role = value; }
    void body(bool value) { role(value ? AgentRendererRole::Projectile : AgentRendererRole::Ordinary); }
    void wrote(const void *address, std::uint8_t before, std::uint8_t after);
    void unknownWrite() { for (auto *w : _witnesses) if (w) w->invalidate(); }
};

// Episode presence, not a shot/attack counter. Unknown (including pause or
// failed presentation) preserves the latch. Only proven absence rearms it.
class AgentProjectileEpisodes
{
    bool _presented = false;
public:
    bool observe(AgentProjectileFact fact)
    {
        if (fact == AgentProjectileFact::Unknown) return false;
        if (fact == AgentProjectileFact::KnownAbsent) { _presented = false; return false; }
        if (_presented) return false;
        _presented = true;
        return true;
    }
};

} }
