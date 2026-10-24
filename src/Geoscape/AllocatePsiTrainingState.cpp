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
#include <sstream>
#include "PsiTrainingState.h"
#include "AllocatePsiTrainingState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Interface/TextList.h"
#include "../Savegame/Soldier.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Psi Training screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to handle.
 */
AllocatePsiTrainingState::AllocatePsiTrainingState(Base *base) : _sel(0)
{
	_base = base;
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_txtTitle = new Text(300, 17, 10, 8);
	_txtRemaining = new Text(300, 10, 10, 24);
	_txtName = new Text(64, 10, 10, 40);
	_txtPsiStrength = new Text(80, 20, 124, 32);
	_txtPsiSkill = new Text(80, 20, 188, 32);
	_txtTraining = new Text(48, 20, 270, 32);
	_btnOk = new TextButton(160, 14, 80, 174);
	_lstSoldiers = new TextList(290, 112, 8, 52);

	// Set palette
	setInterface("allocatePsi");

	add(_window, "window", "allocatePsi");
	add(_btnOk, "button", "allocatePsi");
	add(_txtName, "text", "allocatePsi");
	add(_txtTitle, "text", "allocatePsi");
	add(_txtRemaining, "text", "allocatePsi");
	add(_txtPsiStrength, "text", "allocatePsi");
	add(_txtPsiSkill, "text", "allocatePsi");
	add(_txtTraining, "text", "allocatePsi");
	add(_lstSoldiers, "list", "allocatePsi");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK01.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&AllocatePsiTrainingState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&AllocatePsiTrainingState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_PSIONIC_TRAINING"));

	_labSpace = base->getAvailablePsiLabs() - base->getUsedPsiLabs();
	_txtRemaining->setText(tr("STR_REMAINING_PSI_LAB_CAPACITY").arg(_labSpace));

	_txtName->setText(tr("STR_NAME"));

	_txtPsiStrength->setText(tr("STR_PSIONIC__STRENGTH"));

	_txtPsiSkill->setText(tr("STR_PSIONIC_SKILL_IMPROVEMENT"));

	_txtTraining->setText(tr("STR_IN_TRAINING"));

	_lstSoldiers->setAlign(ALIGN_RIGHT, 3);
	_lstSoldiers->setColumns(4, 114, 80, 62, 30);
	_lstSoldiers->setSelectable(true);
	_lstSoldiers->setBackground(_window);
	_lstSoldiers->setMargin(2);
	_lstSoldiers->onMouseClick((ActionHandler)&AllocatePsiTrainingState::lstSoldiersClick);
	int row = 0;
	for (std::vector<Soldier*>::const_iterator s = base->getSoldiers()->begin(); s != base->getSoldiers()->end(); ++s)
	{
		std::ostringstream ssStr;
		std::ostringstream ssSkl;
		_soldiers.push_back(*s);
		if ((*s)->getCurrentStats()->psiSkill > 0 || (Options::psiStrengthEval && _game->getSavedGame()->isResearched(_game->getMod()->getPsiRequirements())))
		{
			ssStr << "   " << (*s)->getCurrentStats()->psiStrength;
			if (Options::allowPsiStrengthImprovement) ssStr << "/+" << (*s)->getPsiStrImprovement();
		}
		else
		{
			ssStr << tr("STR_UNKNOWN");
		}
		if ((*s)->getCurrentStats()->psiSkill > 0)
		{
			ssSkl << (*s)->getCurrentStats()->psiSkill << "/+" << (*s)->getImprovement();
		}
		else
		{
			ssSkl << "0/+0";
		}
		if ((*s)->isInPsiTraining())
		{
			_lstSoldiers->addRow(4, (*s)->getName(true).c_str(), ssStr.str().c_str(), ssSkl.str().c_str(), tr("STR_YES").c_str());
			_lstSoldiers->setRowColor(row, _lstSoldiers->getSecondaryColor());
		}
		else
		{
			_lstSoldiers->addRow(4, (*s)->getName(true).c_str(), ssStr.str().c_str(), ssSkl.str().c_str(), tr("STR_NO").c_str());
			_lstSoldiers->setRowColor(row, _lstSoldiers->getColor());
		}
		row++;
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *widget : _surfaces) widget->agentTrackPresentation();
    _lstSoldiers->agentEnablePsiTrainingCells();
#endif
}

/**
 *
 */
