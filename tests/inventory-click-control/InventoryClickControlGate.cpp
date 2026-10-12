// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
// A finite, default-OFF native ordinary inventory LEFT click and pixels-only read fixture.
// Normal events, initialization, timers, blits and flips remain native.
// Oracles remain private; test-only frame evidence is retained in the disposable
// user directory and copied by the private runner, never sent on the public wire.
#include "InventoryClickControlGate.h"
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
struct InventoryClickGateAccess {
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
    static bool awaiting(Game *g) { return g->getStanceController()->_awaitingInventoryView; }
    static InventoryState *destination(Game *g) { return g->getStanceController()->_inventoryDestination; }
    static YAML::Node stacks(InventoryState *s) { YAML::Node n; for (const auto &x:s->_inv->_stackLevel) for (const auto &y:x.second) { YAML::Node p; p["x"]=x.first; p["y"]=y.first; p["n"]=y.second; n.push_back(p); } return n; }
};
namespace InventoryClickControlGate { namespace {
using Clock = std::chrono::steady_clock;
enum { FixtureCount = 24, PairCount = FixtureCount * 2, PairWorlds = PairCount * 2 };
enum Stage { WARM, WAIT_INVENTORY, PREPARE_INVENTORY, WAIT_PREPARED, WAIT_I,
    WAIT_HELD_READ, WAIT_HELD_REFUSAL, WAIT_CONTEXT, SEND_COMMAND, WAIT_PENDING,
    CONFIRM, WAIT_INPUT, WAIT_CANCEL_STATUS, WAIT_RETIRE, WAIT_DISPATCH,
    WAIT_SETTLE, BARRIER_REPLY, WAIT_POST_I, WAIT_STATUS, COLLECT_MANUAL,
    WAIT_CAPLESS, CROSS_REPLY, REPLAY_SEND, REPLAY_REPLY, WAIT_REARM,
    LIMIT_SEND, LIMIT_REPLY, LIMIT_FINAL, WAIT_DUPLICATE_PENDING,
    WAIT_TERMINAL_CLOSE, WAIT_TERMINAL_OPEN, TERMINAL_REQUEST, TERMINAL_REPLY,
    WAIT_FAILED_I, WAIT_FAILED_K, WAIT_SEND_FAILURE, WAIT_WARNING_REFUSAL, WAIT_WARNING_FADE, WAIT_MANUAL_CANCEL, DOMAIN_REPLY, FINISHED };
bool enabled=false, running=false, completed=false, pendingSetup=false, moved=false;
bool prepared=false, retry=false, terminalReplaced=false, clickSent=false, lostOriginal=false; int orphanPhase=0; bool orphanAdvance(Game *game);
int peer=-1, world=0, frames=0, inventoryFrames=0, modalFrames=0, inits=0, clicks=0;
int closes=0, destroyed=0, constructed=0, displayed=0, shown=0, dismissed=0, arranged=0;
int baselineInits=0, baselineArranged=0, baselineClicks=0, baselinePage=0, baselineAnimation=0;
int replay=0, barrier=0, limits=0, oracleCount=0, parityCount=0, frameSerial=0, dispatchedFrame=0;
int initialTU=40, lastViewPhase=0, manualTicks=0, manualLastPhase=0;
int callbacks=0, baselineCallbacks=0, targetX=16, targetY=80, prepX=16, prepY=80;
bool needPickup=false, needPage=false, waitingWarning=false;
BattleItem *secondItem=0;
int baselineHeld=-1, prepPhase=0;
std::vector<std::pair<int,int>> refusedDomain={{-1,160},{320,160},{32,80},{127,80},{160,80},{0,62},{0,111},{0,151},{0,200},{100,100},{200,144}};
int domainIndex=0, baselineShown=0;
const std::vector<std::pair<int,int>> admittedDomain={{0,63},{31,110},{128,63},{159,110},{0,152},{319,199}};
std::set<int> commandPhases; std::vector<unsigned char> lastWarningPixels; int warningFrameCount=0;
size_t baselineSurfaces=0;
Stage stage=WARM;
Game *owner=0; BattlescapeState *state=0; InventoryState *inventory=0; InventoryState *originalInventory=0;
BattleUnit *player=0, *twin=0; BattleItem *fixtureItem=0;
Position playerPosition(20,20,0), twinPosition(30,30,0);
Clock::time_point started, worldStarted, requestStarted, modalStarted;
std::string master, profile, epoch, capability, challenge, commandId, commandPacket, firstPacket;
std::set<std::string> tokens, commandIds;
std::deque<SDL_Event> inputs;
std::vector<unsigned char> lastPixels, beforePixels;
std::map<int,std::set<std::vector<unsigned char>>> manualPhasePixels;
struct Snapshot { YAML::Node save, runtime; uint64_t rng; };
Snapshot before, opened, manualResult;
void check(bool ok,int line) { if(!ok) {
    std::cerr << "INVENTORY_CLICK_FIXTURE_DIAGNOSTIC world-" << world << " phase-" << int(stage)
        << " inits-" << inits << " ground-" << clicks << " arranged-" << arranged
        << " closes-" << closes << " destroyed-" << destroyed << " shown-" << shown
        << " dismissed-" << dismissed << " frames-" << frameSerial << " target-" << targetX << "," << targetY << " domain-index-" << domainIndex << std::endl;
    std::cerr << "INVENTORY_CLICK_CONTROL_GATE FAIL assertion-" << line << std::endl;
    std::_Exit(EXIT_FAILURE);
} }
#define require(ok) check((ok),__LINE__)
bool core() { return profile=="core"; }
bool manual() { return core() && world%2==0; }
bool terminal() { return profile=="lifetime" || profile=="reopen" || profile=="save" || profile=="parent"; }
bool inputCase() { return profile=="input-right"||profile=="input-x2"||profile=="input-wheel"||profile=="input-hotkey"||profile=="input-outside"||profile=="input-modifier"; }
bool cancelCase() { return profile=="cancel-held"||inputCase(); }
bool stale() { return profile=="stale-input"||profile=="stale-actor"||profile=="stale-reinit"||profile=="stale-held"||profile=="capability-timeout"||profile=="hidden-after-context"; }
int fixture() { if(core()) return (world/2)%FixtureCount;
    if(profile=="manual-cancel") return 10;
    if(profile=="upper-band"||profile=="overlay-child-changed"||profile=="overlay-child-moved") return 5;
    if(cancelCase()||profile=="disconnect"||profile=="timeout"||profile=="stale-held"||profile=="orphan-left-release") return 5;
    if(profile=="warning"||profile=="warning-crop") return 19; return 0; }
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
    if(inventory && owner->isState(inventory)) { n["inventoryPage"]=InventoryClickGateAccess::page(inventory); BattleItem *held=InventoryClickGateAccess::widget(inventory)->getSelectedItem(); n["held"]=held?held->getId():-1; n["stacks"]=InventoryClickGateAccess::stacks(inventory); n["text"]=InventoryClickGateAccess::text(inventory); n["pressedRight"]=InventoryClickGateAccess::pressed(inventory,SDL_BUTTON_RIGHT); n["pressedX2"]=InventoryClickGateAccess::pressed(inventory,SDL_BUTTON_X2); }
    for(BattleItem *i:*b->getItems()) { YAML::Node v; v["id"]=i->getId(); v["owner"]=i->getOwner()?i->getOwner()->getId():-1; v["ammo"]=i->getAmmoItem()?i->getAmmoItem()->getId():-1; v["quantity"]=i->getAmmoQuantity(); v["fuse"]=i->getFuseTimer(); v["slot"]=i->getSlot()?i->getSlot()->getId():""; v["x"]=i->getSlotX(); v["y"]=i->getSlotY(); if(i->getTile()) v["tile"]=i->getTile()->getPosition(); n["itemRuntime"].push_back(v); }
    require(s.rng==RNG::getSeed()); return s;
}
void compare(const Snapshot &a,const Snapshot &b) { require(a.rng==b.rng && equal(a.save,b.save) && equal(a.runtime,b.runtime)); }
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
    fields(n,{"schema","command_id","status"}); require(n["schema"].as<std::string>()=="experimental-stance-status/1"&&n["command_id"].as<std::string>()==commandId&&n["status"].as<std::string>()==value);
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
    lastPixels=display(); lastViewPhase=inventory?InventoryClickGateAccess::animation(inventory):0; std::map<unsigned int,unsigned int> first;
    for(size_t i=0;i<indices.size();++i) { unsigned int rgb=(lastPixels[i*3]<<16)|(lastPixels[i*3+1]<<8)|lastPixels[i*3+2];
        if(!first.count(rgb)) { unsigned int next=first.size(); first[rgb]=next; } require(first.size()<=256&&indices[i]==first[rgb]);
        for(int c=0;c<3;++c) require(palette[indices[i]*3+c]==lastPixels[i*3+c]); }
    for(size_t i=first.size()*3;i<palette.size();++i) require(palette[i]==0); ++oracleCount;
}
void context(const YAML::Node &n,char type)
{
    if(type=='I') { fields(n,{"schema","challenge","semantics","view"}); require(n["schema"].as<std::string>()=="experimental-inventory-view/1"); }
    else { fields(n,{"schema","challenge","control_epoch","capability","semantics",type!='O'?"view":"panel"});
        require(n["schema"].as<std::string>()==(type=='H'?"experimental-inventory-click-context/1":type=='K'?"experimental-inventory-ground-context/1":type=='J'?"experimental-inventory-close-context/1":type=='V'?"experimental-tactical-context/1":"experimental-stance-context/1"));
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
    const std::string suffix=(kind=='C'||kind=='G')?"":kind=='B'?"o0":kind=='D'?"sk":kind=='N'?"n0":kind=='E'?xy:"160080";
    return std::string(1,kind)+epoch+grant+id+suffix;
}
void command() { commandId=randomToken(); commandPacket=packet('E',commandId,capability); sendPacket(commandPacket); }
void verifyPreserved()
{
    require(inventory&&owner->isState(inventory)&&destroyed==0&&closes==0&&constructed==1);
    require(inits==baselineInits&&arranged==baselineArranged&&clicks==baselineClicks&&InventoryClickGateAccess::page(inventory)==baselinePage&&callbacks==baselineCallbacks);
    require(InventoryClickGateAccess::surfaces(inventory)==baselineSurfaces);
    compare(before,snapshot());
}
void verifyDispatched()
{
    require(inventory==originalInventory&&owner->isState(inventory)&&constructed==1&&destroyed==0&&closes==0);
    require(inits==baselineInits&&clicks==baselineClicks+1&&callbacks==baselineCallbacks+1);
    require(arranged==baselineArranged+(fixture()==23?1:0));
    require(!InventoryClickGateAccess::modal(inventory));
    require(InventoryClickGateAccess::surfaces(inventory)==baselineSurfaces);
    const int f=fixture(); BattleItem *held=InventoryClickGateAccess::widget(inventory)->getSelectedItem();
    if(f==0||f==21) require(!held&&player->getTimeUnits()==initialTU);
    if(f==1||f==2||f==3||f==4||f==12||f==14||f==19) require(held==fixtureItem&&player->getTimeUnits()==initialTU);
    if(f==5||f==6||f==13) require(!held&&fixtureItem->getOwner()==0&&fixtureItem->getTile()==player->getTile()&&fixtureItem->getSlot()->getId()=="STR_GROUND");
    if(f==7||f==8||f==9) require(!held&&fixtureItem->getOwner()==player&&fixtureItem->getSlot()->getId()==(f==8?"STR_LEFT_HAND":"STR_RIGHT_HAND"));
    if(f==10||f==11||f==16||f==17||f==18||f==20||f==22) require(held==fixtureItem&&player->getTimeUnits()==initialTU);
    if(f==15||f==23) require(!held&&secondItem->getAmmoItem()==fixtureItem&&fixtureItem->getOwner()==0&&player->getTimeUnits()==initialTU-15);
    if(f==5||f==6) require(fixtureItem->getSlotX()==5&&fixtureItem->getSlotY()==1);
    if(f==13) require(fixtureItem->getSlotX()==secondItem->getSlotX()&&fixtureItem->getSlotY()==secondItem->getSlotY());
    if(f==14) require(InventoryClickGateAccess::page(inventory)==20&&fixtureItem->getSlotX()>=20);
}
void rememberInventory()
{
    require(inventory&&owner->isState(inventory));
    before=snapshot(); beforePixels=lastPixels; baselineInits=inits; baselineArranged=arranged; baselineClicks=clicks;
    baselinePage=InventoryClickGateAccess::page(inventory); baselineSurfaces=InventoryClickGateAccess::surfaces(inventory);
    baselineAnimation=InventoryClickGateAccess::animation(inventory); initialTU=player->getTimeUnits(); originalInventory=inventory; baselineCallbacks=callbacks; baselineShown=shown; BattleItem *held=InventoryClickGateAccess::widget(inventory)->getSelectedItem(); baselineHeld=held?held->getId():-1;
}
void approved() { key(SDLK_y); stage=WAIT_DISPATCH; clickSent=true; }
void retainFrame(const std::vector<unsigned char> &pixels,int phase,int tick,const char *kind)
{
    require(pixels.size()==320*200*3&&phase>=0&&phase<=8);
    require(phase>0||std::string(kind)=="overlay"||std::string(kind).find("warning")!=std::string::npos);
    require(inventory&&owner->isState(inventory)&&InventoryClickGateAccess::initialized(owner)&&InventoryClickGateAccess::deleted(owner)==0);
    const std::string file="inventory-click-frame-"+std::to_string(world)+"-"+kind+"-"+std::to_string(tick)+"-"+std::to_string(phase)+".ppm";
    std::ofstream out(Options::getUserFolder()+file,std::ios::binary); require(out.good());
    out << "P6\n320 200\n255\n"; out.write(reinterpret_cast<const char *>(pixels.data()),pixels.size()); out.close(); require(out.good());
    std::cerr << "INVENTORY_CLICK_FRAME_EVIDENCE world-" << world << " kind-" << kind << " tick-" << tick << " inventory-phase-" << phase << " same-inventory-1 file-" << file << std::endl;
}
void comparePixels(const std::set<std::vector<unsigned char>> &expected)
{
    require(!expected.empty());
    if(!expected.count(lastPixels)) {
        size_t best=320*200+1; int left=320,right=-1,top=200,bottom=-1;
        for(const auto &frame:expected) {
            size_t count=0; int l=320,r=-1,t=200,b=-1;
            for(int y=0;y<200;++y) for(int x=0;x<320;++x) { const int p=(y*320+x)*3;
                if(!std::equal(lastPixels.begin()+p,lastPixels.begin()+p+3,frame.begin()+p)) { ++count; l=std::min(l,x); r=std::max(r,x); t=std::min(t,y); b=std::max(b,y); } }
            if(count<best) { best=count; left=l; right=r; top=t; bottom=b; }
        }
        int mx,my; SDL_GetMouseState(&mx,&my);
        std::cerr << "INVENTORY_CLICK_PIXEL_DIAGNOSTIC phase-" << lastViewPhase << " closest-count-" << best << " bbox-" << left << "," << top << "," << right << "," << bottom << " physical-mouse-" << mx << "," << my << std::endl;
    }
    require(expected.count(lastPixels)==1);
}
// Targeted presentation oracle, separate from unmasked full-frame pair parity.
// Read already-rendered component surfaces only; never draw/blit/reset timers.
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
    const auto physical=display(); Surface *frame=InventoryClickGateAccess::banner(inventory), *text=InventoryClickGateAccess::overlayText(inventory), *yes=InventoryClickGateAccess::yes(inventory), *no=InventoryClickGateAccess::no(inventory), *pointer=InventoryClickGateAccess::pointer(inventory), *cursor=owner->getCursor();
    Text *label=InventoryClickGateAccess::overlayText(inventory);
    const std::string expected="LEFT pointer ("+std::to_string(targetX)+","+std::to_string(targetY)+"). May pick/place,\nreload or stack; normal TU.\nNo promised transfer. Confirm?";
    require(label->getText()==expected&&label->getTextHeight()>0&&label->getTextHeight()<=label->getHeight());
    int opaque=0;
    for(int y=frame->getY();y<frame->getY()+32;++y) for(int x=0;x<320;++x) {
        SDL_Color color={}; bool drawn=false; for(Surface *component:{frame,text,yes,no,cursor}) if(renderedColor(component,x,y,color)) drawn=true;
        if(drawn) { ++opaque; const int offset=(y*320+x)*3; require(physical[offset]==color.r&&physical[offset+1]==color.g&&physical[offset+2]==color.b); }
    }
    require(opaque>9000);
    int ring=0, exposed=0;
    for(int y=std::max(0,pointer->getY());y<std::min(200,pointer->getY()+17);++y) for(int x=std::max(0,pointer->getX());x<std::min(320,pointer->getX()+17);++x) {
        SDL_Color color={}; if(renderedColor(pointer,x,y,color)) { ++ring; bool covered=false; for(Surface *component:{frame,text,yes,no,cursor}) if(renderedColor(component,x,y,color)) covered=true; if(!covered) ++exposed; const int offset=(y*320+x)*3; require(physical[offset]==color.r&&physical[offset+1]==color.g&&physical[offset+2]==color.b); }
    }
    require(ring>0&&exposed>0);
}
void afterView()
{
    verifyDispatched();
    if(core()) {
        if(manual()) { manualResult=snapshot(); manualPhasePixels.clear(); }
        else { compare(manualResult,snapshot()); require(manualPhasePixels.size()==8); commandPhases.clear(); }
        manualTicks=0; manualLastPhase=0; lastWarningPixels.clear(); stage=COLLECT_MANUAL;
    } else if(profile=="ledger") { opened=snapshot(); replay=0; stage=REPLAY_SEND; }
    else if(profile=="warning"||profile=="warning-crop"||profile=="manual-cancel") { require(InventoryClickGateAccess::warning(inventory)->getVisible()); opened=snapshot(); if(profile=="warning-crop") { SDL_Rect *crop=InventoryClickGateAccess::warning(inventory)->getCrop(); crop->x=0; crop->y=0; crop->w=100; crop->h=12; } request('H'); stage=WAIT_WARNING_REFUSAL; }
    else if(profile=="domain") { ++domainIndex; rememberInventory(); request('H'); stage=WAIT_CONTEXT; }
    else finish();
}
void beginTerminal()
{
    lostOriginal=true;
    if(profile=="save") { terminalReplaced=true; replacement(); }
    else if(profile=="parent") { terminalReplaced=true; pendingSetup=true; owner->setState(new State); }
    else { key(Options::keyBattleInventory); stage=WAIT_TERMINAL_CLOSE; }
}
} // namespace
bool active() { const char *s=std::getenv("OPENXCOM_INVENTORY_CLICK_CONTROL_GATE"); return s&&std::string(s)=="1"; }
void prepare()
{
    enabled=active(); if(!enabled) return; const char *p=std::getenv("OPENXCOM_INVENTORY_CLICK_PROFILE"); profile=p?p:"";
    const std::set<std::string> profiles={"core","cancel-held","input-right","input-x2","input-wheel","input-hotkey","input-outside","input-modifier","stale-input","stale-actor","stale-reinit","stale-held","focus","display","overlay","lifetime","reopen","save","parent","unseen","held-y","held-mouse","timeout","capability-timeout","disconnect","ledger","ledger-limit","disabled","close-disabled","nontu","debug","palette","capless-I","cross-kind","hidden-widget","moved-widget","hidden-after-context","duplicate-pending","failed-I","warning","domain","actual-send-failure","manual-cancel","moved-name","extra-slot","hidden-selection","warning-crop","upper-band","ground-disabled","stale-widget-actor","stale-widget-mode","overlapping-widget","overlay-child-changed","overlay-child-moved","orphan-left-release"}; require(profiles.count(profile)==1);
    require(!std::getenv("OPENXCOM_STANCE_CONTROL_GATE")&&!std::getenv("OPENXCOM_INVENTORY_OPEN_CONTROL_GATE")&&!std::getenv("OPENXCOM_INVENTORY_CLOSE_CONTROL_GATE"));
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
    if(terminalReplaced&&profile=="parent") { state=new BattlescapeState; game->getSavedGame()->getSavedBattle()->setBattleState(state); game->setState(state); inventory=0; moved=false; frames=0; stage=WAIT_TERMINAL_OPEN; return; }
    if(!terminalReplaced) { inits=clicks=callbacks=closes=destroyed=constructed=displayed=shown=dismissed=arranged=frames=inventoryFrames=modalFrames=0;
        moved=prepared=retry=clickSent=lostOriginal=false; inventory=originalInventory=0; fixtureItem=0; inputs.clear(); stage=WARM; worldStarted=Clock::now(); }
    Options::FPS=60; Options::maximizeInfoScreens=core()&&world/2>=FixtureCount;
    Options::battleXcomSpeed=30; Options::battleNewPreviewPath=PATH_NONE;
    RNG::setSeed(123456789); std::srand(12345); // Only independent world construction.
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
    initialTU=(fixture()==10||fixture()==18)?0:40; player->setTimeUnits(initialTU);
    RuleInventory *ground=mod->getInventory("STR_GROUND",true); require(ground->getX()==0&&(320-ground->getX())/16==20&&(200-ground->getY())/16==3);
    auto addGround=[&](BattleItem *i) { player->getTile()->addItem(i,ground); i->setSlotX(200+int(player->getTile()->getInventory()->size())); i->setSlotY(1); return i; };
    const int f=fixture(); targetX=16; targetY=80; prepX=16; prepY=80; needPickup=needPage=false; secondItem=0;
    const std::string ammo=master=="xcom1"?"STR_RIFLE_CLIP":"STR_HARPOON_POD";
    const std::string gun=master=="xcom1"?"STR_RIFLE":"STR_JET_HARPOON";
    const std::string grenade=master=="xcom1"?"STR_GRENADE":"STR_MAGNA_BLAST_GRENADE";
    if(f==1||f==5||f==8||f==9||f==10||f==13) fixtureItem=item(ammo,player,"STR_RIGHT_HAND");
    if(f==2||f==6||f==11||f==20||f==22) fixtureItem=item("STR_MEDI_KIT",player,"STR_RIGHT_HAND");
    if(f==3||f==7||f==12) fixtureItem=addGround(item(ammo,0,"STR_GROUND"));
    if(f==4) fixtureItem=addGround(item("STR_MEDI_KIT",0,"STR_GROUND"));
    if(f==19) { fixtureItem=addGround(item(grenade,0,"STR_GROUND")); fixtureItem->setFuseTimer(0); }
    if(f==12) for(int i=0;i<3;++i) addGround(item(ammo,0,"STR_GROUND"));
    if(f==13) secondItem=addGround(item(ammo,0,"STR_GROUND"));
    if(f==11) secondItem=addGround(item(grenade,0,"STR_GROUND"));
    if(f==14) { for(int i=0;i<25;++i) { BattleItem *med=addGround(item("STR_MEDI_KIT",0,"STR_GROUND")); med->setPainKillerQuantity(i%11); med->setHealQuantity(i/11); } needPage=true; }
    if(f>=15&&f<=18||f==23) { fixtureItem=item(f==16?grenade:ammo,player,"STR_LEFT_HAND"); secondItem=item(gun,f==23?0:player,f==23?"STR_GROUND":"STR_RIGHT_HAND"); if(f==23) addGround(secondItem); if(f==17) { BattleItem *loadedAmmo=item(ammo,0,"STR_RIGHT_HAND"); require(secondItem->setAmmoItem(loadedAmmo)==0); } prepX=144; prepY=80; needPickup=true; }
    if(f==21) { for(const std::string &id:mod->getItemsList()) { RuleItem *rule=mod->getItem(id,true); if(rule->isFixed()&&rule->getInventoryWidth()>0&&rule->getInventoryWidth()<=2&&rule->getInventoryHeight()>0&&rule->getInventoryHeight()<=3&&rule->getBigSprite()>=0) { fixtureItem=item(id,player,"STR_RIGHT_HAND"); break; } } require(fixtureItem); }
    if(f==3||f==4||f==12||f==14||f==19) { targetX=8; targetY=160; }
    if(f>=5&&f<=11||f==13||f==20||f==22) needPickup=true;
    if(f==7) { prepX=8; prepY=160; }
    if(f==5||f==10) { targetX=88; targetY=176; }
    if(f==6) { targetX=88; targetY=184; }
    if(f==8) { targetX=144; targetY=80; }
    if(f==11) { targetX=8; targetY=168; }
    if(f==13||f==23) { targetX=8; targetY=160; }
    if(f==20) { targetX=88; targetY=199; }
    if(f==22) { targetX=0; targetY=152; }
    battle->getTileEngine()->calculateSunShading(); battle->getTileEngine()->calculateTerrainLighting(); battle->getTileEngine()->calculateUnitLighting(); battle->getTileEngine()->recalculateFOV();
    if(profile=="debug") battle->setDebugMode();
    state=new BattlescapeState; battle->setBattleState(state); game->setState(state);
    if(terminalReplaced) { inventory=0; moved=false; frames=0; stage=WAIT_TERMINAL_OPEN; }
}
void beforeEvents(Game *game)
{
    if(!running||pendingSetup) return; if(profile=="orphan-left-release"&&orphanAdvance(game)) return;
    require(Clock::now()-started<std::chrono::seconds(core()?400:(profile=="timeout"||profile=="capability-timeout")?155:45));
    require(Clock::now()-worldStarted<std::chrono::seconds((profile=="timeout"||profile=="capability-timeout")?145:35)); quiet();
    if(!moved) { motion(); moved=true; return; }
    if(stage==PREPARE_INVENTORY) { prepPhase=0; stage=WAIT_PREPARED; }
    else if(stage==WAIT_PREPARED&&inputs.empty()) {
        if(prepPhase==0) { ++prepPhase; if(needPage) { click(304,144); return; } }
        if(prepPhase==1) { ++prepPhase; if(needPage) { require(InventoryClickGateAccess::page(inventory)==20); fixtureItem=player->getItem(game->getMod()->getInventory("STR_GROUND",true),20,0); require(fixtureItem); } if(needPickup) { click(prepX,prepY); return; } }
        if(prepPhase==2) { ++prepPhase; if(needPickup) require(InventoryClickGateAccess::widget(inventory)->getSelectedItem()==fixtureItem); motion(160,profile=="upper-band"?136:80); if(profile=="overlay-child-changed"||profile=="overlay-child-moved") mouseEdge(160,80,true,profile=="overlay-child-changed"?SDL_BUTTON_RIGHT:SDL_BUTTON_X2); return; }
        rememberInventory(); if(profile=="overlay-child-changed"||profile=="overlay-child-moved") require(InventoryClickGateAccess::pressed(inventory,profile=="overlay-child-changed"?SDL_BUTTON_RIGHT:SDL_BUTTON_X2)); request('I'); stage=WAIT_I;
    } else if(stage==SEND_COMMAND) {
        if(profile=="domain") { const auto &point=domainIndex<int(admittedDomain.size())?admittedDomain[domainIndex]:refusedDomain[2]; targetX=point.first; targetY=point.second; }
        if(profile=="stale-input") motion(151,81);
        if(profile=="stale-actor") { game->getSavedGame()->getSavedBattle()->setSelectedUnit(twin); game->getSavedGame()->getSavedBattle()->setSelectedUnit(player); }
        if(profile=="stale-reinit") inventory->init();
        if(profile=="capability-timeout") SDL_Delay(120100);
        if(profile=="hidden-after-context") InventoryClickGateAccess::widget(inventory)->setVisible(false);
        if(profile=="stale-held") { InventoryClickGateAccess::widget(inventory)->setSelectedItem(0); InventoryClickGateAccess::widget(inventory)->setSelectedItem(fixtureItem); }
        command(); stage=WAIT_PENDING;
    } else if(stage==WAIT_PENDING&&available()) {
        if(profile=="domain"&&domainIndex==int(admittedDomain.size())) { unavailable(receive()); char byte; require(recv(peer,&byte,1,MSG_DONTWAIT)==0); verifyPreserved(); finish(); return; }
        if(stale()||profile=="capless-I") { status(receive(),"unavailable"); require(clicks==baselineClicks&&closes==0&&destroyed==0); finish(); return; }
        status(receive(),"pending_confirmation"); require(game->isState(inventory)&&shown==baselineShown+1&&clicks==baselineClicks&&arranged==baselineArranged&&closes==0&&destroyed==0&&inits==baselineInits);
        if(profile=="unseen") { require(shown==1); stage=WAIT_INPUT; }
        else if(profile=="duplicate-pending") { sendPacket(commandPacket); stage=WAIT_DUPLICATE_PENDING; }
        else stage=CONFIRM;
    } else if(stage==WAIT_DUPLICATE_PENDING&&available()) { status(receive(),"pending_confirmation"); require(shown==1&&clicks==baselineClicks&&arranged==baselineArranged); stage=CONFIRM; }
    else if(stage==CONFIRM&&modalFrames>0) {
        require(game->isState(inventory)&&InventoryClickGateAccess::modal(inventory));
        if(inputCase()&&!retry) {
            if(profile=="input-right") { if(Clock::now()-modalStarted<std::chrono::milliseconds(400)) return; require(InventoryClickGateAccess::animation(inventory)!=baselineAnimation); click(152,80,SDL_BUTTON_RIGHT); }
            else if(profile=="input-x2") click(152,80,SDL_BUTTON_X2);
            else if(profile=="input-wheel") click(152,80,SDL_BUTTON_WHEELUP);
            else if(profile=="input-hotkey") key(Options::keyBattleInventory);
            else if(profile=="input-outside") click(8,8);
            else { SDL_Event e={}; e.type=SDL_KEYDOWN; e.key.which=255; e.key.state=SDL_PRESSED; e.key.keysym.sym=SDLK_y; e.key.keysym.mod=SDLMod(KMOD_SHIFT); queue(e); keyEdge(SDLK_y,false); }
            stage=WAIT_INPUT;
        } else if(cancelCase()&&!retry) { key(SDLK_ESCAPE); stage=WAIT_INPUT; }
        else if(profile=="held-y") { keyEdge(SDLK_y,true); stage=WAIT_INPUT; }
        else if(profile=="held-mouse") { mouseEdge(280,113,false); stage=WAIT_INPUT; }
        else if(profile=="overlay-child-changed") { InventoryClickGateAccess::yes(inventory)->setVisible(false); mouseEdge(160,80,true,SDL_BUTTON_RIGHT); mouseEdge(160,80,false,SDL_BUTTON_RIGHT); stage=WAIT_INPUT; }
        else if(profile=="overlay-child-moved") { InventoryClickGateAccess::yes(inventory)->setX(241); mouseEdge(160,80,true,SDL_BUTTON_X2); mouseEdge(160,80,false,SDL_BUTTON_X2); stage=WAIT_INPUT; }
        else if(profile=="focus") { focus(false); stage=WAIT_INPUT; }
        else if(profile=="display") { game->getScreen()->resetDisplay(false); stage=WAIT_INPUT; }
        else if(profile=="overlay") { game->pushState(new State); stage=WAIT_INPUT; }
        else if(profile=="timeout") { SDL_Delay(120100); key(SDLK_y); stage=WAIT_INPUT; }
        else if(profile=="disconnect") { close(peer); peer=-1; stage=WAIT_RETIRE; }
        else if(profile=="palette") { SDL_Color palette[256]; std::copy(game->getScreen()->getPalette(),game->getScreen()->getPalette()+256,palette); game->getScreen()->resetDisplay(false); game->getScreen()->setPalette(palette,0,1,false); stage=WAIT_INPUT; }
        else if(profile=="upper-band"||(core()&&world%4==3)) { Surface *yes=InventoryClickGateAccess::yes(inventory); click(yes->getX()+yes->getWidth()/2,yes->getY()+yes->getHeight()/2); stage=WAIT_DISPATCH; clickSent=true; }
        else approved();
    } else if(stage==WAIT_INPUT&&inputs.empty()) {
        require(clicks==baselineClicks&&arranged==baselineArranged&&closes==0); sendPacket("S"+commandId); stage=WAIT_CANCEL_STATUS;
    } else if(stage==WAIT_CANCEL_STATUS&&available()) {
        status(receive(),"unavailable"); require(clicks==baselineClicks&&arranged==baselineArranged&&closes==0);
        if(cancelCase()) { verifyPreserved(); require(!InventoryClickGateAccess::modal(inventory)&&dismissed==1); retry=true; baselineShown=shown; motion(); request('H'); stage=WAIT_CONTEXT; }
        else {
            if(profile=="focus") focus(true);
            if(profile=="overlay") {
                // Controller cancellation already happened under the external
                // State. Check it before the fixture's own ordinary pop.
                require(inventory==originalInventory&&constructed==1&&destroyed==0&&dismissed==1);
                require(inits==baselineInits&&arranged==baselineArranged&&clicks==baselineClicks&&closes==0);
                require(!InventoryClickGateAccess::modal(inventory)&&InventoryClickGateAccess::surfaces(inventory)==baselineSurfaces);
                Snapshot canceled=snapshot();
                canceled.runtime["inventoryPage"]=InventoryClickGateAccess::page(inventory);
                BattleItem *held=InventoryClickGateAccess::widget(inventory)->getSelectedItem();
                canceled.runtime["held"]=held?held->getId():-1;
                canceled.runtime["stacks"]=InventoryClickGateAccess::stacks(inventory); canceled.runtime["text"]=InventoryClickGateAccess::text(inventory); canceled.runtime["pressedRight"]=InventoryClickGateAccess::pressed(inventory,SDL_BUTTON_RIGHT); canceled.runtime["pressedX2"]=InventoryClickGateAccess::pressed(inventory,SDL_BUTTON_X2);
                compare(before,canceled);
                game->popState(); // Normal restoration must re-init next loop.
            }
            if(profile=="held-y") keyEdge(SDLK_y,false); stage=WAIT_RETIRE;
        }
    } else if(stage==WAIT_RETIRE&&inputs.empty()) {
        if(inventory&&game->isState(inventory)&&!InventoryClickGateAccess::modal(inventory)) {
            require(clicks==baselineClicks&&closes==0&&destroyed==0&&inventory==originalInventory&&constructed==1);
            if(profile=="overlay") {
                // This one init/arrange belongs to Game::popState restoration,
                // not the controller. The canonical one-item world is unchanged.
                require(inits==baselineInits+1&&arranged==baselineArranged+1);
                compare(before,snapshot());
            } else {
                require(arranged==baselineArranged&&inits==baselineInits);
                if(profile!="palette") compare(before,snapshot());
                if(profile=="overlay-child-changed"||profile=="overlay-child-moved") require(InventoryClickGateAccess::consumedButtons(inventory)==0);
            }
            finish();
        }
    } else if(stage==BARRIER_REPLY&&available()) {
        unavailable(receive()); verifyDispatched();
        if(profile=="ground-disabled"&&barrier==2) ++barrier;
        if(++barrier<5) { const char kinds[]={'O','V','J','K','H'}; request(kinds[barrier]); }
        else if(terminal()) beginTerminal();
        else if(profile=="actual-send-failure") { require(InventoryClickGateAccess::awaiting(game)&&InventoryClickGateAccess::destination(game)==inventory); require(shutdown(peer,SHUT_RD)==0); request('I'); stage=WAIT_SEND_FAILURE; }
        else if(profile=="failed-I") { InventoryClickGateAccess::widget(inventory)->setVisible(false); request('I'); stage=WAIT_FAILED_I; }
        else { request('I'); stage=WAIT_POST_I; }
    } else if(stage==WAIT_SEND_FAILURE&&!game->getStanceController()->enabled()) { require(InventoryClickGateAccess::awaiting(game)); verifyDispatched(); finish(); }
    else if(stage==WAIT_FAILED_I&&available()) { unavailable(receive()); InventoryClickGateAccess::widget(inventory)->setVisible(true); request('H'); stage=WAIT_FAILED_K; }
    else if(stage==WAIT_FAILED_K&&available()) { unavailable(receive()); verifyDispatched(); request('I'); stage=WAIT_POST_I; }
    else if(stage==WAIT_WARNING_REFUSAL&&available()) { unavailable(receive()); compare(opened,snapshot()); require(!InventoryClickGateAccess::awaiting(game)); stage=WAIT_WARNING_FADE; }
    else if(stage==WAIT_WARNING_FADE&&!InventoryClickGateAccess::warning(inventory)->getVisible()) {
        compare(opened,snapshot());
        if(profile=="manual-cancel") { require(InventoryClickGateAccess::widget(inventory)->getSelectedItem()==fixtureItem); click(160,80,SDL_BUTTON_RIGHT); stage=WAIT_MANUAL_CANCEL; }
        else { request('H'); stage=WAIT_REARM; }
    } else if(stage==WAIT_MANUAL_CANCEL&&inputs.empty()) {
        require(inventory==originalInventory&&owner->isState(inventory)&&closes==0&&destroyed==0&&inits==baselineInits&&arranged==baselineArranged&&clicks==baselineClicks+2&&callbacks==baselineCallbacks+2);
        require(!InventoryClickGateAccess::widget(inventory)->getSelectedItem()&&fixtureItem->getOwner()==player&&fixtureItem->getSlot()->getId()=="STR_RIGHT_HAND"&&player->getTimeUnits()==initialTU); finish();
    }
    else if(stage==CROSS_REPLY&&available()) {
        status(receive(),"unavailable"); verifyPreserved();
        if(++replay==7) { request('J'); stage=WAIT_REARM; }
        else { const char kinds[]={'B','D','N','R','L','C','G'}; commandId=randomToken(); sendPacket(packet(kinds[replay],commandId,capability)); }
    } else if(stage==WAIT_STATUS&&available()) { status(receive(),"dispatched"); afterView(); }
    else if(stage==REPLAY_SEND) {
        commandId=commandPacket.substr(65,32);
        if(replay==0) sendPacket(commandPacket);
        else if(replay==1) sendPacket("S"+commandId);
        else if(replay==2) { std::string conflict=commandPacket; conflict.replace(33,32,randomToken()); sendPacket(conflict); }
        else if(replay<10) { const char kinds[]={'B','D','N','R','L','C','G'}; sendPacket(packet(kinds[replay-3],commandId,capability)); }
        else if(replay==10) { commandId=randomToken(); std::string consumed=commandPacket; consumed.replace(65,32,commandId); sendPacket(consumed); }
        else { request('H'); stage=WAIT_REARM; return; }
        stage=REPLAY_REPLY;
    } else if(stage==REPLAY_REPLY&&available()) {
        if(replay==100) { status(receive(),"unavailable"); compare(before,snapshot()); stage=LIMIT_SEND; return; }
        status(receive(),replay<2?"dispatched":"unavailable"); compare(opened,snapshot()); verifyDispatched(); require(shown==1); ++replay; stage=REPLAY_SEND;
    } else if(stage==LIMIT_SEND) {
        const char kinds[]={'E','G','C','B','D','N','R','L'}; commandId=randomToken(); std::string p=packet(kinds[limits%8],commandId,randomToken()); if(limits==0) firstPacket=p; sendPacket(p); stage=limits==64?LIMIT_FINAL:LIMIT_REPLY;
    } else if(stage==LIMIT_REPLY&&available()) {
        status(receive(),"unavailable"); compare(before,snapshot()); require(clicks==0&&arranged==0&&closes==0); ++limits;
        if(limits==64) { commandId=firstPacket.substr(65,32); sendPacket(firstPacket); replay=100; stage=REPLAY_REPLY; } else stage=LIMIT_SEND;
    } else if(stage==LIMIT_FINAL&&available()) { unavailable(receive()); char b; require(recv(peer,&b,1,MSG_DONTWAIT)==0&&commandIds.size()==65&&clicks==0); finish(); }
    else if(stage==WAIT_TERMINAL_CLOSE&&destroyed==1&&inputs.empty()&&idle()) {
        require(closes==1&&clicks==baselineClicks+1); if(profile=="reopen") { key(Options::keyBattleInventory); stage=WAIT_TERMINAL_OPEN; } else { barrier=0; stage=TERMINAL_REQUEST; }
    } else if(stage==WAIT_TERMINAL_OPEN&&inputs.empty()) {
        if(inventory&&game->isState(inventory)&&InventoryClickGateAccess::initialized(game)) { require(constructed==2&&destroyed==1); barrier=0; stage=TERMINAL_REQUEST; }
        else if(idle()&&++frames>=2) key(Options::keyBattleInventory);
    } else if(stage==TERMINAL_REQUEST) { const char kinds[]={'O','V','I','J','K','H'}; request(kinds[barrier]); stage=TERMINAL_REPLY; }
    else if(stage==TERMINAL_REPLY&&available()) { unavailable(receive()); if(++barrier==6) { require(clicks==baselineClicks+1&&destroyed==1); finish(); } else stage=TERMINAL_REQUEST; }
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
void afterEvents(Game *game)
{
    if(!running||pendingSetup) return;
    if(stage==WAIT_DISPATCH&&inputs.empty()) {
        verifyDispatched(); dispatchedFrame=frameSerial; baselineAnimation=InventoryClickGateAccess::animation(inventory); motion(); stage=WAIT_SETTLE;
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
    if(inventory&&game->isState(inventory)&&InventoryClickGateAccess::modal(inventory)) { ++modalFrames; if(modalFrames==1) retainFrame(display(),InventoryClickGateAccess::animation(inventory),shown,"overlay"); verifyOverlayPixels(); }
    if(stage==WAIT_INVENTORY&&inventory&&game->isState(inventory)&&inputs.empty()&&++inventoryFrames>=2) {
        require(inits==1&&constructed==1&&clicks==0&&closes==0&&destroyed==0&&displayed>0); Surface *button=InventoryClickGateAccess::groundButton(inventory);
        require(button->getVisible()&&button->getX()==289&&button->getY()==137&&button->getWidth()==32&&button->getHeight()==15); stage=PREPARE_INVENTORY;
    }
    if(stage==COLLECT_MANUAL&&inventory&&game->isState(inventory)) {
        const int phase=InventoryClickGateAccess::animation(inventory); require(phase>=0&&phase<=8);
        const auto pixels=display();
        if(InventoryClickGateAccess::warning(inventory)->getVisible()) {
            // Retain transient full frames at their natural joint primer/fade phases.
            // They are evidence only: warning timers are not aligned across worlds.
            if(pixels!=lastWarningPixels) { lastWarningPixels=pixels; retainFrame(pixels,phase,frameSerial,manual()?"manual-warning":"command-warning"); ++warningFrameCount; }
        } else if(phase>0&&phase!=manualLastPhase) {
            manualLastPhase=phase; ++manualTicks; lastPixels=pixels; lastViewPhase=phase;
            retainFrame(pixels,phase,manualTicks,manual()?"manual":"command");
            if(manual()) manualPhasePixels[phase].insert(pixels);
            else { require(manualPhasePixels.count(phase)); comparePixels(manualPhasePixels.at(phase)); commandPhases.insert(phase); }
        }
        // Eight whole physical frames after natural warning retirement, one per
        // naturally rendered primer phase. No timer reset, mask or forced redraw.
        if((manual()?manualPhasePixels.size():commandPhases.size())==8&&manualTicks>=8) {
            compare(manualResult,snapshot()); if(!manual()) ++parityCount; finishWorld(); return;
        }
    }
    if(stage==WAIT_SETTLE&&frameSerial>dispatchedFrame&&inputs.empty()) {
        verifyDispatched();
        if(manual()) { request('I'); stage=WAIT_POST_I; }
        else { barrier=0; request('O'); stage=BARRIER_REPLY; }
    }
    if(stage==WAIT_I&&available()) {
        if(profile=="nontu"||profile=="debug") { unavailable(receive()); verifyPreserved(); request('H'); stage=WAIT_CONTEXT; return; }
        context(receive(),'I'); verifyPreserved(); beforePixels=lastPixels; baselineAnimation=InventoryClickGateAccess::animation(inventory);
        if(manual()) { click(targetX,targetY); stage=WAIT_DISPATCH; }
        else {
            if(profile=="stale-widget-actor") InventoryClickGateAccess::wrongWidgetActor(inventory,twin);
            if(profile=="stale-widget-mode") InventoryClickGateAccess::wrongWidgetMode(inventory);
            if(profile=="overlapping-widget") { InventoryClickGateAccess::otherButton(inventory)->setX(0); InventoryClickGateAccess::otherButton(inventory)->setY(63); }
            if(profile=="moved-name") InventoryClickGateAccess::name(inventory)->setY(24);
            if(profile=="hidden-selection") InventoryClickGateAccess::selection(inventory)->setVisible(false);
            if(profile=="extra-slot") { RuleInventory *extra=new RuleInventory("AAA_GATE_INTERCEPT"); YAML::Node n; n["x"]=0; n["y"]=63; n["type"]=int(INV_HAND); extra->load(n,0); game->getMod()->getInventories()->insert({"AAA_GATE_INTERCEPT",extra}); }
            if(profile=="hidden-widget") InventoryClickGateAccess::widget(inventory)->setVisible(false);
            if(profile=="moved-widget") InventoryClickGateAccess::widget(inventory)->setX(1);
            if(profile=="held-y") keyEdge(SDLK_y,true);
            if(profile=="held-mouse") { motion(280,113); mouseEdge(280,113,true); }
            if(baselineHeld>=0) { request('J'); stage=WAIT_HELD_READ; } else { request('H'); stage=WAIT_CONTEXT; }
        }
    } else if(stage==WAIT_HELD_READ&&available()) { unavailable(receive()); verifyPreserved(); request('K'); stage=WAIT_HELD_REFUSAL; }
    else if(stage==WAIT_HELD_REFUSAL&&available()) { unavailable(receive()); verifyPreserved(); request('H'); stage=WAIT_CONTEXT; }
    else if(stage==WAIT_CONTEXT&&available()) {
        if(profile=="disabled"||profile=="close-disabled"||profile=="nontu"||profile=="debug"||profile=="hidden-widget"||profile=="moved-widget"||profile=="moved-name"||profile=="hidden-selection"||profile=="extra-slot"||profile=="stale-widget-actor"||profile=="stale-widget-mode"||profile=="overlapping-widget") { unavailable(receive()); require(clicks==baselineClicks&&closes==0); finish(); return; }
        if(profile=="held-mouse") { YAML::Node n=receive(); if(n["error"]) { unavailable(n); verifyPreserved(); mouseEdge(280,113,false); stage=WAIT_RETIRE; return; } context(n,'H'); stage=SEND_COMMAND; return; }
        context(receive(),'H'); verifyPreserved(); require(!available()&&owner->isState(inventory));
        if(profile=="capless-I") { request('I'); stage=WAIT_CAPLESS; }
        else if(profile=="cross-kind"&&replay<7) { replay=0; commandId=randomToken(); sendPacket(packet('B',commandId,capability)); stage=CROSS_REPLY; }
        else stage=SEND_COMMAND;
    } else if(stage==WAIT_CAPLESS&&available()) { context(receive(),'I'); verifyPreserved(); stage=SEND_COMMAND; }
    else if(stage==WAIT_REARM&&available()) {
        if(profile=="warning"||profile=="warning-crop") { context(receive(),'H'); compare(opened,snapshot()); verifyDispatched(); finish(); }
        else if(profile=="ledger") { context(receive(),'H'); compare(opened,snapshot()); verifyDispatched(); require(shown==1); finish(); }
        else if(replay==7) { context(receive(),'J'); commandId=randomToken(); sendPacket(packet('E',commandId,capability)); replay=8; stage=WAIT_REARM; }
        else { status(receive(),"unavailable"); verifyPreserved(); request('H'); stage=WAIT_CONTEXT; replay=9; }
    } else if(stage==WAIT_POST_I&&available()) {
        context(receive(),'I'); require(frameSerial>dispatchedFrame); verifyDispatched();
        if(clickSent) { sendPacket("S"+commandId); stage=WAIT_STATUS; } else afterView();
    }
}
void inventoryConstructed(InventoryState *who) { if(running&&!pendingSetup) { require(!inventory); inventory=who; ++constructed; } }
void inventoryInitialized(InventoryState *who) { if(!running||pendingSetup) return; require(!inventory||inventory==who); inventory=who; ++inits; }
void inventoryDisplayed(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++displayed; }
void inventoryMouseOverEntered(Inventory *,Action *,State *) {}
void inventoryClickEntered(Inventory *who,Action *action,State *parent)
{
    if(!running||pendingSetup||!inventory||who!=InventoryClickGateAccess::widget(inventory)) return;
    ++clicks;
    if(stage==WAIT_DISPATCH) {
        if(!manual()) require(InventoryClickGateAccess::awaiting(owner)&&InventoryClickGateAccess::destination(owner)==inventory);
        require(parent==inventory&&action&&action->getSender()==who&&action->getDetails()->button.button==SDL_BUTTON_LEFT);
        require(action->getAbsoluteXMouse()==targetX&&action->getAbsoluteYMouse()==targetY);
        Surface *selection=InventoryClickGateAccess::selection(inventory);
        require(selection->getX()==targetX-selection->getWidth()/2-who->getX()&&selection->getY()==targetY-selection->getHeight()/2-who->getY());
    }
}
void inventoryInvClickEntered(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++callbacks; }
void inventoryCloseEntered(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++closes; }
void inventoryDestroyed(InventoryState *who) { if(running&&(!pendingSetup||terminalReplaced)&&who==inventory) { ++destroyed; inventory=0; require(lostOriginal); } }
void clickOverlayShown(InventoryState *who)
{
    if(running&&!pendingSetup&&who==inventory) {
        ++shown; modalStarted=Clock::now(); modalFrames=0;
        Surface *band=InventoryClickGateAccess::banner(who), *yes=InventoryClickGateAccess::yes(who), *no=InventoryClickGateAccess::no(who), *sprite=InventoryClickGateAccess::selection(who), *mark=InventoryClickGateAccess::pointer(who), *name=InventoryClickGateAccess::name(who);
        require(band->getX()==0&&band->getWidth()==320&&band->getHeight()==32&&(band->getY()==24||band->getY()==112));
        require(sprite->getY()+sprite->getHeight()<=band->getY()||sprite->getY()>=band->getY()+32);
        require(name->getY()+name->getHeight()<=band->getY());
        require(targetY<band->getY()||targetY>=band->getY()+32);
        require(yes->getX()==242&&yes->getY()==band->getY()+1&&yes->getWidth()==76&&yes->getHeight()==14);
        require(no->getX()==242&&no->getY()==band->getY()+17&&no->getWidth()==76&&no->getHeight()==14);
        for(int y=0;y<200;++y) for(int x=0;x<320;++x) {
            const int expected=x>=242&&x<318?(y>=band->getY()+1&&y<band->getY()+15?1:y>=band->getY()+17&&y<band->getY()+31?2:0):0;
            require(InventoryClickGateAccess::choice(who,x,y)==expected);
        }
        require(mark->getX()==targetX-8&&mark->getY()==targetY-8&&mark->getWidth()==17&&mark->getHeight()==17);
        for(int y=0;y<17;++y) for(int x=0;x<17;++x) { const int d=(x-8)*(x-8)+(y-8)*(y-8); require((mark->getPixel(x,y)!=0)==(d>=25&&d<=36)); }
        if(profile=="upper-band") require(band->getY()==24);
        if(profile=="unseen") { require(shown==1&&modalFrames==0&&clicks==baselineClicks); key(SDLK_y); }
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
        std::cout << "INVENTORY_CLICK_CONTROL_GATE PASS {\"schema\":\"inventory-click-control-gate/1\",\"master\":\"" << master << "\",\"profile\":\"" << profile << "\",\"passed\":true}" << std::endl;
    }
    if(peer>=0) { close(peer); peer=-1; } return enabled&&!completed?EXIT_FAILURE:EXIT_SUCCESS;
}
} }
#undef require
#undef fields

