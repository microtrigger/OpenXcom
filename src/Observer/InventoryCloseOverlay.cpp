// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
#include "InventoryCloseOverlay.h"
#include "StanceController.h"
#include "../Engine/Action.h"
#include "../Interface/Frame.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Mod/RuleInterface.h"
#include <climits>
namespace OpenXcom { namespace Observer {
namespace {
void styleSurface(Surface *surface, Element *element)
{
    if (!element) return;
    surface->setTFTDMode(element->TFTDMode);
    if (element->color != INT_MAX) surface->setColor(element->color);
    if (element->color2 != INT_MAX) surface->setSecondaryColor(element->color2);
    if (element->border != INT_MAX) surface->setBorderColor(element->border);
}
}
InventoryCloseOverlay::InventoryCloseOverlay(InventoryState *owner, RuleInterface *style) :
    InteractiveSurface(320, 200), _owner(owner), _controller(0),
    _frame(new Frame(320, 32, 0, 104)), _text(new Text(236, 30, 4, 105)),
    _yes(new TextButton(76, 14, 242, 105)), _no(new TextButton(76, 14, 242, 121)), _pressedChoice(0)
{
    if (style)
    {
        styleSurface(_frame, style->getElement("infoBoxOK"));
        styleSurface(_text, style->getElement("infoBoxOK"));
        styleSurface(_yes, style->getElement("infoBoxOKButton"));
        styleSurface(_no, style->getElement("infoBoxOKButton"));
    }
    _frame->setThickness(1); _frame->setHighContrast(true);
    _text->setHighContrast(true); _text->setWordWrap(true); _text->setAlign(ALIGN_CENTER);
    _text->setText("Ordinary inventory OK / close.\nNormal gravity, lighting and FOV.\nConfirm the soldier shown above.");
    _yes->setText("Confirm (Y)"); _no->setText("Cancel (Esc)");
    _yes->setHighContrast(true); _no->setHighContrast(true);
    SDL_SetColorKey(getSurface(), 0, 0);
    setVisible(false);
}
InventoryCloseOverlay::~InventoryCloseOverlay()
{
    delete _frame; delete _text; delete _yes; delete _no;
}
void InventoryCloseOverlay::initText(Font *big, Font *small, Language *language)
{
    _text->initText(big, small, language); _yes->initText(big, small, language); _no->initText(big, small, language);
}
void InventoryCloseOverlay::setPalette(SDL_Color *colors, int first, int count)
{
    InteractiveSurface::setPalette(colors, first, count);
    _frame->setPalette(colors, first, count); _text->setPalette(colors, first, count);
    _yes->setPalette(colors, first, count); _no->setPalette(colors, first, count);
}
void InventoryCloseOverlay::show(StanceController *controller)
{
    _controller = controller; _pressedChoice = 0;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND
    _text->setText(controller->inventoryGroundRequest() ?
        "Ordinary inventory Ground.\nRearranges ground slots/stacks\nand cycles the ground page." :
        "Ordinary inventory OK / close.\nNormal gravity, lighting and FOV.\nConfirm the soldier shown above.");
#endif
    const std::vector<unsigned char> &pixels = controller->indices();
    clear();
    if (pixels.size() != 320 * 200) { _controller = 0; return; }
    for (int y = 0; y < 200; ++y) for (int x = 0; x < 320; ++x) setPixel(x, y, pixels[y * 320 + x]);
    setPalette(controller->palette()); setVisible(true);
}
void InventoryCloseOverlay::dismiss()
{
    _controller = 0; _pressedChoice = 0; setVisible(false);
}
bool InventoryCloseOverlay::active() const { return _controller && turnContextVisible(); }
void InventoryCloseOverlay::blit(Surface *surface)
{
    if (!active()) return;
    Surface::blit(surface); _frame->blit(surface); _text->blit(surface); _yes->blit(surface); _no->blit(surface);
}
int InventoryCloseOverlay::choice(Action *action) const
{
    const double x = action->getAbsoluteXMouse(), y = action->getAbsoluteYMouse();
    if (x < 242 || x >= 318) return 0;
    if (y >= 105 && y < 119) return 1;
    if (y >= 121 && y < 135) return 2;
    return 0;
}
void InventoryCloseOverlay::handle(Action *action, State *)
{
    if (!active()) return;
    const SDL_Event &event = *action->getDetails();
    if (event.type == SDL_KEYDOWN)
    {
        _controller->confirmInventoryClose(_owner, event.key.keysym.sym == SDLK_y);
        return;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
    {
        action->setMouseAction(event.button.x, event.button.y, getX(), getY());
        if (event.button.button != SDL_BUTTON_LEFT) { _controller->confirmInventoryClose(_owner, false); return; }
        if (event.type == SDL_MOUSEBUTTONDOWN)
        {
            _pressedChoice = choice(action);
            if (!_pressedChoice) _controller->confirmInventoryClose(_owner, false);
            return;
        }
        const int pressed = _pressedChoice; _pressedChoice = 0;
        _controller->confirmInventoryClose(_owner, pressed == 1 && choice(action) == 1);
    }
}
} }
