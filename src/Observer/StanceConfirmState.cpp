// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution, 2026-10-11; selection/turn/map-left-click extensions, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
#include "StanceConfirmState.h"
#include "StanceController.h"
#include "../Engine/Action.h"
#include "../Engine/Surface.h"
#include "../Interface/Frame.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
namespace OpenXcom { namespace Observer {
StanceConfirmState::StanceConfirmState(StanceController *controller) : _controller(controller)
{
    _screen = true;
    setPalette(controller->palette());
    const bool turn = controller->turnRequest() || controller->mapLeftClickRequest();
    Surface *panel = new Surface(320, turn ? 200 : 56, 0, turn ? 0 : 144);
    const std::vector<unsigned char> &pixels = controller->indices();
    for (int y = 0; y < panel->getHeight(); ++y) for (int x = 0; x < 320; ++x)
        panel->setPixel(x, y, pixels[y * 320 + x]);
    add(panel);
    Frame *frame = new Frame(turn ? 320 : 310, turn ? 32 : 136, turn ? 0 : 5, turn ? 0 : 4);
    Text *text = new Text(turn ? 236 : 292, turn ? 30 : 96, turn ? 4 : 14, turn ? 1 : 10);
    TextButton *yes = new TextButton(turn ? 76 : 100, turn ? 14 : 20, turn ? 242 : 45, turn ? 1 : 108);
    TextButton *no = new TextButton(turn ? 76 : 100, turn ? 14 : 20, turn ? 242 : 175, turn ? 17 : 108);
    add(frame, "infoBoxOK", "battlescape");
    add(text, "infoBoxOK", "battlescape");
    add(yes, "infoBoxOKButton", "battlescape");
    add(no, "infoBoxOKButton", "battlescape");
    frame->setThickness(turn ? 1 : 3); frame->setHighContrast(true);
    text->setHighContrast(true); text->setWordWrap(true); text->setAlign(ALIGN_CENTER);
    if (turn)
    {
        text->setText(controller->mapLeftClickRequest() ?
            "Ordinary left-click: may move/select.\nNormal TU, reserves and reactions.\nConfirm the marked location." :
            "Ordinary right-click: turn/open door.\nNormal TU and reserves apply.\nConfirm the marked location.");
        // A two-tone ring centered on the exact requested logical pixel. The
        // original captured map and bottom HUD remain behind this small marker.
        int light = 0, dark = 0;
        const SDL_Color *palette = controller->palette();
        for (int i = 1; i < 256; ++i)
        {
            const int value = palette[i].r * 299 + palette[i].g * 587 + palette[i].b * 114;
            if (value > palette[light].r * 299 + palette[light].g * 587 + palette[light].b * 114) light = i;
            if (value < palette[dark].r * 299 + palette[dark].g * 587 + palette[dark].b * 114) dark = i;
        }
        for (int dy = -6; dy <= 6; ++dy) for (int dx = -6; dx <= 6; ++dx)
        {
            const int squared = dx * dx + dy * dy;
            if (squared >= 16 && squared <= 36)
                panel->setPixel(controller->targetX() + dx, controller->targetY() + dy, squared <= 25 ? light : dark);
        }
        panel->setPixel(controller->targetX(), controller->targetY(), light);
    }
    else if (controller->selectionRequest())
        text->setText(std::string("SUPERVISED SELECTION PILOT\nUse the normal ") +
            (controller->nextSelection() ? "NEXT" : "PREVIOUS") +
            " unit control? This may select another eligible unit, center the camera and cancel current actions."
            "\nBelow: the current observed HUD. Confirm only if it matches your intent.");
    else text->setText(std::string("SUPERVISED STANCE PILOT\nSet the soldier shown below to ") +
        (controller->desiredKneel() ? "KNEEL?\nNormal cost: 4 TU." : "STAND?\nNormal cost: 8 TU.") +
        " Reservations still apply. This can trigger reaction fire.\nBelow: the proposal's observed HUD. Confirm only if it matches your intent.");
    yes->setText("Confirm (Y)"); no->setText("Cancel (Esc)");
    yes->setHighContrast(true); no->setHighContrast(true);
    yes->onMouseClick((ActionHandler)&StanceConfirmState::approve);
    yes->onKeyboardPress((ActionHandler)&StanceConfirmState::approve, SDLK_y);
    no->onMouseClick((ActionHandler)&StanceConfirmState::cancel);
    no->onKeyboardPress((ActionHandler)&StanceConfirmState::cancel, SDLK_ESCAPE);
}
void StanceConfirmState::approve(Action *) { _controller->confirm(this, true); }
void StanceConfirmState::cancel(Action *) { _controller->confirm(this, false); }
void StanceConfirmState::handle(Action *action)
{
    // No game hotkey can pass through the modal. Only explicit Y or button OK.
    if (action->getDetails()->type == SDL_KEYDOWN &&
        action->getDetails()->key.keysym.sym != SDLK_y && action->getDetails()->key.keysym.sym != SDLK_ESCAPE)
    { cancel(action); return; }
    State::handle(action);
}
} }
