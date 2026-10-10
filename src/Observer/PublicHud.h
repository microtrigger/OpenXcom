// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// This file is free software: you may redistribute and/or modify it under
// GNU GPL version 3 or, at your option, any later version.
// Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
// Experimental public-HUD observer. GPL-3.0-or-later, like OpenXcom.
#include <cstdint>
#include <string>

namespace OpenXcom
{
namespace Observer
{
// This DTO deliberately cannot hold units, tiles, native IDs or savegame state.
struct PublicHud
{
	bool selected;
	unsigned int timeUnits, energy, health, morale;
	PublicHud() : selected(false), timeUnits(0), energy(0), health(0), morale(0) {}
};

class PublicHudCache
{
	std::string _session, _payload, _snapshot;
	std::uint64_t _revision;
	void rebuild();
public:
	explicit PublicHudCache(const std::string &session);
	bool observe(const PublicHud &hud);
	bool close();
	const std::string &snapshot() const { return _snapshot; }
	std::uint64_t revision() const { return _revision; }
};
}
}
