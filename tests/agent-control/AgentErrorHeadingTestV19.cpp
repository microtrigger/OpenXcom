// SPDX-License-Identifier: GPL-3.0-or-later
// Offscreen stock-font native evidence. Actual ErrorMessage provider/dispatch,
// ordinary dismiss callback and Text/Button widgets; explicit owner/screen seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Palette.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Timer.h"
#include "../../src/Engine/Unicode.h"
#define private public
#include "../../src/Interface/Text.h"
#undef private
#include "../../src/Interface/TextButton.h"
#define private public
#include "../../src/Interface/TextList.h"
#undef private
#include "../../src/Interface/ComboBox.h"
#include "../../src/Interface/ArrowButton.h"
#include "../../src/Interface/ScrollBar.h"
#include "../../src/Savegame/QuantityCommand.h"
#include "../../src/fmath.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include "AgentTableControllerDependencies.generated.h"
#include "AgentTableController.generated.h"
using namespace OpenXcom; using namespace OpenXcom::Observer;
namespace OpenXcom { namespace Observer { std::uint64_t agentTestTime=10000; bool diagnosticProfileQualified(){return true;} } }
static Uint32 ticks=10000; static SDLMod mods=KMOD_NONE; static Uint8 appState=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS;
extern "C" Uint32 SDL_GetTicks(){return ticks;}
extern "C" Uint8 SDL_GetAppState(){return appState;}
extern "C" SDLMod SDL_GetModState(){return mods;}
extern "C" Uint8 SDL_GetMouseState(int*,int*){return 0;}
extern "C" void SDL_PumpEvents(){}
extern "C" int SDL_PeepEvents(SDL_Event*,int,SDL_eventaction,Uint32){return 0;}
namespace { unsigned checks=0; void check(bool value,const char *why){++checks;if(!value){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}} }
static Surface *errorCanvas=nullptr;
namespace OpenXcom {
Game *State::_game=nullptr;
State::State():_screen(true),_modal(nullptr),_ruleInterface(nullptr),_ruleInterfaceParent(nullptr){}
State::~State(){}
void State::init(){} void State::handle(Action*){} void State::blit(){for(Surface *p:_surfaces)p->blit(errorCanvas);} void State::resize(int&,int&){} void State::recenter(int,int){}
#include "PurchaseStateBase.generated.inc"
#include "PurchaseGameIdentity.generated.inc"
}

