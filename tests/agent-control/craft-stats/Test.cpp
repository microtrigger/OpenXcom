// SPDX-License-Identifier: GPL-3.0-or-later
#include "Engine/Font.h"
#include "Mod/RuleInterface.h"
#include "Engine/Surface.h"
#include "Engine/Language.h"
#include "Engine/Options.h"
#include "Engine/State.h"
#include "Engine/Sound.h"
#include "Interface/Text.h"
#include "Interface/ImageButton.h"
#include "Interface/TextButton.h"
#include "Interface/TextList.h"
#include "Engine/Unicode.h"
#include "Observer/AgentUi.h"
#include "Observer/AgentProtocol.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <map>
#include <vector>
#include <climits>
#include <stdexcept>
using namespace OpenXcom;using namespace OpenXcom::Observer;
static std::string resources;
namespace OpenXcom { namespace FileMap {const std::string&getFilePath(const std::string&r){static std::map<std::string,std::string>p;return p.emplace(r,resources+"/common/"+r).first->second;}}namespace Options {int baseXResolution=320,baseYResolution=200;std::string language="en-US";TextWrapping wordwrap=WRAP_AUTO;bool debugUi=false;}}
unsigned checks=0;void check(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
namespace OpenXcom {void State::setModal(InteractiveSurface*){} void Sound::play(int,int,int)const{throw std::runtime_error("unexpected audio");}}
namespace Fixture {
struct Screen{Surface canvas{320,200};Surface*getSurface(){return &canvas;}};
struct Game{Screen screen;const void*top=nullptr;Surface*_fpsCounter=nullptr;Screen*getScreen(){return &screen;}bool isState(const void*p){return top==p;}bool agentOverlayClear(const AgentUiState&)const;bool agentNonCursorOverlayClear(const AgentUiState&)const;};
#include "Overlay.generated.inc"
struct State{Game*_game;void*_modal=nullptr;std::vector<Surface*>_surfaces;void blit();};
#include "Blit.generated.inc"
struct ArticleState:State {
Text *_agentTitle=nullptr,*_agentBody=nullptr,*_agentCraftStats=nullptr;std::vector<Surface*>_agentTransparentLayers;bool _agentArticlePaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;TextButton *_btnOk,*_btnPrev,*_btnNext;
void agentRegisterText(Text*,Text*);void agentRegisterCraftStats(Text*);void agentRegisterTransparentLayer(Surface*);
bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit();bool agentUiState(AgentUiState&)const;
~ArticleState(){for(auto*s:_surfaces)delete s;}
};
#include "Provider.generated.inc"
}
std::vector<unsigned char> pixels(Surface&surface){auto*p=surface.getSurface();check(SDL_LockSurface(p)==0,"lock raster");std::vector<unsigned char> result;for(int y=0;y<p->h;++y){auto*b=static_cast<unsigned char*>(p->pixels)+y*p->pitch;result.insert(result.end(),b,b+p->w);}SDL_UnlockSurface(p);return result;}
std::string statsText(YAML::Node craft,YAML::Node language){std::string result;std::vector<std::string>labels={"STR_MAXIMUM_SPEED_UC","STR_ACCELERATION","STR_FUEL_CAPACITY","STR_WEAPON_PODS","STR_DAMAGE_CAPACITY_UC","STR_CARGO_SPACE","STR_HWP_CAPACITY"},keys={"speedMax","accel","fuelMax","weapons","damageMax","soldiers","vehicles"};for(size_t i=0;i<7;++i){std::string line=language[labels[i]].as<std::string>();int n=craft[keys[i]]?craft[keys[i]].as<int>():0;std::string value=(i==0||i==2||i==4)?Unicode::formatNumber(n):std::to_string(n);size_t p;while((p=line.find("{ALT}"))!=std::string::npos)line.replace(p,5,"\1");p=line.find("{0}");line.replace(p,3,value);if(i)result+='\n';result+=line;}return result;}
int main(int argc,char**argv){check(argc==2,"resources");check(std::string(agentProtocolSchema())=="openxcom-agent-control/50-craft-stats1","public fork protocol identifier");resources=argv[1];check(SDL_Init(0)==0,"SDL0");check(!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO)&&!SDL_WasInit(SDL_INIT_AUDIO),"no devices");Font big,small;for(auto f:YAML::LoadFile(resources+"/common/Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);}Language lang;
for(int master=1;master<=2;++master){std::string dir=resources+"/standard/xcom"+std::to_string(master);auto rules=YAML::LoadFile(dir+"/ufopaedia.rul")["ufopaedia"],crafts=YAML::LoadFile(dir+"/crafts.rul")["crafts"],language=YAML::LoadFile(dir+"/Language/en-US.yml")["en-US"];std::vector<std::string>ids=master==1?std::vector<std::string>{"STR_INTERCEPTOR","STR_SKYRANGER"}:std::vector<std::string>{"STR_BARRACUDA","STR_TRITON"};for(auto id:ids){YAML::Node def,craft;for(auto x:rules)if(x["id"].as<std::string>()==id)def=YAML::Clone(x);for(auto x:crafts)if(x["type"].as<std::string>()==id)craft=YAML::Clone(x);
Fixture::Game g;Fixture::ArticleState a;a._game=&g;g.top=&a;
auto text=[&](int w,int h,int x,int y,std::string value){auto*t=new Text(w,h,x,y);t->initText(&big,&small,&lang);t->setText(value);a._surfaces.push_back(t);return t;};
auto button=[&](int x,std::string label){auto*b=new TextButton(master==1?30:23,master==1?14:10,x,master==1?5:179);b->initText(&big,&small,&lang);b->setText(label);a._surfaces.push_back(b);return b;};a._btnOk=button(master==1?5:227,"OK");a._btnPrev=button(master==1?40:254,"<");a._btnNext=button(master==1?75:281,">");
Text*title=text(master==1?210:284,master==1?32:16,master==1?5:36,master==1?24:14,language[id].as<std::string>());title->setBig();title->setWordWrap(true);if(master==2)title->setAlign(ALIGN_CENTER);
int bx=master==1?def["rect_text"]["x"].as<int>():163,by=master==1?def["rect_text"]["y"].as<int>():34,bw=master==1?def["rect_text"]["width"].as<int>():157,bh=master==1?def["rect_text"]["height"].as<int>():80;
Text*body=text(bw,bh,bx,by,language[def["text"].as<std::string>()].as<std::string>());body->setWordWrap(true);body->setScrollable(true);body->setExtraLineSpacing(1);
int x=master==1?def["rect_stats"]["x"].as<int>():187,y=master==1?def["rect_stats"]["y"].as<int>():116,w=master==1?def["rect_stats"]["width"].as<int>():131,h=master==1?def["rect_stats"]["height"].as<int>():56;
std::string expected=statsText(craft,language);Text*t=text(w,h,x,y,expected);a.agentRegisterText(title,body);a.blit();auto rasterBefore=pixels(g.screen.canvas);a.agentRegisterCraftStats(t);a.blit();check(rasterBefore==pixels(g.screen.canvas),"registration preserves index raster");AgentUiState first,now;check(a.agentUiState(first),"provider accepts");auto parsed=YAML::Load(first.visible);std::cout<<id<<" "<<first.visible<<"\n";check(parsed["craft_stats"].IsSequence()&&parsed["craft_stats"].size()==7,"seven actual lines");std::string joined;for(auto line:parsed["craft_stats"]){if(!joined.empty())joined+='\n';joined+=line.as<std::string>();}check(joined==expected,"exact native Text lines");check(parsed["details_status"].as<std::string>()=="partial","partial details");check(!parsed["article_stats"]&&!parsed["item_combat"],"no unrelated DTO fields");
check(a.agentUiState(now)&&first.sameBinding(now),"stable binding");t->setText(expected+"!");check(!a.agentUiState(now),"dirty text rejected");a.blit();check(a.agentUiState(now)&&!first.sameBinding(now),"painted changed text invalidates");t->setText(expected);a.blit();check(a.agentUiState(now)&&!first.sameBinding(now),"restored string ABA rejects old binding");
t->setWidth(10);a.blit();check(a.agentUiState(now)&&YAML::Load(now.visible)["craft_stats"].IsNull(),"clipped text null");t->setWidth(w);a.blit();t->setX(319);a.blit();check(a.agentUiState(now)&&YAML::Load(now.visible)["craft_stats"].IsNull(),"offscreen null");t->setX(x);a.blit();
Surface overlay(1,1,x+w-1,y+h-1);g._fpsCounter=&overlay;check(a.agentUiState(now)&&YAML::Load(now.visible)["craft_stats"].IsNull(),"blank edge overlay null");g._fpsCounter=nullptr;
// The existing whole-widget cap remains strict, despite the wider array wire cap.
t->setText(std::string(1025,'\1'));a.blit();check(a.agentUiState(now)&&YAML::Load(now.visible)["craft_stats"].IsNull(),"oversized whole Text null");
t->setText("A A A A A A A A A A A A A A A A");t->setWidth(50);t->setWordWrap(true);a.blit();check(a.agentUiState(now),"wrapped provider");auto wrapped=YAML::Load(now.visible)["craft_stats"];check(t->getTextHeight()>8 && wrapped.IsSequence()&&wrapped.size()==1&&wrapped[0].as<std::string>()==t->getText(),"soft wraps preserve one logical line");
t->setWordWrap(false);t->setWidth(w);t->setText(expected);a.blit();
auto *sibling=new Surface(1,1,x+w-1,y+h-1);a._surfaces.push_back(sibling);a.blit();check(a.agentUiState(now)&&YAML::Load(now.visible)["craft_stats"].IsNull(),"foreign sibling blank edge blocks");sibling->setVisible(false);a.blit();check(a.agentUiState(now)&&YAML::Load(now.visible)["craft_stats"].IsSequence(),"hidden sibling ignored");
t->setVisible(false);a.blit();check(a.agentUiState(now)&&YAML::Load(now.visible)["craft_stats"].IsNull(),"hidden stats null");t->setVisible(true);a.blit();
a._agentCraftStats=nullptr;a.blit();check(a.agentUiState(now)&&YAML::Load(now.visible)["craft_stats"].IsNull(),"unregistered null");a.agentRegisterCraftStats(t);a.blit();
a._modal=&a;check(!a.agentUiState(now),"modal rejects");a._modal=nullptr;g.top=nullptr;check(!a.agentUiState(now),"wrong top rejects");g.top=&a;
SDL_Rect clip={0,0,319,199};SDL_SetClipRect(g.screen.canvas.getSurface(),&clip);a.blit();check(!a.agentUiState(now),"destination clip rejects");SDL_SetClipRect(g.screen.canvas.getSurface(),nullptr);a.blit();check(a.agentUiState(now),"full paint restored");
}}
std::cout<<"PASS checks="<<checks<<" no SDL video/audio\n";}