// Isolated regression added after the frozen gate: existing numbered endpoints
// and 54 profiles above retain their original physical source lines.
namespace OpenXcom { namespace InventoryClickControlGate { namespace {
bool orphanAdvance(Game *game)
{
    if(stage==CONFIRM&&modalFrames>0&&orphanPhase==0) {
        Surface *yes=InventoryClickGateAccess::yes(inventory);
        motion(yes->getX()+yes->getWidth()/2,yes->getY()+yes->getHeight()/2);
        mouseEdge(yes->getX()+yes->getWidth()/2,yes->getY()+yes->getHeight()/2,true);
        key(SDLK_ESCAPE); // The owned mouse UP is deliberately missing.
        orphanPhase=1; stage=WAIT_RETIRE; return true;
    }
    if(orphanPhase==1&&inputs.empty()) {
        check(inventory==originalInventory&&game->isState(inventory)&&!InventoryClickGateAccess::modal(inventory),9001);
        check(clicks==baselineClicks&&callbacks==baselineCallbacks&&closes==0&&destroyed==0,9002);
        check(InventoryClickGateAccess::widget(inventory)->getSelectedItem()==fixtureItem,9003);
        check(InventoryClickGateAccess::consumedButtons(inventory)&SDL_BUTTON(SDL_BUTTON_LEFT),9004);
        // This fresh ordinary pair is distinct from the missing owned tail.
        click(16,80); orphanPhase=2; return true;
    }
    if(orphanPhase==2&&inputs.empty()) {
        check(clicks==baselineClicks+1&&callbacks==baselineCallbacks+1,9005);
        check(!InventoryClickGateAccess::widget(inventory)->getSelectedItem()&&fixtureItem->getOwner()==player&&fixtureItem->getSlot()->getId()=="STR_RIGHT_HAND"&&player->getTimeUnits()==initialTU,9006);
        check(inits==baselineInits&&arranged==baselineArranged&&closes==0&&destroyed==0&&InventoryClickGateAccess::consumedButtons(inventory)==0,9007);
        finish(); return true;
    }
    return orphanPhase!=0;
}
} } }
