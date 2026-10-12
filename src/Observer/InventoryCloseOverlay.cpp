// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
#include "InventoryCloseOverlay.h"
#include "StanceController.h"
#include "../Engine/Action.h"
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
#include "../Battlescape/InventoryState.h"
#endif
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
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
    , _clickMode(false), _pointer(new Surface(17, 17))
#endif
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
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
    delete _pointer;
#endif
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
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
    _pointer->setPalette(colors, first, count);
#endif
}
void InventoryCloseOverlay::show(StanceController *controller)
{
    _controller = controller; _pressedChoice = 0;
    _text->setText(
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND
        controller->inventoryGroundRequest() ?
        "Ordinary inventory Ground.\nRearranges ground slots/stacks\nand cycles the ground page." :
#endif
        "Ordinary inventory OK / close.\nNormal gravity, lighting and FOV.\nConfirm the soldier shown above.");
    int bannerY = 104;
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
    _clickMode = controller->inventoryClickRequest() || controller->inventoryRightClickRequest() || controller->inventoryUnloadRequest();
    if (_clickMode)
    {
        bannerY = _owner->inventoryClickBannerY();
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD
        if (controller->inventoryUnloadRequest())
        {
            bannerY = _owner->inventoryUnloadBannerY();
            _text->setText("Ordinary inventory Unload.\nMay spend 8 TU; normal refusal.\nConfirm the soldier shown above.");
        }
        else
#endif
        if (controller->inventoryRightClickRequest())
            _text->setText("RIGHT (" + std::to_string(controller->targetX()) + "," + std::to_string(controller->targetY()) +
                "): held = cancel.\nOtherwise closes inventory.\nNormal gravity, lighting/FOV.");
        else _text->setText("LEFT pointer (" + std::to_string(controller->targetX()) + "," + std::to_string(controller->targetY()) +
            "). May pick/place,\nreload or stack; normal TU.\nNo promised transfer. Confirm?");
        // Marker is the requested pointer, never an inferred placement preview.
        // Leave its center transparent and retain the ordinary live inventory.
        _pointer->clear(); _pointer->setX(controller->targetX() - 8); _pointer->setY(controller->targetY() - 8);
        const SDL_Color *palette = controller->palette();
        int light = 1, dark = 1;
        for (int i = 2; i < 256; ++i)
        {
            const int value = palette[i].r * 299 + palette[i].g * 587 + palette[i].b * 114;
            if (value > palette[light].r * 299 + palette[light].g * 587 + palette[light].b * 114) light = i;
            if (value < palette[dark].r * 299 + palette[dark].g * 587 + palette[dark].b * 114) dark = i;
        }
        for (int dy = -6; dy <= 6; ++dy) for (int dx = -6; dx <= 6; ++dx)
        {
            const int squared = dx * dx + dy * dy;
            if (squared >= 25 && squared <= 36) _pointer->setPixel(dx + 8, dy + 8, squared <= 30 ? light : dark);
        }
    }
#endif
    _frame->setY(bannerY); _text->setY(bannerY + 1);
    _yes->setY(bannerY + 1); _no->setY(bannerY + 17);
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
bool InventoryCloseOverlay::active() const
{
    if (!_controller || !turnContextVisible()) return false;
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
    if (_clickMode)
    {
        const int y =
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD
            _controller->inventoryUnloadRequest() ? _owner->inventoryUnloadBannerY() :
#endif
            _owner->inventoryClickBannerY();
        const auto shownAt = [](const Surface *s, int x, int y, int w, int h) {
            return s->turnContextVisible() && s->getX() == x && s->getY() == y && s->getWidth() == w && s->getHeight() == h;
        };
        return shownAt(this, 0, 0, 320, 200) && shownAt(_frame, 0, y, 320, 32) &&
            shownAt(_text, 4, y + 1, 236, 30) && shownAt(_yes, 242, y + 1, 76, 14) &&
            shownAt(_no, 242, y + 17, 76, 14) &&
            (_controller->inventoryUnloadRequest() ||
                shownAt(_pointer, _controller->targetX() - 8, _controller->targetY() - 8, 17, 17));
    }
#endif
    return true;
}
void InventoryCloseOverlay::blit(Surface *surface)
{
    if (!active()) return;
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
    // State already drew the actual inventory, held sprite and animation.
    // Only the two disjoint-band controls and transparent pointer are new.
    if (_clickMode) { if (!_controller->inventoryUnloadRequest()) _pointer->blit(surface); }
    else
#endif
        Surface::blit(surface);
    _frame->blit(surface); _text->blit(surface); _yes->blit(surface); _no->blit(surface);
}
int InventoryCloseOverlay::choice(Action *action) const
{
    const double x = action->getAbsoluteXMouse(), y = action->getAbsoluteYMouse();
    if (x >= _yes->getX() && x < _yes->getX() + _yes->getWidth() &&
        y >= _yes->getY() && y < _yes->getY() + _yes->getHeight()) return 1;
    if (x >= _no->getX() && x < _no->getX() + _no->getWidth() &&
        y >= _no->getY() && y < _no->getY() + _no->getHeight()) return 2;
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
