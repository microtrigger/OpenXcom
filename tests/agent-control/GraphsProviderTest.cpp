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
#include "../../src/Interface/ToggleTextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Window.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
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
struct Element{int color2=80;};struct Interface{Element e;Element*getElement(const std::string&){return &e;}};
struct Mod {Surface*background;Interface iface;int music=0;Surface*getSurface(const std::string&){return background;}int getDefeatScore(){return -1000;}long getDefeatFunds(){return -1000000;}Interface*getInterface(const std::string&){return &iface;}void playMusic(const std::string&){++music;}};
struct Diary{int service=0;bool medal=false;void addMonthlyService(){++service;}bool manageCommendations(Mod*,void*){return medal;}};
struct Soldier{int id=1;Diary diary;int getId(){return id;}Diary*getDiary(){return &diary;}};
struct Base{std::vector<Soldier*> soldiers;std::vector<Soldier*>*getSoldiers(){return &soldiers;}};
struct Time{int getMonth(){return 2;}int getYear(){return 1999;}};
enum{END_NONE=0,END_LOSE=2,OPT_GEOSCAPE=1,SAVE_IRONMAN=2,SAVE_AUTO_GEOSCAPE=3};
struct SavedGame{Time time;bool warned=false,ironman=false;int ending=0;std::vector<Base*>bases;
 int getEnding(){return ending;}Time*getTime(){return &time;}int getDifficultyCoefficient(){return 0;}long getCountryFunding(){return 1000;}long getBaseMaintenance(){return 100;}long getFunds(){return 2000;}bool getWarned(){return warned;}void setWarned(bool b){warned=b;}void setEnding(int e){ending=e;}bool isIronman(){return ironman;}
 std::vector<Base*>*getBases(){return &bases;}Soldier*getSoldier(int id){for(auto*b:bases)for(auto*s:b->soldiers)if(s->id==id)return s;std::abort();}void*getMissionStatistics(){return nullptr;}
};
struct Queued{std::string kind;explicit Queued(std::string s):kind(s){}virtual~Queued(){}};
struct CommendationState:Queued{explicit CommendationState(const std::vector<Soldier*>&):Queued("commendation") {}};
struct PsiTrainingState:Queued{PsiTrainingState():Queued("psi") {}};
struct SaveGameState:Queued{SaveGameState(int,int mode,SDL_Color*):Queued(mode==SAVE_IRONMAN?"ironman":"autosave") {}};
struct CutsceneState:Queued{enum{LOSE_GAME};explicit CutsceneState(int):Queued("lose") {}};
struct GoToMainMenuState:Queued{GoToMainMenuState():Queued("main_menu") {}};
struct Game {
 Screen screen;Mod mod;SavedGame save;Language*language;Font *big,*small;SDL_Color*palette;const void*top=nullptr;bool clear=true;int pops=0;std::vector<std::string> events;
 int cleared=0;void setSavedGame(int value){if(value==0){++cleared;events.push_back("clear_save");}}void setState(Queued*q){top=nullptr;events.push_back(q->kind);delete q;}
 Screen*getScreen(){return &screen;}Mod*getMod(){return &mod;}SavedGame*getSavedGame(){return &save;}Language*getLanguage(){return language;}bool isState(const void*s)const{return top==s;}
 bool cursorVisible=false;SDL_Rect cursor={0,0,0,0};
 bool agentOverlayClear(const AgentUiState&)const{return clear;} // Ordinary cursor is not an overlay; clear models actual covering UI.
 bool agentNonCursorOverlayClear(const AgentUiState&)const{return clear;}
 void popState(){++pops;top=nullptr;events.push_back("pop");}void pushState(Queued*q){events.push_back(q->kind);top=nullptr;delete q;}
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
struct Globe{};

struct GraphButInfo{LocalizedText _name;int _color;bool _pushed=false;GraphButInfo(std::string n,int c):_name(n),_color(c){}};
struct GraphsState:State {
	InteractiveSurface *_bg;
	InteractiveSurface *_btnGeoscape;
	InteractiveSurface *_btnXcomCountry, *_btnUfoCountry;
	InteractiveSurface *_btnXcomRegion, *_btnUfoRegion;
	InteractiveSurface *_btnIncome, *_btnFinance;
	Text *_txtTitle, *_txtFactor;
	TextList *_txtMonths, *_txtYears;
	std::vector<Text *> _txtScale;
	std::vector<ToggleTextButton *> _btnRegions, _btnCountries, _btnFinances;
	std::vector<GraphButInfo *>  _regionToggles, _countryToggles;
	std::vector<bool> _financeToggles;
	ToggleTextButton *_btnRegionTotal, *_btnCountryTotal;
	std::vector<Surface *> _alienRegionLines, _alienCountryLines;
	std::vector<Surface *> _xcomRegionLines, _xcomCountryLines;
	std::vector<Surface *> _financeLines, _incomeLines;
	bool _alien, _income, _country, _finance;
	static const size_t GRAPH_MAX_BUTTONS=16;
	//will be only between 0 and size()
	size_t _butRegionsOffset, _butCountriesOffset;
	//scroll and repaint buttons functions
	void scrollButtons(std::vector<GraphButInfo *> &toggles, std::vector<ToggleTextButton *> &buttons, size_t &offset, int step);
	void updateButton(GraphButInfo *from,ToggleTextButton *to);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentGraphPaint = false;
    std::vector<std::uint64_t> _agentGraphContext;
    std::vector<Surface *> agentLineSurfaces() const;
    std::vector<ToggleTextButton *> agentLegend() const;
#endif
 GraphsState(){
 _bg=new InteractiveSurface(320,200,0,0);add(_bg);_bg->clear(40);
 InteractiveSurface **icons[]={&_btnUfoRegion,&_btnUfoCountry,&_btnXcomRegion,&_btnXcomCountry,&_btnIncome,&_btnFinance,&_btnGeoscape};
 for(int i=0;i<7;++i){*icons[i]=new InteractiveSurface(32,24,96+32*i,0);add(*icons[i]);(*icons[i])->clear(80);}
 _txtTitle=new Text(230,16,90,28);_txtFactor=new Text(38,11,96,28);_txtFactor->setText("1000");add(_txtTitle);add(_txtFactor);
 _txtMonths=new TextList(205,8,115,183);_txtYears=new TextList(200,8,121,191);_txtMonths->agentEnableGraphAxis();_txtYears->agentEnableGraphAxis();add(_txtMonths);add(_txtYears);
 _txtMonths->setColumns(12,17,17,17,17,17,17,17,17,17,17,17,17);_txtMonths->addRow(12,"Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec","Jan");
 _txtYears->setColumns(6,34,34,34,34,34,34);_txtYears->addRow(6,"1998"," "," "," "," ","1999");
 for(int i=0;i<10;++i){auto*t=new Text(42,16,80,171-i*14);t->setAlign(ALIGN_RIGHT);add(t);_txtScale.push_back(t);}
 for(int i=0;i<19;++i){_regionToggles.push_back(new GraphButInfo("Region"+std::to_string(i),17+i));_countryToggles.push_back(new GraphButInfo("Country"+std::to_string(i),37+i));}
 _butRegionsOffset=_butCountriesOffset=0;
 for(int i=0;i<16;++i){auto*r=new ToggleTextButton(88,11,0,i*11);r->setText("Region"+std::to_string(i));r->onMousePress((ActionHandler)&GraphsState::btnRegionListClick);add(r);_btnRegions.push_back(r);auto*c=new ToggleTextButton(88,11,0,i*11);c->setText("Country"+std::to_string(i));c->onMousePress((ActionHandler)&GraphsState::btnCountryListClick);add(c);_btnCountries.push_back(c);}
 _btnRegionTotal=new ToggleTextButton(88,11,0,176);_btnCountryTotal=new ToggleTextButton(88,11,0,176);for(auto*b:{_btnRegionTotal,_btnCountryTotal}){b->setText("TOTAL");add(b);}
 _btnRegionTotal->onMousePress((ActionHandler)&GraphsState::btnRegionListClick);_btnCountryTotal->onMousePress((ActionHandler)&GraphsState::btnCountryListClick);
 for(int i=0;i<5;++i){auto*b=new ToggleTextButton(88,11,0,i*11);b->setText("Finance"+std::to_string(i));b->onMousePress((ActionHandler)&GraphsState::btnFinanceListClick);add(b);_btnFinances.push_back(b);_financeToggles.push_back(false);}
 for(auto*g:{&_alienRegionLines,&_alienCountryLines,&_xcomRegionLines,&_xcomCountryLines,&_incomeLines,&_financeLines})for(int i=0;i<19;++i){auto*l=new Surface(320,200);add(l);g->push_back(l);}
 btnUfoRegionClick(nullptr);updateScale(-90,90);
 }
 void drawLines(){auto &lines=_finance?_financeLines:_income?_incomeLines:_country?(_alien?_alienCountryLines:_xcomCountryLines):(_alien?_alienRegionLines:_xcomRegionLines);for(size_t i=0;i<lines.size();++i){lines[i]->clear();lines[i]->drawLine(125,80+i,312,80+i,60+i);bool on=_finance?(i<_financeToggles.size()&&_financeToggles[i]):(_country?_countryToggles:_regionToggles)[i]->_pushed;lines[i]->setVisible(on);}}
 void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void btnGeoscapeClick(Action*);void btnUfoRegionClick(Action*);void btnUfoCountryClick(Action*);void btnXcomRegionClick(Action*);void btnXcomCountryClick(Action*);void btnIncomeClick(Action*);void btnFinanceClick(Action*);void btnRegionListClick(Action*);void btnCountryListClick(Action*);void btnFinanceListClick(Action*);void shiftButtons(Action*);void resetScreen();void updateScale(double,double);
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();for(auto*t:_regionToggles)delete t;for(auto*t:_countryToggles)delete t;}
};
#include "Graphs.generated.inc"
}
static unsigned checks=0;static void check(bool ok,const char *message){++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==3,"source and master arguments");check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);

 for(const std::string master:{"xcom1","xcom2"}){
 if(std::string(argv[2])!="both"&&std::string(argv[2])!=master)continue;
 Language language;language.loadFile(std::string(argv[1])+"/standard/"+master+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
 Fixture::GraphsState state;game.top=&state;AgentUiState ui;AgentRequest r;
 check(!state.agentUiState(ui),"prepaint refused");state.blit();check(state.agentUiState(ui),"painted graph admitted");


 auto axis=YAML::Load(ui.visible);check(small.getHeight()==9&&small.getSpacing()==-1,"actualfont9spacingminus1");
 check(axis["months"].size()==12&&axis["years"].size()==6,"actualheight8axesallcellslots");check(axis["months"][0]["label"].as<std::string>()=="Feb"&&!axis["months"][0]["clipped"].as<bool>(),"completeFeb");check(axis["months"][2]["label"].as<std::string>()=="Apr"&&axis["months"][2]["clipped"].as<bool>(),"nativeclippeddescenderAprlabelmarked");check(axis["years"][0]["label"].as<std::string>()=="1998"&&axis["years"][5]["label"].as<std::string>()=="1999","paintedyearsdespiteblankallocationoverhang");check(axis["years"][1]["label"].IsNull(),"unpaintedblankyearnull");
 state._txtMonths->setCellText(0,0,"Jan");check(state._txtMonths->agentGraphAxisJson(&canvas,state._surfaces)=="[]","axisstaletextrejected");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["months"][0]["label"].as<std::string>()=="Jan","axisfreshrepaint");
 Surface axisCover(17,8,115,183);axisCover.setPalette(palette);axisCover.clear(30);state._surfaces.push_back(&axisCover);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["months"][0]["label"].IsNull(),"fullycoveredaxislabelnull");state._surfaces.pop_back();state.blit();
 TextList generic(200,8,121,191);generic.setPalette(palette);generic.initText(&big,&small,&language);generic.setColumns(6,34,34,34,34,34,34);generic.addRow(6,"1998"," "," "," "," ","1999");generic.blit(&canvas);AgentUiState gu;AgentListProjection gp;std::vector<Surface*>go{&generic};agentProjectList(&generic,&generic,go,6,false,gu,gp);check(gp.rows=="[]","genericallocationoverhangstillrefused");check(generic.agentGraphAxisJson(&canvas,go)=="[]","axisoptinrequired");state.blit();
 for(const std::string mode:{"alien_region","alien_country","xcom_region","xcom_country","income","finance"}){
 r.action="graph_"+mode;check(state.agentDispatch(r),"native mode callback");state.blit();check(state.agentUiState(ui),"repaint after mode");auto v=YAML::Load(ui.visible);check(v["mode"].as<std::string>()==mode,"mode matches");check(v["legend"].size()>0,"native legend visible");
 bool before=v["legend"][0]["pressed"].as<bool>();r.action="toggle_graph_series";r.choice="r0";check(state.agentDispatch(r),"native button toggle");state.blit();check(state.agentUiState(ui),"after toggle");v=YAML::Load(ui.visible);check(v["legend"][0]["pressed"].as<bool>()!=before,"native pressed state");if(!before)check(v["plot"]["segments"].size()>0,"painted selected line");
 r.choice="r99";check(!state.agentDispatch(r),"invalid row denied");
 }
 r.action="graph_alien_country";r.choice="";check(state.agentDispatch(r),"country mode");state.blit();r.action="scroll_graph_legend";r.choice="down";check(state.agentDispatch(r),"native scroll");state.blit();check(state.agentUiState(ui)&&state._butCountriesOffset==1,"new offset bound");
 game.clear=false;check(!state.agentUiState(ui),"real overlay denied");game.clear=true;state._modal=state._bg;check(!state.agentUiState(ui),"modal denied");state._modal=nullptr;
 state.updateScale(-9000,9000);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["scale"].size()==10,"native scale labels");

 state._btnGeoscape->setVisible(false);state.blit();r.action="close_graphs";check(!state.agentDispatch(r),"hidden mode icon denied");state._btnGeoscape->setVisible(true);state.blit();
 Surface cover(32,24,288,0);cover.setPalette(palette);cover.clear(30);state._surfaces.push_back(&cover);state.blit();check(!state.agentDispatch(r),"later icon cover denied");state._surfaces.pop_back();state.blit();
 auto oldOffset=state._butCountriesOffset;state._butCountriesOffset++;check(!state.agentUiState(ui),"unpainted legend context denied");state._butCountriesOffset=oldOffset;state.blit();
 SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"partial canvas rejected");SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
 state._countryToggles[0]->_pushed=false;state.drawLines();state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["plot"]["segments"].size()==0,"hidden line not exported");
 r.action="close_graphs";r.choice="";check(state.agentDispatch(r)&&game.pops==1,"native close");check(!state.agentDispatch(r),"wrong top after close");state.release();
 }
 std::cout<<"PASS "<<checks<<" actual graph providers/mode/toggle/scroll/scale handlers; synthetic layout and line-data seam\n";
}
