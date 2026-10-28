// SPDX-License-Identifier: GPL-3.0-or-later
// Real offscreen widgets and extracted, unmodified provider/OK method bodies.
// Only Game/strategic followup construction is substituted; no battle is run.
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Interface/Window.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentProtocol.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <iostream>
using namespace OpenXcom;
static unsigned checks=0;
static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
namespace OpenXcom {
#include "GameIdentity.generated.inc"
Game *State::_game = nullptr;
State::State():_screen(true),_modal(nullptr),_ruleInterface(nullptr),_ruleInterfaceParent(nullptr){}
State::~State(){}
void State::init(){} void State::handle(Action*){} void State::blit(){}
void State::resize(int&,int&){} void State::recenter(int,int){}
#include "StateBase.generated.inc"
}
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
struct Localized {std::string text; Localized(const char*s=""):text(s){} Localized arg(const std::string&)const{return *this;} operator std::string()const{return text;}};
struct ElementStub {int color=0;};
struct InterfaceStub {ElementStub element;ElementStub*getElement(const char*){return &element;}};
struct ModStub {InterfaceStub interface;InterfaceStub*getInterface(const char*){return &interface;}};
struct ScreenStub { Surface*surface=nullptr;Surface*getSurface(){return surface;} };
struct GameStub {
 ScreenStub screen;ScreenStub*getScreen(){return &screen;}
 Surface *_cursor=nullptr,*_fpsCounter=nullptr;
 bool agentOverlayClear(const Observer::AgentUiState&)const;
 bool agentNonCursorOverlayClear(const Observer::AgentUiState&)const;
 ModStub mod;ModStub*getMod(){return &mod;}
 const void *top=nullptr; Saved saved; unsigned pops=0; std::vector<std::string> followups;
 bool isState(const void*s)const{return s==top;}Saved*getSavedGame(){return &saved;}
 void popState(){++pops;top=nullptr;}
 void setState(Followup*s){followups.push_back("set:"+s->kind);delete s;}
 void pushState(Followup*s){followups.push_back("push:"+s->kind);delete s;}
};
#include "Overlay.generated.inc"
enum {OPT_BATTLESCAPE,OPT_GEOSCAPE,SAVE_IRONMAN,SAVE_AUTO_GEOSCAPE};
class DebriefingState {
public:
 GameStub *_game; Surface *_modal=nullptr,*_window; Text *_txtTitle,*_txtRating;
 TextList *_lstStats,*_lstRecovery,*_lstTotal;
 TextButton *_btnOk,*_btnStats; std::vector<Surface*> _surfaces;
 std::vector<int> _deadSoldiersCommended,_soldiersCommended,_missingItems;
 bool _destroyBase=false,_promotions=false,_manageContainment=false;BaseStub *_base;
 SDL_Color *_palette=nullptr;
 Localized tr(const char*s){return Localized(std::string(s)=="STR_SCORE"?"Score":std::string(s)=="STR_STATS"?"Stats":s);}
 bool agentUiState(Observer::AgentUiState&)const;
 bool agentDispatch(const std::string&);
 bool agentDispatch(const Observer::AgentRequest&);
 bool agentSoldierStatsUi(Observer::AgentUiState&)const;
 bool _showSoldierStats=false;
 Text *_txtSoldier,*_txtTU,*_txtStamina,*_txtHealth,*_txtBravery,*_txtReactions,*_txtFiring,*_txtThrowing,*_txtMelee,*_txtStrength,*_txtPsiStrength,*_txtPsiSkill,*_txtTooltip;
 TextList *_lstSoldierStats;
 void btnStatsClick(Action*);
 void applyVisibility();
 Text *_txtItem,*_txtQuantity,*_txtScore,*_txtRecovery;
 void btnOkClick(Action*);
};
#include "AgentDebriefMethods.generated.inc"
}
int main(int argc, char **argv){
 check(argc==3,"data directory and game variant supplied");
 FileMap::load("debrief-report-test","./",true);
 Logger::logFile()="/tmp/openxcom-v8c-debrief-widget.log";
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"no video initialization");
 Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(const auto&f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Language lang;lang.loadFile(std::string(argv[1])+"/standard/"+argv[2]+"/Language/en-US.yml");Surface canvas(320,200),cover(40,12,16,180);Window window(nullptr,320,200);window.setColor(16);
 Text title(300,17,16,8),rating(200,9,64,180);TextButton ok(40,12,16,180),stats(40,12,264,180);
 title.initText(&big,&small,&lang);title.setBig();title.setText("Aliens defeated");
 rating.initText(&big,&small,&lang);rating.setText("Rating> Good");
 ok.initText(&big,&small,&lang);ok.setText("OK");stats.initText(&big,&small,&lang);stats.setText("Stats");
 TextList list(290,80,16,32);list.initText(&big,&small,&lang);list.setColumns(3,224,30,64);list.setDot(true);list.addRow(3,"Aliens killed","2","100");
 TextList recovery(290,16,16,120),total(290,9,16,146);
 recovery.initText(&big,&small,&lang);recovery.setColumns(3,224,30,64);recovery.setDot(true);recovery.addRow(3,"Alien corpses","5","25");
 total.initText(&big,&small,&lang);total.setColumns(2,254,64);total.setDot(true);total.addRow(2,"TOTAL","141");
 list.agentEnableReportCells(3);recovery.agentEnableReportCells(3);total.agentEnableReportCells(2);
 DebriefHarness::GameStub game;game.screen.surface=&canvas;DebriefHarness::BaseStub base;DebriefHarness::DebriefingState state;
 state._game=&game;state._base=&base;state._window=&window;state._txtTitle=&title;state._txtRating=&rating;state._btnOk=&ok;state._btnStats=&stats;
 Text scoreHeading(1,1,310,160);state._txtItem=&scoreHeading;state._txtQuantity=&scoreHeading;state._txtScore=&scoreHeading;state._txtRecovery=&scoreHeading;
 state._lstStats=&list;state._lstRecovery=&recovery;state._lstTotal=&total;
 std::vector<Text*> headings;
 for(unsigned i=0;i<12;++i){Text*w=new Text(i?18:90,9,i?106+(i-1)*18:16,24);w->initText(&big,&small,&lang);w->setText(i?"TU":"Soldier");w->setVisible(false);headings.push_back(w);}
 state._txtSoldier=headings[0];state._txtTU=headings[1];state._txtStamina=headings[2];state._txtHealth=headings[3];state._txtBravery=headings[4];state._txtReactions=headings[5];state._txtFiring=headings[6];state._txtThrowing=headings[7];state._txtMelee=headings[8];state._txtStrength=headings[9];state._txtPsiStrength=headings[10];state._txtPsiSkill=headings[11];
 Text tooltip(200,9,64,180);tooltip.initText(&big,&small,&lang);tooltip.setVisible(false);state._txtTooltip=&tooltip;
 TextList soldiers(288,128,16,32);soldiers.initText(&big,&small,&lang);soldiers.setColumns(13,90,18,18,18,18,18,18,18,18,18,18,18,0);soldiers.setAlign(ALIGN_CENTER);soldiers.setAlign(ALIGN_LEFT,0);soldiers.setDot(true);soldiers.agentEnableSoldierStatsCells();
 for(unsigned i=0;i<20;++i)soldiers.addRow(13,"Jane","1","2","3","4","5","6","7","8","9","10","11","");
 soldiers.setVisible(false);state._lstSoldierStats=&soldiers;

 state._surfaces={&window,&ok,&stats,&title,&rating,&list,&recovery,&total,&cover};game.top=&state;cover.setVisible(false);
 for(Text*w:headings)state._surfaces.push_back(w);state._surfaces.push_back(&soldiers);state._surfaces.push_back(&tooltip);
 for(Surface*w:state._surfaces)w->agentTrackPresentation();
 auto draw=[&](){for(auto*s:state._surfaces)s->blit(&canvas);};
 Observer::AgentUiState ui;
 check(!state.agentUiState(ui),"unpainted background refuses provider");draw();
 check(state.agentUiState(ui)&&ui.screen=="debriefing","actual painted debrief provider");
 check(ui.visible.find("\"title\":\"Aliens defeated\"")!=std::string::npos,"complete displayed title exported");
 check(ui.visible.find("\"rating\":\"Rating> Good\"")!=std::string::npos,"complete displayed rating exported");
 check(ui.visible.find("\"stats_label\":\"Stats\"")!=std::string::npos,"visible unsupported Stats label");
 check(ui.visible.find("\"table_status\":\"shown\"")!=std::string::npos&&ui.visible.find("Aliens killed")!=std::string::npos,"current dotted report projected");
 auto report=YAML::Load(ui.visible);check(report["tables"]["stats"]["rows"][0]["cells"].size()==3&&report["tables"]["total"]["rows"][0]["cells"].size()==2,"table column shapes exact");
 check(report["tables"]["total"]["rows"][0]["cells"][1].as<std::string>().find("141")!=std::string::npos,"current displayed total without recomputation");
 // Stock no-recovery layout leaves an empty transparent recovery allocation at y=32.
 recovery.clearList();recovery.setHeight(80);recovery.setY(32);draw();
 check(state.agentUiState(ui),"no-recovery stock provider");

 check(YAML::Load(ui.visible)["tables"]["stats"]["rows"].size()==1,"empty recovery does not obscure score row");
 const auto emptyBinding=ui;
 Surface stockCover(290,8,16,32);state._surfaces.push_back(&stockCover);draw();
 check(state.agentUiState(ui)&&YAML::Load(ui.visible)["tables"]["stats"]["rows"].size()==0,"empty-root exemption never bypasses real later cover");
 state._surfaces.pop_back();draw();
 recovery.addRow(3,"New material","1","1");
 check(state.agentUiState(ui)&&YAML::Load(ui.visible)["tables"]["stats"]["rows"].size()==0&&!ui.sameBinding(emptyBinding),"dirty/nonempty recovery loses exemption immediately");
 recovery.clearList();draw();
 list.clearList();
 const char *keys[]={"STR_ALIENS_KILLED","STR_ALIEN_CORPSES_RECOVERED","STR_ALIEN_ARTIFACTS_RECOVERED","STR_CIVILIANS_KILLED_BY_ALIENS","STR_CIVILIANS_KILLED_BY_XCOM_OPERATIVES","STR_CIVILIANS_SAVED","STR_XCOM_OPERATIVES_KILLED"};
 for(const char *key:keys)list.addRow(3,lang.getString(key).c_str(),"\0019\001","\001-30");
 draw();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["tables"]["stats"]["rows"].size()==7,"both-game native translated seven-row stock no-recovery layout");
 for(const auto &row:YAML::Load(ui.visible)["tables"]["stats"]["rows"])
   check(row["cells"].size()==3&&!row["cells"][0].IsNull()&&!row["cells"][1].IsNull()&&!row["cells"][2].IsNull(),"all translated stock cells present");
 list.clearList();list.addRow(3,"Aliens killed","2","100");

 recovery.addRow(3,"Alien corpses","5","25");recovery.setHeight(16);recovery.setY(120);draw();
 const auto first=ui;list.setCellText(0,1,"3");check(state.agentUiState(ui)&&ui.visible.find("\"table_status\":\"partial\"")!=std::string::npos,"dirty table withheld independently");draw();check(state.agentUiState(ui)&&!ui.sameBinding(first),"redrawn table changes private binding");
 list.setVisible(false);recovery.setVisible(false);total.setVisible(false);draw();check(state.agentUiState(ui)&&ui.visible.find("Aliens killed")==std::string::npos&&ui.visible.find("\"table_status\":\"unavailable\"")!=std::string::npos,"hidden report page never projects rows");list.setVisible(true);recovery.setVisible(true);total.setVisible(true);draw();
 Surface cellCover(224,8,16,32);state._surfaces.push_back(&cellCover);draw();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["tables"]["stats"]["rows"][0]["cells"][0].IsNull(),"later real sibling withholds covered cell");state._surfaces.pop_back();draw();
 Surface rowCover(290,8,16,32);state._surfaces.push_back(&rowCover);draw();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["tables"]["stats"]["rows"].size()==0,"fully occluded row count not disclosed");state._surfaces.pop_back();draw();
 game._fpsCounter=&cellCover;check(state.agentUiState(ui)&&YAML::Load(ui.visible)["tables"]["stats"]["rows"][0]["cells"][0].IsNull(),"real global overlay withholds covered cell");game._fpsCounter=nullptr;game._cursor=&cellCover;check(state.agentUiState(ui)&&!YAML::Load(ui.visible)["tables"]["stats"]["rows"][0]["cells"][0].IsNull(),"ordinary cursor does not hide current report");game._cursor=nullptr;
 check(ui.actions.size()==2&&ui.offers("close_debriefing")&&ui.offers("toggle_debriefing_stats"),"ordinary OK and STATS admitted");
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
 check(state.agentUiState(ui)&&ui.offers("toggle_debriefing_stats"),"visible STATS action");
 check(state.agentDispatch("toggle_debriefing_stats")&&state._showSoldierStats&&!game.pops,"ordinary STATS callback no pop");draw();
 bool statsReady=state.agentUiState(ui); if(!statsReady)std::cerr<<"statsdebug "<<ui.payload()<<" healthy="<<Observer::agentPaintHealthy()<<" window="<<window.agentPresentationPaintedTo(&canvas)<<" visible="<<Observer::agentWidgetVisible(&window)<<"\n";
 check(statsReady&&ui.screen=="debriefing_soldier_stats","stats page provider");
 auto statsReport=YAML::Load(ui.visible);
 check(statsReport["headings"].size()==12,"twelve headings no dummy");
 check(statsReport["table"]["status"].as<std::string>()=="shown","stats table capture shown");
 check(statsReport["table"]["rows"].size()==16,"only current sixteen rows");
 check(statsReport["table"]["rows"][0]["cells"].size()==12,"twelve cells no dummy");
 check(statsReport["table"]["rows"][0]["cells"][0].as<std::string>().find("Jane")!=std::string::npos,"displayed native name");
 state._surfaces.push_back(&fullCover);draw();check(!state.agentUiState(ui),"full real cover blocks stats page");state._surfaces.pop_back();draw();
 check(state.agentUiState(ui),"stats recovered after cover removal");
 const auto statsBinding=ui;soldiers.setCellText(1,1,"7");check(state.agentUiState(ui)&&!ui.sameBinding(statsBinding),"stats content mutation invalidates binding");draw();
 soldiers.setColumns(13,90,18,18,18,18,18,18,18,18,18,18,18,1);draw();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["table"]["status"].as<std::string>()=="unavailable","nonzero thirteenth column rejected");
 soldiers.setColumns(13,90,18,18,18,18,18,18,18,18,18,18,18,0);draw();check(state.agentUiState(ui),"dummy column restored");
 Surface arrowCover(12,14,308,32);SDL_Rect arrowRect={0,0,12,14};arrowCover.drawRect(&arrowRect,16);state._surfaces.push_back(&arrowCover);draw();
 check(state.agentUiState(ui)&&!YAML::Load(ui.visible)["scroll"]["up"].as<bool>()&&YAML::Load(ui.visible)["scroll"]["down"].as<bool>(),"real cover blocks only covered visible arrow strip");state._surfaces.pop_back();draw();check(state.agentUiState(ui),"fresh uncovered arrow strip");
 Observer::AgentRequest request;request.action="scroll_visible_list";request.choice="down";
 check(ui.offers(request.action)&&state.agentDispatch(request),"ordinary visible downward scroll");draw();
 check(state.agentUiState(ui)&&YAML::Load(ui.visible)["scroll"]["up"].as<bool>(),"scroll fresh upward control");
 soldiers.setCellText(1,12,"hidden");draw();
 check(state.agentUiState(ui)&&YAML::Load(ui.visible)["table"]["status"].as<std::string>()=="unavailable","nonempty dummy rejected not disclosed");
 soldiers.setCellText(1,12,"");draw();
 check(state.agentDispatch("toggle_debriefing_stats")&&!state._showSoldierStats&&!game.pops,"normal SCORE callback no pop");draw();
 check(state.agentUiState(ui)&&ui.screen=="debriefing","score view restored");
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
 std::cout<<"AgentDebriefReportTest PASS "<<checks<<" checks\n";return 0;
}
