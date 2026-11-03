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
#include "GeoscapeState.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include "../Engine/RNG.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Screen.h"
#include "../Engine/Surface.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "Globe.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Engine/Timer.h"
#include "../Savegame/GameTime.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#include "../Savegame/Craft.h"
#include "../Mod/RuleCraft.h"
#include "../Savegame/Ufo.h"
#include "../Mod/RuleUfo.h"
#include "../Mod/RuleMissionScript.h"
#include "../Savegame/Waypoint.h"
#include "../Savegame/Transfer.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDiary.h"
#include "../Menu/PauseState.h"
#include "InterceptState.h"
#include "../Basescape/BasescapeState.h"
#include "../Basescape/SellState.h"
#include "../Menu/CutsceneState.h"
#include "../Menu/ErrorMessageState.h"
#include "GraphsState.h"
#include "FundingState.h"
#include "MonthlyReportState.h"
#include "ProductionCompleteState.h"
#include "UfoDetectedState.h"
#include "GeoscapeCraftState.h"
#include "DogfightState.h"
#include "UfoLostState.h"
#include "CraftPatrolState.h"
#include "LowFuelState.h"
#include "MultipleTargetsState.h"
#include "ConfirmLandingState.h"
#include "ItemsArrivingState.h"
#include "CraftErrorState.h"
#include "DogfightErrorState.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Savegame/ResearchProject.h"
#include "ResearchCompleteState.h"
#include "../Mod/RuleResearch.h"
#include "ResearchRequiredState.h"
#include "NewPossibleResearchState.h"
#include "NewPossibleManufactureState.h"
#include "../Savegame/Production.h"
#include "../Mod/RuleManufacture.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/MissionSite.h"
#include "../Savegame/AlienBase.h"
#include "../Mod/RuleRegion.h"
#include "MissionDetectedState.h"
#include "AlienBaseState.h"
#include "../Savegame/Region.h"
#include "../Savegame/Country.h"
#include "../Mod/RuleCountry.h"
#include "../Mod/RuleAlienMission.h"
#include "../Savegame/AlienStrategy.h"
#include "../Savegame/AlienMission.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Battlescape/BattlescapeGenerator.h"
#include "../Battlescape/BriefingState.h"
#include "../Mod/UfoTrajectory.h"
#include "../Mod/Armor.h"
#include "BaseDefenseState.h"
#include "BaseDestroyedState.h"
#include "../Menu/LoadGameState.h"
#include "../Menu/SaveGameState.h"
#include "../Menu/ListSaveState.h"
#include "../Mod/RuleGlobe.h"
#include "../Engine/Exception.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/RuleInterface.h"
#include "../fmath.h"


#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentGlobeInput.h"
#include "../Observer/AgentGlobeGlyphs.h"
#include <map>
#include <array>
#include <stdexcept>
#include <limits>
#include <type_traits>
namespace {
struct AgentGlobeUiEvent { std::uint64_t sequence; const char *action; const char *kind; const char *reason; std::uint64_t ticks; bool changed; };
struct AgentGlobeAudit {
    std::array<AgentGlobeUiEvent,32> events; unsigned size=0;
    std::uint64_t nextSequence=0,attempted=0,omitted=0;
    bool overflow=false,closed=false,countersSaturated=false;
};
struct AgentGlobePulse {
    OpenXcom::Game *game; OpenXcom::GeoscapeState *owner; OpenXcom::Globe *globe;
    void (OpenXcom::GeoscapeState::*release)(OpenXcom::Action *);
    std::uint64_t revision; Uint32 started; std::uint64_t sequence,ticks; const char *action;
};
std::map<OpenXcom::Game *,AgentGlobePulse> agentGlobePulses;
std::map<OpenXcom::Game *,AgentGlobeAudit> agentGlobeAudits;
// Trace action/kind/reason pointers always refer to fixed string literals.
// Pulse copies and retained-slot writes cannot allocate before release.
static_assert(std::is_nothrow_copy_constructible<AgentGlobePulse>::value,"owned pulse copy must not throw");
static_assert(std::is_nothrow_copy_assignable<AgentGlobeUiEvent>::value,"trace slot write must not throw");
void globeIncrement(std::uint64_t &value,AgentGlobeAudit &audit) noexcept {
    if(value==std::numeric_limits<std::uint64_t>::max())audit.countersSaturated=true;
    else ++value;
}
std::uint64_t globeNextSequence(AgentGlobeAudit &audit) noexcept {
    if(audit.nextSequence==std::numeric_limits<std::uint64_t>::max()){
        audit.countersSaturated=true;return 0; // No reused trace identity.
    }
    return ++audit.nextSequence;
}
void globeRecord(OpenXcom::Game *game,std::uint64_t sequence,const char *action,
                 const char *kind,const char *reason,std::uint64_t ticks,bool changed) noexcept {
    // Dispatch creates this sidecar before native input; cleanup never allocates.
    auto it=agentGlobeAudits.find(game);if(it==agentGlobeAudits.end())return;
    auto &audit=it->second;globeIncrement(audit.attempted,audit);
    if(audit.size==audit.events.size() || sequence==0){
        audit.overflow=true;globeIncrement(audit.omitted,audit);return;
    }
    audit.events[audit.size]={sequence,action,kind,reason,ticks,changed};
    ++audit.size; // Publish only after the nonthrowing complete slot assignment.
}
void globeZoomReturned(OpenXcom::Game *game,const char *action) {
    auto &audit=agentGlobeAudits.at(game);audit.closed=false;
    globeRecord(game,globeNextSequence(audit),action,"zoom_callback_return","",0,false);
}
bool globeInputFocused() {
    const Uint8 flags=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;
    return (SDL_GetAppState()&flags)==flags && SDL_GetModState()==KMOD_NONE && SDL_GetMouseState(nullptr,nullptr)==0;
}
const char *globeReason(OpenXcom::Observer::AgentGlobeCancelReason reason) {
    using R=OpenXcom::Observer::AgentGlobeCancelReason;
    switch(reason){case R::StateChange:return "state_change";case R::FocusLost:return "focus_lost";case R::Deadline:return "deadline";case R::ManualInput:return "manual_input";case R::Init:return "init";case R::Destroyed:return "destroyed";case R::Modal:return "modal";case R::ViewTick:return "view_tick";case R::ConnectionClosed:return "connection_closed";case R::Quit:return "quit";}
    return "unknown";
}
}
namespace OpenXcom { namespace Observer {
bool agentGlobeInputHeld(Game *game) { return agentGlobePulses.count(game)!=0; }
bool agentGlobeInputIdle(Game *game) { return !agentGlobeInputHeld(game); }
void agentCancelGlobeInput(Game *game,AgentGlobeCancelReason reason) {
    auto it=agentGlobePulses.find(game); if(it==agentGlobePulses.end())return;
    const AgentGlobePulse owned=it->second;
    const bool changed=owned.globe->agentViewRevision()!=owned.revision;
    agentGlobePulses.erase(it); // Relinquish BEFORE release; never stop later manual input.
    (owned.owner->*owned.release)(nullptr);
    globeRecord(game,owned.sequence,owned.action,"release_return",globeReason(reason),owned.ticks,changed);
}
void agentCancelGlobeInputOwner(State *owner,AgentGlobeCancelReason reason) {
    for(auto it=agentGlobePulses.begin();it!=agentGlobePulses.end();++it)
        if(it->second.owner==owner){ Game *game=it->first;agentCancelGlobeInput(game,reason);return; }
}
void agentPollGlobeInput(Game *game) {
    auto it=agentGlobePulses.find(game); if(it==agentGlobePulses.end())return;
    if(game->agentTopState()!=it->second.owner)agentCancelGlobeInput(game,AgentGlobeCancelReason::StateChange);
    else if(!globeInputFocused())agentCancelGlobeInput(game,AgentGlobeCancelReason::FocusLost);
    else if(static_cast<Uint32>(SDL_GetTicks()-it->second.started)>=250)agentCancelGlobeInput(game,AgentGlobeCancelReason::Deadline);
}
void agentOwnedGlobeRotationTick(Globe *globe) {
    for(auto &entry:agentGlobePulses)if(entry.second.globe==globe){
        auto &pulse=entry.second;globeIncrement(pulse.ticks,agentGlobeAudits.at(entry.first));
        globeRecord(entry.first,pulse.sequence,pulse.action,"ordinary_rotate_entry","",pulse.ticks,false);return;
    }
}
void agentCloseGlobeInput(Game *game) {
    agentCancelGlobeInput(game,AgentGlobeCancelReason::ConnectionClosed);
    auto it=agentGlobeAudits.find(game);if(it!=agentGlobeAudits.end())it->second.closed=true;
}
void agentForgetGlobeInput(Game *game) {
    agentCancelGlobeInput(game,AgentGlobeCancelReason::Destroyed);agentGlobeAudits.erase(game);
}
std::string agentGlobeInputAuditAfterEOF(Game *game) {
    auto it=agentGlobeAudits.find(game);
    if(agentGlobeInputHeld(game) || (it!=agentGlobeAudits.end()&&!it->second.closed))throw std::runtime_error("UI audit before owned transport close");
    std::string events="[";
    if(it!=agentGlobeAudits.end())for(unsigned i=0;i<it->second.size;++i){
        const auto &e=it->second.events[i];if(i)events+=",";
        events+="{\"sequence\":"+agentJsonString(std::to_string(e.sequence))+",\"action\":"+agentJsonString(e.action)+",\"kind\":"+agentJsonString(e.kind)+",\"reason\":"+agentJsonString(e.reason)+",\"ticks\":"+agentJsonString(std::to_string(e.ticks))+",\"view_revision_changed\":"+(e.changed?"true":"false")+"}";
    }
    events+="]";
    const AgentGlobeAudit empty{};const auto &audit=it==agentGlobeAudits.end()?empty:it->second;
    const bool complete=!audit.overflow && !audit.countersSaturated && audit.omitted==0;
    return "{\"schema\":\"openxcom-globe-input-audit/2\",\"scope\":\"game_lifetime_prefix\",\"capacity\":32,\"retained_events\":"+std::to_string(audit.size)+
        ",\"attempted_events\":"+agentJsonString(std::to_string(audit.attempted))+",\"omitted_events\":"+agentJsonString(std::to_string(audit.omitted))+
        ",\"counters_saturated\":"+(audit.countersSaturated?"true":"false")+",\"complete\":"+(complete?"true":"false")+
        ",\"post_eof_only\":true,\"owned_input_cleared\":true,\"overflow\":"+(audit.overflow?"true":"false")+",\"motion_proven\":false,\"events\":"+events+"}";
}
} }

#endif

namespace OpenXcom
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
// Scoped to the background controls exposed under minimized interceptions.
class AgentGeoscapeFightButton : public TextButton
{
public:
    AgentGeoscapeFightButton(int w,int h,int x,int y):TextButton(w,h,x,y){}
    void blit(Surface *destination) override {
        const auto full=[](const SDL_Surface *s){return s && s->clip_rect.x==0 && s->clip_rect.y==0 && s->clip_rect.w==s->w && s->clip_rect.h==s->h;};
        const SDL_Rect *crop=getCrop();
        if(crop->x||crop->y||crop->w||crop->h||!full(getSurface())||!destination||!full(destination->getSurface()))agentPresentationMutation();
        TextButton::blit(destination);
    }
};
#else
using AgentGeoscapeFightButton=TextButton;
#endif


/**
 * Initializes all the elements in the Geoscape screen.
 * @param game Pointer to the core game.
 */
