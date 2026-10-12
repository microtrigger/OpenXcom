// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include <SDL.h>
namespace OpenXcom {
class Game; class State; class BattlescapeState;
namespace TurnControlGate {
bool active();
void prepare();
void silence();
bool loaded(Game *game);
void afterCleanup(Game *game);
void beforeEvents(Game *game);
bool acceptEvent(const SDL_Event &event);
void afterEvents(Game *game);
void beforeFrame(Game *game, State *top);
void afterFrame(Game *game);
void mapClicked(BattlescapeState *state);
void selectionClicked(BattlescapeState *state, bool next);
void kneelClicked(BattlescapeState *state);
int exitStatus();
} }
