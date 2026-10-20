/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Inventory.h"
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE
#include "../../tests/inventory-close-control/InventoryCloseControlGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
#include "../../tests/inventory-click-control/InventoryClickControlGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
#include "../../tests/inventory-right-click-control/InventoryRightClickControlGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
#include "../../tests/inventory-unload-control/InventoryUnloadControlGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
#include "../../tests/inventory-ground-control/InventoryGroundControlGate.h"
#endif
#include <algorithm>
#include <cmath>
#include "../Mod/Mod.h"
#include "../Mod/RuleInventory.h"
#include "../Mod/RuleInterface.h"
#include "../Engine/Game.h"
#include "../Engine/Timer.h"
#include "../Interface/Text.h"
#include "../Interface/NumberText.h"
#include "../Engine/Font.h"
#include "../Engine/Language.h"
#include "../Engine/Options.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Engine/SurfaceSet.h"
#include "../Savegame/BattleItem.h"
#include "../Mod/RuleItem.h"
#include "../Savegame/BattleUnit.h"
#include "../Engine/Action.h"
#include "../Engine/Sound.h"
#include "WarningMessage.h"
#include "../Savegame/Tile.h"
#include "PrimeGrenadeState.h"
#include "../Engine/Screen.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentInventoryPaint.h"
#endif

namespace OpenXcom
{

/**
 * Sets up an inventory with the specified size and position.
 * @param game Pointer to core game.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 * @param base Is the inventory being called from the basescape?
 */
Inventory::Inventory(Game *game, int width, int height, int x, int y, bool base) : InteractiveSurface(width, height, x, y), _game(game), _selUnit(0), _selItem(0), _tu(true), _base(base), _mouseOverItem(0), _groundOffset(0), _animFrame(0)
{
	_depth = _game->getSavedGame()->getSavedBattle()->getDepth();
	_grid = new Surface(width, height, 0, 0);
	_items = new Surface(width, height, 0, 0);
	_selection = new Surface(RuleInventory::HAND_W * RuleInventory::SLOT_W, RuleInventory::HAND_H * RuleInventory::SLOT_H, x, y);
	_warning = new WarningMessage(224, 24, 48, 176);
	_stackNumber = new NumberText(15, 15, 0, 0);
	_stackNumber->setBordered(true);

	_warning->initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());
	_warning->setColor(_game->getMod()->getInterface("battlescape")->getElement("warning")->color2);
	_warning->setTextColor(_game->getMod()->getInterface("battlescape")->getElement("warning")->color);

	_animTimer = new Timer(125);
	_animTimer->onTimer((SurfaceHandler)&Inventory::drawPrimers);
	_animTimer->start();
}

/**
 * Deletes inventory surfaces.
 */
Inventory::~Inventory()
{
	delete _grid;
	delete _items;
	delete _selection;
	delete _warning;
	delete _stackNumber;
	delete _animTimer;
}

/**
 * Replaces a certain amount of colors in the inventory's palette.
 * @param colors Pointer to the set of colors.
 * @param firstcolor Offset of the first color to replace.
 * @param ncolors Amount of colors to replace.
 */
void Inventory::setPalette(SDL_Color *colors, int firstcolor, int ncolors)
{
	Surface::setPalette(colors, firstcolor, ncolors);
	_grid->setPalette(colors, firstcolor, ncolors);
	_items->setPalette(colors, firstcolor, ncolors);
	_selection->setPalette(colors, firstcolor, ncolors);
	_warning->setPalette(colors, firstcolor, ncolors);
	_stackNumber->setPalette(getPalette());
}

/**
 * Changes the inventory's Time Units mode.
 * When True, inventory actions cost soldier time units (for battle).
 * When False, inventory actions don't cost anything (for pre-equip).
 * @param tu Time Units mode.
 */
void Inventory::setTuMode(bool tu)
{
	_tu = tu;
}

/**
 * Changes the unit to display the inventory of.
 * @param unit Pointer to battle unit.
 */
void Inventory::setSelectedUnit(BattleUnit *unit)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentInventoryRevision;
#endif
	_selUnit = unit;
	_groundOffset = 999;
	arrangeGround();
}

/**
 * Draws the inventory elements.
 */
void Inventory::draw()
{
	drawGrid();
	drawItems();
}

/**
 * Draws the inventory grid for item placement.
 */
