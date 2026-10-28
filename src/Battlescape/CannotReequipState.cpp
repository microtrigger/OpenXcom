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
#include "CannotReequipState.h"
#include <sstream>
#include <algorithm>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Engine/Options.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Cannot Reequip screen.
 * @param game Pointer to the core game.
 * @param missingItems List of items still needed for reequip.
 */
CannotReequipState::CannotReequipState(std::vector<ReequipStat> missingItems)
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(120, 18, 100, 174);
	_txtTitle = new Text(220, 32, 50, 8);
	_txtItem = new Text(142, 9, 10, 50);
	_txtQuantity = new Text(88, 9, 152, 50);
	_txtCraft = new Text(74, 9, 218, 50);
	_lstItems = new TextList(288, 112, 8, 58);

	// Set palette
	setInterface("cannotReequip");

	add(_window, "window", "cannotReequip");
	add(_btnOk, "button", "cannotReequip");
	add(_txtTitle, "heading", "cannotReequip");
	add(_txtItem, "text", "cannotReequip");
	add(_txtQuantity, "text", "cannotReequip");
	add(_txtCraft, "text", "cannotReequip");
	add(_lstItems, "list", "cannotReequip");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&CannotReequipState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&CannotReequipState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&CannotReequipState::btnOkClick, Options::keyCancel);

	_txtTitle->setText(tr("STR_NOT_ENOUGH_EQUIPMENT_TO_FULLY_RE_EQUIP_SQUAD"));
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setWordWrap(true);

	_txtItem->setText(tr("STR_ITEM"));

	_txtQuantity->setText(tr("STR_QUANTITY_UC"));

	_txtCraft->setText(tr("STR_CRAFT"));

	_lstItems->setColumns(3, 162, 46, 80);
	_lstItems->setSelectable(true);
	_lstItems->setBackground(_window);
	_lstItems->setMargin(2);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _lstItems->agentEnableReequipCells();
    for (Surface *widget : std::vector<Surface *>{_window, _txtTitle, _txtItem, _txtQuantity, _txtCraft, _btnOk})
        widget->agentTrackPresentation();
#endif

	for (std::vector<ReequipStat>::iterator i = missingItems.begin(); i != missingItems.end(); ++i)
	{
		std::ostringstream ss;
		ss << i->qty;
		_lstItems->addRow(3, tr(i->item).c_str(), ss.str().c_str(), i->craft.c_str());
	}
}

/**
 *
 */
