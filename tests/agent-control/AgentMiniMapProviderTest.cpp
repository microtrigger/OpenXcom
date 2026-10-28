// SPDX-License-Identifier: GPL-3.0-or-later
// Literal MiniMap draw/provider/dispatch methods, actual SCANG glyphs and widgets.
// Camera, tile selection, Game/Screen lifecycle and clock are explicit shims.
// Actual Controller consumes this provider through its State callback, not a
// hand-authored DTO. Software final buffer is copied offscreen, no real flip.
#define State AgentControllerTestState
#include "MiniMapControllerDependencies.generated.h"
#include "../../src/Observer/AgentController.h"
#undef State
#include "../../src/Observer/AgentMiniMapPresentation.h"
#include "../../src/Observer/AgentProjectileOutput.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/SurfaceSet.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/Cursor.h"
#include "../../src/Battlescape/Position.h"
#include <yaml-cpp/yaml.h>
#include <set>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>
using namespace OpenXcom;using namespace OpenXcom::Observer;
static unsigned checks=0;static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
namespace OpenXcom { namespace Observer { std::uint64_t agentTestTime=10000;bool diagnosticProfileQualified(){return true;} } }
static bool pendingInput=false;static Uint8 focus=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;
extern "C" Uint8 SDL_GetAppState(){return focus;}extern "C" SDLMod SDL_GetModState(){return KMOD_NONE;}extern "C" Uint8 SDL_GetMouseState(int*,int*){return 0;}extern "C" void SDL_PumpEvents(){}extern "C" int SDL_PeepEvents(SDL_Event*,int,SDL_eventaction,Uint32){return pendingInput?1:0;}
namespace MiniMapHarness {
using namespace OpenXcom;typedef Surface InteractiveSurface;
const int CELL_WIDTH=4,CELL_HEIGHT=4,MAX_FRAME=2;enum TilePart{O_FLOOR,O_WESTWALL,O_NORTHWALL,O_OBJECT};enum{FACTION_NEUTRAL=2};struct Pathfinding{enum{red=3};};
struct Armor{int getSize(){return 1;}};struct Unit{bool visible=true;Armor armor;Position position;bool getVisible(){return visible;}int getMiniMapSpriteIndex(){return 0;}Armor*getArmor(){return &armor;}Position getPosition(){return position;}int getFaction(){return 0;}};
struct MapData{int getMiniMapIndex(){return 1;}};
struct Tile{Position position;MapData data;bool discovered=true;Unit*unit=nullptr;std::vector<int>items;MapData*getMapData(TilePart p){return p==O_FLOOR?&data:nullptr;}bool isDiscovered(int){return discovered;}int getShade(){return 0;}Unit*getUnit(){return unit;}Position getPosition(){return position;}std::vector<int>*getInventory(){return &items;}};
struct SavedBattleGame{unsigned reads=0;std::vector<Tile>tiles;SavedBattleGame():tiles(40*40){for(int y=0;y<40;++y)for(int x=0;x<40;++x)tiles[y*40+x].position=Position(x,y,0);}Tile*getTile(Position p){++reads;return p.z||p.x<0||p.y<0||p.x>=40||p.y>=40?nullptr:&tiles[p.y*40+p.x];}};
struct Camera{Position center{20,20,0};std::uint64_t revision=1;unsigned centers=0;Position getCenterPosition(){return center;}int getViewLevel(){return center.z;}std::uint64_t agentViewRevision()const{return revision;}void setViewLevel(int z){++revision;center.z=std::max(0,std::min(1,z));}void centerOnPosition(Position p){++revision;++centers;center=Position(std::max(-1,std::min(40,p.x)),std::max(-1,std::min(40,p.y)),p.z);}};
struct Screen{Surface*_surface=nullptr;SDL_Surface*_screen=nullptr;bool _agentLastFlipOk=true,_pushPalette=false;int _baseWidth=320,_baseHeight=200,_numColors=0,_topBlackBand=0,_bottomBlackBand=0,_leftBlackBand=0,_rightBlackBand=0;bool useOpenGL()const{return false;}bool use32bitScaler()const{return false;}bool agentPresentationReady()const;bool agentCaptureOutput(const Surface*,AgentProjectileOutput&)const;void resetDisplay(bool){}static void updateScale(int,int&,int&,bool){}};
#include "MiniMapScreen.generated.inc"
struct Game{const void*top=nullptr;Screen screen;Surface*_cursor=nullptr,*_fpsCounter=nullptr;unsigned pops=0;bool isState(const void*s){return top==s;}Screen*getScreen(){return &screen;}void popState(){++pops;}bool agentOverlayClear(const AgentUiState&)const;};
#include "MiniMapOverlay.generated.inc"
class MiniMapView:public Surface{
public:Game*_game=nullptr;Camera*_camera=nullptr;SavedBattleGame*_battleGame=nullptr;SurfaceSet*_set=nullptr;int _frame=0;bool _isMouseScrolling=false;
 AgentMiniMapPresentation _agentPresentation;std::uint64_t _agentDrawCameraRevision=0;
 MiniMapView():Surface(221,148,48,16){}
 void draw();int up();int down();void activatePoint(int,int);void agentDrawGlyph(Surface*,AgentMiniMapKind,int,int);bool agentReady()const;std::uint64_t agentCameraRevision()const;
 const AgentMiniMapPresentation&agentPresentation()const{return _agentPresentation;}void agentActivatePoint(int x,int y){activatePoint(x,y);}
};
#include "MiniMapDraw.generated.inc"
class MiniMapState{
public:Game*_game=nullptr;MiniMapView*_miniMapView=nullptr;Surface*_btnLvlUp=nullptr,*_btnLvlDwn=nullptr,*_btnOk=nullptr,*_modal=nullptr;Text*_txtLevel=nullptr;std::vector<Surface*>_surfaces;
 LocalizedText tr(const std::string&){return LocalizedText("{0}");}
 bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void btnLevelUpClick(Action*);void btnLevelDownClick(Action*);void btnOkClick(Action*);
};
#include "MiniMapProvider.generated.inc"
}
static std::string token(unsigned n){std::ostringstream s;s<<std::hex<<std::setw(32)<<std::setfill('0')<<n;return s.str();}
static std::string field(const std::string&s,const std::string&key){auto p=s.find("\""+key+"\":");check(p!=std::string::npos,"packet field");p+=key.size()+3;if(s[p]=='"'){auto e=s.find('"',p+1);return s.substr(p+1,e-p-1);}return s.substr(p,s.find_first_of(",}",p)-p);}
struct Channel{
 AgentControllerTestState state;Game game;std::unique_ptr<AgentController>controller;int peer=-1;std::string epoch,last,request,prefix,pending;unsigned challenge=10,command=100;std::vector<std::string>schedule;
 Channel(const std::string&name):prefix(name){int f[2];check(!socketpair(AF_UNIX,SOCK_SEQPACKET,0,f),"anonymous sequence socket");peer=f[1];setenv("OPENXCOM_AGENT_CONTROL","1",1);setenv("OPENXCOM_AGENT_CONTROL_FD",std::to_string(f[0]).c_str(),1);unsetenv("OPENXCOM_STANCE_FD");game.top=&state;controller.reset(new AgentController(&game));auto hello=receive();epoch=field(hello,"epoch");save(prefix+"-hello.json",hello);send("H openxcom-agent-control/16");controller->poll();save(prefix+"-ready.json",receive());}
 ~Channel(){controller->close();if(peer>=0)::close(peer);std::string json="[";for(const auto&e:schedule){if(json.size()>1)json+=",";json+=e;}json+="]";save(prefix+"-schedule.json",json);}
 void send(const std::string&s){pending=s;check(::send(peer,s.data(),s.size(),0)==ssize_t(s.size()),"send packet");}
 std::string receive(){char b[65536];auto n=::recv(peer,b,sizeof(b),MSG_DONTWAIT);if(n<=0)return {};std::string value(b,n),name=prefix+"-wire-"+std::to_string(schedule.size())+".json";save(name,value);schedule.push_back("{\"request\":"+(pending.empty()?std::string("null"):agentJsonString(pending))+",\"response_file\":"+agentJsonString(name)+"}");pending.clear();return value;}
 void frame(){controller->beginFrame();controller->presented();}
 void save(const std::string&name,const std::string&data){const char*d=std::getenv("OPENXCOM_AGENT_TEST_PACKET_DIR");if(d)std::ofstream(std::string(d)+"/"+name)<<data;}
 std::string read(const std::string&name){send("R "+token(challenge++));controller->poll();frame();last=receive();check(!last.empty(),"real controller state packet");save(name,last);return last;}
 void queue(const std::string&action,const std::string&arg=""){request="E "+token(command++)+" 1 "+epoch+" "+field(last,"capability")+" "+field(last,"revision")+" "+action+(arg.empty()?"":" "+arg);send(request);controller->poll();check(receive().empty(),"E waits for frame");}
};
int main(int argc,char**argv){
 check(argc==2,"existing private asset root supplied");Logger::logFile()="/tmp/openxcom-minimap-widget.log";check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");Options::baseXResolution=320;Options::baseYResolution=200;Options::maximizeInfoScreens=false;
 Font big,small;for(const auto&n:YAML::LoadFile("Language/Font.dat")["fonts"]){if(n["id"].as<std::string>()=="FONT_BIG")big.load(n);if(n["id"].as<std::string>()=="FONT_SMALL")small.load(n);}Language language;
 for(const std::string master:{"UFO","TFTD"}){
  SurfaceSet glyphs(4,4);glyphs.loadDat(std::string(argv[1])+"/"+master+"/GEODATA/SCANG.DAT");
  Surface logical(320,200),bg(320,200),up(18,20,24,62),down(18,20,24,88),ok(32,32,275,145);Text level(28,16,281,75);level.initText(&big,&small,&language);level.setBig();level.setColor(40);level.setText("0");
  SDL_Color colors[256];for(int i=0;i<256;++i)colors[i]={Uint8(i),Uint8(i),Uint8(i),0};glyphs.setPalette(colors);for(Surface*s:{&logical,&bg,&up,&down,&ok,static_cast<Surface*>(&level)})s->setPalette(colors);bg.clear(1);up.clear(30);down.clear(30);ok.clear(30);SDL_SetColorKey(logical.getSurface(),0,0);
  SDL_Surface*final=SDL_CreateRGBSurface(0,640,400,8,0,0,0,0);check(final,"software final");SDL_SetColors(final,colors,0,256);
  MiniMapHarness::Game game;MiniMapHarness::Camera camera;MiniMapHarness::SavedBattleGame world;MiniMapHarness::Unit unit;unit.position=Position(12,12,0);world.tiles[12*40+12].unit=&unit;world.tiles[14*40+14].items.push_back(1);
  MiniMapHarness::MiniMapView view;view.setPalette(colors);view._game=&game;view._camera=&camera;view._battleGame=&world;view._set=&glyphs;
  MiniMapHarness::MiniMapState state;state._game=&game;state._miniMapView=&view;state._btnLvlUp=&up;state._btnLvlDwn=&down;state._btnOk=&ok;state._txtLevel=&level;state._surfaces={&bg,&view,&up,&down,&ok,&level};game.top=&state;game.screen._surface=&logical;game.screen._screen=final;
  auto draw=[&](){view.draw();for(auto*s:state._surfaces)s->blit(&logical);if(game._fpsCounter)game._fpsCounter->blit(&logical);if(game._cursor)game._cursor->blit(&logical);for(int y=0;y<200;++y)for(int x=0;x<320;++x)for(int yy=0;yy<2;++yy)for(int xx=0;xx<2;++xx)static_cast<Uint8*>(final->pixels)[(2*y+yy)*final->pitch+2*x+xx]=logical.getPixel(x,y);};
  AgentUiState ui;draw();const auto reads=world.reads;check(state.agentUiState(ui),"actual minimap provider admitted");check(world.reads==reads,"provider never traverses tiles/world");check(ui.visible.find("\"content_status\":\"bounded\"")!=std::string::npos,"stock ordinary SCANG semantic content");check(ui.visible.find("unit_marker")!=std::string::npos&&ui.visible.find("item_marker")!=std::string::npos,"stock visible unit and item glyphs preserved");check(ui.offers("minimap_center")&&ui.offers("close_minimap")&&ui.offers("view_level_up")&&ui.offers("view_level_down"),"four supported controls");check(agentRegionsDisjoint(ui)&&ui.points.size()>=9,"disjoint union and navigation targets");
  const auto firstPoint=ui.points.front();Surface late(4,4,view.getX()+firstPoint.first-2,view.getY()+firstPoint.second-2);late.setPalette(colors);late.clear(20);state._surfaces.push_back(&late);draw();check(state.agentUiState(ui)&&std::find(ui.points.begin(),ui.points.end(),firstPoint)==ui.points.end(),"later painted marker-cell overlay excludes its target");state._surfaces.pop_back();draw();check(state.agentUiState(ui),"uncovered content restores");
  for(auto&t:world.tiles)t.discovered=false;draw();check(state.agentUiState(ui)&&ui.visible.find("\"terrain\":[]")!=std::string::npos&&ui.visible.find("item_marker")==std::string::npos,"ordinary undiscovered shade16 gives no terrain contrast or item glyph");for(auto&t:world.tiles)t.discovered=true;draw();check(state.agentUiState(ui),"discovered ordinary content restores");
  view.getCrop()->w=100;check(state.agentUiState(ui)&&!ui.offers("minimap_center"),"cropped composite withholds map authority");view.getCrop()->w=0;draw();check(state.agentUiState(ui),"full viewport restores");
  auto original=ui;draw();check(state.agentUiState(ui)&&original.sameBinding(ui),"idle draw preserves authority");view._frame=1;draw();check(state.agentUiState(ui),"ordinary blink admitted");view._frame=0;draw();check(state.agentUiState(ui),"blink returns");
  Cursor cursor(9,13);cursor.setPalette(colors);cursor.setColor(200);cursor.setX(60);cursor.setY(30);game._cursor=&cursor;draw();check(state.agentUiState(ui)&&game.agentOverlayClear(ui),"covered cells omitted while remaining semantic content available");game._cursor=nullptr;
  Surface cover(221,148,48,16);cover.setPalette(colors);cover.clear(12);game._fpsCounter=&cover;draw();check(state.agentUiState(ui)&&ui.visible.find("\"content_status\":\"unavailable\"")!=std::string::npos&&!ui.offers("minimap_center"),"full map cover removes marks and targets but close survives");game._fpsCounter=nullptr;draw();
  game.screen._agentLastFlipOk=false;check(state.agentUiState(ui)&&!ui.offers("minimap_center"),"failed flip no semantic map target");game.screen._agentLastFlipOk=true;draw();
  state._modal=&ok;check(!state.agentUiState(ui),"modal denies provider");state._modal=nullptr;game.top=&world;check(!state.agentUiState(ui),"top-state denies provider");game.top=&state;
  game._cursor=&cursor;cursor.setX(276);cursor.setY(146);check(state.agentUiState(ui)&&!ui.offers("close_minimap"),"cursor covering close removes close only");cursor.setX(282);cursor.setY(76);draw();check(state.agentUiState(ui)&&ui.visible.find("\"level_label\":null")!=std::string::npos,"covered level ink withheld");game._cursor=nullptr;draw();
  view._isMouseScrolling=true;check(state.agentUiState(ui)&&!ui.offers("minimap_center"),"active drag no point authority");view._isMouseScrolling=false;draw();
  AgentRequest r;r.action="minimap_center";r.choice="m159";check(!state.agentDispatch(r),"unlisted marker rejected");r.choice="m0";check(state.agentDispatch(r)&&camera.centers==1,"ordinary point activation once");check(!view.agentReady(),"camera change invalidates old draw");draw();check(state.agentUiState(ui)&&!original.sameBinding(ui),"fresh camera state binds differently");
  r.action="view_level_up";check(state.agentDispatch(r)&&camera.center.z==1,"ordinary up invoked");draw();r.action="view_level_down";check(state.agentDispatch(r)&&camera.center.z==0,"ordinary down invoked");draw();
  {Channel channel(master);channel.state.project=[&](AgentUiState&o){return state.agentUiState(o);};channel.state.handler=[&](const AgentRequest&q){check(state.agentDispatch(q),"controller dispatch invokes real provider route");};
   auto packet=channel.read(master+"-state.json");check(packet.find("\"state\":\"minimap\"")!=std::string::npos,"real provider to real controller packet");channel.queue("minimap_center","m0");channel.frame();check(channel.state.calls==1&&channel.receive().empty(),"queue invokes exactly once, no preaction outcome");draw();channel.frame();auto receipt=channel.receive();check(receipt.find("\"status\":\"dispatched\"")!=std::string::npos,"later frame receipt");channel.save(master+"-result.json",receipt);channel.send(channel.request);channel.controller->poll();check(channel.receive()==receipt&&channel.state.calls==1,"duplicate never repeats minimap movement");
   channel.read(master+"-fresh.json");channel.send("V "+token(200)+" "+channel.epoch+" 0 16");channel.controller->poll();auto events=channel.receive();channel.save(master+"-events.json",events);check(events.find("\"kind\":\"presentation\"")!=std::string::npos,"independent retained minimap presentation");
   channel.queue("minimap_center","m0");camera.setViewLevel(1);draw();camera.setViewLevel(0);draw();channel.frame();auto refused=channel.receive();check(refused.find("\"status\":\"unavailable\"")!=std::string::npos&&channel.state.calls==1,"camera away/back rejects queued stale authority");
   channel.read(master+"-before-overlay.json");channel.queue("minimap_center","m0");AgentUiState beforeOverlay;check(state.agentUiState(beforeOverlay)&&!beforeOverlay.points.empty(),"fresh target for overlay fixture");game._cursor=&cursor;cursor.setX(view.getX()+beforeOverlay.points.front().first);cursor.setY(view.getY()+beforeOverlay.points.front().second);draw();channel.frame();refused=channel.receive();check(refused.find("\"status\":\"unavailable\"")!=std::string::npos&&channel.state.calls==1,"overlay movement between read and dispatch rejects changed admission");game._cursor=nullptr;draw();
   channel.read(master+"-after-aba.json");channel.queue("minimap_center","m0");unit.visible=false;draw();unit.visible=true;draw();channel.frame();refused=channel.receive();check(refused.find("\"status\":\"unavailable\"")!=std::string::npos&&channel.state.calls==1,"role away/back rejects queued stale authority");
  }
  SDL_FreeSurface(final);
 }
 std::cout<<"AgentMiniMapProviderTest "<<checks<<" PASS\n";
}