namespace Errors {
struct Screen { Surface *canvas=nullptr; Surface*getSurface(){return canvas;} };
struct Game {Screen screen; Screen*getScreen()const{return const_cast<Screen*>(&screen);}const void*top=nullptr;Surface *_cursor=nullptr,*_fpsCounter=nullptr;unsigned pops=0;
 bool isState(const void*p)const{return top==p;}void popState(){++pops;top=nullptr;}
 bool agentOverlayClear(const AgentUiState&)const;};
#include "ErrorOverlay.generated.inc"
struct ErrorMessageState:OpenXcom::State {
 using State::_surfaces;using State::_modal;
 Game *_game;TextButton *_btnOk;Text *_txtMessage;bool _agentDestinationPaintFull=false;std::vector<std::uint64_t> _agentPaintTextRevisions;std::vector<std::uint64_t> agentPaintTextRevisions()const;bool agentDestinationFull()const;void blit()override;
 ErrorMessageState(Game*g):_game(g){g->top=this;}
 bool agentUiState(AgentUiState&)const override;
 bool agentDispatch(const std::string&)override;
 void btnOkClick(Action*);
};
#include "ErrorMethods.generated.inc"
}
namespace {
#include "AgentManufactureChannel.inc"
struct Fixture {
 Font&big;Font&small;Language&lang;Surface canvas,window;TextButton button;Text text;Errors::Game game;Errors::ErrorMessageState state;
 Fixture(Font&b,Font&s,Language&l,const std::string&master):big(b),small(s),lang(l),canvas(320,200),window(256,160,32,20),button(120,18,100,154),text(246,80,37,50),state(&game){
 Palette pal;pal.loadDat(std::string("../")+(master=="xcom1"?"UFO":"TFTD")+"/GEODATA/PALETTES.DAT",256,Palette::palOffset(1));for(Surface*p:{&canvas,&window,(Surface*)&button,(Surface*)&text})p->setPalette(pal.getColors());
 button.initText(&big,&small,&lang);button.setText(lang.getString("STR_OK"));button.setColor(208);text.initText(&big,&small,&lang);text.setColor(208);text.setAlign(ALIGN_CENTER);text.setVerticalAlign(ALIGN_MIDDLE);text.setBig();text.setWordWrap(true);text.setText(lang.getString("STR_NO_FREE_HANGARS_FOR_PURCHASE"));text.agentEnableErrorHeading();state._txtMessage=&text;state._btnOk=&button;state._surfaces={&window,&button,&text};game.screen.canvas=&canvas;errorCanvas=&canvas;AgentUiState initial;check(!state.agentUiState(initial),"no projection before first full paint");draw();}
 void draw(){errorCanvas=&canvas;state.blit();}
 AgentUiState project(){AgentUiState u;check(state.agentUiState(u),"error provider projects");return u;}
};
std::vector<std::string> cases;
void record(TableChannel&c,const std::string&name,const std::string&game){std::string p=c.read();auto v=YAML::Load(p)["visible"];cases.push_back("{\"name\":"+agentJsonString(name)+",\"game\":"+agentJsonString(game)+",\"state_file\":"+agentJsonString(c.prefix+"-wire-"+std::to_string(c.schedule.size()-1)+".json")+",\"message_expected\":"+((!v["message"] || v["message"].IsNull())?std::string("null"):agentJsonString(v["message"].as<std::string>()))+",\"heading_expected\":"+((!v["heading"] || v["heading"].IsNull())?std::string("null"):agentJsonString(v["heading"].as<std::string>()))+"}");}
void bind(TableChannel&c,Fixture&f){c.state.project=[&](AgentUiState&u){return f.state.agentUiState(u);};c.state.handler=[&](const AgentRequest&r){check(f.state.agentDispatch(r.action),"actual error dispatch");if(f.game.top!=&f.state)c.game.top=&c.unsupportedParent;};}
void run(Font&b,Font&s,Language&l,const std::string&master){Fixture f(b,s,l,master);const std::string stock=l.getString("STR_NO_FREE_HANGARS_FOR_PURCHASE");const std::string title=stock.substr(0,stock.find(Unicode::TOK_NL_SMALL));
 // Enabling observer capture preserves every ordinary rendered source pixel.
 f.text._agentErrorHeading=false;f.text.setText(stock);f.draw();std::vector<Uint8> ordinary;
 for(int y=0;y<f.text.getHeight();++y)for(int x=0;x<f.text.getWidth();++x)ordinary.push_back(f.text.getPixel(x,y));
 const auto revision=f.text.agentTextRevision();f.text.setText(stock);check(f.text.agentTextRevision()==revision,"same text does not self-invalidate revision");f.text.agentEnableErrorHeading();f.draw();unsigned index=0;
 for(int y=0;y<f.text.getHeight();++y)for(int x=0;x<f.text.getWidth();++x)check(f.text.getPixel(x,y)==ordinary[index++],"observer capture leaves ordinary GUI pixels exact");
 TableChannel c(master+"-heading");bind(c,f);record(c,"stock_positive",master);auto v=YAML::Load(c.last)["visible"];check(v["heading"].as<std::string>()==title,"stock whole heading exact");check((!v["message"] || v["message"].IsNull())==(master=="xcom1"),"UFO true comma clip / TFTD complete body");check(f.text._agentHeadingPixels.size()>100,"typed heading ink captured bounded");
 // Explicit typed suffix replacement and repeated same-value writes need paint.
 f.text.setText(stock);record(c,"same_text_without_repaint",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"same setText cannot revive old heading before paint");f.text.setText(stock);record(c,"same_text_twice_without_repaint",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"repeated same replacement remains unpainted");f.draw();
 f.text.setText(stock+" changed suffix");record(c,"changed_suffix_without_repaint",master);check(YAML::Load(c.last)["state"].as<std::string>()=="unsupported","changed suffix invalidates whole-state paint binding");f.text.setText(stock);f.draw();
 // Synthetic later suffix placement, real stock Text glyph blit. No OCR or
 // claim that the stock SMALLLINE layout itself creates this overlap.
 Text suffix(40,20);suffix.initText(&b,&s,&l);suffix.setText("A");suffix.setColor(32);suffix.setPalette(f.text.getPalette());suffix.draw();int sx=-1,sy=-1;for(int y=0;y<suffix.getHeight()&&sx<0;++y)for(int x=0;x<suffix.getWidth();++x)if(suffix.getPixel(x,y)){sx=x;sy=y;break;}
 check(sx>=0,"stock suffix glyph has ink");auto target=f.text._agentHeadingPixels.front();const auto before=f.text.getPixel(target.x,target.y);suffix.setX(target.x-sx);suffix.setY(target.y-sy);suffix.blit(&f.text);check(f.text.getPixel(target.x,target.y)!=before,"actual suffix blit overwrites recorded heading glyph");record(c,"later_suffix_overwrite",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"later suffix draw retires complete heading");f.text.setText(stock);f.draw();
 // Complete body remains separate from a title proof.
 auto px=f.text._agentHeadingPixels.front();Uint8 original=f.text.getPixel(px.x,px.y);f.text.setPixel(px.x,px.y,original==1?2:1);record(c,"changed_heading_pixel",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"changed typed heading pixel rejected");f.text.setPixel(px.x,px.y,original);record(c,"restored_heading_pixel",master);
 f.text.setText("No SMALLLINE marker");f.draw();record(c,"no_marker",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"no invented heading without marker");
 f.text.setText(std::string(1,Unicode::TOK_NL_SMALL)+"Body");f.draw();record(c,"empty_prefix",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"empty heading withheld");
 f.text.setText(stock);f.draw();f.text.setWidth(20);f.draw();record(c,"heading_clip",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"real glyph crop rejected");f.text.setWidth(246);f.text.setText(stock);f.draw();
 SDL_Rect admitted;std::string label;check(f.text.agentErrorHeading(label,admitted),"heading area for exact cover");Surface cover(5,5,admitted.x,admitted.y);cover.clear(3);f.state._surfaces.push_back(&cover);f.draw();record(c,"sibling_overlay",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"later opaque heading cover rejected");f.state._surfaces.pop_back();f.draw();
 SDL_Rect heading;std::string shown;check(f.text.agentErrorHeading(shown,heading),"heading region available for exact overlay target");cover.setX(heading.x);cover.setY(heading.y);f.game._cursor=&cover;record(c,"global_heading_overlay",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"regional global overlay omits heading");check(YAML::Load(c.last)["actions"].size()==1,"uncovered ordinary OK survives");f.game._cursor=nullptr;
 f.text.invalidate();record(c,"dirty_heading",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"dirty heading no authority");f.draw();
 f.text.getCrop()->w=1;record(c,"source_crop",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"source crop withheld");f.text.getCrop()->w=0;
 SDL_Rect clip={1,0,245,80};SDL_SetClipRect(f.text.getSurface(),&clip);record(c,"source_clip_rect",master);check(YAML::Load(c.last)["visible"]["heading"].IsNull(),"nonstandard clip rect withheld");SDL_SetClipRect(f.text.getSurface(),nullptr);
 // Destination clipping is a retained last-paint failure, even after restoration.
 SDL_Rect destClip={160,0,160,200};SDL_SetClipRect(f.canvas.getSurface(),&destClip);f.draw();record(c,"destination_clipped_paint",master);check(YAML::Load(c.last)["state"].as<std::string>()=="unsupported","clipped destination blocks whole provider");SDL_SetClipRect(f.canvas.getSurface(),nullptr);record(c,"destination_restored_without_paint",master);check(YAML::Load(c.last)["state"].as<std::string>()=="unsupported","restoring clip cannot revive clipped paint");f.draw();record(c,"destination_full_repaint",master);check(!YAML::Load(c.last)["visible"]["heading"].IsNull(),"full repaint restores ordinary heading");
 f.text.setText("away before paint");f.text.setText(stock);record(c,"text_aba_without_repaint",master);check(YAML::Load(c.last)["state"].as<std::string>()=="unsupported","typed content revision requires new whole state paint");f.draw();record(c,"text_aba_repaint",master);
 // Exact original typed content mutation away and back changes private revision.
 record(c,"before_text_aba",master);f.text.setText("away");f.text.setText(stock);f.draw();c.queue("dismiss_error");c.frame();auto rejected=c.receive();check(YAML::Load(rejected)["status"].as<std::string>()=="unavailable"&&f.game.pops==0,"text ABA rejects stale E before handler");
 record(c,"after_text_aba",master);f.button.setText("away");f.button.setText(l.getString("STR_OK"));f.draw();c.queue("dismiss_error");c.frame();rejected=c.receive();check(YAML::Load(rejected)["status"].as<std::string>()=="unavailable"&&f.game.pops==0,"button label ABA rejects stale E");
 record(c,"before_overlay_move",master);f.game._cursor=&cover;c.queue("dismiss_error");c.frame();rejected=c.receive();check(YAML::Load(rejected)["status"].as<std::string>()=="unavailable"&&f.game.pops==0,"overlay changed read to dispatch rejects");f.game._cursor=nullptr;
 record(c,"restored_after_negatives",master);const std::string cap=YAML::Load(c.last)["capability"].as<std::string>();c.events();check(YAML::Load(c.last)["capability"].as<std::string>()==cap,"V leaves cached grant");
 AgentUiState modalUi;f.state._modal=&f.button;check(!f.state.agentUiState(modalUi),"modal provider rejects");f.state._modal=nullptr;f.game.top=nullptr;AgentUiState unsupported;check(!f.state.agentUiState(unsupported),"non-top error rejects");f.game.top=&f.state;
 // Work and text budgets are local to heading, never partial label output.
 f.text.setWidth(640);f.text.setHeight(400);f.text.setText(std::string(1000,'M')+char(Unicode::TOK_NL_SMALL)+"Body");f.draw();std::string temp;SDL_Rect tmp={};check(!f.text.agentErrorHeading(temp,tmp),"actual large typed heading exhausts bound");f.text.setWidth(246);f.text.setHeight(80);f.text.setText(std::string(1025,'A')+char(Unicode::TOK_NL_SMALL));f.draw();check(!f.text.agentErrorHeading(temp,tmp),"heading UTF8 source bound");f.text.setText(stock);f.draw();
 record(c,"before_dismiss",master);c.queue("dismiss_error");c.frame();check(c.receive().empty(),"ordinary dismiss waits for later presentation");check(f.game.pops==1,"normal callback one pop");c.frame();auto receipt=c.receive();check(YAML::Load(receipt)["status"].as<std::string>()=="dispatched","later frame ordinary dispatch receipt");c.send(c.request);c.controller->poll();check(c.receive()==receipt&&f.game.pops==1,"duplicate cached no second pop");c.events();check(agentPaintHealthy(),"bounded heading paint healthy");std::cout<<master<<" error heading PASS\n";
}
}
int main(int argc,char**argv){check(argc==2,"source path");check(!SDL_Init(0)&&!SDL_GetVideoSurface(),"offscreen SDL only");Options::baseXResolution=320;Options::baseYResolution=200;Options::keyOk=SDLK_RETURN;Options::keyCancel=SDLK_ESCAPE;Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}for(const std::string master:{"xcom1","xcom2"}){Language lang;lang.loadFile(std::string(argv[1])+"/bin/standard/"+master+"/Language/en-US.yml");run(big,small,lang,master);}Language faultLanguage;faultLanguage.loadFile(std::string(argv[1])+"/bin/standard/xcom1/Language/en-US.yml");Fixture fault(big,small,faultLanguage,"xcom1");agentObservePaint(-1);fault.draw();AgentUiState faultUi;check(!fault.state._agentDestinationPaintFull&&!fault.state.agentUiState(faultUi),"failed paint cannot regain provider authority");std::ofstream records(std::string(std::getenv("OPENXCOM_AGENT_TEST_PACKET_DIR"))+"/case-records.json");records<<"[";for(unsigned i=0;i<cases.size();++i){if(i)records<<",";records<<cases[i];}records<<"]";check(bool(records),"case records saved");check(!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"no GUI/game");std::cout<<"AgentErrorHeadingTestV19 PASS "<<checks<<" checks\n";}
