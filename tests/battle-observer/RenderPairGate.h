// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include <SDL.h>
#include "../../src/Observer/PresentedHud.h"
#include "../../src/Observer/PresentedCursor.h"
namespace OpenXcom {
class Game;
class Tile;
class Surface;
namespace RenderPairGate {
void prepare();
void silence();
bool loaded(Game *game);
bool active();
void afterCleanup(Game *game);
void beforeFrame(Game *game);
void visited(const Tile *tile, int animationPhase);
// SPDX-License-Identifier: GPL-3.0-or-later; smoke fixture addition, 2026-10-11.
#if defined(OPENXCOM_EXPERIMENTAL_SMOKE_DENSITY_GATE) || defined(OPENXCOM_EXPERIMENTAL_SMOKE_CLIPPING_GATE)
void smokeBlitted(const Tile *tile, int animationPhase, int frameNumber, int shade);
#endif
// SPDX-License-Identifier: GPL-3.0-or-later; clipping fixture addition, 2026-10-11.
#ifdef OPENXCOM_EXPERIMENTAL_SMOKE_CLIPPING_GATE
void smokeClipped(const Tile *tile, int animationPhase, int frameNumber, int shade,
                  Surface *sprite, Surface *destination, int x, int y, int tileWidth, int tileHeight);
#endif
void capture(SDL_Surface *surface, int baseWidth, int baseHeight, bool allowed,
             const Observer::HudFrame &hud, const Observer::PixelCrop &crop);
void afterFrame(Game *game);
int exitStatus();
} }
