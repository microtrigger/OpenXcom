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
#include "BuildFacilitiesState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/SavedGame.h"
#include "PlaceFacilityState.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include <typeinfo>
#include "../Engine/Screen.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Build Facilities window.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param state Pointer to the base state to refresh.
 */
BuildFacilitiesState::BuildFacilitiesState(Base *base, State *state) : _base(base), _state(state)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 128, 160, 192, 40, POPUP_VERTICAL);
	_btnOk = new TextButton(112, 16, 200, 176);
	_lstFacilities = new TextList(104, 104, 200, 64);
	_txtTitle = new Text(118, 17, 197, 48);

	// Set palette
	setInterface("selectFacility");

	add(_window, "window", "selectFacility");
	add(_btnOk, "button", "selectFacility");
	add(_txtTitle, "text", "selectFacility");
	add(_lstFacilities, "list", "selectFacility");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getMod()->getSurface("BACK05.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&BuildFacilitiesState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&BuildFacilitiesState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_INSTALLATION"));

	_lstFacilities->setColumns(1, 104);
	_lstFacilities->setSelectable(true);
	_lstFacilities->setBackground(_window);
	_lstFacilities->setMargin(2);
	_lstFacilities->setWordWrap(true);
	_lstFacilities->setScrolling(true, 0);
	_lstFacilities->onMouseClick((ActionHandler)&BuildFacilitiesState::lstFacilitiesClick);

	PopulateBuildList();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    for (Surface *surface : _surfaces) surface->agentTrackPresentation();
    _btnOk->agentEnableNamedControl();
    _lstFacilities->agentEnableFacilityCells();
    if (_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if (_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
#endif
}

/**
 *
 */
BuildFacilitiesState::~BuildFacilitiesState()
{

}

/**
 * Populates the build list from the current "available" facilities.
 */
void BuildFacilitiesState::PopulateBuildList()
{
	const std::vector<std::string> &facilities = _game->getMod()->getBaseFacilitiesList();
	for (std::vector<std::string>::const_iterator i = facilities.begin(); i != facilities.end(); ++i)
	{
		RuleBaseFacility *rule = _game->getMod()->getBaseFacility(*i);
		if (_game->getSavedGame()->isResearched(rule->getRequirements()) && !rule->isLift())
			_facilities.push_back(rule);
	}

	for (std::vector<RuleBaseFacility*>::iterator i = _facilities.begin(); i != _facilities.end(); ++i)
	{
		_lstFacilities->addRow(1, tr((*i)->getType()).c_str());
	}
}

/**
 * The player can change the selected base
 * or change info on other screens.
 */
void BuildFacilitiesState::init()
{
	_state->init();
	State::init();
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void BuildFacilitiesState::btnOkClick(Action *)
{
	_game->popState();
}

/**
 * Places the selected facility.
 * @param action Pointer to an action.
 */
void BuildFacilitiesState::lstFacilitiesClick(Action *)
{
	_game->pushState(new PlaceFacilityState(_base, _facilities[_lstFacilities->getSelectedRow()]));
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace {
// Private readback of the known widget's painted ink, never image discovery or
// exported pixels. Compare current indexed RGB against the logical destination.
bool BuildFacilitiesStateInkCurrent(const Surface *widget, const Surface *logical,
    const Observer::AgentUiState &part)
{
    if (!widget || !logical || !widget->agentPresentationPaintedTo(logical) || part.regions.empty()) return false;
    SDL_Surface *source = widget->getSurface(), *target = logical->getSurface();
    if (!source || !target || source->format->BytesPerPixel != 1 || target->format->BytesPerPixel != 1 ||
        !source->format->palette || !target->format->palette) return false;
    if (Observer::agentObservePaint(SDL_LockSurface(source)) < 0) return false;
    if (Observer::agentObservePaint(SDL_LockSurface(target)) < 0) { SDL_UnlockSurface(source); return false; }
    bool same = true, ink = false; std::size_t work = 0;
    for (const auto &r : part.regions) {
        if (r.x < widget->getX() || r.y < widget->getY() || r.x+r.width > widget->getX()+source->w ||
            r.y+r.height > widget->getY()+source->h || r.x < 0 || r.y < 0 ||
            r.x+r.width > target->w || r.y+r.height > target->h) { same=false; break; }
        for (int y=r.y; same && y<r.y+r.height; ++y) for (int x=r.x; x<r.x+r.width; ++x) {
            if (++work > 131072) { same=false; break; }
            const Uint8 a=static_cast<const Uint8 *>(source->pixels)[(y-widget->getY())*source->pitch+x-widget->getX()];
            if ((source->flags & SDL_SRCCOLORKEY) && a==source->format->colorkey) continue;
            const Uint8 b=static_cast<const Uint8 *>(target->pixels)[y*target->pitch+x];
            if (a>=source->format->palette->ncolors || b>=target->format->palette->ncolors) { same=false; break; }
            const SDL_Color ca=source->format->palette->colors[a], cb=target->format->palette->colors[b];
            ink=true; if (ca.r!=cb.r || ca.g!=cb.g || ca.b!=cb.b) { same=false; break; }
        }
    }
    SDL_UnlockSurface(target); SDL_UnlockSurface(source);
    return same && ink && Observer::agentPaintHealthy();
}
}
bool BuildFacilitiesState::agentDestinationFull() const
{
    const Surface *logical = _game->getScreen()->getSurface();
    const SDL_Surface *destination = logical ? logical->getSurface() : nullptr;
    return destination && destination->w > 0 && destination->h > 0 &&
        destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}
std::vector<std::uint64_t> BuildFacilitiesState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions;
    for (const Surface *surface : Observer::agentFlattenPaintOrder(_surfaces))
        revisions.push_back(surface->agentPresentationRevision());
    revisions.push_back(_txtTitle->agentTextRevision());
    revisions.push_back(_lstFacilities->agentListRevision());
    revisions.push_back(_btnOk->agentLabelRevision());
    return revisions;
}
void BuildFacilitiesState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    for (Surface *surface : Observer::agentFlattenPaintOrder(_surfaces)) surface->agentTrackPresentation();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool BuildFacilitiesState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (typeid(*this) != typeid(BuildFacilitiesState) || _modal || _window->agentPopupActive() || !_base || !_game->isState(const_cast<BuildFacilitiesState *>(this)) || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() || _agentStoredPaintRevisions != agentPaintRevisions()) return false;
    out = AgentUiState(); out.actor = _base; out.screen = "build_facilities";
    const auto painted = agentFlattenPaintOrder(_surfaces);
    const Surface *logical = _game->getScreen()->getSurface();
    for (const Surface *surface : painted) out.widgets.emplace_back(surface, surface->agentPresentationRevision());
    if (_game->getCursor()) out.widgets.emplace_back(_game->getCursor(), _game->getCursor()->agentPresentationRevision());
    if (_game->getFpsCounter()) out.widgets.emplace_back(_game->getFpsCounter(), _game->getFpsCounter()->agentPresentationRevision());
    const auto allowed = [this](const AgentUiState &part) { return _game->agentOverlayClear(part); };
    const auto text = [&](const Text *widget) -> std::string {
        out.widgets.emplace_back(widget, widget->agentTextRevision());
        const auto value = agentTextJson(widget);
        if (value == "null" || !agentWidgetUncovered(widget, painted)) return "null";
        AgentUiState part; agentIncludeWidget(part, widget);
        if (part.regions.empty() || !allowed(part) || !BuildFacilitiesStateInkCurrent(widget, logical, part)) return "null";
        const auto old = out.regions.size();
        out.regions.insert(out.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(out)) return value;
        out.regions.resize(old); return "null";
    };
    const auto title = text(_txtTitle);
    AgentListProjection list;
    std::vector<AgentOutfittingRow> rows;
    if (!_lstFacilities->agentFacilityRows(logical,rows)) return false;
    out.widgets.emplace_back(_lstFacilities,_lstFacilities->agentListRevision());
    // Native clickable row bands are disjoint even when glyph descenders
    // overlap. Caption/selection come from the same ordinary list paint.
    const auto include=[&](const AgentUiState &part,const Surface *owner)->bool {
        if(part.regions.empty() || !agentRegionsUncovered(part.regions,owner,painted) || !allowed(part))return false;
        const auto old=out.regions.size();out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());
        if(agentRegionsDisjoint(out))return true;out.regions.resize(old);return false;
    };
    list.rows="[";
    for(const auto&row:rows){
        if(out.rows.size()==32)break;
        if(row.logicalIndex>=_facilities.size() || row.cells.size()!=1)return false;
        AgentUiState band;band.regions.push_back({row.clickBand.x,row.clickBand.y,row.clickBand.w,row.clickBand.h});
        if(!include(band,_lstFacilities))continue;
        if(!out.rows.empty())list.rows+=",";
        list.rows+="{\"handle\":\"r"+std::to_string(out.rows.size())+"\",\"cells\":["+agentJsonString(row.cells[0].text)+"],\"selected\":"+(row.selected?"true":"false")+"}";
        out.rows.push_back(row.logicalIndex);
    }
    list.rows+="]";
    const Surface *up=nullptr,*down=nullptr;_lstFacilities->agentScrollControls(up,down);
    const auto arrow=[&](const Surface *widget){AgentUiState part;agentIncludeWidget(part,widget);return BuildFacilitiesStateInkCurrent(widget,logical,part)&&include(part,widget);};
    list.up=arrow(up);list.down=arrow(down);
    list.scroll=std::string("{\"up\":")+(list.up?"true":"false")+",\"down\":"+(list.down?"true":"false")+"}";
    for (const auto row : out.rows) {
        if (row >= _facilities.size()) return false;
        out.widgets.emplace_back(_facilities[row], 0);
    }
    if (!out.rows.empty()) out.actions.emplace_back("select_build_facility", "Select visible facility", "visible_control_semantics", "facility_row");
    out.context = {list.up ? 1u : 0u, list.down ? 1u : 0u};
    if (list.up || list.down) out.actions.emplace_back("scroll_visible_list", "Scroll visible list", "visible_control_semantics", "scroll_direction");
    std::string buttons = "[";
    const auto label = agentProjectNamedButton(_btnOk, logical, painted, out, allowed);
    if (!label.empty() && label.size() <= 1024) {
        buttons += "{\"id\":\"close\",\"label\":" + agentJsonString(label) + "}";
        out.actions.emplace_back("close_build_facilities", label, "visible_text");
    }
    buttons += "]";
    out.visible = "{\"title\":" + title + ",\"rows\":" + list.rows + ",\"scroll\":" + list.scroll + ",\"buttons\":" + buttons + "}";
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size() <= 32768;
}
bool BuildFacilitiesState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;
    if (!agentUiState(fresh) || !fresh.offers(request.action)) return false;
    if (request.action == "close_build_facilities") { btnOkClick(nullptr); return true; }
    if (request.action == "select_build_facility") {
        for (std::size_t i = 0; i < fresh.rows.size(); ++i) if (request.choice == "r" + std::to_string(i)) {
            const auto row = fresh.rows[i];
            if (row >= _facilities.size() || !_lstFacilities->agentSelectFacilityRow(_game->getScreen()->getSurface(),row)) return false;
            lstFacilitiesClick(nullptr); return true;
        }
    }
    if (request.action == "scroll_visible_list" && fresh.context.size() == 2) {
        if (request.choice == "up" && fresh.context[0]) { _lstFacilities->scrollUp(false, false); return true; }
        if (request.choice == "down" && fresh.context[1]) { _lstFacilities->scrollDown(false, false); return true; }
    }
    return false;
}
#endif

}
