// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
// A finite, default-OFF native inventory Ground and pixels-only read fixture.
// Normal events, initialization, timers, blits and flips remain native.
// Oracles remain private; test-only frame evidence is retained in the disposable
// user directory and copied by the private runner, never sent on the public wire.
#include "InventoryGroundControlGate.h"
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
struct InventoryGroundGateAccess {
    static Inventory *widget(InventoryState *s) { return s->_inv; }
    static int page(InventoryState *s) { return s->_inv->_groundOffset; }
    static int stack(InventoryState *s,int x,int y) { const auto &levels=s->_inv->_stackLevel; const auto column=levels.find(x); if(column==levels.end()) return 0; const auto entry=column->second.find(y); return entry==column->second.end()?0:entry->second; }
    static int animation(InventoryState *s) { return s->_inv->_animFrame; }
    static bool modal(InventoryState *s) { return s->_modal != 0; }
    static size_t surfaces(InventoryState *s) { return s->_surfaces.size(); }
    static bool initialized(Game *g) { return g->_init; }
    static size_t deleted(Game *g) { return g->_deleted.size(); }
    static Surface *groundButton(InventoryState *s) { return s->_btnGround; }
    static YAML::Node stacks(InventoryState *s) { YAML::Node n; for (const auto &x:s->_inv->_stackLevel) for (const auto &y:x.second) { YAML::Node p; p["x"]=x.first; p["y"]=y.first; p["n"]=y.second; n.push_back(p); } return n; }
};
namespace InventoryGroundControlGate { namespace {
using Clock = std::chrono::steady_clock;
enum { FixtureCount = 10, PairCount = FixtureCount * 2, PairWorlds = PairCount * 2 };
enum Stage { WARM, WAIT_INVENTORY, PREPARE_INVENTORY, WAIT_PREPARED, WAIT_I,
    WAIT_HELD_READ, WAIT_HELD_REFUSAL, WAIT_CONTEXT, SEND_COMMAND, WAIT_PENDING,
    CONFIRM, WAIT_INPUT, WAIT_CANCEL_STATUS, WAIT_RETIRE, WAIT_DISPATCH,
    WAIT_SETTLE, BARRIER_REPLY, WAIT_POST_I, WAIT_STATUS, COLLECT_MANUAL,
    WAIT_CAPLESS, CROSS_REPLY, REPLAY_SEND, REPLAY_REPLY, WAIT_REARM,
    LIMIT_SEND, LIMIT_REPLY, LIMIT_FINAL, WAIT_DUPLICATE_PENDING,
    WAIT_TERMINAL_CLOSE, WAIT_TERMINAL_OPEN, TERMINAL_REQUEST, TERMINAL_REPLY,
    WAIT_FAILED_I, WAIT_FAILED_K, FINISHED };
bool enabled=false, running=false, completed=false, pendingSetup=false, moved=false;
bool prepared=false, retry=false, terminalReplaced=false, groundSent=false, lostOriginal=false;
int peer=-1, world=0, frames=0, inventoryFrames=0, modalFrames=0, inits=0, groundClicks=0;
int closes=0, destroyed=0, constructed=0, displayed=0, shown=0, dismissed=0, arranged=0;
int baselineInits=0, baselineArranged=0, baselineClicks=0, baselinePage=0, baselineAnimation=0;
int replay=0, barrier=0, limits=0, oracleCount=0, parityCount=0, frameSerial=0, dispatchedFrame=0;
int initialTU=40, lastViewPhase=0, manualTicks=0, manualLastPhase=0;
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
Snapshot before, opened, manualGround;
void check(bool ok,int line) { if(!ok) {
    std::cerr << "INVENTORY_GROUND_FIXTURE_DIAGNOSTIC world-" << world << " phase-" << int(stage)
        << " inits-" << inits << " ground-" << groundClicks << " arranged-" << arranged
        << " closes-" << closes << " destroyed-" << destroyed << " shown-" << shown
        << " dismissed-" << dismissed << " frames-" << frameSerial << std::endl;
    std::cerr << "INVENTORY_GROUND_CONTROL_GATE FAIL assertion-" << line << std::endl;
    std::_Exit(EXIT_FAILURE);
} }
#define require(ok) check((ok),__LINE__)
bool core() { return profile=="core"; }
bool manual() { return core() && world%2==0; }
bool terminal() { return profile=="lifetime" || profile=="reopen" || profile=="save" || profile=="parent"; }
bool inputCase() { return profile=="overlay-input"||profile=="input-x2"||profile=="input-wheel"||profile=="input-hotkey"||profile=="input-outside"||profile=="input-modifier"; }
bool cancelCase() { return profile=="cancel-page"||profile=="cancel-layout"||inputCase(); }
bool stale() { return profile=="stale-input"||profile=="stale-actor"||profile=="stale-reinit"||profile=="capability-timeout"||profile=="hidden-after-context"||profile=="held-after-context"; }
int fixture() { if(core()) return (world/2)%FixtureCount; if(profile=="cancel-page") return 4;
    if(profile=="cancel-layout"||profile=="held-item"||profile=="held-after-context"||profile=="held-confirm") return 2; if(profile=="overlay-input") return 8; return 1; }
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
    if(inventory && owner->isState(inventory)) { n["inventoryPage"]=InventoryGroundGateAccess::page(inventory); BattleItem *held=InventoryGroundGateAccess::widget(inventory)->getSelectedItem(); n["held"]=held?held->getId():-1; n["stacks"]=InventoryGroundGateAccess::stacks(inventory); }
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
// Status/2 correlation is transport-only, independent of command identity.
std::string statusQueryNonce(32, '0'), expectedStatusQueryNonce;
void sendStatusQuery()
{
    require(expectedStatusQueryNonce.empty());
    require(statusQueryNonce != std::string(32, 'f'));
    const char *hex = "0123456789abcdef";
    for (int i = 31; i >= 0; --i)
    {
        if (statusQueryNonce[i] == 'f') { statusQueryNonce[i] = '0'; continue; }
        statusQueryNonce[i] = hex[std::string(hex).find(statusQueryNonce[i]) + 1]; break;
    }
    expectedStatusQueryNonce = statusQueryNonce;
    sendPacket("S" + commandId + statusQueryNonce);
}
void status(const YAML::Node &n,const std::string &value)
{
    if (expectedStatusQueryNonce.empty()) require(n["query_nonce"].IsNull());
    else require(n["query_nonce"].IsScalar() && n["query_nonce"].as<std::string>() == expectedStatusQueryNonce);
    expectedStatusQueryNonce.clear();
    fields(n,{"schema","command_id","status","query_nonce"}); require(n["schema"].as<std::string>()=="experimental-stance-status/2"&&n["command_id"].as<std::string>()==commandId&&n["status"].as<std::string>()==value);
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
    lastPixels=display(); lastViewPhase=inventory?InventoryGroundGateAccess::animation(inventory):0; std::map<unsigned int,unsigned int> first;
    for(size_t i=0;i<indices.size();++i) { unsigned int rgb=(lastPixels[i*3]<<16)|(lastPixels[i*3+1]<<8)|lastPixels[i*3+2];
        if(!first.count(rgb)) { unsigned int next=first.size(); first[rgb]=next; } require(first.size()<=256&&indices[i]==first[rgb]);
        for(int c=0;c<3;++c) require(palette[indices[i]*3+c]==lastPixels[i*3+c]); }
    for(size_t i=first.size()*3;i<palette.size();++i) require(palette[i]==0); ++oracleCount;
}
void context(const YAML::Node &n,char type)
{
    if(type=='I') { fields(n,{"schema","challenge","semantics","view"}); require(n["schema"].as<std::string>()=="experimental-inventory-view/1"); }
    else { fields(n,{"schema","challenge","control_epoch","capability","semantics",type!='O'?"view":"panel"});
        require(n["schema"].as<std::string>()==(type=='K'?"experimental-inventory-ground-context/1":type=='J'?"experimental-inventory-close-context/1":type=='V'?"experimental-tactical-context/1":"experimental-stance-context/1"));
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
    const std::string suffix=(kind=='C'||kind=='G')?"":kind=='B'?"o0":kind=='D'?"sk":kind=='N'?"n0":"160080";
    return std::string(1,kind)+epoch+grant+id+suffix;
}
void command() { commandId=randomToken(); commandPacket=packet('G',commandId,capability); sendPacket(commandPacket); }
void verifyPreserved()
{
    require(inventory&&owner->isState(inventory)&&destroyed==0&&closes==0&&constructed==1);
    require(inits==baselineInits&&arranged==baselineArranged&&groundClicks==baselineClicks&&InventoryGroundGateAccess::page(inventory)==baselinePage);
    require(InventoryGroundGateAccess::surfaces(inventory)==baselineSurfaces);
    compare(before,snapshot());
}
Snapshot invariantSnapshot(const Snapshot &s)
{
    Snapshot out; out.rng=s.rng; out.save=YAML::Clone(s.save); out.runtime=YAML::Clone(s.runtime);
    // Only ordinary ground coordinates and presentation page/stack cache may change.
    for(auto i:out.save["items"]) if(i["inventoryslot"]&&i["inventoryslot"].as<std::string>()=="STR_GROUND") { i.remove("inventoryX"); i.remove("inventoryY"); }
    out.runtime.remove("inventoryPage"); out.runtime.remove("stacks"); return out;
}
void verifyDispatched()
{
    require(inventory==originalInventory&&owner->isState(inventory)&&constructed==1&&destroyed==0&&closes==0);
    require(inits==baselineInits&&groundClicks==baselineClicks+1&&arranged==baselineArranged+1);
    require(!InventoryGroundGateAccess::modal(inventory)&&!InventoryGroundGateAccess::widget(inventory)->getSelectedItem());
    require(InventoryGroundGateAccess::surfaces(inventory)==baselineSurfaces&&player->getTimeUnits()==initialTU);
    compare(invariantSnapshot(before),invariantSnapshot(snapshot()));
    const int f=fixture();
    if(f==3||f==5) require(InventoryGroundGateAccess::page(inventory)==20);
    else require(InventoryGroundGateAccess::page(inventory)==0);
    if(f==2) require(fixtureItem->getSlotX()==0&&fixtureItem->getSlotY()==0&&fixtureItem->getOwner()==0&&fixtureItem->getTile()==player->getTile());
    if(f==6) { auto ground=player->getTile()->getInventory(); require(ground->size()==14); require(InventoryGroundGateAccess::stack(inventory,(*ground)[0]->getSlotX(),(*ground)[0]->getSlotY())==12); for(size_t i=1;i<12;++i) require((*ground)[i]->getSlotX()==(*ground)[0]->getSlotX()&&(*ground)[i]->getSlotY()==(*ground)[0]->getSlotY()); require((*ground)[12]->getSlotX()!=(*ground)[13]->getSlotX()||(*ground)[12]->getSlotY()!=(*ground)[13]->getSlotY()); }
    if(f==9) { auto ground=player->getTile()->getInventory(); require(ground->size()==2); require((*ground)[0]->getAmmoItem()&&(*ground)[1]->getAmmoItem()); require((*ground)[0]->getAmmoItem()->getAmmoQuantity()==3&&(*ground)[1]->getAmmoItem()->getAmmoQuantity()==4); require((*ground)[0]->getAmmoItem()->getOwner()==0&&(*ground)[1]->getAmmoItem()->getOwner()==0); }
}
void rememberInventory()
{
    require(inventory&&owner->isState(inventory)&&!InventoryGroundGateAccess::widget(inventory)->getSelectedItem());
    before=snapshot(); beforePixels=lastPixels; baselineInits=inits; baselineArranged=arranged; baselineClicks=groundClicks;
    baselinePage=InventoryGroundGateAccess::page(inventory); baselineSurfaces=InventoryGroundGateAccess::surfaces(inventory);
    baselineAnimation=InventoryGroundGateAccess::animation(inventory); initialTU=player->getTimeUnits(); originalInventory=inventory;
}
void approved() { key(SDLK_y); stage=WAIT_DISPATCH; groundSent=true; }
void retainFrame(const std::vector<unsigned char> &pixels,int phase,int tick,const char *kind)
{
    require(pixels.size()==320*200*3&&phase>=1&&phase<=8);
    require(inventory&&owner->isState(inventory)&&InventoryGroundGateAccess::initialized(owner)&&InventoryGroundGateAccess::deleted(owner)==0);
    const std::string file="inventory-ground-frame-"+std::to_string(world)+"-"+kind+"-"+std::to_string(tick)+"-"+std::to_string(phase)+".ppm";
    std::ofstream out(Options::getUserFolder()+file,std::ios::binary); require(out.good());
    out << "P6\n320 200\n255\n"; out.write(reinterpret_cast<const char *>(pixels.data()),pixels.size()); out.close(); require(out.good());
    std::cerr << "INVENTORY_GROUND_FRAME_EVIDENCE world-" << world << " kind-" << kind << " tick-" << tick << " inventory-phase-" << phase << " same-inventory-1 file-" << file << std::endl;
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
        std::cerr << "INVENTORY_GROUND_PIXEL_DIAGNOSTIC phase-" << lastViewPhase << " closest-count-" << best << " bbox-" << left << "," << top << "," << right << "," << bottom << " physical-mouse-" << mx << "," << my << std::endl;
    }
    require(expected.count(lastPixels)==1);
}
void afterView()
{
    verifyDispatched();
    if(core()) {
        if(manual()) { manualGround=snapshot(); manualPhasePixels.clear(); manualPhasePixels[lastViewPhase].insert(lastPixels); manualTicks=0; manualLastPhase=lastViewPhase; retainFrame(lastPixels,lastViewPhase,0,"manual"); stage=COLLECT_MANUAL; }
        else { compare(manualGround,snapshot()); require(manualPhasePixels.size()==8&&manualPhasePixels.count(lastViewPhase)); retainFrame(lastPixels,lastViewPhase,0,"command"); comparePixels(manualPhasePixels.at(lastViewPhase)); ++parityCount; finishWorld(); }
    } else if(profile=="ledger") { opened=snapshot(); replay=0; stage=REPLAY_SEND; }
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
bool active() { const char *s=std::getenv("OPENXCOM_INVENTORY_GROUND_CONTROL_GATE"); return s&&std::string(s)=="1"; }
void prepare()
{
    enabled=active(); if(!enabled) return; const char *p=std::getenv("OPENXCOM_INVENTORY_GROUND_PROFILE"); profile=p?p:"";
    const std::set<std::string> profiles={"core","cancel-page","cancel-layout","overlay-input","input-x2","input-wheel","input-hotkey","input-outside","input-modifier","held-item","stale-input","stale-actor","stale-reinit","focus","display","overlay","lifetime","reopen","save","parent","unseen","held-y","held-mouse","timeout","capability-timeout","disconnect","ledger","ledger-limit","disabled","close-disabled","nontu","debug","palette","capless-I","cross-kind","hidden-button","moved-button","hidden-after-context","held-after-context","held-confirm","duplicate-pending","failed-I"}; require(profiles.count(profile)==1);
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
    if(!enabled) return false; owner=game; quiet(); YAML::Node hello=receive(); fields(hello,{"schema","control_epoch"}); require(hello["schema"].as<std::string>()=="experimental-stance/2"); epoch=hello["control_epoch"].as<std::string>(); require(token(epoch)&&!available());
    running=true; started=Clock::now(); replacement(); Options::baseXResolution=320; Options::baseYResolution=200; game->getScreen()->resetDisplay(false); game->getCursor()->setVisible(true); game->getFpsCounter()->setVisible(false); return true;
}
void afterCleanup(Game *game)
{
    if(!running||!pendingSetup) return; pendingSetup=false;
    if(terminalReplaced&&profile=="parent") { state=new BattlescapeState; game->getSavedGame()->getSavedBattle()->setBattleState(state); game->setState(state); inventory=0; moved=false; frames=0; stage=WAIT_TERMINAL_OPEN; return; }
    if(!terminalReplaced) { inits=groundClicks=closes=destroyed=constructed=displayed=shown=dismissed=arranged=frames=inventoryFrames=modalFrames=0;
        moved=prepared=retry=groundSent=lostOriginal=false; inventory=originalInventory=0; fixtureItem=0; inputs.clear(); stage=WARM; worldStarted=Clock::now(); }
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
    initialTU=fixture()==7?0:40; player->setTimeUnits(initialTU);
    RuleInventory *ground=mod->getInventory("STR_GROUND",true); require(ground->getX()==0&&(320-ground->getX())/16==20&&(200-ground->getY())/16==3);
    auto addGround=[&](BattleItem *i) { player->getTile()->addItem(i,ground); i->setSlotX(200+int(player->getTile()->getInventory()->size())); i->setSlotY(1); return i; };
    const int f=fixture();
    if(f==1||f==7) fixtureItem=addGround(item("STR_MEDI_KIT",0,"STR_GROUND"));
    if(f==2) fixtureItem=item("STR_MEDI_KIT",player,"STR_RIGHT_HAND");
    if(f==3||f==4||f==5) for(int i=0;i<(f==5?20:25);++i) { BattleItem *med=addGround(item("STR_MEDI_KIT",0,"STR_GROUND")); require(med->getRules()->getInventoryWidth()==1&&med->getRules()->getInventoryHeight()==2); med->setPainKillerQuantity(i%11); med->setHealQuantity(i/11); }
    if(f==6) { for(int i=0;i<12;++i) addGround(item(master=="xcom1"?"STR_RIFLE_CLIP":"STR_HARPOON_POD",0,"STR_GROUND")); for(int i=0;i<2;++i) { BattleItem *med=addGround(item("STR_MEDI_KIT",0,"STR_GROUND")); med->setPainKillerQuantity(i); } }
    if(f==8) { BattleItem *grenade=addGround(item(master=="xcom1"?"STR_GRENADE":"STR_MAGNA_BLAST_GRENADE",0,"STR_GROUND")); grenade->setFuseTimer(0); BattleItem *belt=item(master=="xcom1"?"STR_GRENADE":"STR_MAGNA_BLAST_GRENADE",player,"STR_BELT"); belt->setFuseTimer(0); }
    if(f==9) for(int i=0;i<2;++i) { BattleItem *gun=addGround(item(master=="xcom1"?"STR_RIFLE":"STR_JET_HARPOON",0,"STR_GROUND")); BattleItem *ammo=item(master=="xcom1"?"STR_RIFLE_CLIP":"STR_HARPOON_POD",0,"STR_RIGHT_HAND"); ammo->setAmmoQuantity(3+i); require(gun->setAmmoItem(ammo)==0); }
    battle->getTileEngine()->calculateSunShading(); battle->getTileEngine()->calculateTerrainLighting(); battle->getTileEngine()->calculateUnitLighting(); battle->getTileEngine()->recalculateFOV();
    if(profile=="debug") battle->setDebugMode();
    state=new BattlescapeState; battle->setBattleState(state); game->setState(state);
    if(terminalReplaced) { inventory=0; moved=false; frames=0; stage=WAIT_TERMINAL_OPEN; }
}
void beforeEvents(Game *game)
{
    if(!running||pendingSetup) return;
    require(Clock::now()-started<std::chrono::seconds(core()?240:(profile=="timeout"||profile=="capability-timeout")?155:45));
    require(Clock::now()-worldStarted<std::chrono::seconds((profile=="timeout"||profile=="capability-timeout")?145:35)); quiet();
    if(!moved) { motion(); moved=true; return; }
    if(stage==PREPARE_INVENTORY) {
        if(fixture()==4) { click(304,144); stage=WAIT_PREPARED; }
        else if(fixture()==2) { RuleInventory *hand=game->getMod()->getInventory("STR_RIGHT_HAND",true); click(hand->getX()+8,hand->getY()+8); stage=WAIT_PREPARED; }
        else { rememberInventory(); request('I'); stage=WAIT_I; }
    } else if(stage==WAIT_PREPARED&&inputs.empty()) {
        if(profile=="held-item") { require(InventoryGroundGateAccess::widget(inventory)->getSelectedItem()==fixtureItem); before=snapshot(); baselineArranged=arranged; baselineClicks=groundClicks; request('I'); stage=WAIT_HELD_READ; }
        else if(fixture()==2&&!prepared) { require(InventoryGroundGateAccess::widget(inventory)->getSelectedItem()==fixtureItem); RuleInventory *ground=game->getMod()->getInventory("STR_GROUND",true); click(80+fixtureItem->getRules()->getInventoryWidth()*8,ground->getY()+fixtureItem->getRules()->getInventoryHeight()*8); prepared=true; }
        else { require(!InventoryGroundGateAccess::widget(inventory)->getSelectedItem()); if(fixture()==4) require(InventoryGroundGateAccess::page(inventory)==20); if(fixture()==2) require(fixtureItem->getSlotX()==5); motion(); rememberInventory(); request('I'); stage=WAIT_I; }
    } else if(stage==WAIT_HELD_REFUSAL&&available()) {
        unavailable(receive()); compare(before,snapshot()); require(InventoryGroundGateAccess::widget(inventory)->getSelectedItem()==fixtureItem&&groundClicks==baselineClicks&&arranged==baselineArranged&&closes==0); finish();
    } else if(stage==SEND_COMMAND) {
        if(profile=="stale-input") motion(151,81);
        if(profile=="stale-actor") { game->getSavedGame()->getSavedBattle()->setSelectedUnit(twin); game->getSavedGame()->getSavedBattle()->setSelectedUnit(player); }
        if(profile=="stale-reinit") inventory->init();
        if(profile=="capability-timeout") SDL_Delay(120100);
        if(profile=="hidden-after-context") InventoryGroundGateAccess::groundButton(inventory)->setVisible(false);
        if(profile=="held-after-context") InventoryGroundGateAccess::widget(inventory)->setSelectedItem(fixtureItem);
        command(); stage=WAIT_PENDING;
    } else if(stage==WAIT_PENDING&&available()) {
        if(stale()||profile=="capless-I") { status(receive(),"unavailable"); require(groundClicks==baselineClicks&&closes==0&&destroyed==0); finish(); return; }
        status(receive(),"pending_confirmation"); require(game->isState(inventory)&&shown==(cancelCase()&&retry?2:1)&&groundClicks==baselineClicks&&arranged==baselineArranged&&closes==0&&destroyed==0&&inits==baselineInits);
        if(profile=="unseen") { require(shown==1); stage=WAIT_INPUT; }
        else if(profile=="duplicate-pending") { sendPacket(commandPacket); stage=WAIT_DUPLICATE_PENDING; }
        else stage=CONFIRM;
    } else if(stage==WAIT_DUPLICATE_PENDING&&available()) { status(receive(),"pending_confirmation"); require(shown==1&&groundClicks==baselineClicks&&arranged==baselineArranged); stage=CONFIRM; }
    else if(stage==CONFIRM&&modalFrames>0) {
        require(game->isState(inventory)&&InventoryGroundGateAccess::modal(inventory));
        if(inputCase()&&!retry) {
            if(profile=="overlay-input") { if(Clock::now()-modalStarted<std::chrono::milliseconds(400)) return; require(InventoryGroundGateAccess::animation(inventory)!=baselineAnimation); click(152,80,SDL_BUTTON_X1); }
            else if(profile=="input-x2") click(152,80,SDL_BUTTON_X2);
            else if(profile=="input-wheel") click(152,80,SDL_BUTTON_WHEELUP);
            else if(profile=="input-hotkey") key(Options::keyBattleInventory);
            else if(profile=="input-outside") click(8,8);
            else { SDL_Event e={}; e.type=SDL_KEYDOWN; e.key.which=255; e.key.state=SDL_PRESSED; e.key.keysym.sym=SDLK_y; e.key.keysym.mod=SDLMod(KMOD_SHIFT); queue(e); keyEdge(SDLK_y,false); }
            stage=WAIT_INPUT;
        } else if(cancelCase()&&!retry) { click(280,128); stage=WAIT_INPUT; }
        else if(profile=="held-y") { keyEdge(SDLK_y,true); stage=WAIT_INPUT; }
        else if(profile=="held-mouse") { mouseEdge(280,112,false); stage=WAIT_INPUT; }
        else if(profile=="held-confirm") { InventoryGroundGateAccess::widget(inventory)->setSelectedItem(fixtureItem); before=snapshot(); key(SDLK_y); stage=WAIT_INPUT; }
        else if(profile=="focus") { focus(false); stage=WAIT_INPUT; }
        else if(profile=="display") { game->getScreen()->resetDisplay(false); stage=WAIT_INPUT; }
        else if(profile=="overlay") { game->pushState(new State); stage=WAIT_INPUT; }
        else if(profile=="timeout") { SDL_Delay(120100); key(SDLK_y); stage=WAIT_INPUT; }
        else if(profile=="disconnect") { close(peer); peer=-1; stage=WAIT_RETIRE; }
        else if(profile=="palette") { SDL_Color palette[256]; std::copy(game->getScreen()->getPalette(),game->getScreen()->getPalette()+256,palette); game->getScreen()->resetDisplay(false); game->getScreen()->setPalette(palette,0,1,false); stage=WAIT_INPUT; }
        else if(core()&&world%4==3) { click(280,112); stage=WAIT_DISPATCH; groundSent=true; }
        else approved();
    } else if(stage==WAIT_INPUT&&inputs.empty()) {
        require(groundClicks==baselineClicks&&arranged==baselineArranged&&closes==0); sendStatusQuery(); stage=WAIT_CANCEL_STATUS;
    } else if(stage==WAIT_CANCEL_STATUS&&available()) {
        status(receive(),"unavailable"); require(groundClicks==baselineClicks&&arranged==baselineArranged&&closes==0);
        if(cancelCase()) { verifyPreserved(); require(!InventoryGroundGateAccess::modal(inventory)&&dismissed==1); retry=true; motion(); request('K'); stage=WAIT_CONTEXT; }
        else {
            if(profile=="focus") focus(true);
            if(profile=="overlay") {
                // Controller cancellation already happened under the external
                // State. Check it before the fixture's own ordinary pop.
                require(inventory==originalInventory&&constructed==1&&destroyed==0&&dismissed==1);
                require(inits==baselineInits&&arranged==baselineArranged&&groundClicks==baselineClicks&&closes==0);
                require(!InventoryGroundGateAccess::modal(inventory)&&InventoryGroundGateAccess::surfaces(inventory)==baselineSurfaces);
                Snapshot canceled=snapshot();
                canceled.runtime["inventoryPage"]=InventoryGroundGateAccess::page(inventory);
                BattleItem *held=InventoryGroundGateAccess::widget(inventory)->getSelectedItem();
                canceled.runtime["held"]=held?held->getId():-1;
                canceled.runtime["stacks"]=InventoryGroundGateAccess::stacks(inventory);
                compare(before,canceled);
                game->popState(); // Normal restoration must re-init next loop.
            }
            if(profile=="held-y") keyEdge(SDLK_y,false); stage=WAIT_RETIRE;
        }
    } else if(stage==WAIT_RETIRE&&inputs.empty()) {
        if(inventory&&game->isState(inventory)&&!InventoryGroundGateAccess::modal(inventory)) {
            require(groundClicks==baselineClicks&&closes==0&&destroyed==0&&inventory==originalInventory&&constructed==1);
            if(profile=="overlay") {
                // This one init/arrange belongs to Game::popState restoration,
                // not the controller. The canonical one-item world is unchanged.
                require(inits==baselineInits+1&&arranged==baselineArranged+1);
                compare(before,snapshot());
            } else {
                require(arranged==baselineArranged&&inits==baselineInits);
                if(profile!="palette") compare(before,snapshot());
            }
            finish();
        }
    } else if(stage==BARRIER_REPLY&&available()) {
        unavailable(receive()); verifyDispatched();
        if(++barrier<4) { const char kinds[]={'O','V','J','K'}; request(kinds[barrier]); }
        else if(terminal()) beginTerminal();
        else if(profile=="failed-I") { InventoryGroundGateAccess::widget(inventory)->setVisible(false); request('I'); stage=WAIT_FAILED_I; }
        else { request('I'); stage=WAIT_POST_I; }
    } else if(stage==WAIT_FAILED_I&&available()) { unavailable(receive()); InventoryGroundGateAccess::widget(inventory)->setVisible(true); request('K'); stage=WAIT_FAILED_K; }
    else if(stage==WAIT_FAILED_K&&available()) { unavailable(receive()); verifyDispatched(); request('I'); stage=WAIT_POST_I; }
    else if(stage==CROSS_REPLY&&available()) {
        status(receive(),"unavailable"); verifyPreserved();
        if(++replay==6) { request('J'); stage=WAIT_REARM; }
        else { const char kinds[]={'B','D','N','R','L','C'}; commandId=randomToken(); sendPacket(packet(kinds[replay],commandId,capability)); }
    } else if(stage==WAIT_STATUS&&available()) { status(receive(),"dispatched"); afterView(); }
    else if(stage==REPLAY_SEND) {
        commandId=commandPacket.substr(65,32);
        if(replay==0) sendPacket(commandPacket);
        else if(replay==1) sendStatusQuery();
        else if(replay==2) { std::string conflict=commandPacket; conflict.replace(33,32,randomToken()); sendPacket(conflict); }
        else if(replay<9) { const char kinds[]={'B','D','N','R','L','C'}; sendPacket(packet(kinds[replay-3],commandId,capability)); }
        else if(replay==9) { commandId=randomToken(); std::string consumed=commandPacket; consumed.replace(65,32,commandId); sendPacket(consumed); }
        else { request('K'); stage=WAIT_REARM; return; }
        stage=REPLAY_REPLY;
    } else if(stage==REPLAY_REPLY&&available()) {
        if(replay==100) { status(receive(),"unavailable"); compare(before,snapshot()); stage=LIMIT_SEND; return; }
        status(receive(),replay<2?"dispatched":"unavailable"); compare(opened,snapshot()); verifyDispatched(); require(shown==1); ++replay; stage=REPLAY_SEND;
    } else if(stage==LIMIT_SEND) {
        const char kinds[]={'G','C','B','D','N','R','L'}; commandId=randomToken(); std::string p=packet(kinds[limits%7],commandId,randomToken()); if(limits==0) firstPacket=p; sendPacket(p); stage=limits==64?LIMIT_FINAL:LIMIT_REPLY;
    } else if(stage==LIMIT_REPLY&&available()) {
        status(receive(),"unavailable"); compare(before,snapshot()); require(groundClicks==0&&arranged==0&&closes==0); ++limits;
        if(limits==64) { commandId=firstPacket.substr(65,32); sendPacket(firstPacket); replay=100; stage=REPLAY_REPLY; } else stage=LIMIT_SEND;
    } else if(stage==LIMIT_FINAL&&available()) { unavailable(receive()); char b; require(recv(peer,&b,1,MSG_DONTWAIT)==0&&commandIds.size()==65&&groundClicks==0); finish(); }
    else if(stage==WAIT_TERMINAL_CLOSE&&destroyed==1&&inputs.empty()&&idle()) {
        require(closes==1&&groundClicks==baselineClicks+1); if(profile=="reopen") { key(Options::keyBattleInventory); stage=WAIT_TERMINAL_OPEN; } else { barrier=0; stage=TERMINAL_REQUEST; }
    } else if(stage==WAIT_TERMINAL_OPEN&&inputs.empty()) {
        if(inventory&&game->isState(inventory)&&InventoryGroundGateAccess::initialized(game)) { require(constructed==2&&destroyed==1); barrier=0; stage=TERMINAL_REQUEST; }
        else if(idle()&&++frames>=2) key(Options::keyBattleInventory);
    } else if(stage==TERMINAL_REQUEST) { const char kinds[]={'O','V','I','J','K'}; request(kinds[barrier]); stage=TERMINAL_REPLY; }
    else if(stage==TERMINAL_REPLY&&available()) { unavailable(receive()); if(++barrier==5) { require(groundClicks==baselineClicks+1&&destroyed==1); finish(); } else stage=TERMINAL_REQUEST; }
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
        verifyDispatched(); dispatchedFrame=frameSerial; baselineAnimation=InventoryGroundGateAccess::animation(inventory); motion(); stage=WAIT_SETTLE;
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
    if(inventory&&game->isState(inventory)&&InventoryGroundGateAccess::modal(inventory)) { ++modalFrames; require(display()!=beforePixels); }
    if(stage==WAIT_INVENTORY&&inventory&&game->isState(inventory)&&inputs.empty()&&++inventoryFrames>=2) {
        require(inits==1&&constructed==1&&groundClicks==0&&closes==0&&destroyed==0&&displayed>0); Surface *button=InventoryGroundGateAccess::groundButton(inventory);
        require(button->getVisible()&&button->getX()==289&&button->getY()==137&&button->getWidth()==32&&button->getHeight()==15); stage=PREPARE_INVENTORY;
    }
    if(stage==COLLECT_MANUAL&&inventory&&game->isState(inventory)) {
        const int phase=InventoryGroundGateAccess::animation(inventory); require(phase>=1&&phase<=8);
        if(phase!=manualLastPhase) { require(phase==manualLastPhase%8+1); manualLastPhase=phase; ++manualTicks; const auto pixels=display(); manualPhasePixels[phase].insert(pixels); retainFrame(pixels,phase,manualTicks,"manual"); }
        // Collect every naturally rendered primer phase; no timer resets, redraw
        // helpers, forced tick advancement, cropped image, or pixel masking.
        if(manualPhasePixels.size()==8&&manualTicks>=7) { compare(manualGround,snapshot()); ++world; replacement(); return; }
    }
    if(stage==WAIT_SETTLE&&frameSerial>dispatchedFrame&&inputs.empty()&&InventoryGroundGateAccess::animation(inventory)!=baselineAnimation) {
        verifyDispatched();
        if(manual()) { request('I'); stage=WAIT_POST_I; }
        else { barrier=0; request('O'); stage=BARRIER_REPLY; }
    }
    if(stage==WAIT_I&&available()) {
        if(profile=="nontu"||profile=="debug") { unavailable(receive()); verifyPreserved(); request('K'); stage=WAIT_CONTEXT; return; }
        context(receive(),'I'); verifyPreserved(); beforePixels=lastPixels; baselineAnimation=InventoryGroundGateAccess::animation(inventory);
        if(manual()) { click(304,144); stage=WAIT_DISPATCH; }
        else {
            if(profile=="hidden-button") InventoryGroundGateAccess::groundButton(inventory)->setVisible(false);
            if(profile=="moved-button") InventoryGroundGateAccess::groundButton(inventory)->setX(288);
            if(profile=="held-y") keyEdge(SDLK_y,true);
            if(profile=="held-mouse") { motion(280,112); mouseEdge(280,112,true); }
            request('K'); stage=WAIT_CONTEXT;
        }
    } else if(stage==WAIT_HELD_READ&&available()) { context(receive(),'I'); compare(before,snapshot()); request('K'); stage=WAIT_HELD_REFUSAL; }
    else if(stage==WAIT_CONTEXT&&available()) {
        if(profile=="disabled"||profile=="close-disabled"||profile=="nontu"||profile=="debug"||profile=="hidden-button"||profile=="moved-button") { unavailable(receive()); require(groundClicks==baselineClicks&&closes==0); finish(); return; }
        if(profile=="held-mouse") { YAML::Node n=receive(); if(n["error"]) { unavailable(n); verifyPreserved(); mouseEdge(280,112,false); stage=WAIT_RETIRE; return; } context(n,'K'); stage=SEND_COMMAND; return; }
        context(receive(),'K'); verifyPreserved(); require(!available()&&owner->isState(inventory));
        if(profile=="capless-I") { request('I'); stage=WAIT_CAPLESS; }
        else if(profile=="cross-kind"&&replay<6) { replay=0; commandId=randomToken(); sendPacket(packet('B',commandId,capability)); stage=CROSS_REPLY; }
        else stage=SEND_COMMAND;
    } else if(stage==WAIT_CAPLESS&&available()) { context(receive(),'I'); verifyPreserved(); stage=SEND_COMMAND; }
    else if(stage==WAIT_REARM&&available()) {
        if(profile=="ledger") { context(receive(),'K'); compare(opened,snapshot()); verifyDispatched(); require(shown==1); finish(); }
        else if(replay==6) { context(receive(),'J'); commandId=randomToken(); sendPacket(packet('G',commandId,capability)); replay=7; stage=WAIT_REARM; }
        else { status(receive(),"unavailable"); verifyPreserved(); request('K'); stage=WAIT_CONTEXT; replay=8; }
    } else if(stage==WAIT_POST_I&&available()) {
        context(receive(),'I'); require(frameSerial>dispatchedFrame); verifyDispatched();
        if(groundSent) { sendStatusQuery(); stage=WAIT_STATUS; } else afterView();
    }
}
void inventoryConstructed(InventoryState *who) { if(running&&!pendingSetup) { require(!inventory); inventory=who; ++constructed; } }
void inventoryInitialized(InventoryState *who) { if(!running||pendingSetup) return; require(!inventory||inventory==who); inventory=who; ++inits; }
void inventoryDisplayed(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++displayed; }
void inventoryGroundEntered(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) { ++groundClicks; require(groundClicks<=baselineClicks+1||stage==WAIT_PREPARED); } }
void inventoryCloseEntered(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++closes; }
void inventoryDestroyed(InventoryState *who) { if(running&&(!pendingSetup||terminalReplaced)&&who==inventory) { ++destroyed; inventory=0; require(lostOriginal); } }
void groundOverlayShown(InventoryState *who)
{
    if(running&&!pendingSetup&&who==inventory) { ++shown; modalStarted=Clock::now(); if(profile=="unseen") { require(shown==1&&modalFrames==0&&groundClicks==baselineClicks); key(SDLK_y); } }
}
void groundOverlayDismissed(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++dismissed; }
void inventoryGroundArranged(Inventory *) { if(running&&!pendingSetup) ++arranged; }
void kneelClicked(BattlescapeState *) { if(running&&!pendingSetup) require(false); }
void selectionClicked(BattlescapeState *,bool) { if(running&&!pendingSetup) require(false); }
int exitStatus()
{
    if(enabled&&completed) {
        if(core()) require(world==PairWorlds&&parityCount==PairCount&&oracleCount>=PairWorlds*2);
        std::cout << "INVENTORY_GROUND_CONTROL_GATE PASS {\"schema\":\"inventory-ground-control-gate/1\",\"master\":\"" << master << "\",\"profile\":\"" << profile << "\",\"passed\":true}" << std::endl;
    }
    if(peer>=0) { close(peer); peer=-1; } return enabled&&!completed?EXIT_FAILURE:EXIT_SUCCESS;
}
} }
#undef require
#undef fields
