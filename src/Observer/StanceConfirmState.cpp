// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution, 2026-10-11; selection extension, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
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
    Frame *frame = new Frame(310, 136, 5, 4);
    Text *text = new Text(292, 96, 14, 10);
    TextButton *yes = new TextButton(100, 20, 45, 108);
    TextButton *no = new TextButton(100, 20, 175, 108);
    Surface *panel = new Surface(320, 56, 0, 144);
    add(frame, "infoBoxOK", "battlescape");
    add(text, "infoBoxOK", "battlescape");
    add(yes, "infoBoxOKButton", "battlescape");
    add(no, "infoBoxOKButton", "battlescape");
    add(panel);
    frame->setThickness(3); frame->setHighContrast(true);
    text->setHighContrast(true); text->setWordWrap(true); text->setAlign(ALIGN_CENTER);
    if (controller->selectionRequest())
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
    const std::vector<unsigned char> &pixels = controller->indices();
    for (int y = 0; y < 56; ++y) for (int x = 0; x < 320; ++x) panel->setPixel(x, y, pixels[y * 320 + x]);
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
