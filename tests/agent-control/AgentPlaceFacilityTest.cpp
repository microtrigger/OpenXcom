// SPDX-License-Identifier: GPL-3.0-or-later
// Native BaseView rendering, hover/click/eligibility, Base/Facility/SavedGame and
// literal PlaceFacilityState constructor/provider/dispatch/ordinary callback.
// Seams: owning Game/State initialization, synthetic texture/interface resources,
// and error-popup construction (ordinary error dismiss callback remains literal).
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/LocalizedText.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/SurfaceSet.h"
#include "../../src/Engine/Unicode.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Window.h"
#include "../../src/Basescape/BaseView.h"
#include "../../src/Savegame/Base.h"
#include "../../src/Savegame/BaseFacility.h"
#include "../../src/Savegame/SavedGame.h"
#include "../../src/Mod/Mod.h"
#include "../../src/Mod/RuleInterface.h"
// Test-only synthetic rule setup; native getters and model implementations used.
#define private public
#include "../../src/Mod/RuleBaseFacility.h"
#undef private
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
using namespace OpenXcom;using namespace OpenXcom::Observer;
namespace Fixture {
struct State;
struct Screen {Surface *surface;Surface *getSurface(){return surface;}};
struct Resources {
 Surface *background;SurfaceSet *texture;RuleInterface interfaceRule{"placeFacility"};
 Resources(){interfaceRule.load(YAML::Load("elements: [{id: errorMessage, color: 32}, {id: errorPalette, color: -1}]"));}
 Surface *getSurface(const std::string&){return background;}
 SurfaceSet *getSurfaceSet(const std::string&){return texture;}
 RuleInterface *getInterface(const std::string&){return &interfaceRule;}
};
struct Game {
 Screen screen;Resources mod;SavedGame saved;Language *language;Font *big,*small;SDL_Color *palette;
 std::vector<State*> stack;std::vector<State*> errors;Surface *cursor=nullptr,*fps=nullptr;unsigned pops=0,pushes=0;
 Screen *getScreen(){return &screen;}Resources *getMod(){return &mod;}SavedGame *getSavedGame(){return &saved;}
 Surface *getCursor(){return cursor;}Surface *getFpsCounter(){return fps;}
 bool isState(const void*s)const{return !stack.empty()&&stack.back()==s;}
 bool agentOverlayClear(const AgentUiState&ui)const{
  for(auto*s:{cursor,fps})if(s&&s->getVisible())for(const auto&r:ui.regions)
   if(r.x<s->getX()+s->getWidth()&&r.x+r.width>s->getX()&&r.y<s->getY()+s->getHeight()&&r.y+r.height>s->getY())return false;
  return true;
 }
 void popState(){++pops;if(!stack.empty())stack.pop_back();}
 void pushState(State*s){++pushes;stack.push_back(s);errors.push_back(s);}
};
static Game *currentGame;
struct State:OpenXcom::State {
 Game *_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;
 void setInterface(const std::string&){std::copy(_game->palette,_game->palette+256,_palette);}
 void centerAllSurfaces(){}
 void init()override{}
 LocalizedText tr(const std::string&id)const{return _game->language->getString(id);}
 LocalizedText tr(const std::string&id,unsigned n)const{return _game->language->getString(id,n);}
 void add(Surface*s,const std::string&,const std::string&){
  s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);
  if(auto *view=dynamic_cast<BaseView*>(s)){view->setColor(32);view->setSecondaryColor(48);}
  _surfaces.push_back(s);
 }
 void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}
};
struct ErrorMessageState:State {
 std::string message;TextButton *button;
 ErrorMessageState(const std::string&m,SDL_Color*,int,const std::string&,int):message(m){
  button=new TextButton(80,16,120,92);add(button,"button","error");button->setText("OK");button->onMouseClick((ActionHandler)&ErrorMessageState::btnOkClick);
 }
 void btnOkClick(Action*);
 void dismiss(){SDL_Event event={};event.type=SDL_MOUSEBUTTONUP;event.button.button=SDL_BUTTON_LEFT;Action action(&event,1,1,0,0);action.setSender(button);action.setMouseAction(160,100,120,92);button->mouseClick(&action,this);}
};
struct PlaceFacilityState:State {
 Base *_base;RuleBaseFacility *_rule;BaseView *_view;TextButton *_btnCancel;Window *_window;
 Text *_txtFacility,*_txtCost,*_numCost,*_txtTime,*_numTime,*_txtMaintenance,*_numMaintenance;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 unsigned auditCalls=0;InteractiveSurface *auditSender=nullptr;int auditX=-1,auditY=-1;
 PlaceFacilityState(Base*,RuleBaseFacility*);~PlaceFacilityState();void btnCancelClick(Action*);void viewClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void auditClick(Action*a){++auditCalls;auditSender=a->getSender();auditX=int(a->getRelativeXMouse());auditY=int(a->getRelativeYMouse());}
 void enableAudit(){_view->onMouseClick((ActionHandler)&PlaceFacilityState::auditClick,0);}
};
#include "PlaceFacility.generated.inc"
#include "ErrorDismiss.generated.inc"
}
static unsigned checks=0;
static void check(bool value,const char*what){++checks;if(!value){std::cerr<<"FAIL "<<what<<'\n';std::exit(1);}}
static bool offered(const AgentUiState&ui,int x,int y){const auto grid=YAML::Load(ui.visible)["grid"];if(grid.IsNull())return false;for(auto c:grid["cells"])if(c["x"].as<int>()==x&&c["y"].as<int>()==y)return true;return false;}
int main(int argc,char**argv){
 check(argc==3&&SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen native fixture only");FileMap::load("common","./",false);
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Options::baseXResolution=320;Options::baseYResolution=200;Options::allowBuildingQueue=false;
 SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
 Surface canvas(640,400),background(320,200);canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 SurfaceSet texture(32,32);for(int i=0;i<64;++i){auto*f=texture.addFrame(i);f->setPalette(palette);f->clear(Uint8(16+i));}
 Mod nativeMod;RuleBaseFacility lift("STR_ACCESS_LIFT"),rule("STR_GENERAL_STORES"),wide("STR_HANGAR");
 for(auto*r:{&lift,&rule,&wide}){r->_spriteShape=1;r->_spriteFacility=4;r->_size=1;r->_buildCost=150000;r->_buildTime=10;r->_monthlyCost=5000;}lift._lift=true;wide._size=2;
 const std::string token(32,'1'),prefix="E "+token+" 1 "+token+" "+token+" 1 place_facility_cell ";
 AgentRequest parsed;for(const auto &args:{"0 0","5 5","2 4"})check(parseAgentRequest(prefix+args,parsed),"canonical protocol cell accepted");
 for(const auto &args:{"-1 0","6 0","0 6","00 0","1.0 2","1","1 2 extra","+1 2","1 -0"})check(!parseAgentRequest(prefix+args,parsed),"malformed/offgrid protocol cell refused");
 check(std::string(agentArgumentKind("place_facility_cell"))=="facility_cell"&&std::string(agentProtocolSchema())=="openxcom-agent-control/39","explicit schema and argument kind");
 for(const auto &variant:{"xcom1","xcom2"}){
  Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+variant+"/Language/en-US.yml");
  Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.mod.texture=&texture;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
  Fixture::State previous;Base base(&nativeMod);auto*existing=new BaseFacility(&lift,&base);existing->setX(0);existing->setY(0);base.getFacilities()->push_back(existing);game.saved.setFunds(500000);
  {
   Fixture::PlaceFacilityState state(&base,&rule);state.enableAudit();game.stack={&previous,&state};AgentUiState ui;AgentRequest req;req.action="place_facility_cell";req.x=1;req.y=0;
   check(!state.agentUiState(ui)&&!state.agentDispatch(req),"unpainted placement refused");state.blit();check(state.agentUiState(ui)&&ui.offers(req.action),"painted actual native grid offered");
   std::ofstream(std::string(argv[2])+"/visible-"+variant+".json") << ui.visible << "\n";
   check(YAML::Load(ui.visible)["grid"]["cells"].size()==36&&offered(ui,0,0)&&offered(ui,5,5),"all visible cells offered including occupied disconnected and footprint-invalid");
   req.x=6;check(!state.agentDispatch(req)&&state.auditCalls==0,"direct offgrid request refused");req.x=1;
   state._modal=state._btnCancel;check(!state.agentDispatch(req),"local modal blocks placement");state._modal=nullptr;
   game.stack.pop_back();check(!state.agentDispatch(req),"inactive state blocks placement");game.stack.push_back(&state);
   state._view->setVisible(false);state.blit();check(state.agentUiState(ui)&&!ui.offers(req.action),"hidden view offers no placement");state._view->setVisible(true);check(!state.agentDispatch(req),"show without repaint is stale");state.blit();
   SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentDispatch(req),"destination clip refuses placement");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentDispatch(req),"clip restore alone does not revive authority");state.blit();
   Surface cover(32,32,32,8);cover.setPalette(palette);cover.clear(100);state._surfaces.push_back(&cover);state.blit();check(state.agentUiState(ui)&&!offered(ui,1,0)&&offered(ui,2,0)&&!state.agentDispatch(req),"real later cell overlay removes only covered target");state._surfaces.pop_back();state.blit();
   game.cursor=&cover;cover.blit(&canvas);check(state.agentUiState(ui)&&!offered(ui,1,0)&&!state.agentDispatch(req),"cursor overlay blocks target");game.cursor=nullptr;state.blit();
   AgentFrameCommand frame;check(state.agentUiState(ui)&&frame.stage(req,ui,&state,1,1,100,100),"stage placement frame authority");state._view->setX(1);state._view->setX(0);check(!state.agentDispatch(req),"geometry ABA stale placement refused");state.blit();check(state.agentUiState(ui)&&!frame.matchesFrame(101,1,1,&state,ui),"fresh redraw cannot replay old geometry grant");
   req.x=0;req.y=0;check(state.agentDispatch(req)&&base.getFacilities()->size()==1&&game.saved.getFunds()==500000,"occupied native handler rejects without economic mutation");
   check(state.auditCalls==1&&state.auditSender==state._view&&state.auditX==16&&state.auditY==16,"registered callback receives non-null sender and cell center");
   auto*error=dynamic_cast<Fixture::ErrorMessageState*>(game.stack.back());check(error&&error->message==std::string(language.getString("STR_CANNOT_BUILD_HERE")),"ordinary occupied error popup");check(!state.agentDispatch(req),"error top state blocks replay");error->dismiss();check(game.isState(&state),"ordinary error dismiss restores placement");state.blit();
   req.x=5;req.y=5;check(state.agentDispatch(req)&&base.getFacilities()->size()==1&&game.saved.getFunds()==500000,"disconnected native handler rejection");error=dynamic_cast<Fixture::ErrorMessageState*>(game.stack.back());check(error!=nullptr,"disconnected popup exists");error->dismiss();state.blit();
   req.x=1;req.y=0;check(state.agentDispatch(req)&&base.getFacilities()->size()==2&&game.saved.getFunds()==350000,"ordinary success creates one facility and charges once");
   auto*built=base.getFacilities()->back();check(built->getX()==1&&built->getY()==0&&built->getBuildTime()==10&&built->getRules()==&rule,"native success layout rule and construction time");check(game.isState(&previous)&&!state.agentDispatch(req)&&game.saved.getFunds()==350000&&base.getFacilities()->size()==2,"successful pop prevents duplicate construction");
  }
  {
   game.saved.setFunds(0);Fixture::PlaceFacilityState state(&base,&rule);state.enableAudit();game.stack={&previous,&state};state.blit();AgentUiState ui;check(state.agentUiState(ui)&&offered(ui,0,1),"insufficient funds does not prefilter visible cell");AgentRequest req;req.action="place_facility_cell";req.x=0;req.y=1;
   check(state.agentDispatch(req)&&base.getFacilities()->size()==2&&game.saved.getFunds()==0,"native insufficient funds preserves economy");auto*error=dynamic_cast<Fixture::ErrorMessageState*>(game.stack.back());check(error&&error->message==std::string(language.getString("STR_NOT_ENOUGH_MONEY")),"ordinary funds error");error->dismiss();check(game.isState(&previous)&&!state.agentDispatch(req),"funds path pops placement before error and cannot replay");
  }
  {
   game.saved.setFunds(500000);Fixture::PlaceFacilityState state(&base,&wide);state.enableAudit();game.stack={&previous,&state};state.blit();AgentUiState ui;check(state.agentUiState(ui)&&offered(ui,5,5),"size-two invalid footprint still native offered cell");AgentRequest req;req.action="place_facility_cell";req.x=5;req.y=5;check(state.agentDispatch(req)&&base.getFacilities()->size()==2&&game.saved.getFunds()==500000,"native size-two edge rejection");auto*error=dynamic_cast<Fixture::ErrorMessageState*>(game.stack.back());check(error!=nullptr,"edge error exists");error->dismiss();
  }
  {
   Options::baseXResolution=640;Options::baseYResolution=400;Fixture::PlaceFacilityState state(&base,&rule);state.enableAudit();game.stack={&previous,&state};state._view->setX(40);state._view->setY(208);state.blit();AgentUiState ui;check(state.agentUiState(ui)&&YAML::Load(ui.visible)["grid"]["origin_x"].as<int>()==40&&YAML::Load(ui.visible)["grid"]["origin_y"].as<int>()==208,"translated real grid geometry");AgentRequest req;req.action="place_facility_cell";req.x=0;req.y=1;
   check(state.agentDispatch(req)&&state.auditSender==state._view&&state.auditX==16&&state.auditY==48,"translated action maps actual local cell center");check(base.getFacilities()->back()->getX()==0&&base.getFacilities()->back()->getY()==1&&game.saved.getFunds()==350000,"translated native placement uses cell not absolute pixel coordinates");Options::baseXResolution=320;Options::baseYResolution=200;
  }
  for(auto*error:game.errors)delete error;game.errors.clear();game.stack.clear();
 }
 check(!SDL_GetVideoSurface(),"no game video opened");std::cout<<"PASS "<<checks<<" native facility placement checks; explicit owning/resource/error-construction seams\n";
}
