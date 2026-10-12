// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
// A finite, default-OFF native ordinary inventory RIGHT click and pixels-only read fixture.
// Normal events, initialization, timers, blits and flips remain native.
// Oracles remain private; test-only frame evidence is retained in the disposable
// user directory and copied by the private runner, never sent on the public wire.
#include "InventoryRightClickControlGate.h"
#include "../../src/Battlescape/BattlescapeGame.h"
#include "../../src/Battlescape/BattlescapeState.h"
#include "../../src/Battlescape/BattleState.h"
#include "../../src/Battlescape/UnitTurnBState.h"
#include "../../src/Battlescape/UnitWalkBState.h"
#include "../../src/Battlescape/ProjectileFlyBState.h"
#include "../../src/Battlescape/WarningMessage.h"
#include "../../src/Battlescape/Camera.h"
#include "../../src/Battlescape/Map.h"
#include "../../src/Battlescape/Pathfinding.h"
#include "../../src/Battlescape/TileEngine.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Action.h"
#include "../../src/Interface/Text.h"
#include "../../src/Observer/StanceController.h"
#include "../../src/Observer/InventoryCloseOverlay.h"
#include "../../src/Interface/Frame.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/RNG.h"
#include "../../src/Engine/Screen.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/BattlescapeButton.h"
#include "../../src/Interface/Cursor.h"
#include "../../src/Interface/FpsCounter.h"
#include "../../src/Menu/PauseState.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Mod/MapData.h"
#include "../../src/Mod/MapDataSet.h"
#include "../../src/Mod/Mod.h"
#include "../../src/Mod/RuleItem.h"
#include "../../src/Mod/RuleSoldier.h"
#include "../../src/Mod/Unit.h"
#include "../../src/Savegame/Base.h"
#include "../../src/Savegame/BattleItem.h"
#include "../../src/Savegame/BattleUnit.h"
#include "../../src/Savegame/SavedBattleGame.h"
#include "../../src/Savegame/SavedGame.h"
#include "../../src/Savegame/Soldier.h"
#include "../../src/Savegame/Tile.h"
#include <SDL_mixer.h>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <deque>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <sys/socket.h>
#include <typeinfo>
#include <unistd.h>
#include <vector>

