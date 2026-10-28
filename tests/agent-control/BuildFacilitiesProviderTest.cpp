// SPDX-License-Identifier: GPL-3.0-or-later
// Actual constructor/provider/dispatch/selection/cancel bodies. Explicit Game, Mod, rule enumeration, BaseView and lifecycle seams; no placement or campaign construction claim.
#include "../../src/Engine/Action.h"
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
struct RuleBaseFacility {std::string id;std::string getType(){return id;}int getRequirements(){return 0;}bool isLift(){return false;}int getSize(){return 1;}int getBuildCost(){return 400000;}int getBuildTime(){return 25;}int getMonthlyCost(){return 40000;}};
struct Base {};
struct Screen {Surface *surface;Surface *getSurface(){return surface;}};
struct Mod {Surface *background;std::vector<std::string> ids;std::vector<RuleBaseFacility> rules;
 Surface *getSurface(const std::string&){return background;}int getSurfaceSet(const std::string&){return 0;}
 const std::vector<std::string>&getBaseFacilitiesList(){return ids;}RuleBaseFacility *getBaseFacility(const std::string&id){for(auto&r:rules)if(r.id==id)return &r;return nullptr;}};
struct SavedGame {bool isResearched(int){return true;}};
struct Game {Screen screen;Mod mod;SavedGame saved;Language *language;Font *big,*small;SDL_Color *palette;
 const void *top=nullptr;int pops=0,pushes=0;Surface *cursor=nullptr,*fps=nullptr;
 Screen *getScreen(){return &screen;}Mod *getMod(){return &mod;}SavedGame*getSavedGame(){return &saved;}
 Surface *getCursor(){return cursor;}Surface *getFpsCounter(){return fps;}
 bool isState(const void*s)const{return top==s;}
 bool agentOverlayClear(const AgentUiState&ui)const{for(auto*s:{cursor,fps})if(s&&s->getVisible())for(auto&r:ui.regions)if(r.x<s->getX()+s->getWidth()&&r.x+r.width>s->getX()&&r.y<s->getY()+s->getHeight()&&r.y+r.height>s->getY())return false;return true;}
 void popState(){++pops;top=nullptr;}void pushState(const void*s){++pushes;top=s;}
};
static Game *currentGame;
struct State:OpenXcom::State {Game *_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;
 void setInterface(const std::string&){}void centerAllSurfaces(){}
 LocalizedText tr(const std::string&id)const{return _game->language->getString(id);}LocalizedText tr(const std::string&id,unsigned n)const{return _game->language->getString(id,n);}
 void add(Surface*s,const std::string&,const std::string&){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);if(auto *list=dynamic_cast<TextList*>(s))list->setColor(32);_surfaces.push_back(s);}
 void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
