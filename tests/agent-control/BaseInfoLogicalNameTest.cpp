// SPDX-License-Identifier: GPL-3.0-or-later
// Literal current BaseInfo constructor/init/provider and native TextEdit draw.
// Explicit Game/Mod/Base/Mini/resource/render seams; no full navigation or rename qualification.
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
#include "../../src/Interface/Bar.h"
#include "../../src/Observer/AgentProjectileOutput.h"
#include <cmath>
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <map>
#include <sstream>
#include <vector>
#define private public
#include "../../src/Interface/TextEdit.h"
#undef private
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
static bool forbidModelRead=false;static unsigned modelReads=0,modelWrites=0;
static void modelRead(){++modelReads;if(forbidModelRead){std::cerr<<"FAIL post-init model getter\n";std::abort();}}
struct Base {std::string name="MCP Research";std::string getName(){modelRead();return name;}void setName(const std::string&v){name=v;++modelWrites;}
 int getAvailableContainment(){modelRead();return 10;}
 int getAvailableEngineers(){modelRead();return 10;}
 int getAvailableHangars(){modelRead();return 10;}
 int getAvailableLaboratories(){modelRead();return 10;}
 int getAvailableQuarters(){modelRead();return 10;}
 int getAvailableScientists(){modelRead();return 10;}
 int getAvailableSoldiers(){modelRead();return 10;}
 int getAvailableStores(){modelRead();return 10;}
 int getAvailableWorkshops(){modelRead();return 10;}
 int getDefenseValue(){modelRead();return 10;}
 int getLongRangeDetection(){modelRead();return 10;}
 int getShortRangeDetection(){modelRead();return 10;}
 int getTotalEngineers(){modelRead();return 10;}
 int getTotalScientists(){modelRead();return 10;}
 int getTotalSoldiers(){modelRead();return 10;}
 int getUsedContainment(){modelRead();return 0;}
 int getUsedHangars(){modelRead();return 0;}
 int getUsedLaboratories(){modelRead();return 0;}
 int getUsedQuarters(){modelRead();return 0;}
 int getUsedStores(){modelRead();return 0;}
 int getUsedWorkshops(){modelRead();return 0;}
};
struct SavedGame{std::vector<Base*>bases;std::vector<Base*>*getBases(){modelRead();return &bases;}};
struct BasescapeState{};
struct MiniBaseView:InteractiveSurface{using InteractiveSurface::InteractiveSurface;struct Slot{unsigned slot;bool selected;};std::vector<Base*>*bases=nullptr;unsigned selected=0;
 void setTexture(void*){}void setBases(std::vector<Base*>*b){bases=b;}void setSelectedBase(size_t n){selected=n;}
 std::uint64_t agentRevision()const{return 1;}std::vector<Slot>agentSlots(const AgentProjectileOutput&)const{return {};}
 void hoverAt(int){} };
struct Screen{Surface*surface;Surface*getSurface(){return surface;}bool agentCaptureOutput(const Surface*,AgentProjectileOutput&)const{return false;}};
struct Mod{Surface*background;Surface*getSurface(const std::string&){return background;}void*getSurfaceSet(const std::string&){return nullptr;}};
struct Game{SavedGame saved;SavedGame*getSavedGame(){modelRead();return &saved;}Screen screen;Mod mod;Language*language;Font*big,*small;SDL_Color*palette;
 const void*top=nullptr;int pops=0;bool clear=true,cursorVisible=false,fpsVisible=false;SDL_Rect cursor={},fps={};
 Screen*getScreen(){return &screen;}Mod*getMod(){return &mod;}Language*getLanguage(){return language;}bool isState(const void*s)const{return top==s;}void popState(){++pops;top=nullptr;}
 bool agentNonCursorOverlayClear(const AgentUiState&u)const{if(!clear)return false;for(const auto&r:u.regions)if(fpsVisible&&r.x<fps.x+fps.w&&r.x+r.width>fps.x&&r.y<fps.y+fps.h&&r.y+r.height>fps.y)return false;return true;}
 bool agentOverlayClear(const AgentUiState&u)const{if(!agentNonCursorOverlayClear(u))return false;for(const auto&r:u.regions)if(cursorVisible&&r.x<cursor.x+cursor.w&&r.x+r.width>cursor.x&&r.y<cursor.y+cursor.h&&r.y+r.height>cursor.y)return false;return true;}
};
static Game*currentGame;
struct State:OpenXcom::State{Game*_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;
 void setInterface(const std::string&){}void centerAllSurfaces(){}LocalizedText tr(const std::string&id)const{return _game->language->getString(id);}
 void add(Surface*s){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);}
 void add(Surface*s,const std::string&,const std::string&){add(s);if(auto*t=dynamic_cast<Text*>(s))t->setColor(40);if(auto*b=dynamic_cast<TextButton*>(s))b->setColor(32);if(auto*e=dynamic_cast<TextEdit*>(s))e->setColor(40);}
 void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}
};
struct BaseInfoState:State {

