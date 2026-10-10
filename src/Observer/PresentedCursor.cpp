// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// This file is free software: you may redistribute and/or modify it under
// GNU GPL version 3 or, at your option, any later version.
// Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
#include "PresentedCursor.h"
#include <algorithm>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace OpenXcom { namespace Observer {
bool cursorCropBounds(int sw, int sh, int cx, int cy, PixelCrop &out)
{
	out = PixelCrop();
	if (sw <= 0 || sh <= 0 || sw > MaximumScreenSize || sh > MaximumScreenSize ||
		cx < 0 || cy < 0 || cx >= sw || cy >= sh) return false;
	out.screenWidth = sw; out.screenHeight = sh;
	out.x = std::max(0, cx - CursorCropSize / 2);
	out.y = std::max(0, cy - CursorCropSize / 2);
	out.width = std::min(sw, cx + CursorCropSize / 2) - out.x;
	out.height = std::min(sh, cy + CursorCropSize / 2) - out.y;
	return true;
}
bool validCrop(const PixelCrop &c)
{
	return c.screenWidth > 0 && c.screenWidth <= MaximumScreenSize &&
		c.screenHeight > 0 && c.screenHeight <= MaximumScreenSize &&
		c.width > 0 && c.width <= CursorCropSize && c.height > 0 && c.height <= CursorCropSize &&
		c.x >= 0 && c.y >= 0 && c.x <= c.screenWidth - c.width && c.y <= c.screenHeight - c.height &&
		c.rgb.size() == static_cast<std::size_t>(c.width * c.height * 3);
}
PresentedCursorCache::PresentedCursorCache(const std::string &session) :
	_session(session), _payload("null"), _revision(0)
{
	if (session.size() != 32 || session.find_first_not_of("0123456789abcdef") != std::string::npos)
		throw std::invalid_argument("invalid observer session");
	rebuild();
}
void PresentedCursorCache::rebuild()
{
	std::ostringstream out; out.imbue(std::locale::classic());
	out << "{\"schema\":\"experimental-presented-cursor/0\",\"session\":\"" << _session
		<< "\",\"semantics\":\"last_observation\",\"freshness\":\"unverified\","
		<< "\"profile\":\"software-single-buffer-8bit-integer-scale-no-bands\","
		<< "\"unavailable_or_unsupported\":" << (_payload == "null" ? "true" : "false")
		<< ",\"public_revision\":" << _revision << ",\"crop\":" << _payload << "}\n";
	_snapshot = out.str();
}
std::string serializeCrop(const PixelCrop &c)
{
	if (!validCrop(c)) return "null";
	std::ostringstream out; out.imbue(std::locale::classic());
	out << "{\"screen_width\":" << c.screenWidth << ",\"screen_height\":" << c.screenHeight
		<< ",\"x\":" << c.x << ",\"y\":" << c.y << ",\"width\":" << c.width
		<< ",\"height\":" << c.height << ",\"format\":\"rgb24-hex-row-major\",\"pixels\":\"";
	const char *hex = "0123456789abcdef";
	for (std::size_t i = 0; i < c.rgb.size(); ++i) out << hex[c.rgb[i] >> 4] << hex[c.rgb[i] & 15];
	out << "\"}";
	return out.str();
}
bool PresentedCursorCache::observe(const PixelCrop &c)
{
	if (!validCrop(c)) return close();
	const std::string payload = serializeCrop(c);
	// Compare exact public bytes, never a hash or hidden frame/tick counter.
	if (payload == _payload) return false;
	_payload = payload; ++_revision; rebuild(); return true;
}
bool PresentedCursorCache::close()
{
	if (_payload == "null") return false;
	_payload = "null"; ++_revision; rebuild(); return true;
}
} }
