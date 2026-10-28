// SPDX-License-Identifier: GPL-3.0-or-later
// Literal target popup constructor/provider/dispatch/callback bodies.
// Actual SDL widgets; explicit target models/lifetime/transition/render-loop seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Window.h"
#include "../../src/Observer/AgentUi.h"
#define private public
#include "../../src/Interface/TextList.h"
#undef private
#include "../../src/Observer/AgentOutfitting.h"
#include <cmath>
#include <set>
#include "../../src/Interface/ArrowButton.h"
#include "../../src/Engine/Timer.h"
#include "../../src/fmath.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <map>
#include <sstream>
#include "../../src/Interface/TextList.h"
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


#include "../../src/Interface/TextEdit.h"
#include "../../src/Observer/AgentTargetLifetime.h"
namespace Fixture {
struct MovingTarget;
struct Target {AgentTargetLifetime life;std::string name,defaultName="Waypoint";std::vector<MovingTarget*>followers;virtual ~Target(){}std::weak_ptr<const void> agentLifetime()const{return life.token();}std::string getName(Language*)const{return name.empty()?defaultName:name;}std::string getDefaultName(Language*)const{return defaultName;}void setName(const std::string&s){name=s;}std::vector<MovingTarget*>*getFollowers(){return &followers;}};
struct MovingTarget:Target{};struct Base:Target{};struct Craft:MovingTarget{};struct Ufo:MovingTarget{bool getHyperDetected(){return false;}};
struct Globe{};struct GeoscapeState{Globe globe;Globe*getGlobe(){return &globe;}};
static Target*destinationTarget=nullptr;static std::string destinationKind;
struct InterceptState{InterceptState(Globe*,Base*b=0,Target*t=0){destinationKind="intercept";destinationTarget=t?t:b;}};
struct GeoscapeCraftState{GeoscapeCraftState(Craft*c,Globe*,int){destinationKind="craft";destinationTarget=c;}};
struct UfoDetectedState{UfoDetectedState(Ufo*u,GeoscapeState*,bool,bool){destinationKind="ufo";destinationTarget=u;}};
struct ConfirmDestinationState{ConfirmDestinationState(Craft*,Target*t){destinationKind="confirm";destinationTarget=t;}};
struct Mod {Surface*background;Surface*getSurface(const std::string&){return background;}};
struct Screen {Surface*surface;Surface*getSurface(){return surface;}};
struct SavedGame{};
struct Game {SavedGame save;SavedGame*getSavedGame(){return &save;}mutable Screen screen;Mod mod;Language*language;Font*big,*small;SDL_Color*palette;const void*top=nullptr;bool clear=true;int pops=0,pushes=0;Screen*getScreen()const{return &screen;}Mod*getMod(){return &mod;}Language*getLanguage(){return language;}bool isState(const void*s)const{return top==s;}bool agentOverlayClear(const AgentUiState&)const{return clear;}bool agentNonCursorOverlayClear(const AgentUiState&)const{return clear;}void popState(){++pops;top=nullptr;}template<class T>void pushState(T*p){++pushes;top=p;}};
static Game*currentGame;
struct State:OpenXcom::State {Game*_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;void setInterface(const std::string&){}void centerAllSurfaces(){}LocalizedText tr(const std::string&s)const{return _game->language->getString(s);}void add(Surface*s,const std::string&,const std::string&){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);}void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}};
#include "TargetInfoClass.generated.inc"
#include "MultipleTargetsClass.generated.inc"
#include "TargetInfo.generated.inc"
#include "MultipleTargets.generated.inc"
}
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==3,"data and log args");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");FileMap::load("common","./",false);Options::keyboardMode=KEYBOARD_ON;Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"}){
 Language language;language.loadFile(std::string(argv[1])+"/standard/"+master+"/Language/en-US.yml");Fixture::Game g;g.screen.surface=&canvas;g.mod.background=&background;g.language=&language;g.big=&big;g.small=&small;g.palette=palette;Fixture::currentGame=&g;
 Fixture::Globe globe;Fixture::GeoscapeState geo;Fixture::Target target,other;target.defaultName="Waypoint";other.defaultName="Landing";Fixture::MovingTarget follower;follower.defaultName="Interceptor";target.followers.push_back(&follower);
 {Fixture::TargetInfoState st(&target,&globe);g.top=&st;AgentUiState ui;AgentRequest req;check(!st.agentUiState(ui),"info unpainted denied");st.blit();check(st.agentUiState(ui),"info painted");Options::keyboardMode=KEYBOARD_OFF;check(st.agentUiState(ui)&&!ui.offers("focus_target_name"),"keyboard off editor focus unsupported");Options::keyboardMode=KEYBOARD_ON;check(ui.visible.find("Interceptor")!=std::string::npos,"native visible followers");g.clear=false;check(!st.agentUiState(ui),"info overlay denied");g.clear=true;g.top=nullptr;check(!st.agentUiState(ui),"info non-top denied");g.top=&st;st._modal=st._btnOk;check(!st.agentUiState(ui),"info modal denied");st._modal=nullptr;req.action="focus_target_name";check(st.agentDispatch(req),"native editor focus action");st.blit();check(st.agentUiState(ui)&&!ui.offers("replace_target_name"),"caret occlusion does not reveal title");st._edtTitle->blink();st.blit();check(st.agentUiState(ui)&&ui.offers("replace_target_name")&&!ui.offers("close_target_info"),"native blink restores focused editor authority");req.action="replace_target_name";req.text="Test marker";check(st.agentDispatch(req)&&target.name=="Test marker","native rename");check(!st.agentUiState(ui),"rename awaits repaint");st.blit();check(st.agentUiState(ui)&&ui.visible.find("Test marker")!=std::string::npos,"rename painted");req.action="focus_target_name";check(st.agentDispatch(req),"refocus native editor");st._edtTitle->blink();st.blit();req.action="replace_target_name";req.text="Waypoint";check(st.agentDispatch(req)&&target.name.empty(),"native direct default name restoration despite own caret");st.blit();req.action="focus_target_name";check(st.agentDispatch(req),"refocus for input rejection");st._edtTitle->blink();st.blit();req.action="replace_target_name";req.text=std::string(120,'W');check(st.agentDispatch(req)&&target.name.empty()&&st._edtTitle->getText()=="Waypoint","too-wide input rollback no model mutation");st.blit();if(!st._edtTitle->agentEditable()){st._edtTitle->blink();st.blit();}req.text="\xf4\x8f\xbf\xbf";check(st.agentDispatch(req)&&target.name.empty()&&st._edtTitle->getText()=="Waypoint","unsupported glyph rollback no model mutation");st.blit();if(!st._edtTitle->agentEditable()){st._edtTitle->blink();st.blit();}st._edtTitle->setVisible(false);st.blit();check(st.agentUiState(ui)&&!ui.offers("replace_target_name")&&!st.agentDispatch(req),"hidden editor no input authority");st._edtTitle->setVisible(true);st.blit();if(!st._edtTitle->agentEditable()){st._edtTitle->blink();st.blit();}req.text="";check(st.agentDispatch(req)&&target.name.empty(),"native empty override restoration");st.blit();if(st._modal==st._edtTitle){req.action="finish_target_name";if(!st.agentUiState(ui)||!ui.offers(req.action)){st._edtTitle->blink();st.blit();}check(st.agentDispatch(req),"native finish default-name restoration");}st.blit();req.action="open_target_interception";int before=g.pushes;check(st.agentDispatch(req)&&g.pushes==before+1&&Fixture::destinationTarget==&target,"native interception destination");check(!st.agentDispatch(req),"info cannot replay after transition");g.top=&st;st.blit();req.action="close_target_info";before=g.pops;check(st.agentDispatch(req)&&g.pops==before+1,"native info close");st.release();}
 {Fixture::Target dying;Fixture::TargetInfoState st(&dying,&globe);g.top=&st;st.blit();AgentUiState ui;check(st.agentUiState(ui),"lifetime initially alive");dying.life.invalidate();check(!st.agentUiState(ui),"expired target rejected before dereference");st.release();}
 for(bool cancel:{true,false}){Fixture::MultipleTargetsState st({&target,&other},nullptr,&geo);g.top=&st;AgentUiState ui;AgentRequest req;check(!st.agentUiState(ui),"chooser unpainted denied");st.blit();check(st.agentUiState(ui)&&ui.rows.size()==2,"chooser painted two");AgentRequest staged;staged.action="choose_globe_target";staged.choice="r0";AgentFrameCommand frame;check(frame.stage(staged,ui,&st,1,1,100,100),"chooser staged fresh binding");st._btnTargets[0]->setText("Changed");st._btnTargets[0]->setText("Waypoint");check(!st.agentUiState(ui),"chooser unpainted caption ABA stale");st.blit();check(st.agentUiState(ui)&&!frame.matchesFrame(101,1,1,&st,ui),"chooser caption ABA changes grant after repaint");g.clear=false;check(!st.agentUiState(ui),"chooser overlay denied");g.clear=true;req.action="choose_globe_target";req.choice="r99";check(!st.agentDispatch(req),"chooser invalid handle rejected");if(cancel){req.action="cancel_globe_targets";int before=g.pops;check(st.agentDispatch(req)&&g.pops==before+1,"chooser native cancel");}else{st._btnTargets[0]->setVisible(false);st.blit();check(st.agentUiState(ui)&&ui.rows.size()==1&&ui.rows[0]==1,"chooser excludes hidden row");req.choice="r0";int before=g.pushes;check(st.agentDispatch(req)&&g.pushes==before+1,"chooser native sender transition");auto*child=static_cast<Fixture::TargetInfoState*>(const_cast<void*>(g.top));check(child->_target==&other,"visible handle resolves actual second target");child->release();delete child;g.top=nullptr;}check(!st.agentDispatch(req),"chooser transition no replay");st.release();}
 {Fixture::Target dying;Fixture::MultipleTargetsState st({&dying,&other},nullptr,&geo);g.top=&st;st.blit();AgentUiState ui;check(st.agentUiState(ui),"chooser lifetime initially alive");dying.life.invalidate();check(!st.agentUiState(ui),"chooser expired target denied");st.release();}

 }
 check(!SDL_GetVideoSurface(),"no game window");std::cout<<"PASS "<<checks<<" globe target popup native checks; explicit model/lifetime/transition seams\n";
}
