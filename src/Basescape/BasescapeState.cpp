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
#include "BasescapeState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/TextButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "BaseView.h"
#include "MiniBaseView.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/Region.h"
#include "../Mod/RuleRegion.h"
#include "../Menu/ErrorMessageState.h"
#include "DismantleFacilityState.h"
#include "../Geoscape/BuildNewBaseState.h"
#include "../Engine/Action.h"
#include "BaseInfoState.h"
#include "SoldiersState.h"
#include "CraftsState.h"
#include "BuildFacilitiesState.h"
#include "ResearchState.h"
#include "ManageAlienContainmentState.h"
#include "ManufactureState.h"
#include "PurchaseState.h"
#include "SellState.h"
#include "TransferBaseState.h"
#include "CraftInfoState.h"
#include "../Geoscape/AllocatePsiTrainingState.h"
#include "../Mod/RuleInterface.h"

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include <algorithm>
#include "../Observer/AgentProjectileOutput.h"
#include "../Engine/Screen.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#endif

namespace OpenXcom
{

/**
 * Initializes all the elements in the Basescape screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param globe Pointer to the Geoscape globe.
 */
BasescapeState::BasescapeState(Base *base, Globe *globe) : _base(base), _globe(globe)
{
	// Create objects
	_txtFacility = new Text(192, 9, 0, 0);
	_view = new BaseView(192, 192, 0, 8);
	_mini = new MiniBaseView(128, 16, 192, 41);
	_edtBase = new TextEdit(this, 127, 17, 193, 0);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	_edtBase->agentEnableBasescapeName();
#endif
	_txtLocation = new Text(126, 9, 194, 16);
	_txtFunds = new Text(126, 9, 194, 24);
	_btnNewBase = new TextButton(128, 12, 192, 58);
	_btnBaseInfo = new TextButton(128, 12, 192, 71);
	_btnSoldiers = new TextButton(128, 12, 192, 84);
	_btnCrafts = new TextButton(128, 12, 192, 97);
	_btnFacilities = new TextButton(128, 12, 192, 110);
	_btnResearch = new TextButton(128, 12, 192, 123);
	_btnManufacture = new TextButton(128, 12, 192, 136);
	_btnTransfer = new TextButton(128, 12, 192, 149);
	_btnPurchase = new TextButton(128, 12, 192, 162);
	_btnSell = new TextButton(128, 12, 192, 175);
	_btnGeoscape = new TextButton(128, 12, 192, 188);

	// Set palette
	setInterface("basescape");

	add(_view, "baseView", "basescape");
	add(_mini, "miniBase", "basescape");
	add(_txtFacility, "textTooltip", "basescape");
	add(_edtBase, "text1", "basescape");
	add(_txtLocation, "text2", "basescape");
	add(_txtFunds, "text3", "basescape");
	add(_btnNewBase, "button", "basescape");
	add(_btnBaseInfo, "button", "basescape");
	add(_btnSoldiers, "button", "basescape");
	add(_btnCrafts, "button", "basescape");
	add(_btnFacilities, "button", "basescape");
	add(_btnResearch, "button", "basescape");
	add(_btnManufacture, "button", "basescape");
	add(_btnTransfer, "button", "basescape");
	add(_btnPurchase, "button", "basescape");
	add(_btnSell, "button", "basescape");
	add(_btnGeoscape, "button", "basescape");

	centerAllSurfaces();

	// Set up objects
	_view->setTexture(_game->getMod()->getSurfaceSet("BASEBITS.PCK"));
	_view->onMouseClick((ActionHandler)&BasescapeState::viewLeftClick, SDL_BUTTON_LEFT);
	_view->onMouseClick((ActionHandler)&BasescapeState::viewRightClick, SDL_BUTTON_RIGHT);
	_view->onMouseOver((ActionHandler)&BasescapeState::viewMouseOver);
	_view->onMouseOut((ActionHandler)&BasescapeState::viewMouseOut);

	_mini->setTexture(_game->getMod()->getSurfaceSet("BASEBITS.PCK"));
	_mini->setBases(_game->getSavedGame()->getBases());
	_mini->onMouseClick((ActionHandler)&BasescapeState::miniClick);
	_mini->onKeyboardPress((ActionHandler)&BasescapeState::handleKeyPress);

	_edtBase->setBig();
	_edtBase->onChange((ActionHandler)&BasescapeState::edtBaseChange);

	_btnNewBase->setText(tr("STR_BUILD_NEW_BASE_UC"));
	_btnNewBase->onMouseClick((ActionHandler)&BasescapeState::btnNewBaseClick);

	_btnBaseInfo->setText(tr("STR_BASE_INFORMATION"));
	_btnBaseInfo->onMouseClick((ActionHandler)&BasescapeState::btnBaseInfoClick);

	_btnSoldiers->setText(tr("STR_SOLDIERS_UC"));
	_btnSoldiers->onMouseClick((ActionHandler)&BasescapeState::btnSoldiersClick);

	_btnCrafts->setText(tr("STR_EQUIP_CRAFT"));
	_btnCrafts->onMouseClick((ActionHandler)&BasescapeState::btnCraftsClick);

	_btnFacilities->setText(tr("STR_BUILD_FACILITIES"));
	_btnFacilities->onMouseClick((ActionHandler)&BasescapeState::btnFacilitiesClick);

	_btnResearch->setText(tr("STR_RESEARCH"));
	_btnResearch->onMouseClick((ActionHandler)&BasescapeState::btnResearchClick);

	_btnManufacture->setText(tr("STR_MANUFACTURE"));
	_btnManufacture->onMouseClick((ActionHandler)&BasescapeState::btnManufactureClick);

	_btnTransfer->setText(tr("STR_TRANSFER_UC"));
	_btnTransfer->onMouseClick((ActionHandler)&BasescapeState::btnTransferClick);

	_btnPurchase->setText(tr("STR_PURCHASE_RECRUIT"));
	_btnPurchase->onMouseClick((ActionHandler)&BasescapeState::btnPurchaseClick);

	_btnSell->setText(tr("STR_SELL_SACK_UC"));
	_btnSell->onMouseClick((ActionHandler)&BasescapeState::btnSellClick);

	_btnGeoscape->setText(tr("STR_GEOSCAPE_UC"));
	_btnGeoscape->onMouseClick((ActionHandler)&BasescapeState::btnGeoscapeClick);
	_btnGeoscape->onKeyboardPress((ActionHandler)&BasescapeState::btnGeoscapeClick, Options::keyCancel);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _view->agentEnableFacilitySelection();
    for(Surface *surface:_surfaces) surface->agentTrackPresentation();
    for(TextButton *button:{_btnNewBase,_btnBaseInfo,_btnFacilities,_btnSoldiers,_btnCrafts,_btnResearch,_btnManufacture,_btnPurchase,_btnSell,_btnTransfer,_btnGeoscape})
        button->agentEnableNamedControl();
    if(_game->getCursor()) _game->getCursor()->agentTrackPresentation();
    if(_game->getFpsCounter()) _game->getFpsCounter()->agentTrackPresentation();
#endif
}

/**
 *
 */
BasescapeState::~BasescapeState()
{
	// Clean up any temporary bases
	bool exists = false;
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end() && !exists; ++i)
	{
		if (*i == _base)
		{
			exists = true;
			break;
		}
	}
	if (!exists)
	{
		delete _base;
	}
}