GeoscapeState::GeoscapeState() : _pause(false), _zoomInEffectDone(false), _zoomOutEffectDone(false), _minimizedDogfights(0)
{
	int screenWidth = Options::baseXGeoscape;
	int screenHeight = Options::baseYGeoscape;

	// Create objects
	Surface *hd = _game->getMod()->getSurface("ALTGEOBORD.SCR");
	_bg = new Surface(hd->getWidth(), hd->getHeight(), 0, 0);
	_sideLine = new Surface(64, screenHeight, screenWidth - 64, 0);
	_sidebar = new Surface(64, 200, screenWidth - 64, screenHeight / 2 - 100);

	_globe = new Globe(_game, (screenWidth-64)/2, screenHeight/2, screenWidth-64, screenHeight, 0, 0);
	_bg->setX((_globe->getWidth() - _bg->getWidth()) / 2);
	_bg->setY((_globe->getHeight() - _bg->getHeight()) / 2);

	_btnIntercept = new TextButton(63, 11, screenWidth-63, screenHeight/2-100);
	_btnBases = new TextButton(63, 11, screenWidth-63, screenHeight/2-88);
	_btnGraphs = new TextButton(63, 11, screenWidth-63, screenHeight/2-76);
	_btnUfopaedia = new TextButton(63, 11, screenWidth-63, screenHeight/2-64);
	_btnOptions = new AgentGeoscapeFightButton(63, 11, screenWidth-63, screenHeight/2-52);
	_btnFunding = new TextButton(63, 11, screenWidth-63, screenHeight/2-40);

	// Leave full nine-pixel calendar rows above the speed controls.
	// Twelve-pixel buttons still fit the stock geo font and stop before the
	// fixed globe rotation/zoom artwork and hit regions.
	_btn5Secs = new AgentGeoscapeFightButton(31, 12, screenWidth-63, screenHeight/2+18);
	_btn1Min = new AgentGeoscapeFightButton(31, 12, screenWidth-31, screenHeight/2+18);
	_btn5Mins = new AgentGeoscapeFightButton(31, 12, screenWidth-63, screenHeight/2+31);
	_btn30Mins = new AgentGeoscapeFightButton(31, 12, screenWidth-31, screenHeight/2+31);
	_btn1Hour = new AgentGeoscapeFightButton(31, 12, screenWidth-63, screenHeight/2+44);
	_btn1Day = new AgentGeoscapeFightButton(31, 12, screenWidth-31, screenHeight/2+44);

	_btnRotateLeft = new InteractiveSurface(12, 10, screenWidth-61, screenHeight/2+76);
	_btnRotateRight = new InteractiveSurface(12, 10, screenWidth-37, screenHeight/2+76);
	_btnRotateUp = new InteractiveSurface(13, 12, screenWidth-49, screenHeight/2+62);
	_btnRotateDown = new InteractiveSurface(13, 12, screenWidth-49, screenHeight/2+87);
	_btnZoomIn = new InteractiveSurface(23, 23, screenWidth-25, screenHeight/2+56);
	_btnZoomOut = new InteractiveSurface(13, 17, screenWidth-20, screenHeight/2+82);

	int height = (screenHeight - Screen::ORIGINAL_HEIGHT) / 2 + 10;
	_sideTop = new TextButton(63, height, screenWidth-63, _sidebar->getY() - height - 1);
	_sideBottom = new TextButton(63, height, screenWidth-63, _sidebar->getY() + _sidebar->getHeight() + 1);

	// Right alignment reserves one pixel; wide two-digit hours need 21.
	_txtHour = new Text(21, 16, screenWidth-61, screenHeight/2-26);
	_txtHourSep = new Text(4, 16, screenWidth-41, screenHeight/2-26);
	_txtMin = new Text(20, 16, screenWidth-37, screenHeight/2-26);
	_txtMinSep = new Text(4, 16, screenWidth-17, screenHeight/2-26);
	_txtSec = new Text(11, 9, screenWidth-13, screenHeight/2-22);
	// FONT_SMALL has nine-pixel ink (including descenders), despite its
	// eight-pixel line advance. Separate rows and do not clip the ninth row.
	_txtWeekday = new Text(59, 9, screenWidth-61, screenHeight/2-11);
	_txtDay = new Text(29, 9, screenWidth-61, screenHeight/2-2);
	_txtMonth = new Text(29, 9, screenWidth-32, screenHeight/2-2);
	_txtYear = new Text(59, 9, screenWidth-61, screenHeight/2+7);
	_txtFunds = new Text(59, 9, screenWidth-61, screenHeight/2-28);

	_timeSpeed = _btn5Secs;
	_gameTimer = new Timer(Options::geoClockSpeed);

	_zoomInEffectTimer = new Timer(Options::dogfightSpeed);
	_zoomOutEffectTimer = new Timer(Options::dogfightSpeed);
	_dogfightStartTimer = new Timer(Options::dogfightSpeed);
	_dogfightTimer = new Timer(Options::dogfightSpeed);

	_txtDebug = new Text(200, 32, 0, 0);

	// Set palette
	setInterface("geoscape");

	add(_bg);
	add(_sideLine);
	add(_sidebar);
	add(_globe);

	add(_btnIntercept, "button", "geoscape");
	add(_btnBases, "button", "geoscape");
	add(_btnGraphs, "button", "geoscape");
	add(_btnUfopaedia, "button", "geoscape");
	add(_btnOptions, "button", "geoscape");
	add(_btnFunding, "button", "geoscape");

	add(_btn5Secs, "button", "geoscape");
	add(_btn1Min, "button", "geoscape");
	add(_btn5Mins, "button", "geoscape");
	add(_btn30Mins, "button", "geoscape");
	add(_btn1Hour, "button", "geoscape");
	add(_btn1Day, "button", "geoscape");

	add(_btnRotateLeft);
	add(_btnRotateRight);
	add(_btnRotateUp);
	add(_btnRotateDown);
	add(_btnZoomIn);
	add(_btnZoomOut);

	add(_sideTop, "button", "geoscape");
	add(_sideBottom, "button", "geoscape");

	add(_txtFunds, "text", "geoscape");
	add(_txtHour, "text", "geoscape");
	add(_txtHourSep, "text", "geoscape");
	add(_txtMin, "text", "geoscape");
	add(_txtMinSep, "text", "geoscape");
	add(_txtSec, "text", "geoscape");
	add(_txtWeekday, "text", "geoscape");
	add(_txtDay, "text", "geoscape");
	add(_txtMonth, "text", "geoscape");
	add(_txtYear, "text", "geoscape");

	add(_txtDebug, "text", "geoscape");

	// Set up objects
	Surface *geobord = _game->getMod()->getSurface("GEOBORD.SCR");
	geobord->setX(_sidebar->getX() - geobord->getWidth() + _sidebar->getWidth());
	geobord->setY(_sidebar->getY());
	_sidebar->copy(geobord);

	// GEOBORD contains the old clock frame and speed-button artwork. Reflow
	// that background as well as the widgets, preserving its own theme colors.
	// CalendarBackground: begin (also exercised by the stock-asset regression).
	Uint8 calendarTop[64], calendarBody[64], calendarBottom[2][64], calendarGap[64];
	for (int x = 0; x < 64; ++x)
	{
		calendarTop[x] = _sidebar->getPixel(x, 72);
		calendarBody[x] = _sidebar->getPixel(x, 108);
		calendarBottom[0][x] = _sidebar->getPixel(x, 109);
		calendarBottom[1][x] = _sidebar->getPixel(x, 110);
		calendarGap[x] = _sidebar->getPixel(x, 111);
	}
	for (int y = 71; y < 156; ++y)
		for (int x = 0; x < 64; ++x)
			_sidebar->setPixel(x, y, calendarGap[x]);
	for (int x = 0; x < 64; ++x)
		_sidebar->setPixel(x, 71, calendarTop[x]);
	for (int y = 72; y < 115; ++y)
		for (int x = 0; x < 64; ++x)
			_sidebar->setPixel(x, y, calendarBody[x]);
	for (int y = 0; y < 2; ++y)
		for (int x = 0; x < 64; ++x)
			_sidebar->setPixel(x, y + 115, calendarBottom[y][x]);
	// CalendarBackground: end.
	_game->getMod()->getSurface("ALTGEOBORD.SCR")->blit(_bg);

	_sideLine->drawRect(0, 0, _sideLine->getWidth(), _sideLine->getHeight(), 15);

	_btnIntercept->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btnIntercept->setText(tr("STR_INTERCEPT"));
	_btnIntercept->onMouseClick((ActionHandler)&GeoscapeState::btnInterceptClick);
	_btnIntercept->onKeyboardPress((ActionHandler)&GeoscapeState::btnInterceptClick, Options::keyGeoIntercept);
	_btnIntercept->setGeoscapeButton(true);

	_btnBases->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btnBases->setText(tr("STR_BASES"));
	_btnBases->onMouseClick((ActionHandler)&GeoscapeState::btnBasesClick);
	_btnBases->onKeyboardPress((ActionHandler)&GeoscapeState::btnBasesClick, Options::keyGeoBases);
	_btnBases->setGeoscapeButton(true);

	_btnGraphs->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btnGraphs->setText(tr("STR_GRAPHS"));
	_btnGraphs->onMouseClick((ActionHandler)&GeoscapeState::btnGraphsClick);
	_btnGraphs->onKeyboardPress((ActionHandler)&GeoscapeState::btnGraphsClick, Options::keyGeoGraphs);
	_btnGraphs->setGeoscapeButton(true);

	_btnUfopaedia->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btnUfopaedia->setText(tr("STR_UFOPAEDIA_UC"));
	_btnUfopaedia->onMouseClick((ActionHandler)&GeoscapeState::btnUfopaediaClick);
	_btnUfopaedia->onKeyboardPress((ActionHandler)&GeoscapeState::btnUfopaediaClick, Options::keyGeoUfopedia);
	_btnUfopaedia->setGeoscapeButton(true);

	_btnOptions->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btnOptions->setText(tr("STR_OPTIONS_UC"));
	_btnOptions->onMouseClick((ActionHandler)&GeoscapeState::btnOptionsClick);
	_btnOptions->onKeyboardPress((ActionHandler)&GeoscapeState::btnOptionsClick, Options::keyGeoOptions);
	_btnOptions->setGeoscapeButton(true);

	_btnFunding->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btnFunding->setText(tr("STR_FUNDING_UC"));
	_btnFunding->onMouseClick((ActionHandler)&GeoscapeState::btnFundingClick);
	_btnFunding->onKeyboardPress((ActionHandler)&GeoscapeState::btnFundingClick, Options::keyGeoFunding);
	_btnFunding->setGeoscapeButton(true);

	_btn5Secs->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btn5Secs->setBig();
	_btn5Secs->setText(tr("STR_5_SECONDS"));
	_btn5Secs->setGroup(&_timeSpeed);
	_btn5Secs->onKeyboardPress((ActionHandler)&GeoscapeState::btnTimerClick, Options::keyGeoSpeed1);
	_btn5Secs->setGeoscapeButton(true);

	_btn1Min->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btn1Min->setBig();
	_btn1Min->setText(tr("STR_1_MINUTE"));
	_btn1Min->setGroup(&_timeSpeed);
	_btn1Min->onKeyboardPress((ActionHandler)&GeoscapeState::btnTimerClick, Options::keyGeoSpeed2);
	_btn1Min->setGeoscapeButton(true);

	_btn5Mins->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btn5Mins->setBig();
	_btn5Mins->setText(tr("STR_5_MINUTES"));
	_btn5Mins->setGroup(&_timeSpeed);
	_btn5Mins->onKeyboardPress((ActionHandler)&GeoscapeState::btnTimerClick, Options::keyGeoSpeed3);
	_btn5Mins->setGeoscapeButton(true);

	_btn30Mins->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btn30Mins->setBig();
	_btn30Mins->setText(tr("STR_30_MINUTES"));
	_btn30Mins->setGroup(&_timeSpeed);
	_btn30Mins->onKeyboardPress((ActionHandler)&GeoscapeState::btnTimerClick, Options::keyGeoSpeed4);
	_btn30Mins->setGeoscapeButton(true);

	_btn1Hour->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btn1Hour->setBig();
	_btn1Hour->setText(tr("STR_1_HOUR"));
	_btn1Hour->setGroup(&_timeSpeed);
	_btn1Hour->onKeyboardPress((ActionHandler)&GeoscapeState::btnTimerClick, Options::keyGeoSpeed5);
	_btn1Hour->setGeoscapeButton(true);

	_btn1Day->initText(_game->getMod()->getFont("FONT_GEO_BIG"), _game->getMod()->getFont("FONT_GEO_SMALL"), _game->getLanguage());
	_btn1Day->setBig();
	_btn1Day->setText(tr("STR_1_DAY"));
	_btn1Day->setGroup(&_timeSpeed);
	_btn1Day->onKeyboardPress((ActionHandler)&GeoscapeState::btnTimerClick, Options::keyGeoSpeed6);
	_btn1Day->setGeoscapeButton(true);

	_sideBottom->setGeoscapeButton(true);
	_sideTop->setGeoscapeButton(true);

	_btnRotateLeft->onMousePress((ActionHandler)&GeoscapeState::btnRotateLeftPress);
	_btnRotateLeft->onMouseRelease((ActionHandler)&GeoscapeState::btnRotateLeftRelease);
	_btnRotateLeft->onKeyboardPress((ActionHandler)&GeoscapeState::btnRotateLeftPress, Options::keyGeoLeft);
	_btnRotateLeft->onKeyboardRelease((ActionHandler)&GeoscapeState::btnRotateLeftRelease, Options::keyGeoLeft);

	_btnRotateRight->onMousePress((ActionHandler)&GeoscapeState::btnRotateRightPress);
	_btnRotateRight->onMouseRelease((ActionHandler)&GeoscapeState::btnRotateRightRelease);
	_btnRotateRight->onKeyboardPress((ActionHandler)&GeoscapeState::btnRotateRightPress, Options::keyGeoRight);
	_btnRotateRight->onKeyboardRelease((ActionHandler)&GeoscapeState::btnRotateRightRelease, Options::keyGeoRight);

	_btnRotateUp->onMousePress((ActionHandler)&GeoscapeState::btnRotateUpPress);
	_btnRotateUp->onMouseRelease((ActionHandler)&GeoscapeState::btnRotateUpRelease);
	_btnRotateUp->onKeyboardPress((ActionHandler)&GeoscapeState::btnRotateUpPress, Options::keyGeoUp);
	_btnRotateUp->onKeyboardRelease((ActionHandler)&GeoscapeState::btnRotateUpRelease, Options::keyGeoUp);

	_btnRotateDown->onMousePress((ActionHandler)&GeoscapeState::btnRotateDownPress);
	_btnRotateDown->onMouseRelease((ActionHandler)&GeoscapeState::btnRotateDownRelease);
	_btnRotateDown->onKeyboardPress((ActionHandler)&GeoscapeState::btnRotateDownPress, Options::keyGeoDown);
	_btnRotateDown->onKeyboardRelease((ActionHandler)&GeoscapeState::btnRotateDownRelease, Options::keyGeoDown);

	_btnZoomIn->onMouseClick((ActionHandler)&GeoscapeState::btnZoomInLeftClick, SDL_BUTTON_LEFT);
	_btnZoomIn->onMouseClick((ActionHandler)&GeoscapeState::btnZoomInRightClick, SDL_BUTTON_RIGHT);
	_btnZoomIn->onKeyboardPress((ActionHandler)&GeoscapeState::btnZoomInLeftClick, Options::keyGeoZoomIn);

	_btnZoomOut->onMouseClick((ActionHandler)&GeoscapeState::btnZoomOutLeftClick, SDL_BUTTON_LEFT);
	_btnZoomOut->onMouseClick((ActionHandler)&GeoscapeState::btnZoomOutRightClick, SDL_BUTTON_RIGHT);
	_btnZoomOut->onKeyboardPress((ActionHandler)&GeoscapeState::btnZoomOutLeftClick, Options::keyGeoZoomOut);

	_txtFunds->setAlign(ALIGN_CENTER);
	_txtFunds->setVisible(Options::showFundsOnGeoscape);

	_txtHour->setBig();
	_txtHour->setAlign(ALIGN_RIGHT);

	_txtHourSep->setBig();
	_txtHourSep->setText(":");

	_txtMin->setBig();

	_txtMinSep->setBig();
	_txtMinSep->setText(":");

	_txtWeekday->setAlign(ALIGN_CENTER);

	_txtDay->setAlign(ALIGN_CENTER);

	_txtMonth->setAlign(ALIGN_CENTER);

	_txtYear->setAlign(ALIGN_CENTER);

	if (Options::showFundsOnGeoscape)
	{
		// Small digits have a one-pixel overhang from negative spacing.
		_txtHour->setWidth(20);
		_txtHour->setY(_txtHour->getY()+7);
		_txtHour->setSmall();
		_txtHourSep->setY(_txtHourSep->getY()+7);
		_txtHourSep->setSmall();
		_txtMin->setY(_txtMin->getY()+7);
		_txtMin->setSmall();
		// Two small digits advance ten pixels but paint eleven.
		_txtMinSep->setX(_txtMinSep->getX()-9);
		_txtMinSep->setY(_txtMinSep->getY()+7);
		_txtMinSep->setSmall();
		_txtSec->setX(_txtSec->getX()-10);
		_txtSec->setY(_txtHour->getY());
	}

	_gameTimer->onTimer((StateHandler)&GeoscapeState::timeAdvance);
	_gameTimer->start();

	_zoomInEffectTimer->onTimer((StateHandler)&GeoscapeState::zoomInEffect);
	_zoomOutEffectTimer->onTimer((StateHandler)&GeoscapeState::zoomOutEffect);
	_dogfightStartTimer->onTimer((StateHandler)&GeoscapeState::startDogfight);
	_dogfightTimer->onTimer((StateHandler)&GeoscapeState::handleDogfights);

	timeDisplay();
}

/**
 * Deletes timers.
 */
