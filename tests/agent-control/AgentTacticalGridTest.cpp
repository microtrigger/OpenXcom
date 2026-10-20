// SPDX-License-Identifier: GPL-3.0-or-later
// Pure camera arithmetic only: no SDL, Game, map traversal, window or actions.
#include "../../src/Battlescape/CameraProjection.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
using namespace OpenXcom;
namespace {
unsigned checks = 0;
void check(bool value, const char *why)
{
    ++checks;
    if (!value) { std::cerr << "FAIL " << why << '\n'; std::exit(1); }
}
void oldUnclamped(const CameraProjectionMath &p, int x, int y, int &mapX, int &mapY)
{
    y += (-p.spriteWidth / 2) + p.viewLevel * ((p.spriteHeight + p.spriteWidth / 4) / 2);
    mapY = -x + p.offsetX + 2 * y - 2 * p.offsetY;
    mapX = y - p.offsetY - mapY / 4 - p.spriteWidth / 4;
    mapX /= p.spriteWidth / 4;
    mapY /= p.spriteWidth;
}
struct PrivateMapFixture
{
    CameraProjectionMath publicProjection;
    int width, height;
    PrivateMapFixture(int w, int h) : publicProjection(32, 40, 0, 0, 0), width(w), height(h) {}
    AgentMapSnapshot snapshot() const
    {
        return cameraMapSnapshot(publicProjection, 0, 0, 320, 140, false, "");
    }
    AgentCameraPoint ordinaryClampedInput(int x, int y) const
    {
        AgentCameraPoint p;
        publicProjection.screenToMapUnclamped(x, y, &p.x, &p.y);
        p.x = std::max(-1, std::min(width, p.x));
        p.y = std::max(-1, std::min(height, p.y));
        return p;
    }
};
}
int main()
{
    CameraProjectionMath p(32, 40, -250, 250, 2);
    int x, y;
    p.mapToScreen(3, 2, 2, &x, &y);
    check(x == 16 && y == -8, "forward camera transform retains ordinary projection");
    p.screenToMapUnclamped(-218, 264 + p.spriteHeight / 4, &x, &y);
    check(x == 3 && y == 2, "selector-offset center maps through shared inverse");
    for (int width : {16, 32, 48}) for (int level : {0, 1, 7})
        for (int offset : {-1000, -17, 0, 243})
        {
            CameraProjectionMath projection(width, 40, offset, -offset, level);
            for (int sx = -400; sx <= 400; sx += 31) for (int sy = -300; sy <= 300; sy += 37)
            {
                int oldX, oldY;
                oldUnclamped(projection, sx, sy, oldX, oldY);
                projection.screenToMapUnclamped(sx, sy, &x, &y);
                check(x == oldX && y == oldY, "shared inverse preserves ordinary negative truncation");
            }
        }

    bool truncated = false;
    CameraProjectionMath origin(32, 40, 0, 0, 0);
    auto grid = cameraViewportGrid(origin, 320, 140, truncated);
    check(grid.size() == 128 && truncated, "explicit public target cap and truncation");
    for (std::size_t i = 0; i < grid.size(); ++i)
    {
        const auto &cell = grid[i];
        check(cell.handle == "g" + std::to_string(i), "frame-local handles have no world identity");
        check(cell.target.x >= 0 && cell.target.x < 320 && cell.target.y >= 0 && cell.target.y < 140,
            "target inside public viewport");
        for (const auto &point : cell.polygon)
            check(point.x >= 0 && point.x < 320 && point.y >= 0 && point.y < 140,
                "entire polygon inside public viewport");
        check(cell.polygon[0].x == cell.target.x && cell.polygon[0].y == cell.target.y - 8 &&
            cell.polygon[1].x == cell.target.x + 16 && cell.polygon[1].y == cell.target.y &&
            cell.polygon[2].x == cell.target.x && cell.polygon[2].y == cell.target.y + 8 &&
            cell.polygon[3].x == cell.target.x - 16 && cell.polygon[3].y == cell.target.y,
            "ordered four-point camera diamond");
        if (i) check(cell.target.y > grid[i-1].target.y ||
            (cell.target.y == grid[i-1].target.y && cell.target.x > grid[i-1].target.x),
            "deterministic viewport row-major order");
    }
    const auto repeat = cameraViewportGrid(origin, 320, 140, truncated);
    check(grid == repeat, "repeat same projection has identical grid");
    auto shifted = cameraViewportGrid(CameraProjectionMath(32, 40, 1, 0, 0), 320, 140, truncated);
    check(grid != shifted, "subtile pan updates public phase");
    const auto latticeShift = cameraViewportGrid(CameraProjectionMath(32, 40, 16, 8, 0), 320, 140, truncated);
    check(grid == latticeShift, "integer world translation leaves public grid unchanged");
    const auto effectiveLevel = cameraViewportGrid(CameraProjectionMath(32, 40, 0, 24, 1), 320, 140, truncated);
    check(grid == effectiveLevel, "same projected plane hides private level identity");
    const auto extreme = cameraViewportGrid(CameraProjectionMath(32, 40, INT_MIN, INT_MAX, INT_MAX), 320, 140, truncated);
    check(!extreme.empty() && extreme.size() <= 128, "large private origin normalized with wide arithmetic");

    PrivateMapFixture small(4, 5), large(600, 700);
    check(!(small.ordinaryClampedInput(0, 1000) == large.ordinaryClampedInput(0, 1000)),
        "fixture private bounds genuinely change ordinary clamp");
    check(small.snapshot().grid == large.snapshot().grid &&
        small.snapshot().gridTruncated == large.snapshot().gridTruncated,
        "different private map dimensions cannot change public grid");

    check(cameraViewportGrid(origin, 641, 400, truncated).empty() && !truncated, "overwidth fails closed");
    check(cameraViewportGrid(origin, 640, 401, truncated).empty() && !truncated, "overheight fails closed");
    check(cameraViewportGrid(origin, 0, 140, truncated).empty(), "empty viewport fails closed");
    check(cameraViewportGrid(origin, -1, 140, truncated).empty(), "negative viewport fails closed");
    check(cameraViewportGrid(origin, 32, 30, truncated).empty(), "boundary vertex outside exclusive viewport rejected");
    auto one = cameraViewportGrid(origin, 33, 31, truncated);
    check(one.size() == 1 && !truncated && one[0].target == AgentCameraPoint{16, 22},
        "smallest full stock diamond without clipped neighbors");
    check(cameraViewportGrid(CameraProjectionMath(31, 40, 0, 0, 0), 320, 140, truncated).empty(),
        "unsupported non-quarter sprite projection fails closed");
    check(cameraViewportGrid(CameraProjectionMath(0, 40, 0, 0, 0), 320, 140, truncated).empty(),
        "zero-width projection cannot divide by zero in public grid");
    check(cameraViewportGrid(origin, 640, 400, truncated).size() == 128 && truncated,
        "largest supported viewport bounded without map traversal");

    auto terrain = cameraMapSnapshot(origin, 12, 7, 320, 140, false, "must not escape");
    check(terrain.x == 12 && terrain.y == 7 && terrain.width == 320 && terrain.height == 140 &&
        !terrain.hiddenMovement && terrain.hiddenMovementLabel.empty() && terrain.grid == grid,
        "map origin separate from viewport-local targets; inactive message omitted");
    auto hidden = cameraMapSnapshot(origin, 12, 7, 320, 140, true, "HIDDEN MOVEMENT");
    check(hidden.hiddenMovement && hidden.grid.empty() && !hidden.gridTruncated &&
        hidden.hiddenMovementLabel == "HIDDEN MOVEMENT", "exact hidden branch suppresses all grid geometry");
    hidden = cameraMapSnapshot(origin, 12, 7, 320, 140, true, "");
    check(hidden.hiddenMovement && hidden.grid.empty() && hidden.hiddenMovementLabel.empty(),
        "unreadable hidden label never re-enables underlying map or substitutes translation");
    // The production admission predicate consumes only post-overlay witnesses.
    // These fixtures model the proven visible result, not a renderer execution.
    const auto labeledHidden = cameraMapSnapshot(origin, 12, 7, 320, 140, true, "HIDDEN MOVEMENT");
    check(!agentMapModeAdmitted(terrain, false, 0) && !agentMapModeAdmitted(labeledHidden, false, 0),
        "paired fully covered branches collapse to identical unavailable mode");
    check(!agentMapModeAdmitted(hidden, true, 0), "empty drawn hidden label cannot claim mode");
    check(!agentMapModeAdmitted(labeledHidden, false, 128), "unproven hidden label cannot be rescued by camera geometry");
    check(!agentMapModeAdmitted(terrain, true, 0), "unrelated visible text cannot rescue wholly covered map");
    check(agentMapModeAdmitted(labeledHidden, true, 0), "complete uncovered hidden label proves shown mode");
    check(agentMapModeAdmitted(terrain, false, 1), "partial overlay with surviving full diamond admits canvas lattice");
    check(agentMapModeAdmitted(terrain, false, 128), "ordinary uncovered bounded map admitted");
    std::cout << "AgentTacticalGridTest PASS " << checks << " checks\n";
}
