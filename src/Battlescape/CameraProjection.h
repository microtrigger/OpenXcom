// SPDX-License-Identifier: GPL-3.0-or-later
// Shared camera arithmetic and bounded, viewport-only native targets, 2026-10-18.
#pragma once
#include <cstdint>
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include <array>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#endif

namespace OpenXcom
{

// No map dimensions, terrain, actors or tile validity enter this arithmetic.
// The ordinary input path applies its existing private clamp AFTER this step.
struct CameraProjectionMath
{
    int spriteWidth, spriteHeight, offsetX, offsetY, viewLevel;
    CameraProjectionMath(int w, int h, int x, int y, int level) :
        spriteWidth(w), spriteHeight(h), offsetX(x), offsetY(y), viewLevel(level) {}

    std::int64_t levelStep() const { return (spriteHeight + spriteWidth / 4) / 2; }

    void screenToMapUnclamped(int screenX, int screenY, int *mapX, int *mapY) const
    {
        // Retain ordinary C++ integer truncation, including for negative values.
        const std::int64_t y = std::int64_t(screenY) - spriteWidth / 2 + std::int64_t(viewLevel) * levelStep();
        const std::int64_t row = -std::int64_t(screenX) + offsetX + 2 * y - 2 * std::int64_t(offsetY);
        *mapX = int((y - offsetY - row / 4 - spriteWidth / 4) / (spriteWidth / 4));
        *mapY = int(row / spriteWidth);
    }