GeoscapeState::~GeoscapeState()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    Observer::agentCancelGlobeInputOwner(this,Observer::AgentGlobeCancelReason::Destroyed);
#endif
	delete _gameTimer;
	delete _zoomInEffectTimer;
	delete _zoomOutEffectTimer;
	delete _dogfightStartTimer;
	delete _dogfightTimer;

	std::list<DogfightState*>::iterator it = _dogfights.begin();
	for (; it != _dogfights.end();)
	{
		delete *it;
		it = _dogfights.erase(it);
	}
	for (it = _dogfightsToBeStarted.begin(); it != _dogfightsToBeStarted.end();)
	{
		delete *it;
		it = _dogfightsToBeStarted.erase(it);
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool GeoscapeState::agentCursorMotionSafe() const
{
    if (!State::agentCursorMotionSafe() || !_dogfightsToBeStarted.empty() || !_popups.empty()) return false;
    for (const DogfightState *dogfight : _dogfights)
        if (!dogfight || !dogfight->agentCursorMotionSafe()) return false;
    return true;
}

DogfightState *GeoscapeState::agentActiveDogfight() const
{
	if (_modal || _dogfights.size() != 1 || !_dogfightsToBeStarted.empty() || !_popups.empty()) return nullptr;
	return _dogfights.front();
}

bool GeoscapeState::agentDogfightComposite(Observer::AgentUiState &out) const
{
    using namespace Observer;
    const std::vector<DogfightState*> roster(_dogfights.begin(),_dogfights.end());
    if(!_game->isState(const_cast<GeoscapeState*>(this))||_modal||roster.empty()||roster.size()>4||
       !_dogfightsToBeStarted.empty()||!_popups.empty()||!_agentCompositePainted||!_agentRosterRevision||
       roster!=_agentPaintedDogfights||!agentPaintHealthy())return false;
    out=AgentUiState();out.screen="dogfight";out.actor=this;out.context.push_back(_agentRosterRevision);
    std::string windows="[",controls="[";bool allMinimized=true,mode=false,minimize=false,restore=false;
    std::vector<Surface*> allChildren;
    for(auto *dog:roster){if(!dog)return false;const auto &surfaces=dog->agentPaintSurfaces();allChildren.insert(allChildren.end(),surfaces.begin(),surfaces.end());}
    for(size_t i=0;i<roster.size();++i){
        std::vector<Surface*> later;
        for(size_t j=i+1;j<roster.size();++j){const auto &surfaces=roster[j]->agentPaintSurfaces();later.insert(later.end(),surfaces.begin(),surfaces.end());}
        AgentUiState part;if(!roster[i]->agentWindowUiState(part,later))return false;
        const std::string head="{\"handle\":\"w"+std::to_string(i)+"\",";
        if(i){windows+=",";controls+=",";}windows+=head+part.visible.substr(1);controls+=head+part.controlVisible.substr(1);
        out.context.insert(out.context.end(),{reinterpret_cast<std::uintptr_t>(roster[i]),reinterpret_cast<std::uintptr_t>(part.actor),reinterpret_cast<std::uintptr_t>(part.save)});
        out.context.insert(out.context.end(),part.context.begin(),part.context.end());
        out.widgets.insert(out.widgets.end(),part.widgets.begin(),part.widgets.end());
        out.targetLifetimes.insert(out.targetLifetimes.end(),part.targetLifetimes.begin(),part.targetLifetimes.end());
        out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());
        mode=mode||part.offers("choose_dogfight_mode");minimize=minimize||part.offers("minimize_dogfight");restore=restore||part.offers("restore_dogfight");
        allMinimized=allMinimized&&roster[i]->isMinimized();
    }
    windows+="]";controls+="]";
    if(mode)out.actions.emplace_back("choose_dogfight_mode","Choose dogfight mode","visible_control_semantics","dogfight_mode");
    if(minimize)out.actions.emplace_back("minimize_dogfight","Minimize dogfight","visible_control_semantics","dogfight_window");
    if(restore)out.actions.emplace_back("restore_dogfight","Restore dogfight","visible_control_semantics","dogfight_window");
    std::string geo="null",geoControls="null";
    if(allMinimized){
        auto painted=agentFlattenPaintOrder(_surfaces);const auto children=agentFlattenPaintOrder(allChildren);painted.insert(painted.end(),children.begin(),children.end());
        const Surface *destination=_game->getScreen()->getSurface();
        const auto include=[&](const Surface *widget,bool control){
            if(!widget->agentPresentationPaintedTo(destination)||!agentWidgetVisible(widget))return false;
            AgentUiState part;agentIncludeWidget(part,widget);
            if(part.regions.empty()||!agentRegionsUncovered(part.regions,widget,painted)||!_game->agentOverlayClear(part))return false;
            if(control){auto before=out.regions.size();out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());if(!agentRegionsDisjoint(out)){out.regions.resize(before);return false;}}
            return true;
        };
        const Text *fields[]={_txtHour,_txtMin,_txtSec,_txtWeekday,_txtDay,_txtMonth,_txtYear,_txtFunds};
        const char *keys[]={"hour","minute","second","weekday","day","month","year","funds"};
        std::string clock="{";
        for(unsigned i=0;i<8;++i){if(i)clock+=",";const auto value=agentTextJson(fields[i]);clock+=agentJsonString(keys[i])+":"+(value!="null"&&include(fields[i],false)?value:"null");}clock+="}";
        const TextButton *speeds[]={_btn5Secs,_btn1Min,_btn5Mins,_btn30Mins,_btn1Hour,_btn1Day};
        std::string choices="[",speed="null",first;unsigned selectedCount=0;
        for(unsigned i=0;i<6;++i){const auto *button=speeds[i];out.widgets.emplace_back(button,button->agentSelectionRevision());out.widgets.emplace_back(button,button->agentPresentationRevision());bool selected=false;
            if(!button->agentPressed(selected)||!include(button,true))continue;
            const auto label=agentButtonLabel(button);if(label.empty()||label.size()>1024)continue;
            if(out.rows.empty())first=label;else choices+=",";
            choices+="{\"handle\":\"s"+std::to_string(out.rows.size())+"\",\"label\":"+agentJsonString(label)+",\"selected\":"+(selected?"true":"false")+"}";out.rows.push_back(i);
            if(selected){speed=agentJsonString(label);++selectedCount;}
        }choices+="]";if(selectedCount>1)return false;
        if(!out.rows.empty())out.actions.emplace_back("choose_geoscape_speed",first,"visible_text","geoscape_speed");
        std::string buttons="[";out.widgets.emplace_back(_btnOptions,_btnOptions->agentPresentationRevision());
        const auto label=agentButtonLabel(_btnOptions);
        if(!label.empty()&&label.size()<=1024&&include(_btnOptions,true)){buttons+="{\"id\":\"options\",\"label\":"+agentJsonString(label)+"}";out.actions.emplace_back("open_pause_menu",label,"visible_text");}buttons+="]";
        geoControls="{\"speed\":"+speed+",\"speeds\":"+choices+",\"buttons\":"+buttons+"}";
        geo="{\"clock\":"+clock+","+geoControls.substr(1);
    }
    out.visible="{\"windows\":"+windows+",\"geoscape\":"+geo+"}";
    out.controlVisible="{\"windows\":"+controls+",\"geoscape\":"+geoControls+"}";
    size_t bytes=0;for(unsigned char c:out.payload())if((bytes+=c<128?1:6)>30000)return false;
    return !out.regions.empty()&&agentRegionsDisjoint(out)&&out.visible.size()<=24000;
}
bool GeoscapeState::agentDispatchDogfight(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;if(!agentDogfightComposite(current)||!current.offers(request.action))return false;
    if(request.action=="open_pause_menu"){btnOptionsClick(nullptr);return true;}
    if(request.action=="choose_geoscape_speed"){
        if(request.choice.size()!=2||request.choice[0]!='s'||request.choice[1]<'0'||request.choice[1]>'5')return false;
        const size_t slot=request.choice[1]-'0';if(slot>=current.rows.size())return false;
        TextButton *speeds[]={_btn5Secs,_btn1Min,_btn5Mins,_btn30Mins,_btn1Hour,_btn1Day};return selectGeoscapeSpeed(speeds[current.rows[slot]]);
    }
    if(request.choice.size()!=2||request.choice[0]!='w'||request.choice[1]<'0'||request.choice[1]>'3')return false;
    const std::vector<DogfightState*> roster(_dogfights.begin(),_dogfights.end());const size_t slot=request.choice[1]-'0';
    if(slot>=roster.size())return false;std::vector<Surface*> later;
    for(size_t i=slot+1;i<roster.size();++i){const auto &surfaces=roster[i]->agentPaintSurfaces();later.insert(later.end(),surfaces.begin(),surfaces.end());}
    return roster[slot]->agentDispatchWindow(request,later);
}

bool GeoscapeState::agentUiState(Observer::AgentUiState &out) const
{
	if (_modal) return false;
	// Dogfight windows are painted after the strategic surfaces. This bounded
	// route owns the currently painted composite; never route a background HUD
	// action through an ambiguous, queued, minimized or preview window.
	if (!_dogfights.empty())
	{
		return agentDogfightComposite(out);
	}
	out = Observer::AgentUiState();
	out.screen = "geoscape";
	const auto includeDisjoint = [&out](const Surface *widget) -> bool
	{
		const size_t before = out.regions.size();
		Observer::agentIncludeWidget(out, widget);
		if (Observer::agentRegionsDisjoint(out)) return true;
		out.regions.resize(before);
		return false;
	};
	const Text *fields[] = {_txtHour, _txtMin, _txtSec, _txtWeekday, _txtDay, _txtMonth, _txtYear, _txtFunds};
	const char *keys[] = {"hour", "minute", "second", "weekday", "day", "month", "year", "funds"};
	out.visible = "{";
	int left = fields[0]->getX(), top = fields[0]->getY(), right = left, bottom = top;
	for (unsigned i = 0; i < 8; ++i)
	{
		std::string value = "null";
		if (Observer::agentWidgetUncovered(fields[i], _surfaces))
		{
			const std::string rendered = Observer::agentTextJson(fields[i]);
			if (rendered != "null" && includeDisjoint(fields[i])) value = rendered;
		}
		if (i) out.visible += ",";
		out.visible += Observer::agentJsonString(keys[i]) + ":" + value;
		left = std::min(left, fields[i]->getX());
		top = std::min(top, fields[i]->getY());
		right = std::max(right, fields[i]->getX() + fields[i]->getWidth());
		bottom = std::max(bottom, fields[i]->getY() + fields[i]->getHeight());
	}
	// Text above still needs individual final-paint, clipping and coverage proof.
	// Bind a conservative fixed HUD allocation union instead of changing glyph
	// bounds. Cursor/FPS overlap checks retain coverage of every projected glyph,
	// while an ordinary clock tick cannot revoke an unrelated button grant.
	const Observer::AgentUiRect hud = {left, top, right - left, bottom - top};
	if (!Observer::agentRegionVisible(hud)) return false;
	out.regions.clear();
	out.regions.push_back(hud);

	const TextButton *speeds[] = {_btn5Secs, _btn1Min, _btn5Mins, _btn30Mins, _btn1Hour, _btn1Day};
	std::string speed = "null", choices = "[", firstLabel;
	unsigned selectedCount = 0;
	for (unsigned i = 0; i < 6; ++i)
	{
		const TextButton *button = speeds[i];
		// Include every member's generation, including hidden members, to retain
		// radio selection away/back (ABA) freshness without serializing identity.
		out.widgets.push_back(std::make_pair(button, button->agentSelectionRevision()));
        out.widgets.emplace_back(button,button->agentPresentationRevision());
		bool selected = false;
		if (!Observer::agentWidgetUncovered(button, _surfaces) || !button->agentPressed(selected)) continue;
		const std::string label = Observer::agentButtonLabel(button);
		if (label.empty() || !includeDisjoint(button)) continue;
		if (selected) { speed = Observer::agentJsonString(label); ++selectedCount; }
		if (out.rows.empty()) firstLabel = label; else choices += ",";
		choices += "{\"handle\":\"s" + std::to_string(out.rows.size()) + "\",\"label\":" +
			Observer::agentJsonString(label) + ",\"selected\":" + (selected ? "true" : "false") + "}";
		out.rows.push_back(i);
	}
	if (selectedCount > 1) return false;
	choices += "]";
	if (!out.rows.empty())
		out.actions.push_back(Observer::AgentUiAction("choose_geoscape_speed", firstLabel, "visible_text", "geoscape_speed"));
    if(!out.rows.empty()){
        auto authority=std::make_shared<Observer::AgentUiState>(out);
        authority->actor=this;
        authority->visible="{\"speed\":"+speed+",\"speeds\":"+choices+"}";
        authority->controlVisible=authority->visible;
        // Captured before any marker identity/position/region is attached.
        // The normal composite keeps its full immutable marker projection.
        out.geoscapeSpeedAuthority=authority;
    }
	std::string controls = "{\"speed\":" + speed + ",\"speeds\":" + choices + ",\"buttons\":[";

	const TextButton *buttons[] = {_btnIntercept, _btnBases, _btnGraphs, _btnUfopaedia, _btnOptions, _btnFunding};
	const char *ids[] = {"intercept", "bases", "graphs", "ufopaedia", "options", "funding"};
	unsigned count = 0;
	for (unsigned i = 0; i < 6; ++i)
	{
		if (!Observer::agentWidgetUncovered(buttons[i], _surfaces)) continue;
		const std::string label = Observer::agentButtonLabel(buttons[i]);
		if (label.empty() || !includeDisjoint(buttons[i])) continue;
		out.widgets.push_back(std::make_pair(buttons[i], buttons[i]->agentSelectionRevision()));
		if (buttons[i] == _btnIntercept)
			out.actions.push_back(Observer::AgentUiAction("open_interception", label, "visible_text"));
		if (buttons[i] == _btnBases)
			out.actions.push_back(Observer::AgentUiAction("open_bases", label, "visible_text"));
		if (buttons[i] == _btnOptions && !(_zoomInEffectTimer->isRunning() || _zoomOutEffectTimer->isRunning()))
			out.actions.push_back(Observer::AgentUiAction("open_pause_menu", label, "visible_text"));
		if (buttons[i] == _btnUfopaedia)
			out.actions.push_back(Observer::AgentUiAction("open_ufopaedia", label, "visible_text"));
		if (buttons[i] == _btnGraphs)
            out.actions.emplace_back("open_graphs",label,"visible_text");
		if (buttons[i] == _btnFunding)
			out.actions.push_back(Observer::AgentUiAction("open_funding", label, "visible_text"));
		if (count++) controls += ",";
		controls += "{\"id\":" + Observer::agentJsonString(ids[i]) +
			",\"label\":" + Observer::agentJsonString(label) + "}";
	}
    // Only surfaces painted after the globe can occlude its final markers.
    std::vector<Surface *> markerOverlays;
    bool afterGlobe=false;
    for (Surface *surface : _surfaces) {
        if (surface==_globe) { afterGlobe=true; continue; }
        if (afterGlobe) markerOverlays.push_back(surface);
    }
    Observer::AgentUiState markerProjection;
    std::vector<Target *> markerTargets;
    std::vector<std::pair<int,int> > markerPoints;
    const std::string markers=_globe->agentGeoscapeTargets(markerProjection,markerOverlays,markerTargets,markerPoints);
    // Speed rows deliberately retain their independent native-index mapping.
    out.context.insert(out.context.end(),markerProjection.context.begin(),markerProjection.context.end());
    out.targetLifetimes=markerProjection.targetLifetimes;
    out.regions.insert(out.regions.end(),markerProjection.regions.begin(),markerProjection.regions.end());
    if (!markerTargets.empty())
        out.actions.emplace_back("click_visible_globe_target","Inspect visible target","visible_control_semantics","visible_row");
    // Six empty hitboxes acquire authority only from their current stock glyph
    // pixels in the painted sidebar AND the final composed screen.
    const InteractiveSurface *viewButtons[]={_btnRotateLeft,_btnRotateRight,_btnRotateUp,_btnRotateDown,_btnZoomIn,_btnZoomOut};
    const char *viewIds[]={"rotate_globe_left","rotate_globe_right","rotate_globe_up","rotate_globe_down","zoom_globe_in","zoom_globe_out"};
    const char *viewLabels[]={"Rotate globe left","Rotate globe right","Rotate globe up","Rotate globe down","Zoom globe in","Zoom globe out"};
    const int glyph[6][4]={{3,176,12,10},{27,176,12,10},{15,162,13,12},{15,187,13,12},{39,156,23,23},{44,182,13,17}};
    Surface *shown=_game->getScreen()->getSurface();
    const auto fingerprint=[](const Surface *surface,int x,int y,int w,int h,std::uint64_t &value)->bool {
        if(!surface || surface->getSurface()->format->BytesPerPixel!=1 || !surface->getPalette() || x<0 || y<0 || x+w>surface->getWidth() || y+h>surface->getHeight())return false;
        value=14695981039346656037ULL;
        for(int dy=0;dy<h;++dy)for(int dx=0;dx<w;++dx){const SDL_Color c=surface->getPalette()[surface->getPixel(x+dx,y+dy)];for(unsigned char b:{c.r,c.g,c.b})value=(value^b)*1099511628211ULL;}
        return true;
    };
    std::uint64_t crop[6]={};int registered=-1;
    const std::string activeMaster=Options::getActiveMaster();
    const int master=activeMaster=="xcom1"?0:(activeMaster=="xcom2"?1:-1);
    if(master>=0 && _sidebar->getWidth()==64 && _sidebar->getHeight()==200 && _sidebar->agentPresentationPaintedTo(shown) && Observer::agentWidgetVisible(_sidebar))
        {bool match=true;for(int i=0;i<6;++i)match=match && fingerprint(_sidebar,glyph[i][0],glyph[i][1],glyph[i][2],glyph[i][3],crop[i]) && crop[i]==Observer::agentGlobeGlyphs[master][i];if(match)registered=master;}
    const bool viewEnabled=registered>=0 && Observer::agentGlobeInputIdle(_game) && !_modal && _popups.empty() && _dogfights.empty() && !(_zoomInEffectTimer->isRunning() || _zoomOutEffectTimer->isRunning()) && _globe->agentViewIdle() && !Observer::agentGlobeInputHeld(_game);
    std::string viewControls="[";unsigned viewCount=0;
    for(int i=0;i<6;++i){
        const Surface *button=viewButtons[i];const int x=_sidebar->getX()+glyph[i][0],y=_sidebar->getY()+glyph[i][1],w=glyph[i][2],h=glyph[i][3];
        if(!viewEnabled || !button->getVisible() || button->getX()!=x || button->getY()!=y || button->getWidth()!=w || button->getHeight()!=h)continue;
        Observer::AgentUiState witness;witness.regions.push_back({x,y,w,h});
        if(!Observer::agentRegionVisible(witness.regions[0]) || !_game->agentOverlayClear(witness))continue;
        bool afterSidebar=false,clear=true;
        for(const Surface *surface:_surfaces){
            if(surface==_sidebar){afterSidebar=true;continue;}
            if(!afterSidebar || !surface || !surface->getVisible())continue;
            bool hitbox=false;for(const auto b:viewButtons)if(surface==b)hitbox=true;
            if(hitbox){
                // Their ordinary art is the sidebar. A nonempty/custom hitbox
                // paint invalidates the registered glyph proof.
                const SDL_Surface *raw=surface->getSurface();
                if(raw->format->BytesPerPixel!=1 || !(raw->flags&SDL_SRCCOLORKEY)){clear=false;break;}
                for(int py=0;py<surface->getHeight();++py)for(int px=0;px<surface->getWidth();++px)
                    if(surface->getPixel(px,py)!=raw->format->colorkey)clear=false;
                continue;
            }
            if(x<surface->getX()+surface->getWidth() && x+w>surface->getX() && y<surface->getY()+surface->getHeight() && y+h>surface->getY())clear=false;
        }
        std::uint64_t actual=0;
        if(!clear || !fingerprint(shown,x,y,w,h,actual) || actual!=crop[i])continue;
        const std::size_t before=out.regions.size();out.regions.push_back(witness.regions[0]);
        if(!Observer::agentRegionsDisjoint(out)){out.regions.resize(before);continue;}
        out.widgets.emplace_back(_sidebar,_sidebar->agentPresentationRevision());out.widgets.emplace_back(button,button->agentPresentationRevision());out.context.push_back(_globe->agentViewRevision());
        out.actions.emplace_back(viewIds[i],viewLabels[i],"visible_control_semantics");
        if(viewCount++)viewControls+=",";
        viewControls+="{\"id\":"+Observer::agentJsonString(viewIds[i])+",\"rect\":{\"x\":"+std::to_string(x)+",\"y\":"+std::to_string(y)+",\"width\":"+std::to_string(w)+",\"height\":"+std::to_string(h)+"}}";
    }
    viewControls+="]";
    Observer::AgentUiState rasterProjection;
    const std::string raster=_globe->agentViewRaster(rasterProjection,markerOverlays);
    out.regions.insert(out.regions.end(),rasterProjection.regions.begin(),rasterProjection.regions.end());
    out.context.insert(out.context.end(),rasterProjection.context.begin(),rasterProjection.context.end());
    controls += "],\"targets\":"+markers+",\"globe_controls\":"+viewControls+"}";
	out.visible += ",";
	out.visible.append(controls, 1, std::string::npos);
    out.visible.pop_back();out.visible+=",\"globe_view\":"+raster+"}";
	out.controlVisible = controls;
    if(Observer::agentGlobeInputHeld(_game))out.actions.clear();
    return out.visible.size()<24000 && Observer::agentRegionsDisjoint(out);
}