/**
 * The player can change the selected base
 * or change info on other screens.
 */
void BasescapeState::init()
{
	State::init();

	setBase(_base);
	_view->setBase(_base);
	_mini->draw();
	_edtBase->setText(_base->getName());

	// Get area
	for (std::vector<Region*>::iterator i = _game->getSavedGame()->getRegions()->begin(); i != _game->getSavedGame()->getRegions()->end(); ++i)
	{
		if ((*i)->getRules()->insideRegion(_base->getLongitude(), _base->getLatitude()))
		{
			_txtLocation->setText(tr((*i)->getRules()->getType()));
			break;
		}
	}

	_txtFunds->setText(tr("STR_FUNDS").arg(Unicode::formatFunding(_game->getSavedGame()->getFunds())));

	_btnNewBase->setVisible(_game->getSavedGame()->getBases()->size() < MiniBaseView::MAX_BASES);
}

/**
 * Changes the base currently displayed on screen.
 * @param base Pointer to new base to display.
 */
void BasescapeState::setBase(Base *base)
{
	if (!_game->getSavedGame()->getBases()->empty())
	{
		// Check if base still exists
		bool exists = false;
		for (size_t i = 0; i < _game->getSavedGame()->getBases()->size(); ++i)
		{
			if (_game->getSavedGame()->getBases()->at(i) == base)
			{
				_base = base;
				_mini->setSelectedBase(i);
				_game->getSavedGame()->setSelectedBase(i);
				exists = true;
				break;
			}
		}
		// If base was removed, select first one
		if (!exists)
		{
			_base = _game->getSavedGame()->getBases()->front();
			_mini->setSelectedBase(0);
			_game->getSavedGame()->setSelectedBase(0);
		}
	}
	else
	{
		// Use a blank base for special case when player has no bases
		_base = new Base(_game->getMod());
		_mini->setSelectedBase(0);
		_game->getSavedGame()->setSelectedBase(0);
	}
}

