// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
// A finite, default-OFF native inventory OK/close and pixels-only read fixture.
// Normal events, initialization, timers, blits and flips remain native.
// Oracles remain private; test-only frame evidence is retained in the disposable
// user directory and copied by the private runner, never sent on the public wire.
#include "InventoryCloseControlGate.h"
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
// Test-only access, independently checked against ordinary handler side effects.
// No oracle or inspection method is added to the production controller.
struct InventoryCloseGateAccess {
    static int phase(Map *m) { return m->_animFrame; }
    static Inventory *widget(InventoryState *s) { return s->_inv; }
    static int page(InventoryState *s) { return s->_inv->_groundOffset; }
    static int animation(InventoryState *s) { return s->_inv->_animFrame; }
    static bool modal(InventoryState *s) { return s->_modal != 0; }
    static size_t surfaces(InventoryState *s) { return s->_surfaces.size(); }
    static bool initialized(Game *g) { return g->_init; }
    static size_t deleted(Game *g) { return g->_deleted.size(); }
};
namespace InventoryCloseControlGate { namespace {
using Clock = std::chrono::steady_clock;
enum { PairCount = 14, PairWorlds = PairCount * 2 };
enum Stage { WARM, WAIT_INVENTORY, PREPARE_INVENTORY, WAIT_PREPARED, WAIT_I,
    WAIT_HELD_READ, WAIT_HELD_REFUSAL, WAIT_HELD_RETURN, WAIT_UNLOAD, WAIT_UNLOAD_I, WAIT_RELOAD_PICK, WAIT_RELOAD_HELD_J, WAIT_RELOAD_DONE, WAIT_CONTEXT, SEND_COMMAND,
    WAIT_PENDING, CONFIRM, WAIT_INPUT, CANCEL_STATUS, WAIT_CANCEL_STATUS,
    WAIT_RETIRE, WAIT_RECOVER, WAIT_CLOSE, WAIT_MANUAL_FRAME, WAIT_CAPLESS, CROSS_REPLY, WAIT_GRAVITY_SETTLE, COLLECT_MANUAL, BARRIER_REPLY, WAIT_V, WAIT_STATUS,
    REPLAY_SEND, REPLAY_REPLY, WAIT_POST_REPLAY_O, LIMIT_SEND, LIMIT_REPLY, LIMIT_FINAL,
    TERMINAL_REQUEST, TERMINAL_REPLY, FINISHED };
bool enabled=false, running=false, completed=false, pendingSetup=false, moved=false;
bool prepared=false, retry=false, terminalReplaced=false, closeSent=false, observed=false;
int peer=-1, world=0, frames=0, inventoryFrames=0, modalFrames=0, inits=0, closes=0;
int destroyed=0, shown=0, dismissed=0, arranged=0, baselineInits=0, baselineArranged=0;
int baselinePage=0, baselineAnimation=0, replay=0, barrier=0, limits=0, oracleCount=0, parityCount=0;
int frameSerial=0, shownFrame=0, closedFrame=0, initialTU=40, preservedShade=0, lastViewPhase=0;
size_t baselineSurfaces=0;
Stage stage=WARM;
Game *owner=0; BattlescapeState *state=0; InventoryState *inventory=0;
BattleUnit *player=0, *twin=0; BattleItem *fixtureItem=0, *manualAmmo=0; int initialAmmo=0;
Position playerPosition(20,20,0), twinPosition(30,30,0);
Clock::time_point started, worldStarted, requestStarted, modalStarted;
std::string master, profile, epoch, capability, challenge, commandId, commandPacket, firstPacket;
std::set<std::string> tokens, commandIds;
std::deque<SDL_Event> inputs;
std::vector<unsigned char> lastPixels, beforePixels;
std::map<int,std::set<std::vector<unsigned char>>> manualPhasePixels;
int manualTicks=0, manualLastPhase=0;
struct Snapshot { YAML::Node save, runtime; uint64_t rng; };
Snapshot before, opened, manualClosed;
void check(bool ok,int line) { if(!ok) {
    std::cerr << "INVENTORY_CLOSE_FIXTURE_DIAGNOSTIC world-" << world << " phase-" << int(stage)
        << " inits-" << inits << " closes-" << closes << " destroyed-" << destroyed
        << " shown-" << shown << " dismissed-" << dismissed << std::endl;
    std::cerr << "INVENTORY_CLOSE_READINESS_DIAGNOSTIC focus-" << int(SDL_GetAppState());
    if(owner&&state&&owner->isState(state)) std::cerr << " top-1 panic-" << state->getBattleGame()->getPanicHandled() << " busy-" << state->getBattleGame()->isBusy() << " queued-" << state->getBattleGame()->getStates().size() << " actor-status-" << int(player->getStatus()) << " cursor-" << int(state->getMap()->getCursorType());
    else std::cerr << " top-0";
    std::cerr << std::endl;
    std::cerr << "INVENTORY_CLOSE_CONTROL_GATE FAIL assertion-" << line << std::endl;
    std::_Exit(EXIT_FAILURE);
} }
#define require(ok) check((ok),__LINE__)
bool core() { return profile=="core"; }
bool manual() { return core() && world%2==0; }
bool terminal() { return profile=="lifetime" || profile=="save" || profile=="parent"; }
bool inputCase() { return profile=="overlay-input"||profile=="input-x2"||profile=="input-wheel"||profile=="input-hotkey"||profile=="input-outside"||profile=="input-modifier"; }
bool cancelCase() { return profile=="cancel-page"||profile=="cancel-layout"||inputCase(); }
bool stale() { return profile=="stale-input"||profile=="stale-actor"||profile=="stale-reinit"; }
bool rejectedConsent() { return profile=="unseen"||profile=="held-y"||profile=="held-mouse"; }
int fixture() { if(core()) return (world/2)%7; if(profile=="cancel-page") return 4;
    if(profile=="cancel-layout"||profile=="held-item"||profile=="manual-reload") return 2; if(profile=="overlay-input") return 5; return 1; }
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
    if(inventory && owner->isState(inventory)) { n["inventoryPage"]=InventoryCloseGateAccess::page(inventory); BattleItem *held=InventoryCloseGateAccess::widget(inventory)->getSelectedItem(); n["held"]=held?held->getId():-1; }
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
    lastPixels=display(); lastViewPhase=state?InventoryCloseGateAccess::phase(state->getMap()):0; std::map<unsigned int,unsigned int> first;
    for(size_t i=0;i<indices.size();++i) { unsigned int rgb=(lastPixels[i*3]<<16)|(lastPixels[i*3+1]<<8)|lastPixels[i*3+2];
        if(!first.count(rgb)) { unsigned int next=first.size(); first[rgb]=next; } require(first.size()<=256&&indices[i]==first[rgb]);
        for(int c=0;c<3;++c) require(palette[indices[i]*3+c]==lastPixels[i*3+c]); }
    for(size_t i=first.size()*3;i<palette.size();++i) require(palette[i]==0); ++oracleCount;
}
void context(const YAML::Node &n,char type)
{
    if(type=='I') { fields(n,{"schema","challenge","semantics","view"}); require(n["schema"].as<std::string>()=="experimental-inventory-view/1"); }
    else { fields(n,{"schema","challenge","control_epoch","capability","semantics",type!='O'?"view":"panel"});
        require(n["schema"].as<std::string>()==(type=='J'?"experimental-inventory-close-context/1":type=='V'?"experimental-tactical-context/1":"experimental-stance-context/1"));
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
    const std::string suffix=kind=='C'?"":kind=='B'?"o0":kind=='D'?"sk":kind=='N'?"n0":"160080";
    return std::string(1,kind)+epoch+grant+id+suffix;
}
void command() { commandId=randomToken(); commandPacket=packet('C',commandId,capability); sendPacket(commandPacket); }
void closeManual() { motion(); key(Options::keyBattleInventory); stage=WAIT_CLOSE; }
void verifyPreserved()
{
    require(inventory&&owner->isState(inventory)&&destroyed==0&&closes==0);
    require(inits==baselineInits&&arranged==baselineArranged&&InventoryCloseGateAccess::page(inventory)==baselinePage);
    require(InventoryCloseGateAccess::surfaces(inventory)==baselineSurfaces);
    compare(before,snapshot());
}
void rememberInventory()
{
    require(inventory&&owner->isState(inventory)&&!InventoryCloseGateAccess::widget(inventory)->getSelectedItem());
    before=snapshot(); beforePixels=lastPixels; baselineInits=inits; baselineArranged=arranged;
    baselinePage=InventoryCloseGateAccess::page(inventory); baselineSurfaces=InventoryCloseGateAccess::surfaces(inventory);
    baselineAnimation=InventoryCloseGateAccess::animation(inventory);
}
void approved() { key(SDLK_y); stage=WAIT_CLOSE; closeSent=true; }
void retainFrame(const std::vector<unsigned char> &pixels,int phase,int tick,const char *kind)
{
    require(pixels.size()==320*200*3&&phase>=0&&phase<8);
    const bool ready=owner->isState(state)&&InventoryCloseGateAccess::initialized(owner)&&InventoryCloseGateAccess::deleted(owner)==0;
    require(ready);
    const std::string file="inventory-close-frame-"+std::to_string(world)+"-"+kind+"-"+std::to_string(tick)+"-"+std::to_string(phase)+".ppm";
    std::ofstream out(Options::getUserFolder()+file,std::ios::binary); require(out.good());
    out << "P6\n320 200\n255\n"; out.write(reinterpret_cast<const char *>(pixels.data()),pixels.size()); out.close(); require(out.good());
    std::cerr << "INVENTORY_CLOSE_FRAME_EVIDENCE world-" << world << " kind-" << kind << " tick-" << tick << " map-phase-" << phase << " parent-ready-1 file-" << file << std::endl;
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
        std::cerr << "INVENTORY_CLOSE_PIXEL_DIAGNOSTIC phase-" << lastViewPhase << " closest-count-" << best << " bbox-" << left << "," << top << "," << right << "," << bottom << " physical-mouse-" << mx << "," << my << std::endl;
    }
    require(expected.count(lastPixels)==1);
}
void afterView()
{
    require(destroyed==1&&closes==1&&owner->isState(state)&&InventoryCloseGateAccess::initialized(owner)&&InventoryCloseGateAccess::deleted(owner)==0);
    if(fixture()==6) {
        require(player->getTile()->getShade()<preservedShade);
        Tile *north=owner->getSavedGame()->getSavedBattle()->getTile(Position(30,15,0));
        require(north->isDiscovered(2)&&north->getVisible()>0);
    }
    if(profile=="manual-reload") {
        require(fixtureItem->getAmmoItem()==manualAmmo&&fixtureItem->getOwner()==player&&manualAmmo->getOwner()==0);
        require(manualAmmo->getAmmoQuantity()==initialAmmo&&player->getTimeUnits()==17);
    }
    if(core()) {
        if(manual()) { manualClosed=snapshot(); manualPhasePixels.clear(); manualPhasePixels[lastViewPhase].insert(lastPixels); manualTicks=0; manualLastPhase=lastViewPhase; retainFrame(lastPixels,lastViewPhase,0,"manual"); stage=COLLECT_MANUAL; }
        else { compare(manualClosed,snapshot()); require(manualPhasePixels.size()==8&&manualPhasePixels.count(lastViewPhase)); retainFrame(lastPixels,lastViewPhase,0,"command"); comparePixels(manualPhasePixels.at(lastViewPhase)); ++parityCount; finishWorld(); }
    } else if(profile=="ledger") { opened=snapshot(); replay=0; stage=REPLAY_SEND; }
    else finish();
}
} // namespace
bool active() { const char *s=std::getenv("OPENXCOM_INVENTORY_CLOSE_CONTROL_GATE"); return s&&std::string(s)=="1"; }
void prepare()
{
    enabled=active(); if(!enabled) return; const char *p=std::getenv("OPENXCOM_INVENTORY_CLOSE_PROFILE"); profile=p?p:"";
    const std::set<std::string> profiles={"core","cancel-page","cancel-layout","overlay-input","input-x2","input-wheel","input-hotkey","input-outside","input-modifier","held-item","stale-input","stale-actor","stale-reinit","focus","display","overlay","lifetime","save","parent","unseen","held-y","held-mouse","timeout","disconnect","ledger","ledger-limit","disabled","nontu","debug","gravity","palette","capless-I","cross-kind","manual-reload"}; require(profiles.count(profile)==1);
    require(!std::getenv("OPENXCOM_STANCE_CONTROL_GATE")&&!std::getenv("OPENXCOM_INVENTORY_OPEN_CONTROL_GATE"));
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
    if(terminalReplaced && profile=="parent") {
        state=new BattlescapeState; game->getSavedGame()->getSavedBattle()->setBattleState(state); game->setState(state); inventory=0; moved=false; stage=WAIT_RETIRE; return;
    }
    if(!terminalReplaced) { inits=closes=destroyed=shown=dismissed=arranged=frames=inventoryFrames=modalFrames=0;
        moved=prepared=retry=closeSent=observed=false; inventory=0; fixtureItem=0; inputs.clear(); stage=WARM; worldStarted=Clock::now(); }
    Options::FPS=60; Options::maximizeInfoScreens=core()&&world/2>=7;
    Options::battleXcomSpeed=30; Options::battleNewPreviewPath=PATH_NONE;
    RNG::setSeed(123456789); std::srand(12345); // Independently reconstructed setup only.
    SavedGame *save=new SavedGame; SavedBattleGame *battle=new SavedBattleGame; save->setBattleGame(battle); game->setSavedGame(save);
    playerPosition=Position(20,20,profile=="gravity"?1:0); twinPosition=Position(30,30,0);
    battle->initMap(40,40,profile=="gravity"?2:1); battle->setDepth(0); battle->setAmbientSound(-1); Mod *mod=game->getMod(); master=Options::getActiveMaster(); require((master=="xcom1"||master=="xcom2")&&Options::getActiveMods().size()==1);
    MapDataSet *data=mod->getMapDataSet("BLANKS"); require(data); data->loadData(mod->getMCDPatch(data->getName())); battle->getMapDataSets()->push_back(data); MapData *floor=data->getObject(1); require(floor&&floor->getObjectType()==O_FLOOR&&!floor->isNoFloor()&&floor->getTerrainLevel()==0&&floor->getTUCost(MT_WALK)==4);
    for(int i=0;i<battle->getMapSizeXYZ();++i) battle->getTiles()[i]->setMapData(floor,1,0,O_FLOOR);
    Base *base=new Base(mod); save->getBases()->push_back(base); RuleSoldier *rule=mod->getSoldier("STR_SOLDIER",true); Armor *armor=mod->getArmor(rule->getArmor(),true);
    Soldier *a=new Soldier(rule,armor,1); a->setName("INVENTORY FIXTURE"); a->getCurrentStats()->tu=60; a->getCurrentStats()->stamina=60;
    Soldier *b=new Soldier(rule,armor,2); YAML::Node clone=a->save(); clone["id"]=2; b->load(clone,mod,save); base->getSoldiers()->push_back(a); base->getSoldiers()->push_back(b);
    player=new BattleUnit(a,0); twin=new BattleUnit(b,0); player->setPosition(playerPosition); player->setDirection(0); twin->setPosition(twinPosition); twin->setDirection(4); twin->setTimeUnits(40);
    battle->getUnits()->push_back(player); battle->getUnits()->push_back(twin); battle->resetUnitTiles(); battle->setSelectedUnit(player); battle->initUtilities(mod);
    initialTU=fixture()==0?0:40; player->setTimeUnits(initialTU);
    if(fixture()==2) fixtureItem=arm(player);
    if(fixture()==3) {
        const std::string ammo=master=="xcom1"?"STR_RIFLE_CLIP":"STR_HARPOON_POD";
        require(mod->getItem(ammo,true)->getInventoryWidth()==1&&mod->getItem(ammo,true)->getInventoryHeight()==1);
        for(auto entry:*mod->getInventories()) { RuleInventory *slot=entry.second;
            if(slot->getType()==INV_SLOT) for(const RuleSlot &p:*slot->getSlots()) item(ammo,player,slot->getId().c_str(),p.x,p.y);
            else if(slot->getType()==INV_HAND) item(master=="xcom1"?"STR_RIFLE":"STR_JET_HARPOON",player,slot->getId().c_str()); }
    }
    if(fixture()==4) {
        RuleInventory *ground=mod->getInventory("STR_GROUND",true); require(ground->getX()==0&&(320-ground->getX())/16==20&&(200-ground->getY())/16==3);
        for(int i=0;i<25;++i) { BattleItem *med=item("STR_MEDI_KIT",0,"STR_GROUND"); require(med->getRules()->getInventoryWidth()==1&&med->getRules()->getInventoryHeight()==2); med->setPainKillerQuantity(i%11); med->setHealQuantity(i/11); player->getTile()->addItem(med,ground); med->setSlotX(200+i); med->setSlotY(1); }
    }
    if(fixture()==5) { BattleItem *grenade=item(master=="xcom1"?"STR_GRENADE":"STR_MAGNA_BLAST_GRENADE",player,"STR_BELT"); grenade->setFuseTimer(0); }
    if(fixture()==6) { fixtureItem=item(master=="xcom1"?"STR_ELECTRO_FLARE":"STR_CHEMICAL_FLARE",player,"STR_RIGHT_HAND"); battle->setGlobalShade(15); battle->getTileEngine()->togglePersonalLighting(); }
    if(profile=="gravity") { fixtureItem=item("STR_MEDI_KIT",0,"STR_GROUND"); player->getTile()->addItem(fixtureItem,mod->getInventory("STR_GROUND",true)); }
    battle->getTileEngine()->calculateSunShading(); battle->getTileEngine()->calculateTerrainLighting(); battle->getTileEngine()->calculateUnitLighting(); battle->getTileEngine()->recalculateFOV();
    if(profile=="debug") battle->setDebugMode();
    state=new BattlescapeState; battle->setBattleState(state); game->setState(state);
    if(terminalReplaced) { inventory=0; moved=false; stage=WAIT_RETIRE; }
}
void beforeEvents(Game *game)
{
    if(!running||pendingSetup) return;
    require(Clock::now()-started<std::chrono::seconds(core()?220:profile=="timeout"?150:40)); require(Clock::now()-worldStarted<std::chrono::seconds(profile=="timeout"?140:30)); quiet();
    if(!moved) { motion(); moved=true; return; }
    if(stage==PREPARE_INVENTORY) {
        if(profile=="cancel-page") { click(304,144); stage=WAIT_PREPARED; }
        else if(profile=="cancel-layout"||profile=="held-item"||profile=="manual-reload"||fixture()==6) {
            RuleInventory *hand=game->getMod()->getInventory("STR_RIGHT_HAND",true); click(hand->getX()+8,hand->getY()+8); stage=WAIT_PREPARED;
        } else { request('I'); stage=WAIT_I; }
    } else if(stage==WAIT_PREPARED&&inputs.empty()) {
        if(profile=="manual-reload") {
            require(InventoryCloseGateAccess::widget(inventory)->getSelectedItem()==fixtureItem);
            manualAmmo=fixtureItem->getAmmoItem(); require(manualAmmo&&manualAmmo->getOwner()==0&&player->getTimeUnits()==40);
            initialAmmo=manualAmmo->getAmmoQuantity(); click(304,44); stage=WAIT_UNLOAD;
        }
        else if(profile=="held-item") { require(InventoryCloseGateAccess::widget(inventory)->getSelectedItem()==fixtureItem); before=snapshot(); request('I'); stage=WAIT_HELD_READ; }
        else if((profile=="cancel-layout"||fixture()==6)&&!prepared) { require(InventoryCloseGateAccess::widget(inventory)->getSelectedItem()==fixtureItem); RuleInventory *ground=game->getMod()->getInventory("STR_GROUND",true); click(80+fixtureItem->getRules()->getInventoryWidth()*8,ground->getY()+fixtureItem->getRules()->getInventoryHeight()*8); prepared=true; }
        else { require(!InventoryCloseGateAccess::widget(inventory)->getSelectedItem()); if(profile=="cancel-page") require(InventoryCloseGateAccess::page(inventory)>0);
            if(profile=="cancel-layout") require(fixtureItem->getSlotX()==5); request('I'); stage=WAIT_I; }
    } else if(stage==WAIT_UNLOAD&&inputs.empty()) {
        require(!InventoryCloseGateAccess::widget(inventory)->getSelectedItem()&&fixtureItem->getAmmoItem()==0);
        require(fixtureItem->getOwner()==player&&fixtureItem->getSlot()->getId()=="STR_RIGHT_HAND");
        require(manualAmmo->getOwner()==player&&manualAmmo->getSlot()->getId()=="STR_LEFT_HAND"&&manualAmmo->getAmmoQuantity()==initialAmmo);
        require(player->getTimeUnits()==32&&inits==1&&closes==0); before=snapshot(); request('I'); stage=WAIT_UNLOAD_I;
    } else if(stage==WAIT_RELOAD_PICK&&inputs.empty()) {
        require(InventoryCloseGateAccess::widget(inventory)->getSelectedItem()==manualAmmo); before=snapshot(); request('J'); stage=WAIT_RELOAD_HELD_J;
    } else if(stage==WAIT_RELOAD_HELD_J&&available()) {
        unavailable(receive()); compare(before,snapshot()); require(InventoryCloseGateAccess::widget(inventory)->getSelectedItem()==manualAmmo&&closes==0);
        RuleInventory *hand=game->getMod()->getInventory("STR_RIGHT_HAND",true); click(hand->getX()+8,hand->getY()+8); stage=WAIT_RELOAD_DONE;
    } else if(stage==WAIT_RELOAD_DONE&&inputs.empty()) {
        require(!InventoryCloseGateAccess::widget(inventory)->getSelectedItem()&&fixtureItem->getAmmoItem()==manualAmmo);
        require(fixtureItem->getOwner()==player&&manualAmmo->getOwner()==0&&manualAmmo->getAmmoQuantity()==initialAmmo);
        require(player->getTimeUnits()==17&&inits==1&&closes==0); request('I'); stage=WAIT_I;
    } else if(stage==WAIT_HELD_REFUSAL&&available()) {
        unavailable(receive()); compare(before,snapshot()); require(InventoryCloseGateAccess::widget(inventory)->getSelectedItem()==fixtureItem&&closes==0);
        key(Options::keyBattleInventory); stage=WAIT_HELD_RETURN;
    } else if(stage==WAIT_HELD_RETURN&&inputs.empty()) {
        require(owner->isState(inventory)&&InventoryCloseGateAccess::widget(inventory)->getSelectedItem()==fixtureItem&&closes==1&&destroyed==0); compare(before,snapshot());
        click(160,80,SDL_BUTTON_RIGHT); stage=WAIT_RECOVER; retry=true;
    } else if(stage==SEND_COMMAND) {
        if(profile=="stale-input") motion(151,81);
        if(profile=="stale-actor") { game->getSavedGame()->getSavedBattle()->setSelectedUnit(twin); game->getSavedGame()->getSavedBattle()->setSelectedUnit(player); }
        if(profile=="stale-reinit") inventory->init();
        command(); stage=WAIT_PENDING;
    } else if(stage==WAIT_PENDING&&available()) {
        if(stale()||profile=="capless-I") { status(receive(),"unavailable"); require(closes==0&&destroyed==0); finish(); return; }
        status(receive(),"pending_confirmation"); require(game->isState(inventory)&&shown==(cancelCase()&&retry?2:1)&&closes==0&&destroyed==0&&inits==baselineInits);
        if(profile=="unseen") { require(shown==1); stage=WAIT_INPUT; }
        else stage=CONFIRM;
    } else if(stage==CONFIRM&&modalFrames>0) {
        require(game->isState(inventory)&&InventoryCloseGateAccess::modal(inventory));
        if(inputCase()&&!retry) {
            if(profile=="overlay-input") { if(Clock::now()-modalStarted<std::chrono::milliseconds(400)) return; require(InventoryCloseGateAccess::animation(inventory)!=baselineAnimation); click(152,80,SDL_BUTTON_X1); }
            else if(profile=="input-x2") click(152,80,SDL_BUTTON_X2);
            else if(profile=="input-wheel") click(152,80,SDL_BUTTON_WHEELUP);
            else if(profile=="input-hotkey") key(Options::keyBattleInventory);
            else if(profile=="input-outside") click(8,8);
            else { SDL_Event e={}; e.type=SDL_KEYDOWN; e.key.which=255; e.key.state=SDL_PRESSED; e.key.keysym.sym=SDLK_y; e.key.keysym.mod=SDLMod(KMOD_SHIFT); queue(e); keyEdge(SDLK_y,false); }
            stage=WAIT_INPUT;
        } else if(cancelCase()&&!retry) { click(280,128); stage=CANCEL_STATUS; }
        else if(profile=="held-y") { keyEdge(SDLK_y,true); stage=WAIT_INPUT; }
        else if(profile=="held-mouse") { mouseEdge(280,112,false); stage=WAIT_INPUT; }
        else if(profile=="focus") { focus(false); stage=WAIT_INPUT; }
        else if(profile=="display") { game->getScreen()->resetDisplay(false); stage=WAIT_INPUT; }
        else if(profile=="overlay") { game->pushState(new State); stage=WAIT_INPUT; }
        else if(profile=="timeout") { SDL_Delay(120100); key(SDLK_y); stage=WAIT_INPUT; }
        else if(profile=="disconnect") { close(peer); peer=-1; stage=WAIT_RETIRE; }
        else if(profile=="palette") { SDL_Color palette[256]; std::copy(game->getScreen()->getPalette(),game->getScreen()->getPalette()+256,palette); game->getScreen()->resetDisplay(false); game->getScreen()->setPalette(palette,0,1,false); stage=WAIT_INPUT; }
        else { if(core()&&world%4==3) { click(280,112); stage=WAIT_CLOSE; closeSent=true; } else approved(); }
    } else if(stage==WAIT_INPUT&&inputs.empty()) {
        if(inputCase()) { verifyPreserved(); require(!InventoryCloseGateAccess::modal(inventory)); }
        require(closes==0); sendPacket("S"+commandId); stage=WAIT_CANCEL_STATUS;
    } else if(stage==CANCEL_STATUS&&inputs.empty()) { sendPacket("S"+commandId); stage=WAIT_CANCEL_STATUS; }
    else if(stage==WAIT_CANCEL_STATUS&&available()) {
        status(receive(),"unavailable"); require(closes==0);
        if(terminal()) { require(closes==1); barrier=0; stage=TERMINAL_REQUEST; }
        else if(cancelCase()) { verifyPreserved(); require(!InventoryCloseGateAccess::modal(inventory)&&dismissed==1); retry=true; request('J'); stage=WAIT_CONTEXT; }
        else { if(profile=="focus") focus(true); if(profile=="overlay") game->popState();
            if(profile=="held-y") keyEdge(SDLK_y,false); stage=WAIT_RETIRE; }
    } else if(stage==WAIT_RETIRE&&inputs.empty()) {
        if(terminal()) { barrier=0; stage=TERMINAL_REQUEST; }
        else if(inventory&&game->isState(inventory)&&!InventoryCloseGateAccess::modal(inventory)) {
            require(closes==0&&destroyed==0); if(profile!="overlay"&&profile!="palette") compare(before,snapshot());
            if(profile=="palette") { game->getScreen()->setPalette(inventory->getPalette(),0,256,false); }
            closeManual();
        }
    } else if(stage==WAIT_RECOVER&&inputs.empty()) {
        require(owner->isState(inventory)&&!InventoryCloseGateAccess::widget(inventory)->getSelectedItem()); closes=0; request('I'); stage=WAIT_I;
    } else if(stage==BARRIER_REPLY&&available()) {
        unavailable(receive()); require(closes==1&&destroyed==1);
        if(++barrier<3) request(barrier==1?'I':'J'); else request('V'); stage=barrier<3?BARRIER_REPLY:WAIT_V;
    } else if(stage==CROSS_REPLY&&available()) {
        status(receive(),"unavailable"); verifyPreserved();
        if(++replay==5) stage=SEND_COMMAND;
        else { const char kinds[]={'B','D','N','R','L'}; commandId=randomToken(); sendPacket(packet(kinds[replay],commandId,capability)); }
    } else if(stage==WAIT_STATUS&&available()) { status(receive(),"dispatched"); afterView(); }
    else if(stage==REPLAY_SEND) {
        commandId=commandPacket.substr(65,32);
        if(replay==0) sendPacket(commandPacket);
        else if(replay==1) sendPacket("S"+commandId);
        else if(replay==2) { std::string conflict=commandPacket; conflict.replace(33,32,randomToken()); sendPacket(conflict); }
        else if(replay<8) { const char kinds[]={'B','D','N','R','L'}; sendPacket(packet(kinds[replay-3],commandId,capability)); }
        else if(replay==8) { commandId=randomToken(); std::string consumed=commandPacket; consumed.replace(65,32,commandId); sendPacket(consumed); }
        else { request('O'); stage=WAIT_POST_REPLAY_O; observed=true; return; }
        stage=REPLAY_REPLY;
    } else if(stage==REPLAY_REPLY&&available()) {
        if(replay==100) { status(receive(),"unavailable"); compare(before,snapshot()); stage=LIMIT_SEND; return; }
        status(receive(),replay<2?"dispatched":"unavailable"); compare(opened,snapshot()); require(closes==1&&destroyed==1); ++replay; stage=REPLAY_SEND;
    } else if(stage==LIMIT_SEND) {
        const char kinds[]={'C','B','D','N','R','L'}; commandId=randomToken(); std::string p=packet(kinds[limits%6],commandId,randomToken()); if(limits==0) firstPacket=p; sendPacket(p); stage=limits==64?LIMIT_FINAL:LIMIT_REPLY;
    } else if(stage==LIMIT_REPLY&&available()) {
        status(receive(),"unavailable"); compare(before,snapshot()); require(closes==0); ++limits;
        if(limits==64) { commandId=firstPacket.substr(65,32); sendPacket(firstPacket); replay=100; stage=REPLAY_REPLY; }
        else stage=LIMIT_SEND;
    } else if(stage==LIMIT_FINAL&&available()) { unavailable(receive()); char b; require(recv(peer,&b,1,MSG_DONTWAIT)==0&&commandIds.size()==65&&closes==0); finish(); }
    else if(stage==TERMINAL_REQUEST) { request(barrier==0?'O':barrier==1?'I':barrier==2?'J':'V'); stage=TERMINAL_REPLY; }
    else if(stage==TERMINAL_REPLY&&available()) { unavailable(receive()); if(++barrier==4) finish(); else stage=TERMINAL_REQUEST; }
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
    if(stage==WAIT_CLOSE&&game->isState(state)&&closes==1) {
        require(destroyed==0&&InventoryCloseGateAccess::deleted(game)==1&&!InventoryCloseGateAccess::initialized(game));
        closedFrame=frameSerial; motion();
        if(terminal()) { terminalReplaced=true; replacement(); stage=WAIT_RETIRE; return; }
        if(profile=="disconnect") { stage=WAIT_V; return; }
        if(!closeSent) { stage=WAIT_MANUAL_FRAME; return; }
        if(closeSent&&(profile=="ledger"||(core()&&world%4==3))) { barrier=0; request('O'); stage=BARRIER_REPLY; }
        else { request('V'); stage=WAIT_V; }
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
    if(inventory&&game->isState(inventory)&&InventoryCloseGateAccess::modal(inventory)) { ++modalFrames; require(display()!=beforePixels); }
    if(stage==WAIT_INVENTORY&&inventory&&game->isState(inventory)&&inputs.empty()&&++inventoryFrames>=2) {
        require(inits==1&&closes==0&&destroyed==0); stage=PREPARE_INVENTORY;
    }
    if(stage==COLLECT_MANUAL&&game->isState(state)) {
        const int phase=InventoryCloseGateAccess::phase(state->getMap()); require(phase>=0&&phase<8);
        if(phase!=manualLastPhase) {
            require(phase==(manualLastPhase+1)%8); manualLastPhase=phase; ++manualTicks;
            const auto pixels=display(); manualPhasePixels[phase].insert(pixels); retainFrame(pixels,phase,manualTicks,"manual");
        }
        // Collect all eight naturally rendered Map phases. The positive FOV
        // fixture turns an unselected friendly, avoiding unrelated hostile-UI
        // startup/blink transients. Never reset a phase, advance a timer, force
        // a frame, or mask pixels.
        if(manualPhasePixels.size()==8&&manualTicks>=7) { compare(manualClosed,snapshot()); ++world; replacement(); return; }
    }
    if(stage==WAIT_GRAVITY_SETTLE&&idle()&&player->getPosition().z==0&&inputs.empty()) { request('V'); stage=WAIT_V; observed=true; }
    if(stage==WAIT_MANUAL_FRAME&&idle()&&destroyed==1&&inputs.empty()) { request('V'); stage=WAIT_V; }
    if(stage==WAIT_I&&available()) {
        if(profile=="nontu"||profile=="debug") { unavailable(receive()); request('J'); stage=WAIT_CONTEXT; return; }
        context(receive(),'I');
        if(fixture()==6) {
            // Consequence-positive private fixture setup while inventory is open:
            // turn the unselected friendly from south to north with an ordinary
            // setter. Terrain discovery, not the opposed-unit visibleTiles list,
            // demonstrably excludes the north tile beforehand. Only
            // the normal Inventory destructor recalculates that changed cone.
            Tile *north=game->getSavedGame()->getSavedBattle()->getTile(Position(30,15,0));
            require(north&&twin->getDirection()==4);
            require(!north->isDiscovered(2)&&north->getVisible()==0);
            twin->setDirection(0);
        }
        rememberInventory();
        if(profile=="gravity") { player->getTile()->setMapData(0,0,0,O_FLOOR); before=snapshot(); }
        if(manual()) { if(fixture()==6) preservedShade=player->getTile()->getShade(); closeManual(); }
        else { if(profile=="held-y") keyEdge(SDLK_y,true); if(profile=="held-mouse") { motion(280,112); mouseEdge(280,112,true); }
            if(fixture()==6) preservedShade=player->getTile()->getShade(); request('J'); stage=WAIT_CONTEXT; }
    } else if(stage==WAIT_UNLOAD_I&&available()) {
        context(receive(),'I'); compare(before,snapshot());
        RuleInventory *hand=game->getMod()->getInventory("STR_LEFT_HAND",true); click(hand->getX()+8,hand->getY()+8); stage=WAIT_RELOAD_PICK;
    } else if(stage==WAIT_HELD_READ&&available()) { context(receive(),'I'); compare(before,snapshot()); request('J'); stage=WAIT_HELD_REFUSAL; }
    else if(stage==WAIT_CONTEXT&&available()) {
        if(profile=="disabled"||profile=="nontu"||profile=="debug") { unavailable(receive()); require(closes==0); finish(); return; }
        if(profile=="held-mouse") { YAML::Node n=receive(); if(n["error"]) { unavailable(n); verifyPreserved(); mouseEdge(280,112,false); stage=WAIT_RETIRE; return; } context(n,'J'); stage=SEND_COMMAND; return; }
        context(receive(),'J'); require(!available()&&owner->isState(inventory));
        if(profile=="capless-I") { request('I'); stage=WAIT_CAPLESS; }
        else if(profile=="cross-kind") { replay=0; commandId=randomToken(); sendPacket(packet('B',commandId,capability)); stage=CROSS_REPLY; }
        else stage=SEND_COMMAND;
    } else if(stage==WAIT_CAPLESS&&available()) { context(receive(),'I'); verifyPreserved(); stage=SEND_COMMAND; }
    else if(stage==WAIT_POST_REPLAY_O&&available()) { context(receive(),'O'); compare(opened,snapshot()); require(closes==1&&destroyed==1); request('V'); stage=WAIT_V; }
    else if(stage==WAIT_V) {
        if(profile=="disconnect") { if(idle()&&destroyed==1) finish(); return; }
        if(available()) {
            if(profile=="gravity") { YAML::Node n=receive(); if(n["error"]) { unavailable(n); require(!observed); } else context(n,'V');
                require(destroyed==1&&closes==1&&fixtureItem->getTile()&&fixtureItem->getTile()->getPosition().z==0);
                require(player->getPosition().z==0||!game->getSavedGame()->getSavedBattle()->getFallingUnits()->empty()||!state->getBattleGame()->getStates().empty());
                if(observed) { require(idle()&&player->getPosition().z==0); finish(); } else stage=WAIT_GRAVITY_SETTLE; return; }
            context(receive(),'V'); require(destroyed==1&&frameSerial>closedFrame&&game->isState(state));
            if(observed) { require(closes==1&&destroyed==1); compare(opened,snapshot()); finish(); }
            else if(closeSent) { sendPacket("S"+commandId); stage=WAIT_STATUS; } else afterView();
        }
    }
}
void inventoryInitialized(InventoryState *who) { if(!running||pendingSetup) return; require(!inventory||inventory==who); inventory=who; ++inits; }
void inventoryCloseEntered(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) { ++closes; require(closes==1); } }
void inventoryDestroyed(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) { require(closes==1); ++destroyed; inventory=0; } }
void closeOverlayShown(InventoryState *who)
{
    if(running&&!pendingSetup&&who==inventory) {
        ++shown; shownFrame=frameSerial; modalStarted=Clock::now();
        // Queue consent at the actual show hook, before this overlay can blit.
        // Either normal pre-flip handling or first-flip SDL_PeepEvents must
        // reject it. No event is synthesized by the production controller.
        if(profile=="unseen") { require(shown==1&&modalFrames==0&&closes==0); key(SDLK_y); }
    }
}
void closeOverlayDismissed(InventoryState *who) { if(running&&!pendingSetup&&who==inventory) ++dismissed; }
void inventoryGroundArranged(Inventory *) { if(running&&!pendingSetup) ++arranged; }
void kneelClicked(BattlescapeState *) { if(running&&!pendingSetup) require(false); }
void selectionClicked(BattlescapeState *,bool) { if(running&&!pendingSetup) require(false); }
int exitStatus()
{
    if(enabled&&completed) {
        if(core()) require(world==PairWorlds&&parityCount==PairCount&&oracleCount>=PairWorlds*2);
        std::cout << "INVENTORY_CLOSE_CONTROL_GATE PASS {\"schema\":\"inventory-close-control-gate/1\",\"master\":\"" << master << "\",\"profile\":\"" << profile << "\",\"passed\":true}" << std::endl;
    }
    if(peer>=0) { close(peer); peer=-1; } return enabled&&!completed?EXIT_FAILURE:EXIT_SUCCESS;
}
} }
#undef require
#undef fields