bool GeoscapeState::agentDispatch(const std::string &action)
{
	Observer::AgentUiState current;
	if (!agentUiState(current) || !current.offers(action)) return false;
    if(Observer::agentGlobeInputHeld(_game))return false;
    if(action=="zoom_globe_in"){if(!Observer::agentGlobeInputIdle(_game))return false;agentGlobeAudits[_game];btnZoomInLeftClick(nullptr);globeZoomReturned(_game,"zoom_globe_in");return true;}
    if(action=="zoom_globe_out"){if(!Observer::agentGlobeInputIdle(_game))return false;agentGlobeAudits[_game];btnZoomOutLeftClick(nullptr);globeZoomReturned(_game,"zoom_globe_out");return true;}
    const char *recordAction=nullptr;
    void (GeoscapeState::*press)(Action *)=nullptr;
    void (GeoscapeState::*release)(Action *)=nullptr;
    if(action=="rotate_globe_left"){recordAction="rotate_globe_left";press=&GeoscapeState::btnRotateLeftPress;release=&GeoscapeState::btnRotateLeftRelease;}
    if(action=="rotate_globe_right"){recordAction="rotate_globe_right";press=&GeoscapeState::btnRotateRightPress;release=&GeoscapeState::btnRotateRightRelease;}
    if(action=="rotate_globe_up"){recordAction="rotate_globe_up";press=&GeoscapeState::btnRotateUpPress;release=&GeoscapeState::btnRotateUpRelease;}
    if(action=="rotate_globe_down"){recordAction="rotate_globe_down";press=&GeoscapeState::btnRotateDownPress;release=&GeoscapeState::btnRotateDownRelease;}
    if(press){
        if(!_globe->agentViewIdle() || _game->agentTopState()!=this || !Observer::agentGlobeInputIdle(_game))return false;
        auto &audit=agentGlobeAudits[_game];audit.closed=false;const std::uint64_t sequence=globeNextSequence(audit);
        if(!agentGlobePulses.emplace(_game,AgentGlobePulse{_game,this,_globe,release,_globe->agentViewRevision(),SDL_GetTicks(),sequence,0,recordAction}).second)return false;
        (this->*press)(nullptr);
        globeRecord(_game,sequence,recordAction,"press_return","",0,false);return true;
    }
	if (action == "open_ufopaedia") { btnUfopaediaClick(nullptr); return true; }
	if (action == "open_graphs") { btnGraphsClick(nullptr); return true; }
	if (action == "open_funding") { btnFundingClick(nullptr); return true; }
	if (action == "open_interception") { btnInterceptClick(nullptr); return true; }
	if (action == "open_bases") { btnBasesClick(nullptr); return true; }
	if (action == "open_pause_menu") { btnOptionsClick(nullptr); return true; }
	return false;
}

bool GeoscapeState::agentDispatch(const Observer::AgentRequest &request)
{
	if (!_dogfights.empty()) return agentDispatchDogfight(request);
    if (request.action=="click_visible_globe_target")
    {
        Observer::AgentUiState current;
        if (!agentUiState(current) || !current.offers(request.action)) return false;
        std::vector<Surface *> overlays; bool after=false;
        for (Surface *surface:_surfaces) {
            if(surface==_globe) { after=true;continue; }
            if(after) overlays.push_back(surface);
        }
        Observer::AgentUiState projection; std::vector<Target *> targets;
        std::vector<std::pair<int,int> > points;
        _globe->agentGeoscapeTargets(projection,overlays,targets,points);
        for(size_t i=0;i<points.size();++i) if(request.choice=="r"+std::to_string(i))
        {
            if(i>=current.targetLifetimes.size() || projection.targetLifetimes[i].expired() ||
                current.targetLifetimes[i].owner_before(projection.targetLifetimes[i]) ||
                projection.targetLifetimes[i].owner_before(current.targetLifetimes[i])) return false;
            SDL_Event event={};event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;
            Action action(&event,1.0,1.0,0,0);
            action.setMouseAction(points[i].first,points[i].second,_globe->getX(),_globe->getY());
            action.setSender(_globe);
            globeClick(&action);
            return true;
        }
        return false;
    }
	if (request.action != "choose_geoscape_speed") return agentDispatch(request.action);
	Observer::AgentUiState current;
	if (!agentUiState(current) || !current.offers(request.action) || request.choice.size() != 2 ||
		request.choice[0] != 's' || request.choice[1] < '0' || request.choice[1] > '5') return false;
	const unsigned slot = static_cast<unsigned>(request.choice[1] - '0');
	if (slot >= current.rows.size()) return false;
	TextButton *speeds[] = {_btn5Secs, _btn1Min, _btn5Mins, _btn30Mins, _btn1Hour, _btn1Day};
	return selectGeoscapeSpeed(speeds[current.rows[slot]]);
}

#endif

/**
 * Handle blitting of Geoscape and Dogfights.
 */
void GeoscapeState::blit()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentCompositePainted=false;
    const Surface *beforeLogical=_game->getScreen()->getSurface();
    const SDL_Surface *beforePixels=beforeLogical?beforeLogical->getSurface():nullptr;
    const bool beforeFull=beforePixels && beforePixels->clip_rect.x==0 && beforePixels->clip_rect.y==0 && beforePixels->clip_rect.w==beforePixels->w && beforePixels->clip_rect.h==beforePixels->h;
    const std::vector<DogfightState*> roster(_dogfights.begin(),_dogfights.end());
    if(roster!=_agentPaintedDogfights && _agentRosterRevision)++_agentRosterRevision;
    for(Surface *surface:_surfaces)surface->agentTrackPresentation();
#endif
	State::blit();
	for (std::list<DogfightState*>::iterator it = _dogfights.begin(); it != _dogfights.end(); ++it)
	{
		(*it)->blit();
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	_globe->agentRecordOccluders(_surfaces, _dogfights.empty());
    _agentPaintedDogfights=roster;
    const Surface *logical=_game->getScreen()->getSurface();const SDL_Surface *pixels=logical?logical->getSurface():nullptr;
    _agentCompositePainted=beforeFull && Observer::agentPaintHealthy() && pixels && pixels->clip_rect.x==0 && pixels->clip_rect.y==0 && pixels->clip_rect.w==pixels->w && pixels->clip_rect.h==pixels->h;
#endif
}

/**
 * Handle key shortcuts.
 * @param action Pointer to an action.
 */
void GeoscapeState::handle(Action *action)
{
	if (_dogfights.size() == _minimizedDogfights)
	{
		State::handle(action);
	}

	if (action->getDetails()->type == SDL_KEYDOWN)
	{
		// "ctrl-d" - enable debug mode
		if (Options::debug && action->getDetails()->key.keysym.sym == SDLK_d && (SDL_GetModState() & KMOD_CTRL) != 0)
		{
			_game->getSavedGame()->setDebugMode();
			if (_game->getSavedGame()->getDebugMode())
			{
				_txtDebug->setText("DEBUG MODE");
			}
			else
			{
				_txtDebug->setText("");
			}
		}
		// "ctrl-c" - delete all soldier commendations
		if (Options::debug && action->getDetails()->key.keysym.sym == SDLK_c && (SDL_GetModState() & KMOD_CTRL) != 0)
		{
			if (_game->getSavedGame()->getDebugMode())
			{
				_txtDebug->setText("SOLDIER COMMENDATIONS DELETED");
				for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
				{
					for (std::vector<Soldier*>::iterator j = (*i)->getSoldiers()->begin(); j != (*i)->getSoldiers()->end(); ++j)
					{
						for (std::vector<SoldierCommendations*>::iterator k = (*j)->getDiary()->getSoldierCommendations()->begin(); k != (*j)->getDiary()->getSoldierCommendations()->end(); ++k)
						{
							delete *k;
						}
						(*j)->getDiary()->getSoldierCommendations()->clear();
					}
				}
			}
			else
			{
				_txtDebug->setText("");
			}
		}
		// quick save and quick load
		else if (!_game->getSavedGame()->isIronman())
		{
			if (action->getDetails()->key.keysym.sym == Options::keyQuickSave)
			{
				popup(new SaveGameState(OPT_GEOSCAPE, SAVE_QUICK, _palette));
			}
			else if (action->getDetails()->key.keysym.sym == Options::keyQuickLoad)
			{
				popup(new LoadGameState(OPT_GEOSCAPE, SAVE_QUICK, _palette));
			}
		}
	}
	if (!_dogfights.empty())
	{
		for (std::list<DogfightState*>::iterator it = _dogfights.begin(); it != _dogfights.end(); ++it)
		{
			(*it)->handle(action);
		}
		_minimizedDogfights = minimizedDogfightsCount();
	}
}

/**
 * Updates the timer display and resets the palette
 * since it's bound to change on other screens.
 */
void GeoscapeState::init()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    Observer::agentCancelGlobeInputOwner(this,Observer::AgentGlobeCancelReason::Init);
#endif
	State::init();
	timeDisplay();

	_globe->onMouseClick((ActionHandler)&GeoscapeState::globeClick);
	_globe->onMouseOver(0);
	_globe->rotateStop();
	_globe->setFocus(true);
	_globe->draw();

	// Pop up save screen if it's a new ironman game
	if (_game->getSavedGame()->isIronman() && _game->getSavedGame()->getName().empty())
	{
		popup(new ListSaveState(OPT_GEOSCAPE));
	}

	// Set music if it's not already playing
	if (_dogfights.empty() && !_dogfightStartTimer->isRunning())
	{
		if (_game->getSavedGame()->getMonthsPassed() == -1)
		{
			_game->getMod()->playMusic("GMGEO", 1);
		}
		else
		{
			_game->getMod()->playMusic("GMGEO");
		}
	}
	else
	{
		_game->getMod()->playMusic("GMINTER");
	}
	_globe->setNewBaseHover(false);

		// run once
	if (_game->getSavedGame()->getMonthsPassed() == -1 &&
		// as long as there's a base
		!_game->getSavedGame()->getBases()->empty() &&
		// and it has a name (THIS prevents it from running prior to the base being placed.)
		!_game->getSavedGame()->getBases()->front()->getName().empty())
	{
		_game->getSavedGame()->addMonth();
		determineAlienMissions();
		_game->getSavedGame()->setFunds(_game->getSavedGame()->getFunds() - (_game->getSavedGame()->getBaseMaintenance() - _game->getSavedGame()->getBases()->front()->getPersonnelMaintenance()));
	}
}

/**
 * Runs the game timer and handles popups.
 */
void GeoscapeState::think()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    Observer::agentPollGlobeInput(_game);
    if(_modal || !_popups.empty() || !_dogfights.empty() || buttonsDisabled())
        Observer::agentCancelGlobeInputOwner(this,Observer::AgentGlobeCancelReason::Modal);
#endif
    State::think(); // Ordinary Globe::think is the only rotation timer driver.
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    auto pulse=agentGlobePulses.find(_game);
    if(pulse!=agentGlobePulses.end() && pulse->second.owner==this &&
       pulse->second.globe->agentViewRevision()!=pulse->second.revision)
        Observer::agentCancelGlobeInput(_game,Observer::AgentGlobeCancelReason::ViewTick);
    Observer::agentPollGlobeInput(_game);
#endif

	_zoomInEffectTimer->think(this, 0);
	_zoomOutEffectTimer->think(this, 0);
	_dogfightStartTimer->think(this, 0);

	if (_popups.empty() && _dogfights.empty() && (!_zoomInEffectTimer->isRunning() || _zoomInEffectDone) && (!_zoomOutEffectTimer->isRunning() || _zoomOutEffectDone))
	{
		// Handle timers
		_gameTimer->think(this, 0);
	}
	else
	{
		if (!_dogfights.empty() || _minimizedDogfights != 0)
		{
			// If all dogfights are minimized rotate the globe, etc.
			if (_dogfights.size() == _minimizedDogfights)
			{
				_pause = false;
				_gameTimer->think(this, 0);
			}
			_dogfightTimer->think(this, 0);
		}
		if (!_popups.empty())
		{
			// Handle popups
			_globe->rotateStop();
			_game->pushState(_popups.front());
			_popups.erase(_popups.begin());
		}
	}
}

/**
 * Updates the Geoscape clock with the latest
 * game time and date in human-readable format. (+Funds)
 */
void GeoscapeState::timeDisplay()
{
	if (Options::showFundsOnGeoscape)
	{
		_txtFunds->setText(Unicode::formatFunding(_game->getSavedGame()->getFunds()));
	}

	std::ostringstream ss;
	ss << std::setfill('0') << std::setw(2) << _game->getSavedGame()->getTime()->getSecond();
	_txtSec->setText(ss.str());

	std::ostringstream ss2;
	ss2 << std::setfill('0') << std::setw(2) << _game->getSavedGame()->getTime()->getMinute();
	_txtMin->setText(ss2.str());

	std::ostringstream ss3;
	ss3 << _game->getSavedGame()->getTime()->getHour();
	_txtHour->setText(ss3.str());

	std::ostringstream ss4;
	ss4 << _game->getSavedGame()->getTime()->getDayString(_game->getLanguage());
	_txtDay->setText(ss4.str());

	_txtWeekday->setText(tr(_game->getSavedGame()->getTime()->getWeekdayString()));

	_txtMonth->setText(tr(_game->getSavedGame()->getTime()->getMonthString()));

	std::ostringstream ss5;
	ss5 << _game->getSavedGame()->getTime()->getYear();
	_txtYear->setText(ss5.str());
}

/**
 * Advances the game timer according to
 * the timer speed set, and calls the respective
 * triggers. The timer always advances in "5 secs"
 * cycles, regardless of the speed, otherwise it might
 * skip important steps. Instead, it just keeps advancing
 * the timer until the next speed step (eg. the next day
 * on 1 Day speed) or until an event occurs, since updating
 * the screen on each step would become cumbersomely slow.
 */
void GeoscapeState::timeAdvance()
{
	int timeSpan = 0;
	if (_timeSpeed == _btn5Secs)
	{
		timeSpan = 1;
	}
	else if (_timeSpeed == _btn1Min)
	{
		timeSpan = 12;
	}
	else if (_timeSpeed == _btn5Mins)
	{
		timeSpan = 12 * 5;
	}
	else if (_timeSpeed == _btn30Mins)
	{
		timeSpan = 12 * 5 * 6;
	}
	else if (_timeSpeed == _btn1Hour)
	{
		timeSpan = 12 * 5 * 6 * 2;
	}
	else if (_timeSpeed == _btn1Day)
	{
		timeSpan = 12 * 5 * 6 * 2 * 24;
	}


	for (int i = 0; i < timeSpan && !_pause; ++i)
	{
		TimeTrigger trigger;
		trigger = _game->getSavedGame()->getTime()->advance();
		switch (trigger)
		{
		case TIME_1MONTH:
			time1Month();
		case TIME_1DAY:
			time1Day();
		case TIME_1HOUR:
			time1Hour();
		case TIME_30MIN:
			time30Minutes();
		case TIME_10MIN:
			time10Minutes();
		case TIME_5SEC:
			time5Seconds();
		}
	}

	_pause = !_dogfightsToBeStarted.empty() || _zoomInEffectTimer->isRunning() || _zoomOutEffectTimer->isRunning();

	timeDisplay();
	_globe->draw();
}

/**
 * Takes care of any game logic that has to
 * run every game second, like craft movement.
 */