void Inventory::drawGrid()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentGridReady = false; _agentGridUnambiguous = true; _agentTargets.clear(); _agentFrameReady = false;
#endif
	_grid->clear();
	Text text = Text(80, 9, 0, 0);
	text.setPalette(_grid->getPalette());
	text.initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());

	RuleInterface *rule = _game->getMod()->getInterface("inventory");

	text.setColor(rule->getElement("textSlots")->color);
	text.setHighContrast(true);

	Uint8 color = rule->getElement("grid")->color;

	for (std::map<std::string, RuleInventory*>::iterator i = _game->getMod()->getInventories()->begin(); i != _game->getMod()->getInventories()->end(); ++i)
	{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        const bool admitted = ((i->first == "STR_RIGHT_HAND" && i->second->getX() == 0) ||
            (i->first == "STR_LEFT_HAND" && i->second->getX() == 128)) &&
            i->second->getType() == INV_HAND && i->second->getY() == 63;
        const bool ground = i->first == "STR_GROUND" && i->second->getType() == INV_GROUND &&
            i->second->getX() == 0 && i->second->getY() == 152;
        const auto blocks = [&](int x, int y, int w, int h) {
            if ((x < 32 && x + w > 0 && y < 111 && y + h > 63) ||
                (x < 160 && x + w > 128 && y < 111 && y + h > 63) ||
                (x < 320 && x + w > 0 && y < 200 && y + h > 152)) _agentGridUnambiguous = false;
        };
        if (!admitted && !ground)
        {
            if (i->second->getType() == INV_HAND) blocks(i->second->getX(), i->second->getY(), 32, 48);
            else if (i->second->getType() == INV_GROUND) blocks(i->second->getX(), i->second->getY(), 320-i->second->getX(), 200-i->second->getY());
            else for (const RuleSlot &slot : *i->second->getSlots())
                blocks(i->second->getX()+16*slot.x, i->second->getY()+16*slot.y, 16, 16);
        }
#endif
		// Draw grid
		if (i->second->getType() == INV_SLOT)
		{
			for (std::vector<RuleSlot>::iterator j = i->second->getSlots()->begin(); j != i->second->getSlots()->end(); ++j)
			{
				SDL_Rect r;
				r.x = i->second->getX() + RuleInventory::SLOT_W * j->x;
				r.y = i->second->getY() + RuleInventory::SLOT_H * j->y;
				r.w = RuleInventory::SLOT_W + 1;
				r.h = RuleInventory::SLOT_H + 1;
				_grid->drawRect(&r, color);
				r.x++;
				r.y++;
				r.w -= 2;
				r.h -= 2;
				_grid->drawRect(&r, 0);
			}
		}
		else if (i->second->getType() == INV_HAND)
		{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            if (i->first == "STR_RIGHT_HAND" || i->first == "STR_LEFT_HAND")
                agentCaptureTarget(i->second->getX(), i->second->getY(),
                    RuleInventory::HAND_W * RuleInventory::SLOT_W,
                    RuleInventory::HAND_H * RuleInventory::SLOT_H, "hand");
#endif
			SDL_Rect r;
			r.x = i->second->getX();
			r.y = i->second->getY();
			r.w = RuleInventory::HAND_W * RuleInventory::SLOT_W;
			r.h = RuleInventory::HAND_H * RuleInventory::SLOT_H;
			_grid->drawRect(&r, color);
			r.x++;
			r.y++;
			r.w -= 2;
			r.h -= 2;
			_grid->drawRect(&r, 0);
		}
		else if (i->second->getType() == INV_GROUND)
		{
			for (int x = i->second->getX(); x <= 320; x += RuleInventory::SLOT_W)
			{
				for (int y = i->second->getY(); y <= 200; y += RuleInventory::SLOT_H)
				{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
                    if (i->first == "STR_GROUND" && x + RuleInventory::SLOT_W < getWidth() && y + RuleInventory::SLOT_H < getHeight())
                        agentCaptureTarget(x, y, RuleInventory::SLOT_W, RuleInventory::SLOT_H, "ground");
#endif
					SDL_Rect r;
					r.x = x;
					r.y = y;
					r.w = RuleInventory::SLOT_W + 1;
					r.h = RuleInventory::SLOT_H + 1;
					_grid->drawRect(&r, color);
					r.x++;
					r.y++;
					r.w -= 2;
					r.h -= 2;
					_grid->drawRect(&r, 0);
				}
			}
		}

		// Draw label
		text.setX(i->second->getX());
		text.setY(i->second->getY() - text.getFont()->getHeight() - text.getFont()->getSpacing());
		text.setText(_game->getLanguage()->getString(i->second->getId()));
		text.blit(_grid);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        const std::string label = Observer::agentTextJson(&text);
        SDL_Rect ink;
        if (label != "null" && text.agentPaintedBounds(ink))
            for (AgentTarget &target : _agentTargets)
                if ((target.area == "hand" && target.rect.x == i->second->getX() && target.rect.y == i->second->getY()) ||
                    (target.area == "ground" && i->first == "STR_GROUND"))
                {
                    target.label = label; target.labelInk = {ink.x, ink.y, ink.w, ink.h};
                    for (int y = 0; y < text.getHeight(); ++y) for (int x = 0; x < text.getWidth(); ++x)
                        if (Observer::agentInventoryPixel(&text, x, y))
                            target.labelPixels.push_back(std::make_pair((y + text.getY()) * 320 + x + text.getX(), Observer::agentInventoryPixel(&text, x, y)));
                }
#endif
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        for (AgentTarget &target : _agentTargets)
        for (const auto &pixel : target.labelPixels)
            if (pixel.first < 0 || pixel.first >= 320 * 200 ||
                Observer::agentInventoryPixel(_grid, pixel.first % 320, pixel.first / 320) != pixel.second)
            { target.label = "null"; break; }
    _agentGridReady = _agentGridUnambiguous && Observer::agentPaintHealthy() && Observer::agentInventoryLayer(_grid);
#endif
}

/**
 * Draws the items contained in the soldier's inventory.
 */
void Inventory::drawItems()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentFrameReady = false;
#endif
	_items->clear();
	_grenadeIndicators.clear();
	Uint8 color = _game->getMod()->getInterface("inventory")->getElement("numStack")->color;
	if (_selUnit != 0)
	{
		SurfaceSet *texture = _game->getMod()->getSurfaceSet("BIGOBS.PCK");
		// Soldier items
		for (std::vector<BattleItem*>::iterator i = _selUnit->getInventory()->begin(); i != _selUnit->getInventory()->end(); ++i)
		{
			if ((*i) == _selItem)
				continue;

			Surface *frame = texture->getFrame((*i)->getRules()->getBigSprite());
			if ((*i)->getSlot()->getType() == INV_SLOT)
			{
				frame->setX((*i)->getSlot()->getX() + (*i)->getSlotX() * RuleInventory::SLOT_W);
				frame->setY((*i)->getSlot()->getY() + (*i)->getSlotY() * RuleInventory::SLOT_H);
			}
			else if ((*i)->getSlot()->getType() == INV_HAND)
			{
				frame->setX((*i)->getSlot()->getX() + (RuleInventory::HAND_W - (*i)->getRules()->getInventoryWidth()) * RuleInventory::SLOT_W/2);
				frame->setY((*i)->getSlot()->getY() + (RuleInventory::HAND_H - (*i)->getRules()->getInventoryHeight()) * RuleInventory::SLOT_H/2);
			}
			texture->getFrame((*i)->getRules()->getBigSprite())->blit(_items);

			// grenade primer indicators
			if ((*i)->getFuseTimer() >= 0)
			{
				_grenadeIndicators.push_back(std::make_pair(frame->getX(), frame->getY()));
			}
		}
		Surface *stackLayer = new Surface(getWidth(), getHeight(), 0, 0);
		stackLayer->setPalette(getPalette());
		// Ground items
		for (std::vector<BattleItem*>::iterator i = _selUnit->getTile()->getInventory()->begin(); i != _selUnit->getTile()->getInventory()->end(); ++i)
		{
			Surface *frame = texture->getFrame((*i)->getRules()->getBigSprite());
			// note that you can make items invisible by setting their width or height to 0 (for example used with tank corpse items)
			if ((*i) == _selItem || (*i)->getSlotX() < _groundOffset || (*i)->getRules()->getInventoryHeight() == 0 || (*i)->getRules()->getInventoryWidth() == 0 || !frame)
				continue;
			frame->setX((*i)->getSlot()->getX() + ((*i)->getSlotX() - _groundOffset) * RuleInventory::SLOT_W);
			frame->setY((*i)->getSlot()->getY() + (*i)->getSlotY() * RuleInventory::SLOT_H);
			texture->getFrame((*i)->getRules()->getBigSprite())->blit(_items);

			// grenade primer indicators
			if ((*i)->getFuseTimer() >= 0)
			{
				_grenadeIndicators.push_back(std::make_pair(frame->getX(), frame->getY()));
			}

			// item stacking
			if (_stackLevel[(*i)->getSlotX()][(*i)->getSlotY()] > 1)
			{
				_stackNumber->setX(((*i)->getSlot()->getX() + (((*i)->getSlotX() + (*i)->getRules()->getInventoryWidth()) - _groundOffset) * RuleInventory::SLOT_W)-4);
				if (_stackLevel[(*i)->getSlotX()][(*i)->getSlotY()] > 9)
				{
					_stackNumber->setX(_stackNumber->getX()-4);
				}
				_stackNumber->setY(((*i)->getSlot()->getY() + ((*i)->getSlotY() + (*i)->getRules()->getInventoryHeight()) * RuleInventory::SLOT_H)-6);
				_stackNumber->setValue(_stackLevel[(*i)->getSlotX()][(*i)->getSlotY()]);
				_stackNumber->draw();
				_stackNumber->setColor(color);
				_stackNumber->blit(stackLayer);
			}
		}

		stackLayer->blit(_items);
		delete stackLayer;
	}
}

