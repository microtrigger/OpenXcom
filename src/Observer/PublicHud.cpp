// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// This file is free software: you may redistribute and/or modify it under
// GNU GPL version 3 or, at your option, any later version.
// Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
#include "PublicHud.h"
#include <locale>
#include <sstream>
#include <stdexcept>

namespace OpenXcom
{
namespace Observer
{
PublicHudCache::PublicHudCache(const std::string &session) : _session(session), _payload("{\"sampled\":false}"), _revision(0)
{
	// The only string entering JSON is a validated, transport-created nonce.
	if (session.size() != 32 || session.find_first_not_of("0123456789abcdef") != std::string::npos)
		throw std::invalid_argument("invalid observer session");
	rebuild();
}

void PublicHudCache::rebuild()
{
	std::ostringstream out;
	out.imbue(std::locale::classic());
	out << "{\"schema\":\"experimental-public-hud/0\",\"session\":\"" << _session
		<< "\",\"semantics\":\"last_observation\",\"freshness\":\"unverified\",\"public_revision\":" << _revision << ",\"observation\":" << _payload << "}\n";
	_snapshot = out.str();
}

bool PublicHudCache::observe(const PublicHud &hud)
{
	std::ostringstream out;
	out.imbue(std::locale::classic());
	out << "{\"sampled\":true,\"selected_hud\":";
	if (!hud.selected)
		out << "null";
	else
		out << "{\"time_units\":" << hud.timeUnits << ",\"energy\":" << hud.energy
			<< ",\"health\":" << hud.health << ",\"morale\":" << hud.morale << "}";
	out << "}";
	if (out.str() == _payload)
		return false;
	_payload = out.str();
	++_revision;
	rebuild();
	return true;
}

bool PublicHudCache::close()
{
	const std::string closed = "{\"sampled\":false}";
	if (_payload == closed)
		return false;
	_payload = closed;
	++_revision;
	rebuild();
	return true;
}
}
}