void GeoscapeState::time5Seconds()
{
	// Game over if there are no more bases.
	if (_game->getSavedGame()->getBases()->empty())
	{
		_game->getSavedGame()->setEnding(END_LOSE);
	}
	if (_game->getSavedGame()->getEnding() == END_LOSE)
	{
		_game->pushState(new CutsceneState(CutsceneState::LOSE_GAME));
		if (_game->getSavedGame()->isIronman())
		{
			_game->pushState(new SaveGameState(OPT_GEOSCAPE, SAVE_IRONMAN, _palette));
		}
		return;
	}

	// Handle UFO logic
	for (std::vector<Ufo*>::iterator i = _game->getSavedGame()->getUfos()->begin(); i != _game->getSavedGame()->getUfos()->end(); ++i)
	{
		switch ((*i)->getStatus())
		{
		case Ufo::FLYING:
			(*i)->think();
			if ((*i)->reachedDestination())
			{
				size_t count = _game->getSavedGame()->getMissionSites()->size();
				AlienMission *mission = (*i)->getMission();
				bool detected = (*i)->getDetected();
				mission->ufoReachedWaypoint(**i, *_game, *_globe);
				if (detected != (*i)->getDetected() && !(*i)->getFollowers()->empty())
				{
					if (!((*i)->getTrajectory().getID() == UfoTrajectory::RETALIATION_ASSAULT_RUN && (*i)->getStatus() == Ufo::LANDED))
						popup(new UfoLostState((*i)->getName(_game->getLanguage())));
				}
				if (count < _game->getSavedGame()->getMissionSites()->size())
				{
					MissionSite *site = _game->getSavedGame()->getMissionSites()->back();
					site->setDetected(true);
					popup(new MissionDetectedState(site, this));
				}
				// If UFO was destroyed, don't spawn missions
				if ((*i)->getStatus() == Ufo::DESTROYED)
					return;
				if (Base *base = dynamic_cast<Base*>((*i)->getDestination()))
				{
					mission->setWaveCountdown(30 * (RNG::generate(0, 400) + 48));
					(*i)->setDestination(0);
					base->setupDefenses();
					timerReset();
					if (!base->getDefenses()->empty())
					{
						popup(new BaseDefenseState(base, *i, this));
					}
					else
					{
						handleBaseDefense(base, *i);
						return;
					}
				}
			}
			break;
		case Ufo::LANDED:
			(*i)->think();
			if ((*i)->getSecondsRemaining() == 0)
			{
				AlienMission *mission = (*i)->getMission();
				bool detected = (*i)->getDetected();
				mission->ufoLifting(**i, *_game->getSavedGame());
				if (detected != (*i)->getDetected() && !(*i)->getFollowers()->empty())
				{
					popup(new UfoLostState((*i)->getName(_game->getLanguage())));
				}
			}
			break;
		case Ufo::CRASHED:
			(*i)->think();
			if ((*i)->getSecondsRemaining() == 0)
			{
				(*i)->setDetected(false);
				(*i)->setStatus(Ufo::DESTROYED);
			}
			break;
		case Ufo::DESTROYED:
			// Nothing to do
			break;
		}
	}

	// Handle craft logic
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		for (std::vector<Craft*>::iterator j = (*i)->getCrafts()->begin(); j != (*i)->getCrafts()->end();)
		{
			if ((*j)->isDestroyed())
			{
				for (std::vector<Country*>::iterator country = _game->getSavedGame()->getCountries()->begin(); country != _game->getSavedGame()->getCountries()->end(); ++country)
				{
					if ((*country)->getRules()->insideCountry((*j)->getLongitude(), (*j)->getLatitude()))
					{
						(*country)->addActivityXcom(-(*j)->getRules()->getScore());
						break;
					}
				}
				for (std::vector<Region*>::iterator region = _game->getSavedGame()->getRegions()->begin(); region != _game->getSavedGame()->getRegions()->end(); ++region)
				{
					if ((*region)->getRules()->insideRegion((*j)->getLongitude(), (*j)->getLatitude()))
					{
						(*region)->addActivityXcom(-(*j)->getRules()->getScore());
						break;
					}
				}
				// if a transport craft has been shot down, kill all the soldiers on board.
				if ((*j)->getRules()->getSoldiers() > 0)
				{
					for (std::vector<Soldier*>::iterator k = (*i)->getSoldiers()->begin(); k != (*i)->getSoldiers()->end();)
					{
						if ((*k)->getCraft() == (*j))
						{
							k = _game->getSavedGame()->killSoldier(*k);
						}
						else
						{
							++k;
						}
					}
				}
				Craft *craft = *j;
				j = (*i)->removeCraft(craft, false);
				delete craft;
				continue;
			}
			if ((*j)->getDestination() != 0)
			{
				Ufo* u = dynamic_cast<Ufo*>((*j)->getDestination());
				if (u != 0)
				{
					if (!u->getDetected())
					{
						if (u->getTrajectory().getID() == UfoTrajectory::RETALIATION_ASSAULT_RUN && (u->getStatus() == Ufo::LANDED || u->getStatus() == Ufo::DESTROYED))
						{
							(*j)->returnToBase();
						}
						else
						{
							Waypoint *w = new Waypoint();
							w->setLongitude(u->getLongitude());
							w->setLatitude(u->getLatitude());
							w->setId(u->getId());
							(*j)->setDestination(0);
							popup(new GeoscapeCraftState((*j), _globe, w));
						}
					}
					if (u->getStatus() == Ufo::LANDED && (*j)->isInDogfight())
					{
						(*j)->setInDogfight(false);
					}
					else if (u->getStatus() == Ufo::DESTROYED)
					{
						(*j)->returnToBase();
					}
				}
				else
				{
					if ((*j)->isInDogfight())
					{
						(*j)->setInDogfight(false);
					}
				}
			}

			(*j)->think();

			if ((*j)->reachedDestination())
			{
				Ufo* u = dynamic_cast<Ufo*>((*j)->getDestination());
				Waypoint *w = dynamic_cast<Waypoint*>((*j)->getDestination());
				MissionSite* m = dynamic_cast<MissionSite*>((*j)->getDestination());
				AlienBase* b = dynamic_cast<AlienBase*>((*j)->getDestination());
				if (u != 0)
				{
					switch (u->getStatus())
					{
					case Ufo::FLYING:
						// Not more than 4 interceptions at a time.
						if (_dogfights.size() + _dogfightsToBeStarted.size() >= 4)
						{
							++j;
							continue;
						}
						// Can we actually fight it
						if (!(*j)->isInDogfight() && u->getSpeed() <= (*j)->getRules()->getMaxSpeed())
						{
							DogfightState *dogfight = new DogfightState(this, (*j), u);
							_dogfightsToBeStarted.push_back(dogfight);
							if ((*j)->getRules()->isWaterOnly() && u->getAltitudeInt() > (*j)->getRules()->getMaxAltitude())
							{
								popup(new DogfightErrorState((*j), tr("STR_UNABLE_TO_ENGAGE_DEPTH")));
								dogfight->setMinimized(true);
								dogfight->setWaitForAltitude(true);
							}
							else if ((*j)->getRules()->isWaterOnly() && !_globe->insideLand((*j)->getLongitude(), (*j)->getLatitude()))
							{
								popup(new DogfightErrorState((*j), tr("STR_UNABLE_TO_ENGAGE_AIRBORNE")));
								dogfight->setMinimized(true);
								dogfight->setWaitForPoly(true);
							}
							if (!_dogfightStartTimer->isRunning())
							{
								_pause = true;
								timerReset();
								_globe->center((*j)->getLongitude(), (*j)->getLatitude());
								startDogfight();
								_dogfightStartTimer->start();
							}
							_game->getMod()->playMusic("GMINTER");
						}
						break;
					case Ufo::LANDED:
					case Ufo::CRASHED:
					case Ufo::DESTROYED: // Just before expiration
						if ((*j)->getNumSoldiers() > 0 || (*j)->getNumVehicles() > 0)
						{
							if (!(*j)->isInDogfight())
							{
								// look up polygons texture
								int texture, shade;
								_globe->getPolygonTextureAndShade(u->getLongitude(), u->getLatitude(), &texture, &shade);
								timerReset();
								popup(new ConfirmLandingState(*j, _game->getMod()->getGlobe()->getTexture(texture), shade));
							}
						}
						else if (u->getStatus() != Ufo::LANDED)
						{
							(*j)->returnToBase();
						}
						break;
					}
				}
				else if (w != 0)
				{
					popup(new CraftPatrolState((*j), _globe));
					(*j)->setDestination(0);
				}
				else if (m != 0)
				{
					if ((*j)->getNumSoldiers() > 0 || (*j)->getNumVehicles() > 0)
					{
						// look up polygons texture
						int texture, shade;
						_globe->getPolygonTextureAndShade(m->getLongitude(), m->getLatitude(), &texture, &shade);
						if (_game->getMod()->getGlobe()->getTexture(m->getTexture()) != 0)
						{
							texture = m->getTexture();
						}
						timerReset();
						popup(new ConfirmLandingState(*j, _game->getMod()->getGlobe()->getTexture(texture), shade));
					}
					else
					{
						(*j)->returnToBase();
					}
				}
				else if (b != 0)
				{
					if (b->isDiscovered())
					{
						if ((*j)->getNumSoldiers() > 0 || (*j)->getNumVehicles() > 0)
						{
							int texture, shade;
							_globe->getPolygonTextureAndShade(b->getLongitude(), b->getLatitude(), &texture, &shade);
							timerReset();
							popup(new ConfirmLandingState(*j, _game->getMod()->getGlobe()->getTexture(texture), shade));
						}
						else
						{
							(*j)->returnToBase();
						}
					}
				}
			}
			 ++j;
		}
	}

	// Clean up dead UFOs and end dogfights which were minimized.
	for (std::vector<Ufo*>::iterator i = _game->getSavedGame()->getUfos()->begin(); i != _game->getSavedGame()->getUfos()->end();)
	{
		if ((*i)->getStatus() == Ufo::DESTROYED)
		{
			if (!(*i)->getFollowers()->empty())
			{
				// Remove all dogfights with this UFO.
				for (std::list<DogfightState*>::iterator d = _dogfights.begin(); d != _dogfights.end();)
				{
					if ((*d)->getUfo() == (*i))
					{
						delete *d;
						d = _dogfights.erase(d);
					}
					else
					{
						++d;
					}
				}
			}
			delete *i;
			i = _game->getSavedGame()->getUfos()->erase(i);
		}
		else
		{
			++i;
		}
	}

	// Check any dogfights waiting to open
	for (std::list<DogfightState*>::iterator d = _dogfights.begin(); d != _dogfights.end(); ++d)
	{
		if ((*d)->isMinimized())
		{
			if (((*d)->getWaitForPoly() && _globe->insideLand((*d)->getUfo()->getLongitude(), (*d)->getUfo()->getLatitude())) ||
				((*d)->getWaitForAltitude() && (*d)->getUfo()->getAltitudeInt() <= (*d)->getCraft()->getRules()->getMaxAltitude()))
			{
				_pause = true; // the USO reached the sea during this interval period, stop the timer and let handleDogfights() take it from there.
			}
		}
	}

	// Clean up unused waypoints
	for (std::vector<Waypoint*>::iterator i = _game->getSavedGame()->getWaypoints()->begin(); i != _game->getSavedGame()->getWaypoints()->end();)
	{
		if ((*i)->getFollowers()->empty())
		{
			delete *i;
			i = _game->getSavedGame()->getWaypoints()->erase(i);
		}
		else
		{
			++i;
		}
	}
}

/**
 * Functor that attempt to detect an XCOM base.
 */
class DetectXCOMBase: public std::unary_function<Ufo *, bool>
{
public:
	/// Create a detector for the given base.
	DetectXCOMBase(const Base &base) : _base(base) { /* Empty by design.  */ }
	/// Attempt detection
	bool operator()(const Ufo *ufo) const;
private:
	const Base &_base;	//!< The target base.
};

/**
 * Only UFOs within detection range of the base have a chance to detect it.
 * @param ufo Pointer to the UFO attempting detection.
 * @return If the base is detected by @a ufo.
 */
bool DetectXCOMBase::operator()(const Ufo *ufo) const
{
	if (ufo->getTrajectoryPoint() <= 1) return false;
	if (ufo->getTrajectory().getZone(ufo->getTrajectoryPoint()) == 5) return false;
	if ((ufo->getMission()->getRules().getObjective() != OBJECTIVE_RETALIATION && !Options::aggressiveRetaliation) ||	// only UFOs on retaliation missions actively scan for bases
		ufo->getTrajectory().getID() == UfoTrajectory::RETALIATION_ASSAULT_RUN || 										// UFOs attacking a base don't detect!
		ufo->isCrashed() ||																								// Crashed UFOs don't detect!
		_base.getDistance(ufo) >= Nautical(ufo->getRules()->getSightRange()))											// UFOs have a detection range of 80 XCOM units. - we use a great circle fomrula and nautical miles.
	{
		return false;
	}
	return RNG::percent(_base.getDetectionChance());
}

/**
 * Functor that marks an XCOM base for retaliation.
 * This is required because of the iterator type.
 */
struct SetRetaliationTarget: public std::unary_function<std::map<const Region *, Base *>::value_type, void>
{
	/// Mark as a valid retaliation target.
	void operator()(const argument_type &iter) const { iter.second->setRetaliationTarget(true); }
};

/**
 * Takes care of any game logic that has to
 * run every game ten minutes, like fuel consumption.
 */
void GeoscapeState::time10Minutes()
{
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		// Fuel consumption for XCOM craft.
		for (std::vector<Craft*>::iterator j = (*i)->getCrafts()->begin(); j != (*i)->getCrafts()->end(); ++j)
		{
			if ((*j)->getStatus() == "STR_OUT")
			{
				(*j)->consumeFuel();
				if (!(*j)->getLowFuel() && (*j)->getFuel() <= (*j)->getFuelLimit())
				{
					(*j)->setLowFuel(true);
					(*j)->returnToBase();
					popup(new LowFuelState((*j), this));
				}

				if ((*j)->getDestination() == 0)
				{
					double range = Nautical((*j)->getRules()->getSightRange());
					for (std::vector<AlienBase*>::iterator b = _game->getSavedGame()->getAlienBases()->begin(); b != _game->getSavedGame()->getAlienBases()->end(); ++b)
					{
						if ((*j)->getDistance(*b) <= range)
						{
							if (RNG::percent(50-((*j)->getDistance(*b) / range) * 50) && !(*b)->isDiscovered())
							{
								(*b)->setDiscovered(true);
							}
						}
					}
				}
			}
		}
	}
	if (Options::aggressiveRetaliation)
	{
		// Detect as many bases as possible.
		for (std::vector<Base*>::iterator iBase = _game->getSavedGame()->getBases()->begin(); iBase != _game->getSavedGame()->getBases()->end(); ++iBase)
		{
			// Find a UFO that detected this base, if any.
			std::vector<Ufo*>::const_iterator uu = std::find_if (_game->getSavedGame()->getUfos()->begin(), _game->getSavedGame()->getUfos()->end(), DetectXCOMBase(**iBase));
			if (uu != _game->getSavedGame()->getUfos()->end())
			{
				// Base found
				(*iBase)->setRetaliationTarget(true);
			}
		}
	}
	else
	{
		// Only remember last base in each region.
		std::map<const Region *, Base *> discovered;
		for (std::vector<Base*>::iterator iBase = _game->getSavedGame()->getBases()->begin(); iBase != _game->getSavedGame()->getBases()->end(); ++iBase)
		{
			// Find a UFO that detected this base, if any.
			std::vector<Ufo*>::const_iterator uu = std::find_if (_game->getSavedGame()->getUfos()->begin(), _game->getSavedGame()->getUfos()->end(), DetectXCOMBase(**iBase));
			if (uu != _game->getSavedGame()->getUfos()->end())
			{
				discovered[_game->getSavedGame()->locateRegion(**iBase)] = *iBase;
			}
		}
		// Now mark the bases as discovered.
		std::for_each(discovered.begin(), discovered.end(), SetRetaliationTarget());
	}
}

/** @brief Call AlienMission::think() with proper parameters.
 * This function object calls AlienMission::think() with the proper parameters.
 */
class callThink: public std::unary_function<AlienMission*, void>
{
public:
	/// Store the parameters.
	/**
	 * @param game The game engine.
	 * @param globe The globe object.
	 */
	callThink(Game &game, const Globe &globe) : _game(game), _globe(globe) { /* Empty by design. */ }
	/// Call AlienMission::think() with stored parameters.
	void operator()(AlienMission *am) const { am->think(_game, _globe); }
private:
	Game &_game;
	const Globe &_globe;
};

