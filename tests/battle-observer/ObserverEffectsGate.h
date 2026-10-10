// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include <SDL.h>
namespace OpenXcom {
class Game;
class BattlescapeState;
namespace ObserverEffectsGate {
void prepare();
void silence();
bool loaded(Game *game);
void afterCleanup(Game *game);
void beforeEvents(Game *game);
bool acceptEvent(const SDL_Event &event);
void afterEvents(Game *game);
void thought(BattlescapeState *state, bool clear);
void animated(BattlescapeState *state);
void handledState(BattlescapeState *state);
void beforeFrame(Game *game);
void afterFrame(Game *game);
int exitStatus();
} }
