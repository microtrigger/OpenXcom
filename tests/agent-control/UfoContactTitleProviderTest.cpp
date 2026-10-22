// SPDX-License-Identifier: GPL-3.0-or-later
// Actual constructor/provider/callback bodies; explicit Game/Mod/Craft seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Window.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include "../../src/Interface/TextList.h"
#include "../../src/Engine/Unicode.h"
#include "../../src/Engine/FileMap.h"
#include <sstream>
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
struct Screen { Surface *surface; Surface *getSurface(){return surface;} };
struct RuleCraft { bool isWaterOnly(){return false;} };
struct SavedGame {int getId(const std::string&){return 1;}};
struct Mod { Surface *background; std::vector<std::string> crafts; RuleCraft rule; const std::vector<std::string>& getCraftsList(){return crafts;} RuleCraft* getCraft(const std::string&){return &rule;} Surface *getSurface(const std::string&){return background;} };
struct Game {
    Screen screen; Mod mod; SavedGame saved; int cursorX=-20,cursorY=-20; bool fpsClear=true; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0;
    Screen *getScreen(){return &screen;} Mod *getMod(){return &mod;}
    Language *getLanguage(){return language;}
    bool isState(const void *state) const{return top == state;}
    Surface *_cursor=nullptr,*_fpsCounter=nullptr;
    bool agentOverlayClear(const AgentUiState&)const;
    bool agentNonCursorOverlayClear(const AgentUiState&)const;
    SavedGame* getSavedGame(){return &saved;} void pushState(OpenXcom::State*){}
    void popState(){++pops;}
};
#include "GameOverlays.generated.inc"
static Game *currentGame;
struct State : OpenXcom::State {
    Game *_game = currentGame;
    std::function<void()> paintHook;
    using OpenXcom::State::_surfaces;
    using OpenXcom::State::_modal;
    void setInterface(const std::string&,bool=false){}
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
struct RuleUfo { std::string getSize(){return "STR_VERY_SMALL";} std::string getType(){return "STR_SMALL_SCOUT";} };
struct Mission {std::string getRegion(){return "STR_EUROPE";}};
struct Ufo {enum {FLYING=0}; std::string name="UFO-1",altitude="STR_VERY_LOW"; int status=0,id=1,landId=1; RuleUfo rule;Mission mission;
 int getId(){return id;}void setId(int x){id=x;}int getLandId(){return landId;}void setLandId(int x){landId=x;}
 std::string getName(Language*){return name;}std::string getAltitude(){return altitude;}
 double getLongitude(){return 0;}double getLatitude(){return 0;}RuleUfo*getRules(){return &rule;}
 std::string getDirection(){return "STR_NORTH";}int getStatus(){return status;}int getSpeed(){return 940;}
 std::string getAlienRace(){return "STR_SECTOID";}std::string getMissionType(){return "STR_ALIEN_RESEARCH";}Mission*getMission(){return &mission;}
};
struct Globe {bool insideLand(double,double){return true;}void center(double,double){}};
struct GeoscapeState {Globe globe;Globe*getGlobe(){return &globe;}void timerReset(){}};
struct InterceptState:OpenXcom::State {InterceptState(Globe*,int,Ufo*){}};
struct UfoDetectedState:State {
 Ufo*_ufo;GeoscapeState*_state;Window*_window;TextButton*_btnIntercept,*_btnCentre,*_btnCancel;Text*_txtUfo,*_txtDetected,*_txtHyperwave;TextList*_lstInfo,*_lstInfo2;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
 UfoDetectedState(Ufo*,GeoscapeState*,bool,bool);~UfoDetectedState();
 void btnInterceptClick(Action*);void btnCentreClick(Action*);void btnCancelClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void finishPopup(){_window->think();for(int i=0;i<30;++i)_window->popup();}
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "UfoContactTitle.generated.inc"
}
static unsigned checks=0;static void check(bool ok,const char*msg){++checks;if(!ok){std::cerr<<"FAIL "<<msg<<"\n";std::exit(1);}}
int main(int argc,char**argv){
 check(argc==2,"source argument");check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");
 Options::baseXResolution=320;Options::baseYResolution=200;
 FileMap::load("common","./",false);
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200),cursor(9,13,-20,-20),fps(10,10,-20,-20);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"})for(bool hyper:{false,true})for(bool landed:{false,true}){
 Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");
 Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;game._cursor=&cursor;game._fpsCounter=&fps;Fixture::currentGame=&game;
 cursor.setX(-20);cursor.setY(-20);fps.setX(-20);fps.setY(-20);
 Fixture::Ufo ufo;ufo.name=master=="xcom1"?(landed?"LANDING SITE-1":"UFO-1"):(landed?"TOUCHDOWN SITE-1":"ALIEN SUB-1");ufo.altitude=landed?"STR_GROUND":"STR_VERY_LOW";ufo.status=landed?1:0;
 Fixture::GeoscapeState geo;Fixture::UfoDetectedState state(&ufo,&geo,true,hyper);game.top=&state;AgentUiState ui;
 check(!state.agentUiState(ui),"prepaint refused");state.finishPopup();state.blit();
 auto title=[&](){check(state.agentUiState(ui),"provider available");auto v=YAML::Load(ui.visible)["title"];return v.IsNull()?std::string("<null>"):v.as<std::string>();};
 check(title()==ufo.name,"stock constructor logical title");check(state._txtUfo->agentTextComplete(),"stock title has native glyph coverage");check(ui.offers("open_contact_interception")&&ui.offers("dismiss_ufo_contact"),"two ordinary actions");const auto baseline=ui.visible;
 cursor.setX(state._txtUfo->getX()+50);cursor.setY(state._txtUfo->getY());check(title()==ufo.name,"cursor over title not readability gate");check(ui.visible==baseline,"cursor title leaves all fields unchanged");check(game.agentOverlayClear(ui),"outer controller overlay check retains title cursor admission");
 fps.setX(cursor.getX());fps.setY(cursor.getY());check(title()=="<null>","FPS overlay still obscures title");fps.setX(-20);fps.setY(-20);
 cursor.setX(state._btnIntercept->getX()+10);cursor.setY(state._btnIntercept->getY());check(title()==ufo.name&&!ui.offers("open_contact_interception"),"cursor button admission unchanged");cursor.setX(-20);cursor.setY(-20);
 auto *text=state._txtUfo;text->setText(ufo.name+" ");check(!state.agentUiState(ui),"title text revision stale");text->setText(ufo.name);state.blit();
 const int x=text->getX();text->setX(x+1);text->setX(x);check(!state.agentUiState(ui),"geometry ABA stale");state.blit();check(title()==ufo.name,"repaint restores");
 text->setVisible(false);state.blit();check(title()=="<null>","hidden title omitted");text->setVisible(true);state.blit();
 text->setX(-1);state.blit();check(title()=="<null>","offcanvas title omitted");text->setX(x);state.blit();
 game.top=nullptr;check(!state.agentUiState(ui),"wrong top refused");game.top=&state;state._modal=state._btnCancel;check(!state.agentUiState(ui),"local modal refused");state._modal=nullptr;
 SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped paint refused");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restore requires paint");state.blit();
 Surface cover(10,10,text->getX()+20,text->getY());cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();check(title()=="<null>","later actual widget overlap withheld");state._surfaces.pop_back();state.blit();check(title()==ufo.name,"cover removal repaint restores");
 text->setText(std::string(500,'W'));state.blit();check(title()=="<null>","actual layout overflow withheld");text->setText(ufo.name);state.blit();
 // Font unsupported character is a readability issue, not hidden state: exact UI text remains public.
 text->setText("A\xF4\x8F\xBF\xBF");state.blit();check(!text->agentTextComplete(),"missing glyph makes legacy readability incomplete");check(title()==text->getText(),"missing glyph does not gate title");
 state.release();
 }
 check(!SDL_GetVideoSurface(),"no game video");std::cout<<"PASS "<<checks<<" contact constructor/provider checks; explicit domain seams\n";
}
