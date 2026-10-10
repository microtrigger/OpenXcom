// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// This file is free software: you may redistribute and/or modify it under
// GNU GPL version 3 or, at your option, any later version.
// Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace OpenXcom { namespace Observer {
// Display-pixel geometry only. No map or world coordinates.
struct PixelCrop
{
	int screenWidth = 0, screenHeight = 0;
	int x = 0, y = 0, width = 0, height = 0;
	std::vector<unsigned char> rgb;
};
const int CursorCropSize = 128;
const int MaximumScreenSize = 4096;
bool cursorCropBounds(int screenWidth, int screenHeight, int cursorX, int cursorY, PixelCrop &out);
bool validCrop(const PixelCrop &crop);
class PresentedCursorCache
{
	std::string _session, _payload, _snapshot;
	std::uint64_t _revision;
	void rebuild();
public:
	explicit PresentedCursorCache(const std::string &session);
	bool observe(const PixelCrop &crop);
	bool close();
	const std::string &snapshot() const { return _snapshot; }
	std::uint64_t revision() const { return _revision; }
};
} }
