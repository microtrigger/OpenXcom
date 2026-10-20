// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentProjectileWitness.h"
#include <string>
namespace OpenXcom { namespace Observer {
// Private ordinary-render roles only. No resource, object or world identifier.
enum class AgentMiniMapKind { Terrain = 0, Unit = 1, Item = 2 };
struct AgentMiniMapGlyph { AgentMiniMapKind kind; int x, y; };
class AgentMiniMapPresentation
{
    std::array<AgentProjectileWitness, 3> _witnesses;
    AgentProjectileRaster _raster;
    std::vector<std::uint16_t> _ink;
    std::vector<unsigned char> _previousRoles;
    int _width = 0, _height = 0, _columns = 0, _rows = 0;
    bool _valid = false;
    std::uint64_t _revision = 0;
public:
    bool begin(void *bytes, int width, int height, int pitch);
    void invalidate();
    AgentProjectileRaster *raster() { return &_raster; }
    void role(AgentMiniMapKind kind) { _raster.role(static_cast<AgentRendererRole>(kind)); }
    void ordinary() { _raster.role(AgentRendererRole::Ordinary); }
    // The caller records the ink mask of the exact ordinary 4x4 glyph just
    // blitted, after normal visibility selection. Zero/unsupported clears it.
    void glyph(AgentMiniMapKind kind, int x, int y, std::uint16_t ink);
    void retire(int x, int y, int width, int height);
    void seal(bool healthy);
    std::uint64_t revision() const { return _revision; }
    std::vector<AgentMiniMapGlyph> project(const AgentProjectileWitness::Palette &palette,
        const AgentProjectileWitness::FinalPixelProof &proof,
        const std::function<bool(int, int)> &cellAdmitted) const;
};
} }
