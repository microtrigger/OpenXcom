// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include <SDL.h>
namespace OpenXcom {
class Game; class State; class BattlescapeState; class InventoryState; class Inventory; class Action;
namespace InventoryRightClickControlGate {
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
void inventoryConstructed(InventoryState *state);
void inventoryDisplayed(InventoryState *state);
void inventoryInitialized(InventoryState *state);
void inventoryCloseEntered(InventoryState *state);
void inventoryMouseOverEntered(Inventory *inventory, Action *action, State *state);
void inventoryClickEntered(Inventory *inventory, Action *action, State *state);
void inventoryInvClickEntered(InventoryState *state);
void inventoryDestroyed(InventoryState *state);
void clickOverlayShown(InventoryState *state);
void clickOverlayDismissed(InventoryState *state);
void inventoryGroundArranged(Inventory *inventory);
void kneelClicked(BattlescapeState *state);
void selectionClicked(BattlescapeState *state, bool next);
int exitStatus();
} }