CannotReequipState::~CannotReequipState()
{
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void CannotReequipState::btnOkClick(Action *)
{
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool CannotReequipState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> CannotReequipState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions = {_window->agentPresentationRevision(),
        _lstItems->agentPresentationRevision(), _lstItems->agentListRevision(),
        _btnOk->agentPresentationRevision(), _btnOk->agentLabelRevision()};
    for (const Text *widget : {_txtTitle, _txtItem, _txtQuantity, _txtCraft})
    {
        revisions.push_back(widget->agentPresentationRevision());
        revisions.push_back(widget->agentTextRevision());
    }
    return revisions;
}

void CannotReequipState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool CannotReequipState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<CannotReequipState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "cannot_reequip";
    out.actor = this; // Private current-popup ownership only.
    const Surface *destination = _game->getScreen()->getSurface();
    if (!_window->agentPresentationPaintedTo(destination)) return false;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const auto text = [&](const Text *widget, bool heading) -> std::string {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        out.context.insert(out.context.end(), {static_cast<std::uint64_t>(widget->getX()),
            static_cast<std::uint64_t>(widget->getY()), static_cast<std::uint64_t>(widget->getWidth()),
            static_cast<std::uint64_t>(widget->getHeight())});
        if (!widget->agentPresentationPaintedTo(destination) || !agentWidgetVisible(widget) ||
            widget->getText().size() > 1024) return "null";
        // Native headings allocate9px at y50; list begins at y58. Observe the
        // fixed8px allocation strip, excluding its spare bottom row. No font,
        // caption-fit, glyph, ink or character-completeness admission criterion.
        const int height = heading && widget->getHeight() == 9 ? 8 : widget->getHeight();
        AgentUiState part;
        // The Quantity allocation extends underneath the later Craft heading.
        // Its fixed exposed allocation ends at Craft's left edge. This bounds
        // observation geometry only; no glyph or model reconstruction.
        int width = widget->getWidth();
        if (widget == _txtQuantity && _txtCraft->getVisible() && _txtCraft->getY() == widget->getY() &&
            _txtCraft->getX() > widget->getX())
            width = std::min(width, _txtCraft->getX() - widget->getX());
        part.regions.push_back({widget->getX(), widget->getY(), width, height});
        bool found = false;
        for (const Surface *later : painted)
        {
            if (later == widget) { found = true; continue; }
            if (!found || !later || !later->getVisible()) continue;
            const auto &rect = part.regions.front();
            if (rect.x < later->getX() + later->getWidth() && rect.x + rect.width > later->getX() &&
                rect.y < later->getY() + later->getHeight() && rect.y + rect.height > later->getY()) return "null";
        }
        if (!found || !_game->agentNonCursorOverlayClear(part)) return "null";
        return agentJsonString(widget->getText());
    };
    out.widgets.emplace_back(_lstItems, _lstItems->agentPresentationRevision());
    out.widgets.emplace_back(_lstItems, _lstItems->agentListRevision());
    out.context.insert(out.context.end(), {static_cast<std::uint64_t>(_lstItems->getX()),
        static_cast<std::uint64_t>(_lstItems->getY()), static_cast<std::uint64_t>(_lstItems->getWidth()),
        static_cast<std::uint64_t>(_lstItems->getHeight())});
    std::vector<AgentOutfittingRow> projected;
    if (!_lstItems->agentPresentationPaintedTo(destination) || !agentWidgetVisible(_lstItems) ||
        !_lstItems->agentReequipRows(projected) || projected.size() > 128) return false;
    // Read-only current viewport. Private row identity never becomes a handle,
    // selection, count, action target or reconstructed promotion/model information.
    std::string rows = "[";
    for (const auto &row : projected)
    {
        if (row.cells.size() != 3) return false;
        if (rows.size() > 1) rows += ",";
        rows += "{\"cells\":[";
        for (std::size_t c = 0; c < 3; ++c)
        {
            if (c) rows += ",";
            const auto &cell = row.cells[c];
            bool admitted = cell.known && !cell.regions.empty() && cell.text.size() <= 1024;
            AgentUiState part;
            for (const auto &rect : cell.regions)
                part.regions.push_back({rect.x, rect.y, rect.w, rect.h});
            bool found = false;
            for (const Surface *later : painted)
            {
                if (later == _lstItems) { found = true; continue; }
                if (!found || !later || !later->getVisible()) continue;
                for (const auto &rect : part.regions)
                    if (rect.x < later->getX() + later->getWidth() && rect.x + rect.width > later->getX() &&
                        rect.y < later->getY() + later->getHeight() && rect.y + rect.height > later->getY()) admitted = false;
            }
            admitted = admitted && found && _game->agentNonCursorOverlayClear(part);
            rows += admitted ? agentJsonString(cell.text) : "null";
        }
        rows += "]}";
    }
    rows += "]";
    // Only the ordinary visible OK hitbox admits an action. Caption observation
    // above remains independent of pointer/glyph readability.
    out.widgets.emplace_back(_btnOk, _btnOk->agentPresentationRevision());
    out.widgets.emplace_back(_btnOk, _btnOk->agentLabelRevision());
    out.context.insert(out.context.end(), {static_cast<std::uint64_t>(_btnOk->getX()),
        static_cast<std::uint64_t>(_btnOk->getY()), static_cast<std::uint64_t>(_btnOk->getWidth()),
        static_cast<std::uint64_t>(_btnOk->getHeight())});
    const std::string label = _btnOk->getText();
    AgentUiState button;
    button.regions.push_back({_btnOk->getX(), _btnOk->getY(), _btnOk->getWidth(), _btnOk->getHeight()});
    bool admitted = _btnOk->agentPresentationPaintedTo(destination) && agentWidgetVisible(_btnOk) &&
        !label.empty() && label.size() <= 1024;
    bool found = false;
    for (const Surface *later : painted)
    {
        if (later == _btnOk) { found = true; continue; }
        if (!found || !later || !later->getVisible()) continue;
        const auto &rect = button.regions.front();
        if (rect.x < later->getX() + later->getWidth() && rect.x + rect.width > later->getX() &&
            rect.y < later->getY() + later->getHeight() && rect.y + rect.height > later->getY()) admitted = false;
    }
    std::string buttons = "[]";
    if (admitted && found && _game->agentOverlayClear(button))
    {
        buttons = "[{\"id\":\"ok\",\"label\":" + agentJsonString(label) + "}]";
        out.regions.insert(out.regions.end(), button.regions.begin(), button.regions.end());
        out.actions.emplace_back("acknowledge_cannot_reequip", label, "visible_text");
    }
    const Surface *up = nullptr, *down = nullptr;
    _lstItems->agentScrollControls(up, down);
    const auto arrow = [&](const Surface *widget) -> bool {
        if (!widget || !widget->agentPresentationPaintedTo(destination) || !agentWidgetVisible(widget)) return false;
        AgentUiState part; agentIncludeWidget(part, widget);
        if (part.regions.empty() || !agentRegionsUncovered(part.regions, widget, painted) || !_game->agentOverlayClear(part)) return false;
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (!agentRegionsDisjoint(out)) { out.regions.resize(before); return false; }
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        return true;
    };
    const bool scrollUp = arrow(up), scrollDown = arrow(down);
    out.context.insert(out.context.begin(), {scrollUp ? 1u : 0u, scrollDown ? 1u : 0u});
    if (scrollUp || scrollDown)
        out.actions.emplace_back("scroll_visible_list", "Scroll visible list", "visible_control_semantics", "scroll_direction");
    const std::string scroll = std::string("{\"up\":") + (scrollUp ? "true" : "false") +
        ",\"down\":" + (scrollDown ? "true" : "false") + "}";
    out.visible = "{\"title\":" + text(_txtTitle, false) + ",\"headings\":[" +
        text(_txtItem, true) + "," + text(_txtQuantity, true) + "," + text(_txtCraft, true) +
        "],\"rows\":" + rows + ",\"scroll\":" + scroll + ",\"buttons\":" + buttons + "}";
    // Current UI captions only. No shortage inventory calculation or row actions.
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return true;
}
bool CannotReequipState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "acknowledge_cannot_reequip") { btnOkClick(nullptr); return true; }
    if (request.action == "scroll_visible_list" && current.context.size() >= 2)
    {
        if (request.choice == "up" && current.context[0]) { _lstItems->scrollUp(false, false); return true; }
        if (request.choice == "down" && current.context[1]) { _lstItems->scrollDown(false, false); return true; }
    }
    return false;
}
#endif

}
