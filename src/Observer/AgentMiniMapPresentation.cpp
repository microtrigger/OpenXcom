// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentMiniMapPresentation.h"
#include <new>
#include <algorithm>
namespace OpenXcom { namespace Observer {
bool AgentMiniMapPresentation::begin(void *bytes, int width, int height, int pitch)
{
    _valid = false; _ink.clear();
    if (width <= 0 || height <= 0 || width > 640 || height > 400) return false;
    _width = width; _height = height; _columns = (width + 3) / 4; _rows = (height + 3) / 4;
    try { _ink.assign(3 * _columns * _rows, 0); } catch (const std::bad_alloc &) { return false; }
    _valid = _raster.begin({{&_witnesses[0], &_witnesses[1], &_witnesses[2], nullptr, nullptr}},
        bytes, width, height, pitch);
    return _valid;
}
void AgentMiniMapPresentation::invalidate()
{
    _valid = false; _ink.clear();
    for (auto &w : _witnesses) w.invalidate();
}
void AgentMiniMapPresentation::glyph(AgentMiniMapKind kind, int x, int y, std::uint16_t ink)
{
    if (!_valid || int(kind) < 0 || int(kind) > 2 || x < 0 || y < 0 ||
        x % 4 || y % 4 || x >= _width || y >= _height) return;
    _ink[(int(kind) * _rows + y / 4) * _columns + x / 4] = ink;
}
void AgentMiniMapPresentation::retire(int x, int y, int width, int height)
{
    if (!_valid || width <= 0 || height <= 0) return;
    const int right = int(std::min<std::int64_t>(_width, std::int64_t(x) + width));
    const int bottom = int(std::min<std::int64_t>(_height, std::int64_t(y) + height));
    for (int py = std::max(0,y); py < bottom; ++py)
        for (int px = std::max(0,x); px < right; ++px)
            for (auto &w : _witnesses) w.wrote(px,py,0,0,false);
}
void AgentMiniMapPresentation::seal(bool healthy)
{
    if (!_valid || !healthy) { invalidate(); return; }
    for (auto &w : _witnesses) w.seal(healthy);
    // Blink frames with the same role geometry do not stale an action. Genuine
    // draw-role presence ABA is retained privately even if no read occurs.
    std::vector<unsigned char> roles(_ink.size());
    for (std::size_t i = 0; i < _ink.size(); ++i) roles[i] = _ink[i] ? 1 : 0;
    if (roles != _previousRoles) { ++_revision; _previousRoles.swap(roles); }
}
std::vector<AgentMiniMapGlyph> AgentMiniMapPresentation::project(
    const AgentProjectileWitness::Palette &palette,
    const AgentProjectileWitness::FinalPixelProof &proof,
    const std::function<bool(int, int)> &cellAdmitted) const
{
    std::vector<AgentMiniMapGlyph> out;
    if (!_valid || !proof || !cellAdmitted) return out;
    for (int y = 0; y + 4 <= _height; y += 4)
        for (int x = 0; x + 4 <= _width; x += 4)
        {
            if (!cellAdmitted(x, y)) continue;
            for (int k = 0; k < 3; ++k)
            {
                const std::uint16_t ink = _ink[(k * _rows + y / 4) * _columns + x / 4];
                if (_witnesses[k].completeGlyph(x, y, ink, palette, proof))
                    out.push_back({static_cast<AgentMiniMapKind>(k), x, y});
            }
        }
    return out;
}
} }