/**
 * Goes to the Build New Base screen.
 * @param action Pointer to an action.
 */
void BasescapeState::btnNewBaseClick(Action *)
{
	Base *base = new Base(_game->getMod());
	_game->popState();
	_game->pushState(new BuildNewBaseState(base, _globe, false));
}

/**
 * Goes to the Base Info screen.
 * @param action Pointer to an action.
 */
void BasescapeState::btnBaseInfoClick(Action *)
{
	_game->pushState(new BaseInfoState(_base, this));
}

/**
 * Goes to the Soldiers screen.
 * @param action Pointer to an action.
 */
void BasescapeState::btnSoldiersClick(Action *)
{
	_game->pushState(new SoldiersState(_base));
}

/**
 * Goes to the Crafts screen.
 * @param action Pointer to an action.
 */
void BasescapeState::btnCraftsClick(Action *)
{
	_game->pushState(new CraftsState(_base));
}

/**
 * Opens the Build Facilities window.
 * @param action Pointer to an action.
 */
void BasescapeState::btnFacilitiesClick(Action *)
{
	_game->pushState(new BuildFacilitiesState(_base, this));
}

/**
 * Goes to the Research screen.
 * @param action Pointer to an action.
 */
void BasescapeState::btnResearchClick(Action *)
{
	_game->pushState(new ResearchState(_base));
}

/**
 * Goes to the Manufacture screen.
 * @param action Pointer to an action.
 */
void BasescapeState::btnManufactureClick(Action *)
{
	_game->pushState(new ManufactureState(_base));
}

/**
 * Goes to the Purchase screen.
 * @param action Pointer to an action.
 */
void BasescapeState::btnPurchaseClick(Action *)
{
	_game->pushState(new PurchaseState(_base));
}

/**
 * Goes to the Sell screen.
 * @param action Pointer to an action.
 */
void BasescapeState::btnSellClick(Action *)
{
	_game->pushState(new SellState(_base));
}

/**
 * Goes to the Select Destination Base window.
 * @param action Pointer to an action.
 */
