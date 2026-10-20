// SPDX-License-Identifier: GPL-3.0-or-later
// Real offscreen widgets and extracted, unmodified provider/OK method bodies.
// Only Game/strategic followup construction is substituted; no battle is run.
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Interface/Window.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <iostream>
using namespace OpenXcom;
static unsigned checks=0;
static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
namespace DebriefHarness {
using namespace OpenXcom;
struct Followup { std::string kind; explicit Followup(std::string k):kind(k){} virtual ~Followup(){} };
#define FOLLOWUP(N) struct N:Followup { template<class... T> explicit N(T&&...):Followup(#N){} };
FOLLOWUP(MainMenuState) FOLLOWUP(CommendationLateState) FOLLOWUP(CommendationState)
FOLLOWUP(PromotionsState) FOLLOWUP(CannotReequipState) FOLLOWUP(ManageAlienContainmentState)
FOLLOWUP(ErrorMessageState) FOLLOWUP(SellState) FOLLOWUP(SaveGameState)
#undef FOLLOWUP
struct Saved { int months=-1;bool ironman=false;int getMonthsPassed(){return months;}bool isIronman(){return ironman;} };
struct BaseStub {bool over=false;bool storesOverfull(){return over;}std::string getName(){return "base";}};
struct Localized {Localized arg(const std::string&)const{return *this;}};
struct ElementStub {int color=0;};
struct InterfaceStub {ElementStub element;ElementStub*getElement(const char*){return &element;}};
struct ModStub {InterfaceStub interface;InterfaceStub*getInterface(const char*){return &interface;}};
struct GameStub {
 ModStub mod;ModStub*getMod(){return &mod;}
 const void *top=nullptr; Saved saved; unsigned pops=0; std::vector<std::string> followups;
 bool isState(const void*s)const{return s==top;}Saved*getSavedGame(){return &saved;}
 void popState(){++pops;top=nullptr;}
 void setState(Followup*s){followups.push_back("set:"+s->kind);delete s;}
 void pushState(Followup*s){followups.push_back("push:"+s->kind);delete s;}
};
enum {OPT_BATTLESCAPE,OPT_GEOSCAPE,SAVE_IRONMAN,SAVE_AUTO_GEOSCAPE};
class DebriefingState {
public:
 GameStub *_game; Surface *_modal=nullptr,*_window; Text *_txtTitle,*_txtRating;
 TextButton *_btnOk,*_btnStats; std::vector<Surface*> _surfaces;
 std::vector<int> _deadSoldiersCommended,_soldiersCommended,_missingItems;
 bool _destroyBase=false,_promotions=false,_manageContainment=false;BaseStub *_base;
 SDL_Color *_palette=nullptr;
 Localized tr(const char*){return Localized();}
 bool agentUiState(Observer::AgentUiState&)const;
 bool agentDispatch(const std::string&);
 void btnOkClick(Action*);
};
#include "AgentDebriefMethods.generated.inc"
}
int main(){
 Logger::logFile()="/tmp/openxcom-v8c-debrief-widget.log";
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"no video initialization");
 Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(const auto&f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Language lang;Surface canvas(320,200),cover(40,12,16,180);Window window(nullptr,320,200);window.setColor(16);
 Text title(300,17,16,8),rating(200,9,64,180);TextButton ok(40,12,16,180),stats(40,12,264,180);
 title.initText(&big,&small,&lang);title.setBig();title.setText("Aliens defeated");
 rating.initText(&big,&small,&lang);rating.setText("Rating> Good");
 ok.initText(&big,&small,&lang);ok.setText("OK");stats.initText(&big,&small,&lang);stats.setText("Stats");
 TextList list(290,80,16,32);list.initText(&big,&small,&lang);list.setColumns(3,224,30,64);list.setDot(true);list.addRow(3,"Aliens killed","2","100");
 DebriefHarness::GameStub game;DebriefHarness::BaseStub base;DebriefHarness::DebriefingState state;
 state._game=&game;state._base=&base;state._window=&window;state._txtTitle=&title;state._txtRating=&rating;state._btnOk=&ok;state._btnStats=&stats;
 state._surfaces={&window,&ok,&stats,&title,&rating,&list,&cover};game.top=&state;cover.setVisible(false);
 auto draw=[&](){for(auto*s:state._surfaces)s->blit(&canvas);};
 Observer::AgentUiState ui;
 check(!state.agentUiState(ui),"unpainted background refuses provider");draw();
 check(state.agentUiState(ui)&&ui.screen=="debriefing","actual painted debrief provider");
 check(ui.visible.find("\"title\":\"Aliens defeated\"")!=std::string::npos,"complete displayed title exported");
 check(ui.visible.find("\"rating\":\"Rating> Good\"")!=std::string::npos,"complete displayed rating exported");
 check(ui.visible.find("\"stats_label\":\"Stats\"")!=std::string::npos,"visible unsupported Stats label");
 check(ui.visible.find("\"table_status\":\"unavailable\"")!=std::string::npos&&ui.visible.find("Aliens killed")==std::string::npos,"no table row or hidden result data");
 check(ui.actions.size()==1&&ui.offers("close_debriefing")&&ui.actions[0].label=="OK","only ordinary OK admitted");
 check(Observer::agentRegionsDisjoint(ui),"stock exposed footprints disjoint");
 state._modal=&cover;check(!state.agentUiState(ui)&&!state.agentDispatch("close_debriefing"),"modal blocks provider and dispatch");state._modal=nullptr;
 game.top=&cover;check(!state.agentUiState(ui)&&!state.agentDispatch("close_debriefing"),"covered top state blocks dispatch");game.top=&state;
 cover.setVisible(true);draw();check(state.agentUiState(ui)&&!ui.offers("close_debriefing"),"later surface occludes OK");check(!state.agentDispatch("close_debriefing"),"occluded handler never called");cover.setVisible(false);
 Surface fullCover(320,200);SDL_Rect whole={0,0,320,200};fullCover.drawRect(&whole,16);state._surfaces.push_back(&fullCover);draw();check(!state.agentUiState(ui)&&!state.agentDispatch("close_debriefing"),"fully covered layout exposes no debrief state or action");state._surfaces.pop_back();draw();
 ok.setVisible(false);draw();check(state.agentUiState(ui)&&!ui.offers("close_debriefing"),"hidden OK omitted");ok.setVisible(true);draw();
 ok.setText("WAIT");check(state.agentUiState(ui)&&!ui.offers("close_debriefing"),"dirty button omitted");ok.setText("OK");draw();
 ok.setX(-1);draw();check(state.agentUiState(ui)&&!ui.offers("close_debriefing"),"clipped control omitted");ok.setX(16);draw();
 title.setText(std::string(100,'W'));draw();check(state.agentUiState(ui)&&ui.visible.find("\"title\":null")!=std::string::npos,"genuinely clipped title withheld");
 title.setText("Aliens defeated\xf0\x9f\x8c\x8c");draw();check(state.agentUiState(ui)&&ui.visible.find("\"title\":null")!=std::string::npos,"unknown glyph withheld");title.setText("Aliens defeated");draw();
 rating.setVisible(false);stats.setText("Score");draw();check(state.agentUiState(ui)&&ui.visible.find("\"rating\":null")!=std::string::npos&&ui.visible.find("\"stats_label\":\"Score\"")!=std::string::npos,"physical Stats-page labels reflect current widgets only");
 rating.setVisible(true);stats.setText("Stats");draw();
 // Later composite children outside the list root must also block OK.
 list.setHeight(8);list.setY(180);list.setX(16);list.addRow(3,"second","1","50");draw();
 check(state.agentUiState(ui)&&!ui.offers("close_debriefing"),"later TextList composite blocks OK");list.setY(32);list.setHeight(80);draw();
 check(!state.agentDispatch("show_debrief_stats")&&!game.pops,"unsupported Stats cannot dispatch");
 check(state.agentDispatch("close_debriefing")&&game.pops==1,"actual provider invokes actual ordinary handler once");
 check(game.followups==std::vector<std::string>{"set:MainMenuState"},"ordinary skirmish returns MainMenu");
 check(!state.agentDispatch("close_debriefing")&&game.pops==1,"departed state cannot invoke handler twice");
 // Exercise original campaign branching with state constructors recorded only.
 game.top=&state;game.saved.months=0;game.saved.ironman=false;game.followups.clear();state._promotions=true;state._missingItems.push_back(1);state._soldiersCommended.push_back(1);state._deadSoldiersCommended.push_back(1);
 Options::autosave=false;Options::storageLimitsEnforced=false;
 check(state.agentDispatch("close_debriefing"),"campaign ordinary OK branch invoked");
 check(game.followups==std::vector<std::string>{"push:CommendationLateState","push:CommendationState","push:PromotionsState","push:CannotReequipState"},"ordinary followup order preserved");
 game.top=&state;game.followups.clear();game.saved.ironman=true;check(state.agentDispatch("close_debriefing")&&game.followups.back()=="push:SaveGameState","ordinary Ironman save followup preserved, stub records only");
 check(Observer::agentPaintHealthy(),"constituent rendering remained healthy");check(!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"no Game/window/audio/simulation run");
 std::cout<<"AgentDebriefWidgetTest PASS "<<checks<<" checks\n";return 0;
}
