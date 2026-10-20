// SPDX-License-Identifier: GPL-3.0-or-later
// Literal MiniBase draw and state providers, actual stock BASEBITS and widgets.
// Base/facility data, owner Game/Screen lifecycle and clock are explicit shims.
// Actual Controller consumes this provider through its State callback, not a
// hand-authored DTO. Software final buffer is copied offscreen, no real flip.
#define State AgentControllerTestState
#include "BaseControllerDependencies.generated.h"
#include "../../src/Observer/AgentController.h"
#undef State
#include "../../src/Basescape/MiniBaseView.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/TextEdit.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Sound.h"
#include "AgentBaseNavigationDependencies.h"
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

extern "C" int Mix_GroupAvailable(int){std::abort();}
namespace OpenXcom { void Sound::play(int,int,int) const {std::abort();} }
namespace OpenXcom { void State::setModal(InteractiveSurface *surface) { _modal=surface; } }
namespace BaseHarness {
using namespace OpenXcom;
struct Screen{Surface*_surface=nullptr;SDL_Surface*_screen=nullptr;bool _agentLastFlipOk=true,_pushPalette=false;int _baseWidth=320,_baseHeight=200,_numColors=0,_topBlackBand=0,_bottomBlackBand=0,_leftBlackBand=0,_rightBlackBand=0;bool useOpenGL()const{return false;}bool use32bitScaler()const{return false;}bool agentPresentationReady()const;bool agentCaptureOutput(const Surface*,AgentProjectileOutput&)const;void resetDisplay(bool){}static void updateScale(int,int&,int&,bool){}};
#include "BaseScreen.generated.inc"
struct Game{const void*top=nullptr;Screen screen;Surface*_cursor=nullptr,*_fpsCounter=nullptr;unsigned pops=0,pushes=0;struct Saved { std::vector<Base*> bases; std::vector<Base*>*getBases(){return &bases;} } saved; Saved*getSavedGame(){return &saved;} template<class T>void pushState(T*p){++pushes;delete p;}bool isState(const void*s){return top==s;}Screen*getScreen(){return &screen;}void popState(){++pops;}bool agentOverlayClear(const AgentUiState&)const;};
#include "BaseOverlay.generated.inc"

struct StoresState { explicit StoresState(Base*){} }; struct TransfersState { explicit TransfersState(Base*){} };
class BaseInfoState;
class BasescapeState {
public:Game*_game=nullptr;Base*_base=nullptr;MiniBaseView*_mini=nullptr;TextEdit*_edtBase=nullptr;Text*_txtLocation=nullptr,*_txtFunds=nullptr;TextButton*_btnBaseInfo=nullptr,*_btnGeoscape=nullptr;Surface*_modal=nullptr;std::vector<Surface*>_surfaces;
 bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void miniClick(Action*);void btnBaseInfoClick(Action*);void btnGeoscapeClick(Action*);
 void init(){_edtBase->setText(_base->name); for(unsigned i=0;i<_game->saved.bases.size();++i)if(_game->saved.bases[i]==_base)_mini->setSelectedBase(i);} void setBase(Base*b){_base=b;init();}
};
class BaseInfoState {
public:Game*_game=nullptr;Base*_base=nullptr;BasescapeState*_state=nullptr;MiniBaseView*_mini=nullptr;TextEdit*_edtBase=nullptr;Surface*_modal=nullptr;std::vector<Surface*>_surfaces;
 TextButton*_btnOk=nullptr,*_btnStores=nullptr,*_btnTransfers=nullptr;
 Text *_txtPersonnel=nullptr,*_txtSpace=nullptr;
 Text *_txtSoldiers=nullptr,*_numSoldiers=nullptr,*_txtEngineers=nullptr,*_numEngineers=nullptr,*_txtScientists=nullptr,*_numScientists=nullptr;
 Text *_txtQuarters=nullptr,*_numQuarters=nullptr,*_txtStores=nullptr,*_numStores=nullptr,*_txtLaboratories=nullptr,*_numLaboratories=nullptr,*_txtWorkshops=nullptr,*_numWorkshops=nullptr,*_txtContainment=nullptr,*_numContainment=nullptr,*_txtHangars=nullptr,*_numHangars=nullptr;
 Text *_txtDefense=nullptr,*_numDefense=nullptr,*_txtShortRange=nullptr,*_numShortRange=nullptr,*_txtLongRange=nullptr,*_numLongRange=nullptr;
 BaseInfoState(){} BaseInfoState(Base*b,BasescapeState*s):_base(b),_state(s){}
 bool agentUiState(AgentUiState&)const;bool agentDispatch(const AgentRequest&);void miniClick(Action*);void btnOkClick(Action*);void btnStoresClick(Action*);void btnTransfersClick(Action*);
 void init(){_edtBase->setText(_base->name);}
};
#include "BaseProviders.generated.inc"
}
static std::string token(unsigned n){std::ostringstream s;s<<std::hex<<std::setw(32)<<std::setfill('0')<<n;return s.str();}
static std::string field(const std::string&s,const std::string&key){auto p=s.find("\""+key+"\":");check(p!=std::string::npos,"packet field");p+=key.size()+3;if(s[p]=='"'){auto e=s.find('"',p+1);return s.substr(p+1,e-p-1);}return s.substr(p,s.find_first_of(",}",p)-p);}
struct Channel{
 AgentControllerTestState state;Game game;std::unique_ptr<AgentController>controller;int peer=-1;std::string epoch,last,request,prefix,pending;unsigned challenge=10,command=100;std::vector<std::string>schedule;
 Channel(const std::string&name):prefix(name){int f[2];check(!socketpair(AF_UNIX,SOCK_SEQPACKET,0,f),"anonymous sequence socket");peer=f[1];setenv("OPENXCOM_AGENT_CONTROL","1",1);setenv("OPENXCOM_AGENT_CONTROL_FD",std::to_string(f[0]).c_str(),1);unsetenv("OPENXCOM_STANCE_FD");game.top=&state;controller.reset(new AgentController(&game));auto hello=receive();epoch=field(hello,"epoch");save(prefix+"-hello.json",hello);send("H openxcom-agent-control/17");controller->poll();save(prefix+"-ready.json",receive());}
 ~Channel(){controller->close();if(peer>=0)::close(peer);std::string json="[";for(const auto&e:schedule){if(json.size()>1)json+=",";json+=e;}json+="]";save(prefix+"-schedule.json",json);}
 void send(const std::string&s){pending=s;check(::send(peer,s.data(),s.size(),0)==ssize_t(s.size()),"send packet");}
 std::string receive(){char b[65536];auto n=::recv(peer,b,sizeof(b),MSG_DONTWAIT);if(n<=0)return {};std::string value(b,n),name=prefix+"-wire-"+std::to_string(schedule.size())+".json";save(name,value);schedule.push_back("{\"request\":"+(pending.empty()?std::string("null"):agentJsonString(pending))+",\"response_file\":"+agentJsonString(name)+"}");pending.clear();return value;}
 void frame(){controller->beginFrame();controller->presented();}
 void save(const std::string&name,const std::string&data){const char*d=std::getenv("OPENXCOM_AGENT_TEST_PACKET_DIR");if(d)std::ofstream(std::string(d)+"/"+name)<<data;}
 std::string read(const std::string&name){send("R "+token(challenge++));controller->poll();frame();last=receive();check(!last.empty(),"real controller state packet");save(name,last);return last;}
 void queue(const std::string&action,const std::string&arg=""){request="E "+token(command++)+" "+epoch+" "+field(last,"capability")+" "+field(last,"revision")+" "+action+(arg.empty()?"":" "+arg);send(request);controller->poll();check(receive().empty(),"E waits for frame");}
};
int main(int argc,char**argv) {
 check(argc==2,"local asset root");Logger::logFile()="/tmp/openxcom-base-widget.log";check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen only");Options::baseXResolution=320;Options::baseYResolution=200;Options::storageLimitsEnforced=false;
 Font big,small;for(const auto&n:YAML::LoadFile("Language/Font.dat")["fonts"]){if(n["id"].as<std::string>()=="FONT_BIG")big.load(n);if(n["id"].as<std::string>()=="FONT_SMALL")small.load(n);}Language language;
 for(const std::string master:{"UFO","TFTD"}) {
  SurfaceSet tiles(32,40);tiles.loadPck(std::string(argv[1])+"/"+master+"/GEOGRAPH/BASEBITS.PCK",std::string(argv[1])+"/"+master+"/GEOGRAPH/BASEBITS.TAB");
  SDL_Color colors[256];for(int i=0;i<256;++i)colors[i]={Uint8(i),Uint8(i),Uint8(i),0};tiles.setPalette(colors);
  Surface logical(320,200),bg(320,200);logical.setPalette(colors);bg.setPalette(colors);bg.clear(9);SDL_SetColorKey(logical.getSurface(),0,0);
  SDL_Surface*final=SDL_CreateRGBSurface(0,640,400,8,0,0,0,0);check(final,"software final buffer");SDL_SetColors(final,colors,0,256);
  Base a("MCP Base"),b("MCP Base");BaseFacility facility;a.facilities.push_back(&facility);
  MiniBaseView mini(128,16,192,41);mini.setPalette(colors);mini.setTexture(&tiles);BaseHarness::Game game;game.saved.bases={&a,&b};mini.setBases(&game.saved.bases);mini.setColor(32);mini.setSecondaryColor(48);
  TextEdit name(nullptr,127,17,193,0);name.initText(&big,&small,&language);name.setBig();name.setColor(40);name.setText(a.name);name.setPalette(colors);
  Text location(126,9,194,16),funds(126,9,194,24);for(Text*t:{&location,&funds}){t->initText(&big,&small,&language);t->setColor(40);t->setPalette(colors);}location.setText("Europe");funds.setText("Funds $6,000,000");
  TextButton info(128,12,192,71),geo(128,12,192,188);for(TextButton*t:{&info,&geo}){t->initText(&big,&small,&language);t->setColor(16);t->setPalette(colors);}info.setText("BASE INFORMATION");geo.setText("GEOSCAPE");
  BaseHarness::BasescapeState state;state._game=&game;state._base=&a;state._mini=&mini;state._edtBase=&name;state._txtLocation=&location;state._txtFunds=&funds;state._btnBaseInfo=&info;state._btnGeoscape=&geo;state._surfaces={&bg,&mini,&name,&location,&funds,&info,&geo};game.top=&state;game.screen._surface=&logical;game.screen._screen=final;
  auto draw=[&](){for(Surface*s:state._surfaces)s->blit(&logical);if(game._cursor)game._cursor->blit(&logical);if(game._fpsCounter)game._fpsCounter->blit(&logical);for(int y=0;y<200;++y)for(int x=0;x<320;++x)for(int yy=0;yy<2;++yy)for(int xx=0;xx<2;++xx)static_cast<Uint8*>(final->pixels)[(2*y+yy)*final->pitch+2*x+xx]=logical.getPixel(x,y);};
  AgentUiState ui;draw();check(state.agentUiState(ui),"stock basescape provider");std::cerr<<master<<" initial "<<ui.visible<<'\n';
  check(ui.visible.find("\"base_name\":\"MCP Base\"")!=std::string::npos,"actual base name ink admitted");check(ui.rows.size()==8&&ui.offers("select_base"),"all eight painted slots admitted independent of existence");check(ui.visible.find("\"selected\":true")!=std::string::npos,"selected border contribution proved");check(ui.offers("open_base_info")&&ui.offers("return_geoscape"),"painted navigation");const auto reads=a.reads+b.reads;state.agentUiState(ui);check(a.reads+b.reads==reads,"provider no model traversal");
  auto initial=ui;draw();check(state.agentUiState(ui)&&initial.sameBinding(ui),"idle repaint stable");
  bg.clear(1);draw();check(state.agentUiState(ui)&&ui.visible.find("\"selected\":true")==std::string::npos,"same RGB background cannot prove selected highlight");bg.clear(9);draw();
  mini.setSelectedBase(1);check(state.agentUiState(ui)&&ui.rows.empty(),"dirty selection withholds all mini authority");mini.setSelectedBase(0);draw();state.agentUiState(initial);
  const auto oldPixel=mini.getPixel(2,2);mini.setPixel(2,2,Uint8(oldPixel+1));check(state.agentUiState(ui)&&ui.rows.size()==7,"post-draw mini mutation withholds affected slot");mini.setPixel(2,2,oldPixel);draw();
  Surface *frame=tiles.getFrame(41);const auto padding=frame->getPixel(20,20);frame->setPixel(20,20,9);mini.draw();draw();check(state.agentUiState(ui)&&ui.rows.empty(),"nontransparent clipped sprite padding refused");frame->setPixel(20,20,padding);mini.draw();draw();state.agentUiState(initial);

  AgentRequest req;req.action="select_base";req.choice="b7";check(state.agentDispatch(req)&&state._base==&a,"painted empty slot ordinary no-effect");req.choice="b1";check(state.agentDispatch(req)&&state._base==&b,"atomic shared hover and ordinary base callback");draw();check(state.agentUiState(ui)&&!initial.sameBinding(ui),"same-named base selection still fresh binding");req.choice="b0";state.agentDispatch(req);draw();check(state.agentUiState(ui)&&!initial.sameBinding(ui),"selection ABA invalidates");
  Cursor cursor(9,13);cursor.setPalette(colors);cursor.setColor(200);cursor.setX(193);cursor.setY(42);game._cursor=&cursor;draw();check(state.agentUiState(ui)&&ui.rows.size()==7&&ui.rows.front()==1&&game.agentOverlayClear(ui),"covered slot omitted compact handles preserve uncovered");cursor.setY(72);draw();check(state.agentUiState(ui)&&!ui.offers("open_base_info")&&ui.offers("return_geoscape"),"covered button only omitted");game._cursor=nullptr;draw();
  game.screen._agentLastFlipOk=false;check(state.agentUiState(ui)&&ui.rows.empty()&&!ui.offers("select_base"),"failed final flip withholds mini controls");game.screen._agentLastFlipOk=true;draw();
  Surface cover(128,16,192,41);cover.setPalette(colors);cover.clear(20);state._surfaces.push_back(&cover);draw();check(state.agentUiState(ui)&&ui.rows.empty()&&ui.offers("return_geoscape"),"full mini overlay no slots yet close survives");state._surfaces.pop_back();draw();
  mini.getCrop()->w=50;check(state.agentUiState(ui)&&ui.rows.empty(),"cropped mini no slots");mini.getCrop()->w=0;draw();
  state._modal=&info;check(!state.agentUiState(ui),"modal blocks");state._modal=nullptr;game.top=&b;check(!state.agentUiState(ui),"top state blocks");game.top=&state;
  req.choice="b8";check(!state.agentDispatch(req),"out of domain base handle");req.choice="b01";check(!state.agentDispatch(req),"noncanonical base handle");
  {Channel ch(master);ch.state.project=[&](AgentUiState&o){return state.agentUiState(o);};ch.state.handler=[&](const AgentRequest&q){check(state.agentDispatch(q),"controller actual base dispatch");};ch.read(master+"-state.json");ch.queue("select_base","b1");ch.frame();check(ch.state.calls==1&&ch.receive().empty(),"queued once needs later frame");draw();ch.frame();auto receipt=ch.receive();check(receipt.find("\"status\":\"dispatched\"")!=std::string::npos,"later state receipt");ch.send(ch.request);ch.controller->poll();check(ch.receive()==receipt&&ch.state.calls==1,"exact duplicate cached no reselect");
   ch.read(master+"-fresh.json");ch.send("V "+token(200)+" "+ch.epoch+" 0 16");ch.controller->poll();check(ch.receive().find("\"kind\":\"presentation\"")!=std::string::npos,"retained real provider state");ch.queue("select_base","b0");game._cursor=&cursor;cursor.setX(193);cursor.setY(42);draw();ch.frame();check(ch.receive().find("\"status\":\"unavailable\"")!=std::string::npos&&ch.state.calls==1,"overlay read to dispatch rejection");game._cursor=nullptr;draw();
   ch.read(master+"-before-aba.json");ch.queue("select_base","b0");mini.setSelectedBase(0);draw();mini.setSelectedBase(1);draw();ch.frame();check(ch.receive().find("\"status\":\"unavailable\"")!=std::string::npos&&ch.state.calls==1,"base selection away-back queue rejects");
  }
  // Same literal BaseInfo provider with actual labels/numbers and widgets.
  BaseHarness::BaseInfoState details;details._game=&game;details._base=&a;details._state=&state;details._mini=&mini;details._edtBase=&name;
  mini.setX(182);mini.setY(8);name.setX(8);name.setY(8);mini.setSelectedBase(0);name.setText(a.name);
  TextButton ok(30,14,10,180),stores(80,14,132,180),transfers(80,14,46,180);for(TextButton*t:{&ok,&stores,&transfers}){t->initText(&big,&small,&language);t->setColor(16);t->setPalette(colors);}ok.setText("OK");stores.setText("STORES");transfers.setText("TRANSFERS");details._btnOk=&ok;details._btnStores=&stores;details._btnTransfers=&transfers;
  std::vector<std::unique_ptr<Text>> text;
  auto make=[&](int w,int h,int x,int y,const std::string&s){text.emplace_back(new Text(w,h,x,y));Text*t=text.back().get();t->initText(&big,&small,&language);t->setPalette(colors);t->setColor(40);t->setText(s);return t;};
  details._txtPersonnel=make(300,9,8,30,"PERSONNEL AVAILABLE:PERSONNEL TOTAL");details._txtSpace=make(300,9,8,72,"SPACE USED:SPACE AVAILABLE");
  Text**ls[]={&details._txtSoldiers,&details._txtEngineers,&details._txtScientists,&details._txtQuarters,&details._txtStores,&details._txtLaboratories,&details._txtWorkshops,&details._txtHangars,&details._txtDefense,&details._txtShortRange,&details._txtLongRange};Text**vs[]={&details._numSoldiers,&details._numEngineers,&details._numScientists,&details._numQuarters,&details._numStores,&details._numLaboratories,&details._numWorkshops,&details._numHangars,&details._numDefense,&details._numShortRange,&details._numLongRange};int ys[]={41,51,61,83,93,103,113,123,138,153,163};
  for(unsigned i=0;i<11;++i){*ls[i]=make(114,9,8,ys[i],i==0?"Soldiers":"Category");*vs[i]=make(40,9,126,ys[i],"8:10");}
  details._surfaces={&bg,&mini,&ok,&transfers,&stores,&name};for(auto&t:text)details._surfaces.push_back(t.get());game.top=&details;auto old=state._surfaces;state._surfaces=details._surfaces;draw();check(details.agentUiState(ui),"actual BaseInfo provider");check(ui.visible.find("8:10")!=std::string::npos&&ui.rows.size()==8,"actual metric text plus mini slots");check(ui.offers("close_base_info")&&ui.offers("open_base_stores")&&ui.offers("open_base_transfers"),"three ordinary BaseInfo controls");
  req.action="open_base_stores";check(details.agentDispatch(req)&&game.pushes==1,"literal normal stores handler");req.action="open_base_transfers";check(details.agentDispatch(req)&&game.pushes==2,"literal normal transfers handler");req.action="close_base_info";check(details.agentDispatch(req)&&game.pops==1,"literal normal info OK");
  game._cursor=&cursor;cursor.setX(127);cursor.setY(42);draw();check(details.agentUiState(ui)&&ui.visible.find("\"value\":null")!=std::string::npos,"covered number null while category remains");game._cursor=nullptr;draw();
  {Channel ch(master+"-info");ch.state.project=[&](AgentUiState&o){return details.agentUiState(o);};ch.read(master+"-info-state.json");ch.send("V "+token(201)+" "+ch.epoch+" 0 16");ch.controller->poll();check(!ch.receive().empty(),"BaseInfo actual provider events packet");}
  state._surfaces=old;SDL_FreeSurface(final);
 }
 std::cout<<"AgentBaseNavigationProviderTest "<<checks<<" PASS\n";
}
