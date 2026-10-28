// SPDX-License-Identifier: GPL-3.0-or-later
// Literal directory constructor/provider/dispatch/callback and availability-filter bodies.
// Actual SDL widgets; explicit Game/Mod/research/article/render-loop seams.
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


namespace Fixture {
struct ArticleDefinition {std::string id,title,section;std::vector<std::string> _requires;};
using ArticleDefinitionList=std::vector<ArticleDefinition*>;
static const std::string UFOPAEDIA_NOT_AVAILABLE="STR_NOT_AVAILABLE";
struct SavedGame {std::set<std::string>known;bool isResearched(const std::vector<std::string>&v)const{for(const auto&s:v)if(!known.count(s))return false;return true;}};
struct Mod {Surface*background;std::vector<std::string>cats,articles;std::map<std::string,ArticleDefinition*>defs;Surface*getSurface(const std::string&){return background;}const std::vector<std::string>&getUfopaediaCategoryList()const{return cats;}const std::vector<std::string>&getUfopaediaList()const{return articles;}ArticleDefinition*getUfopaediaArticle(const std::string&id){return defs.at(id);}};
struct Screen {Surface*surface;Surface*getSurface(){return surface;}};
struct Game {mutable Screen screen;Mod mod;SavedGame save;Language*language;Font*big,*small;SDL_Color*palette;const void*top=nullptr;bool clear=true;int pops=0,pushes=0,opens=0;std::string section,opened;Screen*getScreen()const{return &screen;}Mod*getMod(){return &mod;}SavedGame*getSavedGame(){return &save;}Language*getLanguage(){return language;}bool isState(const void*s)const{return top==s;}bool agentOverlayClear(const AgentUiState&)const{return clear;}bool agentNonCursorOverlayClear(const AgentUiState&)const{return clear;}void popState(){++pops;top=nullptr;}template<class T>void pushState(T*p){++pushes;section=p->_section;p->release();delete p;top=nullptr;}};
struct Ufopaedia {static bool isArticleAvailable(SavedGame*,ArticleDefinition*);static ArticleDefinitionList getAvailableArticles(SavedGame*,Mod*);static void list(SavedGame*,Mod*,const std::string&,ArticleDefinitionList&);static void openArticle(Game*g,ArticleDefinition*a){++g->opens;g->opened=a->id;g->top=nullptr;}};
#include "UfopaediaFilter.generated.inc"
static Game*currentGame;
struct State:OpenXcom::State {Game*_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;void setInterface(const std::string&){}void centerAllSurfaces(){}LocalizedText tr(const std::string&s)const{return _game->language->getString(s);}void add(Surface*s,const std::string&,const std::string&){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);}void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}};
#include "UfopaediaSelectClass.generated.inc"
#include "UfopaediaStartClass.generated.inc"
#include "UfopaediaSelect.generated.inc"
const size_t UfopaediaStartState::CAT_MIN_BUTTONS=9;
const size_t UfopaediaStartState::CAT_MAX_BUTTONS=10;
#include "UfopaediaStart.generated.inc"
}
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==3,"data and log args");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"}){
 Language language;language.loadFile(std::string(argv[1])+"/standard/"+master+"/Language/en-US.yml");Fixture::Game g;g.screen.surface=&canvas;g.mod.background=&background;g.language=&language;g.big=&big;g.small=&small;g.palette=palette;Fixture::currentGame=&g;
 {TextList list(206,80,50,50);list.setPalette(palette);list.initText(&big,&small,&language);list.setColumns(1,206);list.setSelectable(true);list.setBackground(&background);const std::string longLabel(300,'W');list.addRow(1,longLabel.c_str());list.agentEnableOutfitting(1);check(list._agentOutfittingColumns==0,"generic one-column opt-in still rejected");list.agentEnableFacilityCells();list.blit(&canvas);std::vector<AgentOutfittingRow> rows;check(!list._agentOutfittingPaintedSingleColumn&&list.agentFacilityRows(&canvas,rows)&&!rows.empty()&&rows[0].cells[0].text==longLabel,"existing facility logical one-column unchanged");list.agentEnablePaintedSingleColumn();list.blit(&canvas);check(list._agentOutfittingPaintedSingleColumn&&list.agentOutfittingRows(rows)&&!rows.empty()&&!rows[0].cells[0].known,"directory strict one-column hides clipped text");list.agentEnableFacilityCells();list.blit(&canvas);check(!list._agentOutfittingPaintedSingleColumn&&list.agentFacilityRows(&canvas,rows)&&!rows.empty()&&rows[0].cells[0].text==longLabel,"facility opt-in restores its separate semantics");}
 std::vector<Fixture::ArticleDefinition> defs(24);for(int i=0;i<24;++i){auto&a=defs[i];a.id="A"+std::to_string(i);a.title="KNOWN"+std::to_string(i);a.section="SECTION";g.mod.articles.push_back(a.id);g.mod.defs[a.id]=&a;}
 defs[20]._requires={"PRIVATE_UNRESEARCHED"};defs[20].title="PRIVATE_HIDDEN_SENTINEL";defs[21].section=Fixture::UFOPAEDIA_NOT_AVAILABLE;defs[21].title="PRIVATE_UNAVAILABLE_SENTINEL";defs[22]._requires={"KNOWN_TECH"};defs[23].section="OTHER";g.save.known.insert("KNOWN_TECH");
 Fixture::ArticleDefinitionList filtered;Fixture::Ufopaedia::list(&g.save,&g.mod,"SECTION",filtered);check(filtered.size()==21,"actual research/section/unavailable filter");check(std::find(filtered.begin(),filtered.end(),&defs[20])==filtered.end()&&std::find(filtered.begin(),filtered.end(),&defs[21])==filtered.end()&&std::find(filtered.begin(),filtered.end(),&defs[22])!=filtered.end(),"hidden excluded researched included");
 for(int count:{0,1,9,10,12}){
 g.mod.cats.clear();for(int i=0;i<count;++i)g.mod.cats.push_back("CATEGORY"+std::to_string(i));g.pops=g.pushes=0;
 Fixture::UfopaediaStartState st;g.top=&st;AgentUiState ui;AgentRequest req;check(!st.agentUiState(ui),"categories unpainted denied");st.blit();check(st.agentUiState(ui),"categories painted");auto before=ui;auto v=YAML::Load(ui.visible);check(st._btnSections.size()==size_t(std::min(count,10)),"actual category button bound");
 g.top=nullptr;check(!st.agentUiState(ui),"category non-top denied");g.top=&st;st._modal=st._btnOk;check(!st.agentUiState(ui),"category modal denied");st._modal=nullptr;g.clear=false;check(!st.agentUiState(ui),"category overlay denied");g.clear=true;check(st.agentUiState(ui),"category fresh after probes");
 if(count>10){req.action="scroll_visible_list";req.choice="down";check(st.agentDispatch(req),"category ordinary down");check(st._offset==1,"category native offset");check(!st.agentUiState(ui),"category scroll needs repaint");st.blit();check(st.agentUiState(ui)&&!ui.sameBinding(before),"category rebound after scroll");req.choice="up";check(st.agentDispatch(req)&&st._offset==0,"category native up");st.blit();}
 req.action="close_ufopaedia";check(st.agentDispatch(req)&&g.pops==1,"category native close");check(!st.agentDispatch(req)&&g.pops==1,"category repeat denied");st.release();
 }
 for(bool empty:{false,true}){
 g.pops=g.opens=0;Fixture::UfopaediaSelectState st(empty?"EMPTY":"SECTION");g.top=&st;AgentUiState ui;AgentRequest req;check(!st.agentUiState(ui),"articles unpainted denied");st.blit();check(st.agentUiState(ui),"articles painted");check(ui.visible.find("PRIVATE_")==std::string::npos&&ui.visible.find("_requires")==std::string::npos,"no hidden articles or requirements in payload");
 g.top=nullptr;check(!st.agentUiState(ui),"article non-top denied");g.top=&st;st._modal=st._btnOk;check(!st.agentUiState(ui),"article modal denied");st._modal=nullptr;g.clear=false;check(!st.agentUiState(ui),"article overlay denied");g.clear=true;
 if(!empty){const std::string longTitle(300,'W');st._lstSelection->setCellText(0,0,longTitle);check(!st.agentUiState(ui),"unpainted article text stale");st.blit();check(st.agentUiState(ui)&&ui.visible.find(longTitle)==std::string::npos,"clipped full article title not leaked");st._lstSelection->setCellText(0,0,"KNOWN0");st.blit();}
 if(!empty){g.save.known.erase("KNOWN_TECH");check(!st.agentUiState(ui),"research availability drift invalidates cached article list");g.save.known.insert("KNOWN_TECH");}
 check(st.agentUiState(ui),"article fresh after probes");req.action="select_ufopaedia_article";req.choice="r99";check(!st.agentDispatch(req)&&g.opens==0,"unknown article handle no callback");
 if(empty){check(ui.rows.empty(),"empty category no fabricated rows");req.action="close_ufopaedia_select";check(st.agentDispatch(req)&&g.pops==1,"empty native close");}
 else {check(!ui.rows.empty()&&ui.rows.size()<filtered.size(),"only viewport rows");auto before=ui;req.action="scroll_visible_list";req.choice="down";check(st.agentDispatch(req),"article native scroll");check(!st.agentUiState(ui),"article scroll needs repaint");st.blit();check(st.agentUiState(ui)&&!ui.sameBinding(before)&&ui.rows.front()>0,"logical viewport advanced");auto*expected=st._article_list.at(ui.rows.front());req.action="select_ufopaedia_article";req.choice="r0";check(st.agentDispatch(req)&&g.opens==1&&g.opened==expected->id,"scrolled handle native article callback");check(!st.agentDispatch(req)&&g.opens==1,"article no replay");g.top=&st;st.blit();check(st.agentUiState(ui),"article destination return repaints original directory");auto returned=YAML::Load(ui.visible);check(returned["rows"].size()>0&&returned["rows"][0]["selected"].as<bool>(),"returned native selected article row remains selected true");req.action="close_ufopaedia_select";check(st.agentDispatch(req)&&g.pops==1,"selected directory native close after return");}
 st.release();
 }
 // Actual category callback must use clicked sender and current offset, not guessed category ID.
 g.mod.cats={"SECTION","OTHER"};g.pushes=0;Fixture::UfopaediaStartState st;g.top=&st;st.blit();AgentUiState ui;check(st.agentUiState(ui),"category choice initial");AgentRequest req;req.action="select_ufopaedia_category";req.choice="r1";check(st.agentDispatch(req)&&g.pushes==1&&g.section=="OTHER","category native sender routes chosen section");st.release();
 }
 check(!SDL_GetVideoSurface(),"no game window");std::cout<<"PASS "<<checks<<" UFOPAEDIA directory constructor/provider/callback/filter checks; explicit domain seams\n";
}