/** @brief Process a MissionSite.
 * This function object will count down towards expiring a MissionSite, and handle expired MissionSites.
 * @param ts Pointer to mission site.
 * @return Has mission site expired?
 */
bool GeoscapeState::processMissionSite(MissionSite *site) const
{
	bool removeSite = site->getSecondsRemaining() < 30 * 60;
	if (!removeSite)
	{
		site->setSecondsRemaining(site->getSecondsRemaining() - 30 * 60);
	}
	else
	{
		removeSite = site->getFollowers()->empty(); // CHEEKY EXPLOIT
	}

	int score = removeSite ? site->getDeployment()->getDespawnPenalty() : site->getDeployment()->getPoints();

	Region *region = _game->getSavedGame()->locateRegion(*site);
	if (region)
	{
		region->addActivityAlien(score);
	}
	for (std::vector<Country*>::iterator k = _game->getSavedGame()->getCountries()->begin(); k != _game->getSavedGame()->getCountries()->end(); ++k)
	{
		if ((*k)->getRules()->insideCountry(site->getLongitude(), site->getLatitude()))
		{
			(*k)->addActivityAlien(score);
			break;
		}
	}
	if (!removeSite)
	{
		return false;
	}
	delete site;
	return true;
}

/** @brief Advance time for crashed UFOs.
 * This function object will decrease the expiration timer for crashed UFOs.
 */
struct expireCrashedUfo: public std::unary_function<Ufo*, void>
{
	/// Decrease UFO expiration timer.
	void operator()(Ufo *ufo) const
	{
		if (ufo->getStatus() == Ufo::CRASHED)
		{
			if (ufo->getSecondsRemaining() >= 30 * 60)
			{
				ufo->setSecondsRemaining(ufo->getSecondsRemaining() - 30 * 60);
				return;
			}
			// Marked expired UFOs for removal.
			ufo->setStatus(Ufo::DESTROYED);
		}
	}
};

/**
 * Takes care of any game logic that has to
 * run every game half hour, like UFO detection.
 */
void GeoscapeState::time30Minutes()
{
	// Decrease mission countdowns
	std::for_each(_game->getSavedGame()->getAlienMissions().begin(),
			  _game->getSavedGame()->getAlienMissions().end(),
			  callThink(*_game, *_globe));
	// Remove finished missions
	for (std::vector<AlienMission*>::iterator am = _game->getSavedGame()->getAlienMissions().begin();
		am != _game->getSavedGame()->getAlienMissions().end();)
	{
		if ((*am)->isOver())
		{
			delete *am;
			am = _game->getSavedGame()->getAlienMissions().erase(am);
		}
		else
		{
			++am;
		}
	}

	// Handle crashed UFOs expiration
	std::for_each(_game->getSavedGame()->getUfos()->begin(),
			  _game->getSavedGame()->getUfos()->end(),
			  expireCrashedUfo());


	// Handle craft maintenance and alien base detection
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		for (std::vector<Craft*>::iterator j = (*i)->getCrafts()->begin(); j != (*i)->getCrafts()->end(); ++j)
		{
			if ((*j)->getStatus() == "STR_REFUELLING")
			{
				std::string s = (*j)->refuel();
				if (!s.empty())
				{
					std::string msg = tr("STR_NOT_ENOUGH_ITEM_TO_REFUEL_CRAFT_AT_BASE")
										.arg(tr(s))
										.arg((*j)->getName(_game->getLanguage()))
										.arg((*i)->getName());
					popup(new CraftErrorState(this, msg));
				}
			}
		}
	}

	// Handle UFO detection and give aliens points
	for (std::vector<Ufo*>::iterator u = _game->getSavedGame()->getUfos()->begin(); u != _game->getSavedGame()->getUfos()->end(); ++u)
	{
		int points = (*u)->getRules()->getMissionScore(); //one point per UFO in-flight per half hour
		switch ((*u)->getStatus())
		{
		case Ufo::LANDED:
			points *= 2;
		case Ufo::FLYING:
			// Get area
			for (std::vector<Region*>::iterator k = _game->getSavedGame()->getRegions()->begin(); k != _game->getSavedGame()->getRegions()->end(); ++k)
			{
				if ((*k)->getRules()->insideRegion((*u)->getLongitude(), (*u)->getLatitude()))
				{
					(*k)->addActivityAlien(points);
					break;
				}
			}
			// Get country
			for (std::vector<Country*>::iterator k = _game->getSavedGame()->getCountries()->begin(); k != _game->getSavedGame()->getCountries()->end(); ++k)
			{
				if ((*k)->getRules()->insideCountry((*u)->getLongitude(), (*u)->getLatitude()))
				{
					(*k)->addActivityAlien(points);
					break;
				}
			}
			if (!(*u)->getDetected())
			{
				bool detected = false, hyperdetected = false;
				for (std::vector<Base*>::iterator b = _game->getSavedGame()->getBases()->begin(); !hyperdetected && b != _game->getSavedGame()->getBases()->end(); ++b)
				{
					switch ((*b)->detect(*u))
					{
					case 2:	// hyper-wave decoder
						(*u)->setHyperDetected(true);
						hyperdetected = true;
					case 1: // conventional radar
						detected = true;
					}
					for (std::vector<Craft*>::iterator c = (*b)->getCrafts()->begin(); !detected && c != (*b)->getCrafts()->end(); ++c)
					{
						if ((*c)->getStatus() == "STR_OUT" && (*c)->detect(*u))
						{
							detected = true;
							break;
						}
					}
				}
				if (detected)
				{
					(*u)->setDetected(true);
					popup(new UfoDetectedState((*u), this, true, (*u)->getHyperDetected()));
				}
			}
			else
			{
				bool detected = false, hyperdetected = false;
				for (std::vector<Base*>::iterator b = _game->getSavedGame()->getBases()->begin(); !hyperdetected && b != _game->getSavedGame()->getBases()->end(); ++b)
				{
					switch ((*b)->insideRadarRange(*u))
					{
					case 2:	// hyper-wave decoder
						detected = true;
						hyperdetected = true;
						(*u)->setHyperDetected(true);
						break;
					case 1: // conventional radar
						detected = true;
						hyperdetected = (*u)->getHyperDetected();
					}
					for (std::vector<Craft*>::iterator c = (*b)->getCrafts()->begin(); !detected && c != (*b)->getCrafts()->end(); ++c)
					{
						if ((*c)->getStatus() == "STR_OUT" && (*c)->insideRadarRange(*u))
						{
							detected = true;
							hyperdetected = (*u)->getHyperDetected();
							break;
						}
					}
				}
				if (!detected)
				{
					(*u)->setDetected(false);
					(*u)->setHyperDetected(false);
					if (!(*u)->getFollowers()->empty())
					{
						popup(new UfoLostState((*u)->getName(_game->getLanguage())));
					}
				}
			}
			break;
		case Ufo::CRASHED:
		case Ufo::DESTROYED:
			break;
		}
	}

	// Processes MissionSites
	for (std::vector<MissionSite*>::iterator site = _game->getSavedGame()->getMissionSites()->begin(); site != _game->getSavedGame()->getMissionSites()->end();)
	{
		if (processMissionSite(*site))
		{
			site = _game->getSavedGame()->getMissionSites()->erase(site);
		}
		else
		{
			++site;
		}
	}
}

/**
 * Takes care of any game logic that has to
 * run every game hour, like transfers.
 */
void GeoscapeState::time1Hour()
{
	// Handle craft maintenance
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		for (std::vector<Craft*>::iterator j = (*i)->getCrafts()->begin(); j != (*i)->getCrafts()->end(); ++j)
		{
			if ((*j)->getStatus() == "STR_REPAIRS")
			{
				(*j)->repair();
			}
			else if ((*j)->getStatus() == "STR_REARMING")
			{
				std::string s = (*j)->rearm(_game->getMod());
				if (!s.empty())
				{
					std::string msg = tr("STR_NOT_ENOUGH_ITEM_TO_REARM_CRAFT_AT_BASE")
									   .arg(tr(s))
									   .arg((*j)->getName(_game->getLanguage()))
									   .arg((*i)->getName());
					popup(new CraftErrorState(this, msg));
				}
			}
		}
	}

	// Handle transfers
	bool window = false;
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		for (std::vector<Transfer*>::iterator j = (*i)->getTransfers()->begin(); j != (*i)->getTransfers()->end(); ++j)
		{
			(*j)->advance(*i);
			if (!window && (*j)->getHours() <= 0)
			{
				window = true;
			}
		}
	}
	if (window)
	{
		popup(new ItemsArrivingState(this));
	}
	// Handle Production
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		std::map<Production*, productionProgress_e> toRemove;
		for (std::vector<Production*>::const_iterator j = (*i)->getProductions().begin(); j != (*i)->getProductions().end(); ++j)
		{
			toRemove[(*j)] = (*j)->step((*i), _game->getSavedGame(), _game->getMod());
		}
		for (std::map<Production*, productionProgress_e>::iterator j = toRemove.begin(); j != toRemove.end(); ++j)
		{
			if (j->second > PROGRESS_NOT_COMPLETE)
			{
				popup(new ProductionCompleteState((*i),  tr(j->first->getRules()->getName()), this, j->second));
				(*i)->removeProduction(j->first);
			}
		}

		if (Options::storageLimitsEnforced && (*i)->storesOverfull())
		{
			popup(new ErrorMessageState(tr("STR_STORAGE_EXCEEDED").arg((*i)->getName()), _palette, _game->getMod()->getInterface("geoscape")->getElement("errorMessage")->color, "BACK13.SCR", _game->getMod()->getInterface("geoscape")->getElement("errorPalette")->color));
			popup(new SellState((*i)));
		}
	}
	for (std::vector<MissionSite*>::iterator i = _game->getSavedGame()->getMissionSites()->begin(); i != _game->getSavedGame()->getMissionSites()->end(); ++i)
	{
		if (!(*i)->getDetected())
		{
			(*i)->setDetected(true);
			popup(new MissionDetectedState(*i, this));
			break;
		}
	}
}

/**
 * This class will attempt to generate a supply mission for a base.
 * Each alien base has a 6/101 chance to generate a supply mission.
 */
class GenerateSupplyMission: public std::unary_function<const AlienBase *, void>
{
public:
	/// Store rules and game data references for later use.
	GenerateSupplyMission(const Mod &mod, SavedGame &save) : _mod(mod), _save(save) { /* Empty by design */ }
	/// Check and spawn mission.
	void operator()(const AlienBase *base) const;
private:
	const Mod &_mod;
	SavedGame &_save;
};

/**
 * Check and create supply mission for the given base.
 * There is a 6/101 chance of the mission spawning.
 * @param base A pointer to the alien base.
 */
void GenerateSupplyMission::operator()(const AlienBase *base) const
{
	std::string missionName = base->getDeployment()->chooseGenMissionType();
	if (_mod.getAlienMission(missionName))
	{
		if (RNG::percent(base->getDeployment()->getGenMissionFrequency()))
		{
			//Spawn supply mission for this base.
			const RuleAlienMission &rule = *_mod.getAlienMission(missionName);
			AlienMission *mission = new AlienMission(rule);
			mission->setRegion(_save.locateRegion(*base)->getRules()->getType(), _mod);
			mission->setId(_save.getId("ALIEN_MISSIONS"));
			mission->setRace(base->getAlienRace());
			mission->setAlienBase(base);
			mission->start();
			_save.getAlienMissions().push_back(mission);
		}
	}
	else if (!missionName.empty())
	{
		throw Exception("Alien Base tried to generate undefined mission: " + missionName);
	}
}

/**
 * Takes care of any game logic that has to
 * run every game day, like constructions.
 */
void GeoscapeState::time1Day()
{
	for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
	{
		// Handle facility construction
		for (std::vector<BaseFacility*>::iterator j = (*i)->getFacilities()->begin(); j != (*i)->getFacilities()->end(); ++j)
		{
			if ((*j)->getBuildTime() > 0)
			{
				(*j)->build();
				if ((*j)->getBuildTime() == 0)
				{
					popup(new ProductionCompleteState((*i),  tr((*j)->getRules()->getType()), this, PROGRESS_CONSTRUCTION));
				}
			}
		}

		// Handle science project
		// 1. gather finished research
		std::vector<ResearchProject*> finished;
		for (std::vector<ResearchProject*>::const_iterator iter = (*i)->getResearch().begin(); iter != (*i)->getResearch().end(); ++iter)
		{
			if ((*iter)->step())
			{
				finished.push_back(*iter);
			}
		}
		// 2. remember available research before adding new finished research
		std::vector<RuleResearch *> before;
		if (!finished.empty())
		{
			_game->getSavedGame()->getAvailableResearchProjects(before, _game->getMod(), *i);
		}
		// 3. add finished research, including lookups and getonefrees (up to 4x)
		for (std::vector<ResearchProject*>::iterator iter = finished.begin(); iter != finished.end(); ++iter)
		{
			const RuleResearch *bonus = 0;
			const RuleResearch *research = (*iter)->getRules();

			// 3a. remove finished research from the base where it was researched
			(*i)->removeResearch(*iter);
			(*iter) = 0;

			// 3b. handle interrogation
			if (Options::retainCorpses && research->destroyItem() && _game->getMod()->getUnit(research->getName()))
			{
				(*i)->getStorageItems()->addItem(_game->getMod()->getArmor(_game->getMod()->getUnit(research->getName())->getArmor(), true)->getCorpseGeoscape());
			}
			// 3c. handle getonefrees (topic+lookup)
			if (!research->getGetOneFree().empty())
			{
				std::vector<std::string> possibilities;
				for (std::vector<std::string>::const_iterator f = research->getGetOneFree().begin(); f != research->getGetOneFree().end(); ++f)
				{
					if (!_game->getSavedGame()->isResearched(*f, false))
					{
						possibilities.push_back(*f);
					}
				}
				if (!possibilities.empty())
				{
					size_t pick = RNG::generate(0, possibilities.size()-1);
					std::string sel = possibilities.at(pick);
					bonus = _game->getMod()->getResearch(sel, true);
					_game->getSavedGame()->addFinishedResearch(bonus, _game->getMod(), (*i));
					if (!bonus->getLookup().empty())
					{
						_game->getSavedGame()->addFinishedResearch(_game->getMod()->getResearch(bonus->getLookup(), true), _game->getMod(), (*i));
					}
				}
			}
			// 3d. determine and remember if the ufopedia article should pop up again or not
			// Note: because different topics may lead to the same lookup
			const RuleResearch * newResearch = research;
			std::string name = research->getLookup().empty() ? research->getName() : research->getLookup();
			if (_game->getSavedGame()->isResearched(name, false))
			{
				newResearch = 0;
			}
			// 3e. handle core research (topic+lookup)
			_game->getSavedGame()->addFinishedResearch(research, _game->getMod(), (*i));
			if (!research->getLookup().empty())
			{
				_game->getSavedGame()->addFinishedResearch(_game->getMod()->getResearch(research->getLookup(), true), _game->getMod(), (*i));
			}
			// 3e. handle cutscenes
			if (!research->getCutscene().empty())
			{
				popup(new CutsceneState(research->getCutscene()));
			}
			if (bonus && !bonus->getCutscene().empty())
			{
				popup(new CutsceneState(bonus->getCutscene()));
			}
			// 3e. handle research complete popup + ufopedia article popups (topic+bonus)
			popup(new ResearchCompleteState(newResearch, bonus, research));
			// 3f. reset timer
			timerReset();
			// 3g. warning if weapon is researched before its clip
			if (newResearch)
			{
				RuleItem *item = _game->getMod()->getItem(newResearch->getName());
				if (item && item->getBattleType() == BT_FIREARM && !item->getCompatibleAmmo()->empty())
				{
					RuleManufacture *man = _game->getMod()->getManufacture(item->getType());
					if (man && !man->getRequirements().empty())
					{
						const std::vector<std::string> &req = man->getRequirements();
						RuleItem *ammo = _game->getMod()->getItem(item->getCompatibleAmmo()->front());
						if (ammo && std::find(req.begin(), req.end(), ammo->getType()) != req.end() && !_game->getSavedGame()->isResearched(req, true))
						{
							popup(new ResearchRequiredState(item));
						}
					}
				}
			}
			// 3h. inform about new possible research
			std::vector<RuleResearch *> after;
			_game->getSavedGame()->getAvailableResearchProjects(after, _game->getMod(), *i);
			std::vector<RuleResearch *> newPossibleResearch;
			_game->getSavedGame()->getNewlyAvailableResearchProjects(before, after, newPossibleResearch);
			popup(new NewPossibleResearchState(*i, newPossibleResearch));
			// 3i. inform about new possible manufacture
			std::vector<RuleManufacture *> newPossibleManufacture;
			_game->getSavedGame()->getDependableManufacture(newPossibleManufacture, research, _game->getMod(), *i);
			if (!newPossibleManufacture.empty())
			{
				popup(new NewPossibleManufactureState(*i, newPossibleManufacture));
			}
			// 3j. now iterate through all the bases and remove this project from their labs (unless it can still yield more stuff!)
			for (std::vector<Base*>::iterator j = _game->getSavedGame()->getBases()->begin(); j != _game->getSavedGame()->getBases()->end(); ++j)
			{
				for (std::vector<ResearchProject*>::const_iterator iter2 = (*j)->getResearch().begin(); iter2 != (*j)->getResearch().end(); ++iter2)
				{
					if (research->getName() == (*iter2)->getRules()->getName())
					{
						if (!_game->getSavedGame()->isResearched(research->getGetOneFree(), false))
						{
							// This research topic still has some more undiscovered "getOneFree" topics, keep it!
						}
						else if (_game->getSavedGame()->hasUndiscoveredProtectedUnlock(research, _game->getMod()))
						{
							// This research topic still has one or more undiscovered "protected unlocks", keep it!
						}
						else
						{
							// This topic can't give you anything else anymore, remove it!
							(*j)->removeResearch(*iter2);
							break;
						}
					}
				}
			}
		}

		// Handle soldier wounds
		for (std::vector<Soldier*>::iterator j = (*i)->getSoldiers()->begin(); j != (*i)->getSoldiers()->end(); ++j)
		{
			if ((*j)->getWoundRecovery() > 0)
			{
				(*j)->heal();
			}
		}
		// Handle psionic training
		if ((*i)->getAvailablePsiLabs() > 0 && Options::anytimePsiTraining)
		{
			for (std::vector<Soldier*>::const_iterator s = (*i)->getSoldiers()->begin(); s != (*i)->getSoldiers()->end(); ++s)
			{
				(*s)->trainPsi1Day();
				(*s)->calcStatString(_game->getMod()->getStatStrings(), (Options::psiStrengthEval && _game->getSavedGame()->isResearched(_game->getMod()->getPsiRequirements())));
			}
		}
	}
	// handle regional and country points for alien bases
	for (std::vector<AlienBase*>::const_iterator b = _game->getSavedGame()->getAlienBases()->begin(); b != _game->getSavedGame()->getAlienBases()->end(); ++b)
	{
		for (std::vector<Region*>::iterator k = _game->getSavedGame()->getRegions()->begin(); k != _game->getSavedGame()->getRegions()->end(); ++k)
		{
			if ((*k)->getRules()->insideRegion((*b)->getLongitude(), (*b)->getLatitude()))
			{
				(*k)->addActivityAlien((*b)->getDeployment()->getPoints());
				break;
			}
		}
		for (std::vector<Country*>::iterator k = _game->getSavedGame()->getCountries()->begin(); k != _game->getSavedGame()->getCountries()->end(); ++k)
		{
			if ((*k)->getRules()->insideCountry((*b)->getLongitude(), (*b)->getLatitude()))
			{
				(*k)->addActivityAlien((*b)->getDeployment()->getPoints());
				break;
			}
		}
	}

	// Handle resupply of alien bases.
	std::for_each(_game->getSavedGame()->getAlienBases()->begin(), _game->getSavedGame()->getAlienBases()->end(),
			  GenerateSupplyMission(*_game->getMod(), *_game->getSavedGame()));

	// Autosave 3 times a month
	int day = _game->getSavedGame()->getTime()->getDay();
	if (day == 10 || day == 20)
	{
		if (_game->getSavedGame()->isIronman())
		{
			popup(new SaveGameState(OPT_GEOSCAPE, SAVE_IRONMAN, _palette));
		}
		else if (Options::autosave)
		{
			popup(new SaveGameState(OPT_GEOSCAPE, SAVE_AUTO_GEOSCAPE, _palette));
		}
	}
}