/**
 * Moves an item to a specified slot in the
 * selected player's inventory.
 * @param item Pointer to battle item.
 * @param slot Inventory slot, or NULL if none.
 * @param x X position in slot.
 * @param y Y position in slot.
 */
void Inventory::moveItem(BattleItem *item, RuleInventory *slot, int x, int y)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentInventoryRevision;
#endif
	// Make items vanish (eg. ammo in weapons)
	if (slot == 0)
	{
		if (item->getSlot()->getType() == INV_GROUND)
		{
			_selUnit->getTile()->removeItem(item);
		}
		else
		{
			item->moveToOwner(0);
		}
	}
	else
	{
		// Handle dropping from/to ground.
		if (slot != item->getSlot())
		{
			if (slot->getType() == INV_GROUND)
			{
				item->moveToOwner(0);
				_selUnit->getTile()->addItem(item, item->getSlot());
				if (item->getUnit() && item->getUnit()->getStatus() == STATUS_UNCONSCIOUS)
				{
					item->getUnit()->setPosition(_selUnit->getPosition());
				}
			}
			else if (item->getSlot() == 0 || item->getSlot()->getType() == INV_GROUND)
			{
				item->moveToOwner(_selUnit);
				_selUnit->getTile()->removeItem(item);
				item->setTurnFlag(false);
				if (item->getUnit() && item->getUnit()->getStatus() == STATUS_UNCONSCIOUS)
				{
					item->getUnit()->setPosition(Position(-1,-1,-1));
				}
			}
		}
		item->setSlot(slot);
		item->setSlotX(x);
		item->setSlotY(y);
	}
}

/**
 * Checks if an item in a certain slot position would
 * overlap with any other inventory item.
 * @param unit Pointer to current unit.
 * @param item Pointer to battle item.
 * @param slot Inventory slot, or NULL if none.
 * @param x X position in slot.
 * @param y Y position in slot.
 * @return If there's overlap.
 */
bool Inventory::overlapItems(BattleUnit *unit, BattleItem *item, RuleInventory *slot, int x, int y)
{
	if (slot->getType() != INV_GROUND)
	{
		for (std::vector<BattleItem*>::const_iterator i = unit->getInventory()->begin(); i != unit->getInventory()->end(); ++i)
		{
			if ((*i)->getSlot() == slot && (*i)->occupiesSlot(x, y, item))
			{
				return true;
			}
		}
	}
	else if (unit->getTile() != 0)
	{
		for (std::vector<BattleItem*>::const_iterator i = unit->getTile()->getInventory()->begin(); i != unit->getTile()->getInventory()->end(); ++i)
		{
			if ((*i)->occupiesSlot(x, y, item))
			{
				return true;
			}
		}
	}
	return false;
}

/**
 * Gets the inventory slot located in the specified mouse position.
 * @param x Mouse X position. Returns the slot's X position.
 * @param y Mouse Y position. Returns the slot's Y position.
 * @return Slot rules, or NULL if none.
 */
RuleInventory *Inventory::getSlotInPosition(int *x, int *y) const
{
	for (std::map<std::string, RuleInventory*>::iterator i = _game->getMod()->getInventories()->begin(); i != _game->getMod()->getInventories()->end(); ++i)
	{
		if (i->second->checkSlotInPosition(x, y))
		{
			return i->second;
		}
	}
	return 0;
}

/**
 * Returns the item currently grabbed by the player.
 * @return Pointer to selected item, or NULL if none.
 */
BattleItem *Inventory::getSelectedItem() const
{
	return _selItem;
}

/**
 * Changes the item currently grabbed by the player.
 * @param item Pointer to selected item, or NULL if none.
 */
void Inventory::setSelectedItem(BattleItem *item)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentInventoryRevision;
#endif
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
    BattleItem *previous = _selItem;
#endif
	_selItem = (item && !item->getRules()->isFixed()) ? item : 0;
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
    if (_selItem != previous) ++_heldSelectionRevision;
#endif
	if (_selItem == 0)
	{
		_selection->clear();
	}
	else
	{
		if (_selItem->getSlot()->getType() == INV_GROUND)
		{
			_stackLevel[_selItem->getSlotX()][_selItem->getSlotY()] -= 1;
		}
		_selItem->getRules()->drawHandSprite(_game->getMod()->getSurfaceSet("BIGOBS.PCK"), _selection);
	}
	drawItems();
}

/**
 * Returns the item currently under mouse cursor.
 * @return Pointer to selected item, or 0 if none.
 */
BattleItem *Inventory::getMouseOverItem() const
{
	return _mouseOverItem;
}

