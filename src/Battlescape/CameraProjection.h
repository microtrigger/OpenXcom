// SPDX-License-Identifier: GPL-3.0-or-later
// Shared camera arithmetic and bounded, viewport-only native targets, 2026-10-18.
#pragma once
#include <cstdint>
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include <array>
#include <string>
#include <vector>
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

struct AgentMapSnapshot
{
    int x = 0, y = 0, width = 0, height = 0;
    bool hiddenMovement = false, gridTruncated = false;
    std::string hiddenMovementLabel;
    // Private viewport-local painted ink, for the owner's later-overlay check.
    // It is not a semantic map field and must not be serialized.
    AgentCameraRect messageRegion;
    std::vector<AgentCameraGridTarget> grid;
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
    const int firstY = quarter + int(cameraPositiveMod(originY - quarter, quarter));
    for (int y = firstY; y + quarter < height; y += quarter)
    {
        const std::int64_t row = (y - originY) / quarter;
        const int firstX = half + int(cameraPositiveMod(originX + row * half - half, projection.spriteWidth));
        for (int x = firstX; x + half < width; x += projection.spriteWidth)
        {
            if (result.size() == 128) { truncated = true; return result; }
            AgentCameraGridTarget target;
            target.handle = "g" + std::to_string(result.size());
            target.target = {x, y};
            target.polygon = {{{x, y - quarter}, {x + half, y}, {x, y + quarter}, {x - half, y}}};
            result.push_back(target);
        }
    }
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
