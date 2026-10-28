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
#include "../../src/Observer/AgentGlobeProjection.h"
#include <cmath>
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
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
    void setInterface(const std::string&){}
    void centerAllSurfaces(){}
    LocalizedText tr(const std::string &id) const{return _game->language->getString(id);}
    void add(Surface *surface,const std::string& = "",const std::string& = ""){
        surface->setPalette(_game->palette);
        surface->initText(_game->big,_game->small,_game->language);
        _surfaces.push_back(surface);
    }
    void blit() override{
        for(auto *surface:_surfaces)surface->blit(_game->screen.surface);
        if(paintHook)paintHook();
    }
};
struct Target {AgentTargetLifetime lifetime; int marker=0; double x=80,y=80;virtual ~Target(){} std::string name;std::string getName(Language*)const{return name;}int getMarker(){return marker;}double getLongitude(){return x;}double getLatitude(){return y;}void setLongitude(double v){x=v;}void setLatitude(double v){y=v;}std::weak_ptr<const void> agentLifetime(){return lifetime.token();}};
struct Craft;
struct MissionSite:Target{};struct AlienBase:Target{};
struct Base:Target {std::vector<Craft*> crafts;std::vector<Craft*>*getCrafts(){return &crafts;}std::string getName()const{return name;}};
struct Waypoint:Target {int id=0;int getId(){return id;}void setId(int i){id=i;}};
struct Ufo:Target {enum{FLYING}; int getStatus(){return FLYING;}int getId(){return 1;}int getSpeed(){return 500;}};
struct RuleCraft {bool getSpacecraft(){return false;}int getMaxSpeed(){return 1000;}bool isWaterOnly(){return false;}int getWeapons(){return 0;}int getSoldiers(){return 0;}int getVehicles(){return 0;}};
struct RuleCraftWeapon {std::string getType(){return "STR_NONE_UC";}};
struct CraftWeapon {RuleCraftWeapon rule; RuleCraftWeapon*getRules(){return &rule;}int getAmmo(){return 0;}};
struct Craft : Target {
 std::string name;Target*destination=nullptr;int returns=0,sets=0;Base base;RuleCraft rule;std::vector<CraftWeapon*> weapons;
 std::string getName(Language*)const{return name;}Target*getDestination()const{return destination;}
 void returnToBase(){++returns;}void setDestination(Target*t){++sets;destination=t;}
 bool getLowFuel(){return false;}bool getMissionComplete(){return false;}bool isInDogfight(){return false;}
 Base*getBase(){return &base;}int getSpeed(){return 500;}RuleCraft*getRules(){return &rule;}
 std::string status="STR_OUT";std::string getStatus(){return status;}double getBaseRange(){return 1;}std::string getAltitude(){return "STR_HIGH";}double getLongitude(){return 0;}double getLatitude(){return 0;}
 int getFuelPercentage(){return 50;}int getDamagePercentage(){return 0;}std::vector<CraftWeapon*>*getWeapons(){return &weapons;}
 int getNumSoldiers(){return 0;}int getNumVehicles(){return 0;}
};
struct MultipleTargetsState {std::vector<Target*> targets;Craft*craft;MultipleTargetsState(std::vector<Target*> t,Craft*c,void*):targets(t),craft(c){}};
struct MarkerSet {Surface*marker;std::map<int,Surface*>frames,resources;std::map<int,Surface*>*getFrames(){resources=frames;if(!resources.count(0))resources[0]=marker;return &resources;}Surface*getFrame(int n){auto i=frames.find(n);return i==frames.end()?marker:i->second;}};
struct Globe : InteractiveSurface {int speed=60,stops=0,rangeCalls=0,invalidates=0;double lastRange=-1;
 Game*_game=currentGame; Surface *_markers,*_radars,*_countries; MarkerSet storage;MarkerSet*_markerSet=&storage;
 AgentGlobeProjection _agentProjection;
 struct AgentPaintedTarget{Target*target;std::weak_ptr<const void> lifetime;bool destinationEligible=false;};
 std::vector<AgentPaintedTarget>_agentMarkerPixels;int _agentMarkerWidth=0,_agentMarkerHeight=0;bool _agentTargetCandidates=false;
 bool _agentBlitReady=false,_isMouseScrolling=false,_isMouseScrolled=false;int _rotLon=0,_rotLat=0;unsigned _agentShownViewRevision=0,_agentViewRevision=1;
 Globe():InteractiveSurface(320,200){_markers=new Surface(320,200);_radars=new Surface(320,200);_countries=new Surface(320,200);storage.marker=new Surface(5,5);for(auto*s:{_markers,_radars,_countries,storage.marker})s->setPalette(_game->palette);storage.marker->clear(90);_markers->agentTrackPresentation();}
 ~Globe(){delete _markers;delete _radars;delete _countries;delete storage.marker;}
 bool pointBack(double x,double){return x<0;}void polarToCart(double x,double y,Sint16*ox,Sint16*oy){*ox=x;*oy=y;}
 unsigned agentViewRevision()const{return _agentViewRevision;}
 bool validPoint=true;int targetQueries=0;std::vector<Target*> clickTargets;
 void cartToPolar(int x,int y,double*lon,double*lat){*lon=x/100.0;*lat=y/100.0;}
 bool pointOnVisibleGlobe(int,int)const{return validPoint;}
 std::vector<Target*> getTargets(int,int,bool craft){if(!craft)std::abort();++targetQueries;return clickTargets;}
 std::string agentGeography(AgentUiState&,const std::vector<Surface*>&,int=0)const;
 void agentCoverRect(Surface*,Surface*){}
 void drawTarget(Target*,Surface*);void drawMarkers();void blit(Surface*)override;
 std::string agentDestinationTargets(AgentUiState&,const std::vector<Surface*>&,std::vector<Target*>&)const;
 std::string agentGeoscapeTargets(AgentUiState&,const std::vector<Surface*>&,std::vector<Target*>&,std::vector<std::pair<int,int>>&,bool=false)const;
 void onMouseClick(ActionHandler){} void rotateStop(){++stops;}void setCraftRange(double,double,double r){++rangeCalls;lastRange=r;}void invalidate(){++invalidates;}
};
#include "DestinationGlobe.generated.inc"
struct SelectDestinationState : State {
 Craft *_craft;Globe *_globe;InteractiveSurface *_btnRotateLeft,*_btnRotateRight,*_btnRotateUp,*_btnRotateDown,*_btnZoomIn,*_btnZoomOut;
 TextButton *_btnCancel,*_btnCydonia;Window *_window;Text *_txtTitle;
 std::weak_ptr<const void> _agentCraftLifetime;const void*_agentSave=nullptr;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 SelectDestinationState(Craft*,Globe*);~SelectDestinationState();void init()override;void btnCancelClick(Action*);
 void globeClick(Action*);void btnRotateLeftPress(Action*){std::abort();}void btnRotateLeftRelease(Action*){std::abort();}void btnRotateRightPress(Action*){std::abort();}void btnRotateRightRelease(Action*){std::abort();}void btnRotateUpPress(Action*){std::abort();}void btnRotateUpRelease(Action*){std::abort();}void btnRotateDownPress(Action*){std::abort();}void btnRotateDownRelease(Action*){std::abort();}void btnZoomInLeftClick(Action*){std::abort();}void btnZoomInRightClick(Action*){std::abort();}void btnZoomOutLeftClick(Action*){std::abort();}void btnZoomOutRightClick(Action*){std::abort();}void btnCydoniaClick(Action*){std::abort();}
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "SelectDestination.generated.inc"
}
static unsigned checks = 0;
static void check(bool ok,const char *message){
    ++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}
}
int main(int argc,char **argv){
    check(argc==2,"source argument");
    check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");
    FileMap::load("common","./",false);
    Options::baseXResolution=320;Options::baseYResolution=200;
    Font big,small,overlap;
    for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){
        if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);
        if(f["id"].as<std::string>()=="FONT_SMALL"){
            small.load(f);auto altered=YAML::Clone(f);altered["spacing"]=-2;
            for(auto image:altered["images"])image["spacing"]=-2;
            overlap.load(altered);
        }
    }
    Surface canvas(320,200),background(320,200);SDL_Color palette[256];
    for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
    canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
    AgentRequest request;
    const std::string token(32,'1'),prefix="E "+token+" 1 "+token+" "+token+" 1 ";
    for(const std::string action:{"cancel_craft_destination"}){
        check(parseAgentRequest(prefix+action,request),"empty argument protocol accepted");
        for(const std::string extra:{" extra"," r0"," true"," 0","\n"})
            check(!parseAgentRequest(prefix+action+extra,request),"nonempty argument rejected");
    }
    for(const std::string master:{"xcom1","xcom2"}){
 Language language;language.loadFile(std::string(argv[1])+"/standard/"+master+"/Language/en-US.yml");
 Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
 Fixture::Craft craft;craft.name="CRAFT";Fixture::Target retained;craft.destination=&retained;Fixture::Globe globe;
 auto *state=new Fixture::SelectDestinationState(&craft,&globe);game.top=state;AgentUiState ui;
 check(globe.rangeCalls==0,"out craft constructor no range preview");state->init();check(globe.stops==1&&craft.destination==&retained,"init stops globe presentation only");
 check(!state->agentUiState(ui),"prepaint refused");state->blit();check(state->agentUiState(ui)&&ui.screen=="craft_destination_selection","literal picker constructor admitted");
 auto visible=YAML::Load(ui.visible);const std::string title=language.getString("STR_SELECT_DESTINATION");
 check(visible.size()==4&&visible["title"].as<std::string>()==title&&visible["buttons"].size()==1&&visible["buttons"][0]["id"].as<std::string>()=="cancel","exact DTO");check(ui.actions.size()==1&&ui.offers("cancel_craft_destination")&&visible["targets"].size()==0,"no unpainted targets");
 game.cursorVisible=true;game.cursor={11,7,4,4};check(state->agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==title,"cursor caption does not gate");game.cursor={111,9,4,4};request.action="cancel_craft_destination";check(!state->agentDispatch(request)&&game.pops==0,"cursor over control no dispatch");game.cursorVisible=false;
 for(const std::string caption:{std::string(500,'W'),std::string("LINE1\nLINE2\nLINE3"),std::string("\xF0\x9F\x99\x82")}){state->_txtTitle->setText(caption);check(!state->agentUiState(ui),"text edit stale before paint");state->blit();SDL_FillRect(state->_txtTitle->getSurface(),nullptr,0);check(state->agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==caption,"active logical caption exact independent glyph ink or clipping");}
 state->_txtTitle->setText(title);state->blit();state->_txtTitle->setVisible(false);state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull(),"hidden caption null");state->_txtTitle->setVisible(true);state->blit();
 for(Surface*w:std::vector<Surface*>{state->_txtTitle,state->_btnCancel,state->_window}){int x=w->getX();w->setX(x+1);w->setX(x);check(!state->agentUiState(ui),"geometry ABA");state->blit();w->setVisible(false);w->setVisible(true);check(!state->agentUiState(ui),"visibility ABA");state->blit();}
 state->_modal=state->_btnCancel;check(!state->agentUiState(ui)&&!state->agentDispatch(request),"modal refused");state->_modal=nullptr;game.top=nullptr;check(!state->agentUiState(ui),"wrong top");game.top=state;game.clear=false;check(!state->agentUiState(ui),"overlay refused");game.clear=true;
 SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state->blit();check(!state->agentUiState(ui),"clipped destination");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state->agentUiState(ui),"clip restoration needs repaint");state->blit();
 Surface cover(4,4,11,7);cover.setPalette(palette);cover.clear(80);state->_surfaces.push_back(&cover);state->blit();check(state->agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull(),"later real occlusion hides caption");state->_surfaces.pop_back();state->blit();

 // Actual drawMarkers/drawTarget/blit/projection, with explicit polar/world-list seams.
 Fixture::MissionSite visibleSite, hiddenSite; visibleSite.name="VISIBLE";hiddenSite.name="SECRET";hiddenSite.marker=-1;
 game.save.sites={&visibleSite};globe.drawMarkers();globe.blit(&canvas);state->blit();
 AgentUiState one;check(state->agentUiState(one)&&one.offers("choose_visible_craft_destination"),"painted target offered");
 auto targets=YAML::Load(one.visible)["targets"];check(targets.size()==1&&targets[0]["row"].as<std::string>()=="r0"&&targets[0].size()==5,"geometry only target");
 game.save.sites.push_back(&hiddenSite);globe.drawMarkers();globe.blit(&canvas);state->blit();AgentUiState two;check(state->agentUiState(two)&&one.payload()==two.payload()&&one.sameBinding(two),"hidden population observational equivalence including binding");
 game.save.sites={&hiddenSite,&visibleSite};globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&one.payload()==two.payload()&&one.sameBinding(two),"hidden insertion before visible preserves complete DTO and private binding");
 request.action="choose_visible_craft_destination";request.choice="r1";check(!state->agentDispatch(request),"unknown row refused");request.choice="r0";
 check(state->agentDispatch(request)&&game.pushes==1&&craft.sets==0,"semantic selection only pushes native next-state boundary");auto*selection=static_cast<Fixture::MultipleTargetsState*>(game.pushed);check(selection->targets.size()==1&&selection->targets[0]==&visibleSite&&selection->craft==&craft,"singleton exact painted target no hidden target");delete selection;game.top=state;
 // Later visible craft marker occludes eligible site; hidden craft cannot affect it.
 Fixture::Craft blocker;Fixture::Base holder;holder.marker=-1;holder.crafts={&blocker};game.save.bases={&holder};globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"all painted craft classes occlude");{AgentUiState own;std::vector<Fixture::Target*> admitted;std::vector<std::pair<int,int>>points;auto json=globe.agentGeoscapeTargets(own,{},admitted,points);check(admitted.size()==1&&admitted[0]==&blocker&&points.size()==1,"own marker admitted through general globe final witness");check(YAML::Load(json).size()==1&&json.find("SECRET")==std::string::npos,"own target remains geometry only");Surface ownCover(5,5,78,78);ownCover.setVisible(true);AgentUiState covered;globe.agentGeoscapeTargets(covered,{&ownCover},admitted,points);check(admitted.empty(),"own final marker obscured by real overlay");}blocker.marker=-1;globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&two.offers("choose_visible_craft_destination"),"hidden craft has no admission effect");
 globe.storage.marker->clear();globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"transparent marker not exposed");globe.storage.marker->clear(90);
 visibleSite.x=-80;globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"back hemisphere absent");visibleSite.x=80;

 visibleSite.x=330;globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"fully offscreen marker absent");visibleSite.x=80;
 globe.drawMarkers();globe.blit(&canvas);state->blit();++globe._agentViewRevision;check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"view revision stale");globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&!one.sameBinding(two),"repaint same geometry after view change invalidates old binding");
 globe.drawMarkers();state->blit();check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"draw without final markers blit has no authority");globe.blit(&canvas);state->blit();
 game.cursorVisible=true;game.cursor={78,78,5,5};check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"cursor over marker excludes action");game.cursorVisible=false;
 globe._rotLon=1;check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"rotating view refused");globe._rotLon=0;
 Surface markerCover(5,5,78,78);markerCover.setPalette(palette);markerCover.clear(80);state->_surfaces.push_back(&markerCover);state->blit();check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"later visible state surface occludes marker");state->_surfaces.pop_back();state->blit();
 Surface replacementCanvas(320,200);replacementCanvas.setPalette(palette);game.screen.surface=&replacementCanvas;state->blit();check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"equal geometry screen replacement cannot reuse marker paint");game.screen.surface=&canvas;globe.blit(&canvas);state->blit();
 // More than 32 separately painted markers has deterministic visible-only truncation.
 std::vector<Fixture::MissionSite> many(33);game.save.sites.clear();for(unsigned i=0;i<many.size();++i){many[i].x=12+(i%15)*18;many[i].y=50+(i/15)*18;game.save.sites.push_back(&many[i]);}globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&YAML::Load(two.visible)["targets"].size()==32,"bounded visible targets");
 std::vector<Fixture::MissionSite> overflowing(257);game.save.sites.clear();for(auto&t:overflowing)game.save.sites.push_back(&t);globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&YAML::Load(two.visible)["targets"].size()==1,"fully overpainted attempts leave only final surviving owner");

 // Exact visible geography points route through the actual ordinary globeClick.
 globe._agentProjection.samples={{120,120,false,false,true,0},{136,24,false,false,true,0}};
 globe.drawMarkers();globe.blit(&canvas);state->blit();AgentUiState pointUi;
 check(state->agentUiState(pointUi)&&pointUi.offers("choose_visible_craft_point")&&pointUi.points.size()==1&&YAML::Load(pointUi.visible)["globe"]["samples"].size()==1,"minimum y28 only admits visible actionable geography");
 AgentRequest pointRequest;pointRequest.action="choose_visible_craft_point";pointRequest.x=121;pointRequest.y=120;
 check(!state->agentDispatch(pointRequest)&&globe.targetQueries==0,"unadvertised neighbor rejected before target query");pointRequest.x=136;pointRequest.y=24;
 check(!state->agentDispatch(pointRequest)&&globe.targetQueries==0,"header point rejected before target query");pointRequest.x=120;pointRequest.y=120;
 globe.validPoint=false;check(!state->agentDispatch(pointRequest)&&globe.targetQueries==0,"nonfinite projection refused before handler");globe.validPoint=true;
 globe._rotLon=1;check(state->agentUiState(two)&&!two.offers("choose_visible_craft_point")&&!state->agentDispatch(pointRequest),"rotating geometry unavailable");globe._rotLon=0;
 globe._agentProjection.samples[0].sphere=false;check(state->agentUiState(two)&&!two.offers("choose_visible_craft_point"),"back or outside sphere not advertised");globe._agentProjection.samples[0].sphere=true;
 globe._agentProjection.samples[0].uncertain=true;check(state->agentUiState(two)&&!two.offers("choose_visible_craft_point"),"uncertain raster sample not advertised");globe._agentProjection.samples[0].uncertain=false;
 Surface pointCover(3,3,119,119);pointCover.setPalette(palette);pointCover.clear(30);state->_surfaces.push_back(&pointCover);state->blit();check(state->agentUiState(two)&&!two.offers("choose_visible_craft_point")&&!state->agentDispatch(pointRequest),"real overlay prevents point action");state->_surfaces.pop_back();state->blit();
 AgentFrameCommand pointFrame;check(state->agentUiState(pointUi)&&pointFrame.stage(pointRequest,pointUi,state,1,1,100,100),"fresh point grant staged");++globe._agentViewRevision;globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&!pointFrame.matchesFrame(101,1,1,state,two),"same geometry new view revision invalidates point grant");
 const void*oldSave=state->_agentSave;state->_agentSave=&pointCover;check(!state->agentUiState(two)&&!state->agentDispatch(pointRequest),"changed saved game rejected before callback");state->_agentSave=oldSave;
 const int beforePush=game.pushes;check(state->agentDispatch(pointRequest)&&game.pushes==beforePush+1&&globe.targetQueries==1&&craft.sets==0,"native point callback only opens provisional selection");
 auto*pointSelection=static_cast<Fixture::MultipleTargetsState*>(game.pushed);auto*waypoint=dynamic_cast<Fixture::Waypoint*>(pointSelection->targets.at(0));check(waypoint&&waypoint->id==0&&waypoint->x==1.2&&waypoint->y==1.2&&game.save.waypoints.empty(),"native provisional waypoint preserves projected coordinates without saving");delete waypoint;delete pointSelection;game.top=state;
 globe.clickTargets={&visibleSite};check(state->agentDispatch(pointRequest)&&globe.targetQueries==2,"native point handler can resolve ordinary nearby target");pointSelection=static_cast<Fixture::MultipleTargetsState*>(game.pushed);check(pointSelection->targets.size()==1&&pointSelection->targets[0]==&visibleSite,"point handler preserves native target resolution");delete pointSelection;game.top=state;globe.clickTargets.clear();globe._agentProjection.samples.clear();

 // Actual palette conversion maps source non-key ink90 to layer transparent key0.
 game.save.sites={&visibleSite};SDL_Color converted[256];for(int i=0;i<256;++i)converted[i]=palette[i];converted[90]=palette[0];globe.storage.marker->setPalette(converted);globe.drawMarkers();globe.blit(&canvas);state->blit();check(globe._markers->getPixel(80,80)==0&&state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"actual palette-to-final-key conversion has no visible witness");globe.storage.marker->setPalette(palette);

 // >256 invisible attempted paints cannot alter a visible target's authority.
 game.save.sites={&visibleSite};globe.drawMarkers();globe.blit(&canvas);state->blit();AgentUiState clean;check(state->agentUiState(clean),"clean raster baseline");
 for(auto&t:overflowing){t.x=500;t.y=80;}game.save.sites.clear();for(auto&t:overflowing)game.save.sites.push_back(&t);game.save.sites.push_back(&visibleSite);globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&clean.payload()==two.payload()&&clean.sameBinding(two),"offscreen attempted population has no DTO or grant effect");
 for(auto&t:overflowing){t.x=80;t.y=80;}globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&clean.payload()==two.payload()&&clean.sameBinding(two),"fully erased attempted population has no DTO or grant effect");
 // Sparse-hole counterexample: transient B bbox contains A, but its ink does not.
 Surface centerMarker(5,5),sparseMarker(5,5),smallMarker(1,1);for(auto*m:{&centerMarker,&sparseMarker,&smallMarker})m->setPalette(palette);
 centerMarker.setPixel(2,2,90);sparseMarker.setPixel(0,2,91);sparseMarker.setPixel(4,2,91);smallMarker.setPixel(0,0,92);
 globe.storage.frames={{0,&centerMarker},{1,&sparseMarker},{2,&smallMarker}};
 Fixture::MissionSite transient,leftEnd,rightEnd;transient.marker=1;leftEnd.marker=rightEnd.marker=2;leftEnd.x=78;rightEnd.x=82;
 game.save.sites={&visibleSite,&leftEnd,&rightEnd};globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(clean)&&YAML::Load(clean.visible)["targets"].size()==3,"sparse baseline three surviving witnesses");
 std::vector<Uint8> image;for(int y=0;y<200;++y)for(int x=0;x<320;++x)image.push_back(globe._markers->getPixel(x,y));
 game.save.sites={&visibleSite,&transient,&leftEnd,&rightEnd};globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&clean.payload()==two.payload()&&clean.sameBinding(two),"fully erased sparse bbox never retires earlier visible marker");
 bool sameImage=true;for(int y=0;y<200;++y)for(int x=0;x<320;++x)if(image[y*320+x]!=globe._markers->getPixel(x,y))sameImage=false;check(sameImage,"sparse counterexample actual final pixels equal");
 // Unsupported resource format disables the subset irrespective of world population.
 Surface truecolorMarker(5,5,0,0,32);globe.storage.frames[99]=&truecolorMarker;globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"unused truecolor resource fails static format eligibility, not hidden-target admission");globe.storage.frames.erase(99);
 // Cropped noncandidate native draw overwrites the surviving A pixel.
 game.save.sites={&visibleSite};blocker.marker=1;blocker.x=82;blocker.y=82;*sparseMarker.getCrop()={0,2,1,1};game.save.bases={&holder};globe.drawMarkers();globe.blit(&canvas);state->blit();check(globe._markers->getPixel(80,80)==91&&state->agentUiState(two)&&!two.offers("choose_visible_craft_destination"),"actual cropped craft write clears target ownership");
 *sparseMarker.getCrop()={0,0,0,0};game.save.bases.clear();globe.storage.frames.clear();
 game.save.sites={&visibleSite,&hiddenSite};
 globe.drawMarkers();globe.blit(&canvas);state->blit();check(state->agentUiState(one),"fresh marker baseline");visibleSite.lifetime.invalidate();check(state->agentUiState(two)&&!two.offers("choose_visible_craft_destination")&&!one.sameBinding(two),"destroyed target proof expires without dereference");
 game.save.sites.clear();game.save.bases.clear();globe.drawMarkers();globe.blit(&canvas);state->blit();request.action="cancel_craft_destination";request.choice.clear();
 check(state->agentUiState(ui),"authority baseline");AgentFrameCommand frame;check(frame.stage(request,ui,state,1,1,100,100),"stage cancel");Fixture::Craft other;state->_craft=&other;AgentUiState changed;check(state->agentUiState(changed)&&!frame.matchesFrame(101,1,1,state,changed),"craft context change stale");state->_craft=&craft;Fixture::Globe otherGlobe;state->_globe=&otherGlobe;check(state->agentUiState(changed)&&!frame.matchesFrame(101,1,1,state,changed),"globe actor change stale");state->_globe=&globe;
 for(const std::string action:{"select_destination","confirm_craft_destination","go_to_cydonia","open_craft_destination","cancel_craft_navigation"}){request.action=action;check(!state->agentDispatch(request)&&game.pops==0&&craft.sets==0,"unsupported selection or neighbour cannot dispatch");}
 request.action="cancel_craft_destination";check(state->agentDispatch(request)&&game.pops==1&&craft.destination==&retained&&craft.sets==0&&craft.returns==0&&globe.speed==60,"cancel pop only leaves craft target and speed");check(!state->agentDispatch(request)&&game.pops==1,"cannot replay after pop");state->release();delete state;check(globe.rangeCalls==1&&globe.lastRange==0&&craft.destination==&retained,"destructor clears range presentation only");
 craft.status="STR_READY";auto *preview=new Fixture::SelectDestinationState(&craft,&globe);check(globe.rangeCalls==2&&globe.lastRange==1&&globe.invalidates==1&&craft.destination==&retained,"ready craft range preview no target mutation");preview->release();delete preview;check(globe.rangeCalls==3&&globe.lastRange==0,"preview destructor range reset");
 {Fixture::Craft expiring;auto*expired=new Fixture::SelectDestinationState(&expiring,&globe);game.top=expired;expired->blit();AgentUiState before;check(expired->agentUiState(before),"live craft begins valid");expiring.lifetime.invalidate();check(!expired->agentUiState(ui)&&!expired->agentDispatch(pointRequest),"expired craft weak identity refuses observation and action");expired->release();delete expired;game.top=nullptr;}
 // Intentional actual paint fault runs LAST: paint-health is sticky and must
 // never be reset by the fixture. This is safe-refusal proof, not evidence
 // that this SDK completed a healthy zero-alpha final-layer blit.
 if(master=="xcom2"){
 Fixture::MissionSite alphaTarget;game.save.sites={&alphaTarget};craft.status="STR_OUT";
 auto*alphaState=new Fixture::SelectDestinationState(&craft,&globe);game.top=alphaState;
 globe.drawMarkers();globe.blit(&canvas);alphaState->blit();AgentUiState alphaBaseline;
 check(agentPaintHealthy()&&alphaState->agentUiState(alphaBaseline)&&alphaBaseline.offers("choose_visible_craft_destination"),"healthy authority before intentional alpha paint");
 globe.drawMarkers();const int alphaResult=SDL_SetAlpha(globe._markers->getSurface(),SDL_SRCALPHA,0);const Uint32 beforeFlags=globe._markers->getSurface()->flags;const int beforeAlpha=globe._markers->getSurface()->format->alpha;globe.blit(&canvas);const Uint32 afterFlags=globe._markers->getSurface()->flags;const int afterAlpha=globe._markers->getSurface()->format->alpha;alphaState->blit();const bool alphaAvailable=alphaState->agentUiState(two);const bool alphaOffered=alphaAvailable&&two.offers("choose_visible_craft_destination");
 std::cerr<<"{\"diagnostic\":\"final_layer_alpha\",\"master\":\""<<master<<"\",\"set_alpha_rc\":"<<alphaResult<<",\"srcalpha_flag\":"<<SDL_SRCALPHA<<",\"before_flags\":"<<beforeFlags<<",\"before_alpha\":"<<beforeAlpha<<",\"after_flags\":"<<afterFlags<<",\"after_alpha\":"<<afterAlpha<<",\"state_available\":"<<alphaAvailable<<",\"target_action\":"<<alphaOffered<<",\"paint_healthy\":"<<agentPaintHealthy()<<",\"expected_available_if_healthy\":true,\"expected_unavailable_if_unhealthy\":true,\"expected_action\":false}\n";
 check(alphaResult==0&&(beforeFlags&SDL_SRCALPHA)&&beforeAlpha==0,"native alpha-zero precondition reached");check(!alphaOffered&&alphaAvailable==agentPaintHealthy(),"alpha paint yields no target authority; paint fault makes whole state unavailable");
 alphaState->release();delete alphaState;game.top=nullptr;game.save.sites.clear();
 }
 }
 check(!SDL_GetVideoSurface(),"no game video");std::cout<<"PASS "<<checks<<" picker constructor/init/provider/cancel/destructor checks; explicit seams\n";
}