	Base *_base;
	BasescapeState *_state;

	Surface *_bg;
	MiniBaseView *_mini;
	TextButton *_btnOk, *_btnTransfers, *_btnStores, *_btnMonthlyCosts;
	TextEdit *_edtBase;

	Text *_txtPersonnel, *_txtSoldiers, *_txtEngineers, *_txtScientists;
	Text *_numSoldiers, *_numEngineers, *_numScientists;
	Bar *_barSoldiers, *_barEngineers, *_barScientists;

	Text *_txtSpace, *_txtQuarters, *_txtStores, *_txtLaboratories, *_txtWorkshops, *_txtContainment, *_txtHangars;
	Text *_numQuarters, *_numStores, *_numLaboratories, *_numWorkshops, *_numContainment, *_numHangars;
	Bar *_barQuarters, *_barStores, *_barLaboratories, *_barWorkshops, *_barContainment, *_barHangars;

	Text *_txtDefense, *_txtShortRange, *_txtLongRange;
	Text *_numDefense, *_numShortRange, *_numLongRange;
	Bar *_barDefense, *_barShortRange, *_barLongRange;
BaseInfoState(Base*,BasescapeState*);~BaseInfoState();void init();void edtBaseChange(Action*);void btnOkClick(Action*);
 void miniClick(Action*){std::abort();}void handleKeyPress(Action*){std::abort();}void btnTransfersClick(Action*){std::abort();}void btnStoresClick(Action*){std::abort();}void btnMonthlyCostsClick(Action*){std::abort();}
 bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "BaseInfo.generated.inc"
}
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
static void key(TextEdit&e,SDLKey k,Uint16 c=0){SDL_Event v={};v.type=SDL_KEYDOWN;v.key.keysym.sym=k;v.key.keysym.unicode=c;Action a(&v,1,1,0,0);e.keyboardPress(&a,nullptr);}
int main(int argc,char**argv){
 check(argc==3,"source/log arguments");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen native widgets");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;Options::keyboardMode=KEYBOARD_ON;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"})for(bool storage:{false,true}){
  Options::storageLimitsEnforced=storage;Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;Fixture::Base base;Fixture::BasescapeState parent;game.saved.bases={&base};
  Fixture::BaseInfoState state(&base,&parent);game.top=&state;state.init();const unsigned reads=Fixture::modelReads,writes=Fixture::modelWrites;Fixture::forbidModelRead=true;
  auto*edit=state._edtBase;AgentUiState ui;const auto name=[&](const std::string&value){check(state.agentUiState(ui),"active BaseInfo DTO admitted");check(YAML::Load(ui.visible)["base_name"].as<std::string>()==value,"exact current painted child caption");};const auto absent=[&](){check(state.agentUiState(ui)&&YAML::Load(ui.visible)["base_name"].IsNull(),"unavailable name null with no fallback");};
  check(!state.agentUiState(ui),"whole constructor before firstpaint unavailable");state.blit();name("MCP Research");check(ui.context.size()==5,"only private epoch and allocation binding");base.name="HIDDEN MODEL NAME";name("MCP Research");
  std::string shown;SDL_Rect allocation={};std::uint64_t epoch=0;check(edit->agentBaseInfoName(&canvas,shown,allocation,epoch)&&epoch>0&&shown=="MCP Research"&&allocation.x==8&&allocation.y==8&&allocation.w==127&&allocation.h==16,"actual ctor structural allocation and capture");
  auto oldEpoch=epoch;state.blit();check(edit->agentBaseInfoName(&canvas,shown,allocation,epoch)&&epoch==oldEpoch,"cached blit no newdraw epoch");AgentRequest close;close.action="close_base_info";AgentFrameCommand frame;check(frame.stage(close,ui,&state,1,1,100,100),"stage current name authority");edit->invalidate();state.blit();name("MCP Research");check(edit->agentBaseInfoName(&canvas,shown,allocation,epoch)&&epoch!=oldEpoch&&!frame.matchesFrame(101,1,1,&state,ui),"identical actual redraw newepoch invalidates authority");
  edit->draw();absent();edit->blit(&canvas);name("MCP Research");
  for(const std::string caption:{std::string(""),std::string(500,'W'),std::string("LINE\nLINE"),std::string("\xF0\x9F\x99\x82"),std::string(1024,'X')}){edit->setText(caption);absent();state.blit();name(caption);}
  edit->setText(std::string(1025,'X'));state.blit();absent();edit->setText("VISIBLE CHILD");state.blit();name("VISIBLE CHILD");
  SDL_FillRect(edit->_text->getSurface(),nullptr,0);SDL_FillRect(edit->getSurface(),nullptr,0);name("VISIBLE CHILD");
  edit->_value=Unicode::convUtf8ToUtf32("HIDDEN EDITOR VALUE");name("VISIBLE CHILD");check(edit->getText()=="HIDDEN EDITOR VALUE","fixture contradictory editorvalue differs from currentchild");
  edit->setText("MCP");state.blit();Options::keyboardMode=KEYBOARD_OFF;edit->setFocus(true,false);state.blit();check(edit->_blink&&edit->getText()=="MCP","native keyboardoff focused preview does not mutate editorvalue");name("MCPA");edit->blink();absent();state.blit();name("MCP");edit->blink();state.blit();name("MCPA");edit->setFocus(false);Options::keyboardMode=KEYBOARD_ON;state.blit();name("MCP");
  edit->setFocus(true);AgentUiState blocked;check(!state.agentUiState(blocked),"ordinary modal focused editor refuses wholeprovider");edit->setFocus(false);state.blit();
  edit->setText("MCP TFTD Test");edit->setFocus(true,false);state.blit();name("MCP TFTD Test");check(edit->_caretPos==edit->_value.size(),"native end caret position retained");edit->blink();state.blit();name("MCP TFTD Test");edit->setFocus(false);state.blit();
  edit->_text->setText("UNPAINTED CHILD SECRET");absent();edit->invalidate();state.blit();name("MCP TFTD Test");
  edit->_text->setVisible(false);absent();edit->invalidate();state.blit();absent();edit->_text->setVisible(true);edit->invalidate();state.blit();name("MCP TFTD Test");edit->_text->setVisible(false);edit->_text->setVisible(true);absent();edit->invalidate();state.blit();name("MCP TFTD Test");
  edit->setVisible(false);absent();edit->setVisible(true);absent();state.blit();name("MCP TFTD Test");
  int x=edit->getX();edit->setX(x+1);edit->setX(x);absent();state.blit();name("MCP TFTD Test");int cx=edit->_text->getX();edit->_text->setX(cx+1);edit->_text->setX(cx);absent();edit->invalidate();state.blit();name("MCP TFTD Test");
  edit->setX(194);state.blit();absent();edit->setX(8);state.blit();name("MCP TFTD Test");
  edit->_text->setX(1);edit->invalidate();state.blit();absent();edit->_text->setX(0);edit->invalidate();state.blit();name("MCP TFTD Test");edit->_text->setWidth(126);edit->invalidate();state.blit();absent();edit->_text->setWidth(127);edit->invalidate();state.blit();name("MCP TFTD Test");
  auto inputBefore=edit->agentInputRevision();edit->setText("TEMPORARY");edit->setText("MCP TFTD Test");check(edit->agentInputRevision()!=inputBefore,"samefinalvalue input ABA changes revision");absent();state.blit();name("MCP TFTD Test");
  Text*original=edit->_text;edit->_text=new Text(127,16,0,0);edit->_text->setPalette(palette);edit->_text->initText(&big,&small,&language);edit->_text->setBig();absent();edit->invalidate();state.blit();name("MCP TFTD Test");delete original;
  SDL_Rect tiny={0,0,1,1};SDL_SetClipRect(edit->getSurface(),&tiny);edit->invalidate();state.blit();absent();SDL_SetClipRect(edit->getSurface(),nullptr);absent();edit->invalidate();state.blit();name("MCP TFTD Test");
  SDL_SetClipRect(edit->_text->getSurface(),&tiny);edit->invalidate();state.blit();absent();SDL_SetClipRect(edit->_text->getSurface(),nullptr);absent();edit->invalidate();state.blit();name("MCP TFTD Test");
  Surface alternate(320,200);alternate.setPalette(palette);game.screen.surface=&alternate;absent();game.screen.surface=&canvas;name("MCP TFTD Test");game.screen.surface=&alternate;state.blit();name("MCP TFTD Test");game.screen.surface=&canvas;absent();state.blit();name("MCP TFTD Test");
  SDL_SetClipRect(canvas.getSurface(),&tiny);state.blit();check(!edit->agentBaseInfoName(&canvas,shown,allocation,epoch)&&shown.empty()&&epoch==0,"destinationclip helper refuses and clearsoutput");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!edit->agentBaseInfoName(&canvas,shown,allocation,epoch),"destinationclip restore alone not newpaint");edit->invalidate();state.blit();name("MCP TFTD Test");
  Surface cover(5,5,8,8);cover.setPalette(palette);cover.clear(90);state._surfaces.push_back(&cover);state.blit();absent();state._surfaces.pop_back();state.blit();name("MCP TFTD Test");game.fpsVisible=true;game.fps={8,8,5,5};absent();game.fpsVisible=false;game.cursorVisible=true;game.cursor={8,8,5,5};name("MCP TFTD Test");game.cursorVisible=false;
  game.cursorVisible=true;game.cursor={12,182,5,5};int pops=game.pops;check(!state.agentDispatch(close)&&game.pops==pops,"existing nativeOK cursor guard unchanged");game.cursorVisible=false;game.top=nullptr;check(!state.agentUiState(blocked),"inactive wholeprovider refused");game.top=&state;
  check(Fixture::modelReads==reads&&Fixture::modelWrites==writes,"all observation never reads or renames Base after init");check(state.agentDispatch(close)&&game.pops==pops+1&&Fixture::modelWrites==writes,"existing literal OK onlypop no rename");state.release();Fixture::forbidModelRead=false;
  TextEdit pair(nullptr,127,16,8,8);pair.setPalette(palette);pair.initText(&big,&small,&language);pair.setBig();pair.setText("PAIR");pair.blit(&canvas);shown="sentinel";epoch=999;check(!pair.agentBaseInfoName(&canvas,shown,allocation,epoch)&&shown.empty()&&epoch==0,"sameobject validnative defaultoff clearsoutputs");pair.agentEnableBaseInfoName();check(!pair.agentBaseInfoName(&canvas,shown,allocation,epoch),"enable needs actualnewdraw");pair.blit(&canvas);check(pair.agentBaseInfoName(&canvas,shown,allocation,epoch)&&shown=="PAIR","sameobject optin plus nativepaint succeeds");
  pair._agentBaseInfoNameDrawEpoch=UINT64_MAX;pair.invalidate();pair.blit(&canvas);check(pair._agentBaseInfoNameEpochExhausted&&!pair.agentBaseInfoName(&canvas,shown,allocation,epoch)&&shown.empty()&&epoch==0,"draw epoch overflow sticky refusal clearsoutput");pair._agentBaseInfoNameDrawEpoch=1;pair.invalidate();pair.blit(&canvas);check(!pair.agentBaseInfoName(&canvas,shown,allocation,epoch),"exhausted observer cannot revive");
  // Existing edit path remains strict and atomic, independent of new observer.
  TextEdit legacy(nullptr,127,16,8,8);legacy.setPalette(palette);legacy.initText(&big,&small,&language);legacy.setBig();legacy.setFocus(true,false);legacy.blit(&canvas);check(legacy.agentReplaceText("MCP Draft"),"legacy normalreplacement accepted");key(legacy,SDLK_LEFT);key(legacy,SDLK_LEFT);key(legacy,SDLK_LEFT);legacy.blink();legacy.blit(&canvas);auto rev=legacy.agentInputRevision();check(!legacy.agentReplaceText(std::string(128,'W'))&&!legacy.agentReplaceText("A\xF0\x9F\x8C\x8C")&&legacy.getText()=="MCP Draft"&&legacy.agentInputRevision()==rev,"legacy wide/missingglyph invalidreplacement atomically refused");key(legacy,SDLK_EXCLAIM,'!');legacy.blit(&canvas);check(legacy.getText()=="MCP Dr!aft","legacy rollback preserves middle insertion position");
 }
 std::cout<<"PASS "<<checks<<" BaseInfo literal constructor/init/current-child checks; explicit domain/Mini/render seams\n";
}
