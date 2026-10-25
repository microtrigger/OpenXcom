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
#include "../../src/Observer/AgentFrameCommand.h"
#include "../../src/Observer/AgentTargetLifetime.h"
#include <yaml-cpp/yaml.h>
#include <functional>
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
struct Screen { Surface *surface; Surface *getSurface(){return surface;} };
struct Mod { Surface *background; Surface *getSurface(const std::string&){return background;} };
struct Waypoint;
struct SavedGame {std::vector<Waypoint*> waypoints; int idCalls=0; int getId(const std::string&){++idCalls;return 42;} std::vector<Waypoint*> *getWaypoints(){return &waypoints;}};
struct Game {
    SavedGame save; SavedGame *getSavedGame(){return &save;}
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0;int pushes=0;void*pushed=nullptr;template<class T>void pushState(T*s){++pushes;pushed=s;top=s;}
    Screen *getScreen(){return &screen;} Mod *getMod(){return &mod;}
    Language *getLanguage(){return language;}
    bool isState(const void *state) const{return top == state;}
    bool cursorVisible=false; SDL_Rect cursor={0,0,0,0};
    bool agentOverlayClear(const AgentUiState &ui) const{
        if(!clear)return false;
        for(const auto &r:ui.regions)if(cursorVisible && r.x<cursor.x+cursor.w && r.x+r.width>cursor.x && r.y<cursor.y+cursor.h && r.y+r.height>cursor.y)return false;
        return true;
    }
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
    void add(Surface *surface,const std::string&,const std::string&){
        surface->setPalette(_game->palette);
        surface->initText(_game->big,_game->small,_game->language);
        _surfaces.push_back(surface);
    }
    void blit() override{
        for(auto *surface:_surfaces)surface->blit(_game->screen.surface);
        if(paintHook)paintHook();
    }
};
struct Target {virtual ~Target(){} std::string name;std::string getName(Language*)const{return name;}};
struct Base:Target {std::string getName()const{return name;}};
static int waypointDeletes=0;
struct Waypoint:Target {Observer::AgentTargetLifetime lifetime;std::weak_ptr<const void>agentLifetime()const{return lifetime.token();}~Waypoint(){++waypointDeletes;}int id=1;int getId(){return id;}void setId(int i){id=i;}};
struct Ufo:Target {enum{FLYING}; int getStatus(){return FLYING;}int getId(){return 1;}int getSpeed(){return 500;}};
struct RuleCraft {int getMaxSpeed(){return 1000;}bool isWaterOnly(){return false;}int getWeapons(){return 0;}int getSoldiers(){return 0;}int getVehicles(){return 0;}};
struct RuleCraftWeapon {std::string getType(){return "STR_NONE_UC";}};
struct CraftWeapon {RuleCraftWeapon rule; RuleCraftWeapon*getRules(){return &rule;}int getAmmo(){return 0;}};
struct Craft {
 std::string name;Target*destination=nullptr;int returns=0,sets=0;Base base;RuleCraft rule;std::vector<CraftWeapon*> weapons;
 std::string getName(Language*)const{return name;}Target*getDestination()const{return destination;}
 void returnToBase(){++returns;}void setDestination(Target*t){++sets;destination=t;}
 bool getLowFuel(){return false;}bool getMissionComplete(){return false;}bool isInDogfight(){return false;}
 Base*getBase(){return &base;}int getSpeed(){return 500;}RuleCraft*getRules(){return &rule;}
 std::string getAltitude(){return "STR_HIGH";}double getLongitude(){return 0;}double getLatitude(){return 0;}
 int getFuelPercentage(){return 50;}int getDamagePercentage(){return 0;}std::vector<CraftWeapon*>*getWeapons(){return &weapons;}
 int getNumSoldiers(){return 0;}int getNumVehicles(){return 0;}
};
struct Globe {int speed=60;bool insideLand(double,double){return true;}};
struct SelectDestinationState {Craft*craft;Globe*globe;SelectDestinationState(Craft*c,Globe*g):craft(c),globe(g){}}; // Capture seam; actual ctor qualified separately.
struct GeoscapeCraftState : State {
 Craft *_craft;Globe *_globe;Waypoint *_waypoint;
 TextButton *_btnBase,*_btnTarget,*_btnPatrol,*_btnCancel;Window *_window;
 Text *_txtTitle,*_txtStatus,*_txtBase,*_txtSpeed,*_txtMaxSpeed,*_txtAltitude,*_txtFuel,*_txtDamage,*_txtW1Name,*_txtW1Ammo,*_txtW2Name,*_txtW2Ammo,*_txtRedirect,*_txtSoldier,*_txtHWP;
 Waypoint *_agentWaypoint=nullptr;std::weak_ptr<const void>_agentWaypointLifetime;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 GeoscapeCraftState(Craft*,Globe*,Waypoint*);~GeoscapeCraftState();
 void btnCancelClick(Action*);void btnBaseClick(Action*);void btnTargetClick(Action*);void btnPatrolClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void finishPopup(){_window->think();for(int i=0;i<30;++i)_window->popup();}
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "GeoscapeCraft.generated.inc"
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
    const std::string token(32,'1'),prefix="E "+token+" "+token+" "+token+" 1 ";
    for(const std::string action:{"cancel_craft_navigation"}){
        check(parseAgentRequest(prefix+action,request),"empty argument protocol accepted");
        for(const std::string extra:{" extra"," r0"," true"," 0","\n"})
            check(!parseAgentRequest(prefix+action+extra,request),"nonempty argument rejected");
    }
    for(const std::string master:{"xcom1","xcom2"}){
 Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");
 Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
 Fixture::Craft craft;craft.name="VISIBLE CRAFT";craft.base.name="BASE";Fixture::Globe globe;
 Fixture::GeoscapeCraftState state(&craft,&globe,nullptr);game.top=&state;AgentUiState ui;
 check(!state.agentUiState(ui),"prepaint rejected");state.blit();check(!state.agentUiState(ui),"animation rejected");state.finishPopup();state.blit();
 check(state.agentUiState(ui)&&ui.screen=="geoscape_craft_navigation","actual constructor admitted");
 check(ui.actions.size()==2&&ui.offers("cancel_craft_navigation")&&ui.offers("open_craft_destination"),"target and ordinary cancel exposed");
 std::cout << "NATIVE_NAVIGATION_PAYLOAD " << "{" << ui.payload() << "}" << "\n";
 auto visible=YAML::Load(ui.visible);check(visible.size()==4&&visible["redirect"].IsNull()&&visible["title"].as<std::string>()=="VISIBLE CRAFT"&&visible["buttons"].size()==2&&visible["buttons"][0]["id"].as<std::string>()=="target"&&visible["buttons"][1]["id"].as<std::string>()=="cancel","exact UI DTO");
 craft.name="UNDISPLAYED MODEL";check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()=="VISIBLE CRAFT","no model reconstruction");
 game.cursorVisible=true;game.cursor={33,21,4,4};check(state.agentUiState(ui)&&ui.offers("cancel_craft_navigation"),"caption cursor ignored");game.cursor={23,173,4,4};request.action="cancel_craft_navigation";check(!state.agentDispatch(request)&&game.pops==0,"button cursor refusal");game.cursorVisible=false;
 for(auto text:{state._txtTitle,state._txtStatus}){
  std::string old=text->getText();text->setText(std::string(500,'W'));state.blit();if(text==state._txtTitle)check(text->getFont()==&small,"long title actually auto-switches to small native font");check(state.agentUiState(ui)&&YAML::Load(ui.visible)[text==state._txtTitle?"title":"status"].as<std::string>()==std::string(500,'W'),"long active logical caption exact despite auto small font");
  SDL_FillRect(text->getSurface(),nullptr,0);check(state.agentUiState(ui),"ink not admission");text->setText(old);check(!state.agentUiState(ui),"changed text requires repaint");state.blit();
 }
 for(auto text:{state._txtTitle,state._txtStatus}){
  std::string old=text->getText();const char*field=text==state._txtTitle?"title":"status";
  for(const std::string caption:{std::string("LINE1\nLINE2\nLINE3"),std::string("\xF0\x9F\x99\x82")}){text->setText(caption);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)[field].as<std::string>()==caption,"multiline or missing glyph logical caption exact");}
  text->setText(old);state.blit();
 }
 const auto titleText=state._txtTitle->getText();state._txtTitle->setSmall();state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==titleText,"alternate font height never changes fixed observation strip");state._txtTitle->setBig();state.blit();
 Surface retainedCover(5,2,33,22);retainedCover.setPalette(palette);retainedCover.clear(80);state._surfaces.push_back(&retainedCover);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull(),"real later overlay inside retained title region withheld");state._surfaces.pop_back();state.blit();
 Surface spareCover(5,1,33,36);spareCover.setPalette(palette);spareCover.clear(80);state._surfaces.push_back(&spareCover);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==titleText,"fixed spare row does not hide title");state._surfaces.pop_back();state.blit();
 state._txtTitle->setVisible(false);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull(),"hidden title null");state._txtTitle->setVisible(true);state.blit();
 for(Surface*w:std::vector<Surface*>{state._txtTitle,state._txtStatus,state._btnCancel,state._txtRedirect}){int x=w->getX();w->setX(x+1);w->setX(x);check(!state.agentUiState(ui),"geometry ABA");state.blit();w->setVisible(!w->getVisible());w->setVisible(!w->getVisible());check(!state.agentUiState(ui),"visibility ABA");state.blit();}
 state._modal=state._btnCancel;check(!state.agentUiState(ui)&&!state.agentDispatch(request),"modal refused");state._modal=nullptr;game.top=nullptr;check(!state.agentUiState(ui),"wrong top");game.top=&state;
 game.clear=false;check(!state.agentUiState(ui),"overlay");game.clear=true;
 SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped paint");SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
 state._txtRedirect->setVisible(true);state.blit();check(!state.agentUiState(ui)&&!state.agentDispatch(request),"redirect heading unsafe");state._txtRedirect->setVisible(false);state.blit();
 std::string label=state._btnCancel->getText();state._btnCancel->setText("WRONG");state.blit();check(!state.agentDispatch(request),"noncancel label refused");state._btnCancel->setText(label);state.blit();
 Fixture::Waypoint waypoint;state._waypoint=&waypoint;state.blit();check(!state.agentUiState(ui)&&!state.agentDispatch(request)&&craft.sets==0,"nonnull waypoint rejected despite spoof ordinary label and hidden heading");request.action="open_craft_destination";check(!state.agentDispatch(request)&&game.pushes==0,"nonnull waypoint also refuses Target opening");request.action="cancel_craft_navigation";state._waypoint=nullptr;state.blit();
 check(state.agentUiState(ui),"fresh authority");AgentFrameCommand frame;check(frame.stage(request,ui,&state,1,1,100,100),"stage cancel");Fixture::Craft alternate;state._craft=&alternate;AgentUiState changed;check(state.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&state,changed),"craft change invalidates");state._craft=&craft;
 for(const std::string invalid:{"unknown","acknowledge_craft_patrol","redirect_patrolling_craft"}){request.action=invalid;check(!state.agentDispatch(request)&&game.pops==0,"other actions no dispatch");}
 Fixture::Target retained;craft.destination=&retained;request.action="cancel_craft_navigation";check(state.agentDispatch(request)&&game.pops==1&&craft.destination==&retained&&craft.sets==0&&craft.returns==0&&globe.speed==60&&game.save.waypoints.empty()&&game.save.idCalls==0,"ordinary cancel pop only preserves nonnull craft target and speed");check(!state.agentDispatch(request)&&game.pops==1,"no repeat after pop");state.release();
 Fixture::GeoscapeCraftState redirected(&craft,&globe,&waypoint);game.top=&redirected;redirected.finishPopup();redirected.blit();check(redirected.agentUiState(ui)&&ui.offers("go_to_craft_last_known_position")&&!ui.offers("cancel_craft_navigation"),"actual nonnull constructor has explicit last-known not Cancel");redirected._txtRedirect->setVisible(false);redirected._btnCancel->setText(language.getString("STR_CANCEL_UC"));redirected.blit();check(!redirected.agentDispatch(request)&&craft.sets==0,"spoofed nonnull constructor remains refused");redirected.release();
 Fixture::GeoscapeCraftState target(&craft,&globe,nullptr);game.top=&target;target.finishPopup();target.blit();request.action="open_craft_destination";int oldPops=game.pops;
 target._btnTarget->setVisible(false);target.blit();check(!target.agentDispatch(request)&&game.pushes==0,"hidden Target no opening");target._btnTarget->setVisible(true);target.blit();game.cursorVisible=true;game.cursor={23,141,4,4};check(!target.agentDispatch(request)&&game.pushes==0,"pointer on Target refuses opening");game.cursorVisible=false;check(target.agentDispatch(request)&&game.pops==oldPops+1&&game.pushes==1,"Target literal pop then push exactly once");auto*pushed=static_cast<Fixture::SelectDestinationState*>(game.pushed);check(pushed&&pushed->craft==&craft&&pushed->globe==&globe&&craft.destination==&retained&&craft.sets==0&&craft.returns==0&&globe.speed==60&&game.save.idCalls==0,"Target preserves craft and passes same craft/globe");check(!target.agentDispatch(request)&&game.pushes==1,"Target no replay");delete pushed;game.top=nullptr;target.release();

 for(const std::string action:{"return_craft_to_base","patrol_craft_current_position","go_to_craft_last_known_position","open_craft_destination"}){
   Fixture::Craft rc;rc.name="VISIBLE CRAFT";rc.base.name="VISIBLE BASE";Fixture::Target prior;rc.destination=&prior;
   auto*wp=new Fixture::Waypoint;const int deleted=Fixture::waypointDeletes, pops=game.pops, pushes=game.pushes, ids=game.save.idCalls;
   Fixture::GeoscapeCraftState modal(&rc,&globe,wp);game.top=&modal;modal.finishPopup();modal.blit();
   bool admitted=modal.agentUiState(ui);std::cerr<<"REDIRECT_DIAG admitted="<<admitted<<" actions=";for(const auto&a:ui.actions)std::cerr<<a.id<<",";std::cerr<<" base="<<modal._btnBase->getX()<<","<<modal._btnBase->getY()<<","<<modal._btnBase->getWidth()<<","<<modal._btnBase->getHeight()<<" redirect="<<modal._txtRedirect->getX()<<","<<modal._txtRedirect->getY()<<","<<modal._txtRedirect->getWidth()<<","<<modal._txtRedirect->getHeight()<<"\n";check(admitted&&ui.actions.size()==4,"redirect constructor four visible choices");
 std::cout << "NATIVE_NAVIGATION_PAYLOAD " << "{" << ui.payload() << "}" << "\n";
   bool baseRegion=false;for(const auto&r:ui.regions)if(r.x==22&&r.y==125&&r.width==212&&r.height==11)baseRegion=true;check(baseRegion,"Base control admitted only in actual unobscured eleven-pixel hitbox");
   Surface interior(2,2,23,125);interior.setPalette(palette);interior.clear(80);modal._surfaces.push_back(&interior);modal.blit();check(modal.agentUiState(ui)&&!ui.offers("return_craft_to_base"),"remaining Base interior overlay withholds action");modal._surfaces.pop_back();modal.blit();
   game.cursorVisible=true;game.cursor={23,125,2,2};check(modal.agentUiState(ui)&&!ui.offers("return_craft_to_base"),"remaining Base cursor overlap withholds action");game.cursorVisible=false;
   modal._txtRedirect->setHeight(18);modal.blit();check(!modal.agentUiState(ui)||!ui.offers("return_craft_to_base"),"expanded caption cannot use native hitbox exception");modal._txtRedirect->setHeight(17);modal.blit();
   modal._txtRedirect->setY(109);modal.blit();check(!modal.agentUiState(ui)||!ui.offers("return_craft_to_base"),"moved caption cannot use native hitbox exception");modal._txtRedirect->setY(108);modal.blit();check(modal.agentUiState(ui),"redirect restored after fresh paint");
   check(YAML::Load(ui.visible)["redirect"].as<std::string>()==std::string(language.getString("STR_REDIRECT_CRAFT")),"actual current redirect caption");
   request.action="cancel_craft_navigation";check(!modal.agentDispatch(request)&&game.pops==pops,"last-known never generic Cancel");
   request.action=action;AgentFrameCommand owned;check(owned.stage(request,ui,&modal,1,1,100,100),"redirect action staged");
   auto*other=new Fixture::Waypoint;modal._waypoint=other;modal.blit();check(!modal.agentUiState(ui),"replacement temporary waypoint cannot retarget authority");modal._waypoint=wp;delete other;modal.blit();
   modal._txtRedirect->setVisible(false);modal.blit();check(!modal.agentDispatch(request),"hidden redirect mode cannot dispatch");modal._txtRedirect->setVisible(true);modal.blit();
   game.clear=false;check(!modal.agentDispatch(request),"redirect overlay blocks dispatch");game.clear=true;
   modal._btnBase->setText("CHANGED");check(!modal.agentUiState(ui),"new button stale label invalidates authority");modal._btnBase->setText(language.getString("STR_RETURN_TO_BASE"));modal.blit();
   const int beforeActionDelete=Fixture::waypointDeletes;
   check(modal.agentDispatch(request)&&game.pops==pops+1,"one native redirect action");
   if(action=="go_to_craft_last_known_position"){
     check(Fixture::waypointDeletes==beforeActionDelete&&game.save.waypoints.size()==1&&game.save.waypoints[0]==wp&&rc.destination==wp&&wp->id==42&&game.save.idCalls==ids+1&&rc.sets==1&&rc.returns==0,"last-known transfers live waypoint to save and craft");
   }else{
     check(Fixture::waypointDeletes==beforeActionDelete+1&&game.save.waypoints.empty()&&game.save.idCalls==ids,"other redirect actions destroy temporary waypoint exactly once");
     if(action=="return_craft_to_base")check(rc.returns==1&&rc.sets==0,"return uses native return request only");
     if(action=="patrol_craft_current_position")check(rc.destination==nullptr&&rc.sets==1&&rc.returns==0,"patrol clears destination natively");
     if(action=="open_craft_destination"){check(game.pushes==pushes+1,"target opens picker");delete static_cast<Fixture::SelectDestinationState*>(game.pushed);game.pushed=nullptr;}
   }
   check(!modal.agentDispatch(request)&&game.pops==pops+1,"redirect no replay after pop");
   if(action=="go_to_craft_last_known_position"){game.save.waypoints.clear();delete wp;rc.destination=nullptr;}
   game.top=nullptr;modal.release();check(Fixture::waypointDeletes>=deleted+2,"fixture releases all temporary ownership");
 }
 // Expired target lifetime must reject without dereferencing a destroyed temporary.
 {auto*wp=new Fixture::Waypoint;Fixture::GeoscapeCraftState expired(&craft,&globe,wp);game.top=&expired;expired.finishPopup();expired.blit();delete wp;request.action="go_to_craft_last_known_position";check(!expired.agentUiState(ui)&&!expired.agentDispatch(request),"expired temporary lifetime refused");game.top=nullptr;expired.release();}

 }
 check(!SDL_GetVideoSurface(),"no game video");std::cout<<"PASS "<<checks<<" navigation constructor/provider/cancel checks; explicit domain seams\n";
}
