// SPDX-License-Identifier: GPL-3.0-or-later
// Actual site constructor/provider/dispatch/cancel with SDL widgets. Explicit Game/resources, Globe geography and site-selection seams; no real-globe or campaign construction claim.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Timer.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include <map>
#include <vector>
// Test-only access to actual private native cells; production headers unchanged.
#define private public
#include "../../src/Interface/TextList.h"
#undef private
#include <sstream>
#include "../../src/Interface/Window.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
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
static int deletedBases=0;
struct Base {~Base(){++deletedBases;}};
struct Screen {Surface *surface;Surface *getSurface(){return surface;}int getDX(){return 0;}int getDY(){return 0;}};
struct Mod {Surface *background;Surface *getSurface(const std::string&){return background;}};
struct Game {Screen screen;Mod mod;Language *language;Font *big,*small;SDL_Color *palette;const void *top=nullptr;int pops=0;Surface *cursor=nullptr,*fps=nullptr;int campaignBases=1,funds=123456;
 Screen *getScreen(){return &screen;}Mod *getMod(){return &mod;}Surface *getCursor(){return cursor;}Surface *getFpsCounter(){return fps;}bool isState(const void*s)const{return top==s;}
 bool agentOverlayClear(const AgentUiState&ui)const{for(auto*s:{cursor,fps})if(s&&s->getVisible())for(auto&r:ui.regions)if(r.x<s->getX()+s->getWidth()&&r.x+r.width>s->getX()&&r.y<s->getY()+s->getHeight()&&r.y+r.height>s->getY())return false;return true;}
 void popState(){++pops;top=nullptr;}
};
static Game *currentGame;
struct State:OpenXcom::State {Game *_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;
 void setInterface(const std::string&){} LocalizedText tr(const std::string&id)const{return _game->language->getString(id);}
 void add(Surface*s){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);}
 void add(Surface*s,const std::string&,const std::string&){add(s);}
 void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
// Deliberate sampled-geography seam: real globe drawing, land eligibility and
// campaign establishment are tested elsewhere, not claimed by this fixture.
struct Globe:InteractiveSurface {bool samples=true,onGlobe=true;int selected=0,sx=-1,sy=-1;Globe():InteractiveSurface(256,172,0,28){}
 std::uint64_t agentViewRevision()const{return 7;}bool pointOnVisibleGlobe(int,int){return onGlobe;}
 std::string agentGeography(AgentUiState&ui,const std::vector<Surface*>&){if(!samples)return "null";ui.regions.push_back({90,70,1,1});return "{\"samples\":[]}";}
};
struct BuildNewBaseState:State {Base *_base;Globe *_globe;InteractiveSurface *_btnRotateLeft,*_btnRotateRight,*_btnRotateUp,*_btnRotateDown,*_btnZoomIn,*_btnZoomOut;Window *_window;Text *_txtTitle;TextButton *_btnCancel;Timer *_hoverTimer;bool _first,_oldshowradar;double _oldlat,_oldlon;int _mousex,_mousey;
 BuildNewBaseState(Base*,Globe*,bool);~BuildNewBaseState();bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;void btnCancelClick(Action*);
 void selectSite(int x,int y){++_globe->selected;_globe->sx=x;_globe->sy=y;}
 void hoverRedraw(){}void globeClick(Action*){}
 void btnRotateLeftPress(Action*){}
 void btnRotateLeftRelease(Action*){}
 void btnRotateRightPress(Action*){}
 void btnRotateRightRelease(Action*){}
 void btnRotateUpPress(Action*){}
 void btnRotateUpRelease(Action*){}
 void btnRotateDownPress(Action*){}
 void btnRotateDownRelease(Action*){}
 void btnZoomInLeftClick(Action*){}
 void btnZoomInRightClick(Action*){}
 void btnZoomOutLeftClick(Action*){}
 void btnZoomOutRightClick(Action*){}
};
#include "BuildNewBase.generated.inc"
}
static unsigned checks=0;static void check(bool ok,const char*m){++checks;if(!ok){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==2&&SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(auto variant:{"xcom1","xcom2"}){
 Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+variant+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
 for(bool first:{true,false}){
 Fixture::Globe globe;auto*base=new Fixture::Base;Fixture::BuildNewBaseState state(base,&globe,first);game.top=&state;AgentUiState ui;AgentRequest req;
 check(!state.agentUiState(ui),"prepaint refuses");state.blit();check(state.agentUiState(ui)&&ui.screen=="build_base","native constructor/provider paints");check(ui.offers("place_base_site"),"offered geography site action");check(ui.offers("cancel_new_base")==!first,"cancel only nonfirst");
 Window animated(&state,256,28,0,0,POPUP_BOTH);auto*originalWindow=state._window;state._window=&animated;check(animated.agentPopupActive()&&!state.agentUiState(ui),"active native popup refused");state._window=originalWindow;
 req.action="place_base_site";req.x=91;req.y=70;check(!state.agentDispatch(req)&&globe.selected==0,"unoffered sample denied");req.x=90;req.y=70;check(state.agentDispatch(req)&&globe.selected==1&&globe.sx==90&&globe.sy==70,"exact sample calls selection seam");globe.samples=false;check(!state.agentDispatch(req),"absent geography denied");globe.samples=true;globe.onGlobe=false;check(!state.agentDispatch(req),"outside native globe denied");globe.onGlobe=true;
 state._modal=state._btnCancel;check(!state.agentUiState(ui),"modal blocks");state._modal=nullptr;game.top=nullptr;check(!state.agentUiState(ui),"inactive blocks");game.top=&state;
 state._window->setVisible(false);check(!state.agentUiState(ui),"hidden header blocks");state._window->setVisible(true);check(!state.agentUiState(ui),"visibility ABA needs repaint");state.blit();
 SDL_Rect clip={0,0,319,200};SDL_SetClipRect(canvas.getSurface(),&clip);check(!state.agentUiState(ui),"destination clip blocks");SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
 req.action="cancel_new_base";int deleted=Fixture::deletedBases,pops=game.pops;
 if(first){check(!state.agentDispatch(req)&&Fixture::deletedBases==deleted,"first cancel denied without deletion");delete base;}
 else{
 Surface overlay(54,12,186,8);overlay.setPalette(palette);overlay.clear(50);game.cursor=&overlay;overlay.blit(&canvas);check(!state.agentDispatch(req)&&Fixture::deletedBases==deleted,"actual overlay blocks cancel");game.cursor=nullptr;state.blit();
 state._btnCancel->setText("UNPAINTED");check(state.agentUiState(ui)&&!ui.offers("cancel_new_base"),"dirty named cancel no grant");state._btnCancel->setText(language.getString("STR_CANCEL_UC"));state.blit();
 check(state.agentDispatch(req)&&Fixture::deletedBases==deleted+1&&game.pops==pops+1,"ordinary cancel deletes provisional base and pops");check(game.campaignBases==1&&game.funds==123456,"cancel leaves campaign seam unchanged");check(!state.agentDispatch(req)&&Fixture::deletedBases==deleted+1,"inactive cancel replay denied");}
 state.release();
 }
 }
 std::cout<<"PASS "<<checks<<" site-provider checks; Globe/site-selection and campaign ownership seams explicit\n";return 0;
}
