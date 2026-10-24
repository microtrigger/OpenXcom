// SPDX-License-Identifier: GPL-3.0-or-later
// Literal save-list constructors/provider/NEW selection/edit/save callback.
// Game/domain/path operations/queued SaveGameState are explicit no-disk seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Sound.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/FileMap.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Observer/AgentUi.h"
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <map>
#include <sstream>
#include "../../src/Interface/TextList.h"
#define private public
#include "../../src/Interface/Window.h"
#include "../../src/Interface/TextEdit.h"
#undef private
#include "../../src/Interface/ArrowButton.h"
#include "../../src/Savegame/SavedGame.h"
#include "../../src/Menu/OptionsBaseState.h"
#include "../../src/Engine/CrossPlatform.h"
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
struct Screen {Surface *surface;Surface *getSurface(){return surface;}};
struct SavedGame {bool ironman=false;std::string name="UNCHANGED";int setNames=0;bool isIronman(){return ironman;}struct Battle {int getAmbientSound(){return -1;}} battle; Battle *getSavedBattle(){return &battle;}static std::string sanitizeModName(const std::string&s){return s;}void setName(const std::string&n){name=n;++setNames;}};
struct Mod {Sound *getSoundByDepth(int,int){std::abort();}Surface *background;Surface *getSurface(const std::string&){return background;}};
struct SaveGameState {std::string filename;SaveGameState(OptionsOrigin,const std::string&f,SDL_Color*):filename(f){}};
struct LoadGameState {std::string filename;LoadGameState(OptionsOrigin,const std::string&f,SDL_Color*):filename(f){}};
struct ConfirmLoadState {std::string filename;ConfirmLoadState(OptionsOrigin,const std::string&f):filename(f){}};
struct DeleteGameState {DeleteGameState(OptionsOrigin,const std::string&){std::abort();}};
struct Game {
 SavedGame saved;SavedGame*getSavedGame(){return &saved;} Screen screen;Mod mod;Language *language;Font *big,*small;SDL_Color *palette;const void *top=nullptr;bool clear=true;int pops=0,pushes=0;std::string queued;int loads=0,confirms=0;
 Screen*getScreen(){return &screen;}Mod*getMod(){return &mod;}Language*getLanguage(){return language;}
 bool isState(const void*s)const{return top==s;}bool agentOverlayClear(const AgentUiState&)const{return clear;}bool agentNonCursorOverlayClear(const AgentUiState&)const{return clear;}
 void popState(){++pops;top=nullptr;}void pushState(SaveGameState*s){++pushes;queued=s->filename;delete s;top=nullptr;}void pushState(LoadGameState*s){++loads;queued=s->filename;delete s;top=nullptr;}void pushState(ConfirmLoadState*s){++confirms;queued=s->filename;delete s;top=nullptr;}void pushState(DeleteGameState*){std::abort();}
};
static Game *currentGame;
// Path operations are a deterministic no-write seam here; real persistence is a separate gate.
struct CrossPlatform {static std::string sanitizeFilename(const std::string&s){return s;}static bool fileExists(const std::string&s){const std::string collision="MCP Test.sav";return s.size()>=collision.size()&&s.substr(s.size()-collision.size())==collision;}static bool moveFile(const std::string&,const std::string&){std::abort();}};
struct State:OpenXcom::State {
 Game *_game=currentGame;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;
 void setPalette(SDL_Color *p){for(int i=0;i<256;++i)_palette[i]=p[i];}
 void setInterface(const std::string&,bool=false,void * = nullptr){}void centerAllSurfaces(){}void applyBattlescapeTheme(){}
 LocalizedText tr(const std::string&id)const{return _game->language->getString(id);}
 void add(Surface*s,const std::string & = "",const std::string & = ""){s->setPalette(_game->palette);s->initText(_game->big,_game->small,_game->language);_surfaces.push_back(s);}
 void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.surface);}
};
#include "ListGames.declaration.inc"
#include "ListSave.declaration.inc"
#include "ListLoad.declaration.inc"
#include "ListGames.generated.inc"
#include "ListSave.generated.inc"
#include "ListLoad.generated.inc"
}
namespace ProgressFixture {
using Fixture::State;
struct SaveGameState:State {int _firstRun;OptionsOrigin _origin;std::string _filename;SaveType _type;Text *_txtStatus;
 SaveGameState(OptionsOrigin,const std::string&,SDL_Color*);void buildUi(SDL_Color*);bool agentUiState(AgentUiState&)const override;};
struct LoadGameState:State {int _firstRun;OptionsOrigin _origin;std::string _filename;Text *_txtStatus;
 LoadGameState(OptionsOrigin,const std::string&,SDL_Color*);void buildUi(SDL_Color*);bool agentUiState(AgentUiState&)const override;};
#include "SaveProgress.generated.inc"
#include "LoadProgress.generated.inc"
}
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
template<class T> static void popupPresentation(Fixture::Game &game, Surface &canvas, SDL_Color *palette, const char *kind)
{
 T state(OPT_GEOSCAPE);game.top=&state;state.updateList();AgentUiState ui;
 check(!state.agentUiState(ui),"active popup before actual paint refused");
 state._window->think();state._window->popup();state.blit();
 check(state._window->agentPopupActive() && state.agentUiState(ui) && ui.screen==kind && ui.actions.empty() && ui.rows.empty(),"painted active popup recognized without semantic actions");
 check(YAML::Load(ui.visible)["title"].IsNull() && YAML::Load(ui.visible)["rows"].size()==0,"popup exports no titles/catalog rows");
 state._modal=state._btnCancel;check(!state.agentUiState(ui),"popup unrelated modal refused");state._modal=nullptr;
 Surface other(320,200);other.setPalette(palette);game.screen.surface=&other;check(!state.agentUiState(ui),"popup wrong current canvas refused");game.screen.surface=&canvas;
 game.clear=false;check(!state.agentUiState(ui),"popup external overlay refused");game.clear=true;
 Surface overlay(40,20,20,20);overlay.setPalette(palette);overlay.setHidden(true);state._surfaces.push_back(&overlay);state.blit();
 check(state.agentUiState(ui),"hidden overlapping child does not occlude animation");overlay.setHidden(false);state.blit();check(!state.agentUiState(ui),"shown same overlapping child occludes animation");state._surfaces.pop_back();
 SDL_Rect clip={0,0,160,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped popup paint refused");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restoration alone no fresh paint");state.blit();
 for(int i=0;i<24 && state._window->_popupStep<1.0;++i)state._window->popup();state.blit();
 check(state._window->agentPopupActive() && state.agentUiState(ui) && ui.actions.empty(),"final step still active until native showAll callback");
 state._window->popup();state.blit();check(!state._window->agentPopupActive() && state.agentUiState(ui),"ordinary completed popup resumes full provider");
 SaveInfo saved={};saved.fileName="PRIVATE-POPUP.sav";saved.displayName="Slot";saved.isoDate="2026-10-24";saved.isoTime="15:00";
 state._saves={saved};state._lstSaves->clearList();state.updateList();state.blit();check(state.agentUiState(ui),"stopped catalog current");state._saves[0].fileName="PRIVATE-CHANGED.sav";check(!state.agentUiState(ui),"stopped catalog stale is refusal not transition fallback");
 for(auto *surface:state._surfaces)delete surface;state._surfaces.clear();
}
int main(int argc,char**argv){
 check(argc==3,"source/log args");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;Options::keyboardMode=KEYBOARD_ON;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 for(const std::string master:{"xcom1","xcom2"}){
  Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
  popupPresentation<Fixture::ListSaveState>(game,canvas,palette,"save_list");
  popupPresentation<Fixture::ListLoadState>(game,canvas,palette,"load_list");
  TextList off(288,112,8,42);off.setPalette(palette);off.initText(&big,&small,&language);off.setColumns(3,188,60,40);off.addRow(3,"X","Y","Z");off.blit(&canvas);std::vector<AgentOutfittingRow> offRows;check(!off.agentSaveListRows(offRows),"save helper default off");
  Fixture::ListSaveState state(OPT_GEOSCAPE);game.top=&state;
  // Popup animation is advanced by its ordinary callback, without a game loop.
  for(int i=0;i<25;++i)state._window->popup();
  SaveInfo first={};first.fileName="PRIVATE-FIRST.sav";first.displayName="DUPLICATE";first.isoDate="2026-10-24";first.isoTime="14:00";SaveInfo second=first;second.fileName="PRIVATE-SECOND.sav";state._saves={first,second};state.updateList();AgentUiState ui;
  check(!state.agentUiState(ui),"prepaint unavailable");state.blit();
  check(state.agentUiState(ui),"fresh save list");auto v=YAML::Load(ui.visible);check(v["rows"].size()==3&&v["rows"][0]["cells"].size()==1,"one NEW then3cell existing rows");
  check(ui.visible.find("PRIVATE-")==std::string::npos&&ui.offers("select_new_save_slot")&&!ui.offers("save_new_game"),"no filename; selection notsave");
  state._modal=state._btnCancel;check(!state.agentUiState(ui),"unrelated modal refused");state._modal=nullptr;
  state._lstSaves->setVisible(false);state.blit();check(!state.agentUiState(ui),"hidden list cannot offer NEW");state._lstSaves->setVisible(true);state.blit();
  Surface overlay(190,8,8,42);overlay.setPalette(palette);state._surfaces.push_back(&overlay);state.blit();check(state.agentUiState(ui)&&!ui.offers("select_new_save_slot"),"covered NEW caption has no action");state._surfaces.pop_back();state.blit();
  SDL_Rect clip={0,60,320,140};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui)||!ui.offers("select_new_save_slot"),"wholly clipped NEW row no action");SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();check(state.agentUiState(ui),"restored actual repaint");
  const AgentUiState old=ui;state._saves[0].fileName="PRIVATE-REPLACED.sav";check(!state.agentUiState(ui),"catalog replacement beforepaint refused");state.blit();check(state.agentUiState(ui)&&!old.sameBinding(ui),"replacement invalidates old binding");
  Surface other(320,200);other.setPalette(palette);game.screen.surface=&other;check(!state.agentUiState(ui),"equalgeometry screen replacement refused");game.screen.surface=&canvas;
  AgentRequest q;q.action="select_new_save_slot";check(state.agentDispatch(q)&&state._selectedRow==0&&state._modal==nullptr,"native NEW selection nonmodal");check(game.saved.name=="UNCHANGED"&&game.pushes==0,"selection no domainwrite");
  state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["new_save_name"].as<std::string>().empty()&&ui.offers("replace_new_save_name")&&!ui.offers("save_new_game"),"rendered empty editor");
  // Characterize a caret-only repaint across real queued-command binding, no sleeps/RPC.
  const AgentUiState beforeBlink=ui;const auto beforeInput=state._edtSave->agentInputRevision();
  AgentRequest editing;editing.action="replace_new_save_name";editing.text="MCPRT155930";
  AgentFrameCommand queued;
  check(queued.stage(editing,beforeBlink,&state,10,20,100,101),"stage isolated queued edit against current UI grant");
  state.blit();check(state.agentUiState(ui)&&beforeBlink.sameBinding(ui)&&queued.matchesFrame(102,10,20,&state,ui),"ordinary unchanged repaint retains queued edit binding");
  state._edtSave->blink();state.blit();
  check(state.agentUiState(ui)&&ui.offers("replace_new_save_name")&&YAML::Load(ui.visible)["new_save_name"].as<std::string>().empty()&&state._edtSave->agentInputRevision()==beforeInput,"ordinary caret blink preserves logical name/input revision");
  check(beforeBlink.sameBinding(ui)&&queued.matchesFrame(103,10,20,&state,ui),"REGRESSION caret-only repaint preserves semantic queued grant");
  queued.clear();
  check(state.agentDispatch(editing),"fresh direct exact public label insertion succeeds");state.blit();
  check(state.agentUiState(ui)&&YAML::Load(ui.visible)["new_save_name"].as<std::string>()=="MCPRT155930","exact inserted label reaches current painted projection");
  check(game.saved.name=="UNCHANGED"&&game.pushes==0,"characterization performs no save/domain name mutation");
  // Genuine changes retain stale rejection; the current painted helper is never bypassed.
  AgentUiState stable=ui;state._edtSave->setText("Changed");state.blit();check(state.agentUiState(ui)&&!stable.sameBinding(ui),"actual text edit invalidates binding");
  stable=ui;SDL_Event left={};left.type=SDL_KEYDOWN;left.key.keysym.sym=SDLK_LEFT;Action leftAction(&left,1.,1.,0,0);leftAction.setSender(state._edtSave);
  state._edtSave->keyboardPress(&leftAction,&state);state.blit();check(state.agentUiState(ui)&&!stable.sameBinding(ui),"native caret movement invalidates binding");
  stable=ui;state._edtSave->setFocus(false,false);state.blit();check(state.agentUiState(ui)&&!stable.sameBinding(ui),"focus loss invalidates binding");state._edtSave->setFocus(true,false);state.blit();check(state.agentUiState(ui),"restore focus with fresh paint");
  stable=ui;const int editorX=state._edtSave->getX();state._edtSave->setX(editorX+1);state.blit();check(!state.agentUiState(ui)||!stable.sameBinding(ui),"editor geometry invalidates binding");state._edtSave->setX(editorX);state.blit();check(state.agentUiState(ui),"restore geometry with fresh paint");
  stable=ui;game.screen.surface=&other;state.blit();check(state.agentUiState(ui)&&!stable.sameBinding(ui),"equal geometry new painted canvas invalidates binding");game.screen.surface=&canvas;state.blit();check(state.agentUiState(ui),"restore current canvas with fresh paint");
  stable=ui;Text *originalChild=state._edtSave->_text;Text *replacementChild=new Text(*originalChild);state._edtSave->_text=replacementChild;state._edtSave->invalidate();state.blit();
  check(state.agentUiState(ui)&&!stable.sameBinding(ui),"equal logical child replacement invalidates binding");
  replacementChild->setX(1);state._edtSave->invalidate();state.blit();check(!state.agentUiState(ui)||!ui.offers("replace_new_save_name"),"current child geometry mismatch still refuses editing");replacementChild->setX(0);state._edtSave->invalidate();state.blit();check(state.agentUiState(ui),"restore exact child geometry with fresh paint");
  state._edtSave->_text=originalChild;state._edtSave->invalidate();state.blit();delete replacementChild;check(state.agentUiState(ui),"restore owned child with fresh paint");
  stable=ui;state._saves[0].fileName="PRIVATE-EDITOR-CHANGED.sav";check(!state.agentUiState(ui),"editing catalog mutation refuses old capture");state.blit();check(state.agentUiState(ui)&&!stable.sameBinding(ui),"repainted catalog mutation invalidates binding");

  // Native painted missing glyphs must not become an MCP readability gate.
  state._edtSave->setText("\xF4\x8F\xBF\xBF");state.blit();std::string legacy;
  check(!state._edtSave->agentRenderedText(legacy),"legacy missing-glyph gate demonstrably refuses");
  check(state.agentUiState(ui)&&ui.offers("replace_new_save_name"),"logical painted missing-glyph caption permits edit");
  check(!state._edtSave->agentReplaceSaveName(&other,"Probe"),"save-only helper rejects wrong current destination");
  state._edtSave->setFocus(false,false);state.blit();check(!state._edtSave->agentReplaceSaveName(&canvas,"Probe"),"save-only helper rejects unfocused");
  state._edtSave->setFocus(true,false);state.blit();Options::keyboardMode=KEYBOARD_OFF;
  check(!state._edtSave->agentReplaceSaveName(&canvas,"Probe"),"save-only helper rejects keyboard-off");Options::keyboardMode=KEYBOARD_ON;state.blit();
  q.action="replace_new_save_name";q.text="MCP Test";check(state.agentDispatch(q),"bounded replacement");check(!state.agentUiState(ui)||!ui.offers("save_new_game"),"unpainted edit cannot save");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["new_save_name"].as<std::string>()==q.text&&ui.offers("save_new_game"),"fresh logical edit offers save");
  const auto rev=state._edtSave->agentInputRevision();const auto caret=state._edtSave->_caretPos;q.text=std::string(128,'W');check(!state.agentDispatch(q)&&state._edtSave->getText()=="MCP Test"&&state._edtSave->agentInputRevision()==rev&&state._edtSave->_caretPos==caret,"native too wide value/caret rollback");q.text="../bad";check(!state.agentDispatch(q),"path input refused");
  state._selectedRow=1;state.blit();check(state.agentUiState(ui)&&!ui.offers("save_new_game")&&!ui.offers("replace_new_save_name"),"existing slot never writable");state._selectedRow=0;state.blit();
  Options::keyboardMode=KEYBOARD_OFF;check(state.agentUiState(ui)&&!ui.offers("save_new_game"),"keyboardoffpreview no mutation capability");Options::keyboardMode=KEYBOARD_ON;
  q.action="save_new_game";check(state.agentDispatch(q)&&game.pushes==1&&game.saved.name=="MCP Test"&&game.queued=="MCP Test_.sav","literal native NEW collision appends underscore; queues work, notdisk success");
  check(!state.agentDispatch(q)&&game.pushes==1,"replaced top refuses second Save dispatch");
  for(auto*s:state._surfaces)delete s;state._surfaces.clear();
  Fixture::ListLoadState load(OPT_MENU);game.top=&load;for(int i=0;i<25;++i)load._window->popup();load._saves={first,second};load.updateList();load.blit();
  check(load.agentUiState(ui)&&ui.rows.size()==2&&ui.offers("load_saved_game"),"actual load constructor offers current visible rows");
  const AgentUiState oldLoad=ui;std::swap(load._saves[0],load._saves[1]);check(!load.agentUiState(ui),"duplicatecaption reordered catalog invalidates prior load");std::swap(load._saves[0],load._saves[1]);load.blit();
  q.action="load_saved_game";q.choice="r1";check(load.agentDispatch(q)&&game.loads==1&&game.queued=="PRIVATE-SECOND.sav","explicit visible duplicatecaption row routes correct native private operand");
  check(!load.agentDispatch(q)&&game.loads==1,"load no automatic replay after top replacement");
  game.top=&load;load._saves[1].mods={"missing-content"};load.blit();
  check(load.agentDispatch(q)&&game.confirms==1&&game.loads==1,"native missingcontent opens confirmation; never autoYes");
  for(auto*s:load._surfaces)delete s;load._surfaces.clear();
  ProgressFixture::SaveGameState saving(OPT_GEOSCAPE,"PRIVATE-FILENAME.sav",palette);game.top=&saving;
  check(!saving.agentUiState(ui),"saving prepaint unavailable");saving.blit();check(saving.agentUiState(ui)&&ui.screen=="save_in_progress"&&ui.actions.empty()&&ui.visible.find("PRIVATE")==std::string::npos,"only actual saving caption, no action/filename");
  saving._txtStatus->setText(std::string(200,'W'));saving.blit();check(saving.agentUiState(ui)&&YAML::Load(ui.visible)["message"].as<std::string>()==std::string(200,'W'),"logical overflowing progress caption without glyphgate");
  game.screen.surface=&other;check(!saving.agentUiState(ui),"saving changed destination refused");game.screen.surface=&canvas;
  for(auto*s:saving._surfaces)delete s;saving._surfaces.clear();
  ProgressFixture::LoadGameState loading(OPT_MENU,"PRIVATE-FILENAME.sav",palette);game.top=&loading;loading.blit();check(loading.agentUiState(ui)&&ui.screen=="load_in_progress"&&ui.actions.empty()&&ui.visible.find("PRIVATE")==std::string::npos,"actual loading caption only");
  loading._modal=loading._txtStatus;check(!loading.agentUiState(ui),"loading modal refused");loading._modal=nullptr;
  for(auto*s:loading._surfaces)delete s;loading._surfaces.clear();

 }
 std::cout<<"PASS "<<checks<<" checks / 2 masters; persistence/transport/live NOT RUN\n";
}
