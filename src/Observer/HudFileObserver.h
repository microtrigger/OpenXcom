// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// This file is free software: you may redistribute and/or modify it under
// GNU GPL version 3 or, at your option, any later version.
// Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include "PublicHud.h"
#include <memory>

namespace OpenXcom
{
namespace Observer
{
// POSIX-only experimental transport. No input protocol or gameplay commands.
// Requires an explicitly supplied, pre-existing, owner-only directory.
class HudFileObserver
{
	int _directory, _lock;
	std::unique_ptr<PublicHudCache> _cache;
	bool publish();
	void disable();
public:
	explicit HudFileObserver(const char *directory);
	~HudFileObserver();
	void observe(const PublicHud &hud);
	bool enabled() const { return _cache.get() != 0; }
private:
	HudFileObserver(const HudFileObserver &);
	HudFileObserver &operator=(const HudFileObserver &);
};
}
}
