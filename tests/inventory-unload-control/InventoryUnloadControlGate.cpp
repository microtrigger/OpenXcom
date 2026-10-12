// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
// A finite, default-OFF native ordinary fixed Unload button click and pixels-only read fixture.
// Normal events, initialization, timers, blits and flips remain native.
// Oracles remain private; test-only frame evidence is retained in the disposable
// user directory and copied by the private runner, never sent on the public wire.
#include "InventoryUnloadControlGate.h"
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
#include "../../src/Engine/Language.h"
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
struct InventoryUnloadGateAccess {
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
    static Surface *otherButton(InventoryState *s) { return s->_btnGround; }
    static BattlescapeButton *unloadButton(InventoryState *s) { return s->_btnUnload; }
    static Surface *preview(InventoryState *s) { return s->_selAmmo; }
    static std::string warningText(InventoryState *s) { return s->_inv->_warning->_text->getText(); }
    static bool widgetHovered(InventoryState *s) { return s->_inv->_isHovered; }
    static bool buttonPressed(InventoryState *s) { return s->_btnUnload->isButtonPressed(SDL_BUTTON_LEFT); }
    static bool ownedY(Game *g) { return g->getStanceController()->_inventoryOwnedYRelease; }

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
    static bool unresolved(Game *g) { return g->getStanceController()->_inventoryUnloadDispatching && g->getStanceController()->_awaitingInventoryView && !g->getStanceController()->_inventoryDestination; }
    static bool baseMode(InventoryState *s) { return s->_inv->_base; }
    static void setBaseMode(InventoryState *s) { s->_inv->_base=true; }
    static bool awaiting(Game *g) { return g->getStanceController()->_awaitingInventoryView; }
    static InventoryState *destination(Game *g) { return g->getStanceController()->_inventoryDestination; }
    static YAML::Node stacks(InventoryState *s) { YAML::Node n; for (const auto &x:s->_inv->_stackLevel) for (const auto &y:x.second) { YAML::Node p; p["x"]=x.first; p["y"]=y.first; p["n"]=y.second; n.push_back(p); } return n; }
};
namespace InventoryUnloadControlGate { namespace {
using Clock = std::chrono::steady_clock;
enum { FixtureCount=24, PairCount=FixtureCount*2, PairWorlds=PairCount*2 };
// Stable semantic endpoints. Mutation recipes name these BEFORE native execution.
enum Endpoint { DISPATCH=101, PREHANDLER=102, CALLBACK=103, SEMANTIC=104,
    PIXELS=105, BARRIER=106, TERMINAL=107, RETIRE=108, CONSENT=109,
    RECEIPT=110, DOMAIN=111, ACTIVATION=112, OVERLAY=113, SEND_FAILURE=114,
    HELD_REFUSAL=115, STALE=116, KIND=117, LEDGER=118, RELEASE=119, TRANSITION=120, BUTTON_ROUTE=121, SOUND=122, OUTCOME=123, WARNING=124, MALFORMED=125, GEOMETRY=126, ORDER=127, PRESSED=128, SUPPORT=9000, SCHEMA=9001 };
enum Stage { WARM, WAIT_INVENTORY, PREPARE, WAIT_PREPARED, WAIT_I,
    HELD_J, HELD_K, WAIT_CONTEXT, SEND_COMMAND, WAIT_PENDING, CONFIRM,
    WAIT_CANCEL, WAIT_RETIRE, WAIT_ACTION, WAIT_SETTLE, BARRIER_REPLY,
    WAIT_POST, WAIT_STATUS, COLLECT, REPLAY, REPLAY_REPLY, WAIT_REARM,
    TERMINAL_WAIT, TERMINAL_OPEN, TERMINAL_REPLY, WAIT_SEND_FAILURE,
    FAILED_VIEW, FAILED_VIEW_REARM, CROSS_REPLY, LIMIT_SEND, LIMIT_REPLY,
    DOMAIN_REPLY, ORPHAN_DOWN, ORPHAN_RETIRE, ORPHAN_PAIR, FINISHED, COLLECT_ENTRY, MALFORMED_REPLY, ORPHAN_Y_SECOND, WAIT_WARNING, WAIT_DUPLICATE };
bool enabled=false,running=false,completed=false,pendingSetup=false,moved=false;
bool proposed=false,clickSent=false,terminalReplaced=false,needPickup=false,needPage=false;
int peer=-1,world=0,frames=0,inventoryFrames=0,modalFrames=0,inits=0,clicks=0;
int closes=0,destroyed=0,constructed=0,displayed=0,shown=0,dismissed=0,arranged=0;
int callbacks=0,baselineCallbacks=0,baselineClicks=0,baselineInits=0,baselineArranged=0;
int baselinePage=0,baselineHeld=-1,initialTU=40,targetX=16,targetY=80,prepX=16,prepY=80;
int parentYDown=0,parentYUp=0,parentMouseDown=0,parentMouseUp=0;
int buttonCalls=0, soundCalls=0, downCalls=0, upCalls=0;
int baselineButtons=0, baselineSounds=0, baselineDowns=0, baselineUps=0;
int inventoryYDown=0,inventoryYUp=0,realUpWithoutModal=0;
std::vector<std::string> route;
std::vector<int> initialGroundIds, initialUnitIds;
BattleItem *fixtureAmmo=0;
int initialAmmoQuantity=0;
bool initialTurnFlag=false;
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
    std::cerr << "INVENTORY_UNLOAD_FIXTURE_DIAGNOSTIC world-" << world << " stage-" << int(stage)
        << " clicks-" << clicks << " callbacks-" << callbacks << " inits-" << inits
        << " arranged-" << arranged << " destroyed-" << destroyed << " shown-" << shown << std::endl;
    std::cerr << "INVENTORY_UNLOAD_CONTROL_GATE FAIL assertion-" << endpoint << std::endl;
    std::_Exit(EXIT_FAILURE);
} }
#define require(ok) check((ok),SUPPORT)
#define expect(endpoint,ok) check((ok),(endpoint))
bool core() { return profile=="core"; }
bool manual() { return core()&&world%2==0; }
int fixture() { if(core()) return (world/2)%FixtureCount; if(profile=="disabled"||profile=="close-disabled") return 0; return 1; }
bool heldCase() { return fixture()!=0&&fixture()!=23; }
bool successCase() { const int f=fixture(); return f==1||f==2||f==3||f==7||f==9||f==11||f==20||f==21; }
bool groundCase() { const int f=fixture(); return f==3||f==10||f==11||f==14||f==15||f==20||f==21; }
const char *warningKey() { const int f=fixture(); if(f==4||f==16) return "STR_NO_AMMUNITION_LOADED"; if(f==12||f==13||f==14||f==15||f==19||f==22) return "STR_BOTH_HANDS_MUST_BE_EMPTY"; if(f==8||f==10) return "STR_NOT_ENOUGH_TIME_UNITS"; return ""; }
bool terminal() { return profile=="lost-I"||profile=="reopen-I"||profile=="save-I"||profile=="parent-I"; }
char postKind() { return 'I'; }
int phase() { return inventory&&owner->isState(inventory)?InventoryUnloadGateAccess::animation(inventory):0; }
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
    if(inventory && owner->isState(inventory)) { n["inventoryPage"]=InventoryUnloadGateAccess::page(inventory); BattleItem *held=InventoryUnloadGateAccess::widget(inventory)->getSelectedItem(); n["held"]=held?held->getId():-1; n["stacks"]=InventoryUnloadGateAccess::stacks(inventory); n["text"]=InventoryUnloadGateAccess::text(inventory); n["pressedLeft"]=InventoryUnloadGateAccess::pressed(inventory,SDL_BUTTON_LEFT); n["buttonPressed"]=InventoryUnloadGateAccess::buttonPressed(inventory); n["buttonInverted"]=InventoryUnloadGateAccess::unloadButton(inventory)->stanceToggled(); n["pressedRight"]=InventoryUnloadGateAccess::pressed(inventory,SDL_BUTTON_RIGHT); n["pressedX2"]=InventoryUnloadGateAccess::pressed(inventory,SDL_BUTTON_X2); }
    for(BattleItem *i:*b->getItems()) { YAML::Node v; v["id"]=i->getId(); v["owner"]=i->getOwner()?i->getOwner()->getId():-1; v["previousOwner"]=i->getPreviousOwner()?i->getPreviousOwner()->getId():-1; v["turnFlag"]=i->getTurnFlag(); v["isAmmo"]=i->isAmmo(); v["ammo"]=i->getAmmoItem()?i->getAmmoItem()->getId():-1; v["quantity"]=i->getAmmoQuantity(); v["fuse"]=i->getFuseTimer(); v["slot"]=i->getSlot()?i->getSlot()->getId():""; v["x"]=i->getSlotX(); v["y"]=i->getSlotY(); if(i->getTile()) v["tile"]=i->getTile()->getPosition(); n["itemRuntime"].push_back(v); }
    for(BattleItem *i:*player->getTile()->getInventory()) n["groundVector"].push_back(i->getId());
    for(BattleItem *i:*player->getInventory()) n["unitVector"].push_back(i->getId());
    if(inventory&&owner->isState(inventory)) { Surface *a=InventoryUnloadGateAccess::preview(inventory); std::string pixels; const char *hex="0123456789abcdef"; for(int y=0;y<a->getHeight();++y) for(int x=0;x<a->getWidth();++x) { const Uint8 c=a->getPixel(x,y); pixels+=hex[c>>4]; pixels+=hex[c&15]; } n["ammoPreview"]=pixels; }
    require(s.rng==RNG::getSeed()); return s;
}
void compare(const Snapshot &a,const Snapshot &b) { expect(SEMANTIC,a.rng==b.rng && equal(a.save,b.save) && equal(a.runtime,b.runtime)); }
void fieldsAt(int line,const YAML::Node &n,std::initializer_list<const char*> names)
{
    check(n.IsMap()&&n.size()==names.size(),line); std::set<std::string> left;
    for(const char *s:names) left.insert(s);
    for(auto e:n) check(e.first.IsScalar()&&left.erase(e.first.Scalar())==1,line); check(left.empty(),line);
}
#define fields(...) fieldsAt(SCHEMA,__VA_ARGS__)
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
        require(n["schema"].as<std::string>()==(type=='Q'?"experimental-inventory-unload-context/1":type=='U'?"experimental-inventory-right-click-context/1":type=='H'?"experimental-inventory-click-context/1":type=='K'?"experimental-inventory-ground-context/1":type=='J'?"experimental-inventory-close-context/1":type=='V'?"experimental-tactical-context/1":"experimental-stance-context/1"));
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
    const std::string suffix=(kind=='C'||kind=='G'||kind=='F')?"":kind=='B'?"o0":kind=='D'?"sk":kind=='N'?"n0":(kind=='X'||kind=='E')?"016080":"160080";
    return std::string(1,kind)+epoch+grant+id+suffix;
}
void command() { proposed=true; commandId=randomToken(); commandPacket=packet('F',commandId,capability); sendPacket(commandPacket); }
void remember()
{
    require(inventory&&owner->isState(inventory)); before=snapshot(); originalInventory=inventory;
    baselineClicks=clicks; baselineCallbacks=callbacks; baselineInits=inits; baselineArranged=arranged;
    baselinePage=InventoryUnloadGateAccess::page(inventory); baselineSurfaces=InventoryUnloadGateAccess::surfaces(inventory);
    BattleItem *held=InventoryUnloadGateAccess::widget(inventory)->getSelectedItem(); baselineHeld=held?held->getId():-1;
    initialTU=player->getTimeUnits(); baselineButtons=buttonCalls; baselineSounds=soundCalls; baselineDowns=downCalls; baselineUps=upCalls; route.clear();
    initialAmmoQuantity=fixtureAmmo?fixtureAmmo->getAmmoQuantity():0; initialTurnFlag=fixtureItem?fixtureItem->getTurnFlag():false;
    initialGroundIds.clear(); for(BattleItem *i:*player->getTile()->getInventory()) initialGroundIds.push_back(i->getId());
    initialUnitIds.clear(); for(BattleItem *i:*player->getInventory()) initialUnitIds.push_back(i->getId());
}
void preserved()
{
    expect(RETIRE,inventory==originalInventory&&owner->isState(inventory)&&closes==0&&destroyed==0&&constructed==1);
    expect(RETIRE,inits==baselineInits&&arranged==baselineArranged&&clicks==baselineClicks&&callbacks==baselineCallbacks);
    expect(RETIRE,InventoryUnloadGateAccess::page(inventory)==baselinePage&&InventoryUnloadGateAccess::surfaces(inventory)==baselineSurfaces);
    compare(before,snapshot());
}
void outcome()
{
    const int f=fixture(); const bool success=successCase();
    expect(OUTCOME,player->getTimeUnits()==initialTU-(success?8:0));
    expect(OUTCOME,InventoryUnloadGateAccess::widget(inventory)->getSelectedItem()==(success?0:heldCase()?fixtureItem:0));
    expect(SOUND,soundCalls==baselineSounds+(success?1:0));
    if(success) {
        expect(OUTCOME,fixtureItem&&fixtureAmmo&&fixtureItem->getAmmoItem()==0&&!fixtureAmmo->isAmmo());
        expect(OUTCOME,fixtureItem->getOwner()==player&&fixtureAmmo->getOwner()==player&&fixtureItem->getPreviousOwner()==player&&fixtureAmmo->getPreviousOwner()==player);
        expect(OUTCOME,fixtureItem->getSlot()->getId()=="STR_RIGHT_HAND"&&fixtureAmmo->getSlot()->getId()=="STR_LEFT_HAND"&&fixtureItem->getSlotX()==0&&fixtureItem->getSlotY()==0&&fixtureAmmo->getSlotX()==0&&fixtureAmmo->getSlotY()==0);
        expect(OUTCOME,fixtureAmmo->getAmmoQuantity()==initialAmmoQuantity&&fixtureItem->getTurnFlag()==(groundCase()?false:initialTurnFlag));
        std::vector<int> ground; for(BattleItem *i:*player->getTile()->getInventory()) ground.push_back(i->getId());
        std::vector<int> expected=initialGroundIds; expected.erase(std::remove(expected.begin(),expected.end(),fixtureItem->getId()),expected.end()); expect(ORDER,ground==expected);
        std::vector<int> unit; for(BattleItem *i:*player->getInventory()) unit.push_back(i->getId());
        expected=initialUnitIds; expected.erase(std::remove(expected.begin(),expected.end(),fixtureItem->getId()),expected.end()); expected.push_back(fixtureAmmo->getId()); expected.push_back(fixtureItem->getId()); expect(ORDER,unit==expected);
        const auto text=InventoryUnloadGateAccess::text(inventory); expect(OUTCOME,text["item"].as<std::string>().empty()&&text["ammo"].as<std::string>().empty());
        Surface *preview=InventoryUnloadGateAccess::preview(inventory); for(int y=0;y<preview->getHeight();++y) for(int x=0;x<preview->getWidth();++x) expect(OUTCOME,preview->getPixel(x,y)==0);
    } else { Snapshot now=snapshot(); expect(OUTCOME,before.rng==now.rng&&equal(before.save,now.save)); }
    const std::string warning=warningKey();
    if(stage==WAIT_ACTION) expect(WARNING,InventoryUnloadGateAccess::warning(inventory)->getVisible()==!warning.empty());
    if(!warning.empty()) expect(WARNING,InventoryUnloadGateAccess::warningText(inventory)==std::string(owner->getLanguage()->getString(warning)));
}
void dispatched(bool settled=false)
{
    expect(DISPATCH,clicks==baselineClicks+1&&inits==baselineInits&&arranged==baselineArranged&&closes==0);
    expect(CALLBACK,callbacks==baselineCallbacks+1);
    expect(BUTTON_ROUTE,buttonCalls==baselineButtons+1&&downCalls==baselineDowns+1&&upCalls==baselineUps+1);
    expect(ORDER,route==std::vector<std::string>({"down","hover-down","down-return","up","hover-up","click","invClick","button","up-return"}));
    expect(DISPATCH,inventory==originalInventory&&owner->isState(inventory)&&constructed==1&&destroyed==0);
    expect(OUTCOME,InventoryUnloadGateAccess::page(inventory)==baselinePage);
    expect(PRESSED,!InventoryUnloadGateAccess::pressed(inventory,SDL_BUTTON_LEFT)&&!InventoryUnloadGateAccess::buttonPressed(inventory)&&!InventoryUnloadGateAccess::unloadButton(inventory)->stanceToggled());
    if(settled) expect(DISPATCH,!InventoryUnloadGateAccess::modal(inventory)&&InventoryUnloadGateAccess::surfaces(inventory)==baselineSurfaces);
    outcome();
}
void verifyBarrier()
{
    expect(BARRIER,InventoryUnloadGateAccess::awaiting(owner)&&InventoryUnloadGateAccess::destination(owner)==inventory&&!InventoryUnloadGateAccess::awaitingClose(owner));
}
void retainFrame(const std::vector<unsigned char> &pixels,int p,const char *kind)
{
    require(pixels.size()==320*200*3&&p>=0&&p<=8);
    const std::string file="inventory-unload-frame-"+std::to_string(world)+"-"+kind+"-"+std::to_string(p)+".ppm";
    std::ofstream out(Options::getUserFolder()+file,std::ios::binary); require(out.good());
    out << "P6\n320 200\n255\n"; out.write(reinterpret_cast<const char *>(pixels.data()),pixels.size()); out.close(); require(out.good());
    std::cerr << "INVENTORY_UNLOAD_FRAME_EVIDENCE world-" << world << " phase-" << p << " file-" << file << std::endl;
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
    const auto physical=display(); Surface *frame=InventoryUnloadGateAccess::banner(inventory), *text=InventoryUnloadGateAccess::overlayText(inventory), *yes=InventoryUnloadGateAccess::yes(inventory), *no=InventoryUnloadGateAccess::no(inventory), *pointer=InventoryUnloadGateAccess::pointer(inventory), *cursor=owner->getCursor();
    Text *label=InventoryUnloadGateAccess::overlayText(inventory);
    const std::string expected="Ordinary inventory Unload.\nMay spend 8 TU; normal refusal.\nConfirm the soldier shown above.";
    expect(OVERLAY,label->getText()==expected&&label->getTextHeight()>0&&label->getTextHeight()<=label->getHeight());
    int opaque=0;
    for(int y=frame->getY();y<frame->getY()+32;++y) for(int x=0;x<320;++x) {
        SDL_Color color={}; bool drawn=false; for(Surface *component:{frame,text,yes,no,cursor}) if(renderedColor(component,x,y,color)) drawn=true;
        if(drawn) { ++opaque; const int offset=(y*320+x)*3; expect(OVERLAY,physical[offset]==color.r&&physical[offset+1]==color.g&&physical[offset+2]==color.b); }
    }
    require(opaque>9000);

}
void afterView()
{
    dispatched(true);
    expect(BARRIER,!InventoryUnloadGateAccess::awaiting(owner)&&!InventoryUnloadGateAccess::awaitingClose(owner));
    if(core()) {
        if(manual()) { manualResult=snapshot(); manualPhasePixels.clear(); }
        else { compare(manualResult,snapshot()); expect(PIXELS,manualPhasePixels.size()==8); commandPhases.clear(); }
        // Retain the actual post-view frame (possibly transient). drawItems() may
        // clear primer pixels without advancing the existing phase label.
        retainFrame(lastPixels,lastViewPhase,manual()?"manual-post-view":"command-post-view");
        // Enter collection on an actual rendered frame, then require a natural
        // phase advance from that entry in both manual and controlled outcomes.
        manualTicks=0; stage=WAIT_WARNING;
    } else if(profile=="ledger-I") { after=snapshot(); replay=0; stage=REPLAY; }
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
bool active() { const char *s=std::getenv("OPENXCOM_INVENTORY_UNLOAD_CONTROL_GATE"); return s&&std::string(s)=="1"; }
void prepare()
{
    enabled=active(); if(!enabled) return; const char *p=std::getenv("OPENXCOM_INVENTORY_UNLOAD_PROFILE"); profile=p?p:"";
    const std::set<std::string> profiles={"core","deny-held","timeout","capability-timeout","disconnect","unseen","held-y","held-mouse","invalid-child-left","invalid-child-right","invalid-child-x2","invalid-child-moved","release-Y","release-mouse","stale-input","stale-actor","stale-reinit","stale-held","stale-held-position","capless-I","cross-kind","duplicate-pending","ledger-I","ledger-limit","disabled","close-disabled","independent","nontu","base","debug","malformed","malformed-short","malformed-extra","malformed-token","malformed-Q-short","malformed-Q-long","malformed-nul","upper-band","failed-I","send-failure-I","lost-I","reopen-I","save-I","parent-I","orphan-left-held","orphan-left-disconnect","orphan-Y","orphan-Y-disconnect","nested-transition","nested-pop","nested-save-notice","hidden-button","moved-button","resized-button","overlap-button","hidden-widget","moved-widget","widget-actor","widget-mode","slot-geometry","focus","palette"}; require(profiles.count(profile)==1);
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
        inits=clicks=callbacks=closes=destroyed=constructed=displayed=shown=dismissed=arranged=frames=inventoryFrames=modalFrames=buttonCalls=soundCalls=downCalls=upCalls=0; route.clear(); inventoryYDown=inventoryYUp=realUpWithoutModal=0;
        moved=proposed=clickSent=false; parentYDown=parentYUp=parentMouseDown=parentMouseUp=0; inventory=originalInventory=0; fixtureItem=fixtureAmmo=0; inputs.clear(); stage=WARM; worldStarted=Clock::now();
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
    const int f=fixture(); initialTU=(f==8||f==10||f==16||f==17||f==18||f==19)?7:(f==9||f==11)?8:(f==22||f==23)?0:40; player->setTimeUnits(initialTU);
    RuleInventory *ground=mod->getInventory("STR_GROUND",true); require(ground->getX()==0&&(320-ground->getX())/16==20&&(200-ground->getY())/16==3);
    auto addGround=[&](BattleItem *i) { player->getTile()->addItem(i,ground); i->setSlotX(200+int(player->getTile()->getInventory()->size())); i->setSlotY(1); return i; };
    targetX=304; targetY=44; prepX=16; prepY=80; needPickup=heldCase(); needPage=f==21;
    const std::string ammo=master=="xcom1"?"STR_RIFLE_CLIP":"STR_HARPOON_POD";
    const std::string gun=master=="xcom1"?"STR_RIFLE":"STR_JET_HARPOON";
    if(f==21) for(int i=0;i<25;++i) { BattleItem *med=addGround(item("STR_MEDI_KIT",0,"STR_GROUND")); med->setPainKillerQuantity(i%11); med->setHealQuantity(i/11); }
    if(f==20) { addGround(item(ammo,0,"STR_GROUND")); addGround(item(ammo,0,"STR_GROUND")); addGround(item("STR_MEDI_KIT",0,"STR_GROUND")); }
    if(f!=0) {
        const bool unloaded=f==4||f==16, intrinsic=f==5||f==17, nonweapon=f==6||f==18;
        if(intrinsic) fixtureItem=item(master=="xcom1"?"STR_LASER_PISTOL":"STR_THERMAL_TAZER",player,"STR_RIGHT_HAND");
        else if(nonweapon) fixtureItem=item("STR_MEDI_KIT",player,"STR_RIGHT_HAND");
        else if(unloaded) fixtureItem=item(gun,player,"STR_RIGHT_HAND");
        else { fixtureItem=arm(player); fixtureAmmo=fixtureItem->getAmmoItem(); }
        if(f==2||f==13) { fixtureItem->setSlot(mod->getInventory("STR_LEFT_HAND",true)); prepX=144; }
        if(groundCase()) { fixtureItem->moveToOwner(0); addGround(fixtureItem); fixtureItem->setTurnFlag(true); }
        if(f==7||f==19) fixtureAmmo->setAmmoQuantity(0);
        if(f==12||f==15||f==16||f==17||f==18||f==19||f==22||f==23) item(ammo,player,"STR_LEFT_HAND");
        if(f==13||f==14) item(ammo,player,"STR_RIGHT_HAND");
    }
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
bool hasRight() {
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK
    return profile!="independent";
#else
    return false;
#endif
}
std::vector<char> otherCommands() {
    std::vector<char> kinds={'B','D','N','R','L','C'};
    if(hasGround()) kinds.push_back('G'); if(hasClick()) kinds.push_back('E'); if(hasRight()) kinds.push_back('X'); return kinds;
}
std::vector<char> allReads() {
    std::vector<char> kinds={'O','V','I','J','Q'};
    if(hasGround()) kinds.push_back('K'); if(hasClick()) kinds.push_back('H'); if(hasRight()) kinds.push_back('U'); return kinds;
}
std::vector<char> blockedReads()
{
    std::vector<char> kinds=allReads();
    kinds.erase(std::remove(kinds.begin(),kinds.end(),postKind()),kinds.end()); return kinds;
}
bool geometry() { return profile=="hidden-button"||profile=="moved-button"||profile=="resized-button"||profile=="overlap-button"||profile=="hidden-widget"||profile=="moved-widget"||profile=="widget-actor"||profile=="widget-mode"||profile=="slot-geometry"; }
void changeGeometry()
{
    if(profile=="hidden-button") InventoryUnloadGateAccess::unloadButton(inventory)->setVisible(false);
    if(profile=="moved-button") InventoryUnloadGateAccess::unloadButton(inventory)->setX(287);
    if(profile=="resized-button") InventoryUnloadGateAccess::unloadButton(inventory)->setWidth(31);
    if(profile=="overlap-button") { InventoryUnloadGateAccess::otherButton(inventory)->setX(288); InventoryUnloadGateAccess::otherButton(inventory)->setY(32); }
    if(profile=="hidden-widget") InventoryUnloadGateAccess::widget(inventory)->setVisible(false);
    if(profile=="moved-widget") InventoryUnloadGateAccess::widget(inventory)->setX(1);
    if(profile=="widget-actor") InventoryUnloadGateAccess::wrongWidgetActor(inventory,twin);
    if(profile=="widget-mode") InventoryUnloadGateAccess::wrongWidgetMode(inventory);
    if(profile=="slot-geometry") { YAML::Node n; n["x"]=1; owner->getMod()->getInventory("STR_RIGHT_HAND",true)->load(n,0); }
}
bool denial() { return profile=="deny-held"||profile=="timeout"||profile=="unseen"||profile=="held-y"||profile=="held-mouse"||profile=="invalid-child-right"||profile=="invalid-child-x2"||profile=="invalid-child-left"||profile=="invalid-child-moved"; }
bool stale() { return profile.find("stale-")==0||profile=="capless-I"||profile=="capability-timeout"||profile=="focus"||profile=="palette"; }
void queryStatus() { sendPacket("S"+commandId); stage=WAIT_CANCEL; }
void startBarrier() { barrier=0; request(blockedReads()[0]); stage=BARRIER_REPLY; }
}
void beforeEvents(Game *game)
{
    if(!running||pendingSetup) return;
    require(Clock::now()-started<std::chrono::seconds(core()?600:(profile=="timeout"||profile=="capability-timeout")?155:45));
    require(Clock::now()-worldStarted<std::chrono::seconds((profile=="timeout"||profile=="capability-timeout")?145:35)); quiet();
    if(!moved) { motion(); moved=true; return; }
    if(stage==PREPARE) { prepPhase=0; stage=WAIT_PREPARED; }
    else if(stage==WAIT_PREPARED&&inputs.empty()) {
        if(prepPhase==0) {
            if(groundCase()) {
                const int wanted=(fixtureItem->getSlotX()/20)*20;
                if(InventoryUnloadGateAccess::page(inventory)!=wanted) { click(304,144); return; }
                prepX=(fixtureItem->getSlotX()-wanted)*16+8; prepY=game->getMod()->getInventory("STR_GROUND",true)->getY()+fixtureItem->getSlotY()*16+8;
            }
            ++prepPhase; if(needPickup) { click(prepX,prepY); return; }
        }
        if(prepPhase==1) {
            ++prepPhase; if(needPickup) require(InventoryUnloadGateAccess::widget(inventory)->getSelectedItem()==fixtureItem);
            if(profile=="held-mouse") motion(330,210); // Establish normal mouse-out presentation before baseline.
            else motion(160,profile=="upper-band"?136:80);
            return;
        }
        remember(); request('I'); stage=WAIT_I;
    } else if(stage==SEND_COMMAND) {
        if(profile=="stale-input") motion(151,81);
        if(profile=="stale-actor") { game->getSavedGame()->getSavedBattle()->setSelectedUnit(twin); game->getSavedGame()->getSavedBattle()->setSelectedUnit(player); }
        if(profile=="stale-reinit") inventory->init();
        if(profile=="stale-held") { InventoryUnloadGateAccess::widget(inventory)->setSelectedItem(0); InventoryUnloadGateAccess::widget(inventory)->setSelectedItem(fixtureItem); }
        if(profile=="stale-held-position") InventoryUnloadGateAccess::selection(inventory)->setX(InventoryUnloadGateAccess::selection(inventory)->getX()+1);
        if(profile=="capability-timeout") SDL_Delay(120100);
        if(profile=="focus") focus(false);
        if(profile=="palette") {
            SDL_Color palette[256]; std::copy(game->getScreen()->getPalette(),game->getScreen()->getPalette()+256,palette);
            // Invalidate the supported presentation, then leave only a partial
            // deferred palette. Changing the offscreen Surface alone does not.
            game->getScreen()->resetDisplay(false);
            game->getScreen()->setPalette(palette,0,1,false);
            require(!game->getScreen()->stanceAllowed()&&!game->getScreen()->inventoryWaitingAllowed());
        }
        command(); stage=WAIT_PENDING;
    } else if(stage==WAIT_PENDING&&available()) {
        const YAML::Node n=receive();
        if(stale()) { expect(STALE,n["status"]&&n["status"].as<std::string>()=="unavailable"); status(n,"unavailable"); expect(STALE,clicks==baselineClicks&&closes==0&&destroyed==0); finish(); return; }
        expect(CONSENT,n["status"]&&n["status"].as<std::string>()=="pending_confirmation"); status(n,"pending_confirmation");
        expect(CONSENT,game->isState(inventory)&&shown==domainIndex+1&&clicks==baselineClicks&&arranged==baselineArranged&&closes==0&&destroyed==0&&inits==baselineInits);
        if(profile=="duplicate-pending") { replay=0; sendPacket(commandPacket); stage=WAIT_DUPLICATE; }
        else stage=profile=="unseen"?WAIT_RETIRE:CONFIRM;
    } else if(stage==WAIT_DUPLICATE&&available()) {
        const YAML::Node n=receive(); const std::string expected=replay==0?"pending_confirmation":"unavailable";
        expect(LEDGER,n["status"]&&n["status"].as<std::string>()==expected&&shown==1&&buttonCalls==baselineButtons&&clicks==baselineClicks); status(n,expected);
        if(replay==0) { std::string conflict=commandPacket; conflict.replace(33,32,randomToken()); sendPacket(conflict); ++replay; }
        else if(replay==1) { sendPacket(packet('C',commandId,capability)); ++replay; }
        else stage=CONFIRM;
    } else if(stage==CONFIRM&&modalFrames>0) {
        require(game->isState(inventory)&&InventoryUnloadGateAccess::modal(inventory));
        if(profile=="orphan-left-held"||profile=="orphan-left-disconnect") {
            Surface *yes=InventoryUnloadGateAccess::yes(inventory);
            motion(yes->getX()+yes->getWidth()/2,yes->getY()+yes->getHeight()/2);
            mouseEdge(yes->getX()+yes->getWidth()/2,yes->getY()+yes->getHeight()/2,true); stage=ORPHAN_DOWN;
        } else if(profile=="orphan-Y"||profile=="orphan-Y-disconnect") {
                keyEdge(SDLK_y,true); clickSent=true; stage=WAIT_ACTION; // Deliberately omit UP.
        } else if(profile=="release-mouse") {
            Surface *yes=InventoryUnloadGateAccess::yes(inventory);
            motion(yes->getX()+10,yes->getY()+5); mouseEdge(yes->getX()+10,yes->getY()+5,true);
            key(SDLK_y); mouseEdge(yes->getX()+10,yes->getY()+5,false); clickSent=true; stage=WAIT_ACTION;
        } else if(profile=="disconnect") { close(peer); peer=-1; stage=WAIT_RETIRE; }
        else if(profile=="timeout") { SDL_Delay(120100); key(SDLK_y); stage=WAIT_RETIRE; }
        else if(profile=="held-y") { keyEdge(SDLK_y,true); keyEdge(SDLK_y,false); stage=WAIT_RETIRE; }
        else if(profile=="held-mouse") { Surface *yes=InventoryUnloadGateAccess::yes(inventory); mouseEdge(yes->getX()+10,yes->getY()+5,false); stage=WAIT_RETIRE; }
        else if(profile=="invalid-child-right"||profile=="invalid-child-x2"||profile=="invalid-child-left"||profile=="invalid-child-moved") {
            if(profile=="invalid-child-right"||profile=="invalid-child-left") InventoryUnloadGateAccess::yes(inventory)->setVisible(false);
            else InventoryUnloadGateAccess::yes(inventory)->setX(241);
            const int button=profile=="invalid-child-right"?SDL_BUTTON_RIGHT:profile=="invalid-child-x2"?SDL_BUTTON_X2:SDL_BUTTON_LEFT;
            mouseEdge(160,80,true,button); mouseEdge(160,80,false,button); stage=WAIT_RETIRE;
        } else if(denial()) { key(SDLK_ESCAPE); stage=WAIT_RETIRE; }
        else if(core()&&world%4==3) {
            Surface *yes=InventoryUnloadGateAccess::yes(inventory); click(yes->getX()+yes->getWidth()/2,yes->getY()+yes->getHeight()/2); clickSent=true; stage=WAIT_ACTION;
        } else { key(SDLK_y); clickSent=true; stage=WAIT_ACTION; }
    } else if(stage==ORPHAN_DOWN&&inputs.empty()) {
        if(profile=="orphan-left-disconnect") { close(peer); peer=-1; }
        else key(SDLK_ESCAPE);
        stage=ORPHAN_RETIRE;
    } else if(stage==ORPHAN_RETIRE&&inputs.empty()&&inventory&&game->isState(inventory)&&!InventoryUnloadGateAccess::modal(inventory)) {
        expect(RELEASE,clicks==baselineClicks&&callbacks==baselineCallbacks&&InventoryUnloadGateAccess::widget(inventory)->getSelectedItem()==fixtureItem);
        // Missing original UP cannot capture this NEW ordinary DOWN/UP pair.
        click(16,80); stage=ORPHAN_PAIR;
    } else if(stage==ORPHAN_PAIR&&inputs.empty()) {
        expect(RELEASE,clicks==baselineClicks+1&&callbacks==baselineCallbacks+1&&!InventoryUnloadGateAccess::widget(inventory)->getSelectedItem());
        expect(RELEASE,InventoryUnloadGateAccess::consumedButtons(inventory)==0&&inits==baselineInits&&arranged==baselineArranged&&closes==0&&destroyed==0);
        expect(RELEASE,player->getTimeUnits()==initialTU&&fixtureItem->getAmmoItem()==fixtureAmmo&&fixtureItem->getOwner()==player&&fixtureItem->getSlot()->getId()=="STR_RIGHT_HAND");
        finish();
    } else if(stage==ORPHAN_Y_SECOND&&inputs.empty()) {
        expect(RELEASE,inventoryYDown==1&&inventoryYUp==1&&!InventoryUnloadGateAccess::ownedY(game));
        expect(RELEASE,buttonCalls==baselineButtons+1&&clicks==baselineClicks+1&&callbacks==baselineCallbacks+1); finish();
    } else if(stage==MALFORMED_REPLY&&available()) {
        YAML::Node n=receive(); expect(MALFORMED,n.IsMap()&&n.size()==1&&n["error"]&&n["error"].as<std::string>()=="unavailable"); preserved(); finish();
    } else if(stage==WAIT_RETIRE&&inputs.empty()&&inventory&&game->isState(inventory)&&!InventoryUnloadGateAccess::modal(inventory)) {
        preserved(); expect(RETIRE,InventoryUnloadGateAccess::consumedButtons(inventory)==0);
        if(peer<0||!proposed) { expect(RETIRE,InventoryUnloadGateAccess::widget(inventory)->getSelectedItem()==fixtureItem); finish(); }
        else queryStatus();
    } else if(stage==WAIT_CANCEL&&available()) {
        YAML::Node n=receive(); expect(CONSENT,n["status"]&&n["status"].as<std::string>()=="unavailable"); status(n,"unavailable"); preserved();
        finish();
    } else if(stage==BARRIER_REPLY&&available()) {
        YAML::Node n=receive(); expect(BARRIER,n.IsMap()&&n.size()==1&&n["error"]&&n["error"].as<std::string>()=="unavailable"); verifyBarrier(); dispatched(true);
        if(++barrier<int(blockedReads().size())) request(blockedReads()[barrier]);
        else if(terminal()) beginTerminal();
        else if(profile.find("send-failure-")==0) { require(shutdown(peer,SHUT_RD)==0); request(postKind()); stage=WAIT_SEND_FAILURE; }
        else if(profile=="failed-I") { InventoryUnloadGateAccess::widget(inventory)->setVisible(false); request('I'); stage=FAILED_VIEW; }
        else { request(postKind()); stage=WAIT_POST; }
    } else if(stage==WAIT_SEND_FAILURE&&!game->getStanceController()->enabled()) {
        expect(SEND_FAILURE,InventoryUnloadGateAccess::awaiting(game)); dispatched(true); finish();
    } else if(stage==FAILED_VIEW&&available()) {
        unavailable(receive()); verifyBarrier();
        InventoryUnloadGateAccess::widget(inventory)->setVisible(true); request('Q'); stage=FAILED_VIEW_REARM;
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
        std::vector<char> kinds=otherCommands(); kinds.push_back('F'); commandId=randomToken(); std::string p=packet(kinds[limits%kinds.size()],commandId,randomToken()); if(limits==0) firstPacket=p; sendPacket(p); stage=LIMIT_REPLY;
    } else if(stage==LIMIT_REPLY&&available()) {
        YAML::Node n=receive();
        if(limits<64) { expect(LEDGER,n["status"]&&n["status"].as<std::string>()=="unavailable"); status(n,"unavailable"); compare(before,snapshot()); ++limits; stage=LIMIT_SEND; }
        else { expect(LEDGER,n["error"]&&n["error"].as<std::string>()=="unavailable"&&commandIds.size()==65); expect(LEDGER,clicks==0&&closes==0); finish(); }
    } else if(stage==TERMINAL_WAIT&&inputs.empty()&&idle()&&InventoryUnloadGateAccess::initialized(game)&&InventoryUnloadGateAccess::deleted(game)==0) {
        if(profile=="reopen-I") { key(Options::keyBattleInventory); stage=TERMINAL_OPEN; }
        else { barrier=0; request('O'); stage=TERMINAL_REPLY; }
    } else if(stage==TERMINAL_OPEN&&inputs.empty()) {
        if(inventory&&game->isState(inventory)&&InventoryUnloadGateAccess::initialized(game)) { barrier=0; request('O'); stage=TERMINAL_REPLY; }
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
        if(profile=="nested-transition"||profile=="nested-pop"||profile=="nested-save-notice") {
            expect(TRANSITION,downCalls==baselineDowns+1&&upCalls==baselineUps&&clicks==baselineClicks&&callbacks==baselineCallbacks&&buttonCalls==baselineButtons&&soundCalls==baselineSounds&&InventoryUnloadGateAccess::awaiting(owner)&&!InventoryUnloadGateAccess::destination(owner)&&!InventoryUnloadGateAccess::awaitingClose(owner));
            if(profile=="nested-pop") stage=TERMINAL_WAIT;
            else { barrier=0; request('O'); stage=TERMINAL_REPLY; }
            return;
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
    if(inventory&&game->isState(inventory)&&InventoryUnloadGateAccess::modal(inventory)) { ++modalFrames; if(modalFrames==1) retainFrame(display(),phase(),"overlay"); verifyOverlayPixels(); }
    if(stage==WAIT_INVENTORY&&inventory&&game->isState(inventory)&&inputs.empty()&&++inventoryFrames>=2) {
        require(inits==1&&constructed==1&&clicks==0&&closes==0&&destroyed==0&&displayed>0); stage=PREPARE;
    }
    if(stage==WAIT_SETTLE&&frameSerial==dispatchedFrame+1) retainFrame(display(),phase(),manual()?"manual-first-rendered-after-action":"command-first-rendered-after-action");
    if(stage==WAIT_WARNING&&inventory&&!InventoryUnloadGateAccess::warning(inventory)->getVisible()) stage=COLLECT_ENTRY;
    if(stage==COLLECT_ENTRY) {
        dispatched(true); lastPixels=display(); lastViewPhase=phase();
        retainFrame(lastPixels,lastViewPhase,manual()?"manual-collection-entry":"command-collection-entry");
        manualLastPhase=lastViewPhase; stage=COLLECT; return;
    }
    if(stage==COLLECT) {
        dispatched(true); const int p=phase();
        require(p>=0&&p<=8);
        if(p>0&&p!=manualLastPhase) {
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
        (inventory&&game->isState(inventory)&&InventoryUnloadGateAccess::initialized(game)&&InventoryUnloadGateAccess::deleted(game)==0)) {
        dispatched(true);
        expect(RELEASE,parentYDown==0&&parentYUp==0&&parentMouseDown==0&&parentMouseUp==0&&realUpWithoutModal==0&&inventoryYDown==0&&inventoryYUp==0);
        if(profile=="orphan-Y"||profile=="orphan-Y-disconnect") {
            if(profile=="orphan-Y-disconnect") { close(peer); peer=-1; key(SDLK_y); stage=ORPHAN_Y_SECOND; }
            else { key(SDLK_y); stage=ORPHAN_Y_SECOND; }
            return;
        }
        if(manual()) { request(postKind()); stage=WAIT_POST; } else { verifyBarrier(); startBarrier(); }
    }
    if(stage==WAIT_I&&available()) {
        if(profile=="nontu"||profile=="debug") { unavailable(receive()); preserved(); request('Q'); stage=WAIT_CONTEXT; return; }
        context(receive(),'I'); preserved();
        if(manual()) { click(targetX,targetY,SDL_BUTTON_LEFT); stage=WAIT_ACTION; }
        else {
            if(profile=="base") InventoryUnloadGateAccess::setBaseMode(inventory);
            if(geometry()) changeGeometry();
            if(profile=="held-y") keyEdge(SDLK_y,true);
            if(profile=="held-mouse") mouseEdge(330,210,true); // Held input outside all ordinary surfaces.
            if(heldCase()&&profile!="independent") { request('J'); stage=HELD_J; }
            else { request('Q'); stage=WAIT_CONTEXT; }
        }
    } else if(stage==HELD_J&&available()) {
        YAML::Node n=receive(); expect(HELD_REFUSAL,n["error"]&&n["error"].as<std::string>()=="unavailable"); preserved(); if(hasGround()) { request('K'); stage=HELD_K; } else { request('Q'); stage=WAIT_CONTEXT; }
    } else if(stage==HELD_K&&available()) {
        YAML::Node n=receive(); expect(HELD_REFUSAL,n["error"]&&n["error"].as<std::string>()=="unavailable"); preserved(); request('Q'); stage=WAIT_CONTEXT;
    } else if(stage==WAIT_CONTEXT&&available()) {
        YAML::Node n=receive();
        if(geometry()) { expect(GEOMETRY,n["error"]&&n["error"].as<std::string>()=="unavailable"&&shown==0&&buttonCalls==baselineButtons&&clicks==baselineClicks); finish(); return; }
        if(profile=="disabled"||profile=="close-disabled"||profile=="nontu"||profile=="base"||profile=="debug") {
            expect(ACTIVATION,n["error"]&&n["error"].as<std::string>()=="unavailable"); expect(ACTIVATION,clicks==baselineClicks&&closes==0&&destroyed==0); finish(); return;
        }
        if(profile=="held-mouse"&&n["error"]) { unavailable(n); preserved(); mouseEdge(330,210,false); stage=WAIT_RETIRE; return; }
        context(n,'Q'); preserved();
        if(profile.find("malformed")==0) {
            commandId=randomToken(); const std::string valid=packet('F',commandId,capability);
            std::string bad=valid+"304044";
            if(profile=="malformed-short") bad=valid.substr(0,96);
            if(profile=="malformed-extra") bad=valid+"x";
            if(profile=="malformed-token") bad=std::string("F")+std::string(96,'Z');
            if(profile=="malformed-Q-short") bad=std::string("Q")+std::string(31,'a');
            if(profile=="malformed-Q-long") bad=std::string("Q")+std::string(33,'a');
            if(profile=="malformed-nul") bad=valid+std::string(1,'\0');
            sendPacket(bad); stage=MALFORMED_REPLY;
        }
        else if(profile=="capless-I") { request('I'); stage=WAIT_REARM; }
        else if(profile=="cross-kind") { replay=0; commandId=randomToken(); sendPacket(packet('B',commandId,capability)); stage=CROSS_REPLY; }
        else stage=SEND_COMMAND;
    } else if(stage==WAIT_REARM&&available()) {
        if(profile=="capless-I") { context(receive(),'I'); preserved(); stage=SEND_COMMAND; }
        else { context(receive(),'H'); commandId=randomToken(); sendPacket(packet('F',commandId,capability)); stage=CROSS_REPLY; }
    } else if(stage==WAIT_POST&&available()) {
        context(receive(),postKind()); require(frameSerial>dispatchedFrame); dispatched(true);
        if(clickSent) { sendPacket("S"+commandId); stage=WAIT_STATUS; } else afterView();
    }
}
void inventoryConstructed(InventoryState *who) { if(running&&!pendingSetup) { require(!inventory); inventory=who; ++constructed; } }
void inventoryInitialized(InventoryState *who) { if(!running||pendingSetup) return; require(!inventory||inventory==who); inventory=who; ++inits; }
void inventoryDisplayed(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++displayed; }
bool actionEdge(Action *action)
{
    if(!action) return false; const SDL_Event *e=action->getDetails();
    return (e->type==SDL_MOUSEBUTTONDOWN||e->type==SDL_MOUSEBUTTONUP)&&e->button.button==SDL_BUTTON_LEFT&&int(action->getAbsoluteXMouse())==targetX&&int(action->getAbsoluteYMouse())==targetY;
}
void inventoryUnloadEventEntered(InventoryState *who,Action *action)
{
    if(!running||pendingSetup||who!=inventory||!action) return;
    SDL_Event *e=action->getDetails();
    if(!owner->isState(who)) {
        const bool edge=stage==WAIT_ACTION&&(e->type==SDL_MOUSEBUTTONDOWN||e->type==SDL_MOUSEBUTTONUP)&&e->button.button==SDL_BUTTON_LEFT&&e->button.x==(manual()?targetX*2:targetX)&&e->button.y==(manual()?targetY*2:targetY);
        if(edge) expect(TRANSITION,false); return;
    }
    if(!InventoryUnloadGateAccess::modal(who)) {
        if(e->type==SDL_KEYDOWN&&e->key.keysym.sym==SDLK_y) ++inventoryYDown;
        if(e->type==SDL_KEYUP&&e->key.keysym.sym==SDLK_y) ++inventoryYUp;
        if(stage==WAIT_ACTION&&!manual()&&e->type==SDL_MOUSEBUTTONUP&&e->button.which==255) ++realUpWithoutModal;
    }
    if(stage!=WAIT_ACTION) return;
    // At State::handle entry sender/scaled coordinates are not yet assigned.
    if((e->type!=SDL_MOUSEBUTTONDOWN&&e->type!=SDL_MOUSEBUTTONUP)||e->button.button!=SDL_BUTTON_LEFT) return;
    if(e->button.x!=(manual()?targetX*2:targetX)||e->button.y!=(manual()?targetY*2:targetY)) return;
    if(!manual()) expect(PREHANDLER,InventoryUnloadGateAccess::unresolved(owner));
    const bool down=e->type==SDL_MOUSEBUTTONDOWN;
    if(down) { ++downCalls; expect(BUTTON_ROUTE,downCalls==baselineDowns+1&&upCalls==baselineUps); }
    else { ++upCalls; expect(BUTTON_ROUTE,upCalls==baselineUps+1&&downCalls==baselineDowns+1); }
    route.push_back(down?"down":"up");
    if(!down) expect(PRESSED,InventoryUnloadGateAccess::pressed(who,SDL_BUTTON_LEFT)&&InventoryUnloadGateAccess::buttonPressed(who));
}
void inventoryUnloadEventReturned(InventoryState *who,Action *action)
{
    if(!running||pendingSetup||stage!=WAIT_ACTION||who!=inventory||!actionEdge(action)) return;
    expect(TRANSITION,owner->isState(who));
    const bool down=action->getDetails()->type==SDL_MOUSEBUTTONDOWN;
    route.push_back(down?"down-return":"up-return");
    expect(PRESSED,InventoryUnloadGateAccess::pressed(who,SDL_BUTTON_LEFT)==down&&InventoryUnloadGateAccess::buttonPressed(who)==down);
    expect(PRESSED,InventoryUnloadGateAccess::unloadButton(who)->stanceToggled()==(down&&master=="xcom2"));
    if(down&&!manual()) {
        if(profile=="nested-transition") { owner->pushState(new State); owner->popState(); }
        else if(profile=="nested-pop") owner->popState();
        else if(profile=="nested-save-notice") owner->getStanceController()->beforeSaveChange();
    }
}
void inventoryMouseOverEntered(Inventory *who,Action *action,State *parent)
{
    if(!running||pendingSetup||stage!=WAIT_ACTION||!inventory||who!=InventoryUnloadGateAccess::widget(inventory)||!actionEdge(action)) return;
    if(!manual()) expect(PREHANDLER,InventoryUnloadGateAccess::unresolved(owner));
    expect(DISPATCH,parent==inventory&&action->getSender()==who&&InventoryUnloadGateAccess::widgetHovered(inventory));
    route.push_back(action->getDetails()->type==SDL_MOUSEBUTTONDOWN?"hover-down":"hover-up");
}
void inventoryClickEntered(Inventory *who,Action *action,State *parent)
{
    if(!running||pendingSetup||!inventory||who!=InventoryUnloadGateAccess::widget(inventory)) return;
    ++clicks;
    if(stage==WAIT_ACTION) {
        expect(DISPATCH,clicks==baselineClicks+1); // Before any duplicate handler can mutate/free state.
        if(!manual()) expect(PREHANDLER,InventoryUnloadGateAccess::unresolved(owner));
        expect(DISPATCH,parent==inventory&&action&&action->getSender()==who&&actionEdge(action)&&action->getDetails()->type==SDL_MOUSEBUTTONUP);
        route.push_back("click");
        Surface *selection=InventoryUnloadGateAccess::selection(inventory);
        expect(DISPATCH,selection->getX()==targetX-selection->getWidth()/2-who->getX()&&selection->getY()==targetY-selection->getHeight()/2-who->getY());
    }
}
void inventoryInvClickEntered(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) { ++callbacks; if(stage==WAIT_ACTION) { if(!manual()) expect(PREHANDLER,InventoryUnloadGateAccess::unresolved(owner)); route.push_back("invClick"); } } }
void inventoryUnloadButtonEntered(InventoryState *who,Action *action)
{
    if(!running||pendingSetup||who!=inventory) return; ++buttonCalls;
    if(stage==WAIT_ACTION) {
        expect(TRANSITION,owner->isState(who));
        if(!manual()) expect(PREHANDLER,InventoryUnloadGateAccess::unresolved(owner));
        expect(BUTTON_ROUTE,buttonCalls==baselineButtons+1&&callbacks==baselineCallbacks+1&&clicks==baselineClicks+1);
        expect(BUTTON_ROUTE,action&&actionEdge(action)&&action->getSender()==InventoryUnloadGateAccess::unloadButton(who));
        expect(PRESSED,!InventoryUnloadGateAccess::buttonPressed(who)&&!InventoryUnloadGateAccess::unloadButton(who)->stanceToggled());
        route.push_back("button");
    }
}
void soundLookup(int depth,int sound)
{
    if(!running||pendingSetup||stage!=WAIT_ACTION) return;
    ++soundCalls; expect(SOUND,depth==0&&sound==Mod::ITEM_DROP&&soundCalls==baselineSounds+1&&successCase());
}
void inventoryCloseEntered(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++closes; }
void inventoryDestroyed(InventoryState *who) { if(running&&(!pendingSetup||terminalReplaced)&&who==inventory) { ++destroyed; inventory=0; } }
void clickOverlayShown(InventoryState *who)
{
    if(running&&!pendingSetup&&who==inventory) {
        ++shown; modalStarted=Clock::now(); modalFrames=0;
        Surface *band=InventoryUnloadGateAccess::banner(who),*yes=InventoryUnloadGateAccess::yes(who),*no=InventoryUnloadGateAccess::no(who),*sprite=InventoryUnloadGateAccess::selection(who),*mark=InventoryUnloadGateAccess::pointer(who);
        expect(OVERLAY,band->getX()==0&&band->getWidth()==320&&band->getHeight()==32&&(band->getY()==64||band->getY()==152));
        expect(OVERLAY,!heldCase()||sprite->getY()+sprite->getHeight()<=band->getY()||sprite->getY()>=band->getY()+32);
        expect(OVERLAY,targetY<band->getY()||targetY>=band->getY()+32);
        expect(OVERLAY,yes->getX()==242&&yes->getY()==band->getY()+1&&yes->getWidth()==76&&yes->getHeight()==14);
        expect(OVERLAY,no->getX()==242&&no->getY()==band->getY()+17&&no->getWidth()==76&&no->getHeight()==14);
        for(int y=0;y<200;++y) for(int x=0;x<320;++x) {
            const int expected=x>=242&&x<318?(y>=band->getY()+1&&y<band->getY()+15?1:y>=band->getY()+17&&y<band->getY()+31?2:0):0;
            expect(OVERLAY,InventoryUnloadGateAccess::choice(who,x,y)==expected);
        }
        if(profile=="upper-band") expect(OVERLAY,band->getY()==64);
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
        std::cout << "INVENTORY_UNLOAD_CONTROL_GATE PASS {\"schema\":\"inventory-unload-control-gate/1\",\"master\":\"" << master << "\",\"profile\":\"" << profile << "\",\"passed\":true}" << std::endl;
    }
    if(peer>=0) { close(peer); peer=-1; } return enabled&&!completed?EXIT_FAILURE:EXIT_SUCCESS;
}
} }
#undef require
#undef expect
#undef fields
