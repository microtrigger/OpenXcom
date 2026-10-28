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
#include "MonthlyCostsState.h"
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleSoldier.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Engine/Screen.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#include <typeinfo>
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Monthly Costs screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 */
MonthlyCostsState::MonthlyCostsState(Base *base) : _base(base)
{
	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(300, 20, 10, 170);
	_txtTitle = new Text(310, 17, 5, 12);
	_txtCost = new Text(80, 9, 115, 32);
	_txtQuantity = new Text(55, 9, 195, 32);
	_txtTotal = new Text(60, 9, 249, 32);
	_txtRental = new Text(150, 9, 10, 40);
	_txtSalaries = new Text(150, 9, 10, 80);
	_txtIncome = new Text(150, 9, 10, 146);
	_txtMaintenance = new Text(150, 9, 10, 154);
	_lstCrafts = new TextList(288, 32, 10, 48);
	_lstSalaries = new TextList(288, 40, 10, 88);
	_lstMaintenance = new TextList(300, 9, 10, 128);
	_lstTotal = new TextList(100, 9, 205, 150);

	// Set palette
	setInterface("costsInfo");

	add(_window, "window", "costsInfo");
	add(_btnOk, "button", "costsInfo");
	add(_txtTitle, "text1", "costsInfo");
	add(_txtCost, "text1", "costsInfo");
	add(_txtQuantity, "text1", "costsInfo");
	add(_txtTotal, "text1", "costsInfo");
	add(_txtRental, "text1", "costsInfo");
	add(_lstCrafts, "list", "costsInfo");
	add(_txtSalaries, "text1", "costsInfo");
	add(_lstSalaries, "list", "costsInfo");
	add(_lstMaintenance, "text1", "costsInfo");
	add(_txtIncome, "list", "costsInfo");
	add(_txtMaintenance, "list", "costsInfo");
	add(_lstTotal, "text2", "costsInfo");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK13.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&MonthlyCostsState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&MonthlyCostsState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&MonthlyCostsState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_MONTHLY_COSTS"));

	_txtCost->setText(tr("STR_COST_PER_UNIT"));

	_txtQuantity->setText(tr("STR_QUANTITY"));

	_txtTotal->setText(tr("STR_TOTAL"));

	_txtRental->setText(tr("STR_CRAFT_RENTAL"));

	_txtSalaries->setText(tr("STR_SALARIES"));

	std::ostringstream ss;
	ss << tr("STR_INCOME") << "=" << Unicode::formatFunding(_game->getSavedGame()->getCountryFunding());
	_txtIncome->setText(ss.str());

	std::ostringstream ss2;
	ss2 << tr("STR_MAINTENANCE") << "=" << Unicode::formatFunding(_game->getSavedGame()->getBaseMaintenance());
	_txtMaintenance->setText(ss2.str());

	_lstCrafts->setColumns(4, 125, 70, 44, 50);
	_lstCrafts->setDot(true);

	const std::vector<std::string> &crafts = _game->getMod()->getCraftsList();
	for (std::vector<std::string>::const_iterator i = crafts.begin(); i != crafts.end(); ++i)
	{
		RuleCraft *craft = _game->getMod()->getCraft(*i);
		if (craft->getRentCost() != 0 && _game->getSavedGame()->isResearched(craft->getRequirements()))
		{
			std::ostringstream ss3;
			ss3 << _base->getCraftCount(*i);
			_lstCrafts->addRow(4, tr(*i).c_str(), Unicode::formatFunding(craft->getRentCost()).c_str(), ss3.str().c_str(), Unicode::formatFunding(_base->getCraftCount(*i) * craft->getRentCost()).c_str());
		}
	}

	_lstSalaries->setColumns(4, 125, 70, 44, 50);
	_lstSalaries->setDot(true);

	const std::vector<std::string> &soldiers = _game->getMod()->getSoldiersList();
	for (std::vector<std::string>::const_iterator i = soldiers.begin(); i != soldiers.end(); ++i)
	{
		RuleSoldier *soldier = _game->getMod()->getSoldier(*i);
		if (soldier->getSalaryCost() != 0 && _game->getSavedGame()->isResearched(soldier->getRequirements()))
		{
			std::ostringstream ss4;
			ss4 << _base->getSoldierCount(*i);
			std::string name = (*i);
			if (soldiers.size() == 1)
			{
				name = "STR_SOLDIERS";
			}
			_lstSalaries->addRow(4, tr(name).c_str(), Unicode::formatFunding(soldier->getSalaryCost()).c_str(), ss4.str().c_str(), Unicode::formatFunding(_base->getSoldierCount(*i) * soldier->getSalaryCost()).c_str());
		}
	}
	std::ostringstream ss5;
	ss5 << _base->getTotalEngineers();
	_lstSalaries->addRow(4, tr("STR_ENGINEERS").c_str(), Unicode::formatFunding(_game->getMod()->getEngineerCost()).c_str(), ss5.str().c_str(), Unicode::formatFunding(_base->getTotalEngineers() * _game->getMod()->getEngineerCost()).c_str());
	std::ostringstream ss6;
	ss6 << _base->getTotalScientists();
	_lstSalaries->addRow(4, tr("STR_SCIENTISTS").c_str(), Unicode::formatFunding(_game->getMod()->getScientistCost()).c_str(), ss6.str().c_str(), Unicode::formatFunding(_base->getTotalScientists() * _game->getMod()->getScientistCost()).c_str());

	_lstMaintenance->setColumns(2, 239, 60);
	_lstMaintenance->setDot(true);
	std::ostringstream ss7;
	ss7 << Unicode::TOK_COLOR_FLIP << Unicode::formatFunding(_base->getFacilityMaintenance());
	_lstMaintenance->addRow(2, tr("STR_BASE_MAINTENANCE").c_str(), ss7.str().c_str());

	_lstTotal->setColumns(2, 44, 55);
	_lstTotal->setDot(true);
	_lstTotal->addRow(2, tr("STR_TOTAL").c_str(), Unicode::formatFunding(_base->getMonthlyMaintenace()).c_str());
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
    _btnOk->agentEnableNamedControl();
    _lstCrafts->agentEnableReportCells(4);
    _lstSalaries->agentEnableReportCells(4);
    _lstMaintenance->agentEnableReportCells(2);
    _lstTotal->agentEnableReportCells(2);
    if (_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if (_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
#endif
}

/**
 *
 */
MonthlyCostsState::~MonthlyCostsState()
{

}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void MonthlyCostsState::btnOkClick(Action *)
{
	_game->popState();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool MonthlyCostsState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> MonthlyCostsState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    for (const Text *text : {_txtTitle, _txtCost, _txtQuantity, _txtTotal, _txtRental, _txtSalaries, _txtIncome, _txtMaintenance})
        revisions.push_back(text->agentTextRevision());
    for (const TextList *list : {_lstCrafts, _lstSalaries, _lstMaintenance, _lstTotal})
        revisions.push_back(list->agentListRevision());
    revisions.push_back(_btnOk->agentLabelRevision());
    return revisions;
}
void MonthlyCostsState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool MonthlyCostsState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(MonthlyCostsState) || _modal || _window->agentPopupActive() ||
        !_game->isState(const_cast<MonthlyCostsState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState(); out.screen = "monthly_costs"; out.actor = _base;
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const Surface *logical = _game->getScreen()->getSurface();
    if (!agentWidgetVisible(_window) || !_window->agentPresentationPaintedTo(logical)) return false;
    for (const Surface *surface : painted) out.widgets.emplace_back(surface, surface->agentPresentationRevision());
    if (_game->getCursor()) out.widgets.emplace_back(_game->getCursor(), _game->getCursor()->agentPresentationRevision());
    if (_game->getFpsCounter()) out.widgets.emplace_back(_game->getFpsCounter(), _game->getFpsCounter()->agentPresentationRevision());
    const auto allowed = [this](const AgentUiState &part) { return _game->agentOverlayClear(part); };
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        const auto value = agentTextJson(widget);
        if (value == "null" || !agentWidgetVisible(widget) || !widget->agentPresentationPaintedTo(logical)) return "null";
        // Stock small-text allocations have a ninth padding/descender row,
        // while adjoining native sections advance eight pixels. Observe the
        // current logical caption's row band, not neighboring decorative ink.
        const int height=widget->getHeight()==9 ? 8 : widget->getHeight();
        AgentUiState part;part.regions.push_back({widget->getX(),widget->getY(),widget->getWidth(),height});
        if (!agentRegionsUncovered(part.regions,widget,painted) || !allowed(part)) return "null";
        // These observations do not authorize row input. Keep their native
        // overlapping text allocations separate from named-button authority.
        return value;
    };
    const auto title = text(_txtTitle), cost = text(_txtCost), quantity = text(_txtQuantity), total = text(_txtTotal);
    const auto rental = text(_txtRental), salaries = text(_txtSalaries), income = text(_txtIncome), maintenance = text(_txtMaintenance);
    const auto table = [&](const TextList *list, unsigned columns) -> std::string {
        out.widgets.emplace_back(list, list->agentListRevision());
        std::vector<AgentOutfittingRow> rows; std::uint64_t epoch = 0;
        if (!list->agentPresentationPaintedTo(logical) || !list->agentReportRows(rows, epoch) || rows.size() > 32) return "[]";
        out.context.push_back(epoch);
        std::string values = "[";
        for (const auto &row : rows) {
            if (row.cells.size() != columns) return "[]";
            bool witness = false;
            std::string value = "{\"cells\":[";
            for (unsigned column = 0; column < columns; ++column) {
                if (column) value += ",";
                const auto &cell = row.cells[column];
                bool known = cell.known && cell.text.size() <= 1024 && !cell.regions.empty();
                bool visible = !cell.regions.empty();
                for (const SDL_Rect &band : cell.regions) {
                    AgentUiState part; part.regions.push_back({band.x, band.y, band.w, band.h});
                    if (!agentRegionsUncovered(part.regions, list, painted) || !allowed(part)) { known = false; visible = false; }
                }
                witness = witness || visible;
                value += known ? agentJsonString(cell.text) : "null";
            }
            value += "]}";
            if (!witness) continue;
            if (values.size() > 1) values += ",";
            values += value;
            if (values.size() > 8192) return "[]";
        }
        return values + "]";
    };
    const auto crafts = table(_lstCrafts, 4), salary = table(_lstSalaries, 4);
    const auto upkeep = table(_lstMaintenance, 2), totals = table(_lstTotal, 2);
    std::string buttons = "[";
    const auto label = agentProjectNamedButton(_btnOk, logical, painted, out, allowed);
    if (!label.empty() && label.size() <= 1024) {
        buttons += "{\"id\":\"ok\",\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back("close_monthly_costs", label, "visible_text");
    }
    buttons += "]";
    out.visible = "{\"title\":" + title + ",\"headings\":[" + cost + "," + quantity + "," + total +
        "],\"craft_rental_label\":" + rental + ",\"salaries_label\":" + salaries + ",\"income\":" + income +
        ",\"maintenance\":" + maintenance + ",\"craft_rows\":" + crafts + ",\"salary_rows\":" + salary +
        ",\"maintenance_rows\":" + upkeep + ",\"total_rows\":" + totals + ",\"buttons\":" + buttons + "}";
    return out.visible.size() <= 32768 && agentRegionsDisjoint(out) &&
        (!out.actions.empty() || title != "null" || cost != "null" || quantity != "null" || total != "null" ||
         rental != "null" || salaries != "null" || income != "null" || maintenance != "null" ||
         crafts != "[]" || salary != "[]" || upkeep != "[]" || totals != "[]");
}
bool MonthlyCostsState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;
    if (!agentUiState(fresh) || !fresh.offers(request.action)) return false;
    if (request.action == "close_monthly_costs") { btnOkClick(nullptr); return true; }
    return false;
}
#endif

}
