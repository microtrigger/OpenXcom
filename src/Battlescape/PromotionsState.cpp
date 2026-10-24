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
#include "PromotionsState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/Soldier.h"
#include "../Engine/Options.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Promotions screen.
 * @param game Pointer to the core game.
 */
PromotionsState::PromotionsState()
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(288, 16, 16, 176);
	_txtTitle = new Text(300, 17, 10, 8);
	_txtName = new Text(114, 9, 16, 32);
	_txtRank = new Text(90, 9, 130, 32);
	_txtBase = new Text(80, 9, 220, 32);
	_lstSoldiers = new TextList(288, 128, 8, 40);

	// Set palette
	setInterface("promotions");

	add(_window, "window", "promotions");
	add(_btnOk, "button", "promotions");
	add(_txtTitle, "heading", "promotions");
	add(_txtName, "text", "promotions");
	add(_txtRank, "text", "promotions");
	add(_txtBase, "text", "promotions");
	add(_lstSoldiers, "list", "promotions");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&PromotionsState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&PromotionsState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&PromotionsState::btnOkClick, Options::keyCancel);

	_txtTitle->setText(tr("STR_PROMOTIONS"));
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();

	_txtName->setText(tr("STR_NAME"));

	_txtRank->setText(tr("STR_NEW_RANK"));

	_txtBase->setText(tr("STR_BASE"));

	_lstSoldiers->setColumns(3, 114, 90, 84);
	_lstSoldiers->setSelectable(true);
	_lstSoldiers->setBackground(_window);
	_lstSoldiers->setMargin(8);

	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		for (std::vector<Soldier*>::iterator j = (*i)->getSoldiers()->begin(); j != (*i)->getSoldiers()->end(); ++j)
		{
			if ((*j)->isPromoted())
			{
				_lstSoldiers->addRow(3, (*j)->getName().c_str(), tr((*j)->getRankString()).c_str(), (*i)->getName().c_str());
			}
		}
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _lstSoldiers->agentEnablePromotionCells();
    for (Surface *widget : std::vector<Surface *>{_window, _btnOk, _txtTitle, _txtName, _txtRank, _txtBase, _lstSoldiers})
        widget->agentTrackPresentation();
#endif
}

/**
 *
 */
PromotionsState::~PromotionsState()
{
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void PromotionsState::btnOkClick(Action *)
{
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool PromotionsState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> PromotionsState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions = {_window->agentPresentationRevision(),
        _lstSoldiers->agentPresentationRevision(), _lstSoldiers->agentListRevision(),
        _btnOk->agentPresentationRevision(), _btnOk->agentLabelRevision()};
    for (const Text *widget : {_txtTitle, _txtName, _txtRank, _txtBase})
    {
        revisions.push_back(widget->agentPresentationRevision());
        revisions.push_back(widget->agentTextRevision());
    }
    return revisions;
}

void PromotionsState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool PromotionsState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<PromotionsState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState();
    out.screen = "promotions";
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
        // Native headings allocate9px at y32; list begins at y40. Observe the
        // fixed8px allocation strip, excluding its spare bottom row. No font,
        // caption-fit, glyph, ink or character-completeness admission criterion.
        const int height = heading && widget->getHeight() == 9 ? 8 : widget->getHeight();
        AgentUiState part;
        part.regions.push_back({widget->getX(), widget->getY(), widget->getWidth(), height});
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
    out.widgets.emplace_back(_lstSoldiers, _lstSoldiers->agentPresentationRevision());
    out.widgets.emplace_back(_lstSoldiers, _lstSoldiers->agentListRevision());
    out.context.insert(out.context.end(), {static_cast<std::uint64_t>(_lstSoldiers->getX()),
        static_cast<std::uint64_t>(_lstSoldiers->getY()), static_cast<std::uint64_t>(_lstSoldiers->getWidth()),
        static_cast<std::uint64_t>(_lstSoldiers->getHeight())});
    std::vector<AgentOutfittingRow> projected;
    if (!_lstSoldiers->agentPresentationPaintedTo(destination) || !agentWidgetVisible(_lstSoldiers) ||
        !_lstSoldiers->agentPromotionRows(projected) || projected.size() > 128) return false;
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
                if (later == _lstSoldiers) { found = true; continue; }
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
        out.regions = button.regions;
        out.actions.emplace_back("close_promotions", label, "visible_text");
    }
    out.visible = "{\"title\":" + text(_txtTitle, false) + ",\"headings\":[" +
        text(_txtName, true) + "," + text(_txtRank, true) + "," + text(_txtBase, true) +
        "],\"rows\":" + rows + ",\"buttons\":" + buttons + "}";
    // No row/scroll actions. Constructor promotion-flag consumption is already
    // complete; neither projection nor this native pop-only acknowledgment repeats it.
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return true;
}
bool PromotionsState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (request.action != "close_promotions" || !agentUiState(current) || !current.offers(request.action)) return false;
    btnOkClick(nullptr);
    return true;
}
#endif

}
