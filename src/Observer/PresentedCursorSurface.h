// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// This file is free software: you may redistribute and/or modify it under
// GNU GPL version 3 or, at your option, any later version.
// Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include "PresentedCursor.h"
#include <SDL.h>
namespace OpenXcom { namespace Observer {
// Caller must supply the final software display surface after successful flip.
// Synthetic test surfaces do not establish actual desktop presentation.
bool supportedPresentedSurface(SDL_Surface *screen, int baseWidth, int baseHeight, bool profileAllowed);
PixelCrop capturePresentedCursor(SDL_Surface *screen, int baseWidth, int baseHeight,
	int cursorX, int cursorY, bool profileAllowed);
} }