/**
 * Changes the item currently under mouse cursor.
 * @param item Pointer to selected item, or NULL if none.
 */
void Inventory::setMouseOverItem(BattleItem *item)
{
	_mouseOverItem = (item && !item->getRules()->isFixed()) ? item : 0;
}

/**
 * Handles timers.
 */
void Inventory::think()
{
	_warning->think();
	_animTimer->think(0,this);
}

/**
 * Blits the inventory elements.
 * @param surface Pointer to surface to blit onto.
 */
void Inventory::blit(Surface *surface)
{
	clear();
	_grid->blit(this);
	_items->blit(this);
	_selection->blit(this);
	_warning->blit(this);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentCaptureItems(surface);
#endif
	Surface::blit(surface);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentFrameReady = Observer::agentPaintHealthy() && Observer::agentInventoryLayer(this) &&
        Observer::agentInventoryLayer(_grid) && Observer::agentInventoryLayer(_items) &&
        Observer::agentInventoryLayer(_selection);
    _agentShownRevision = _agentInventoryRevision;
#endif
}

/**
 * Moves the selected item.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
#if defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK) || defined(OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD)
bool Inventory::inventoryClickPresentationAllowed() const
{
    if (!turnContextVisible() || getX() != 0 || getY() != 0 || getWidth() != 320 || getHeight() != 200 ||
        !_grid->turnContextVisible() || !_items->turnContextVisible() || !_selection->turnContextVisible() ||
        _grid->getX() != 0 || _grid->getY() != 0 || _grid->getWidth() != 320 || _grid->getHeight() != 200 ||
        _items->getX() != 0 || _items->getY() != 0 || _items->getWidth() != 320 || _items->getHeight() != 200 ||
        _selection->getWidth() != 32 || _selection->getHeight() != 48 || _warning->inventoryClickOccluding()) return false;
    // Validate only the geometry that is drawn by the ordinary inventory.
    // Never query items, fit/eligibility, costs, page offsets or desired slots.
    const char *names[] = {"STR_RIGHT_HAND", "STR_LEFT_HAND", "STR_GROUND"};
    const int xs[] = {0, 128, 0}, ys[] = {63, 63, 152};
    for (int i = 0; i < 3; ++i)
    {
        const RuleInventory *rule = _game->getMod()->getInventory(names[i], false);
        if (!rule || rule->getX() != xs[i] || rule->getY() != ys[i] ||
            rule->getType() != (i == 2 ? INV_GROUND : INV_HAND)) return false;
    }
    const auto intersectsDomain = [](int x, int y, int w, int h) {
        return (x < 32 && x + w > 0 && y < 111 && y + h > 63) ||
            (x < 160 && x + w > 128 && y < 111 && y + h > 63) ||
            (x < 320 && x + w > 0 && y < 200 && y + h > 152);
    };
    for (const auto &entry : *_game->getMod()->getInventories())
    {
        if (entry.first == names[0] || entry.first == names[1] || entry.first == names[2]) continue;
        RuleInventory *rule = entry.second;
        if (!rule) return false;
        // These are exactly the rectangles rendered by drawGrid, independent
        // of any item. A modded extra region must not intercept a stock pointer.
        if (rule->getType() == INV_HAND)
        {
            if (intersectsDomain(rule->getX(), rule->getY(), 32, 48)) return false;
        }
        else if (rule->getType() == INV_GROUND)
        {
            if (intersectsDomain(rule->getX(), rule->getY(), 320 - rule->getX(), 200 - rule->getY())) return false;
        }
        else for (const RuleSlot &slot : *rule->getSlots())
            if (intersectsDomain(rule->getX() + slot.x * 16, rule->getY() + slot.y * 16, 16, 16)) return false;
    }
    return true;
}
int Inventory::inventoryClickBannerY() const
{
    // Use the actual presented selection surface, even when it is empty.
    // Two separated bands cannot both overlap the ordinary 48px-high sprite.
    const int top = _selection->getY(), bottom = top + _selection->getHeight();
    return top < 144 && bottom > 112 ? 24 : 112;
}
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD
bool Inventory::inventoryUnloadPresentationAllowed() const
{
    if (!inventoryClickPresentationAllowed() || isButtonPressed()) return false;
    const auto intersectsUnload = [](int x, int y, int w, int h) {
        return x < 320 && x + w > 288 && y < 57 && y + h > 32;
    };
    // Only visible slot geometry is checked, never a selected item's dimensions
    // or the preliminary click's eligibility. Arbitrary modded items are outside
    // this narrow stock-layout pilot; the full ordinary click remains intact.
    for (const auto &entry : *_game->getMod()->getInventories())
    {
        RuleInventory *rule = entry.second;
        if (!rule) return false;
        if (rule->getType() == INV_HAND)
        {
            if (intersectsUnload(rule->getX(), rule->getY(), 32, 48)) return false;
        }
        else if (rule->getType() == INV_GROUND)
        {
            if (intersectsUnload(rule->getX(), rule->getY(), 320 - rule->getX(), 200 - rule->getY())) return false;
        }
        else for (const RuleSlot &slot : *rule->getSlots())
            if (intersectsUnload(rule->getX() + slot.x * 16, rule->getY() + slot.y * 16, 16, 16)) return false;
    }
    return true;
}
#endif
void Inventory::mouseOver(Action *action, State *state)
{
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    InventoryClickControlGate::inventoryMouseOverEntered(this, action, state);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    InventoryRightClickControlGate::inventoryMouseOverEntered(this, action, state);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::inventoryMouseOverEntered(this, action, state);
#endif
	if (movePointer((int)floor(action->getAbsoluteXMouse()) - getX(),
        (int)floor(action->getAbsoluteYMouse()) - getY()))
        InteractiveSurface::mouseOver(action, state);
}

// Common normal pointer path. Semantic targets supply the same local coordinates;
// this never fabricates SDL input or changes the physical pointer.
bool Inventory::movePointer(int localX, int localY)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentInventoryRevision;
#endif
	_selection->setX(localX - _selection->getWidth()/2);
	_selection->setY(localY - _selection->getHeight()/2);
	if (_selUnit == 0)
		return false;

	int x = localX,
		y = localY;
	RuleInventory *slot = getSlotInPosition(&x, &y);
	if (slot != 0)
	{
		if (slot->getType() == INV_GROUND)
		{
			x += _groundOffset;
		}
		BattleItem *item = _selUnit->getItem(slot, x, y);
		setMouseOverItem(item);
	}
	else
	{
		setMouseOverItem(0);
	}

	_selection->setX(localX - _selection->getWidth()/2);
	_selection->setY(localY - _selection->getHeight()/2);
	return true;
}

/**
 * Picks up / drops an item.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Inventory::mouseClick(Action *action, State *state)
{
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    InventoryClickControlGate::inventoryClickEntered(this, action, state);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    InventoryRightClickControlGate::inventoryClickEntered(this, action, state);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::inventoryClickEntered(this, action, state);
#endif
	if (activateButton(action->getDetails()->button.button,
        (int)floor(action->getAbsoluteXMouse()) - getX(),
        (int)floor(action->getAbsoluteYMouse()) - getY(), (SDL_GetModState() & KMOD_CTRL) != 0))
        InteractiveSurface::mouseClick(action, state);
}

// The existing primary/secondary behavior lives here unchanged. Agent dispatch
// calls only primary; its explicit cancel uses the held-only return branch.
bool Inventory::activateButton(Uint8 button, int localX, int localY, bool control)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentInventoryRevision;
#endif
	if (button == SDL_BUTTON_LEFT)
	{
		if (_selUnit == 0)
			return false;
		// Pickup item
		if (_selItem == 0)
		{
			int x = localX,
				y = localY;
			RuleInventory *slot = getSlotInPosition(&x, &y);
			if (slot != 0)
			{
				if (slot->getType() == INV_GROUND)
				{
					x += _groundOffset;
				}
				BattleItem *item = _selUnit->getItem(slot, x, y);
				if (item != 0 && !item->getRules()->isFixed())
				{
					if (control)
					{
						RuleInventory *newSlot = _game->getMod()->getInventory("STR_GROUND", true);
						std::string warning = "STR_NOT_ENOUGH_SPACE";
						bool placed = false;

						if (slot->getType() == INV_GROUND)
						{
							switch (item->getRules()->getBattleType())
							{
							case BT_FIREARM:
								newSlot = _game->getMod()->getInventory("STR_RIGHT_HAND", true);
								break;
							case BT_MINDPROBE:
							case BT_PSIAMP:
							case BT_MELEE:
							case BT_CORPSE:
								newSlot = _game->getMod()->getInventory("STR_LEFT_HAND", true);
								break;
							default:
								if (item->getRules()->getInventoryHeight() > 2)
								{
									newSlot = _game->getMod()->getInventory("STR_BACK_PACK", true);
								}
								else
								{
									newSlot = _game->getMod()->getInventory("STR_BELT", true);
								}
								break;
							}
						}

						if (newSlot->getType() != INV_GROUND)
						{
							_stackLevel[item->getSlotX()][item->getSlotY()] -= 1;

							placed = fitItem(newSlot, item, warning);

							if (!placed)
							{
								for (std::map<std::string, RuleInventory *>::const_iterator wildCard = _game->getMod()->getInventories()->begin(); wildCard != _game->getMod()->getInventories()->end() && !placed; ++wildCard)
								{
									newSlot = wildCard->second;
									if (newSlot->getType() == INV_GROUND)
									{
										continue;
									}
									placed = fitItem(newSlot, item, warning);
								}
							}
							if (!placed)
							{
								_stackLevel[item->getSlotX()][item->getSlotY()] += 1;
							}
						}
						else
						{
							if (!_tu || _selUnit->spendTimeUnits(item->getSlot()->getCost(newSlot)))
							{
								placed = true;
								moveItem(item, newSlot, 0, 0);
								_game->getMod()->getSoundByDepth(_depth, Mod::ITEM_DROP)->play();
								arrangeGround(false);
							}
							else
							{
								warning = "STR_NOT_ENOUGH_TIME_UNITS";
							}
						}

						if (!placed)
						{
							_warning->showMessage(_game->getLanguage()->getString(warning));
						}
					}
					else
					{
						setSelectedItem(item);
						if (item->getFuseTimer() >= 0)
						{
							_warning->showMessage(_game->getLanguage()->getString("STR_GRENADE_IS_ACTIVATED"));
						}
					}
				}
			}
		}
		// Drop item
		else
		{
			int x = _selection->getX() + (RuleInventory::HAND_W - _selItem->getRules()->getInventoryWidth()) * RuleInventory::SLOT_W/2 + RuleInventory::SLOT_W/2,
				y = _selection->getY() + (RuleInventory::HAND_H - _selItem->getRules()->getInventoryHeight()) * RuleInventory::SLOT_H/2 + RuleInventory::SLOT_H/2;
			RuleInventory *slot = getSlotInPosition(&x, &y);
			if (slot != 0)
			{
				if (slot->getType() == INV_GROUND)
				{
					x += _groundOffset;
				}
				BattleItem *item = _selUnit->getItem(slot, x, y);

				bool canStack = slot->getType() == INV_GROUND && canBeStacked(item, _selItem);

				// Put item in empty slot, or stack it, if possible.
				if (item == 0 || item == _selItem || canStack)
				{
					if (!overlapItems(_selUnit, _selItem, slot, x, y) && slot->fitItemInSlot(_selItem->getRules(), x, y))
					{
						if (!_tu || _selUnit->spendTimeUnits(_selItem->getSlot()->getCost(slot)))
						{
							moveItem(_selItem, slot, x, y);
							if (slot->getType() == INV_GROUND)
							{
								_stackLevel[x][y] += 1;
							}
							setSelectedItem(0);
							_game->getMod()->getSoundByDepth(_depth, Mod::ITEM_DROP)->play();
						}
						else
						{
							_warning->showMessage(_game->getLanguage()->getString("STR_NOT_ENOUGH_TIME_UNITS"));
						}
					}
					else if (canStack)
					{
						if (!_tu || _selUnit->spendTimeUnits(_selItem->getSlot()->getCost(slot)))
						{
							moveItem(_selItem, slot, item->getSlotX(), item->getSlotY());
							_stackLevel[item->getSlotX()][item->getSlotY()] += 1;
							setSelectedItem(0);
							_game->getMod()->getSoundByDepth(_depth, Mod::ITEM_DROP)->play();
						}
						else
						{
							_warning->showMessage(_game->getLanguage()->getString("STR_NOT_ENOUGH_TIME_UNITS"));
						}
					}
				}
				// Put item in weapon
				else if (!item->getRules()->getCompatibleAmmo()->empty())
				{
					bool wrong = true;
					for (std::vector<std::string>::iterator i = item->getRules()->getCompatibleAmmo()->begin(); i != item->getRules()->getCompatibleAmmo()->end(); ++i)
					{
						if ((*i) == _selItem->getRules()->getType())
						{
							wrong = false;
							break;
						}
					}
					if (wrong)
					{
						_warning->showMessage(_game->getLanguage()->getString("STR_WRONG_AMMUNITION_FOR_THIS_WEAPON"));
					}
					else
					{
						if (item->getAmmoItem() != 0)
						{
							_warning->showMessage(_game->getLanguage()->getString("STR_WEAPON_IS_ALREADY_LOADED"));
						}
						else if (!_tu || _selUnit->spendTimeUnits(15))
						{
							moveItem(_selItem, 0, 0, 0);
							item->setAmmoItem(_selItem);
							_selItem->moveToOwner(0);
							setSelectedItem(0);
							_game->getMod()->getSoundByDepth(_depth, Mod::ITEM_RELOAD)->play();
							if (item->getSlot()->getType() == INV_GROUND)
							{
								arrangeGround(false);
							}
						}
						else
						{
							_warning->showMessage(_game->getLanguage()->getString("STR_NOT_ENOUGH_TIME_UNITS"));
						}
					}
				}
				// else swap the item positions?
			}
			else
			{
				// try again, using the position of the mouse cursor, not the item (slightly more intuitive for stacking)
				x = localX;
				y = localY;
				slot = getSlotInPosition(&x, &y);
				if (slot != 0 && slot->getType() == INV_GROUND)
				{
					x += _groundOffset;
					BattleItem *item = _selUnit->getItem(slot, x, y);
					if (canBeStacked(item, _selItem))
					{
						if (!_tu || _selUnit->spendTimeUnits(_selItem->getSlot()->getCost(slot)))
						{
							moveItem(_selItem, slot, item->getSlotX(), item->getSlotY());
							_stackLevel[item->getSlotX()][item->getSlotY()] += 1;
							setSelectedItem(0);
							_game->getMod()->getSoundByDepth(_depth, Mod::ITEM_DROP)->play();
						}
						else
						{
							_warning->showMessage(_game->getLanguage()->getString("STR_NOT_ENOUGH_TIME_UNITS"));
						}
					}
				}
			}
		}
	}
	else if (button == SDL_BUTTON_RIGHT)
	{
		if (_selItem == 0)
		{
			if (!_base || Options::includePrimeStateInSavedLayout)
			{
				if (!_tu)
				{
					int x = localX,
						y = localY;
					RuleInventory *slot = getSlotInPosition(&x, &y);
					if (slot != 0)
					{
						if (slot->getType() == INV_GROUND)
						{
							x += _groundOffset;
						}
						BattleItem *item = _selUnit->getItem(slot, x, y);
						if (item != 0)
						{
							BattleType itemType = item->getRules()->getBattleType();
							if (BT_GRENADE == itemType || BT_PROXIMITYGRENADE == itemType)
							{
								if (item->getFuseTimer() == -1)
								{
									// Prime that grenade!
									if (BT_PROXIMITYGRENADE == itemType)
									{
										_warning->showMessage(_game->getLanguage()->getString("STR_GRENADE_IS_ACTIVATED"));
										item->setFuseTimer(0);
										arrangeGround(false);
									}
									else _game->pushState(new PrimeGrenadeState(0, true, item));
								}
								else
								{
									_warning->showMessage(_game->getLanguage()->getString("STR_GRENADE_IS_DEACTIVATED"));
									item->setFuseTimer(-1);  // Unprime the grenade
									arrangeGround(false);
								}
							}
						}
					}
				}
				else
				{
					_game->popState(); // Closes the inventory window on right-click (if not in preBattle equip screen!)
				}
			}
		}
		else
		{
			returnHeldItem();
		}
	}
	return true;
}

bool Inventory::returnHeldItem()
{
    if (!_selItem) return false;
    if (_selItem->getSlot()->getType() == INV_GROUND)
        _stackLevel[_selItem->getSlotX()][_selItem->getSlotY()] += 1;
    setSelectedItem(0);
    return true;
}

/**
 * Unloads the selected weapon, placing the gun
 * on the right hand and the ammo on the left hand.
 * @return The success of the weapon being unloaded.
 */