/**
 * Takes care of any game logic that has to
 * run every game month, like funding.
 */
void GeoscapeState::time1Month()
{
	_game->getSavedGame()->addMonth();

	// Determine alien mission for this month.
	determineAlienMissions();

	// Handle Psi-Training and initiate a new retaliation mission, if applicable
	bool psi = false;
	if (!Options::anytimePsiTraining)
	{
		for (std::vector<Base*>::const_iterator b = _game->getSavedGame()->getBases()->begin(); b != _game->getSavedGame()->getBases()->end(); ++b)
		{
			if ((*b)->getAvailablePsiLabs() > 0)
			{
				psi = true;
				for (std::vector<Soldier*>::const_iterator s = (*b)->getSoldiers()->begin(); s != (*b)->getSoldiers()->end(); ++s)
				{
					if ((*s)->isInPsiTraining())
					{
						(*s)->trainPsi();
						(*s)->calcStatString(_game->getMod()->getStatStrings(), (Options::psiStrengthEval && _game->getSavedGame()->isResearched(_game->getMod()->getPsiRequirements())));
					}
				}
			}
		}
	}

	// Handle funding
	timerReset();
	_game->getSavedGame()->monthlyFunding();
	popup(new MonthlyReportState(psi, _globe));

	// Handle Xcom Operatives discovering bases
	if (!_game->getSavedGame()->getAlienBases()->empty() && RNG::percent(20))
	{
		for (std::vector<AlienBase*>::const_iterator b = _game->getSavedGame()->getAlienBases()->begin(); b != _game->getSavedGame()->getAlienBases()->end(); ++b)
		{
			if (!(*b)->isDiscovered())
			{
				(*b)->setDiscovered(true);
				popup(new AlienBaseState(*b, this));
				break;
			}
		}
	}
}

/**
 * Slows down the timer back to minimum speed,
 * for when important events occur.
 */
void GeoscapeState::timerReset()
{
	selectGeoscapeSpeed(_btn5Secs);
}

/**
 * Adds a new popup window to the queue
 * (this prevents popups from overlapping)
 * and pauses the game timer respectively.
 * @param state Pointer to popup state.
 */
void GeoscapeState::popup(State *state)
{
	_pause = true;
	_popups.push_back(state);
}

/**
 * Returns a pointer to the Geoscape globe for
 * access by other substates.
 * @return Pointer to globe.
 */
Globe *GeoscapeState::getGlobe() const
{
	return _globe;
}

/**
 * Processes any left-clicks on globe markers,
 * or right-clicks to scroll the globe.
 * @param action Pointer to an action.
 */

void GeoscapeState::globeClick(Action *action)
{
	int mouseX = (int)floor(action->getAbsoluteXMouse()), mouseY = (int)floor(action->getAbsoluteYMouse());

	// Clicking markers on the globe
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		std::vector<Target*> v = _globe->getTargets(mouseX, mouseY, false);
		if (!v.empty())
		{
			_game->pushState(new MultipleTargetsState(v, 0, this));
		}
	}

	if (_game->getSavedGame()->getDebugMode())
	{
		double lon, lat;
		int texture, shade;
		_globe->cartToPolar(mouseX, mouseY, &lon, &lat);
		double lonDeg = lon / M_PI * 180, latDeg = lat / M_PI * 180;
		_globe->getPolygonTextureAndShade(lon, lat, &texture, &shade);
		std::ostringstream ss;
		ss << "rad: " << lon << ", " << lat << std::endl;
		ss << "deg: " << lonDeg << ", " << latDeg << std::endl;
		ss << "texture: " << texture << ", shade: " << shade << std::endl;

		_txtDebug->setText(ss.str());
	}
}

/**
 * Opens the Intercept window.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnInterceptClick(Action *)
{
	if (buttonsDisabled())
	{
		return;
	}
	_game->pushState(new InterceptState(_globe));
}

/**
 * Goes to the Basescape screen.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnBasesClick(Action *)
{
	if (buttonsDisabled())
	{
		return;
	}
	timerReset();
	if (!_game->getSavedGame()->getBases()->empty())
	{
		_game->pushState(new BasescapeState(_game->getSavedGame()->getSelectedBase(), _globe));
	}
	else
	{
		_game->pushState(new BasescapeState(0, _globe));
	}
}

/**
 * Goes to the Graphs screen.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnGraphsClick(Action *)
{
	if (buttonsDisabled())
	{
		return;
	}
	_game->pushState(new GraphsState);
}

/**
 * Goes to the Ufopaedia window.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnUfopaediaClick(Action *)
{
	if (buttonsDisabled())
	{
		return;
	}
	Ufopaedia::open(_game);
}

/**
 * Opens the Options window.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnOptionsClick(Action *)
{
	if (buttonsDisabled())
	{
		return;
	}
	_game->pushState(new PauseState(OPT_GEOSCAPE));
}

/**
 * Goes to the Funding screen.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnFundingClick(Action *)
{
	if (buttonsDisabled())
	{
		return;
	}
	_game->pushState(new FundingState);
}

/**
 * Starts rotating the globe to the left.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnRotateLeftPress(Action *)
{
	_globe->rotateLeft();
}

/**
 * Stops rotating the globe to the left.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnRotateLeftRelease(Action *)
{
	_globe->rotateStopLon();
}

/**
 * Starts rotating the globe to the right.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnRotateRightPress(Action *)
{
	_globe->rotateRight();
}

/**
 * Stops rotating the globe to the right.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnRotateRightRelease(Action *)
{
	_globe->rotateStopLon();
}

/**
 * Starts rotating the globe upwards.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnRotateUpPress(Action *)
{
	_globe->rotateUp();
}

/**
 * Stops rotating the globe upwards.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnRotateUpRelease(Action *)
{
	_globe->rotateStopLat();
}

/**
 * Starts rotating the globe downwards.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnRotateDownPress(Action *)
{
	_globe->rotateDown();
}

/**
 * Stops rotating the globe downwards.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnRotateDownRelease(Action *)
{
	_globe->rotateStopLat();
}

/**
 * Zooms into the globe.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnZoomInLeftClick(Action *)
{
	_globe->zoomIn();
}

/**
 * Zooms the globe maximum.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnZoomInRightClick(Action *)
{
	_globe->zoomMax();
}

/**
 * Zooms out of the globe.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnZoomOutLeftClick(Action *)
{
	_globe->zoomOut();
}

/**
 * Zooms the globe minimum.
 * @param action Pointer to an action.
 */
void GeoscapeState::btnZoomOutRightClick(Action *)
{
	_globe->zoomMin();
}

/**
 * Zoom in effect for dogfights.
 */
void GeoscapeState::zoomInEffect()
{
	if (_globe->zoomDogfightIn())
	{
		_zoomInEffectDone = true;
		_zoomInEffectTimer->stop();
	}
}

/**
 * Zoom out effect for dogfights.
 */
void GeoscapeState::zoomOutEffect()
{
	if (_globe->zoomDogfightOut())
	{
		_zoomOutEffectDone = true;
		_zoomOutEffectTimer->stop();
		init();
	}
}

/**
 * Dogfight logic. Moved here to have the code clean.
 */
void GeoscapeState::handleDogfights()
{
	// Handle dogfights logic.
	_minimizedDogfights = 0;

	std::list<DogfightState*>::iterator d = _dogfights.begin();
	for (; d != _dogfights.end(); ++d)
	{
		(*d)->getUfo()->setInterceptionProcessed(false);
	}
	d = _dogfights.begin();
	while (d != _dogfights.end())
	{
		if ((*d)->isMinimized())
		{
			if ((*d)->getWaitForPoly() && _globe->insideLand((*d)->getUfo()->getLongitude(), (*d)->getUfo()->getLatitude()))
			{
				(*d)->setMinimized(false);
				(*d)->setWaitForPoly(false);
			}
			else if ((*d)->getWaitForAltitude() && (*d)->getUfo()->getAltitudeInt() <= (*d)->getCraft()->getRules()->getMaxAltitude())
			{
				(*d)->setMinimized(false);
				(*d)->setWaitForAltitude(false);
			}
			else
			{
				_minimizedDogfights++;
			}
		}
		else
		{
			_globe->rotateStop();
		}
		(*d)->think();
		if ((*d)->dogfightEnded())
		{
			if ((*d)->isMinimized())
			{
				_minimizedDogfights--;
			}
			delete *d;
			d = _dogfights.erase(d);
		}
		else
		{
			++d;
		}
	}
	if (_dogfights.empty())
	{
		_dogfightTimer->stop();
		_zoomOutEffectTimer->start();
	}
}

/**
 * Gets the number of minimized dogfights.
 * @return Number of minimized dogfights.
 */
int GeoscapeState::minimizedDogfightsCount()
{
	int minimizedDogfights = 0;
	for (std::list<DogfightState*>::iterator d = _dogfights.begin(); d != _dogfights.end(); ++d)
	{
		if ((*d)->isMinimized())
		{
			++minimizedDogfights;
		}
	}
	return minimizedDogfights;
}

/**
 * Starts a new dogfight.
 */
void GeoscapeState::startDogfight()
{
	if (_globe->getZoom() < 3)
	{
		if (!_zoomInEffectTimer->isRunning())
		{
			_globe->saveZoomDogfight();
			_globe->rotateStop();
			_zoomInEffectTimer->start();
		}
	}
	else
	{
		_dogfightStartTimer->stop();
		_zoomInEffectTimer->stop();
		_dogfightTimer->start();
		timerReset();
		while (!_dogfightsToBeStarted.empty())
		{
			_dogfights.push_back(_dogfightsToBeStarted.back());
			_dogfightsToBeStarted.pop_back();
			_dogfights.back()->setInterceptionNumber(getFirstFreeDogfightSlot());
			_dogfights.back()->setInterceptionsCount(_dogfights.size() + _dogfightsToBeStarted.size());
		}
		// Set correct number of interceptions for every dogfight.
		for (std::list<DogfightState*>::iterator d = _dogfights.begin(); d != _dogfights.end(); ++d)
		{
			(*d)->setInterceptionsCount(_dogfights.size());
		}
	}
}

/**
 * Returns the first free dogfight slot.
 * @return free slot
 */
int GeoscapeState::getFirstFreeDogfightSlot()
{
	int slotNo = 1;
	for (std::list<DogfightState*>::iterator d = _dogfights.begin(); d != _dogfights.end(); ++d)
	{
		if ((*d)->getInterceptionNumber() == slotNo)
		{
			++slotNo;
		}
	}
	return slotNo;
}

/**
 * Handle base defense
 * @param base Base to defend.
 * @param ufo Ufo attacking base.
 */
void GeoscapeState::handleBaseDefense(Base *base, Ufo *ufo)
{
	// Whatever happens in the base defense, the UFO has finished its duty
	ufo->setStatus(Ufo::DESTROYED);

	if (base->getAvailableSoldiers(true) > 0 || !base->getVehicles()->empty())
	{
		SavedBattleGame *bgame = new SavedBattleGame();
		_game->getSavedGame()->setBattleGame(bgame);
		bgame->setMissionType("STR_BASE_DEFENSE");
		BattlescapeGenerator bgen = BattlescapeGenerator(_game);
		bgen.setBase(base);
		bgen.setAlienRace(ufo->getAlienRace());
		bgen.run();
		_pause = true;
		_game->pushState(new BriefingState(0, base));
	}
	else
	{
		// Please garrison your bases in future
		popup(new BaseDestroyedState(base));
	}
}

/**
 * Determine the alien missions to start this month.
 */
