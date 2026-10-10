// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include <SDL.h>
namespace OpenXcom {
class Game; class State; class Sound;
namespace BattlefieldAudioGate {
void prepare();
bool loaded(Game *game);
void afterCleanup(Game *game);
void beforeEvents(Game *game);
bool acceptEvent(const SDL_Event &event);
void afterEvents(Game *game);
void beforeFrame(Game *game, State *top);
void beforePoll();
void afterPoll(Game *game);
void played(const Sound *sound, int channel);
int exitStatus();
} }