void BasescapeState::btnTransferClick(Action *)
{
	_game->pushState(new TransferBaseState(_base));
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void BasescapeState::btnGeoscapeClick(Action *)
{
	_game->popState();
}

/**
 * Processes clicking on facilities.
 * @param action Pointer to an action.
 */
void BasescapeState::viewLeftClick(Action *)
{
	BaseFacility *fac = _view->getSelectedFacility();
	if (fac != 0)
	{
		// Is facility in use?
		if (fac->inUse())
		{
			_game->pushState(new ErrorMessageState(tr("STR_FACILITY_IN_USE"), _palette, _game->getMod()->getInterface("basescape")->getElement("errorMessage")->color, "BACK13.SCR", _game->getMod()->getInterface("basescape")->getElement("errorPalette")->color));
		}
		// Would base become disconnected?
		else if (!_base->getDisconnectedFacilities(fac).empty())
		{
			_game->pushState(new ErrorMessageState(tr("STR_CANNOT_DISMANTLE_FACILITY"), _palette, _game->getMod()->getInterface("basescape")->getElement("errorMessage")->color, "BACK13.SCR", _game->getMod()->getInterface("basescape")->getElement("errorPalette")->color));
		}
		else
		{
			_game->pushState(new DismantleFacilityState(_base, _view, fac));
		}
	}
}

/**
 * Processes right clicking on facilities.
 * @param action Pointer to an action.
 */
void BasescapeState::viewRightClick(Action *)
{
	BaseFacility *f = _view->getSelectedFacility();
	if (f == 0)
	{
		_game->pushState(new BaseInfoState(_base, this));
	}
	else if (f->getRules()->getCrafts() > 0)
	{
		if (f->getCraft() == 0)
		{
			_game->pushState(new CraftsState(_base));
		}
		else
			for (size_t craft = 0; craft < _base->getCrafts()->size(); ++craft)
			{
				if (f->getCraft() == _base->getCrafts()->at(craft))
				{
					_game->pushState(new CraftInfoState(_base, craft));
					break;
				}
			}
	}
	else if (f->getRules()->getStorage() > 0)
	{
		_game->pushState(new SellState(_base));
	}
	else if (f->getRules()->getPersonnel() > 0)
	{
		_game->pushState(new SoldiersState(_base));
	}
	else if (f->getRules()->getPsiLaboratories() > 0 && Options::anytimePsiTraining && _base->getAvailablePsiLabs() > 0)
	{
		_game->pushState(new AllocatePsiTrainingState(_base));
	}
	else if (f->getRules()->getLaboratories() > 0)
	{
		_game->pushState(new ResearchState(_base));
	}
	else if (f->getRules()->getWorkshops() > 0)
	{
		_game->pushState(new ManufactureState(_base));
	}
	else if (f->getRules()->getAliens() > 0)
	{
		_game->pushState(new ManageAlienContainmentState(_base, OPT_GEOSCAPE));
	}
	else if (f->getRules()->isLift() || f->getRules()->getRadarRange() > 0)
	{
		_game->popState();
	}
}

/**
 * Displays the name of the facility the mouse is over.
 * @param action Pointer to an action.
 */
void BasescapeState::viewMouseOver(Action *)
{
	BaseFacility *f = _view->getSelectedFacility();
	std::ostringstream ss;
	if (f != 0)
	{
		if (f->getRules()->getCrafts() == 0 || f->getBuildTime() > 0)
		{
			ss << tr(f->getRules()->getType());
		}
		else
		{
			ss << tr(f->getRules()->getType());
			if (f->getCraft() != 0)
			{
				ss << " " << tr("STR_CRAFT_").arg(f->getCraft()->getName(_game->getLanguage()));
			}
		}
	}
	_txtFacility->setText(ss.str());
}

/**
 * Clears the facility name.
 * @param action Pointer to an action.
 */
void BasescapeState::viewMouseOut(Action *)
{
	_txtFacility->setText("");
}

/**
 * Selects a new base to display.
 * @param action Pointer to an action.
 */
void BasescapeState::miniClick(Action *)
{
	size_t base = _mini->getHoveredBase();
	if (base < _game->getSavedGame()->getBases()->size())
	{
		_base = _game->getSavedGame()->getBases()->at(base);
		init();
	}
}

/**
 * Selects a new base to display.
 * @param action Pointer to an action.
 */
void BasescapeState::handleKeyPress(Action *action)
{
	if (action->getDetails()->type == SDL_KEYDOWN)
	{
		SDLKey baseKeys[] = {Options::keyBaseSelect1,
			                 Options::keyBaseSelect2,
			                 Options::keyBaseSelect3,
			                 Options::keyBaseSelect4,
			                 Options::keyBaseSelect5,
			                 Options::keyBaseSelect6,
			                 Options::keyBaseSelect7,
			                 Options::keyBaseSelect8};
		int key = action->getDetails()->key.keysym.sym;
		for (size_t i = 0; i < _game->getSavedGame()->getBases()->size(); ++i)
		{
			if (key == baseKeys[i])
			{
				_base = _game->getSavedGame()->getBases()->at(i);
				init();
				break;
			}
		}
	}
}

/**
 * Changes the Base name.
 * @param action Pointer to an action.
 */
void BasescapeState::edtBaseChange(Action *)
{
	_base->setName(_edtBase->getText());
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool BasescapeState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (_modal || !_game->isState(const_cast<BasescapeState *>(this)) || !agentPaintHealthy()) return false;
    out=AgentUiState(); out.screen="basescape"; out.actor=_base;
    const auto painted=agentFlattenPaintOrder(_surfaces);
    const Surface *logical=_game->getScreen()->getSurface();
    for(const Surface *surface:painted) if(surface) out.widgets.emplace_back(surface,surface->agentPresentationRevision());
    if(_game->getCursor()) out.widgets.emplace_back(_game->getCursor(),_game->getCursor()->agentPresentationRevision());
    if(_game->getFpsCounter()) out.widgets.emplace_back(_game->getFpsCounter(),_game->getFpsCounter()->agentPresentationRevision());
    const auto include=[&](const Surface *widget)->bool
    {
        if (!agentWidgetUncovered(widget,painted)) return false;
        AgentUiState part; agentIncludeWidget(part,widget);
        if (part.regions.empty() || !_game->agentOverlayClear(part)) return false;
        const auto old=out.regions.size();
        out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());
        if (!agentRegionsDisjoint(out)) { out.regions.resize(old); return false; }
        return true;
    };
    const auto text=[&](const Text *widget)->std::string
    {
        const auto value=agentTextJson(widget);
        return value!="null" && include(widget) ? value : "null";
    };
    std::string name="null",shown;
    if (_edtBase)
    {
        SDL_Rect allocation;
        std::uint64_t captureEpoch=0;
        const bool captured=_edtBase->agentBasescapeName(logical,shown,allocation,captureEpoch);
        out.widgets.push_back({_edtBase,_edtBase->agentInputRevision()});
        // Keep full editor geometry in authority; only the observation strip
        // excludes the native 17px allocation's bottom spare row.
        out.context.insert(out.context.end(), {
            captureEpoch,static_cast<std::uint64_t>(_edtBase->getX()),
            static_cast<std::uint64_t>(_edtBase->getY()),
            static_cast<std::uint64_t>(_edtBase->getWidth()),
            static_cast<std::uint64_t>(_edtBase->getHeight())});
        if (captured)
        {
            AgentUiState part;
            const int observedHeight=allocation.h==17 ? 16 : allocation.h;
            part.regions.push_back({allocation.x,allocation.y,allocation.w,observedHeight});
            bool found=false,uncovered=true;
            for (const Surface *later:painted)
            {
                if (later==_edtBase) { found=true; continue; }
                if (!found || !later || !later->getVisible()) continue;
                const auto &rect=part.regions.front();
                if (rect.x<later->getX()+later->getWidth() && rect.x+rect.width>later->getX() &&
                    rect.y<later->getY()+later->getHeight() && rect.y+rect.height>later->getY()) uncovered=false;
            }
            if (found && uncovered && _game->agentNonCursorOverlayClear(part)) name=agentJsonString(shown);
        }
        // Logical name observation is not an outward action region. Existing
        // button/slot cursor guards and all-surface authority bindings remain.
    }
    std::string slots="[";
    if (_mini)
    {
        out.widgets.push_back({_mini,_mini->agentRevision()});
        AgentProjectileOutput output;
        if (agentWidgetVisible(_mini) && _game->getScreen()->agentCaptureOutput(_mini,output))
            for (const auto &slot:_mini->agentSlots(output))
            {
                AgentUiState part; part.regions.push_back({_mini->getX()+int(slot.slot)*16,_mini->getY(),16,16});
                if (!agentRegionsUncovered(part.regions,_mini,painted) || !_game->agentOverlayClear(part)) continue;
                const auto old=out.regions.size(); out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());
                if (!agentRegionsDisjoint(out)) { out.regions.resize(old); continue; }
                if (!out.rows.empty()) slots+=",";
                slots+="{\"handle\":\"b"+std::to_string(out.rows.size())+"\",\"selected\":"+(slot.selected?"true":"false")+"}";
                out.rows.push_back(slot.slot);
            }
    }
    slots+="]";
    if (!out.rows.empty()) out.actions.emplace_back("select_base","Select base","visible_control_semantics","base_target");
    std::string grid="null";
    std::size_t cellCount=0;
    if(_view->agentFacilityPresented(logical)) {
        std::string cells="[";
        for(int y=0;y<6;++y)for(int x=0;x<6;++x){
            // Authority is the actual ordinary click target, not the whole
            // decorative cell border. Native hover labels may overlap that
            // border while leaving its center fully visible and interactive.
            AgentUiState cell;cell.regions.push_back({_view->getX()+32*x+16,_view->getY()+32*y+16,1,1});
            if(!agentRegionsUncovered(cell.regions,_view,painted) || !_game->agentOverlayClear(cell))continue;
            const auto old=out.regions.size();out.regions.insert(out.regions.end(),cell.regions.begin(),cell.regions.end());
            if(!agentRegionsDisjoint(out)){out.regions.resize(old);continue;}
            if(cells.size()>1)cells+=",";
            cells+="{\"x\":"+std::to_string(x)+",\"y\":"+std::to_string(y)+"}";
            out.context.push_back(y*6+x);++cellCount;
        }
        cells+="]";
        if(cellCount) {
            grid="{\"columns\":6,\"rows\":6,\"origin_x\":"+std::to_string(_view->getX())+",\"origin_y\":"+std::to_string(_view->getY())+",\"cell_size\":32,\"cells\":"+cells+"}";
            out.actions.emplace_back("click_base_facility_cell","Select visible base facility","visible_control_semantics","facility_cell");
        }
    }
    // Trailing count delimits cell indices from existing name authority metadata.
    out.context.push_back(cellCount);
    std::string buttons="[";
    const auto button=[&](TextButton *widget,const char *id,const char *action)
    {
        const auto label=agentProjectNamedButton(widget,logical,painted,out,
            [&](const AgentUiState &part){return _game->agentOverlayClear(part);});
        if (label.empty() || label.size()>1024) return;
        if (buttons.size()>1) buttons+=",";
        buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";
        out.actions.emplace_back(action,label,"visible_text");
    };
    const auto location=text(_txtLocation),funds=text(_txtFunds);
    button(_btnNewBase,"new_base","open_new_base");
    button(_btnBaseInfo,"base_info","open_base_info");
    button(_btnFacilities,"build_facilities","open_build_facilities");
    button(_btnResearch,"research","open_research");
    button(_btnManufacture,"manufacture","open_manufacture");
    button(_btnPurchase,"purchase","open_purchase");
    button(_btnSell,"sell","open_sell");
    button(_btnTransfer,"transfer","open_transfer");
    button(_btnSoldiers,"soldiers","open_soldiers");
    button(_btnCrafts,"crafts","open_crafts");
    button(_btnGeoscape,"geoscape","return_geoscape"); buttons+="]";
    out.visible="{\"base_name\":"+name+",\"location\":"+location+",\"funds\":"+funds+",\"base_slots\":"+slots+",\"grid\":"+grid+",\"buttons\":"+buttons+"}";
    return !out.regions.empty() && agentRegionsDisjoint(out) && out.visible.size()<=32768;
}

