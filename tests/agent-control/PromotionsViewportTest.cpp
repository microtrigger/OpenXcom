// SPDX-License-Identifier: GPL-3.0-or-later
// Literal constructor/provider and Soldier::isPromoted; explicit Game/Mod/Base/SavedGame seams.
// Actual visible OK dispatch calls literal native pop-only handler; model seams remain explicit.
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
#include "../../src/Observer/AgentFrameCommand.h"
#include <yaml-cpp/yaml.h>
#include <functional>
#include <map>
#include <sstream>
#include <vector>
#define private public
#include "../../src/Interface/TextList.h"
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
struct Screen { Surface *surface; Surface *getSurface(){return surface;} };
static bool forbidModelRead=false;static unsigned modelReads=0;
static void modelRead(){++modelReads;if(forbidModelRead){std::cerr<<"FAIL post-constructor model getter\n";std::abort();}}
struct Soldier {bool _recentlyPromoted=false;std::string name="PRIVATE SOLDIER",rank="STR_ROOKIE";bool nativeIsPromoted();bool isPromoted(){modelRead();return nativeIsPromoted();}const std::string&getName(){modelRead();return name;}const std::string&getRankString(){modelRead();return rank;}};
#include "SoldierPromotion.generated.inc"
struct Base {std::string name="PRIVATE BASE";std::vector<Soldier*> soldiers;std::vector<Soldier*> *getSoldiers(){modelRead();return &soldiers;}const std::string&getName(){modelRead();return name;}};
struct SavedGame {std::vector<Base*> bases;std::vector<Base*> *getBases(){modelRead();return &bases;}};
struct Mod {Surface *background;Surface *getSurface(const std::string&){return background;}};
struct Game {SavedGame saved;SavedGame *getSavedGame(){modelRead();return &saved;}
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0;
    Screen *getScreen(){return &screen;} Mod *getMod(){return &mod;}
    Language *getLanguage(){return language;}
    bool isState(const void *state) const{return top == state;}
    bool cursorVisible=false; SDL_Rect cursor={0,0,0,0};bool fpsVisible=false;SDL_Rect fps={0,0,0,0};
    bool agentOverlayClear(const AgentUiState &ui) const{
        if(!clear)return false;
        for(const auto &r:ui.regions)if(cursorVisible && r.x<cursor.x+cursor.w && r.x+r.width>cursor.x && r.y<cursor.y+cursor.h && r.y+r.height>cursor.y)return false;
        return true;
    }
    void popState(){++pops;top=nullptr;}
    bool agentNonCursorOverlayClear(const AgentUiState&ui) const{if(!clear)return false;for(const auto&r:ui.regions)if(fpsVisible&&r.x<fps.x+fps.w&&r.x+r.width>fps.x&&r.y<fps.y+fps.h&&r.y+r.height>fps.y)return false;return true;}
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
struct PromotionsState : State {
 TextButton *_btnOk;Window *_window;Text *_txtTitle,*_txtName,*_txtRank,*_txtBase;TextList *_lstSoldiers;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentStoredPaintRevisions;
 PromotionsState();~PromotionsState();void btnOkClick(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t> agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
 void release(){for(auto*s:_surfaces)delete s;_surfaces.clear();}
};
#include "Promotions.generated.inc"
}
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 check(argc==3,"source and isolated log arguments");Logger::logFile()=argv[2];check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");FileMap::load("common","./",false);Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}
 Surface canvas(320,200),background(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
 AgentRequest parsed;const std::string token(32,'1'),prefix="E "+token+" 1 "+token+" "+token+" 1 close_promotions";
 check(parseAgentRequest(prefix,parsed),"native protocol accepts registered close empty args");for(const std::string suffix:{" extra"," r0"," 0","\n"})check(!parseAgentRequest(prefix+suffix,parsed),"native protocol rejects extra close args");
 for(const std::string master:{"xcom1","xcom2"}){
  Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;game.language=&language;game.big=&big;game.small=&small;game.palette=palette;Fixture::currentGame=&game;
  Fixture::Soldier first,second,unpromoted;Fixture::Base base;base.soldiers={&first,&second,&unpromoted};game.saved.bases={&base};first._recentlyPromoted=second._recentlyPromoted=true;
  Fixture::PromotionsState state;game.top=&state;AgentUiState ui;Fixture::forbidModelRead=true;const unsigned readsAfterCtor=Fixture::modelReads;
  check(!first._recentlyPromoted&&!second._recentlyPromoted&&!unpromoted._recentlyPromoted&&state._lstSoldiers->getRows()==2,"literal constructor consumes promotion flags and lists only promoted soldiers");
  check(!state.agentUiState(ui),"prepaint refused");state.blit();check(state.agentUiState(ui)&&ui.screen=="promotions","native constructor current screen admitted");
  auto v=YAML::Load(ui.visible);check(v.size()==4&&v["title"].as<std::string>()==std::string(language.getString("STR_PROMOTIONS"))&&v["headings"].size()==3,"only current title and three headings");
  for(size_t i=0;i<3;++i)check(v["headings"][i].as<std::string>()==std::string(language.getString(std::vector<std::string>{"STR_NAME","STR_NEW_RANK","STR_BASE"}[i])),"native heading retained despite onepixel spare overlap with list");
  check(ui.actions.size()==1&&ui.offers("close_promotions")&&ui.rows.empty()&&YAML::Load(ui.visible)["rows"].size()==2,"only current cells and visible OK no row handles");
  auto report=YAML::Load(ui.visible)["rows"];check(report.size()==2,"two actual constructor rows");for(auto row:report)check(row.size()==1&&row["cells"].size()==3,"public row exact cells-only no index selection count");
  check(report[0]["cells"][0].as<std::string>()==first.name&&report[0]["cells"][1].as<std::string>()==std::string(language.getString(first.rank))&&report[0]["cells"][2].as<std::string>()==base.name,"native name rank base column order");
  check(report[0]["cells"][0].as<std::string>()==report[1]["cells"][0].as<std::string>(),"duplicate names remain distinct viewport rows");
  std::vector<AgentOutfittingRow> captured;check(state._lstSoldiers->agentPromotionRows(captured)&&captured.size()==2&&captured[0].bounds.h==8&&state._lstSoldiers->_font->getHeight()==9&&state._lstSoldiers->_font->getSpacing()==-1,"native9minus1 advance8 geometry");
  check(captured[0].cells[2].known&&captured[0].cells[2].regions.size()==1&&captured[0].cells[2].regions[0].w==76&&state._lstSoldiers->_texts[0][2]->getX()==212,"native final8px horizontal clip retains76px logical cell");
  check(YAML::Load(ui.visible)["buttons"].size()==1&&YAML::Load(ui.visible)["buttons"][0]["id"].as<std::string>()=="ok"&&YAML::Load(ui.visible)["buttons"][0]["label"].as<std::string>()==std::string(language.getString("STR_OK")),"visible native OK exact label and ID");
  first._recentlyPromoted=second._recentlyPromoted=true;first.name="HIDDEN CHANGED SOLDIER";base.name="HIDDEN CHANGED BASE";
  for(int i=0;i<3;++i)check(state.agentUiState(ui)&&first._recentlyPromoted&&second._recentlyPromoted&&ui.visible.find("HIDDEN CHANGED")==std::string::npos,"observation never consumes flag or traverses changed models");check(YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>()=="PRIVATE SOLDIER"&&YAML::Load(ui.visible)["rows"][0]["cells"][2].as<std::string>()=="PRIVATE BASE","rows retain captured UI not mutated models");
  for(const std::string action:{"acknowledge_promotions","close_commendations","unknown"}){AgentRequest q;q.action=action;check(!state.agentDispatch(q)&&game.pops==0&&first._recentlyPromoted,"neighbor and unknown actions refused without pop");}
  std::vector<Text*> fields={state._txtTitle,state._txtName,state._txtRank,state._txtBase};
  for(size_t i=0;i<fields.size();++i){Text*t=fields[i];const std::string original=t->getText();
   for(const std::string caption:{std::string(500,'W'),std::string("LINE\nLINE"),std::string("\xF0\x9F\x99\x82")}){
    t->setText(caption);check(!state.agentUiState(ui),"dirty logical caption refused beforepaint");state.blit();check(state.agentUiState(ui),"logical overflowing caption admitted afterpaint");auto node=YAML::Load(ui.visible);check((i?node["headings"][i-1]:node["title"]).as<std::string>()==caption,"exact current logical string regardless glyph fit");
   }
   SDL_FillRect(t->getSurface(),nullptr,0);check(state.agentUiState(ui),"erased glyph ink is not admission");
   t->setVisible(false);state.blit();check(state.agentUiState(ui),"hidden caption preserves readonly DTO");auto node=YAML::Load(ui.visible);check((i?node["headings"][i-1]:node["title"]).IsNull(),"hidden current caption null");
   t->setVisible(true);t->setText(original);state.blit();t->setVisible(false);t->setVisible(true);check(!state.agentUiState(ui),"visibility ABA stale");state.blit();
   int x=t->getX();t->setX(x+1);t->setX(x);check(!state.agentUiState(ui),"geometry ABA stale");state.blit();
  }
  game.cursorVisible=true;game.cursor={20,34,8,8};check(state.agentUiState(ui)&&!YAML::Load(ui.visible)["headings"][0].IsNull(),"cursor over heading not readability gate");game.cursorVisible=false;
  game.fpsVisible=true;game.fps={20,34,8,3};check(state.agentUiState(ui)&&YAML::Load(ui.visible)["headings"][0].IsNull(),"FPS actual retainedstrip occlusion withholds heading");game.fpsVisible=false;
  Surface cover(5,3,20,34);cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["headings"][0].IsNull(),"later retainedstrip overlay withholds heading");state._surfaces.pop_back();state.blit();
  Surface spare(5,1,20,40);spare.setPalette(palette);spare.clear(80);state._surfaces.push_back(&spare);state.blit();check(state.agentUiState(ui)&&!YAML::Load(ui.visible)["headings"][0].IsNull(),"sparebottom overlay outside fixed8px observation strip");state._surfaces.pop_back();state.blit();
  game.top=nullptr;check(!state.agentUiState(ui),"inactive state refusal");game.top=&state;state._modal=state._btnOk;check(!state.agentUiState(ui),"modal refusal");state._modal=nullptr;
  Surface other(320,200);other.setPalette(palette);game.screen.surface=&other;check(!state.agentUiState(ui),"wrong destination refusal");game.screen.surface=&canvas;
  SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"partial destination refusal");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restoration needs repaint");state.blit();
  check(state.agentUiState(ui)&&first._recentlyPromoted&&second._recentlyPromoted&&game.pops==0,"all reads no promotion flag consumption and no native pop");check(Fixture::modelReads==readsAfterCtor,"all observations avoid every trapped model getter");
  AgentRequest close;close.action="close_promotions";
  state._btnOk->setVisible(false);state.blit();check(state.agentUiState(ui)&&ui.actions.empty()&&YAML::Load(ui.visible)["buttons"].size()==0&&!YAML::Load(ui.visible)["title"].IsNull(),"hidden control preserves readonly captions without close authority");check(!state.agentDispatch(close)&&game.pops==0,"hidden OK cannot dispatch");state._btnOk->setVisible(true);state.blit();
  int buttonX=state._btnOk->getX();state._btnOk->setX(buttonX+1);check(!state.agentDispatch(close)&&game.pops==0,"moved button before repaint refuses");state._btnOk->setX(buttonX);check(!state.agentDispatch(close),"button geometry ABA stale");state.blit();
  state._btnOk->setVisible(false);state._btnOk->setVisible(true);check(!state.agentDispatch(close),"button visibility ABA stale");state.blit();
  Surface buttonCover(6,4,20,180);buttonCover.setPalette(palette);buttonCover.clear(80);state._surfaces.push_back(&buttonCover);state.blit();check(!state.agentDispatch(close)&&game.pops==0,"actual later overlay on OK refuses");state._surfaces.pop_back();state.blit();
  game.cursorVisible=true;game.cursor={20,180,6,4};check(!state.agentDispatch(close)&&game.pops==0,"cursor on actual control refuses close");game.cursorVisible=false;
  check(state.agentUiState(ui),"fresh close authority");AgentFrameCommand frame;check(frame.stage(close,ui,&state,1,1,100,100),"stage actual close");state._btnOk->setText("CURRENT OK LABEL");state.blit();AgentUiState changed;check(state.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&state,changed),"changed current label invalidates prior authority");
  check(state.agentDispatch(close)&&game.pops==1&&first._recentlyPromoted&&second._recentlyPromoted,"literal OK singlepop no promotion flag reconsumption");
  check(!state.agentDispatch(close)&&game.pops==1,"closed popup refuses repeated dispatch");check(Fixture::modelReads==readsAfterCtor,"native close never calls model getters");game.pops=0;
  state.release();
  Fixture::forbidModelRead=false;first._recentlyPromoted=true;second._recentlyPromoted=false;Fixture::PromotionsState single;game.top=&single;Fixture::forbidModelRead=true;single.blit();check(single.agentUiState(ui)&&single._lstSoldiers->getRows()==1&&YAML::Load(ui.visible)["rows"].size()==1,"literal single-promotion report before observation");single.release();
  Fixture::forbidModelRead=false;game.saved.bases.clear();Fixture::PromotionsState empty;Fixture::forbidModelRead=true;game.top=&empty;empty.blit();check(empty.agentUiState(ui)&&empty._lstSoldiers->getRows()==0&&YAML::Load(ui.visible).size()==4&&ui.actions.size()==1&&ui.offers("close_promotions"),"zero report same readonly heading shape");empty.release();Fixture::forbidModelRead=false;
        {
            Fixture::PromotionsState hidden;game.top=&hidden;Fixture::forbidModelRead=true;hidden._lstSoldiers->addRow(3,"NEVER PAINTED SECRET","OTHER","BASE");hidden._lstSoldiers->_texts[0][0]->setVisible(false);hidden.blit();AgentUiState ui;
            check(!hidden.agentUiState(ui),"never-painted dirty hidden cell conservatively refuses projection");AgentRequest q;q.action="unknown";int pops=game.pops;check(!hidden.agentDispatch(q)&&game.pops==pops&&ui.visible.find("NEVER PAINTED SECRET")==std::string::npos,"rejected hidden capture cannot expose secret or authorize action");hidden.release();Fixture::forbidModelRead=false;
        }
        // Standalone same-object native configuration: only opt-in changes between paints.
        {
            TextList list(288,128,8,40);list.setPalette(palette);list.initText(&big,&small,&language);list.setColumns(3,114,90,84);list.setSelectable(true);list.setBackground(&background);list.setMargin(8);
            list.addRow(3,"PAIRED NAME","PAIRED DECORATION","BASE");list.blit(&canvas);std::vector<AgentOutfittingRow> rows;
            check(!list.agentPromotionRows(rows)&&rows.empty(),"valid native three-column shape defaults opt-out");
            list.agentEnablePromotionCells();check(!list.agentPromotionRows(rows)&&rows.empty(),"enabling requires new native paint");list.blit(&canvas);
            check(list.agentPromotionRows(rows)&&rows.size()==1&&rows[0].cells.size()==3&&rows[0].cells[0].known&&rows[0].cells[1].known&&rows[0].cells[2].known&&rows[0].cells[2].text=="BASE"&&rows[0].cells[0].text=="PAIRED NAME"&&rows[0].cells[1].text=="PAIRED DECORATION","same object enable plus freshpaint exact cells");
        }
        {
            Font overlap;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"])if(f["id"].as<std::string>()=="FONT_SMALL"){f["spacing"]=-2;for(auto image:f["images"])image["spacing"]=-2;overlap.load(f);}
            TextList invalidSpacing(288,128,8,40);invalidSpacing.setPalette(palette);invalidSpacing.initText(&big,&overlap,&language);invalidSpacing.setColumns(3,114,90,84);invalidSpacing.setSelectable(true);invalidSpacing.setBackground(&background);invalidSpacing.setMargin(8);invalidSpacing.agentEnablePromotionCells();invalidSpacing.addRow(3,"ROW ONE","VALUE","BASE");invalidSpacing.addRow(3,"ROW TWO","VALUE","BASE");invalidSpacing.blit(&canvas);std::vector<AgentOutfittingRow> rows;
            check(overlap.getSpacing()<-1&&!invalidSpacing.agentPromotionRows(rows)&&rows.empty(),"more than native onepixel structural overlap refused without glyph analysis");
        }
        {
            TextList wrong(288,128,8,40);wrong.setPalette(palette);wrong.initText(&big,&small,&language);wrong.setColumns(2,204,84);wrong.setSelectable(true);wrong.setBackground(&background);wrong.setMargin(8);wrong.agentEnablePromotionCells();wrong.addRow(2,"WRONG","SHAPE");wrong.blit(&canvas);std::vector<AgentOutfittingRow> rows;check(!wrong.agentPromotionRows(rows)&&rows.empty(),"promotion helper rejects wrong two-column shape");
        }
        {
            TextList narrow(200,128,8,40);narrow.setPalette(palette);narrow.initText(&big,&small,&language);narrow.setColumns(3,114,90,84);narrow.setSelectable(true);narrow.setBackground(&background);narrow.setMargin(8);narrow.agentEnablePromotionCells();narrow.addRow(3,"VISIBLE PART","VISIBLE SECOND","WHOLLY OFFSCREEN SECRET");narrow.blit(&canvas);std::vector<AgentOutfittingRow> rows;
            check(narrow.agentPromotionRows(rows)&&rows.size()==1&&rows[0].cells[0].known&&!rows[0].cells[2].known&&rows[0].cells[2].text.empty(),"wholly horizontal offscreen final cell unavailable without hidden text");
        }
        {
            Fixture::PromotionsState state;game.top=&state;Fixture::forbidModelRead=true;auto*list=state._lstSoldiers;
            list->addRow(3,""," ","BASE");list->addRow(3,"VISIBLE FIRST","VISIBLE SECOND","BASE");list->addRow(3,"VISIBLE NEIGHBOUR","NEIGHBOUR DECORATION","BASE");
            for(int i=0;i<30;++i)list->addRow(3,("OFFSCREEN SENTINEL "+std::to_string(i)).c_str(),"PRIVATE DECORATION","BASE");
            state.blit();AgentUiState ui;check(state.agentUiState(ui),"viewport report with offscreen rows admitted");
            auto first=YAML::Load(ui.visible);check(first["rows"].size()<33&&ui.visible.find("OFFSCREEN SENTINEL 29")==std::string::npos,"far offscreen sentinel not enumerated");
            check(first["rows"][0]["cells"][0].as<std::string>()==""&&first["rows"][0]["cells"][1].as<std::string>()==" ","genuine empty and space preserved exactly");
            check(ui.rows.empty()&&ui.actions.size()==1&&ui.offers("close_promotions")&&!ui.offers("scroll_visible_list"),"no row handles or new scrolling action");
            auto*cell=list->_texts[1][0];cell->setText("HIDDEN CELL SENTINEL");list->invalidate();state.blit();check(state.agentUiState(ui),"sentinel first painted");
            cell->setVisible(false);check(!state.agentUiState(ui),"hidden afterpaint stale refusal");list->invalidate();state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][1]["cells"][0].IsNull()&&ui.visible.find("HIDDEN CELL SENTINEL")==std::string::npos,"fresh hidden cell null no cached sentinel");
            check(YAML::Load(ui.visible)["rows"][1]["cells"][1].as<std::string>()=="VISIBLE SECOND","hidden cell leaves other current cell unchanged");
            cell->setVisible(true);list->invalidate();state.blit();cell->setVisible(false);cell->setVisible(true);check(!state.agentUiState(ui),"individual visibility ABA stale");list->invalidate();state.blit();check(state.agentUiState(ui),"fresh ABA restore");
            int x=cell->getX();cell->setX(x+1);cell->setX(x);check(!state.agentUiState(ui),"individual geometry ABA stale");list->invalidate();state.blit();
            SDL_Rect localClip={0,0,1,1};SDL_SetClipRect(list->getSurface(),&localClip);state.blit();check(!state.agentUiState(ui),"local list clip rejects cached cell presentation");SDL_SetClipRect(list->getSurface(),nullptr);check(!state.agentUiState(ui),"restoring local clip alone does not restore stale capture");list->invalidate();state.blit();check(state.agentUiState(ui),"local clip restore with actual fresh draw");
            Text*laterCell=list->_texts[1][1];const int laterX=laterCell->getX();laterCell->setX(cell->getX());list->invalidate();state.blit();
            check(!state.agentUiState(ui),"later same-list cell overlaps previous cell after repaint: structural refusal");laterCell->setX(laterX);list->invalidate();state.blit();check(state.agentUiState(ui),"native cell allocation restored");
            for(const std::string caption:{std::string(500,'W'),std::string("LINE\nLINE"),std::string("\xF0\x9F\x99\x82")}){
                list->setCellText(1,0,caption);check(!state.agentUiState(ui),"changed cell dirty beforepaint");state.blit();check(state.agentUiState(ui),"fresh logical long multiline missingglyph capture");
                auto rows=YAML::Load(ui.visible)["rows"];check(rows[1]["cells"][0].as<std::string>()==caption&&rows[2]["cells"][0].as<std::string>()=="VISIBLE NEIGHBOUR","exact caption and following actual row retained");
                std::vector<AgentOutfittingRow> native;check(list->agentPromotionRows(native)&&native[2].logicalIndex==2&&native[2].bounds.y==list->getY()+list->_texts[2][0]->getY(),"neighbor bounds match actual native drawn row not inferred text grouping");
            }
            list->setCellText(1,0,std::string(1024,'X'));state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][1]["cells"][0].as<std::string>()==std::string(1024,'X'),"1024byte resource boundary exact not truncated");list->setCellText(1,0,std::string(1025,'X'));state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][1]["cells"][0].IsNull(),"overlength cell unavailable not truncated");list->setCellText(1,0,"VISIBLE FIRST");state.blit();
            SDL_FillRect(list->getSurface(),nullptr,0);check(state.agentUiState(ui),"erased list pixels do not remove logical projection");
            cell=list->_texts[1][0];SDL_FillRect(cell->getSurface(),nullptr,0);check(state.agentUiState(ui),"erased cell ink not gate");
            cell->setText("VISIBLE FIRST");list->invalidate();state.blit();
            Surface cover(5,5,list->getX()+cell->getX(),list->getY()+cell->getY());cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][1]["cells"][0].IsNull(),"later overlay cell withheld");state._surfaces.pop_back();state.blit();
            game.fpsVisible=true;game.fps={Sint16(list->getX()+cell->getX()),Sint16(list->getY()+cell->getY()),5,5};
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][1]["cells"][0].IsNull(),"FPS occlusion null current cell");game.fpsVisible=false;
            game.cursorVisible=true;game.cursor=game.fps;check(state.agentUiState(ui)&&!YAML::Load(ui.visible)["rows"][1]["cells"][0].IsNull(),"cosmetic cursor over cell not text gate");game.cursorVisible=false;
            list->scrollDown(false);check(!state.agentUiState(ui),"native scroll requires repaint");state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>()=="VISIBLE FIRST","native viewport shifted without public scroll action");
            check(ui.visible.find("OFFSCREEN SENTINEL 29")==std::string::npos,"scroll snapshot still not exhaustive");
            list->scrollUp(true);state.blit();list->setHeight(13);state.blit();std::vector<AgentOutfittingRow> clippedRows;
            check(list->agentPromotionRows(clippedRows),"short vertical viewport capture succeeds conservatively");
            for(const auto&r:clippedRows)check(r.bounds.y+r.bounds.h<=list->getY()+list->getHeight(),"no partially vertically clipped row exported");
            check(state.agentUiState(ui)&&ui.visible.find("VISIBLE NEIGHBOUR")==std::string::npos,"vertical clipped neighbor omitted");
            state.release();Fixture::forbidModelRead=false;
        }
 }
 std::cout<<"PASS "<<checks<<" promotions viewport/native privacy checks; explicit domain seams\n";
}