    void mapToScreen(int mapX, int mapY, int mapZ, int *screenX, int *screenY) const
    {
        *screenX = int(std::int64_t(mapX) * (spriteWidth / 2) - std::int64_t(mapY) * (spriteWidth / 2));
        *screenY = int(std::int64_t(mapX) * (spriteWidth / 4) + std::int64_t(mapY) * (spriteWidth / 4) - std::int64_t(mapZ) * levelStep());
    }
};

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
struct AgentCameraPoint
{
    int x, y;
    bool operator==(const AgentCameraPoint &other) const { return x == other.x && y == other.y; }
};

struct AgentCameraGridTarget
{
    std::string handle;
    // Top, right, bottom, left, entirely inside the viewport. No tile identity.
    std::array<AgentCameraPoint, 4> polygon;
    AgentCameraPoint target;
    bool operator==(const AgentCameraGridTarget &other) const
    { return handle == other.handle && polygon == other.polygon && target == other.target; }
};

struct AgentCameraRect
{
    int x = 0, y = 0, width = 0, height = 0;
};

// Semantic facts admitted by the ordinary terrain draw, not pixel recognition.
struct AgentMapPosition
{
    int x = 0, y = 0, z = 0;
    bool operator==(const AgentMapPosition &o) const { return x == o.x && y == o.y && z == o.z; }
};
struct AgentMapCell
{
    AgentMapPosition position;
    // unknown, empty, floor/wall/door/object. No raw terrain IDs or properties.
    std::array<std::string, 4> parts = {{"unknown", "unknown", "unknown", "unknown"}};
    std::string unit = "unknown"; // never infer absence from hidden occupancy
    bool rendered = false; // private draw-branch witness
    bool operator==(const AgentMapCell &o) const
    { return position == o.position && parts == o.parts && unit == o.unit && rendered == o.rendered; }
};
inline std::string agentMapPart(bool discoveredContent, bool discoveredWall,
    bool present, bool sprite, bool door, const char *kind)
{
    if (discoveredContent) return present ? (sprite ? (door ? "door" : kind) : "unknown") : "empty";
    return discoveredWall && present && sprite && door ? "door" : "unknown";
}

struct AgentMapSnapshot
{
    int x = 0, y = 0, width = 0, height = 0;
    bool hiddenMovement = false, gridTruncated = false;
    std::string hiddenMovementLabel;
    // Private viewport-local painted ink, for the owner's later-overlay check.
    // It is not a semantic map field and must not be serialized.
    AgentCameraRect messageRegion;
    std::vector<AgentCameraGridTarget> grid;
    std::vector<AgentMapCell> cells;
    bool selectedPositionKnown = false;
    AgentMapPosition selectedPosition;
    AgentCameraPoint focus = {0, 0}; // viewport-only ordinary centering activation point
};

// Branch selection alone is not a public mode witness. The state provider must
// first prove a complete surviving message or at least one surviving diamond.
// Both fully covered branches collapse to the same unavailable DTO.
inline bool agentMapModeAdmitted(const AgentMapSnapshot &snapshot,
    bool completeMessageUncovered, std::size_t visibleGridCount)
{
    return snapshot.hiddenMovement ?
        completeMessageUncovered && !snapshot.hiddenMovementLabel.empty() : visibleGridCount != 0;
}

inline std::int64_t cameraPositiveMod(std::int64_t value, std::int64_t modulus)
{
    const std::int64_t remainder = value % modulus;
    return remainder < 0 ? remainder + modulus : remainder;
}

// A read-only mathematical lattice, NOT evidence of a visible/valid/passable tile.
// Enumeration uses only the public viewport and camera projection phase. In
// particular it never traverses the map or uses its dimensions as a stopping rule.
inline std::vector<AgentCameraGridTarget> cameraViewportGrid(const CameraProjectionMath &projection,
    int width, int height, bool &truncated)
{
    std::vector<AgentCameraGridTarget> result;
    truncated = false;
    if (width <= 0 || height <= 0 || width > 640 || height > 400 ||
        projection.spriteWidth < 4 || projection.spriteWidth > 128 || projection.spriteWidth % 4 ||
        projection.spriteHeight <= 0 || projection.spriteHeight > 256) return result;

    const int half = projection.spriteWidth / 2, quarter = projection.spriteWidth / 4;
    // Invert the same camera transform used by ordinary mouse selection. The
    // extra spriteHeight/4 matches Map::setSelectorPosition's existing input.
    // Integer lattice centers have half-cell offsets; only their phase survives.
    const std::int64_t originX = std::int64_t(projection.offsetX) + half;
    const std::int64_t originY = std::int64_t(projection.offsetY) -
        std::int64_t(projection.viewLevel) * projection.levelStep() +
        projection.spriteWidth - projection.spriteHeight / 4;
    const AgentCameraPoint focus = {width / 2 + half, height / 2 + projection.spriteWidth - projection.spriteHeight / 4};
    const auto distance = [&](const AgentCameraGridTarget &a) {
        const std::int64_t dx = std::int64_t(a.target.x) - focus.x, dy = std::int64_t(a.target.y) - focus.y;
        return dx * dx + dy * dy;
    };
    const auto closer = [&](const AgentCameraGridTarget &a, const AgentCameraGridTarget &b) {
        if (distance(a) != distance(b)) return distance(a) < distance(b);
        return a.target.y != b.target.y ? a.target.y < b.target.y : a.target.x < b.target.x;
    };
    // Geometry-only top128. The heap never holds more than128 candidates,
    // including the smallest supported sprite/maximum viewport combination.
    std::priority_queue<AgentCameraGridTarget, std::vector<AgentCameraGridTarget>, decltype(closer)> nearest(closer);
    const int firstY = quarter + int(cameraPositiveMod(originY - quarter, quarter));
    for (int y = firstY; y + quarter < height; y += quarter)
    {
        const std::int64_t row = (y - originY) / quarter;
        const int firstX = half + int(cameraPositiveMod(originX + row * half - half, projection.spriteWidth));
        for (int x = firstX; x + half < width; x += projection.spriteWidth)
        {
            AgentCameraGridTarget target;
            target.target = {x, y};
            target.polygon = {{{x, y - quarter}, {x + half, y}, {x, y + quarter}, {x - half, y}}};
            if (nearest.size() < 128) nearest.push(target);
            else
            {
                truncated = true;
                if (closer(target, nearest.top())) { nearest.pop(); nearest.push(target); }
            }
        }
    }
    while (!nearest.empty()) { result.push_back(nearest.top()); nearest.pop(); }
    std::sort(result.begin(), result.end(), closer);
    for (std::size_t i = 0; i < result.size(); ++i) result[i].handle = "g" + std::to_string(i);
    return result;
}

inline AgentMapSnapshot cameraMapSnapshot(const CameraProjectionMath &projection,
    int x, int y, int width, int height, bool hiddenMovement, const std::string &paintedMessage)
{
    AgentMapSnapshot snapshot;
    snapshot.x = x; snapshot.y = y; snapshot.width = width; snapshot.height = height;
    snapshot.hiddenMovement = hiddenMovement;
    // centerOnPosition aligns the tile's projected origin with the viewport
    // center. This activation offset is the same selector arithmetic; no actor
    // anchor, world coordinate or map validity enters this public geometry.
    snapshot.focus = {width / 2 + projection.spriteWidth / 2,
        height / 2 + projection.spriteWidth - projection.spriteHeight / 4};
    // Whole-map Hidden Movement suppression is independent of message legibility.
    if (hiddenMovement) snapshot.hiddenMovementLabel = paintedMessage;
    else snapshot.grid = cameraViewportGrid(projection, width, height, snapshot.gridTruncated);
    return snapshot;
}
#endif
}