bool Inventory::unload()
{
	// Must be holding an item
	if (_selItem == 0)
	{
		return false;
	}

	// Item must be loaded
	if (_selItem->getAmmoItem() == 0 && !_selItem->getRules()->getCompatibleAmmo()->empty())
	{
		_warning->showMessage(_game->getLanguage()->getString("STR_NO_AMMUNITION_LOADED"));
	}
	if (_selItem->getAmmoItem() == 0 || !_selItem->needsAmmo())
	{
		return false;
	}

	// Hands must be free
	for (std::vector<BattleItem*>::iterator i = _selUnit->getInventory()->begin(); i != _selUnit->getInventory()->end(); ++i)
	{
		if ((*i)->getSlot()->getType() == INV_HAND && (*i) != _selItem)
		{
			_warning->showMessage(_game->getLanguage()->getString("STR_BOTH_HANDS_MUST_BE_EMPTY"));
			return false;
		}
	}

	if (!_tu || _selUnit->spendTimeUnits(8))
	{
		moveItem(_selItem->getAmmoItem(), _game->getMod()->getInventory("STR_LEFT_HAND", true), 0, 0);
		_selItem->getAmmoItem()->moveToOwner(_selUnit);
		moveItem(_selItem, _game->getMod()->getInventory("STR_RIGHT_HAND", true), 0, 0);
		_selItem->moveToOwner(_selUnit);
		_selItem->setAmmoItem(0);
		setSelectedItem(0);
	}
	else
	{
		_warning->showMessage(_game->getLanguage()->getString("STR_NOT_ENOUGH_TIME_UNITS"));
		return false;
	}

	return true;
}