// BaseView deliberately a rendering/selection setup seam; viewClick is excluded.
struct BaseView:InteractiveSurface {BaseView(int w,int h,int x,int y):InteractiveSurface(w,h,x,y){}void setTexture(int){}void setBase(Base*){}void setSelectable(int){}};
struct PlaceFacilityState:State {Base *_base;RuleBaseFacility *_rule;BaseView *_view;TextButton *_btnCancel;Window *_window;
 Text *_txtFacility,*_txtCost,*_numCost,*_txtTime,*_numTime,*_txtMaintenance,*_numMaintenance;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 PlaceFacilityState(Base*,RuleBaseFacility*);~PlaceFacilityState();void btnCancelClick(Action*);void viewClick(Action*){}
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
struct BuildFacilitiesState:State {Base *_base;State *_state;std::vector<RuleBaseFacility*> _facilities;TextButton *_btnOk;Window *_window;Text *_txtTitle;TextList *_lstFacilities;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 BuildFacilitiesState(Base*,State*);~BuildFacilitiesState();void PopulateBuildList();void init()override;void btnOkClick(Action*);void lstFacilitiesClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
#include "Facilities.generated.inc"
}
static unsigned checks=0;
static void check(bool ok,const char*m){++checks;if(!ok){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==2&&SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(auto variant:{"xcom1","xcom2"}){
 Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+variant+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
 for(int i=0;i<24;++i){game.mod.ids.push_back("STR_LABORATORY");Fixture::RuleBaseFacility rule;rule.id="STR_LABORATORY";game.mod.rules.push_back(rule);}
 Fixture::Base base;Fixture::State previous;Fixture::BuildFacilitiesState state(&base,&previous);game.top=&state;AgentUiState ui;AgentRequest req;
 check(!state.agentUiState(ui),"prepaint refuses");state.blit();check(!state.agentUiState(ui),"active popup refuses");for(int i=0;i<22;++i)state._window->popup();state.blit();
 check(state.agentUiState(ui)&&ui.screen=="build_facilities","actual constructor list admitted");
 check(!ui.rows.empty()&&ui.rows.size()<=32&&ui.offers("select_build_facility")&&ui.offers("close_build_facilities"),"bounded row and close actions");
 check(ui.visible.find("STR_LABORATORY")==std::string::npos,"rule identifiers never exported");
 SDL_Rect erased={200,64,104,104};SDL_FillRect(canvas.getSurface(),&erased,0);check(state.agentUiState(ui)&&ui.rows.empty()&&!ui.offers("select_build_facility"),"current list RGB erasure removes handles");state.blit();check(state.agentUiState(ui)&&!ui.rows.empty(),"list repaint restores rows");
 auto visible=YAML::Load(ui.visible);check(visible["rows"][0]["cells"].size()==1&&visible["rows"][0]["cells"][0].as<std::string>()==std::string(language.getString("STR_LABORATORY")),"displayed localized facility");
 req.action="select_build_facility";req.choice="r99";check(!state.agentDispatch(req)&&game.pushes==0,"invalid row denied");req.choice="r00";check(!state.agentDispatch(req),"noncanonical row denied");
 req.action="place_facility";check(!state.agentDispatch(req),"placement absent");
 state._modal=state._btnOk;check(!state.agentUiState(ui),"modal refused");state._modal=nullptr;game.top=nullptr;check(!state.agentUiState(ui),"inactive refused");game.top=&state;
 state._lstFacilities->setCellText(0,0,"UNPAINTED PRIVATE");check(!state.agentUiState(ui),"dirty list refuses");state._lstFacilities->setCellText(0,0,"VISIBLE");state.blit();check(state.agentUiState(ui)&&ui.visible.find("UNPAINTED PRIVATE")==std::string::npos,"no unpainted text leak");
 Surface overlay(104,104,200,64);overlay.setPalette(palette);overlay.clear(50);state._surfaces.push_back(&overlay);state.blit();check(state.agentUiState(ui)&&ui.rows.empty()&&!ui.offers("select_build_facility"),"later actual overlay removes row authority");state._surfaces.pop_back();state.blit();
 SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"destination clipping refused");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restoration alone not authority");state.blit();
 check(state.agentUiState(ui)&&ui.offers("scroll_visible_list"),"scroll offered");auto oldRows=ui.rows;AgentFrameCommand frame;req.action="select_build_facility";req.choice="r0";check(frame.stage(req,ui,&state,1,1,100,100),"stage row authority");req.action="scroll_visible_list";req.choice="down";check(state.agentDispatch(req),"ordinary scroll dispatched");check(!state.agentUiState(ui),"scroll requires repaint");state.blit();check(state.agentUiState(ui)&&oldRows!=ui.rows&&!frame.matchesFrame(101,1,1,&state,ui),"scroll invalidates prior handles");
 req.action="select_build_facility";req.choice="r0";check(state.agentDispatch(req)&&game.pushes==1,"ordinary selection opens detail");auto *detail=const_cast<Fixture::PlaceFacilityState*>(static_cast<const Fixture::PlaceFacilityState*>(game.top));check(!detail->agentUiState(ui),"detail prepaint refused");detail->blit();check(detail->agentUiState(ui)&&ui.screen=="place_facility","detail paints");visible=YAML::Load(ui.visible);check(visible["cost"].IsScalar()&&visible["time"].IsScalar()&&visible["maintenance"].IsScalar(),"visible detail prices and time");check(ui.actions.size()==1&&ui.offers("cancel_place_facility"),"cancel only no construction");
 req.action="select_build_facility";check(!detail->agentDispatch(req),"wrong-state action refused");req.action="cancel_place_facility";
 Surface cursor(112,16,200,176);cursor.setPalette(palette);cursor.clear(90);game.cursor=&cursor;cursor.blit(&canvas);check(!detail->agentDispatch(req)&&game.pops==0,"real cursor-painted overlap blocks cancel");game.cursor=nullptr;detail->blit();
 detail->_numCost->setText("HIDDEN PRICE");check(!detail->agentUiState(ui),"dirty detail refused");detail->_numCost->setText("$123");detail->blit();check(detail->agentUiState(ui)&&ui.visible.find("$123")!=std::string::npos,"widget text rather than rule cost");
 SDL_FillRect(canvas.getSurface(),nullptr,0);check(!detail->agentDispatch(req)&&game.pops==0,"erased current RGB blocks named cancel");detail->blit();check(detail->agentDispatch(req)&&game.pops==1,"ordinary cancel pops");check(!detail->agentDispatch(req)&&game.pops==1,"no cancel replay");detail->release();delete detail;
 game.top=&state;state.blit();req.action="close_build_facilities";check(state.agentDispatch(req)&&game.pops==2,"ordinary close pops list");state.release();
 struct DerivedBuild:Fixture::BuildFacilitiesState {DerivedBuild(Fixture::Base*b,Fixture::State*s):BuildFacilitiesState(b,s){}};
 DerivedBuild start(&base,&previous);game.top=&start;for(int i=0;i<22;++i)start._window->popup();start.blit();check(!start.agentUiState(ui),"derived startup facility list not admitted");start.release();
 struct DerivedPlace:Fixture::PlaceFacilityState {DerivedPlace(Fixture::Base*b,Fixture::RuleBaseFacility*r):PlaceFacilityState(b,r){}};
 DerivedPlace placement(&base,&game.mod.rules[0]);game.top=&placement;placement.blit();check(!placement.agentUiState(ui),"derived start facility placement not admitted");placement.release();
 }
 check(!SDL_GetVideoSurface(),"no game opened");std::cout<<"PASS "<<checks<<" facilities constructor/provider/dispatch checks; explicit domain seams, no placement\n";
}
