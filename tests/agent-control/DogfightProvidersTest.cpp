// SPDX-License-Identifier: GPL-3.0-or-later
// Real SDL ImageButton/Text and current extracted provider; explicit owning-game and combat callback seams.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Interface/ImageButton.h"
#include "../../src/Interface/Text.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <list>
#include <functional>
using namespace OpenXcom;using namespace OpenXcom::Observer;
namespace OpenXcom {
Game *State::_game=nullptr;
State::State():_screen(true),_modal(nullptr),_ruleInterface(nullptr),_ruleInterfaceParent(nullptr){}
State::~State(){}
void State::init(){}void State::handle(Action*){}void State::blit(){}void State::resize(int&,int&){}void State::recenter(int,int){}
#include "CompletionStateBase.generated.inc"
#include "CompletionGameIdentity.generated.inc"
}
namespace Fixture {
struct Screen{Surface*logical=nullptr;Surface*getSurface(){return logical;}};
struct Game{Screen screen;const void*top=nullptr;bool clear=true;Screen*getScreen(){return &screen;}bool isState(const void*p)const{return p==top;}bool agentOverlayClear(const AgentUiState&)const{return clear;}};
struct State:OpenXcom::State {Game*_game;std::function<void()>paintHook;using OpenXcom::State::_surfaces;using OpenXcom::State::_modal;explicit State(Game*g):_game(g){}void blit()override{for(auto*s:_surfaces)s->blit(_game->screen.logical);if(paintHook)paintHook();}};
const int STANDOFF_DIST=560;
struct WeaponRule{int getCautiousReload(){return 1;}int getStandardReload(){return 2;}int getAggressiveReload(){return 3;}};
struct CraftWeapon{WeaponRule rule;WeaponRule*getRules(){return &rule;}};
struct CraftRule{int slots=0;int getWeapons(){return slots;}};
struct Craft{bool destroyed=false;CraftRule rule;std::vector<CraftWeapon*>weapons;bool isDestroyed(){return destroyed;}CraftRule*getRules(){return &rule;}std::vector<CraftWeapon*>*getWeapons(){return &weapons;}};
struct Ufo{bool crashed=false;bool isCrashed(){return crashed;}};
struct DogfightState:State{
 void*_state;Craft craft;Ufo ufo;Craft*_craft=&craft;Ufo*_ufo=&ufo;bool _ufoBreakingOff=false,_end=false;int _targetDist=0,_w1FireInterval=0,_w2FireInterval=0;bool _minimized=false;
 InteractiveSurface preview;InteractiveSurface*_preview=&preview;Text status,distance,ammo1,ammo2,interception;Text*_txtStatus=&status,*_txtDistance=&distance,*_txtAmmo1=&ammo1,*_txtAmmo2=&ammo2,*_txtInterceptionNumber=&interception;
 ImageButton standoff,cautious,standard,aggressive,disengage;ImageButton*_btnStandoff=&standoff,*_btnCautious=&cautious,*_btnStandard=&standard,*_btnAggressive=&aggressive,*_btnDisengage=&disengage,*_mode=&standoff;
 bool _agentDestinationPaintFull=false;std::vector<std::uint64_t>_agentStoredPaintRevisions;int calls=0;ImageButton*callbackMode=nullptr;
 DogfightState(Game*g,void*geo,Font&big,Font&small,Language&lang,SDL_Color*palette):State(g),_state(geo),preview(1,1),status(100,9,0,0),distance(100,9,0,12),ammo1(100,9,0,24),ammo2(100,9,0,36),interception(100,9,0,48),standoff(12,12,0,100),cautious(12,12,20,100),standard(12,12,40,100),aggressive(12,12,60,100),disengage(12,12,80,100){
 preview.setVisible(false);g->top=geo;
 for(Text*t:{&status,&distance,&ammo1,&ammo2,&interception}){t->agentTrackPresentation();t->setPalette(palette);t->initText(&big,&small,&lang);t->setColor(80);t->setText("560");_surfaces.push_back(t);}
 for(ImageButton*b:{&standoff,&cautious,&standard,&aggressive,&disengage}){b->setPalette(palette);b->clear(80);b->setColor(80);b->setGroup(&_mode);b->agentTrackRadio();_surfaces.push_back(b);}

 standoff.onMousePress((ActionHandler)&DogfightState::btnStandoffPress);cautious.onMousePress((ActionHandler)&DogfightState::btnCautiousPress);standard.onMousePress((ActionHandler)&DogfightState::btnStandardPress);aggressive.onMousePress((ActionHandler)&DogfightState::btnAggressivePress);disengage.onMousePress((ActionHandler)&DogfightState::btnDisengagePress);
 }
 void setStatus(const std::string &text){++calls;callbackMode=_mode;status.setText(text);}void minimumDistance(){_targetDist=200;}void maximumDistance(){_targetDist=100;}
 void btnStandoffPress(Action*);void btnCautiousPress(Action*);void btnStandardPress(Action*);void btnAggressivePress(Action*);void btnDisengagePress(Action*);
 bool agentDestinationFull()const;std::vector<std::uint64_t>agentPaintRevisions()const;void blit()override;bool agentUiState(AgentUiState&)const override;bool agentDispatch(const AgentRequest&)override;
};
struct GeoscapeState{void*_modal=nullptr;std::list<DogfightState*>_dogfights,_dogfightsToBeStarted;std::list<void*>_popups;DogfightState*agentActiveDogfight()const;};
#include "Dogfight.generated.inc"
}
static int checks=0;static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(){
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen SDL only");Options::baseXResolution=320;Options::baseYResolution=200;
 Font big,small;for(auto f:YAML::LoadFile("Language/Font.dat")["fonts"]){if(f["id"].as<std::string>()=="FONT_BIG")big.load(f);if(f["id"].as<std::string>()=="FONT_SMALL")small.load(f);}Language lang;
 Surface canvas(320,200);SDL_Color palette[256];for(int i=0;i<256;++i)palette[i]={Uint8(i),Uint8(i),Uint8(i),0};canvas.setPalette(palette);canvas.clear(40);
 Fixture::Game game;game.screen.logical=&canvas;Fixture::GeoscapeState geo;Fixture::DogfightState s(&game,&geo,big,small,lang,palette);s.blit();AgentUiState u;
 check(s.agentUiState(u)&&u.screen=="dogfight"&&u.offers("choose_dogfight_mode"),"native image mode projection");auto v=YAML::Load(u.visible);check(v.size()==7&&v["damage"].IsNull()&&v["buttons"].size()==5,"exact bounded schema");check(v["buttons"][0]["selected"].as<bool>(),"painted initial group selection");for(auto key:{"status","distance","ammo1","ammo2","interception"})check(v[key].as<std::string>()=="560","initial fields derive from actual rendered Text");
 const char*ids[]={"standoff","cautious","standard","aggressive","disengage"};ImageButton*bs[]={&s.standoff,&s.cautious,&s.standard,&s.aggressive,&s.disengage};
 for(int i=0;i<5;++i){AgentRequest r;r.action="choose_dogfight_mode";r.choice=ids[i];auto old=u;int before=s.calls;check(s.agentDispatch(r),"native semantic selection admitted");check(s.calls==before+1&&s._mode==bs[i]&&s.callbackMode==bs[i],"actual ImageButton group updated BEFORE callback");check(!s.agentUiState(u),"radio change not admitted until repaint");s.blit();check(s.agentUiState(u)&&!old.sameBinding(u),"selection revision binds refreshed frame");auto q=YAML::Load(u.visible);for(int j=0;j<5;++j)check(q["buttons"][j]["selected"].as<bool>()==(i==j),"exact unique selected radio");}
 AgentRequest bad;bad.action="choose_dogfight_mode";for(auto id:{"","standard_attack","MAX","r0"}){bad.choice=id;check(!s.agentDispatch(bad),"unknown mode rejected");}
 check(s._mode==&s.disengage&&s.calls==5,"invalid mode never mutates selection/callback");
 bad.action="choose_visible_row";bad.choice="standard";check(!s.agentDispatch(bad),"wrong action rejected");
 for(Text*t:{&s.status,&s.distance,&s.ammo1,&s.ammo2,&s.interception}){int x=t->getX(),y=t->getY();t->setX(x+1);check(!s.agentUiState(u),"text moved without repaint unavailable");t->setX(x);check(!s.agentUiState(u),"text X ABA still stale");s.blit();check(s.agentUiState(u),"text X redraw restores");t->setY(y+1);t->setY(y);check(!s.agentUiState(u),"text Y ABA stale");s.blit();check(s.agentUiState(u),"text Y redraw restores");t->setVisible(false);t->setVisible(true);check(!s.agentUiState(u),"text visibility ABA stale");s.blit();check(s.agentUiState(u),"text visibility redraw restores");}
 auto old=u;s.status.setText("DISENGAGING");check(!s.agentUiState(u),"changed text without repaint rejected");s.blit();check(s.agentUiState(u)&&!old.sameBinding(u),"rendered status binding updated");
 s._minimized=true;check(!s.agentUiState(u),"minimized unavailable");s._minimized=false;s.preview.setVisible(true);check(!s.agentUiState(u),"preview unavailable");s.preview.setVisible(false);s._modal=&s.preview;check(!s.agentUiState(u),"local modal unavailable");s._modal=nullptr;game.top=nullptr;check(!s.agentUiState(u),"not owning geoscape unavailable");game.top=&geo;
 SDL_Rect clipped={0,0,100,200};SDL_SetClipRect(canvas.getSurface(),&clipped);s.blit();check(!s.agentUiState(u),"clipped draw unavailable");SDL_SetClipRect(canvas.getSurface(),nullptr);check(!s.agentUiState(u),"restored clip needs redraw");s.blit();check(s.agentUiState(u),"full redraw accepted");
 s.paintHook=[&](){SDL_SetClipRect(canvas.getSurface(),&clipped);};s.blit();check(!s.agentUiState(u),"clip during draw unavailable");s.paintHook=nullptr;SDL_SetClipRect(canvas.getSurface(),nullptr);s.blit();
 game.clear=false;check(!s.agentUiState(u),"overlay unavailable");game.clear=true;
 s.standard.setVisible(false);s.blit();check(s.agentUiState(u)&&u.rows.size()==4,"hidden mode omitted");bad.action="choose_dogfight_mode";bad.choice="standard";check(!s.agentDispatch(bad),"hidden mode cannot dispatch");s.standard.setVisible(true);s.blit();
 s._mode=&s.standard;check(!s.agentUiState(u)||u.rows.empty(),"group mutation since blit revokes radio controls");s.blit();check(s.agentUiState(u),"repaint certifies ordinary selected state");
 // Configurable 0/1/2 real fixture weapon slots; only range calculations remain seams.
 Fixture::CraftWeapon first,second;
 for(int count=0;count<=2;++count){s.craft.rule.slots=count;s.craft.weapons.clear();if(count>0)s.craft.weapons.push_back(&first);if(count>1)s.craft.weapons.push_back(&second);
 for(int i=0;i<5;++i){s._w1FireInterval=s._w2FireInterval=-1;s._targetDist=-1;s._end=true;s.blit();AgentRequest r;r.action="choose_dogfight_mode";r.choice=ids[i];check(s.agentDispatch(r),"weapon matrix dispatch");int expectedDistance[]={560,200,100,64,800};check(s._targetDist==expectedDistance[i]&&s._end==(i==4),"ordinary target distance/end transition");int reload=i>=1&&i<=3?i:-1;check(s._w1FireInterval==(count>0?reload:-1)&&s._w2FireInterval==(count>1?reload:-1),"ordinary weapon reload branch 0/1/2 slots");s.blit();check(s.agentUiState(u),"weapon matrix rerender");auto shown=YAML::Load(u.visible);const auto raw=agentTextJson(s._txtStatus);check(raw=="null"?shown["status"].IsNull():shown["status"].as<std::string>()==s.status.getText(),"status matches actual rendered Text or explicit null");}}
 // Missing weapon pointers also preserve ordinary null-slot guards.
 s.craft.rule.slots=2;s.craft.weapons={nullptr,nullptr};s._w1FireInterval=s._w2FireInterval=-1;s.blit();AgentRequest emptyWeapon;emptyWeapon.action="choose_dogfight_mode";emptyWeapon.choice="aggressive";check(s.agentDispatch(emptyWeapon)&&s._w1FireInterval==-1&&s._w2FireInterval==-1,"empty weapon slots skip reload access");
 // Real ordinary handler guards may decline after ordinary radio selection.
 for(int guard=0;guard<3;++guard){s.craft.destroyed=guard==0;s.ufo.crashed=guard==1;s._ufoBreakingOff=guard==2;s.blit();int before=s.calls;s._targetDist=-7;s._end=false;s._w1FireInterval=-8;s._w2FireInterval=-9;AgentRequest r;r.action="choose_dogfight_mode";r.choice="disengage";check(s.agentDispatch(r)&&s._mode==&s.disengage&&s.calls==before,"guarded callback refusal preserves ordinary radio behavior");check(s._targetDist==-7&&!s._end&&s._w1FireInterval==-8&&s._w2FireInterval==-9,"refused callback preserves combat settings");s.blit();check(s.agentDispatch(r)&&s._mode==&s.disengage&&s.calls==before,"repeated same guarded mode harmless");}
 s.craft.destroyed=s.ufo.crashed=s._ufoBreakingOff=false;s.blit();ImageButton*other=&s.standoff;s.standard.setGroup(&other);check(!s.agentUiState(u),"group pointer mutation needs repaint");s.standard.setGroup(&s._mode);s.blit();
 // The selected control may be unavailable while the other painted modes remain actionable.
 ImageButton*hiddenSelected=s._mode;hiddenSelected->setVisible(false);s.status.setVisible(false);s.blit();check(s.agentUiState(u),"zero visible selected mode remains valid bounded state");auto zero=YAML::Load(u.visible);unsigned selectedCount=0;for(auto button:zero["buttons"])selectedCount+=button["selected"].as<bool>();check(zero["buttons"].size()==4&&selectedCount==0&&zero["status"].IsNull(),"no inferred hidden selection and unknown text stays null");int unchanged=s.calls;AgentRequest hidden;hidden.action="choose_dogfight_mode";hidden.choice="disengage";check(!s.agentDispatch(hidden)&&s.calls==unchanged&&s._mode==hiddenSelected,"hidden selected mode refusal no mutation");hidden.choice="invalid";check(!s.agentDispatch(hidden)&&s.calls==unchanged&&s._mode==hiddenSelected,"invalid mode refusal no mutation with partial view");hiddenSelected->setVisible(true);s.status.setVisible(true);s.blit();check(s.agentUiState(u),"restored selected control requires redraw");
 check(!geo.agentActiveDogfight(),"zero windows unavailable");geo._dogfights={&s};check(geo.agentActiveDogfight()==&s,"exactly one active child");geo._dogfights.push_back(&s);check(!geo.agentActiveDogfight(),"two windows unavailable");geo._dogfights={nullptr};check(!geo.agentActiveDogfight(),"null window unavailable");geo._dogfights={&s};geo._dogfightsToBeStarted={&s};check(!geo.agentActiveDogfight(),"queued window unavailable");geo._dogfightsToBeStarted.clear();geo._popups={&s};check(!geo.agentActiveDogfight(),"popup unavailable");geo._popups.clear();geo._modal=&s;check(!geo.agentActiveDogfight(),"geoscape modal unavailable");
 check(!SDL_GetVideoSurface(),"no actual game/video session");std::cout<<checks<<" native dogfight provider/radio checks passed; combat/owning Game seams, no live encounter\n";
}