/**
 * Arranges items on the ground for the inventory display.
 * Since items on the ground aren't assigned to anyone,
 * they don't actually have permanent slot positions.
 * @param alterOffset Whether to alter the ground offset.
 */
void Inventory::arrangeGround(bool alterOffset)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentInventoryRevision;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLOSE_CONTROL_GATE
    InventoryCloseControlGate::inventoryGroundArranged(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE
    InventoryClickControlGate::inventoryGroundArranged(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_RIGHT_CLICK_CONTROL_GATE
    InventoryRightClickControlGate::inventoryGroundArranged(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE
    InventoryUnloadControlGate::inventoryGroundArranged(this);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_INVENTORY_GROUND_CONTROL_GATE
    InventoryGroundControlGate::inventoryGroundArranged(this);
#endif
	RuleInventory *ground = _game->getMod()->getInventory("STR_GROUND", true);

	int slotsX = (Screen::ORIGINAL_WIDTH - ground->getX()) / RuleInventory::SLOT_W;
	int slotsY = (Screen::ORIGINAL_HEIGHT - ground->getY()) / RuleInventory::SLOT_H;
	int x = 0;
	int y = 0;
	bool ok = false;
	int xMax = 0;
	_stackLevel.clear();

	if (_selUnit != 0)
	{
		// first move all items out of the way - a big number in X direction
		for (std::vector<BattleItem*>::iterator i = _selUnit->getTile()->getInventory()->begin(); i != _selUnit->getTile()->getInventory()->end(); ++i)
		{
			(*i)->setSlot(ground);
			(*i)->setSlotX(1000000);
			(*i)->setSlotY(0);
		}

		// now for each item, find the most topleft position that is not occupied and will fit
		for (std::vector<BattleItem*>::iterator i = _selUnit->getTile()->getInventory()->begin(); i != _selUnit->getTile()->getInventory()->end(); ++i)
		{
			x = 0;
			y = 0;
			ok = false;
			while (!ok)
			{
				ok = true; // assume we can put the item here, if one of the following checks fails, we can't.
				for (int xd = 0; xd < (*i)->getRules()->getInventoryWidth() && ok; xd++)
				{
					if ((x + xd) % slotsX < x % slotsX)
					{
						ok = false;
					}
					else
					{
						for (int yd = 0; yd < (*i)->getRules()->getInventoryHeight() && ok; yd++)
						{
							BattleItem *item = _selUnit->getItem(ground, x + xd, y + yd);
							ok = item == 0;
							if (canBeStacked(item, *i))
							{
								ok = true;
							}
						}
					}
				}
				if (ok)
				{
					(*i)->setSlotX(x);
					(*i)->setSlotY(y);
					// only increase the stack level if the item is actually visible.
					if ((*i)->getRules()->getInventoryWidth())
					{
						_stackLevel[x][y] += 1;
					}
					xMax = std::max(xMax, x + (*i)->getRules()->getInventoryWidth());
				}
				else
				{
					y++;
					if (y > slotsY - (*i)->getRules()->getInventoryHeight())
					{
						y = 0;
						x++;
					}
				}
			}
		}
	}
	if (alterOffset)
	{
		if (xMax >= _groundOffset + slotsX)
		{
			_groundOffset += slotsX;
		}
		else
		{
			_groundOffset = 0;
		}
	}
	drawItems();
}

/**
 * Attempts to place the item in the inventory slot.
 * @param newSlot Where to place the item.
 * @param item Item to be placed.
 * @param warning Warning message if item could not be placed.
 * @return True, if the item was successfully placed in the inventory.
 */
bool Inventory::fitItem(RuleInventory *newSlot, BattleItem *item, std::string &warning)
{
	bool placed = false;
	int maxSlotX = 0;
	int maxSlotY = 0;
	for (std::vector<RuleSlot>::iterator j = newSlot->getSlots()->begin(); j != newSlot->getSlots()->end(); ++j)
	{
		if (j->x > maxSlotX) maxSlotX = j->x;
		if (j->y > maxSlotY) maxSlotY = j->y;
	}
	for (int y2 = 0; y2 <= maxSlotY && !placed; ++y2)
	{
		for (int x2 = 0; x2 <= maxSlotX && !placed; ++x2)
		{
			if (!overlapItems(_selUnit, item, newSlot, x2, y2) && newSlot->fitItemInSlot(item->getRules(), x2, y2))
			{
				if (!_tu || _selUnit->spendTimeUnits(item->getSlot()->getCost(newSlot)))
				{
					placed = true;
					moveItem(item, newSlot, x2, y2);
					_game->getMod()->getSoundByDepth(_depth, Mod::ITEM_DROP)->play();
					drawItems();
				}
				else
				{
					warning = "STR_NOT_ENOUGH_TIME_UNITS";
				}
			}
		}
	}
	return placed;
}

/**
 * Checks if two items can be stacked on one another.
 * @param itemA First item.
 * @param itemB Second item.
 * @return True, if the items can be stacked on one another.
 */
bool Inventory::canBeStacked(BattleItem *itemA, BattleItem *itemB)
{
		//both items actually exist
	return (itemA != 0 && itemB != 0 &&
		//both items have the same ruleset
		itemA->getRules() == itemB->getRules() &&
		// either they both have no ammo
		((!itemA->getAmmoItem() && !itemB->getAmmoItem()) ||
		// or they both have ammo
		(itemA->getAmmoItem() && itemB->getAmmoItem() &&
		// and the same ammo type
		itemA->getAmmoItem()->getRules() == itemB->getAmmoItem()->getRules() &&
		// and the same ammo quantity
		itemA->getAmmoItem()->getAmmoQuantity() == itemB->getAmmoItem()->getAmmoQuantity())) &&
		// and neither is set to explode
		itemA->getFuseTimer() == -1 && itemB->getFuseTimer() == -1 &&
		// and neither is a corpse or unconscious unit
		itemA->getUnit() == 0 && itemB->getUnit() == 0 &&
		// and if it's a medkit, it has the same number of charges
		itemA->getPainKillerQuantity() == itemB->getPainKillerQuantity() &&
		itemA->getHealQuantity() == itemB->getHealQuantity() &&
		itemA->getStimulantQuantity() == itemB->getStimulantQuantity());

}

/**
 * Shows a warning message.
 * @param msg The message to show.
 */
void Inventory::showWarning(const std::string &msg)
{
	_warning->showMessage(msg);
}

/**
 * Shows primer warnings on all live grenades.
 */
void Inventory::drawPrimers()
{
	const int Pulsate[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
	if (_animFrame == 8)
	{
		_animFrame = 0;
	}
	Surface *tempSurface = _game->getMod()->getSurfaceSet("SCANG.DAT")->getFrame(6);
	for (std::vector<std::pair<int, int> >::const_iterator i = _grenadeIndicators.begin(); i != _grenadeIndicators.end(); ++i)
	{
		tempSurface->blitNShade(_items, (*i).first, (*i).second, Pulsate[_animFrame]);
	}
	_animFrame++;
}

}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace OpenXcom {
void Inventory::agentCaptureTarget(int x, int y, int width, int height, const std::string &area)
{
    // Initial scope is the ordinary stock two-hand/current-ground layout.
    // Names select compiled supported controls here, never enter a DTO.
    if (_agentTargets.size() >= 64 || x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x + width > getWidth() || y + height > getHeight()) return;
    if (area == "hand" && !((x == 0 || x == 128) && y == 63 && width == 32 && height == 48)) return;
    if (area == "ground" && (y < 152 || (y - 152) % 16 || x % 16 || width != 16 || height != 16)) return;
    AgentTarget target; target.rect = {x,y,width,height}; target.area = area;
    _agentTargets.push_back(target);
}
bool Inventory::agentProjectionReady() const
{
    return _agentFrameReady && _agentGridReady && _agentShownRevision == _agentInventoryRevision &&
        getX() == 0 && getY() == 0 && getWidth() == 320 && getHeight() == 200 &&
        Observer::agentInventoryLayer(this) && Observer::agentInventoryLayer(_grid) &&
        Observer::agentInventoryLayer(_items) && Observer::agentInventoryLayer(_selection) &&
        _grid->getX() == 0 && _grid->getY() == 0 && _items->getX() == 0 && _items->getY() == 0;
}
bool Inventory::agentOverlayClear(const Observer::AgentUiRect &r, bool selection) const
{
    return Observer::agentInventoryInside(r) &&
        (!selection || Observer::agentInventoryClear(_selection, r)) &&
        Observer::agentInventoryClear(_warning, r);
}
void Inventory::agentCaptureItems(const Surface *destination)
{
    _agentHeldPresented = false;
    _agentShownItemInk.assign(320 * 200, 0);
    _agentShownGridInk.assign(320 * 200, 0);
    if (!Observer::agentInventoryLayer(this) || !Observer::agentInventoryLayer(_grid) ||
        !Observer::agentInventoryLayer(_items) || !Observer::agentInventoryLayer(_selection) ||
        !Observer::agentInventoryLayer(destination) || getX() || getY() || getWidth() != 320 || getHeight() != 200 ||
        destination->getWidth() < 320 || destination->getHeight() < 200) return;
    const auto color = [](const Surface *surface, unsigned index) -> unsigned {
        const SDL_Color &c = surface->getSurface()->format->palette->colors[index];
        return (unsigned(c.r) << 16) | (unsigned(c.g) << 8) | unsigned(c.b);
    };
    Observer::AgentUiRect heldInk;
    if (Observer::agentInventoryInk(_selection, heldInk) && heldInk.x >= 0 && heldInk.y >= 0 &&
        heldInk.x + heldInk.width <= 320 && heldInk.y + heldInk.height <= 200 && agentOverlayClear(heldInk, false))
        for (int y = heldInk.y; y < heldInk.y + heldInk.height; ++y)
            for (int x = heldInk.x; x < heldInk.x + heldInk.width; ++x)
            {
                const unsigned held = Observer::agentInventoryPixel(_selection, x-_selection->getX(), y-_selection->getY());
                if (!held) continue;
                const unsigned item = Observer::agentInventoryPixel(_items,x,y), grid = Observer::agentInventoryPixel(_grid,x,y);
                const unsigned under = item ? color(_items,item) : grid ? color(_grid,grid) : color(destination,Observer::agentInventoryPixel(destination,x,y));
                if (color(_selection,held) != under && color(this,Observer::agentInventoryPixel(this,x,y)) == color(_selection,held)) _agentHeldPresented = true;
            }
    for (int y = 0; y < 200; ++y) for (int x = 0; x < 320; ++x)
    {
        const Observer::AgentUiRect point = {x,y,1,1};
        if (!agentOverlayClear(point)) continue;
        const unsigned item = Observer::agentInventoryPixel(_items, x, y);
        const unsigned grid = Observer::agentInventoryPixel(_grid, x, y);
        const unsigned background = color(destination, Observer::agentInventoryPixel(destination, x, y));
        if (item && color(this,Observer::agentInventoryPixel(this,x,y)) == color(_items,item) && color(_items, item) != (grid ? color(_grid, grid) : background))
            _agentShownItemInk[y * 320 + x] = 1;
        else if (!item && grid && color(this,Observer::agentInventoryPixel(this,x,y)) == color(_grid,grid) && color(_grid, grid) != background)
            _agentShownGridInk[y * 320 + x] = 1;
    }
}
bool Inventory::agentItemsVisible(const Observer::AgentUiRect &r) const
{
    if (_agentShownItemInk.size() != 320 * 200 || r.x < 0 || r.y < 0 || r.x + r.width > 320 || r.y + r.height > 200) return false;
    for (int y = r.y; y < r.y + r.height; ++y) for (int x = r.x; x < r.x + r.width; ++x)
        if (_agentShownItemInk[y * 320 + x]) return true;
    return false;
}
bool Inventory::agentGridVisible(const AgentTarget &target) const
{
    if (_agentShownGridInk.size() != 320 * 200) return false;
    const Observer::AgentUiRect &r = target.rect;
    const int right = r.x + r.width - (target.area == "hand" ? 1 : 0);
    const int bottom = r.y + r.height - (target.area == "hand" ? 1 : 0);
    if (right >= 320 || bottom >= 200) return false;
    bool top = false, down = false, left = false, edge = false;
    for (int x = r.x; x <= right; ++x)
    { top |= _agentShownGridInk[r.y * 320 + x] != 0; down |= _agentShownGridInk[bottom * 320 + x] != 0; }
    for (int y = r.y; y <= bottom; ++y)
    { left |= _agentShownGridInk[y * 320 + r.x] != 0; edge |= _agentShownGridInk[y * 320 + right] != 0; }
    return top && down && left && edge;
}
}
#endif
