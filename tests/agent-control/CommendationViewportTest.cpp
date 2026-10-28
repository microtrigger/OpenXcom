// SPDX-License-Identifier: GPL-3.0-or-later
// Literal constructor/provider/OK; explicit Game/Mod/Soldier/Diary/RuleCommendations seams.
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
struct RuleCommendations { std::map<std::string,std::vector<int>> criteria={{"criterion",{1,2}}}; const std::map<std::string,std::vector<int>> *getCriteria(){return &criteria;} };
struct SoldierCommendations { std::string type="STR_PRIVATE_MEDAL",noun="noNoun"; bool fresh=true; int oldCalls=0; const std::string &getType(){return type;} bool isNew(){return fresh;} void makeOld(){++oldCalls;fresh=false;} const std::string &getNoun(){return noun;} int getDecorationLevelInt(){return 0;} std::string getDecorationLevelName(int){return "STR_PRIVATE_LEVEL";} };
struct SoldierDiary { std::vector<SoldierCommendations*> medals; std::vector<SoldierCommendations*> *getSoldierCommendations(){return &medals;} };
struct Soldier { std::string name="PRIVATE SOLDIER NAME"; SoldierDiary diary; SoldierDiary *getDiary(){return &diary;} const std::string &getName(){return name;} };
struct Mod { Surface *background; std::map<std::string,RuleCommendations*> rules; Surface *getSurface(const std::string&){return background;} const std::map<std::string,RuleCommendations*> &getCommendationsList(){return rules;} };
struct Game {
    Screen screen; Mod mod; Language *language; Font *big, *small; SDL_Color *palette;
    const void *top = nullptr; bool clear = true; int pops = 0;
    Screen *getScreen(){return &screen;} Mod *getMod(){return &mod;}
    Language *getLanguage(){return language;}
    bool isState(const void *state) const{return top == state;}
    bool cursorVisible=false; SDL_Rect cursor={0,0,0,0};
    bool fpsVisible=false;SDL_Rect fps={0,0,0,0};
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
struct CommendationState : State {
    TextList *_lstSoldiers;
    TextButton *_btnOk;
    Window *_window;
    Text *_txtTitle;
    bool _agentDestinationPaintFull = false;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    CommendationState(std::vector<Soldier*>);
    ~CommendationState();
    void btnOkClick(Action*);
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> agentPaintRevisions() const;
    void blit() override;
    bool agentUiState(AgentUiState&) const override;
    bool agentDispatch(const AgentRequest&) override;
    void finishPopup(){_window->think();for(int i=0;i<30;++i)_window->popup();}
    void release(){for(auto *surface:_surfaces)delete surface;_surfaces.clear();}
};
#include "Commendation.generated.inc"
}
static unsigned checks = 0;
static void check(bool ok,const char *message){
    ++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}
}
int main(int argc,char **argv){
    check(argc==3,"source and isolated native log arguments");
    Logger::logFile()=argv[2];
    check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");
    FileMap::load("common","./",false);
    Options::baseXResolution=320;Options::baseYResolution=200;
    Font big,small,overlap;
    for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){
        if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);
        if(f["id"].as<std::string>()=="FONT_SMALL"){
            small.load(f);auto altered=YAML::Clone(f);altered["spacing"]=-2;
            for(auto image:altered["images"])image["spacing"]=-2;
            overlap.load(altered);
        }
    }
    Surface canvas(320,200),background(320,200);SDL_Color palette[256];
    for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};
    canvas.setPalette(palette);background.setPalette(palette);background.clear(40);
    AgentRequest request;
    const std::string token(32,'1'),prefix="E "+token+" 1 "+token+" "+token+" 1 ";
    for(const std::string action:{"close_commendations"}){
        check(parseAgentRequest(prefix+action,request),"empty argument protocol accepted");
        for(const std::string extra:{" extra"," r0"," true"," 0","\n"})
            check(!parseAgentRequest(prefix+action+extra,request),"nonempty argument rejected");
    }
    for(const std::string gameName:{"xcom1","xcom2"}){
        Language language;language.loadFile(std::string(argv[1])+"/bin/standard/"+gameName+"/Language/en-US.yml");
        Fixture::Game game;game.screen.surface=&canvas;game.mod.background=&background;
        game.language=&language;game.big=&big;game.small=&small;game.palette=palette;
        Fixture::currentGame=&game;
        {
            const std::string message=language.getString("STR_MEDALS");
            Fixture::RuleCommendations rule;Fixture::SoldierCommendations medal;Fixture::Soldier soldier;soldier.diary.medals.push_back(&medal);
            game.mod.rules[medal.type]=&rule;
            Fixture::CommendationState state({&soldier});
            check(medal.oldCalls==1&&!medal.fresh,"literal constructor marks displayed medal old before observation");game.top=&state;
            AgentUiState ui;
            check(!state.agentUiState(ui),"constructor prepaint denied");
            state.blit();
            state.finishPopup();state.blit();
            if(!state.agentUiState(ui)) { auto*l=state._lstSoldiers;std::cerr<<"DIAGNOSTIC initial_list spacing="<<l->_font->getSpacing()<<" height="<<l->_font->getHeight()<<" capture_healthy="<<l->_agentCommendationCaptureHealthy<<" captured="<<l->_agentPaintCommendationRows.size()<<" list_painted="<<l->agentPresentationPaintedTo(&canvas)<<" list_revision_match="<<(l->_agentPaintRevision==l->_agentListRevision)<<" destination_full="<<state._agentDestinationPaintFull<<"\n"; }
            check(state.agentUiState(ui)&&ui.screen=="commendations","actual constructor popup admitted");
            auto visible=YAML::Load(ui.visible);
            check(visible.size()==3&&visible["title"].as<std::string>()==message,"only current UI captions projected");
            check(ui.actions.size()==1&&ui.offers("close_commendations"),"one explicit acknowledgement");
            check(visible["rows"].size()==2&&visible["rows"][0]["cells"][1].as<std::string>()==""&&visible["rows"][1]["cells"][0].as<std::string>()=="   PRIVATE SOLDIER NAME","native heading empty decoration and exact soldier indentation");
            for(auto row:visible["rows"])check(row.size()==1&&row["cells"].size()==2,"public rows expose cells only no index selection or count");
            std::vector<AgentOutfittingRow> nativeRows;check(state._lstSoldiers->agentCommendationRows(nativeRows)&&nativeRows.size()==2,"actual two cell capture");
            check(state._lstSoldiers->_font->getHeight()==9&&state._lstSoldiers->_font->getSpacing()==-1&&nativeRows[1].bounds.h==8,"stock9pixel allocation minus1 spacing yields native8pixel observation strip");
            check(state._lstSoldiers->_texts[1][1]->getX()==212&&state._lstSoldiers->_texts[1][1]->getWidth()==84&&nativeRows[1].cells[1].known,"native second column allocation eight pixels outside still known");
            check(nativeRows[1].cells[1].regions.size()==1&&nativeRows[1].cells[1].regions[0].w==76,"second cell observation intersects visible76px strip");

            check(visible["buttons"][0]["id"].as<std::string>()=="ok"&&visible["buttons"].size()==1,"ordinary button order");
            soldier.name="UNDISPLAYED CHANGED SOLDIER";medal.type="UNDISPLAYED CHANGED MEDAL";check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==message,"projection reads current UI text not constructor identifier model");check(YAML::Load(ui.visible)["rows"][1]["cells"][0].as<std::string>()=="   PRIVATE SOLDIER NAME"&&ui.visible.find("UNDISPLAYED CHANGED")==std::string::npos,"rows remain current painted strings not mutated soldier or medal model");
            request.action="acknowledge_craft_error";check(!state.agentDispatch(request)&&game.pops==0,"neighbour action rejected");
            request.action="unknown";check(!state.agentDispatch(request)&&game.pops==0,"unknown action no mutation");
            game.cursorVisible=true;game.cursor={49,10,8,8};
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==message&&ui.actions.size()==1,"pointer over caption does not gate logical text or choices");
            game.cursor={100,180,8,8};request.action="close_commendations";
            check((!state.agentUiState(ui)||!ui.offers(request.action))&&!state.agentDispatch(request)&&game.pops==0,"pointer on sole control refuses dispatch");game.cursorVisible=false;
            Surface otherCanvas(320,200);otherCanvas.setPalette(palette);game.screen.surface=&otherCanvas;
            check(!state.agentUiState(ui),"different destination without repaint rejected");game.screen.surface=&canvas;
            SDL_FillRect(state._txtTitle->getSurface(),nullptr,0);
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==message,"erased glyph pixels do not gate active logical message");
            state._txtTitle->setText("\xF0\x9F\x99\x82");state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()=="\xF0\x9F\x99\x82","missing glyph not admission");state._txtTitle->setText(message);state.blit();
            for(Surface *widget:std::vector<Surface *>{state._txtTitle,state._btnOk}){
                int x=widget->getX();widget->setX(x+1);widget->setX(x);
                check(!state.agentUiState(ui),"geometry ABA without repaint denied");
                request.action="close_commendations";check(!state.agentDispatch(request),"stale dispatch no mutation");
                state.blit();check(state.agentUiState(ui),"geometry repaint restores");
                widget->setVisible(false);widget->setVisible(true);
                check(!state.agentUiState(ui),"visibility ABA stale");state.blit();
            }
            state._txtTitle->setText("Changed caption");check(!state.agentUiState(ui),"text mutation stale");state.blit();check(state.agentUiState(ui),"new caption repaint");
            state._txtTitle->setVisible(false);state.blit();check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull(),"hidden message is null");
            state._txtTitle->setVisible(true);state._txtTitle->setText(message);state.blit();
            state._btnOk->setVisible(false);state.blit();request.action="close_commendations";
            check(!state.agentDispatch(request),"hidden acknowledgement denied");state._btnOk->setVisible(true);state.blit();
            state._modal=state._btnOk;check(!state.agentUiState(ui),"local modal denied");state._modal=nullptr;
            game.top=nullptr;check(!state.agentUiState(ui),"wrong top denied");game.top=&state;
            game.clear=false;check(!state.agentUiState(ui),"external overlay denied");game.clear=true;
            SDL_Rect clip={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clip);state.blit();check(!state.agentUiState(ui),"clipped paint denied");
            SDL_SetClipRect(canvas.getSurface(),nullptr);check(!state.agentUiState(ui),"clip restore needs repaint");state.blit();
            state.paintHook=[&](){SDL_SetClipRect(canvas.getSurface(),&clip);};state.blit();check(!state.agentUiState(ui),"clip during paint denied");
            state.paintHook=nullptr;SDL_SetClipRect(canvas.getSurface(),nullptr);state.blit();
            Surface cover(10,10,state._txtTitle->getX(),state._txtTitle->getY());cover.setPalette(palette);cover.clear(80);state._surfaces.push_back(&cover);state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].IsNull(),"occluded message withheld");state._surfaces.pop_back();state.blit();
            check(state.agentUiState(ui),"authority baseline");AgentFrameCommand frame;request.action="close_commendations";
            check(frame.stage(request,ui,&state,1,1,100,100),"stage public choice");AgentUiState changed;
            check(ui.actor==&state,"actor is current popup not hidden model");
            Fixture::CommendationState other({});game.top=&other;other.finishPopup();other.blit();
            check(other.agentUiState(changed)&&!frame.matchesFrame(101,1,1,&other,changed),"different popup instance rejects prior authority");check(YAML::Load(changed.visible)["rows"].size()==1&&YAML::Load(changed.visible)["rows"][0]["cells"][0].as<std::string>()==""&&YAML::Load(changed.visible)["rows"][0]["cells"][1].as<std::string>()=="","literal constructor unmatched rule leaves genuine blank placeholder");other.release();game.top=&state;state.blit();
            state._txtTitle->initText(&big,&overlap,&language);state._txtTitle->setSmall();
            const std::string overlapping="OVERLAP\nOVERLAP";
            state._txtTitle->setText(overlapping);state.blit();
            check(overlap.getHeight()+overlap.getSpacing()<overlap.getHeight()&&state._txtTitle->getTextWidth()>0,"synthetic overlap retains valid horizontal layout");
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==overlapping,"overlapping glyph lines do not gate logical UI text");
            state._txtTitle->initText(&big,&small,&language);state._txtTitle->setSmall();state._txtTitle->setText(message);state.blit();
            state._txtTitle->setText(std::string(500,'W'));state.blit();
            check(state.agentUiState(ui)&&YAML::Load(ui.visible)["title"].as<std::string>()==std::string(500,'W'),"long logical message remains exported without layout gate");
            state._txtTitle->setText(message);state.blit();
            const std::string originalCaption=state._btnOk->getText();
            state._btnOk->setText(std::string(500,'W'));state.blit();request.action="close_commendations";
            check(state.agentUiState(ui)&&ui.offers(request.action),"long logical button label not a readability gate");
            state._btnOk->setText(originalCaption);state.blit();
            request.action="close_commendations";
            check(medal.oldCalls==1,"all provider observations leave ctor medal mutation count unchanged");
            check(state.agentDispatch(request)&&game.pops==1,"OK only pops once");
            check(medal.oldCalls==1&&soldier.name=="UNDISPLAYED CHANGED SOLDIER"&&medal.type=="UNDISPLAYED CHANGED MEDAL","OK does not mutate diary or soldier model");
            check(!state.agentDispatch(request)&&game.pops==1,"no replay after popup closed");
            game.pops=0;state.release();
            game.mod.rules.clear();Fixture::CommendationState empty({});game.top=&empty;empty.finishPopup();empty.blit();
            check(empty.agentUiState(ui)&&YAML::Load(ui.visible).size()==3&&YAML::Load(ui.visible)["title"].as<std::string>()==message,"zero report exports same title and buttons only");
            check(ui.actions.size()==1&&ui.offers("close_commendations")&&YAML::Load(ui.visible)["rows"].size()==0,"no diary criteria or hidden model IDs exposed");
            empty.release();
        }

        {
            Fixture::CommendationState hidden({});game.top=&hidden;hidden._lstSoldiers->addRow(2,"NEVER PAINTED SECRET","OTHER");hidden._lstSoldiers->_texts[0][0]->setVisible(false);hidden.finishPopup();hidden.blit();AgentUiState ui;
            check(!hidden.agentUiState(ui),"never-painted dirty hidden cell conservatively refuses projection");AgentRequest q;q.action="close_commendations";int pops=game.pops;check(!hidden.agentDispatch(q)&&game.pops==pops&&ui.visible.find("NEVER PAINTED SECRET")==std::string::npos,"rejected hidden capture cannot expose secret or authorize action");hidden.release();
        }
        // Standalone same-object native configuration: only opt-in changes between paints.
        {
            TextList list(288,128,8,32);list.setPalette(palette);list.initText(&big,&small,&language);list.setColumns(2,204,84);list.setSelectable(true);list.setBackground(&background);list.setMargin(8);
            list.addRow(2,"PAIRED NAME","PAIRED DECORATION");list.blit(&canvas);std::vector<AgentOutfittingRow> rows;
            check(!list.agentCommendationRows(rows)&&rows.empty(),"valid native two-column shape defaults opt-out");
            list.agentEnableCommendationCells();check(!list.agentCommendationRows(rows)&&rows.empty(),"enabling requires new native paint");list.blit(&canvas);
            check(list.agentCommendationRows(rows)&&rows.size()==1&&rows[0].cells.size()==2&&rows[0].cells[0].known&&rows[0].cells[1].known&&rows[0].cells[0].text=="PAIRED NAME"&&rows[0].cells[1].text=="PAIRED DECORATION","same object enable plus freshpaint exact cells");
        }
        {
            TextList invalidSpacing(288,128,8,32);invalidSpacing.setPalette(palette);invalidSpacing.initText(&big,&overlap,&language);invalidSpacing.setColumns(2,204,84);invalidSpacing.setSelectable(true);invalidSpacing.setBackground(&background);invalidSpacing.setMargin(8);invalidSpacing.agentEnableCommendationCells();invalidSpacing.addRow(2,"ROW ONE","VALUE");invalidSpacing.addRow(2,"ROW TWO","VALUE");invalidSpacing.blit(&canvas);std::vector<AgentOutfittingRow> rows;
            check(overlap.getSpacing()<-1&&!invalidSpacing.agentCommendationRows(rows)&&rows.empty(),"more than native onepixel structural overlap refused without glyph analysis");
        }
        {
            TextList narrow(200,128,8,32);narrow.setPalette(palette);narrow.initText(&big,&small,&language);narrow.setColumns(2,204,84);narrow.setSelectable(true);narrow.setBackground(&background);narrow.setMargin(8);narrow.agentEnableCommendationCells();narrow.addRow(2,"VISIBLE PART","WHOLLY OFFSCREEN SECRET");narrow.blit(&canvas);std::vector<AgentOutfittingRow> rows;
            check(narrow.agentCommendationRows(rows)&&rows.size()==1&&rows[0].cells[0].known&&!rows[0].cells[1].known&&rows[0].cells[1].text.empty(),"wholly horizontal offscreen cell unavailable without hidden text");
        }
        {
            Fixture::CommendationState state({});game.top=&state;auto*list=state._lstSoldiers;
            list->addRow(2,""," ");list->addRow(2,"VISIBLE FIRST","VISIBLE SECOND");list->addRow(2,"VISIBLE NEIGHBOUR","NEIGHBOUR DECORATION");
            for(int i=0;i<30;++i)list->addRow(2,("OFFSCREEN SENTINEL "+std::to_string(i)).c_str(),"PRIVATE DECORATION");
            state.finishPopup();state.blit();AgentUiState ui;check(state.agentUiState(ui),"viewport report with offscreen rows admitted");
            auto first=YAML::Load(ui.visible);check(first["rows"].size()<33&&ui.visible.find("OFFSCREEN SENTINEL 29")==std::string::npos,"far offscreen sentinel not enumerated");
            check(first["rows"][0]["cells"][0].as<std::string>()==""&&first["rows"][0]["cells"][1].as<std::string>()==" ","genuine empty and space preserved exactly");
            check(ui.rows.empty()&&ui.actions.size()==1&&ui.offers("close_commendations")&&!ui.offers("scroll_visible_list"),"no row handles or new scrolling action");
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
                std::vector<AgentOutfittingRow> native;check(list->agentCommendationRows(native)&&native[2].logicalIndex==2&&native[2].bounds.y==list->getY()+list->_texts[2][0]->getY(),"neighbor bounds match actual native drawn row not inferred text grouping");
            }
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
            check(list->agentCommendationRows(clippedRows),"short vertical viewport capture succeeds conservatively");
            for(const auto&r:clippedRows)check(r.bounds.y+r.bounds.h<=list->getY()+list->getHeight(),"no partially vertically clipped row exported");
            check(state.agentUiState(ui)&&ui.visible.find("VISIBLE NEIGHBOUR")==std::string::npos,"vertical clipped neighbor omitted");
            state.release();
        }
    }
    check(!SDL_GetVideoSurface(),"no game video opened");
    std::cout<<"PASS "<<checks<<" commendations viewport/native privacy checks; explicit domain seams\n";
}