AllocatePsiTrainingState::~AllocatePsiTrainingState()
{

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void AllocatePsiTrainingState::btnOkClick(Action *)
{
	for (std::vector<Soldier*>::iterator i = _base->getSoldiers()->begin(); i != _base->getSoldiers()->end(); ++i)
	{
		(*i)->calcStatString(_game->getMod()->getStatStrings(), (Options::psiStrengthEval && _game->getSavedGame()->isResearched(_game->getMod()->getPsiRequirements())));
	}
	_game->popState();
}

/**
 * Assigns / removes a soldier from Psi Training.
 * @param action Pointer to an action.
 */
void AllocatePsiTrainingState::lstSoldiersClick(Action *action)
{
	_sel = _lstSoldiers->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		if (!_base->getSoldiers()->at(_sel)->isInPsiTraining())
		{
			if (_base->getUsedPsiLabs() < _base->getAvailablePsiLabs())
			{
				_lstSoldiers->setCellText(_sel, 3, tr("STR_YES"));
				_lstSoldiers->setRowColor(_sel, _lstSoldiers->getSecondaryColor());
				_labSpace--;
				_txtRemaining->setText(tr("STR_REMAINING_PSI_LAB_CAPACITY").arg(_labSpace));
				_base->getSoldiers()->at(_sel)->setPsiTraining(true);
			}
		}
		else
		{
			_lstSoldiers->setCellText(_sel, 3, tr("STR_NO"));
			_lstSoldiers->setRowColor(_sel, _lstSoldiers->getColor());
			_labSpace++;
			_txtRemaining->setText(tr("STR_REMAINING_PSI_LAB_CAPACITY").arg(_labSpace));
			_base->getSoldiers()->at(_sel)->setPsiTraining(false);
		}
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool AllocatePsiTrainingState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> AllocatePsiTrainingState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> result;
    for (const Surface *widget : _surfaces) result.push_back(widget->agentPresentationRevision());
    for (const Text *widget : std::vector<const Text *>{_txtTitle, _txtRemaining, _txtName, _txtPsiStrength, _txtPsiSkill, _txtTraining})
        result.push_back(widget->agentTextRevision());
    result.push_back(_btnOk->agentLabelRevision());
    result.push_back(_lstSoldiers->agentListRevision());
    return result;
}

void AllocatePsiTrainingState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool AllocatePsiTrainingState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<AllocatePsiTrainingState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    // Native click indexes the current roster, while captions were constructed
    // from _soldiers. Bind the complete private pointer order, never stats.
    if (!_base || *_base->getSoldiers() != _soldiers) return false;
    out = AgentUiState();
    out.screen = "allocate_psi_training";
    out.actor = this; // Private active-popup identity, never serialized.
    out.widgets.emplace_back(_base, 0);
    for (const Soldier *soldier : _soldiers) out.widgets.emplace_back(soldier, 0);
    const Surface *destination = _game->getScreen()->getSurface();
    if (!_window->agentPresentationPaintedTo(destination)) return false;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const auto include = [&](const Surface *widget, bool observationOnly) -> bool {
        if (!widget->agentPresentationPaintedTo(destination) || !agentWidgetVisible(widget)) return false;
        AgentUiState part;
        part.regions.push_back({widget->getX(), widget->getY(), widget->getWidth(), widget->getHeight()});
        // Fixed native allocation overlaps, independent of content/font/fit.
        // Keep full-widget viewport admission above, but exclude spare rows and
        // the strength heading's native overlap with the following heading.
        if (observationOnly && widget == _txtTitle && widget->getHeight() == 17) part.regions.front().height = 16;
        if (observationOnly && widget == _txtRemaining && widget->getHeight() == 10) part.regions.front().height = 8;
        if (observationOnly && widget == _txtPsiStrength && widget->getWidth() == 80) part.regions.front().width = 64;
        bool found = false;
        for (const Surface *later : painted)
        {
            if (later == widget) { found = true; continue; }
            if (!found || !later || !later->getVisible()) continue;
            const auto &rect = part.regions.front();
            if (rect.x < later->getX() + later->getWidth() && rect.x + rect.width > later->getX() &&
                rect.y < later->getY() + later->getHeight() && rect.y + rect.height > later->getY()) return false;
        }
        if (!found || !(observationOnly ? _game->agentNonCursorOverlayClear(part) : _game->agentOverlayClear(part))) return false;
        if (observationOnly) return true;
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before);
        return false;
    };
    // These are current active UI captions, not a glyph-pixel readability test.
    // Hidden, geometrically absent, occluded or stale widgets contribute no value.
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        out.context.insert(out.context.end(), {
            static_cast<std::uint64_t>(widget->getX()), static_cast<std::uint64_t>(widget->getY()),
            static_cast<std::uint64_t>(widget->getWidth()), static_cast<std::uint64_t>(widget->getHeight())});
        // Active logical captions: glyph/ink completeness and pointer overlap
        // are not text admission criteria. Resource bound is not a layout test.
        if (widget->getText().size() > 1024 || !include(widget, true)) return "null";
        return agentJsonString(widget->getText());
    };
    const auto title = text(_txtTitle), remaining = text(_txtRemaining);
    const std::string headings = "[" + text(_txtName) + "," + text(_txtPsiStrength) + "," + text(_txtPsiSkill) + "," + text(_txtTraining) + "]";
    out.widgets.emplace_back(_lstSoldiers, _lstSoldiers->agentListRevision());
    std::vector<AgentOutfittingRow> projected;
    if (!include(_lstSoldiers, true) || !_lstSoldiers->agentPsiTrainingRows(projected)) return false;
    std::string rows = "[";
    for (const auto &row : projected)
    {
        if (row.logicalIndex >= _soldiers.size() || row.cells.size() != 4 || out.rows.size() >= 32) return false;
        AgentUiState part;
        part.regions.push_back({row.bounds.x, row.bounds.y, row.bounds.w, row.bounds.h});
        if (!_game->agentOverlayClear(part)) continue;
        const auto before = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (!agentRegionsDisjoint(out)) { out.regions.resize(before); continue; }
        if (rows.size() > 1) rows += ",";
        rows += "{\"handle\":" + agentJsonString("r" + std::to_string(out.rows.size())) + ",\"cells\":[";
        for (size_t c = 0; c < 4; ++c)
        {
            if (c) rows += ",";
            rows += row.cells[c].known ? agentJsonString(row.cells[c].text) : "null";
        }
        rows += "],\"selected\":" + std::string(row.selected ? "true" : "false") + "}";
        out.rows.push_back(row.logicalIndex);
    }
    rows += "]";
    if (!out.rows.empty()) out.actions.emplace_back("toggle_psi_training", "Toggle psi training", "visible_control_semantics", "outfitting_row");
    const Surface *up = nullptr, *down = nullptr;
    _lstSoldiers->agentScrollControls(up, down);
    for (const Surface *arrow : {up, down}) if (arrow)
    {
        out.widgets.emplace_back(arrow, arrow->agentPresentationRevision());
        out.context.insert(out.context.end(), {static_cast<std::uint64_t>(arrow->getX()),
            static_cast<std::uint64_t>(arrow->getY()), static_cast<std::uint64_t>(arrow->getWidth()),
            static_cast<std::uint64_t>(arrow->getHeight())});
    }
    const bool canUp = up && include(up, false), canDown = down && include(down, false);
    if (canUp || canDown) out.actions.emplace_back("scroll_visible_list", "Scroll visible list", "visible_control_semantics", "scroll_direction");
    const std::string scroll = "{\"up\":" + std::string(canUp ? "true" : "false") + ",\"down\":" + (canDown ? "true" : "false") + "}";
    std::string buttons = "[";
    const auto button = [&](TextButton *widget, const char *id, const char *action) {
        out.widgets.emplace_back(widget, widget->agentPresentationRevision());
        out.widgets.emplace_back(widget, widget->agentLabelRevision());
        const std::string label = widget->getText();
        if (label.empty() || label.size() > 1024 || !include(widget, false)) return;
        if (buttons.size() > 1) buttons += ",";
        buttons += "{\"id\":" + agentJsonString(id) + ",\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back(action, label, "visible_text");
    };
    button(_btnOk, "ok", "close_psi_training_allocation");
    buttons += "]";
    out.context.push_back(canUp); out.context.push_back(canDown);
    out.visible = "{\"title\":" + title + ",\"remaining\":" + remaining + ",\"headings\":" + headings + ",\"rows\":" + rows + ",\"scroll\":" + scroll + ",\"buttons\":" + buttons + "}";
    const std::string payload = out.payload();
    if (payload.size() > 9000) return false;
    std::size_t asciiBytes = 0;
    for (unsigned char c : payload)
        if ((asciiBytes += c < 128 ? 1 : 6) > 30000) return false;
    return !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}

bool AllocatePsiTrainingState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "scroll_visible_list" && current.context.size() >= 2)
    {
        const size_t n = current.context.size();
        if (request.choice == "up" && current.context[n-2]) { _lstSoldiers->scrollUp(false, false); return true; }
        if (request.choice == "down" && current.context[n-1]) { _lstSoldiers->scrollDown(false, false); return true; }
        return false;
    }
    if (request.action == "toggle_psi_training")
    {
        for (size_t i = 0; i < current.rows.size(); ++i)
            if (request.choice == "r" + std::to_string(i) && _lstSoldiers->agentSelectPsiTrainingRow(current.rows[i]))
            {
                SDL_Event event = {}; event.type = SDL_MOUSEBUTTONDOWN; event.button.button = SDL_BUTTON_LEFT;
                Action action(&event, 1.0, 1.0, 0, 0);
                lstSoldiersClick(&action);
                return true;
            }
        return false;
    }
    if (request.action == "close_psi_training_allocation") { btnOkClick(nullptr); return true; }
    return false;
}
#endif

}