bool BasescapeState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState fresh;
    if (!agentUiState(fresh) || !fresh.offers(request.action)) return false;
    if(request.action=="click_base_facility_cell") {
        if(request.x<0 || request.y<0 || request.x>5 || request.y>5 || fresh.context.empty())return false;
        const auto count=fresh.context.back();
        if(count>36 || count>=fresh.context.size())return false;
        const auto end=fresh.context.end()-1,begin=end-count;
        if(std::find(begin,end,request.y*6+request.x)==end)return false;
        return _view->agentFacilityClick(this,request.x,request.y);
    }
    if (request.action=="select_base")
    {
        if (request.choice.size()!=2 || request.choice[0]!='b' || request.choice[1]<'0' || request.choice[1]>'7') return false;
        const unsigned index=request.choice[1]-'0';
        if (index>=fresh.rows.size()) return false;
        _mini->hoverAt(int(fresh.rows[index])*16+8); miniClick(nullptr); return true;
    }
    if (request.action=="open_new_base") { btnNewBaseClick(nullptr); return true; }
    if (request.action=="open_base_info") { btnBaseInfoClick(nullptr); return true; }
    if (request.action=="open_build_facilities") { btnFacilitiesClick(nullptr); return true; }
    if (request.action=="open_research") { btnResearchClick(nullptr); return true; }
    if (request.action=="open_manufacture") { btnManufactureClick(nullptr); return true; }
    if (request.action=="open_purchase") { btnPurchaseClick(nullptr); return true; }
    if (request.action=="open_sell") { btnSellClick(nullptr); return true; }
    if (request.action=="open_transfer") { btnTransferClick(nullptr); return true; }
    if (request.action=="open_soldiers") { btnSoldiersClick(nullptr); return true; }
    if (request.action=="open_crafts") { btnCraftsClick(nullptr); return true; }
    if (request.action=="return_geoscape") { btnGeoscapeClick(nullptr); return true; }
    return false;
}
#endif

}
