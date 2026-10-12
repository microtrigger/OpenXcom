// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include "../Engine/InteractiveSurface.h"
namespace OpenXcom {
class InventoryState; class Frame; class Text; class TextButton; class RuleInterface;
namespace Observer {
class StanceController;
// State-owned presentation only. No inventory or item manipulation lives here.
class InventoryCloseOverlay : public InteractiveSurface {
    InventoryState *_owner;
    StanceController *_controller;
    Frame *_frame;
    Text *_text;
    TextButton *_yes, *_no;
    int _pressedChoice;
    int choice(Action *action) const;
public:
    InventoryCloseOverlay(InventoryState *owner, RuleInterface *style);
    ~InventoryCloseOverlay();
    void initText(Font *big, Font *small, Language *language) override;
    void setPalette(SDL_Color *colors, int first = 0, int count = 256) override;
    void show(StanceController *controller);
    void dismiss();
    bool active() const;
    void blit(Surface *surface) override;
    void handle(Action *action, State *state) override;
};
} }