#include "../../src/Battlescape/InventoryState.h"
#include "../../src/Mod/RuleInventory.h"
#include "../../src/Battlescape/Inventory.h"
namespace OpenXcom {
// Test-only fixture access; production admission never consults these oracles.
struct InventoryRightClickGateAccess {
    static Inventory *widget(InventoryState *s) { return s->_inv; }
    static int page(InventoryState *s) { return s->_inv->_groundOffset; }
    static int stack(InventoryState *s,int x,int y) { const auto &levels=s->_inv->_stackLevel; const auto column=levels.find(x); if(column==levels.end()) return 0; const auto entry=column->second.find(y); return entry==column->second.end()?0:entry->second; }
    static int animation(InventoryState *s) { return s->_inv->_animFrame; }
    static bool modal(InventoryState *s) { return s->_modal != 0; }
    static size_t surfaces(InventoryState *s) { return s->_surfaces.size(); }
    static bool initialized(Game *g) { return g->_init; }
    static size_t deleted(Game *g) { return g->_deleted.size(); }
    static Surface *groundButton(InventoryState *s) { return s->_btnGround; }
    static Surface *selection(InventoryState *s) { return s->_inv->_selection; }
    static bool pressed(InventoryState *s,int button) { return s->_inv->isButtonPressed(button); }
    static unsigned consumedButtons(InventoryState *s) { return s->_inventoryClickConsumedButtons; }
    static void wrongWidgetActor(InventoryState *s,BattleUnit *actor) { s->_inv->_selUnit=actor; }
    static void wrongWidgetMode(InventoryState *s) { s->_inv->_tu=false; }
    static Surface *otherButton(InventoryState *s) { return s->_btnUnload; }
    static Text *overlayText(InventoryState *s) { return s->_inventoryCloseOverlay->_text; }
    static Surface *name(InventoryState *s) { return s->_txtName; }
    static Surface *banner(InventoryState *s) { return s->_inventoryCloseOverlay->_frame; }
    static Surface *yes(InventoryState *s) { return s->_inventoryCloseOverlay->_yes; }
    static Surface *no(InventoryState *s) { return s->_inventoryCloseOverlay->_no; }
    static Surface *pointer(InventoryState *s) { return s->_inventoryCloseOverlay->_pointer; }
    static int choice(InventoryState *s,int x,int y) { SDL_Event e={}; Action a(&e,1.0,1.0,0,0); a.setMouseAction(x,y,0,0); return s->_inventoryCloseOverlay->choice(&a); }
    static WarningMessage *warning(InventoryState *s) { return s->_inv->_warning; }
    static YAML::Node text(InventoryState *s) { YAML::Node n; n["name"]=s->_txtName->getText(); n["item"]=s->_txtItem->getText(); n["ammo"]=s->_txtAmmo->getText(); n["TU"]=s->_txtTus->getText(); n["weight"]=s->_txtWeight->getText(); return n; }
    static int phase(Map *m) { return m->_animFrame; }
    static bool awaitingClose(Game *g) { return g->getStanceController()->_awaitingCloseView; }
    static bool unresolved(Game *g) { return g->getStanceController()->_inventoryRightDispatching && g->getStanceController()->_awaitingInventoryView && !g->getStanceController()->_inventoryDestination; }
    static bool baseMode(InventoryState *s) { return s->_inv->_base; }
    static void setBaseMode(InventoryState *s) { s->_inv->_base=true; }
    static bool awaiting(Game *g) { return g->getStanceController()->_awaitingInventoryView; }
    static InventoryState *destination(Game *g) { return g->getStanceController()->_inventoryDestination; }
    static YAML::Node stacks(InventoryState *s) { YAML::Node n; for (const auto &x:s->_inv->_stackLevel) for (const auto &y:x.second) { YAML::Node p; p["x"]=x.first; p["y"]=y.first; p["n"]=y.second; n.push_back(p); } return n; }
};
namespace InventoryRightClickControlGate { namespace {
using Clock = std::chrono::steady_clock;
enum { FixtureCount=12, PairCount=FixtureCount*2, PairWorlds=PairCount*2 };
// Stable semantic endpoints. Mutation recipes name these BEFORE native execution.
enum Endpoint { DISPATCH=101, PREHANDLER=102, CALLBACK=103, SEMANTIC=104,
    PIXELS=105, BARRIER=106, TERMINAL=107, RETIRE=108, CONSENT=109,
    RECEIPT=110, DOMAIN=111, ACTIVATION=112, OVERLAY=113, SEND_FAILURE=114,
    HELD_REFUSAL=115, STALE=116, KIND=117, LEDGER=118, RELEASE=119, TRANSITION=120 };
enum Stage { WARM, WAIT_INVENTORY, PREPARE, WAIT_PREPARED, WAIT_I,
    HELD_J, HELD_K, WAIT_CONTEXT, SEND_COMMAND, WAIT_PENDING, CONFIRM,
    WAIT_CANCEL, WAIT_RETIRE, WAIT_ACTION, WAIT_SETTLE, BARRIER_REPLY,
    WAIT_POST, WAIT_STATUS, COLLECT, REPLAY, REPLAY_REPLY, WAIT_REARM,
    TERMINAL_WAIT, TERMINAL_OPEN, TERMINAL_REPLY, WAIT_SEND_FAILURE,
    FAILED_VIEW, FAILED_VIEW_REARM, CROSS_REPLY, LIMIT_SEND, LIMIT_REPLY,
    DOMAIN_REPLY, ORPHAN_DOWN, ORPHAN_RETIRE, ORPHAN_PAIR, FINISHED, COLLECT_ENTRY };
bool enabled=false,running=false,completed=false,pendingSetup=false,moved=false;
bool proposed=false,clickSent=false,terminalReplaced=false,needPickup=false,needPage=false;
int peer=-1,world=0,frames=0,inventoryFrames=0,modalFrames=0,inits=0,clicks=0;
int closes=0,destroyed=0,constructed=0,displayed=0,shown=0,dismissed=0,arranged=0;
int callbacks=0,baselineCallbacks=0,baselineClicks=0,baselineInits=0,baselineArranged=0;
int baselinePage=0,baselineHeld=-1,initialTU=40,targetX=16,targetY=80,prepX=16,prepY=80;
int parentYDown=0,parentYUp=0,parentMouseDown=0,parentMouseUp=0;
int prepPhase=0,frameSerial=0,dispatchedFrame=0,oracleCount=0,parityCount=0;
int lastViewPhase=0,manualLastPhase=-1,manualTicks=0,barrier=0,replay=0,limits=0,domainIndex=0;
size_t baselineSurfaces=0;
Stage stage=WARM;
Game *owner=0; BattlescapeState *state=0; InventoryState *inventory=0,*originalInventory=0;
BattleUnit *player=0,*twin=0; BattleItem *fixtureItem=0;
Position playerPosition(20,20,0),twinPosition(30,30,0);
Clock::time_point started,worldStarted,requestStarted,modalStarted;
std::string master,profile,epoch,capability,challenge,commandId,commandPacket,firstPacket;
std::set<std::string> tokens,commandIds;
std::deque<SDL_Event> inputs;
std::vector<unsigned char> lastPixels;
std::map<int,std::set<std::vector<unsigned char>>> manualPhasePixels;
std::set<int> commandPhases;
struct Snapshot { YAML::Node save,runtime; uint64_t rng; };
Snapshot before,manualResult,after;
// Observe the real parent event route without replacing ordinary handlers.
class FixtureBattlescapeState : public BattlescapeState {
public:
    void handle(Action *action) override {
        SDL_Event *e=action->getDetails();
        if(e->type==SDL_KEYDOWN&&e->key.keysym.sym==SDLK_y) ++parentYDown;
        if(e->type==SDL_KEYUP&&e->key.keysym.sym==SDLK_y) ++parentYUp;
        if(e->type==SDL_MOUSEBUTTONDOWN&&e->button.button==SDL_BUTTON_LEFT) ++parentMouseDown;
        if(e->type==SDL_MOUSEBUTTONUP&&e->button.button==SDL_BUTTON_LEFT) ++parentMouseUp;
        BattlescapeState::handle(action);
    }
};
void check(bool ok,int endpoint) { if(!ok) {
    std::cerr << "INVENTORY_RIGHT_CLICK_FIXTURE_DIAGNOSTIC world-" << world << " stage-" << int(stage)
        << " clicks-" << clicks << " callbacks-" << callbacks << " inits-" << inits
        << " arranged-" << arranged << " destroyed-" << destroyed << " shown-" << shown << std::endl;
    std::cerr << "INVENTORY_RIGHT_CLICK_CONTROL_GATE FAIL assertion-" << endpoint << std::endl;
    std::_Exit(EXIT_FAILURE);
} }
#define require(ok) check((ok),1000+__LINE__)
#define expect(endpoint,ok) check((ok),(endpoint))
bool core() { return profile=="core"; }
bool manual() { return core()&&world%2==0; }
bool closeCase() { return profile.find("-V")!=std::string::npos||profile=="release-Y"||profile=="release-mouse"; }
int fixture() { if(core()) return (world/2)%FixtureCount; if(closeCase()||profile=="disabled"||profile=="close-disabled") return 0; if(profile=="upper-band") return 2; return 1; }
bool heldCase() { return fixture()>0&&fixture()<10; }
bool terminal() { return profile.find("lost-")==0||profile.find("reopen-")==0||profile.find("save-")==0||profile.find("parent-")==0; }
char postKind() { return heldCase()?'I':'V'; }
int phase() { return inventory&&owner->isState(inventory)?InventoryRightClickGateAccess::animation(inventory):InventoryRightClickGateAccess::phase(state->getMap()); }
bool equal(const YAML::Node &a,const YAML::Node &b)
{
    if(a.Type()!=b.Type()||a.Tag()!=b.Tag()) return false;
    if(a.IsScalar()) return a.Scalar()==b.Scalar();
    if(a.IsNull()||!a.IsDefined()) return true;
    if(a.size()!=b.size()) return false;
    if(a.IsSequence()) { for(size_t i=0;i<a.size();++i) if(!equal(a[i],b[i])) return false; return true; }
    std::map<std::string,YAML::Node> right;
    for(auto e:b) { require(e.first.IsScalar()); require(right.insert({e.first.Scalar(),e.second}).second); }
    for(auto e:a) { auto f=right.find(e.first.Scalar()); if(f==right.end()||!equal(e.second,f->second)) return false; }
    return true;
}
YAML::Node actionSnapshot(const BattleAction &a)
{
    YAML::Node n; n["type"]=int(a.type); n["actor"]=a.actor?a.actor->getId():-1;
    n["weapon"]=a.weapon?a.weapon->getId():-1; n["target"]=a.target; n["TU"]=a.TU;
    n["targeting"]=a.targeting; n["result"]=a.result; n["value"]=a.value;
    n["strafe"]=a.strafe; n["run"]=a.run; n["diff"]=a.diff; n["autoShotCounter"]=a.autoShotCounter;
    n["cameraPosition"]=a.cameraPosition; n["desperate"]=a.desperate; n["finalFacing"]=a.finalFacing;
    n["finalAction"]=a.finalAction; n["number"]=a.number;
    for(const Position &p:a.waypoints) n["waypoints"].push_back(p); return n;
}
Snapshot snapshot()
{
    SavedBattleGame *b=owner->getSavedGame()->getSavedBattle(); Snapshot s; s.rng=RNG::getSeed(); s.save=b->save();
    YAML::Node &n=s.runtime; n["selected"]=b->getSelectedUnit()?b->getSelectedUnit()->getId():-1;
    n["current"]=actionSnapshot(*state->getBattleGame()->getCurrentAction());
    n["camera"]=state->getMap()->getCamera()->getMapOffset(); n["busy"]=state->getBattleGame()->isBusy();
    n["panic"]=state->getBattleGame()->getPanicHandled();
    for(BattleState *q:state->getBattleGame()->getStates()) { YAML::Node v; v["kind"]=std::string(typeid(*q).name()); v["action"]=actionSnapshot(q->getAction()); n["queue"].push_back(v); }
    for(BattleUnit *u:*b->getUnits()) {
        YAML::Node v; v["id"]=u->getId(); v["TU"]=u->getTimeUnits(); v["energy"]=u->getEnergy();
        v["status"]=int(u->getStatus()); v["position"]=u->getPosition(); v["direction"]=u->getDirection();
        v["health"]=u->getHealth(); v["morale"]=u->getMorale(); v["kneeled"]=u->isKneeled(); v["visible"]=u->getVisible();
        for(BattleUnit *x:*u->getVisibleUnits()) v["visibleUnits"].push_back(x->getId());
        for(Tile *x:*u->getVisibleTiles()) v["visibleTiles"].push_back(b->getTileIndex(x->getPosition()));
        for(BattleUnit *x:u->getUnitsSpottedThisTurn()) v["spotted"].push_back(x->getId()); n["units"].push_back(v);
    }
    for(int i=0;i<b->getMapSizeXYZ();++i) { YAML::Node t; t["shade"]=b->getTiles()[i]->getShade(); t["visible"]=b->getTiles()[i]->getVisible(); n["tiles"].push_back(t); }
    if(inventory && owner->isState(inventory)) { n["inventoryPage"]=InventoryRightClickGateAccess::page(inventory); BattleItem *held=InventoryRightClickGateAccess::widget(inventory)->getSelectedItem(); n["held"]=held?held->getId():-1; n["stacks"]=InventoryRightClickGateAccess::stacks(inventory); n["text"]=InventoryRightClickGateAccess::text(inventory); n["pressedRight"]=InventoryRightClickGateAccess::pressed(inventory,SDL_BUTTON_RIGHT); n["pressedX2"]=InventoryRightClickGateAccess::pressed(inventory,SDL_BUTTON_X2); }
    for(BattleItem *i:*b->getItems()) { YAML::Node v; v["id"]=i->getId(); v["owner"]=i->getOwner()?i->getOwner()->getId():-1; v["ammo"]=i->getAmmoItem()?i->getAmmoItem()->getId():-1; v["quantity"]=i->getAmmoQuantity(); v["fuse"]=i->getFuseTimer(); v["slot"]=i->getSlot()?i->getSlot()->getId():""; v["x"]=i->getSlotX(); v["y"]=i->getSlotY(); if(i->getTile()) v["tile"]=i->getTile()->getPosition(); n["itemRuntime"].push_back(v); }
    require(s.rng==RNG::getSeed()); return s;
}
void compare(const Snapshot &a,const Snapshot &b) { expect(SEMANTIC,a.rng==b.rng && equal(a.save,b.save) && equal(a.runtime,b.runtime)); }
void fieldsAt(int line,const YAML::Node &n,std::initializer_list<const char*> names)
{
    check(n.IsMap()&&n.size()==names.size(),line); std::set<std::string> left;
    for(const char *s:names) left.insert(s);
    for(auto e:n) check(e.first.IsScalar()&&left.erase(e.first.Scalar())==1,line); check(left.empty(),line);
}
#define fields(...) fieldsAt(__LINE__,__VA_ARGS__)
bool token(const std::string &s) { return s.size()==32&&s.find_first_not_of("0123456789abcdef")==std::string::npos; }
std::string randomToken()
{
    unsigned char b[16]; int fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC); require(fd>=0);
    ssize_t n=read(fd,b,sizeof(b)); close(fd); require(n==sizeof(b)); std::string out; const char *h="0123456789abcdef";
    for(unsigned char c:b) { out+=h[c>>4]; out+=h[c&15]; } require(tokens.insert(out).second); return out;
}
void sendPacket(const std::string &s)
{
    if(s.size()==97||s.size()==99||s.size()==103) { commandIds.insert(s.substr(65,32)); require(commandIds.size()<=size_t(profile=="ledger-limit"?65:64)); }
    require(peer>=0&&send(peer,s.data(),s.size(),MSG_DONTWAIT|MSG_NOSIGNAL)==ssize_t(s.size()));
}
bool available() { char b; ssize_t n=recv(peer,&b,1,MSG_PEEK|MSG_DONTWAIT); require(n>=0||errno==EAGAIN||errno==EWOULDBLOCK); return n>0; }
YAML::Node receive()
{
    char b[140000]; iovec part={b,sizeof(b)}; msghdr m={}; m.msg_iov=&part; m.msg_iovlen=1;
    ssize_t n=recvmsg(peer,&m,MSG_DONTWAIT); require(n>0&&!(m.msg_flags&MSG_TRUNC));
    try { return YAML::Load(std::string(b,n)); } catch(...) { require(false); } return YAML::Node();
}
void unavailable(const YAML::Node &n) { fields(n,{"error"}); require(n["error"].as<std::string>()=="unavailable"); }
void status(const YAML::Node &n,const std::string &value)
{
    fields(n,{"schema","command_id","status"}); expect(RECEIPT,n["schema"].as<std::string>()=="experimental-stance-status/1"&&n["command_id"].as<std::string>()==commandId&&n["status"].as<std::string>()==value);
}
std::vector<unsigned char> display()
{
    // Independent physical SDL surface; never the producer framebuffer/helper.
    SDL_Surface *d=SDL_GetVideoSurface(); require(d&&d->w==640&&d->h==400&&d->format&&d->format->BitsPerPixel==8&&d->format->BytesPerPixel==1&&d->format->palette&&d->format->palette->ncolors==256&&!(d->flags&(SDL_OPENGL|SDL_HWSURFACE|SDL_DOUBLEBUF)));
    std::vector<unsigned char> out; out.reserve(320*200*3); require(SDL_LockSurface(d)==0);
    for(int y=0;y<200;++y) for(int x=0;x<320;++x) {
        const unsigned char *r=static_cast<const unsigned char*>(d->pixels)+(y*2)*d->pitch; SDL_Color c=d->format->palette->colors[r[x*2]];
        for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx) { const unsigned char *q=static_cast<const unsigned char*>(d->pixels)+(y*2+dy)*d->pitch; SDL_Color p=d->format->palette->colors[q[x*2+dx]]; require(p.r==c.r&&p.g==c.g&&p.b==c.b); }
        out.push_back(c.r); out.push_back(c.g); out.push_back(c.b);
    }
    SDL_UnlockSurface(d); return out;
}
std::vector<unsigned char> unhex(const std::string &s,size_t bytes)
{
    const std::string h="0123456789abcdef"; require(s.size()==bytes*2&&s.find_first_not_of(h)==std::string::npos);
    std::vector<unsigned char> out(bytes); for(size_t i=0;i<bytes;++i) out[i]=h.find(s[i*2])*16+h.find(s[i*2+1]); return out;
}
void view(const YAML::Node &v)
{
    fields(v,{"width","height","indices_hex","palette_hex"}); require(v["width"].as<int>()==320&&v["height"].as<int>()==200);
    const auto indices=unhex(v["indices_hex"].as<std::string>(),320*200), palette=unhex(v["palette_hex"].as<std::string>(),256*3);
    lastPixels=display(); lastViewPhase=phase(); std::map<unsigned int,unsigned int> first;
    for(size_t i=0;i<indices.size();++i) { unsigned int rgb=(lastPixels[i*3]<<16)|(lastPixels[i*3+1]<<8)|lastPixels[i*3+2];
        if(!first.count(rgb)) { unsigned int next=first.size(); first[rgb]=next; } expect(PIXELS,first.size()<=256&&indices[i]==first[rgb]);
        for(int c=0;c<3;++c) expect(PIXELS,palette[indices[i]*3+c]==lastPixels[i*3+c]); }
    for(size_t i=first.size()*3;i<palette.size();++i) require(palette[i]==0); ++oracleCount;
}
void context(const YAML::Node &n,char type)
{
    if(type=='I') { fields(n,{"schema","challenge","semantics","view"}); require(n["schema"].as<std::string>()=="experimental-inventory-view/1"); }
    else { fields(n,{"schema","challenge","control_epoch","capability","semantics",type!='O'?"view":"panel"});
        require(n["schema"].as<std::string>()==(type=='U'?"experimental-inventory-right-click-context/1":type=='H'?"experimental-inventory-click-context/1":type=='K'?"experimental-inventory-ground-context/1":type=='J'?"experimental-inventory-close-context/1":type=='V'?"experimental-tactical-context/1":"experimental-stance-context/1"));
        epoch=n["control_epoch"].as<std::string>(); capability=n["capability"].as<std::string>(); require(token(epoch)&&token(capability)&&tokens.insert(capability).second); }
    require(n["challenge"].as<std::string>()==challenge&&n["semantics"].as<std::string>()=="presented_after_request");
    if(type!='O') view(n["view"]); else { fields(n["panel"],{"width","height","rgb_hex"}); require(n["panel"]["width"].as<int>()==320&&n["panel"]["height"].as<int>()==56); auto rgb=unhex(n["panel"]["rgb_hex"].as<std::string>(),320*56*3); lastPixels=display(); require(std::equal(rgb.begin(),rgb.end(),lastPixels.begin()+320*144*3)); }
}
void request(char type) { challenge=randomToken(); sendPacket(std::string(1,type)+challenge); requestStarted=Clock::now(); }
void queue(SDL_Event e) { inputs.push_back(e); require(SDL_PushEvent(&e)==0); }
void motion(int x=160,int y=80) { SDL_Event e={}; e.type=SDL_MOUSEMOTION; e.motion.which=255; e.motion.x=x*2; e.motion.y=y*2; queue(e); }
void keyEdge(SDLKey key,bool down) { SDL_Event e={}; e.type=down?SDL_KEYDOWN:SDL_KEYUP; e.key.which=255; e.key.state=down?SDL_PRESSED:SDL_RELEASED; e.key.keysym.sym=key; e.key.keysym.mod=KMOD_NONE; queue(e); }
void key(SDLKey k) { keyEdge(k,true); keyEdge(k,false); }
void mouseEdge(int x,int y,bool down,int button=SDL_BUTTON_LEFT) { SDL_Event e={}; e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP; e.button.which=255; e.button.x=x*2; e.button.y=y*2; e.button.button=button; e.button.state=down?SDL_PRESSED:SDL_RELEASED; queue(e); }
void click(int x,int y,int button=SDL_BUTTON_LEFT) { motion(x,y); mouseEdge(x,y,true,button); mouseEdge(x,y,false,button); }
void focus(bool gain) { SDL_Event e={}; e.type=SDL_ACTIVEEVENT; e.active.gain=gain?1:0; e.active.state=SDL_APPINPUTFOCUS; queue(e); }
void quiet() { int f,c; Uint16 format; char driver[64]={}; require(Options::mute&&Mix_QuerySpec(&f,&format,&c)>0&&SDL_AudioDriverName(driver,sizeof(driver))&&std::string(driver)=="dummy"&&Mix_Playing(-1)==0&&Mix_PlayingMusic()==0); }
bool idle() { const Uint8 focus=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS; return owner->isState(state)&&(SDL_GetAppState()&focus)==focus&&state->getBattleGame()->getPanicHandled()&&!state->getBattleGame()->isBusy()&&state->getBattleGame()->getStates().empty()&&player->getStatus()==STATUS_STANDING&&!state->getMap()->getProjectile()&&state->getMap()->getCursorType()==CT_NORMAL; }
void finish() { require(inputs.empty()); running=false; completed=true; stage=FINISHED; owner->quit(); }
void replacement() { pendingSetup=true; owner->setState(new State); }
void finishWorld() { if(!core()) { finish(); return; } ++world; if(world==PairWorlds) finish(); else replacement(); }
BattleItem *item(const std::string &name,BattleUnit *unit,const char *slot,int x=0,int y=0)
{
    SavedBattleGame *b=owner->getSavedGame()->getSavedBattle(); Mod *m=owner->getMod(); BattleItem *i=new BattleItem(m->getItem(name,true),b->getCurrentItemId()); b->getItems()->push_back(i);
    if(unit) i->moveToOwner(unit); i->setSlot(m->getInventory(slot,true)); i->setSlotX(x); i->setSlotY(y); return i;
}
BattleItem *arm(BattleUnit *unit)
{
    BattleItem *gun=item(master=="xcom1"?"STR_RIFLE":"STR_JET_HARPOON",unit,"STR_RIGHT_HAND");
    BattleItem *ammo=item(master=="xcom1"?"STR_RIFLE_CLIP":"STR_HARPOON_POD",0,"STR_RIGHT_HAND"); require(gun->setAmmoItem(ammo)==0); return gun;
}
std::string packet(char kind,const std::string &id,const std::string &grant)
{
    char xy[7]; std::snprintf(xy,sizeof(xy),"%03d%03d",targetX,targetY);
    const std::string suffix=(kind=='C'||kind=='G')?"":kind=='B'?"o0":kind=='D'?"sk":kind=='N'?"n0":(kind=='X'||kind=='E')?xy:"160080";
    return std::string(1,kind)+epoch+grant+id+suffix;
}
void command() { proposed=true; commandId=randomToken(); commandPacket=packet('X',commandId,capability); sendPacket(commandPacket); }
void remember()
{
    require(inventory&&owner->isState(inventory)); before=snapshot(); originalInventory=inventory;
    baselineClicks=clicks; baselineCallbacks=callbacks; baselineInits=inits; baselineArranged=arranged;
    baselinePage=InventoryRightClickGateAccess::page(inventory); baselineSurfaces=InventoryRightClickGateAccess::surfaces(inventory);
    BattleItem *held=InventoryRightClickGateAccess::widget(inventory)->getSelectedItem(); baselineHeld=held?held->getId():-1;
    initialTU=player->getTimeUnits();
}
void preserved()
{
    expect(RETIRE,inventory==originalInventory&&owner->isState(inventory)&&closes==0&&destroyed==0&&constructed==1);
    expect(RETIRE,inits==baselineInits&&arranged==baselineArranged&&clicks==baselineClicks&&callbacks==baselineCallbacks);
    expect(RETIRE,InventoryRightClickGateAccess::page(inventory)==baselinePage&&InventoryRightClickGateAccess::surfaces(inventory)==baselineSurfaces);
    compare(before,snapshot());
}
void dispatched(bool settled=false)
{
    expect(DISPATCH,clicks==baselineClicks+1&&inits==baselineInits&&arranged==baselineArranged&&closes==0);
    expect(CALLBACK,callbacks==baselineCallbacks+1);
    if(heldCase()) {
        expect(DISPATCH,inventory==originalInventory&&owner->isState(inventory)&&constructed==1&&destroyed==0);
        expect(SEMANTIC,!InventoryRightClickGateAccess::widget(inventory)->getSelectedItem()&&player->getTimeUnits()==initialTU);
        expect(SEMANTIC,InventoryRightClickGateAccess::page(inventory)==baselinePage);
        if(settled) expect(DISPATCH,!InventoryRightClickGateAccess::modal(inventory)&&InventoryRightClickGateAccess::surfaces(inventory)==baselineSurfaces);
    } else {
        expect(DISPATCH,owner->isState(state));
        if(settled) expect(DISPATCH,destroyed==1&&!inventory&&InventoryRightClickGateAccess::initialized(owner)&&InventoryRightClickGateAccess::deleted(owner)==0);
    }
}
void verifyBarrier()
{
    if(heldCase()) expect(BARRIER,InventoryRightClickGateAccess::awaiting(owner)&&InventoryRightClickGateAccess::destination(owner)==inventory);
    else expect(BARRIER,InventoryRightClickGateAccess::awaitingClose(owner));
}
void retainFrame(const std::vector<unsigned char> &pixels,int p,const char *kind)
{
    require(pixels.size()==320*200*3&&p>=0&&p<=8);
    const std::string file="inventory-right-click-frame-"+std::to_string(world)+"-"+kind+"-"+std::to_string(p)+".ppm";
    std::ofstream out(Options::getUserFolder()+file,std::ios::binary); require(out.good());
    out << "P6\n320 200\n255\n"; out.write(reinterpret_cast<const char *>(pixels.data()),pixels.size()); out.close(); require(out.good());
    std::cerr << "INVENTORY_RIGHT_CLICK_FRAME_EVIDENCE world-" << world << " phase-" << p << " file-" << file << std::endl;
}
bool renderedColor(Surface *surface,int x,int y,SDL_Color &color)
{
    if(!surface->getVisible()||x<surface->getX()||y<surface->getY()||x>=surface->getX()+surface->getWidth()||y>=surface->getY()+surface->getHeight()) return false;
    SDL_Surface *raw=surface->getSurface(); require(raw&&raw->format&&raw->format->BytesPerPixel==1&&raw->format->palette);
    const Uint8 index=surface->getPixel(x-surface->getX(),y-surface->getY());
    if((raw->flags&SDL_SRCCOLORKEY)&&index==raw->format->colorkey) return false;
    require(!(raw->flags&SDL_SRCALPHA)||raw->format->alpha==255); color=raw->format->palette->colors[index]; return true;
}
void verifyOverlayPixels()
{
    const auto physical=display(); Surface *frame=InventoryRightClickGateAccess::banner(inventory), *text=InventoryRightClickGateAccess::overlayText(inventory), *yes=InventoryRightClickGateAccess::yes(inventory), *no=InventoryRightClickGateAccess::no(inventory), *pointer=InventoryRightClickGateAccess::pointer(inventory), *cursor=owner->getCursor();
    Text *label=InventoryRightClickGateAccess::overlayText(inventory);
    const std::string expected="RIGHT ("+std::to_string(targetX)+","+std::to_string(targetY)+"): held = cancel.\nOtherwise closes inventory.\nNormal gravity, lighting/FOV.";
    expect(OVERLAY,label->getText()==expected&&label->getTextHeight()>0&&label->getTextHeight()<=label->getHeight());
    int opaque=0;
    for(int y=frame->getY();y<frame->getY()+32;++y) for(int x=0;x<320;++x) {
        SDL_Color color={}; bool drawn=false; for(Surface *component:{frame,text,yes,no,cursor}) if(renderedColor(component,x,y,color)) drawn=true;
        if(drawn) { ++opaque; const int offset=(y*320+x)*3; expect(OVERLAY,physical[offset]==color.r&&physical[offset+1]==color.g&&physical[offset+2]==color.b); }
    }
    require(opaque>9000);
    int ring=0, exposed=0;
    for(int y=std::max(0,pointer->getY());y<std::min(200,pointer->getY()+17);++y) for(int x=std::max(0,pointer->getX());x<std::min(320,pointer->getX()+17);++x) {
        SDL_Color color={}; if(renderedColor(pointer,x,y,color)) { ++ring; bool covered=false; for(Surface *component:{frame,text,yes,no,cursor}) if(renderedColor(component,x,y,color)) covered=true; if(!covered) ++exposed; const int offset=(y*320+x)*3; expect(OVERLAY,physical[offset]==color.r&&physical[offset+1]==color.g&&physical[offset+2]==color.b); }
    }
    require(ring>0&&exposed>0);
}
void afterView()
{
    dispatched(true);
    expect(BARRIER,!InventoryRightClickGateAccess::awaiting(owner)&&!InventoryRightClickGateAccess::awaitingClose(owner));
    if(core()) {
        if(manual()) { manualResult=snapshot(); manualPhasePixels.clear(); }
        else { compare(manualResult,snapshot()); expect(PIXELS,manualPhasePixels.size()==8); commandPhases.clear(); }
        // Retain the actual post-view frame (possibly transient). drawItems() may
        // clear primer pixels without advancing the existing phase label.
        retainFrame(lastPixels,lastViewPhase,manual()?"manual-post-view":"command-post-view");
        // Enter collection on an actual rendered frame, then require a natural
        // phase advance from that entry in both manual and controlled outcomes.
        manualTicks=0; stage=COLLECT_ENTRY;
    } else if(profile=="ledger-I"||profile=="ledger-V") { after=snapshot(); replay=0; stage=REPLAY; }
    else finish();
}
void beginTerminal()
{
    if(profile.find("save-")==0) { terminalReplaced=true; replacement(); }
    else if(profile.find("parent-")==0) { terminalReplaced=true; pendingSetup=true; owner->setState(new State); }
    else if(heldCase()) { key(Options::keyBattleInventory); stage=TERMINAL_WAIT; }
    else { key(Options::keyBattleInventory); stage=TERMINAL_OPEN; }
}
} // namespace
bool active() { const char *s=std::getenv("OPENXCOM_INVENTORY_RIGHT_CLICK_CONTROL_GATE"); return s&&std::string(s)=="1"; }
void prepare()
{
    enabled=active(); if(!enabled) return; const char *p=std::getenv("OPENXCOM_INVENTORY_RIGHT_CLICK_PROFILE"); profile=p?p:"";
    const std::set<std::string> profiles={"core","deny-held","timeout","disconnect","unseen","held-y","held-mouse","invalid-child-right","invalid-child-x2","release-Y","release-mouse","stale-input","stale-actor","stale-reinit","stale-held","capless-I","cross-kind","ledger-I","ledger-V","ledger-limit","disabled","close-disabled","independent","nontu","base","debug","domain","upper-band","failed-I","failed-V","send-failure-I","send-failure-V","lost-I","reopen-I","reopen-V","save-I","save-V","parent-I","parent-V","orphan-left-held","orphan-left-disconnect","orphan-Y-V","orphan-left-V","orphan-Y-disconnect-V","nested-transition","nested-save-notice"}; require(profiles.count(profile)==1);
    require(!std::getenv("OPENXCOM_STANCE_CONTROL_GATE")&&!std::getenv("OPENXCOM_INVENTORY_OPEN_CONTROL_GATE")&&!std::getenv("OPENXCOM_INVENTORY_CLOSE_CONTROL_GATE")&&!std::getenv("OPENXCOM_INVENTORY_CLICK_CONTROL_GATE")&&!std::getenv("OPENXCOM_INVENTORY_GROUND_CONTROL_GATE"));
    require(Options::language=="en-US"&&!Options::playIntro);
    require(!std::getenv("OPENXCOM_STANCE_FD")&&!std::getenv("OPENXCOM_PRESENTATION_FD")&&!std::getenv("OPENXCOM_AUDIO_FD"));
    int sockets[2]; require(socketpair(AF_UNIX,SOCK_SEQPACKET,0,sockets)==0); peer=sockets[0]; std::string fd=std::to_string(sockets[1]); require(setenv("OPENXCOM_STANCE_FD",fd.c_str(),1)==0&&setenv("SDL_AUDIODRIVER","dummy",1)==0);
    Options::displayWidth=640; Options::displayHeight=400; Options::baseXResolution=320; Options::baseYResolution=200;
    Options::fullscreen=false; Options::allowResize=false; Options::asyncBlit=false; Options::useOpenGL=false;
    Options::useScaleFilter=false; Options::useHQXFilter=false; Options::useXBRZFilter=false;
    Options::fpsCounter=false; Options::debug=false; Options::debugUi=false; Options::traceAI=false;
    Options::autosave=false; Options::pauseMode=0; Options::battleDragScrollButton=SDL_BUTTON_MIDDLE;
    Options::strafe=false; Options::battleNewPreviewPath=PATH_NONE; Options::FPS=60;
    Options::battlescapeScale=0; Options::baseXBattlescape=320; Options::baseYBattlescape=200;
}
void silence() { if(!enabled) return; Mix_HaltChannel(-1); Mix_HaltMusic(); Options::mute=true; quiet(); }
bool loaded(Game *game)
{
    if(!enabled) return false; owner=game; quiet(); YAML::Node hello=receive(); fields(hello,{"schema","control_epoch"}); require(hello["schema"].as<std::string>()=="experimental-stance/1"); epoch=hello["control_epoch"].as<std::string>(); require(token(epoch)&&!available());
    running=true; started=Clock::now(); replacement(); Options::baseXResolution=320; Options::baseYResolution=200; game->getScreen()->resetDisplay(false); game->getCursor()->setVisible(true); game->getFpsCounter()->setVisible(false); return true;
}
void afterCleanup(Game *game)
{
    if(!running||!pendingSetup) return; pendingSetup=false;
    if(terminalReplaced&&profile.find("parent-")==0) {
        state=new FixtureBattlescapeState; game->getSavedGame()->getSavedBattle()->setBattleState(state); game->setState(state);
        inventory=0; moved=false; frames=0; stage=TERMINAL_OPEN; return;
    }
    if(!terminalReplaced) {
        inits=clicks=callbacks=closes=destroyed=constructed=displayed=shown=dismissed=arranged=frames=inventoryFrames=modalFrames=0;
        moved=proposed=clickSent=false; parentYDown=parentYUp=parentMouseDown=parentMouseUp=0; inventory=originalInventory=0; fixtureItem=0; inputs.clear(); stage=WARM; worldStarted=Clock::now();
    }
    Options::maximizeInfoScreens=core()&&world/2>=FixtureCount; Options::battleXcomSpeed=30;
    RNG::setSeed(123456789); std::srand(12345); // Independent world construction only.
    SavedGame *save=new SavedGame; SavedBattleGame *battle=new SavedBattleGame; save->setBattleGame(battle); game->setSavedGame(save);
    playerPosition=Position(20,20,0); twinPosition=Position(30,30,0);
    battle->initMap(40,40,1); battle->setDepth(0); battle->setAmbientSound(-1); Mod *mod=game->getMod(); master=Options::getActiveMaster(); require((master=="xcom1"||master=="xcom2")&&Options::getActiveMods().size()==1);
    MapDataSet *data=mod->getMapDataSet("BLANKS"); require(data); data->loadData(mod->getMCDPatch(data->getName())); battle->getMapDataSets()->push_back(data); MapData *floor=data->getObject(1); require(floor&&floor->getObjectType()==O_FLOOR&&!floor->isNoFloor()&&floor->getTerrainLevel()==0&&floor->getTUCost(MT_WALK)==4);
    for(int i=0;i<battle->getMapSizeXYZ();++i) battle->getTiles()[i]->setMapData(floor,1,0,O_FLOOR);
    Base *base=new Base(mod); save->getBases()->push_back(base); RuleSoldier *rule=mod->getSoldier("STR_SOLDIER",true); Armor *armor=mod->getArmor(rule->getArmor(),true);
    Soldier *a=new Soldier(rule,armor,1); a->setName("INVENTORY FIXTURE"); a->getCurrentStats()->tu=60; a->getCurrentStats()->stamina=60;
    Soldier *b=new Soldier(rule,armor,2); YAML::Node clone=a->save(); clone["id"]=2; b->load(clone,mod,save); base->getSoldiers()->push_back(a); base->getSoldiers()->push_back(b);
    player=new BattleUnit(a,0); twin=new BattleUnit(b,0); player->setPosition(playerPosition); player->setDirection(0); twin->setPosition(twinPosition); twin->setDirection(4); twin->setTimeUnits(40);
    battle->getUnits()->push_back(player); battle->getUnits()->push_back(twin); battle->resetUnitTiles(); battle->setSelectedUnit(player); battle->initUtilities(mod);
    const int f=fixture(); initialTU=f==8?0:40; player->setTimeUnits(initialTU);
    RuleInventory *ground=mod->getInventory("STR_GROUND",true); require(ground->getX()==0&&(320-ground->getX())/16==20&&(200-ground->getY())/16==3);
    auto addGround=[&](BattleItem *i) { player->getTile()->addItem(i,ground); i->setSlotX(200+int(player->getTile()->getInventory()->size())); i->setSlotY(1); return i; };
    targetX=16; targetY=80; prepX=16; prepY=80; needPickup=heldCase(); needPage=f==6||f==11;
    const std::string ammo=master=="xcom1"?"STR_RIFLE_CLIP":"STR_HARPOON_POD";
    const std::string grenade=master=="xcom1"?"STR_GRENADE":"STR_MAGNA_BLAST_GRENADE";
    if(f==1||f==8) fixtureItem=item(ammo,player,"STR_RIGHT_HAND");
    if(f==2) fixtureItem=item("STR_MEDI_KIT",player,"STR_RIGHT_HAND");
    if(f==3||f==5) fixtureItem=addGround(item(ammo,0,"STR_GROUND"));
    if(f==4) fixtureItem=addGround(item("STR_MEDI_KIT",0,"STR_GROUND"));
    if(f==5) for(int i=0;i<3;++i) addGround(item(ammo,0,"STR_GROUND"));
    if(needPage) for(int i=0;i<25;++i) { BattleItem *med=addGround(item("STR_MEDI_KIT",0,"STR_GROUND")); med->setPainKillerQuantity(i%11); med->setHealQuantity(i/11); }
    if(f==7) { fixtureItem=addGround(item(grenade,0,"STR_GROUND")); fixtureItem->setFuseTimer(0); }
    if(f==9||f==10) fixtureItem=arm(player);
    if((f>=3&&f<=7)||f==11) { targetX=8; targetY=160; prepX=8; prepY=160; }
    battle->getTileEngine()->calculateSunShading(); battle->getTileEngine()->calculateTerrainLighting(); battle->getTileEngine()->calculateUnitLighting(); battle->getTileEngine()->recalculateFOV();
    if(profile=="debug") battle->setDebugMode();
    state=new FixtureBattlescapeState; battle->setBattleState(state); game->setState(state);
    if(terminalReplaced) { inventory=0; moved=false; frames=0; stage=TERMINAL_OPEN; }
}
namespace {
bool hasGround() {
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND
    return profile!="independent";
#else
    return false;
#endif
}
bool hasClick() {
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK
    return profile!="independent";
#else
    return false;
#endif
}
std::vector<char> otherCommands() {
    std::vector<char> kinds={'B','D','N','R','L','C'};
    if(hasGround()) kinds.push_back('G'); if(hasClick()) kinds.push_back('E'); return kinds;
}
std::vector<char> allReads() {
    std::vector<char> kinds={'O','V','I','J','U'};
    if(hasGround()) kinds.push_back('K'); if(hasClick()) kinds.push_back('H'); return kinds;
}
std::vector<char> blockedReads()
{
    std::vector<char> kinds=allReads();
    kinds.erase(std::remove(kinds.begin(),kinds.end(),postKind()),kinds.end()); return kinds;
}
bool denial() { return profile=="deny-held"||profile=="timeout"||profile=="unseen"||profile=="held-y"||profile=="held-mouse"||profile=="invalid-child-right"||profile=="invalid-child-x2"||profile=="domain"; }
bool stale() { return profile.find("stale-")==0||profile=="capless-I"; }
void queryStatus() { sendPacket("S"+commandId); stage=WAIT_CANCEL; }
void startBarrier() { barrier=0; request(blockedReads()[0]); stage=BARRIER_REPLY; }
}
void beforeEvents(Game *game)
{
    if(!running||pendingSetup) return;
    require(Clock::now()-started<std::chrono::seconds(core()?250:profile=="timeout"?155:45));
    require(Clock::now()-worldStarted<std::chrono::seconds(profile=="timeout"?145:35)); quiet();
    if(!moved) { motion(); moved=true; return; }
    if(stage==PREPARE) { prepPhase=0; stage=WAIT_PREPARED; }
    else if(stage==WAIT_PREPARED&&inputs.empty()) {
        if(prepPhase==0) { ++prepPhase; if(needPage) { click(304,144); return; } }
        if(prepPhase==1) { ++prepPhase; if(needPage) { require(InventoryRightClickGateAccess::page(inventory)==20); fixtureItem=player->getItem(game->getMod()->getInventory("STR_GROUND",true),20,0); require(fixtureItem); } if(needPickup) { click(prepX,prepY); return; } }
        if(prepPhase==2) { ++prepPhase; if(needPickup) require(InventoryRightClickGateAccess::widget(inventory)->getSelectedItem()==fixtureItem); if(fixture()==8) { click(88,176); return; } }
        if(prepPhase==3) {
            if((fixture()==7||fixture()==8)&&InventoryRightClickGateAccess::warning(inventory)->getVisible()) return;
            ++prepPhase; if(needPickup) require(InventoryRightClickGateAccess::widget(inventory)->getSelectedItem()==fixtureItem);
            motion(160,profile=="upper-band"?136:80);
            if(profile=="invalid-child-right"||profile=="invalid-child-x2") mouseEdge(160,80,true,profile=="invalid-child-right"?SDL_BUTTON_RIGHT:SDL_BUTTON_X2);
            return;
        }
        remember(); request('I'); stage=WAIT_I;
    } else if(stage==SEND_COMMAND) {
        if(profile=="stale-input") motion(151,81);
        if(profile=="stale-actor") { game->getSavedGame()->getSavedBattle()->setSelectedUnit(twin); game->getSavedGame()->getSavedBattle()->setSelectedUnit(player); }
        if(profile=="stale-reinit") inventory->init();
        if(profile=="stale-held") { InventoryRightClickGateAccess::widget(inventory)->setSelectedItem(0); InventoryRightClickGateAccess::widget(inventory)->setSelectedItem(fixtureItem); }
        if(profile=="domain") { const std::pair<int,int> points[]={{0,63},{31,110},{128,63},{159,110},{0,152},{319,199},{32,80}}; targetX=points[domainIndex].first; targetY=points[domainIndex].second; }
        command(); stage=WAIT_PENDING;
    } else if(stage==WAIT_PENDING&&available()) {
        const YAML::Node n=receive();
        if(profile=="domain"&&domainIndex==6) { expect(DOMAIN,n.IsMap()&&n.size()==1&&n["error"]&&n["error"].as<std::string>()=="unavailable"); preserved(); finish(); return; }
        if(stale()) { expect(STALE,n["status"]&&n["status"].as<std::string>()=="unavailable"); status(n,"unavailable"); expect(STALE,clicks==baselineClicks&&closes==0&&destroyed==0); finish(); return; }
        expect(CONSENT,n["status"]&&n["status"].as<std::string>()=="pending_confirmation"); status(n,"pending_confirmation");
        expect(CONSENT,game->isState(inventory)&&shown==domainIndex+1&&clicks==baselineClicks&&arranged==baselineArranged&&closes==0&&destroyed==0&&inits==baselineInits);
        stage=profile=="unseen"?WAIT_RETIRE:CONFIRM;
    } else if(stage==CONFIRM&&modalFrames>0) {
        require(game->isState(inventory)&&InventoryRightClickGateAccess::modal(inventory));
        if(profile=="orphan-left-held"||profile=="orphan-left-disconnect") {
            Surface *yes=InventoryRightClickGateAccess::yes(inventory);
            motion(yes->getX()+yes->getWidth()/2,yes->getY()+yes->getHeight()/2);
            mouseEdge(yes->getX()+yes->getWidth()/2,yes->getY()+yes->getHeight()/2,true); stage=ORPHAN_DOWN;
        } else if(profile=="orphan-Y-V"||profile=="orphan-left-V"||profile=="orphan-Y-disconnect-V") {
            if(profile=="orphan-left-V") { Surface *yes=InventoryRightClickGateAccess::yes(inventory); motion(yes->getX()+10,yes->getY()+5); mouseEdge(yes->getX()+10,yes->getY()+5,true); }
            keyEdge(SDLK_y,true); clickSent=true; stage=WAIT_ACTION; // Deliberately omit UP.
        } else if(profile=="release-mouse") {
            Surface *yes=InventoryRightClickGateAccess::yes(inventory);
            motion(yes->getX()+10,yes->getY()+5); mouseEdge(yes->getX()+10,yes->getY()+5,true);
            key(SDLK_y); mouseEdge(yes->getX()+10,yes->getY()+5,false); clickSent=true; stage=WAIT_ACTION;
        } else if(profile=="disconnect") { close(peer); peer=-1; stage=WAIT_RETIRE; }
        else if(profile=="timeout") { SDL_Delay(120100); key(SDLK_y); stage=WAIT_RETIRE; }
        else if(profile=="held-y") { keyEdge(SDLK_y,true); keyEdge(SDLK_y,false); stage=WAIT_RETIRE; }
        else if(profile=="held-mouse") { mouseEdge(280,113,false); stage=WAIT_RETIRE; }
        else if(profile=="invalid-child-right"||profile=="invalid-child-x2") {
            if(profile=="invalid-child-right") InventoryRightClickGateAccess::yes(inventory)->setVisible(false);
            else InventoryRightClickGateAccess::yes(inventory)->setX(241);
            const int button=profile=="invalid-child-right"?SDL_BUTTON_RIGHT:SDL_BUTTON_X2;
            mouseEdge(160,80,true,button); mouseEdge(160,80,false,button); stage=WAIT_RETIRE;
        } else if(denial()) { key(SDLK_ESCAPE); stage=WAIT_RETIRE; }
        else if(core()&&world%4==3) {
            Surface *yes=InventoryRightClickGateAccess::yes(inventory); click(yes->getX()+yes->getWidth()/2,yes->getY()+yes->getHeight()/2); clickSent=true; stage=WAIT_ACTION;
        } else { key(SDLK_y); clickSent=true; stage=WAIT_ACTION; }
    } else if(stage==ORPHAN_DOWN&&inputs.empty()) {
        if(profile=="orphan-left-disconnect") { close(peer); peer=-1; }
        else key(SDLK_ESCAPE);
        stage=ORPHAN_RETIRE;
    } else if(stage==ORPHAN_RETIRE&&inputs.empty()&&inventory&&game->isState(inventory)&&!InventoryRightClickGateAccess::modal(inventory)) {
        expect(RELEASE,clicks==baselineClicks&&callbacks==baselineCallbacks&&InventoryRightClickGateAccess::widget(inventory)->getSelectedItem()==fixtureItem);
        // Missing original UP cannot capture this NEW ordinary DOWN/UP pair.
        click(16,80); stage=ORPHAN_PAIR;
    } else if(stage==ORPHAN_PAIR&&inputs.empty()) {
        if(heldCase()) {
            expect(RELEASE,clicks==baselineClicks+1&&callbacks==baselineCallbacks+1&&!InventoryRightClickGateAccess::widget(inventory)->getSelectedItem());
            expect(RELEASE,InventoryRightClickGateAccess::consumedButtons(inventory)==0&&inits==baselineInits&&arranged==baselineArranged&&closes==0&&destroyed==0);
        } else if(profile=="orphan-left-V") expect(RELEASE,parentMouseDown==1&&parentMouseUp==1);
        else expect(RELEASE,parentYDown==1&&parentYUp==1);
        finish();
    } else if(stage==WAIT_RETIRE&&inputs.empty()&&inventory&&game->isState(inventory)&&!InventoryRightClickGateAccess::modal(inventory)) {
        preserved(); expect(RETIRE,InventoryRightClickGateAccess::consumedButtons(inventory)==0);
        if(peer<0||!proposed) { expect(RETIRE,InventoryRightClickGateAccess::widget(inventory)->getSelectedItem()==fixtureItem); finish(); }
        else queryStatus();
    } else if(stage==WAIT_CANCEL&&available()) {
        YAML::Node n=receive(); expect(CONSENT,n["status"]&&n["status"].as<std::string>()=="unavailable"); status(n,"unavailable"); preserved();
        if(profile=="domain") { ++domainIndex; motion(); request('U'); stage=WAIT_CONTEXT; } else finish();
    } else if(stage==BARRIER_REPLY&&available()) {
        YAML::Node n=receive(); expect(BARRIER,n.IsMap()&&n.size()==1&&n["error"]&&n["error"].as<std::string>()=="unavailable"); verifyBarrier(); dispatched(true);
        if(++barrier<int(blockedReads().size())) request(blockedReads()[barrier]);
        else if(terminal()) beginTerminal();
        else if(profile.find("send-failure-")==0) { require(shutdown(peer,SHUT_RD)==0); request(postKind()); stage=WAIT_SEND_FAILURE; }
        else if(profile=="failed-I") { InventoryRightClickGateAccess::widget(inventory)->setVisible(false); request('I'); stage=FAILED_VIEW; }
        else if(profile=="failed-V") { game->pushState(new State); request('V'); stage=FAILED_VIEW; }
        else { request(postKind()); stage=WAIT_POST; }
    } else if(stage==WAIT_SEND_FAILURE&&!game->getStanceController()->enabled()) {
        expect(SEND_FAILURE,heldCase()?InventoryRightClickGateAccess::awaiting(game):InventoryRightClickGateAccess::awaitingClose(game)); dispatched(true); finish();
    } else if(stage==FAILED_VIEW&&available()) {
        unavailable(receive()); verifyBarrier();
        if(heldCase()) { InventoryRightClickGateAccess::widget(inventory)->setVisible(true); request('U'); stage=FAILED_VIEW_REARM; }
        else { game->popState(); barrier=0; stage=TERMINAL_WAIT; }
    } else if(stage==FAILED_VIEW_REARM&&available()) { unavailable(receive()); verifyBarrier(); request('I'); stage=WAIT_POST; }
    else if(stage==WAIT_STATUS&&available()) { status(receive(),"dispatched"); afterView(); }
    else if(stage==REPLAY) {
        commandId=commandPacket.substr(65,32);
        if(replay==0) sendPacket(commandPacket);
        else if(replay==1) sendPacket("S"+commandId);
        else if(replay==2) { std::string conflict=commandPacket; conflict.replace(33,32,randomToken()); sendPacket(conflict); }
        else if(replay<int(otherCommands().size())+3) sendPacket(packet(otherCommands()[replay-3],commandId,capability));
        else { expect(LEDGER,shown==1); finish(); return; }
        stage=REPLAY_REPLY;
    } else if(stage==REPLAY_REPLY&&available()) {
        YAML::Node n=receive(); expect(LEDGER,n["status"]&&n["status"].as<std::string>()==(replay<2?"dispatched":"unavailable")); status(n,replay<2?"dispatched":"unavailable");
        compare(after,snapshot()); dispatched(true); ++replay; stage=REPLAY;
    } else if(stage==CROSS_REPLY&&available()) {
        YAML::Node n=receive(); expect(KIND,n["status"]&&n["status"].as<std::string>()=="unavailable"); status(n,"unavailable"); preserved();
        if(++replay<int(otherCommands().size())) { commandId=randomToken(); sendPacket(packet(otherCommands()[replay],commandId,capability)); }
        else if(replay==int(otherCommands().size())) { if(hasClick()) { request('H'); stage=WAIT_REARM; } else finish(); }
        else finish();
    } else if(stage==LIMIT_SEND) {
        std::vector<char> kinds=otherCommands(); kinds.push_back('X'); commandId=randomToken(); std::string p=packet(kinds[limits%kinds.size()],commandId,randomToken()); if(limits==0) firstPacket=p; sendPacket(p); stage=LIMIT_REPLY;
    } else if(stage==LIMIT_REPLY&&available()) {
        YAML::Node n=receive();
        if(limits<64) { expect(LEDGER,n["status"]&&n["status"].as<std::string>()=="unavailable"); status(n,"unavailable"); compare(before,snapshot()); ++limits; stage=LIMIT_SEND; }
        else { expect(LEDGER,n["error"]&&n["error"].as<std::string>()=="unavailable"&&commandIds.size()==65); expect(LEDGER,clicks==0&&closes==0); finish(); }
    } else if(stage==TERMINAL_WAIT&&inputs.empty()&&idle()&&InventoryRightClickGateAccess::initialized(game)&&InventoryRightClickGateAccess::deleted(game)==0) {
        if(profile=="reopen-I") { key(Options::keyBattleInventory); stage=TERMINAL_OPEN; }
        else { barrier=0; request('O'); stage=TERMINAL_REPLY; }
    } else if(stage==TERMINAL_OPEN&&inputs.empty()) {
        if(inventory&&game->isState(inventory)&&InventoryRightClickGateAccess::initialized(game)) { barrier=0; request('O'); stage=TERMINAL_REPLY; }
        else if(idle()&&++frames>=2) key(Options::keyBattleInventory);
    } else if(stage==TERMINAL_REPLY&&available()) {
        YAML::Node n=receive(); expect(TERMINAL,n.IsMap()&&n.size()==1&&n["error"]&&n["error"].as<std::string>()=="unavailable");
        if(++barrier==int(allReads().size())) finish(); else request(allReads()[barrier]);
    }
}
bool acceptEvent(const SDL_Event &e)
{
    if(!running) return true; if(e.type==SDL_QUIT) return true;
    if(e.type==SDL_ACTIVEEVENT&&(inputs.empty()||inputs.front().type!=SDL_ACTIVEEVENT)) return true;
    const bool scripted=e.type==SDL_ACTIVEEVENT||(e.type==SDL_MOUSEMOTION&&e.motion.which==255)||((e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEBUTTONUP)&&e.button.which==255)||((e.type==SDL_KEYDOWN||e.type==SDL_KEYUP)&&e.key.which==255);
    if(!scripted) return e.type==SDL_MOUSEMOTION&&inventory&&owner->isState(inventory);
    require(!inputs.empty()&&e.type==inputs.front().type); SDL_Event x=inputs.front(); inputs.pop_front();
    if(e.type==SDL_MOUSEMOTION) require(e.motion.x==x.motion.x&&e.motion.y==x.motion.y);
    else if(e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEBUTTONUP) require(e.button.x==x.button.x&&e.button.y==x.button.y&&e.button.button==x.button.button&&e.button.state==x.button.state);
    else if(e.type==SDL_KEYDOWN||e.type==SDL_KEYUP) require(e.key.keysym.sym==x.key.keysym.sym&&e.key.state==x.key.state);
    else require(e.active.state==x.active.state&&e.active.gain==x.active.gain); return true;
}
void afterEvents(Game *)
{
    if(!running||pendingSetup) return;
    if(stage==WAIT_ACTION&&inputs.empty()) {
        if(profile=="nested-transition"||profile=="nested-save-notice") {
            expect(TRANSITION,clicks==baselineClicks+1&&callbacks==baselineCallbacks+1&&InventoryRightClickGateAccess::awaiting(owner)&&!InventoryRightClickGateAccess::destination(owner));
            stage=TERMINAL_OPEN; return;
        }
        dispatched(); dispatchedFrame=frameSerial; motion(); stage=WAIT_SETTLE;
    }
}
void beforeFrame(Game *game,State *)
{
    if(!running||pendingSetup) return;
    if(stage==WARM&&moved&&inputs.empty()&&idle()&&++frames>=2) {
        before=snapshot(); if(profile=="ledger-limit") { stage=LIMIT_SEND; return; }
        if(profile=="nontu") game->pushState(new InventoryState(false,state)); else key(Options::keyBattleInventory);
        stage=WAIT_INVENTORY;
    }
}
void afterFrame(Game *game)
{
    if(!running||pendingSetup) return; ++frameSerial;
    if(inventory&&game->isState(inventory)&&InventoryRightClickGateAccess::modal(inventory)) { ++modalFrames; if(modalFrames==1) retainFrame(display(),phase(),"overlay"); verifyOverlayPixels(); }
    if(stage==WAIT_INVENTORY&&inventory&&game->isState(inventory)&&inputs.empty()&&++inventoryFrames>=2) {
        require(inits==1&&constructed==1&&clicks==0&&closes==0&&destroyed==0&&displayed>0); stage=PREPARE;
    }
    if(stage==COLLECT_ENTRY) {
        dispatched(true); lastPixels=display(); lastViewPhase=phase();
        retainFrame(lastPixels,lastViewPhase,manual()?"manual-collection-entry":"command-collection-entry");
        manualLastPhase=lastViewPhase; stage=COLLECT; return;
    }
    if(stage==COLLECT) {
        dispatched(true); const int p=phase();
        require(p>=0&&p<=(heldCase()?8:7));
        if((!heldCase()||p>0)&&p!=manualLastPhase) {
            manualLastPhase=p; ++manualTicks; lastPixels=display(); retainFrame(lastPixels,p,manual()?"manual":"command");
            if(manual()) manualPhasePixels[p].insert(lastPixels);
            else { expect(PIXELS,manualPhasePixels.count(p)&&manualPhasePixels.at(p).count(lastPixels)==1); commandPhases.insert(p); }
        }
        // Settled-natural-phase parity: eight WHOLE physical frames after an
        // observed collection-entry phase advance. No masks, timer reset, forced redraw,
        // or matching by elapsed wall-clock time. Post-view and entry frames are
        // retained separately; paired transient equivalence is not claimed.
        if((manual()?manualPhasePixels.size():commandPhases.size())==8&&manualTicks>=8) {
            compare(manualResult,snapshot()); if(!manual()) ++parityCount; finishWorld(); return;
        }
    }
    if(stage==WAIT_SETTLE&&frameSerial>dispatchedFrame&&inputs.empty()&&
        (heldCase()||(idle()&&InventoryRightClickGateAccess::initialized(game)&&InventoryRightClickGateAccess::deleted(game)==0))) {
        dispatched(true);
        if(!heldCase()) expect(RELEASE,parentYDown==0&&parentYUp==0&&parentMouseDown==0&&parentMouseUp==0);
        if(profile=="orphan-Y-V"||profile=="orphan-left-V"||profile=="orphan-Y-disconnect-V") {
            if(profile=="orphan-Y-disconnect-V") { close(peer); peer=-1; }
            if(profile=="orphan-left-V") click(1,199); else key(SDLK_y);
            stage=ORPHAN_PAIR; return;
        }
        if(manual()) { request(postKind()); stage=WAIT_POST; } else { verifyBarrier(); startBarrier(); }
    }
    if(stage==WAIT_I&&available()) {
        if(profile=="nontu"||profile=="debug") { unavailable(receive()); preserved(); request('U'); stage=WAIT_CONTEXT; return; }
        context(receive(),'I'); preserved();
        if(manual()) { click(targetX,targetY,SDL_BUTTON_RIGHT); stage=WAIT_ACTION; }
        else {
            if(profile=="base") InventoryRightClickGateAccess::setBaseMode(inventory);
            if(profile=="held-y") keyEdge(SDLK_y,true);
            if(profile=="held-mouse") { motion(280,113); mouseEdge(280,113,true); }
            if(heldCase()&&profile!="independent") { request('J'); stage=HELD_J; }
            else { request('U'); stage=WAIT_CONTEXT; }
        }
    } else if(stage==HELD_J&&available()) {
        YAML::Node n=receive(); expect(HELD_REFUSAL,n["error"]&&n["error"].as<std::string>()=="unavailable"); preserved(); if(hasGround()) { request('K'); stage=HELD_K; } else { request('U'); stage=WAIT_CONTEXT; }
    } else if(stage==HELD_K&&available()) {
        YAML::Node n=receive(); expect(HELD_REFUSAL,n["error"]&&n["error"].as<std::string>()=="unavailable"); preserved(); request('U'); stage=WAIT_CONTEXT;
    } else if(stage==WAIT_CONTEXT&&available()) {
        YAML::Node n=receive();
        if(profile=="disabled"||profile=="close-disabled"||profile=="nontu"||profile=="base"||profile=="debug") {
            expect(ACTIVATION,n["error"]&&n["error"].as<std::string>()=="unavailable"); expect(ACTIVATION,clicks==baselineClicks&&closes==0&&destroyed==0); finish(); return;
        }
        if(profile=="held-mouse"&&n["error"]) { unavailable(n); preserved(); mouseEdge(280,113,false); stage=WAIT_RETIRE; return; }
        context(n,'U'); preserved();
        if(profile=="capless-I") { request('I'); stage=WAIT_REARM; }
        else if(profile=="cross-kind") { replay=0; commandId=randomToken(); sendPacket(packet('B',commandId,capability)); stage=CROSS_REPLY; }
        else stage=SEND_COMMAND;
    } else if(stage==WAIT_REARM&&available()) {
        if(profile=="capless-I") { context(receive(),'I'); preserved(); stage=SEND_COMMAND; }
        else { context(receive(),'H'); commandId=randomToken(); sendPacket(packet('X',commandId,capability)); stage=CROSS_REPLY; }
    } else if(stage==WAIT_POST&&available()) {
        context(receive(),postKind()); require(frameSerial>dispatchedFrame); dispatched(true);
        if(clickSent) { sendPacket("S"+commandId); stage=WAIT_STATUS; } else afterView();
    }
}
void inventoryConstructed(InventoryState *who) { if(running&&!pendingSetup) { require(!inventory); inventory=who; ++constructed; } }
void inventoryInitialized(InventoryState *who) { if(!running||pendingSetup) return; require(!inventory||inventory==who); inventory=who; ++inits; }
void inventoryDisplayed(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++displayed; }
void inventoryMouseOverEntered(Inventory *who,Action *action,State *parent)
{
    if(!running||pendingSetup||stage!=WAIT_ACTION||manual()||!inventory||who!=InventoryRightClickGateAccess::widget(inventory)) return;
    // Ordinary restoration may hover before a queued confirmation UP drains.
    // Only the synthetic RIGHT callback belongs to synchronous dispatch here.
    if(!action||action->getDetails()->type!=SDL_MOUSEBUTTONUP||action->getDetails()->button.button!=SDL_BUTTON_RIGHT) return;
    expect(PREHANDLER,InventoryRightClickGateAccess::unresolved(owner));
    if(profile=="nested-transition") {
        // A real intervening push+pop restores the same pointer/topology but
        // must permanently poison the synchronous transition witness.
        owner->pushState(new State); owner->popState();
    } else if(profile=="nested-save-notice") {
        // Exercise the actual lifecycle notification hook without freeing save
        // objects while an ordinary callback still holds live fixture pointers.
        owner->getStanceController()->beforeSaveChange();
    }
    expect(DISPATCH,parent==inventory&&action&&action->getSender()==who&&action->getAbsoluteXMouse()==targetX&&action->getAbsoluteYMouse()==targetY);
}
void inventoryClickEntered(Inventory *who,Action *action,State *parent)
{
    if(!running||pendingSetup||!inventory||who!=InventoryRightClickGateAccess::widget(inventory)) return;
    ++clicks;
    if(stage==WAIT_ACTION) {
        if(!manual()) expect(PREHANDLER,InventoryRightClickGateAccess::unresolved(owner));
        expect(DISPATCH,parent==inventory&&action&&action->getSender()==who&&action->getDetails()->button.button==SDL_BUTTON_RIGHT);
        expect(DISPATCH,action->getAbsoluteXMouse()==targetX&&action->getAbsoluteYMouse()==targetY);
        Surface *selection=InventoryRightClickGateAccess::selection(inventory);
        expect(DISPATCH,selection->getX()==targetX-selection->getWidth()/2-who->getX()&&selection->getY()==targetY-selection->getHeight()/2-who->getY());
    }
}
void inventoryInvClickEntered(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++callbacks; }
void inventoryCloseEntered(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++closes; }
void inventoryDestroyed(InventoryState *who) { if(running&&(!pendingSetup||terminalReplaced)&&who==inventory) { ++destroyed; inventory=0; } }
void clickOverlayShown(InventoryState *who)
{
    if(running&&!pendingSetup&&who==inventory) {
        ++shown; modalStarted=Clock::now(); modalFrames=0;
        Surface *band=InventoryRightClickGateAccess::banner(who),*yes=InventoryRightClickGateAccess::yes(who),*no=InventoryRightClickGateAccess::no(who),*sprite=InventoryRightClickGateAccess::selection(who),*mark=InventoryRightClickGateAccess::pointer(who);
        expect(OVERLAY,band->getX()==0&&band->getWidth()==320&&band->getHeight()==32&&(band->getY()==24||band->getY()==112));
        expect(OVERLAY,sprite->getY()+sprite->getHeight()<=band->getY()||sprite->getY()>=band->getY()+32);
        expect(OVERLAY,targetY<band->getY()||targetY>=band->getY()+32);
        expect(OVERLAY,yes->getX()==242&&yes->getY()==band->getY()+1&&yes->getWidth()==76&&yes->getHeight()==14);
        expect(OVERLAY,no->getX()==242&&no->getY()==band->getY()+17&&no->getWidth()==76&&no->getHeight()==14);
        for(int y=0;y<200;++y) for(int x=0;x<320;++x) {
            const int expected=x>=242&&x<318?(y>=band->getY()+1&&y<band->getY()+15?1:y>=band->getY()+17&&y<band->getY()+31?2:0):0;
            expect(OVERLAY,InventoryRightClickGateAccess::choice(who,x,y)==expected);
        }
        expect(OVERLAY,mark->getX()==targetX-8&&mark->getY()==targetY-8&&mark->getWidth()==17&&mark->getHeight()==17);
        for(int y=0;y<17;++y) for(int x=0;x<17;++x) { const int d=(x-8)*(x-8)+(y-8)*(y-8); expect(OVERLAY,(mark->getPixel(x,y)!=0)==(d>=25&&d<=36)); }
        if(profile=="upper-band") expect(OVERLAY,band->getY()==24);
        if(profile=="unseen") { expect(CONSENT,shown==1&&modalFrames==0&&clicks==baselineClicks); key(SDLK_y); }
    }
}
void clickOverlayDismissed(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++dismissed; }
void inventoryGroundArranged(Inventory *) { if(running&&!pendingSetup) ++arranged; }
void kneelClicked(BattlescapeState *) { if(running&&!pendingSetup) require(false); }
void selectionClicked(BattlescapeState *,bool) { if(running&&!pendingSetup) require(false); }
int exitStatus()
{
    if(enabled&&completed) {
        if(core()) require(world==PairWorlds&&parityCount==PairCount&&oracleCount>=PairWorlds*2);
        std::cout << "INVENTORY_RIGHT_CLICK_CONTROL_GATE PASS {\"schema\":\"inventory-right-click-control-gate/1\",\"master\":\"" << master << "\",\"profile\":\"" << profile << "\",\"passed\":true}" << std::endl;
    }
    if(peer>=0) { close(peer); peer=-1; } return enabled&&!completed?EXIT_FAILURE:EXIT_SUCCESS;
}
} }
#undef require
#undef expect
#undef fields
