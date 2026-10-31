// SPDX-License-Identifier: GPL-3.0-or-later
#include "Engine/Font.h"
#include "Mod/RuleInterface.h"
#include "Engine/Surface.h"
#include "Engine/Language.h"
#include "Engine/Options.h"
#include "Engine/State.h"
#include "Engine/Game.h"
#include "Engine/Sound.h"
#include "Interface/Text.h"
#include "Interface/ImageButton.h"
#include "Interface/TextButton.h"
#include "Interface/TextList.h"
#include "Interface/Window.h"
#include <typeinfo>
#include "Engine/Unicode.h"
#include "Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <map>
#include <set>
#include "Observer/AgentProtocol.h"
#include <vector>
#include <climits>
#include <stdexcept>
using namespace OpenXcom;using namespace OpenXcom::Observer;
static std::string resources;
namespace OpenXcom { namespace FileMap {const std::string&getFilePath(const std::string&r){static std::map<std::string,std::string>p;return p.emplace(r,resources+"/common/"+r).first->second;}}namespace Options {int maxFrameSkip=0,changeValueByMouseWheel=0;int mousewheelSpeed=3;int baseXResolution=320,baseYResolution=200;std::string language="en-US";TextWrapping wordwrap=WRAP_AUTO;bool debugUi=false;}}
unsigned checks=0;void check(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
namespace OpenXcom {Game*State::_game=nullptr;bool Game::isState(State*)const{throw std::runtime_error("unexpected timer game access");}void State::setModal(InteractiveSurface*){} void Sound::play(int,int,int)const{throw std::runtime_error("unexpected audio");}}
namespace OpenXcom {void State::hideAll(){throw std::runtime_error("unexpected popup hideAll");}void State::showAll(){throw std::runtime_error("unexpected popup showAll");}void State::toggleScreen(){throw std::runtime_error("unexpected popup toggleScreen");}bool State::isScreen()const{throw std::runtime_error("unexpected popup isScreen");}namespace RNG {int seedless(int,int){throw std::runtime_error("unexpected popup RNG");}}}
namespace Fixture {
struct Soldier{int id=1;bool dead=true;int getId(){return id;}void*getDeath(){return dead?this:nullptr;}};
struct Base{std::vector<Soldier*> soldiers;std::vector<Soldier*>*getSoldiers(){return &soldiers;}};
struct SavedGame{std::vector<Soldier*>dead;std::vector<Soldier*>*getDeadSoldiers(){return &dead;}};
struct Mod{std::vector<int>awards{1};const std::vector<int>&getCommendationsList(){return awards;}};
struct Screen{Surface canvas{320,200};Surface*getSurface(){return &canvas;}};
struct Game{Screen screen;const void*top=nullptr;Surface*_fpsCounter=nullptr;SavedGame save;Mod mod;Screen*getScreen(){return &screen;}bool isState(const void*p){return top==p;}SavedGame*getSavedGame(){return &save;}Mod*getMod(){return &mod;}Surface*getCursor(){return nullptr;}Surface*getFpsCounter(){return _fpsCounter;}bool agentOverlayClear(const AgentUiState&)const;bool agentNonCursorOverlayClear(const AgentUiState&)const;};
#include "Overlay.generated.inc"
struct State{Game*_game;void*_modal=nullptr;std::vector<Surface*>_surfaces;virtual void blit();void init(){}};
#include "Blit.generated.inc"
enum SoldierDiaryDisplay{DIARY_KILLS,DIARY_MISSIONS,DIARY_COMMENDATIONS};
struct SoldierDiaryPerformanceState:State{
Base*_base=nullptr;Soldier*_soldier=nullptr;std::vector<Soldier*>*_list=nullptr;size_t _soldierId=0;std::vector<std::pair<Soldier*,int>>_agentDeadRoster;size_t _agentDeadOrdinal=0;
TextButton *_btnOk,*_btnPrev,*_btnNext,*_btnKills,*_btnMissions,*_btnCommendations;Window*_window;Text*_txtTitle,*_txtMedalName,*_txtMedalLevel,*_txtMedalInfo;TextList*_lstPerformance,*_lstKillTotals,*_lstMissionTotals,*_lstCommendations;std::vector<Surface*>_commendations,_commendationDecorations;SoldierDiaryDisplay _display=DIARY_MISSIONS;int _lastScrollPos=0;bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;
bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit();bool agentDeadRosterValid()const;bool agentUiState(AgentUiState&)const;
void baseline(){
#include "InitBaseline.generated.inc"
}
void fixed(){
#include "InitFixed.generated.inc"
}
~SoldierDiaryPerformanceState(){for(auto*p:_surfaces)delete p;}
};
#include "Provider.generated.inc"
}
std::vector<unsigned char> pixels(Surface&s){auto*p=s.getSurface();check(SDL_LockSurface(p)==0,"lock");std::vector<unsigned char>r;for(int y=0;y<p->h;++y){auto*b=static_cast<unsigned char*>(p->pixels)+y*p->pitch;r.insert(r.end(),b,b+p->w);}SDL_UnlockSurface(p);return r;}
int main(int argc,char**argv){check(argc==2,"resource path");resources=argv[1];check(SDL_Init(0)==0,"SDL init0");check(!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO)&&!SDL_WasInit(SDL_INIT_AUDIO),"no devices");Font big,small;for(auto f:YAML::LoadFile(resources+"/common/Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);}Language lang;
for(int master=1;master<=2;++master)for(int memorial=0;memorial<2;++memorial)for(int mode=0;mode<2;++mode){auto language=YAML::LoadFile(resources+"/standard/xcom"+std::to_string(master)+"/Language/en-US.yml")["en-US"];auto common=YAML::LoadFile(resources+"/common/Language/en-US.yml")["en-US"];auto tr=[&](const char*k){return (language[k]?language[k]:common[k]).as<std::string>();};
Fixture::Game g;Fixture::Soldier soldier;Fixture::Base base;base.soldiers.push_back(&soldier);g.save.dead.push_back(&soldier);Fixture::SoldierDiaryPerformanceState a;auto paint=[&](){g.screen.canvas.clear();a.blit();};a._game=&g;g.top=&a;a._base=memorial?nullptr:&base;a._list=memorial?&g.save.dead:&base.soldiers;a._soldier=&soldier;a._agentDeadRoster.push_back({&soldier,1});a._display=mode?Fixture::DIARY_MISSIONS:Fixture::DIARY_KILLS;
a._window=new Window(nullptr,320,200,0,0,POPUP_NONE);a._surfaces.push_back(a._window);a._window->setColor(16);
auto button=[&](int w,int h,int x,int y,std::string label){auto*b=new TextButton(w,h,x,y);b->initText(&big,&small,&lang);b->setText(label);b->agentEnableNamedControl();a._surfaces.push_back(b);return b;};
a._btnOk=button(70,16,242,176,"OK");a._btnKills=button(70,16,8,176,tr("STR_COMBAT"));a._btnMissions=button(70,16,86,176,tr("STR_PERFORMANCE"));a._btnCommendations=button(70,16,164,176,tr("STR_AWARDS"));a._btnPrev=button(28,14,8,8,"<<");a._btnNext=button(28,14,284,8,">>");
auto text=[&](int w,int h,int x,int y,std::string s){auto*t=new Text(w,h,x,y);t->initText(&big,&small,&lang);t->setText(s);a._surfaces.push_back(t);return t;};
a._txtTitle=text(310,16,5,8,"Owner History Fixture");a._txtTitle->setBig();a._txtTitle->setAlign(ALIGN_CENTER);
auto list=[&](int w,int h,int x,int y){auto*l=new TextList(w,h,x,y);l->initText(&big,&small,&lang);a._surfaces.push_back(l);return l;};
a._lstPerformance=list(288,128,8,28);a._lstPerformance->setColumns(2,273,15);a._lstPerformance->setDot(true);a._lstPerformance->agentEnableReportCells(2,true,true);
a._lstKillTotals=list(302,9,8,164);a._lstKillTotals->setColumns(4,72,72,72,86);a._lstKillTotals->agentEnableReportCells(4,true,true);
a._lstMissionTotals=list(302,9,8,164);a._lstMissionTotals->setColumns(4,72,72,72,86);a._lstMissionTotals->agentEnableReportCells(4,false,true);
a._txtMedalName=text(120,18,16,36,tr("STR_MEDAL_NAME"));a._txtMedalLevel=text(120,18,186,36,tr("STR_MEDAL_DECOR_LEVEL"));a._txtMedalInfo=text(280,32,20,135,"Fixture award description");a._txtMedalInfo->setWordWrap(true);
a._lstCommendations=list(240,80,48,52);a._lstCommendations->setColumns(2,138,100);a._lstCommendations->setSelectable(true);a._lstCommendations->agentEnableReportCells(2,false,true);
for(int i=0;i<10;++i){auto*m=new Surface(31,8,16,52+8*i);auto*d=new Surface(31,8,16,52+8*i);a._commendations.push_back(m);a._commendationDecorations.push_back(d);a._surfaces.push_back(m);a._surfaces.push_back(d);}
std::vector<std::pair<std::string,std::string>>expected;auto add=[&](std::string s,std::string v){if(v.empty())a._lstPerformance->addRow(1,s.c_str());else a._lstPerformance->addRow(2,s.c_str(),v.c_str());expected.push_back({s,v});};
if(mode){add(tr("STR_MISSIONS_BY_LOCATION"),"");add(master==1?"Europe":"Caribbean","1");add(master==1?"North America":"South Atlantic","1");add("","");add(tr("STR_MISSIONS_BY_TYPE"),"");add(master==1?"Alien Terror":"Port Attack","1");add("","");add(tr("STR_MISSIONS_BY_UFO"),"");add(master==1?"Small Scout":"Cruiser","1");}else{add(tr("STR_NEUTRALIZATIONS_BY_RACE"),"");add(master==1?"Sectoid":"Aquatoid","1");add("","");add(tr("STR_NEUTRALIZATIONS_BY_RANK"),"");add("Medic","1");add("","");add(tr("STR_NEUTRALIZATIONS_BY_WEAPON"),"");add(master==1?"Rifle":"Jet Harpoon","1");}
a._lstKillTotals->addRow(3,"KILLS> 1","STUNS> 0","ACCURACY> 33%");a._lstMissionTotals->addRow(4,"MISSIONS> 2","WINS> 0","SCORE> -621","DAYS WOUNDED> 0");a._lstCommendations->addRow(2,"Fixture Award","Level 1");for(auto*s:a._surfaces)s->agentTrackPresentation();
// Stock masters do not expose Awards. Explicit fixture enables it only in Awards tests below.
g.mod.awards.clear();a.baseline();paint();AgentUiState before,after;check(a.agentUiState(before),"baseline provider");auto original=pixels(g.screen.canvas);auto old=YAML::Load(before.visible);unsigned missing=0;for(auto r:old["rows"])if(r["cells"][0].IsNull())++missing;check(missing>=2,"reproduced baseline lower captions withheld");
std::cout<<"XCOM"<<master<<" "<<(memorial?"MEMORIAL":"LIVING")<<" "<<(mode?"MISSIONS":"COMBAT")<<" BASELINE "<<before.visible<<"\n";
a.fixed();check(!a.agentUiState(after),"visibility change requires new paint");paint();check(a.agentUiState(after),"fixed provider");check(original==pixels(g.screen.canvas),"clear decoration hiding preserves full index raster");check(!before.sameBinding(after),"changed presentation invalidates old binding");auto fixed=YAML::Load(after.visible);check(fixed["rows"].size()==expected.size(),"all ordered rows incl headings/separators");for(size_t i=0;i<expected.size();++i){auto c=fixed["rows"][i]["cells"];check(!c[0].IsNull()&&c[0].as<std::string>()==expected[i].first,"exact ordered caption");check(expected[i].second.empty()?c[1].IsNull():(!c[1].IsNull()&&c[1].as<std::string>()==expected[i].second),"exact value/missing cell");}check(fixed["headings"][0].IsNull()&&fixed["headings"][1].IsNull(),"nonaward headings intentionally null");std::cout<<"XCOM"<<master<<" "<<(memorial?"MEMORIAL":"LIVING")<<" "<<(mode?"MISSIONS":"COMBAT")<<" FIXED "<<after.visible<<"\n";
// Actual foreign overlays remain strict; the fix only hides native inactive decorations.
a._commendations[0]->setVisible(true);a._commendations[0]->setPixel(0,0,99);paint();check(a.agentUiState(before),"visible medal provider");auto covered=YAML::Load(before.visible);check(covered["rows"].size()<expected.size()||covered["rows"][3]["cells"][0].IsNull(),"visible medal blocks caption");a.fixed();paint();
Surface fps(31,8,16,52);g._fpsCounter=&fps;check(a.agentUiState(before),"fps partial provider");check(before.visible!=after.visible,"FPS overlay still withholds");g._fpsCounter=nullptr;
const std::string originalCellText=a._lstPerformance->getCellText(1,0);a._lstPerformance->setCellText(1,0,"Changed");check(!a.agentUiState(before),"dirty cell rejects");paint();check(a.agentUiState(before)&&!after.sameBinding(before),"mutated cell binding");a._lstPerformance->setCellText(1,0,originalCellText);paint();check(a.agentUiState(before)&&!after.sameBinding(before),"ABA old binding invalid");
// Exercise Awards transition with explicitly synthetic award content and actual painted glyphs.
g.mod.awards.push_back(1);a._display=Fixture::DIARY_COMMENDATIONS;
// Restore unpatched constructor visibility invariant before the baseline Awards comparison.
for(auto*s:a._commendations)s->setVisible(true);for(auto*s:a._commendationDecorations)s->setVisible(true);a.baseline();a._commendations[0]->setPixel(2,2,99);a._commendationDecorations[0]->setPixel(3,3,100);paint();check(a.agentUiState(before),"baseline awards provider");auto awardsRaster=pixels(g.screen.canvas);a.fixed();a._commendations[0]->setPixel(2,2,99);a._commendationDecorations[0]->setPixel(3,3,100);paint();check(a.agentUiState(after),"fixed awards provider");check(awardsRaster==pixels(g.screen.canvas)&&before.visible==after.visible,"awards visible sprites/provider unchanged");for(auto*s:a._commendations)check(s->getVisible(),"awards medal visible");for(auto*s:a._commendationDecorations)check(s->getVisible(),"awards decoration visible");check(YAML::Load(after.visible)["mode"].as<std::string>()=="awards","awards mode");
g.mod.awards.clear();a._display=mode?Fixture::DIARY_MISSIONS:Fixture::DIARY_KILLS;a.fixed();check(!a.agentUiState(before),"mode transition stale paint rejected");paint();check(a.agentUiState(before),"back from awards");check(pixels(g.screen.canvas)==original,"return clears medal pixels and restores raster");
a._modal=&a;check(!a.agentUiState(after),"modal rejects");a._modal=nullptr;g.top=nullptr;check(!a.agentUiState(after),"wrongtop rejects");g.top=&a;SDL_Rect clip={0,0,319,199};SDL_SetClipRect(g.screen.canvas.getSurface(),&clip);paint();check(!a.agentUiState(after),"partial destination rejects");SDL_SetClipRect(g.screen.canvas.getSurface(),nullptr);paint();check(a.agentUiState(after),"restored destination");if(memorial){soldier.dead=false;check(!a.agentUiState(after),"dead roster identity required");soldier.dead=true;}else{base.soldiers.clear();check(!a.agentUiState(after),"living roster identity required");}
}
check(!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO)&&!SDL_WasInit(SDL_INIT_AUDIO),"no devices after");std::cout<<"PASS checks="<<checks<<" no SDL video/audio\n";}