void GeoscapeState::determineAlienMissions()
{
	SavedGame *save = _game->getSavedGame();
	AlienStrategy &strategy = save->getAlienStrategy();
	Mod *mod = _game->getMod();
	int month = _game->getSavedGame()->getMonthsPassed();
	std::vector<RuleMissionScript*> availableMissions;
	std::map<int, bool> conditions;

	// well, here it is, ladies and gents, the nuts and bolts behind the geoscape mission scheduling.

	// first we need to build a list of "valid" commands
	for (std::vector<std::string>::const_iterator i = mod->getMissionScriptList()->begin(); i != mod->getMissionScriptList()->end(); ++i)
	{
		RuleMissionScript *command = mod->getMissionScript(*i);

			// level one condition check: make sure we're within our time constraints
		if (command->getFirstMonth() <= month &&
			(command->getLastMonth() >= month || command->getLastMonth() == -1) &&
			// make sure we haven't hit our run limit, if we have one
			(command->getMaxRuns() == -1 ||	command->getMaxRuns() > strategy.getMissionsRun(command->getVarName())) &&
			// and make sure we satisfy the difficulty restrictions
			command->getMinDifficulty() <= save->getDifficulty())
		{
			// level two condition check: make sure we meet any research requirements, if any.
			bool triggerHappy = true;
			for (std::map<std::string, bool>::const_iterator j = command->getResearchTriggers().begin(); triggerHappy && j != command->getResearchTriggers().end(); ++j)
			{
				triggerHappy = (save->isResearched(j->first) == j->second);
			}
			// levels one and two passed: insert this command into the array.
			if (triggerHappy)
			{
				availableMissions.push_back(command);
			}
		}
	}

	// start processing command array.
	for (std::vector<RuleMissionScript*>::const_iterator i = availableMissions.begin(); i != availableMissions.end(); ++i)
	{
		RuleMissionScript *command = *i;
		bool process = true;
		bool success = false;
		// level three condition check: make sure our conditionals are met, if any. this list is dynamic, and must be checked here.
		for (std::vector<int>::const_iterator j = command->getConditionals().begin(); process && j != command->getConditionals().end(); ++j)
		{
			std::map<int, bool>::const_iterator found = conditions.find(std::abs(*j));
			// just an FYI: if you add a 0 to your conditionals, this flag will never resolve to true, and your command will never run.
			process = (found == conditions.end() || (found->second == true && *j > 0) || (found->second == false && *j < 0));
		}
		if (command->getLabel() > 0 && conditions.find(command->getLabel()) != conditions.end())
		{
			std::ostringstream ss;
			ss << "Mission generator encountered an error: multiple commands: " << command->getType() << " and ";
			for (std::vector<RuleMissionScript*>::const_iterator j = availableMissions.begin(); j != availableMissions.end(); ++j)
			{
				if (command->getLabel() == (*j)->getLabel() && (*j) != (*i))
				{
					ss << (*j)->getType() << ", ";
				}
			}
			ss  << "are sharing the same label: " << command->getLabel();
			throw Exception(ss.str());
		}
		// level four condition check: does random chance favour this command's execution?
		if (process && RNG::percent(command->getExecutionOdds()))
		{
			// good news, little command pointer! you're FDA approved! off to the main processing facility with you!
			success = processCommand(command);
		}
		if (command->getLabel() > 0)
		{
			// tsk, tsk. you really should be careful with these unique labels, they're supposed to be unique.
			if (conditions.find(command->getLabel()) != conditions.end())
			{
				throw Exception("Error in mission scripts: " + command->getType() + ". Two or more commands sharing the same label. That's bad, Mmmkay?");
			}
			// keep track of what happened to this command, so others may reference it.
			conditions[command->getLabel()] = success;
		}
	}
}


/**
 * Proccesses a directive to start up a mission, if possible.
 * @param command the directive from which to read information.
 * @return whether the command successfully produced a new mission.
 */
bool GeoscapeState::processCommand(RuleMissionScript *command)
{
	SavedGame *save = _game->getSavedGame();
	AlienStrategy &strategy = save->getAlienStrategy();
	Mod *mod = _game->getMod();
	int month = _game->getSavedGame()->getMonthsPassed();
	std::string targetRegion;
	const RuleAlienMission *missionRules;
	std::string missionType;
	std::string missionRace;
	int targetZone = -1;

	// terror mission type deal? this will require special handling.
	if (command->getSiteType())
	{
		// we know for a fact that this command has mission weights defined, otherwise this flag could not be set.
		missionType = command->generate(month, GEN_MISSION);
		std::vector<std::string> missions = command->getMissionTypes(month);
		int maxMissions = missions.size();
		bool targetBase = RNG::percent(command->getTargetBaseOdds());
		int currPos = 0;
		for (; currPos != maxMissions; ++currPos)
		{
			if (missions[currPos] == missionType)
			{
				break;
			}
		}

		// let's build a list of regions with spawn zones to pick from
		std::vector<std::pair<std::string, int> > validAreas;

		// this is actually a bit of a cheat, we ARE using the mission weights as defined, but we'll try them all if the one we pick first isn't valid.
		for (int h = 0; h != maxMissions; ++h)
		{
			// we'll use the regions listed in the command, if any, otherwise check all the regions in the ruleset looking for matches
			std::vector<std::string> regions = (command->hasRegionWeights()) ? command->getRegions(month) : mod->getRegionsList();
			missionRules = mod->getAlienMission(missionType, true);
			targetZone = missionRules->getSpawnZone();

			if (targetBase)
			{
				std::vector<std::string> regionsToKeep;
				//if we're targetting a base, we ignore regions that don't contain bases, simple.
				for (std::vector<Base*>::iterator i = save->getBases()->begin(); i != save->getBases()->end(); ++i)
				{
					regionsToKeep.push_back(save->locateRegion((*i)->getLongitude(), (*i)->getLatitude())->getRules()->getType());
				}
				for (std::vector<std::string>::iterator i = regions.begin(); i != regions.end();)
				{
					if (std::find(regionsToKeep.begin(), regionsToKeep.end(), *i) == regionsToKeep.end())
					{
						i = regions.erase(i);
					}
					else
					{
						++i;
					}
				}
			}

			for (std::vector<std::string>::iterator i = regions.begin(); i != regions.end();)
			{
				// we don't want the same mission running in any given region twice simultaneously, so prune the list as needed.
				bool processThisRegion = true;
				for (std::vector<AlienMission*>::const_iterator j = save->getAlienMissions().begin(); j != save->getAlienMissions().end(); ++j)
				{
					if ((*j)->getRules().getType() == missionRules->getType() && (*j)->getRegion() == *i)
					{
						processThisRegion = false;
						break;
					}
				}
				if (!processThisRegion)
				{
					i = regions.erase(i);
					continue;
				}
				// ok, we found a region that doesn't have our mission in it, let's see if it has an appropriate landing zone.
				// if it does, let's add it to our list of valid areas, taking note of which mission area(s) matched.
				RuleRegion *region = mod->getRegion(*i, true);
				if ((int)(region->getMissionZones().size()) > targetZone)
				{
					std::vector<MissionArea> areas = region->getMissionZones()[targetZone].areas;
					int counter = 0;
					for (std::vector<MissionArea>::const_iterator j = areas.begin(); j != areas.end(); ++j)
					{
						// validMissionLocation checks to make sure this city/whatever hasn't been used by the last n missions using this varName
						// this prevents the same location getting hit more than once every n missions.
						if ((*j).isPoint() && strategy.validMissionLocation(command->getVarName(), region->getType(), counter))
						{
							validAreas.push_back(std::make_pair(region->getType(), counter));
						}
						counter++;
					}
				}
				++i;
			}

			// oh bother, we couldn't find anything valid, this mission won't run this month.
			if (validAreas.empty())
			{
				if (maxMissions > 1 && ++currPos == maxMissions)
				{
					currPos = 0;
				}
				missionType = missions[currPos];
			}
			else
			{
				break;
			}
		}

		if (validAreas.empty())
		{
			// now we're in real trouble, we've managed to make it out of the loop and we still don't have any valid choices
			// this command cannot run this month, we have failed, forgive us senpai.
			return false;
		}
		// reset this, we may have used it earlier, it longer represents the target zone type, but the target zone number within that type
		targetZone = -1;
		// everything went according to plan: we can now pick a city/whatever to attack.
		while (targetZone == -1)
		{
			if (command->hasRegionWeights())
			{
				// if we have a weighted region list, we know we have at least one valid choice for this mission
				targetRegion = command->generate(month, GEN_REGION);
			}
			else
			{
				// if we don't have a weighted list, we'll select a region at random from the ruleset,
				// validate that it's in our list, and pick one of its cities at random
				// this will give us an even distribution between regions regardless of the number of cities.
				targetRegion = mod->getRegionsList().at(RNG::generate(0, mod->getRegionsList().size() - 1));
			}

			// we need to know the range of the region within our vector, in order to randomly select a city from it
			int min = -1;
			int max = -1;
			int curr = 0;
			for (std::vector<std::pair<std::string, int> >::const_iterator i = validAreas.begin(); i != validAreas.end(); ++i)
			{
				if ((*i).first == targetRegion)
				{
					if (min == -1)
					{
						min = curr;
					}
					max = curr;
				}
				else if (min > -1)
				{
					// if we've stopped detecting matches, we're done looking.
					break;
				}
				++curr;
			}
			if (min != -1)
			{
				// we have our random range, we can make a selection, and we're done.
				targetZone = validAreas[RNG::generate(min, max)].second;
			}
		}
		// now add that city to the list of sites we've hit, store the array, etc.
		strategy.addMissionLocation(command->getVarName(), targetRegion, targetZone, command->getRepeatAvoidance());
	}
	else if (RNG::percent(command->getTargetBaseOdds()))
	{
		// build a list of the mission types we're dealing with, if any
		std::vector<std::string> types = command->getMissionTypes(month);
		// now build a list of regions with bases in.
		std::vector<std::string> regionsMaster;
		for (std::vector<Base*>::const_iterator i = save->getBases()->begin(); i != save->getBases()->end(); ++i)
		{
			regionsMaster.push_back(save->locateRegion(*(*i))->getRules()->getType());
		}
		// no defined mission types? then we'll prune the region list to ensure we only have a region that can generate a mission.
		if (types.empty())
		{
			for (std::vector<std::string>::iterator i = regionsMaster.begin(); i != regionsMaster.end();)
			{
				if (!strategy.validMissionRegion(*i))
				{
					i = regionsMaster.erase(i);
					continue;
				}
				++i;
			}
			// no valid missions in any base regions? oh dear, i guess we failed.
			if (regionsMaster.empty())
			{
				return false;
			}
			// pick a random region from our list
			targetRegion = regionsMaster[RNG::generate(0, regionsMaster.size()-1)];
		}
		else
		{
			// we don't care about regional mission distributions, we're targetting a base with whatever mission we pick, so let's pick now
			// we'll iterate the mission list, starting at a random point, and wrapping around to the beginning
			int max = types.size();
			int entry = RNG::generate(0,  max - 1);
			std::vector<std::string> regions;

			for (int i = 0; i != max; ++i)
			{
				regions = regionsMaster;
				for (std::vector<AlienMission*>::const_iterator j = save->getAlienMissions().begin(); j != save->getAlienMissions().end(); ++j)
				{
					// if the mission types match
					if (types[entry] == (*j)->getRules().getType())
					{
						for (std::vector<std::string>::iterator k = regions.begin(); k != regions.end();)
						{
							// and the regions match
							if ((*k) == (*j)->getRegion())
							{
								// prune the entry from the list
								k = regions.erase(k);
								continue;
							}
							++k;
						}
					}
				}

				// we have a valid list of regions containing bases, pick one.
				if (!regions.empty())
				{
					missionType = types[entry];
					targetRegion = regions[RNG::generate(0, regions.size()-1)];
					break;
				}
				// otherwise, try the next mission in the list.
				if (max > 1 && ++entry == max)
				{
					entry = 0;
				}
			}
		}
	}
	// now the easy stuff
	else if (!command->hasRegionWeights())
	{
		// no regionWeights means we pick from the table
		targetRegion = strategy.chooseRandomRegion(mod);
	}
	else
	{
		// otherwise, let the command dictate the region.
		targetRegion = command->generate(month, GEN_REGION);
	}

	if (targetRegion.empty())
	{
		// something went horribly wrong, we should have had at LEAST a region by now.
		return false;
	}

	// we're bound to end up with typos, so let's throw an exception instead of simply returning false
	// that way, the modder can fix their mistake
	if (mod->getRegion(targetRegion) == 0)
	{
		throw Exception("Error proccessing mission script named: " + command->getType() + ", region named: " + targetRegion + " is not defined");
	}

	if (missionType.empty()) // ie: not a terror mission, not targetting a base, or otherwise not already chosen
	{
		if (!command->hasMissionWeights())
		{
			// no weights means let the strategy pick
			missionType = strategy.chooseRandomMission(targetRegion);
		}
		else
		{
			// otherwise the command gives us the weights.
			missionType = command->generate(month, GEN_MISSION);
		}
	}

	if (missionType.empty())
	{
		// something went horribly wrong, we didn't manage to choose a mission type
		return false;
	}

	missionRules = mod->getAlienMission(missionType);

	// we're bound to end up with typos, so let's throw an exception instead of simply returning false
	// that way, the modder can fix their mistake
	if (missionRules == 0)
	{
		throw Exception("Error proccessing mission script named: " + command->getType() + ", mission type: " + missionType + " is not defined");
	}

	// do i really need to comment this? shouldn't it be obvious what's happening here?
	if (!command->hasRaceWeights())
	{
		missionRace = missionRules->generateRace(month);
	}
	else
	{
		missionRace = command->generate(month, GEN_RACE);
	}

	if (missionRace.empty())
	{
		throw Exception("Error proccessing mission script named: " + command->getType() + ", mission type: " + missionType + " has no available races");
	}

	// we're bound to end up with typos, so let's throw an exception instead of simply returning false
	// that way, the modder can fix their mistake
	if (mod->getAlienRace(missionRace) == 0)
	{
		throw Exception("Error proccessing mission script named: " + command->getType() + ", race: " + missionRace + " is not defined");
	}

	// ok, we've derived all the variables we need to start up our mission, let's do magic to turn those values into a mission
	AlienMission *mission = new AlienMission(*missionRules);
	mission->setRace(missionRace);
	mission->setId(_game->getSavedGame()->getId("ALIEN_MISSIONS"));
	mission->setRegion(targetRegion, *_game->getMod());
	mission->setMissionSiteZone(targetZone);
	strategy.addMissionRun(command->getVarName());
	mission->start(command->getDelay());
	_game->getSavedGame()->getAlienMissions().push_back(mission);
	// if this flag is set, we want to delete it from the table so it won't show up again until the schedule resets.
	if (command->getUseTable())
	{
		strategy.removeMission(targetRegion, missionType);
	}

	// we did it, we can go home now.
	return true;

}

/**
 * Handler for clicking on a timer button.
 * @param action pointer to the mouse action.
 */
void GeoscapeState::btnTimerClick(Action *action)
{
	if (action) selectGeoscapeSpeed(dynamic_cast<TextButton *>(action->getSender()));
}

/** Selects one owned speed button through the ordinary radio-group operation. */
bool GeoscapeState::selectGeoscapeSpeed(TextButton *button)
{
	TextButton *speeds[] = {_btn5Secs, _btn1Min, _btn5Mins, _btn30Mins, _btn1Hour, _btn1Day};
	for (TextButton *speed : speeds)
		if (speed == button) return speed->selectRadio();
	return false;
}

/**
 * Updates the scale.
 * @param dX delta of X;
 * @param dY delta of Y;
 */
void GeoscapeState::resize(int &dX, int &dY)
{
	if (_game->getSavedGame()->getSavedBattle())
		return;
	dX = Options::baseXResolution;
	dY = Options::baseYResolution;
	int divisor = 1;
	double pixelRatioY = 1.0;

	if (Options::nonSquarePixelRatio)
	{
		pixelRatioY = 1.2;
	}
	switch (Options::geoscapeScale)
	{
	case SCALE_SCREEN_DIV_3:
		divisor = 3;
		break;
	case SCALE_SCREEN_DIV_2:
		divisor = 2;
		break;
	case SCALE_SCREEN:
		break;
	default:
		dX = 0;
		dY = 0;
		return;
	}

	Options::baseXResolution = std::max(Screen::ORIGINAL_WIDTH, Options::displayWidth / divisor);
	Options::baseYResolution = std::max(Screen::ORIGINAL_HEIGHT, (int)(Options::displayHeight / pixelRatioY / divisor));

	dX = Options::baseXResolution - dX;
	dY = Options::baseYResolution - dY;

	_globe->resize();

	for (std::vector<Surface*>::const_iterator i = _surfaces.begin(); i != _surfaces.end(); ++i)
	{
		if (*i != _globe)
		{
			(*i)->setX((*i)->getX() + dX);
			(*i)->setY((*i)->getY() + dY/2);
		}
	}

	_bg->setX((_globe->getWidth() - _bg->getWidth()) / 2);
	_bg->setY((_globe->getHeight() - _bg->getHeight()) / 2);

	int height = (Options::baseYResolution - Screen::ORIGINAL_HEIGHT) / 2 + 10;
	_sideTop->setHeight(height);
	_sideTop->setY(_sidebar->getY() - height - 1);
	_sideBottom->setHeight(height);
	_sideBottom->setY(_sidebar->getY() + _sidebar->getHeight() + 1);

	_sideLine->setHeight(Options::baseYResolution);
	_sideLine->setY(0);
	_sideLine->drawRect(0, 0, _sideLine->getWidth(), _sideLine->getHeight(), 15);
}
bool GeoscapeState::buttonsDisabled()
{
	return _zoomInEffectTimer->isRunning() || _zoomOutEffectTimer->isRunning();
}

}
