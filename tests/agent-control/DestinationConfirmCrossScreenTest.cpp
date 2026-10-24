// SPDX-License-Identifier: GPL-3.0-or-later
// Actual constructor/provider/callback bodies; explicit Game/Mod/Craft seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Window.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentTargetLifetime.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <new>
#include <map>
#include <sstream>
#include "../../src/Engine/Unicode.h"
#include <iostream>
#include <cstdlib>
using namespace OpenXcom;
using namespace OpenXcom::Observer;
namespace OpenXcom {
#include "GameIdentity.generated.inc"
Game *State::_game = nullptr;
State::State():_screen(true),_modal(nullptr),_ruleInterface(nullptr),_ruleInterfaceParent(nullptr){}
State::~State(){}
void State::init(){} void State::handle(Action*){} void State::blit(){}
void State::resize(int&,int&){} void State::recenter(int,int){}
#include "StateBase.generated.inc"
}
namespace Fixture {
struct Screen { Surface *surface; Surface *getSurface(){return surface;} int getDX(){return 0;}int getDY(){return 0;} };
struct Mod { Surface *background; Surface *getSurface(const std::string&){return background;} std::string getFinalResearch(){return "FINAL";} };
struct Waypoint; struct Base; struct Ufo; struct MissionSite; struct AlienBase;
struct SavedGame {std::vector<Base*> bases;std::vector<Ufo*> ufos;std::vector<MissionSite*> sites;std::vector<AlienBase*> aliens;
 auto getBases()->std::vector<Base*>*{return &bases;} auto getUfos()->std::vector<Ufo*>*{return &ufos;} auto getMissionSites()->std::vector<MissionSite*>*{return &sites;} auto getAlienBases()->std::vector<AlienBase*>*{return &aliens;}
 bool isResearched(const std::string&){return false;}std::vector<Waypoint*> waypoints; int getId(const std::string&){std::abort();} std::vector<Waypoint*> *getWaypoints(){return &waypoints;}};
struct Game {
    SavedGame save; SavedGame *getSavedGame(){return &save;}
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0;
    Screen *getScreen(){return &screen;} Mod *getMod(){return &mod;}
    Language *getLanguage(){return language;}
    bool isState(const void *state) const{return top == state;}
    bool cursorVisible=false; SDL_Rect cursor={0,0,0,0};
    bool agentOverlayClear(const AgentUiState &ui) const{
        if(!clear)return false;
        for(const auto &r:ui.regions)if(cursorVisible && r.x<cursor.x+cursor.w && r.x+r.width>cursor.x && r.y<cursor.y+cursor.h && r.y+r.height>cursor.y)return false;
        return true;
    }
    int pushes=0; void *pushed=nullptr; void pushState(void*s){++pushes;pushed=s;top=s;}
    void popState(){++pops;top=nullptr;}
    bool agentNonCursorOverlayClear(const AgentUiState&) const{return clear;}
};
static Game *currentGame;
struct State : OpenXcom::State {
    Game *_game = currentGame;
    std::function<void()> paintHook;
    using OpenXcom::State::_surfaces;
    using OpenXcom::State::_modal;
    void setInterface(const std::string&,bool=false){}
    void centerAllSurfaces(){}
    LocalizedText tr(const std::string &id) const{return _game->language->getString(id);}
    void add(Surface *surface,const std::string& = "",const std::string& = ""){
        surface->setPalette(_game->palette);
        surface->initText(_game->big,_game->small,_game->language);
        _surfaces.push_back(surface);
    }
    void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
    void blit() override{
        for(auto *surface:_surfaces)surface->blit(_game->screen.surface);
        if(paintHook)paintHook();
    }
};
struct Target {AgentTargetLifetime lifetime; int marker=0; double x=80,y=80;virtual ~Target(){} mutable int nameReads=0;std::string name;std::string getName(Language*)const{++nameReads;return name;}int getMarker(){return marker;}double getLongitude(){return x;}double getLatitude(){return y;}std::weak_ptr<const void> agentLifetime(){return lifetime.token();}};
struct Craft;
struct MissionSite:Target{};struct AlienBase:Target{};
struct Base:Target {std::vector<Craft*> crafts;std::vector<Craft*>*getCrafts(){return &crafts;}std::string getName()const{return name;}};
struct Waypoint:Target {int id=1;int getId(){return id;}void setId(int i){id=i;}};
struct Ufo:Target {enum{FLYING}; bool getHyperDetected(){return false;}int getStatus(){return FLYING;}int getId(){return 1;}int getSpeed(){return 500;}};
struct RuleCraft {bool getSpacecraft(){return false;}int getMaxSpeed(){return 1000;}bool isWaterOnly(){return false;}int getWeapons(){return 0;}int getSoldiers(){return 0;}int getVehicles(){return 0;}};
struct RuleCraftWeapon {std::string getType(){return "STR_NONE_UC";}};
struct CraftWeapon {RuleCraftWeapon rule; RuleCraftWeapon*getRules(){return &rule;}int getAmmo(){return 0;}};
struct Craft : Target {
 std::string name;Target*destination=nullptr;int returns=0,sets=0;Base base;RuleCraft rule;std::vector<CraftWeapon*> weapons;
 std::string getName(Language*)const{++nameReads;return name;}Target*getDestination()const{return destination;}
 void returnToBase(){++returns;}void setDestination(Target*t){++sets;destination=t;}void setStatus(const std::string&s){status=s;}
 bool getLowFuel(){return false;}bool getMissionComplete(){return false;}bool isInDogfight(){return false;}
 Base*getBase(){return &base;}int getSpeed(){return 500;}RuleCraft*getRules(){return &rule;}
 std::string status="STR_OUT";std::string getStatus(){return status;}double getBaseRange(){return 1;}std::string getAltitude(){return "STR_HIGH";}double getLongitude(){return 0;}double getLatitude(){return 0;}
 int getFuelPercentage(){return 50;}int getDamagePercentage(){return 0;}std::vector<CraftWeapon*>*getWeapons(){return &weapons;}
 int getNumSoldiers(){return 0;}int getNumVehicles(){return 0;}
};
struct MarkerSet {Surface*marker;std::map<int,Surface*>frames,resources;std::map<int,Surface*>*getFrames(){resources=frames;if(!resources.count(0))resources[0]=marker;return &resources;}Surface*getFrame(int n){auto i=frames.find(n);return i==frames.end()?marker:i->second;}};
struct Globe : Surface {int speed=60,stops=0,rangeCalls=0,invalidates=0;double lastRange=-1;
 Game*_game=currentGame; Surface *_markers,*_radars,*_countries; MarkerSet storage;MarkerSet*_markerSet=&storage;
 struct Projection{void clearLayer(int){}}_agentProjection;
 struct AgentPaintedTarget{Target*target;std::weak_ptr<const void> lifetime;};
 std::vector<AgentPaintedTarget>_agentMarkerPixels;int _agentMarkerWidth=0,_agentMarkerHeight=0;bool _agentTargetCandidates=false;
 bool _agentBlitReady=false,_isMouseScrolling=false,_isMouseScrolled=false;int _rotLon=0,_rotLat=0;unsigned _agentShownViewRevision=0,_agentViewRevision=1;
 Globe():Surface(320,200){_markers=new Surface(320,200);_radars=new Surface(320,200);_countries=new Surface(320,200);storage.marker=new Surface(5,5);for(auto*s:{_markers,_radars,_countries,storage.marker})s->setPalette(_game->palette);storage.marker->clear(90);_markers->agentTrackPresentation();}
 ~Globe(){delete _markers;delete _radars;delete _countries;delete storage.marker;}
 bool pointBack(double x,double){return x<0;}void polarToCart(double x,double y,Sint16*ox,Sint16*oy){*ox=x;*oy=y;}
 void agentCoverRect(Surface*,Surface*){}
 void drawTarget(Target*,Surface*);void drawMarkers();void blit(Surface*)override;
 std::string agentDestinationTargets(AgentUiState&,const std::vector<Surface*>&,std::vector<Target*>&)const;
 void onMouseClick(ActionHandler){} void rotateStop(){++stops;}void setCraftRange(double,double,double r){++rangeCalls;lastRange=r;}void invalidate(){++invalidates;}
};
#include "DestinationGlobe.generated.inc"
struct GeoscapeState {Globe*getGlobe(){std::abort();return nullptr;}};
struct InterceptState {InterceptState(Globe*,Base*){std::abort();}};
struct GeoscapeCraftState {GeoscapeCraftState(Craft*,Globe*,int){std::abort();}};
struct UfoDetectedState {UfoDetectedState(Ufo*,GeoscapeState*,bool,bool){std::abort();}};
struct TargetInfoState {TargetInfoState(Target*,Globe*){std::abort();}};
struct ConfirmDestinationState:State {
 Craft*_craft;Target*_target;Window*_window;Text*_txtTarget;TextButton*_btnOk,*_btnCancel;
 std::weak_ptr<const void>_agentTargetLifetime;bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 ConfirmDestinationState(Craft*,Target*);~ConfirmDestinationState();
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit()override;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;void btnOkClick(Action*);void btnCancelClick(Action*);
};
struct MultipleTargetsState:State {
 static const int MARGIN=10,SPACING=4,BUTTON_HEIGHT=16;
 std::vector<Target*>_targets;Craft*_craft;GeoscapeState*_state;Window*_window;std::vector<TextButton*>_btnTargets;
 MultipleTargetsState(std::vector<Target*>,Craft*,GeoscapeState*);~MultipleTargetsState();void init()override;void popupTarget(Target*);
 void btnTargetClick(Action*){std::abort();}void btnCancelClick(Action*){std::abort();}
};
struct SelectDestinationState : State {
 Craft *_craft;Globe *_globe;InteractiveSurface *_btnRotateLeft,*_btnRotateRight,*_btnRotateUp,*_btnRotateDown,*_btnZoomIn,*_btnZoomOut;
 TextButton *_btnCancel,*_btnCydonia;Window *_window;Text *_txtTitle;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 SelectDestinationState(Craft*,Globe*);~SelectDestinationState();void init()override;void btnCancelClick(Action*);
 void globeClick(Action*){std::abort();}void btnRotateLeftPress(Action*){std::abort();}void btnRotateLeftRelease(Action*){std::abort();}void btnRotateRightPress(Action*){std::abort();}void btnRotateRightRelease(Action*){std::abort();}void btnRotateUpPress(Action*){std::abort();}void btnRotateUpRelease(Action*){std::abort();}void btnRotateDownPress(Action*){std::abort();}void btnRotateDownRelease(Action*){std::abort();}void btnZoomInLeftClick(Action*){std::abort();}void btnZoomInRightClick(Action*){std::abort();}void btnZoomOutLeftClick(Action*){std::abort();}void btnZoomOutRightClick(Action*){std::abort();}void btnCydoniaClick(Action*){std::abort();}
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "SelectDestination.generated.inc"
#include "ConfirmDestination.generated.inc"
#include "MultipleTargets.generated.inc"
}
static unsigned checks=0;static bool lastAvailable=false,lastConfirm=false,lastCancel=false;static std::string lastKind="not_read";
template<class T>static bool observe(T*state,AgentUiState&ui){const bool ok=state->agentUiState(ui);lastAvailable=ok;lastKind=ok?ui.screen:"unavailable";lastConfirm=ok&&ui.offers("confirm_interception");lastCancel=ok&&ui.offers("cancel_interception_confirmation");return ok;}
static void check(bool ok,const char*why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<'\n'<<"{\"check_ordinal\":"<<checks<<",\"last_read_kind\":\""<<lastKind<<"\",\"last_read_available\":"<<lastAvailable<<",\"last_confirm_action\":"<<lastConfirm<<",\"last_cancel_action\":"<<lastCancel<<",\"paint_healthy\":"<<agentPaintHealthy()<<",\"native_pop_count\":"<<(Fixture::currentGame?Fixture::currentGame->pops:0)<<",\"native_push_count\":"<<(Fixture::currentGame?Fixture::currentGame->pushes:0)<<"}\n";std::exit(1);}}
static AgentRequest action(const std::string&id,const std::string&row=""){AgentRequest r;r.action=id;r.choice=row;return r;}
int main(int argc,char**argv){
 check(argc==2,"source argument");check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");FileMap::load("common","./",false);
 Options::baseXResolution=320;Options::baseYResolution=200;Font big,small;
 for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"}){
 Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");
 Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
 Fixture::Craft craft;Fixture::Target retained;craft.destination=&retained;Fixture::MissionSite visible,hidden;visible.name="VISIBLE TARGET";hidden.name="SECRET TARGET";hidden.marker=-1;game.save.sites={&visible};Fixture::Globe globe;
 auto*picker=new Fixture::SelectDestinationState(&craft,&globe);game.top=picker;picker->init();globe.drawMarkers();globe.blit(&canvas);picker->blit();AgentUiState choice;
 check(observe(picker,choice)&&choice.offers("choose_visible_craft_destination"),"actual painted choice offered");
 check(picker->agentDispatch(action("choose_visible_craft_destination","r0"))&&craft.sets==0&&craft.destination==&retained,"selection does not auto-confirm");
 auto*multiple=static_cast<Fixture::MultipleTargetsState*>(game.pushed);check(multiple->_targets.size()==1&&multiple->_targets[0]==&visible,"literal singleton constructor exact target");multiple->init();
 auto*confirm=static_cast<Fixture::ConfirmDestinationState*>(game.pushed);check(confirm->_target==&visible&&game.pops==1&&craft.sets==0,"literal init/popup opens actual confirmation without mutation");multiple->release();delete multiple;
 AgentUiState first;check(!observe(confirm,first),"confirmation requires own paint");confirm->blit();check(observe(confirm,first)&&first.offers("confirm_interception")&&first.offers("cancel_interception_confirmation"),"current literal confirmation authority");
 const std::string firstPayload=first.payload(),nativeCaption=confirm->_txtTarget->getText();check(nativeCaption.find(visible.name)!=std::string::npos&&firstPayload.find(hidden.name)==std::string::npos,"only current UI caption exposed");
 check(confirm->agentDispatch(action("cancel_interception_confirmation"))&&craft.sets==0&&craft.destination==&retained&&game.pops==2,"native Cancel only pop preserves destination");confirm->release();delete confirm;
 // Same surviving marker with extra hidden population reaches the same UI.
 game.save.sites={&hidden,&visible};game.top=picker;globe.drawMarkers();globe.blit(&canvas);picker->blit();check(picker->agentDispatch(action("choose_visible_craft_destination","r0")),"hidden population does not prevent semantic selection");multiple=static_cast<Fixture::MultipleTargetsState*>(game.pushed);multiple->init();confirm=static_cast<Fixture::ConfirmDestinationState*>(game.pushed);multiple->release();delete multiple;confirm->blit();AgentUiState second;
 check(observe(confirm,second)&&second.payload()==firstPayload,"same visible selected confirmation after hidden population change");
 const int reads=visible.nameReads;visible.name="MODEL_ONLY_SECRET";
 for(const std::string caption:{std::string(500,'W'),std::string("\xF0\x9F\x99\x82")}){confirm->_txtTarget->setText(caption);check(!observe(confirm,second),"edited caption stale before paint");confirm->blit();SDL_FillRect(confirm->_txtTarget->getSurface(),nullptr,0);check(observe(confirm,second)&&YAML::Load(second.visible)["target"].as<std::string>()==caption,"logical caption independent of glyph ink and fit");}
 const std::string okLabel=confirm->_btnOk->getText();confirm->_btnOk->setText(std::string(300,'W'));check(!observe(confirm,second),"logical action caption stale before paint");confirm->blit();check(observe(confirm,second)&&second.offers("confirm_interception"),"action label has no glyph completeness gate");confirm->_btnOk->setText(okLabel);confirm->blit();
 confirm->_txtTarget->setText(nativeCaption);confirm->blit();game.cursorVisible=true;game.cursor={20,75,4,4};check(observe(confirm,second)&&YAML::Load(second.visible)["target"].as<std::string>()==nativeCaption,"cursor over caption not a readability gate");game.cursor={72,106,4,4};check(observe(confirm,second)&&!second.offers("confirm_interception")&&!confirm->agentDispatch(action("confirm_interception")),"cursor over real action hitbox prevents confirm");game.cursorVisible=false;
 confirm->_txtTarget->setVisible(false);confirm->blit();check(observe(confirm,second)&&YAML::Load(second.visible)["target"].IsNull()&&visible.nameReads==reads,"hidden logical widget has no target-name fallback");confirm->_txtTarget->setVisible(true);confirm->blit();
 confirm->_modal=confirm->_btnOk;check(!observe(confirm,second)&&!confirm->agentDispatch(action("confirm_interception")),"modal refuses authority");confirm->_modal=nullptr;
 Surface otherCanvas(320,200);otherCanvas.setPalette(palette);game.screen.surface=&otherCanvas;check(!observe(confirm,second),"equal geometry different canvas requires own paint");confirm->blit();game.screen.surface=&canvas;check(!observe(confirm,second),"restored old canvas requires current paint");confirm->blit();
 check(confirm->agentDispatch(action("confirm_interception"))&&craft.sets==1&&craft.destination==&visible&&craft.status=="STR_OUT"&&game.pops==5,"native confirm destination/status and exactly two pops");confirm->release();delete confirm;picker->release();delete picker;game.top=nullptr;
 // Same-address replacement cannot revive the old popup's target authority.
 alignas(Fixture::MissionSite) unsigned char storage[sizeof(Fixture::MissionSite)];auto*old=new(storage)Fixture::MissionSite;old->name="STALE";auto*stale=new Fixture::ConfirmDestinationState(&craft,old);game.top=stale;stale->blit();check(observe(stale,second),"stale test starts painted/live");old->~MissionSite();auto*replacement=new(storage)Fixture::MissionSite;replacement->name="REPLACEMENT";
 check(!observe(stale,second)&&!stale->agentDispatch(action("confirm_interception"))&&!stale->agentDispatch(action("cancel_interception_confirmation"))&&craft.sets==1,"expired weak owner rejects both handlers before stale dynamic_cast");stale->release();delete stale;replacement->~MissionSite();game.top=nullptr;game.save.sites.clear();Fixture::currentGame=nullptr;
 }
 check(!SDL_GetVideoSurface(),"no video or game loop");std::cout<<"PASS "<<checks<<" actual singleton-to-confirm checks, two masters; explicit Game/Mod/domain seams\n";
}
