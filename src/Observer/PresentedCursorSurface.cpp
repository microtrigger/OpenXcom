// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// This file is free software: you may redistribute and/or modify it under
// GNU GPL version 3 or, at your option, any later version.
// Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
#include "PresentedCursorSurface.h"
namespace OpenXcom { namespace Observer {
PixelCrop capturePresentedCursor(SDL_Surface *screen, int bw, int bh, int cx, int cy, bool allowed)
{
	PixelCrop crop;
	if (!allowed || !screen || !screen->format || !screen->pixels || bw <= 0 || bh <= 0 ||
		bw > MaximumScreenSize || bh > MaximumScreenSize ||
		(screen->flags & (SDL_OPENGL | SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_ASYNCBLIT)) ||
		screen->w <= 0 || screen->h <= 0 || screen->w > MaximumScreenSize || screen->h > MaximumScreenSize ||
		screen->format->BitsPerPixel != 8 || screen->format->BytesPerPixel != 1 ||
		!screen->format->palette || !screen->format->palette->colors ||
		screen->format->palette->ncolors != 256 || screen->pitch < screen->w ||
		screen->w % bw || screen->h % bh || screen->w / bw != screen->h / bh ||
		screen->w / bw < 1 || screen->w / bw > 6 || cx < 0 || cy < 0 || cx >= bw || cy >= bh)
		return crop;
	const int scale = screen->w / bw;
	if (!cursorCropBounds(screen->w, screen->h, cx * scale, cy * scale, crop)) return PixelCrop();
	crop.rgb.resize(crop.width * crop.height * 3);
	if (SDL_LockSurface(screen) != 0) return PixelCrop();
	std::size_t at = 0;
	for (int y = crop.y; y < crop.y + crop.height; ++y)
	{
		const unsigned char *row = static_cast<const unsigned char *>(screen->pixels) + y * screen->pitch;
		for (int x = crop.x; x < crop.x + crop.width; ++x)
		{
			const SDL_Color &color = screen->format->palette->colors[row[x]];
			crop.rgb[at++] = color.r; crop.rgb[at++] = color.g; crop.rgb[at++] = color.b;
		}
	}
	SDL_UnlockSurface(screen);
	return crop;
}
} }
