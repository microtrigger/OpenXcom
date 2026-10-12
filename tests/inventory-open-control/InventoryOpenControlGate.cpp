// SPDX-License-Identifier: GPL-3.0-or-later
// Original test contribution, 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md.
// A finite, default-OFF native inventory-opening and pixels-only read fixture.
// Normal events, initialization, timers, blits and flips remain native.
// Oracle values, resources and captures remain private in memory.
#include "InventoryOpenControlGate.h"
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
#include <map>
#include <set>
#include <string>
#include <sys/socket.h>
#include <typeinfo>
#include <unistd.h>
#include <vector>

#include "../../src/Battlescape/InventoryState.h"
#include "../../src/Mod/RuleInventory.h"
namespace OpenXcom { namespace InventoryOpenControlGate {
namespace {
using Clock = std::chrono::steady_clock;
enum { PairCount = 12, PairWorlds = 24 };
enum Adversary { KNOW_A, KNOW_B, HOVER, PAGE, SELECT, HELD_ITEM, DEADLINE, PARTIAL_PALETTE,
    O_FOR_B, V_FOR_D, STALE_CAMERA, STALE_POSE, PENDING_CAMERA, PENDING_POSE,
    CANCEL, UNSEEN_Y, HELD_Y, HELD_MOUSE, LEDGER_B, LEDGER_D, LEDGER_N,
    LEDGER_R, LEDGER_L, I_NO_GRANT, AdversaryCount };
enum Stage { WARM, WAIT_HELD, WAIT_CONTEXT, COMMAND, WAIT_PENDING, CONFIRM,
    WAIT_BLOCKED, WAIT_CANCELLED, WAIT_REJECTED, WAIT_OPEN, WAIT_FIRST_I,
    WAIT_BARRIER_O, WAIT_BARRIER_V, WAIT_B_STATUS, WAIT_REPEAT_I,
    REQUEST_DELAYED_I, WAIT_DELAYED_ADMISSION, WAIT_DELAYED_I, WAIT_FAILED_I,
    WAIT_HELD_ITEM_CLOSE, WAIT_RETURN_ITEM, WAIT_CLOSE, WAIT_POST_CONTEXT, WAIT_LEDGER_CONTEXT, LEDGER_SEND, LEDGER_REPLY,
    NO_GRANT_SEND, NO_GRANT_REPLY, WAIT_ACTION, WAIT_POST_MAP_SETTLED, WAIT_POST_MAP_INVENTORY, WAIT_POST_MAP_O,
    REQUEST_PARTIAL, WAIT_PARTIAL_FAILURE, WAIT_PALETTE_RESTORED, WAIT_PARTIAL_SUCCESS, WAIT_TERMINAL_FAILURE,
    WAIT_TERMINAL_CLOSE, TERMINAL_REQUEST, TERMINAL_REPLY,
    LIMIT_SEND, LIMIT_REPLY, LIMIT_FINAL, FINISHED };
bool enabled=false, running=false, completed=false, pendingSetup=false, moved=false;
bool sentAtEntry=false, faultInjected=false, replacementOnly=false, terminalReplacement=false, primerChanged=false, cancelStatusSent=false;
int peer=-1, world=0, entries=0, inits=0, stanceCalls=0, selectionCalls=0;
int frames=0, modalFrames=0, repeat=0, replay=0, parityCount=0, postCount=0;
int oracleCount=0, naturalWarps=0, firstWarp=0, pendingLoops=0, terminalRead=0;
int initialTU=40, firstInitCount=0, limitCount=0, frameSerial=0, partialSerial=0;
SDL_Color savedPalette[256];
Stage stage=WARM;
Game *owner=0; BattlescapeState *state=0; InventoryState *inventory=0;
BattleUnit *player=0, *twin=0;
const Position playerPosition(20,20,0), twinPosition(30,30,0);
Clock::time_point started, worldStarted, requestStarted, firstReadTime;
std::string master, profile, epoch, capability, challenge, commandId, commandPacket, firstPacket;
std::set<std::string> tokens, commandIds;
std::deque<SDL_Event> inputs;
std::vector<unsigned char> lastPixels, knowledgePixels, initialInventoryPixels, pendingPixels;
YAML::Node knowledgeView;
struct Snapshot { YAML::Node save, runtime; uint64_t rng; };
Snapshot before, manualOpened, manualClosed, opened;
void check(bool ok,int line) { if(!ok) {
    // Bounded fixture coordinates only. Never packet content, names or state.
    // The strict wrapper suppresses this private diagnosis and exposes at most
    // the single fixed assertion label below.
    std::cerr << "INVENTORY_FIXTURE_DIAGNOSTIC world-" << world << " phase-" << int(stage)
        << " entries-" << entries << " inits-" << inits << std::endl;
    std::cerr << "INVENTORY_OPEN_CONTROL_GATE FAIL assertion-" << line << std::endl;
    std::_Exit(EXIT_FAILURE);
} }
#define require(ok) check((ok),__LINE__)
bool core() { return profile=="core"; }
bool paired() { return core() && world<PairWorlds; }
int adversary() { return world-PairWorlds; }
bool is(int a) { return core() && !paired() && adversary()==a; }
bool ledger() { return is(LEDGER_B)||is(LEDGER_D)||is(LEDGER_N)||is(LEDGER_R)||is(LEDGER_L); }
bool delayed() { return is(HOVER)||is(PAGE)||is(SELECT)||is(HELD_ITEM)||is(DEADLINE); }
bool terminal() { return profile=="focus"||profile=="overlay"||profile=="pop"||profile=="state"||profile=="save"||profile=="display"; }
bool postMap() { return profile=="post-r"||profile=="post-l"; }
bool disabled() { return profile=="disabled"||profile=="turn-disabled"; }
bool rejectModal() { return is(CANCEL)||is(PENDING_CAMERA)||is(PENDING_POSE)||is(UNSEEN_Y)||is(HELD_Y)||is(HELD_MOUSE); }
bool rejectCommand() { return is(O_FOR_B)||is(V_FOR_D)||is(STALE_CAMERA)||is(STALE_POSE); }
bool manualOpen() { return (paired() && world%2==0)||is(KNOW_A)||is(KNOW_B)||delayed()||is(PARTIAL_PALETTE)||is(I_NO_GRANT)||profile=="nontu"||profile=="debug"; }
int fixture() { if(paired()) return (world/2)%6; if(is(KNOW_A)||is(KNOW_B)||is(PAGE)) return 4; if(is(HELD_ITEM)||is(HOVER)||is(I_NO_GRANT)) return 2; return 1; }
char kind() { if(is(LEDGER_D)||is(V_FOR_D)||profile=="ledger-limit") return 'D'; if(is(LEDGER_N)) return 'N'; if(is(LEDGER_R)||profile=="post-r") return 'R'; if(is(LEDGER_L)||profile=="post-l") return 'L'; return 'B'; }
char contextKind() { return is(O_FOR_B)||kind()=='D'||kind()=='N'||profile=="turn-disabled" ? (is(V_FOR_D)?'V':'O') : 'V'; }
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
    if(s.size()==99||s.size()==103) { commandIds.insert(s.substr(65,32)); require(commandIds.size()<=size_t(profile=="ledger-limit"?65:64)); }
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
    lastPixels=display(); std::map<unsigned int,unsigned int> first;
    for(size_t i=0;i<indices.size();++i) { unsigned int rgb=(lastPixels[i*3]<<16)|(lastPixels[i*3+1]<<8)|lastPixels[i*3+2];
        if(!first.count(rgb)) { unsigned int next=first.size(); first[rgb]=next; } require(first.size()<=256&&indices[i]==first[rgb]);
        for(int c=0;c<3;++c) require(palette[indices[i]*3+c]==lastPixels[i*3+c]); }
    for(size_t i=first.size()*3;i<palette.size();++i) require(palette[i]==0); ++oracleCount;
}
void context(const YAML::Node &n,char type)
{
    if(type=='I') { fields(n,{"schema","challenge","semantics","view"}); require(n["schema"].as<std::string>()=="experimental-inventory-view/1"); }
    else { fields(n,{"schema","challenge","control_epoch","capability","semantics",type=='V'?"view":"panel"});
        require(n["schema"].as<std::string>()==(type=='V'?"experimental-tactical-context/1":"experimental-stance-context/1"));
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
void approve(bool yes) { if(kind()=='D'||kind()=='N') click(yes?95:225,118); else click(280,yes?8:24); }
void focus(bool gain) { SDL_Event e={}; e.type=SDL_ACTIVEEVENT; e.active.gain=gain?1:0; e.active.state=SDL_APPINPUTFOCUS; queue(e); }
std::string coordinate(int n) { require(n>=0&&n<1000); std::string s=std::to_string(n); return std::string(3-s.size(),'0')+s; }
std::string packet(char k,const std::string &id,const std::string &grant)
{
    Position p; state->getMap()->getCamera()->convertMapToScreen(owner->getSavedGame()->getSavedBattle()->getSelectedUnit()->getPosition(),&p); p+=state->getMap()->getCamera()->getMapOffset(); p.x+=16; p.y+=24;
    require(p.x>=16&&p.x<=303&&p.y>=40&&p.y<=127);
    std::string payload=k=='B'?"o0":k=='D'?"sk":k=='N'?"n0":coordinate(p.x)+coordinate(p.y);
    return std::string(1,k)+epoch+grant+id+payload;
}
void command() { commandId=randomToken(); commandPacket=packet(kind(),commandId,capability); sendPacket(commandPacket); }
void roundtrip(bool camera) { if(camera) { Camera *c=state->getMap()->getCamera(); Position p=c->getMapOffset(); c->setMapOffset(p+Position(8,0,0)); c->setMapOffset(p); } else { int d=player->getDirection(); player->setDirection((d+1)%8); player->setDirection(d); } }
void quiet() { int f,c; Uint16 format; char driver[64]={}; require(Options::mute&&Mix_QuerySpec(&f,&format,&c)>0&&SDL_AudioDriverName(driver,sizeof(driver))&&std::string(driver)=="dummy"&&Mix_Playing(-1)==0&&Mix_PlayingMusic()==0); }
bool idle() { const Uint8 focus=SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS; return owner->isState(state)&&(SDL_GetAppState()&focus)==focus&&state->getBattleGame()->getPanicHandled()&&!state->getBattleGame()->isBusy()&&state->getBattleGame()->getStates().empty()&&player->getStatus()==STATUS_STANDING&&!state->getMap()->getProjectile()&&state->getMap()->getCursorType()==CT_NORMAL; }
void finish() { require(inputs.empty()); running=false; completed=true; stage=FINISHED; owner->quit(); }
void replacement() { pendingSetup=true; owner->setState(new State); }
void finishWorld() { require(inputs.empty()); if(!core()) { finish(); return; } ++world; if(world==PairWorlds+AdversaryCount) finish(); else replacement(); }
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
void canonicalGround()
{
    if(fixture()!=4) return; auto ground=player->getTile()->getInventory(); require(ground->size()==25);
    for(size_t i=0;i<ground->size();++i) require((*ground)[i]->getSlotX()==int(i)&&(*ground)[i]->getSlotY()==0&&(*ground)[i]->getSlot()->getId()=="STR_GROUND");
}
void openingOutcome()
{
    require(inventory&&owner->isState(inventory)&&inits==1&&entries==1&&player->getTimeUnits()==initialTU&&player->getEnergy()==60&&player->getPosition()==playerPosition&&player->getDirection()==0&&!player->isKneeled());
    require(state->getBattleGame()->getStates().empty()&&!state->getBattleGame()->isBusy()&&state->getBattleGame()->getCurrentAction()->type==BA_NONE);
    canonicalGround();
}
void closeInventory() { motion(); key(Options::keyBattleInventory); stage=WAIT_CLOSE; }
void afterInitialRead()
{
    if(paired()) { openingOutcome(); opened=snapshot(); if(world%2) { compare(manualOpened,opened); ++parityCount; } else manualOpened=opened; }
    else opened=snapshot();
    firstInitCount=inits; repeat=0; firstReadTime=Clock::now(); initialInventoryPixels=lastPixels; primerChanged=false;
    if(delayed()) { stage=REQUEST_DELAYED_I; return; }
    if(is(PARTIAL_PALETTE)) { stage=REQUEST_PARTIAL; return; }
    if(ledger()&&kind()=='B') { stage=LEDGER_SEND; replay=0; return; }
    if(is(I_NO_GRANT)) { replay=0; stage=NO_GRANT_SEND; return; }
    if(postMap()) { closeInventory(); return; }
    request('I'); stage=WAIT_REPEAT_I;
}
} // namespace
bool active() { const char *s=std::getenv("OPENXCOM_INVENTORY_OPEN_CONTROL_GATE"); return s&&std::string(s)=="1"; }
void prepare()
{
    enabled=active(); if(!enabled) return; const char *p=std::getenv("OPENXCOM_INVENTORY_OPEN_PROFILE"); profile=p?p:"";
    const std::set<std::string> profiles={"core","focus","overlay","pop","state","save","display","ledger-limit","post-r","post-l","disabled","turn-disabled","nontu","debug"}; require(profiles.count(profile)==1);
    require(!std::getenv("OPENXCOM_STANCE_CONTROL_GATE")&&!std::getenv("OPENXCOM_SELECTION_CONTROL_GATE")&&!std::getenv("OPENXCOM_TURN_CONTROL_GATE")&&!std::getenv("OPENXCOM_MAP_LEFT_CLICK_CONTROL_GATE"));
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
    if(terminalReplacement&&replacementOnly) {
        state=new BattlescapeState; game->getSavedGame()->getSavedBattle()->setBattleState(state); game->setState(state); inventory=0; moved=false; stage=WAIT_TERMINAL_FAILURE; return;
    }
    if(!terminalReplacement) { entries=inits=stanceCalls=selectionCalls=frames=modalFrames=repeat=replay=pendingLoops=0; moved=sentAtEntry=faultInjected=cancelStatusSent=false; inventory=0; inputs.clear(); stage=WARM; worldStarted=Clock::now(); }
    Options::FPS=60; Options::maximizeInfoScreens=paired()&&world/2>=6;
    Options::battleXcomSpeed=30; Options::battleNewPreviewPath=PATH_NONE;
    RNG::setSeed(123456789); std::srand(12345); // Independently reconstructed setup only.
    SavedGame *save=new SavedGame; SavedBattleGame *battle=new SavedBattleGame; save->setBattleGame(battle); game->setSavedGame(save);
    battle->initMap(40,40,1); battle->setDepth(0); battle->setAmbientSound(-1); Mod *mod=game->getMod(); master=Options::getActiveMaster(); require((master=="xcom1"||master=="xcom2")&&Options::getActiveMods().size()==1);
    MapDataSet *data=mod->getMapDataSet("BLANKS"); require(data); data->loadData(mod->getMCDPatch(data->getName())); battle->getMapDataSets()->push_back(data); MapData *floor=data->getObject(1); require(floor&&floor->getObjectType()==O_FLOOR&&!floor->isNoFloor()&&floor->getTerrainLevel()==0&&floor->getTUCost(MT_WALK)==4);
    for(int i=0;i<battle->getMapSizeXYZ();++i) battle->getTiles()[i]->setMapData(floor,1,0,O_FLOOR);
    Base *base=new Base(mod); save->getBases()->push_back(base); RuleSoldier *rule=mod->getSoldier("STR_SOLDIER",true); Armor *armor=mod->getArmor(rule->getArmor(),true);
    Soldier *a=new Soldier(rule,armor,1); a->setName("INVENTORY FIXTURE"); a->getCurrentStats()->tu=60; a->getCurrentStats()->stamina=60;
    Soldier *b=new Soldier(rule,armor,2); YAML::Node clone=a->save(); clone["id"]=2; b->load(clone,mod,save); if(is(SELECT)) b->setName("SECOND INVENTORY FIXTURE"); base->getSoldiers()->push_back(a); base->getSoldiers()->push_back(b);
    player=new BattleUnit(a,0); twin=new BattleUnit(b,0); player->setPosition(playerPosition); player->setDirection(0); twin->setPosition(twinPosition); twin->setDirection(4); twin->setTimeUnits(40);
    battle->getUnits()->push_back(player); battle->getUnits()->push_back(twin); battle->resetUnitTiles(); battle->setSelectedUnit(player); battle->initUtilities(mod);
    initialTU=fixture()==0?0:40; player->setTimeUnits(initialTU);
    if(fixture()==2) arm(player);
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
        if(is(KNOW_A)||is(KNOW_B)) {
            arm(twin)->getAmmoItem()->setAmmoQuantity(is(KNOW_A)?1:2);
            player->getTile()->getInventory()->back()->setStimulantQuantity(is(KNOW_A)?9:8);
        }
    }
    if(fixture()==5) { BattleItem *grenade=item(master=="xcom1"?"STR_GRENADE":"STR_MAGNA_BLAST_GRENADE",player,"STR_BELT"); grenade->setFuseTimer(0); }
    battle->getTileEngine()->calculateSunShading(); battle->getTileEngine()->calculateTerrainLighting(); battle->getTileEngine()->calculateUnitLighting(); battle->getTileEngine()->recalculateFOV();
    if(profile=="debug") battle->setDebugMode();
    state=new BattlescapeState; battle->setBattleState(state); game->setState(state);
    if(terminalReplacement) { inventory=0; moved=false; stage=WAIT_TERMINAL_FAILURE; }
}
void beforeEvents(Game *game)
{
    if(!running||pendingSetup) return;
    require(Clock::now()-started<std::chrono::seconds(core()?220:40)); require(Clock::now()-worldStarted<std::chrono::seconds(20)); quiet(); require(SDL_GetModState()==KMOD_NONE);
    if(!moved) { motion(); moved=true; return; }
    if(stage==COMMAND) {
        if(is(STALE_CAMERA)||is(STALE_POSE)) roundtrip(is(STALE_CAMERA)); before=snapshot(); command(); stage=rejectCommand()?WAIT_REJECTED:WAIT_PENDING;
    } else if(stage==WAIT_PENDING&&available()) {
        status(receive(),"pending_confirmation"); require(!game->isState(state)&&entries==0);
        if(is(UNSEEN_Y)) { require(modalFrames==0); key(SDLK_y); stage=WAIT_BLOCKED; } else stage=CONFIRM;
    } else if(stage==CONFIRM&&modalFrames>0) {
        if(is(PENDING_CAMERA)||is(PENDING_POSE)) { roundtrip(is(PENDING_CAMERA)); before=snapshot(); approve(true); stage=WAIT_CANCELLED; }
        else if(is(CANCEL)) { approve(false); stage=WAIT_CANCELLED; }
        else if(is(HELD_Y)||is(HELD_MOUSE)) { if(is(HELD_Y)) keyEdge(SDLK_y,true); else mouseEdge(280,8,false); stage=WAIT_BLOCKED; }
        else { if(paired()&&world%4==3) key(SDLK_y); else approve(true); stage=kind()=='B'?WAIT_OPEN:WAIT_ACTION; }
    } else if(stage==WAIT_BLOCKED&&inputs.empty()) {
        require(entries==0); sendPacket("S"+commandId); stage=WAIT_REJECTED;
    } else if(stage==WAIT_REJECTED&&available()) {
        if(disabled()) { unavailable(receive()); require(entries==0); finish(); return; }
        status(receive(),"unavailable"); require(entries==0); compare(before,snapshot());
        if(is(HELD_Y)) keyEdge(SDLK_y,false);
        if(!game->isState(state)||is(HELD_Y)) { if(!game->isState(state)) key(SDLK_ESCAPE); stage=WAIT_CANCELLED; }
        else finishWorld();
    } else if(stage==WAIT_CANCELLED&&inputs.empty()&&game->isState(state)&&available()) {
        status(receive(),"unavailable"); require(entries==0); compare(before,snapshot()); finishWorld();
    } else if(stage==WAIT_BARRIER_O&&available()) {
        unavailable(receive()); require(entries==1&&inits==1); request('V'); stage=WAIT_BARRIER_V;
    } else if(stage==WAIT_BARRIER_V&&available()) {
        unavailable(receive()); request('I'); stage=WAIT_FIRST_I;
    } else if(stage==WAIT_B_STATUS&&available()) {
        status(receive(),"dispatched");
        if(kind()!='B') {
            require(entries==0);
            if(kind()=='D') require(stanceCalls==1&&player->isKneeled()&&player->getTimeUnits()==initialTU-4);
            if(kind()=='N') require(selectionCalls==1&&game->getSavedGame()->getSavedBattle()->getSelectedUnit()==twin);
            if(postMap()) { stage=WAIT_POST_MAP_SETTLED; }
            else { request(kind()=='R'||kind()=='L'?'V':'O'); stage=WAIT_LEDGER_CONTEXT; }
        } else { require(entries==1); afterInitialRead(); }
    } else if(stage==REQUEST_PARTIAL&&inputs.empty()) {
        before=snapshot(); firstInitCount=inits; partialSerial=frameSerial; Options::FPS=1;
        Screen *screen=game->getScreen(); std::copy(screen->getPalette(),screen->getPalette()+256,savedPalette);
        // Test setup changes only the public display lifecycle. A single color
        // is deliberately insufficient to establish a full exact palette.
        screen->resetDisplay(false);
        // resetDisplay queues a full palette. Keeping firstcolor=0 replaces
        // that pending range instead of the normal full+partial merge path.
        screen->setPalette(savedPalette,0,1,false);
        request('I'); stage=WAIT_PARTIAL_FAILURE;
    } else if(stage==WAIT_PARTIAL_FAILURE&&available()) {
        unavailable(receive()); require(frameSerial==partialSerial&&inits==firstInitCount); compare(before,snapshot());
        Options::FPS=60; game->getScreen()->setPalette(savedPalette,0,256,false); stage=WAIT_PALETTE_RESTORED;
    } else if(stage==REQUEST_DELAYED_I&&inputs.empty()) {
        // Ordinary FPS cap creates a presentation window. No timer is advanced,
        // no draw/init is called, and every read retains its fixed 1500ms limit.
        Options::FPS=1; before=snapshot(); pendingPixels=lastPixels; firstInitCount=inits; pendingLoops=0; request('I'); stage=WAIT_DELAYED_ADMISSION;
    } else if(stage==WAIT_DELAYED_ADMISSION&&++pendingLoops>=1) {
        require(!available()&&inventory&&game->isState(inventory));
        if(is(DEADLINE)) { SDL_Delay(1600); stage=WAIT_FAILED_I; }
        else { if(is(HOVER)) { RuleInventory *hand=game->getMod()->getInventory("STR_RIGHT_HAND",true); motion(hand->getX()+8,hand->getY()+8); } else if(is(PAGE)) click(304,144); else if(is(SELECT)) key(Options::keyBattleNextUnit); else if(is(HELD_ITEM)) { RuleInventory *hand=game->getMod()->getInventory("STR_RIGHT_HAND",true); click(hand->getX()+8,hand->getY()+8); } stage=WAIT_DELAYED_I; }
    } else if(stage==WAIT_FAILED_I&&available()) {
        unavailable(receive()); require(Clock::now()-requestStarted>=std::chrono::milliseconds(1500)); compare(before,snapshot()); require(inits==firstInitCount); Options::FPS=60; request('I'); stage=WAIT_DELAYED_I;
    } else if(stage==WAIT_FIRST_I&&terminal()&&!faultInjected&&inits==1) {
        require(!available()); faultInjected=true;
        if(profile=="focus") focus(false);
        else if(profile=="overlay") game->pushState(new State);
        else if(profile=="pop") key(Options::keyBattleInventory);
        else if(profile=="display") game->getScreen()->resetDisplay(false);
        else { terminalReplacement=true; replacementOnly=profile=="state"; replacement(); }
        stage=WAIT_TERMINAL_FAILURE;
    } else if(stage==WAIT_TERMINAL_FAILURE&&available()) {
        unavailable(receive()); require(entries==1); Options::FPS=60;
        if(profile=="focus") focus(true);
        if(profile=="overlay") game->popState();
        if(inventory&&game->isState(inventory)) { key(Options::keyBattleInventory); stage=WAIT_TERMINAL_CLOSE; }
        else stage=WAIT_TERMINAL_CLOSE;
    } else if(stage==WAIT_TERMINAL_CLOSE&&inputs.empty()&&idle()) {
        terminalRead=0; stage=TERMINAL_REQUEST;
    } else if(stage==TERMINAL_REQUEST) {
        request(terminalRead==0?'O':terminalRead==1?'V':'I'); stage=TERMINAL_REPLY;
    } else if(stage==TERMINAL_REPLY&&available()) {
        unavailable(receive()); if(++terminalRead==3) { require(entries==1); finish(); } else stage=TERMINAL_REQUEST;
    } else if(stage==LEDGER_SEND) {
        commandId=commandPacket.substr(65,32);
        if(replay==0) sendPacket(commandPacket);
        else if(replay==1) sendPacket("S"+commandId);
        else if(replay==2) { std::string conflict=commandPacket; conflict.replace(33,32,randomToken()); sendPacket(conflict); }
        else if(replay>=3&&replay<8) { const char kinds[]={'B','D','N','R','L'}; char other=kinds[replay-3]; if(other==kind()) { ++replay; return; } sendPacket(packet(other,commandId,capability)); }
        else if(replay==8) { commandId=randomToken(); std::string consumed=commandPacket; consumed.replace(65,32,commandId); sendPacket(consumed); }
        else { compare(opened,snapshot()); if(kind()=='B') closeInventory(); else finishWorld(); return; }
        stage=LEDGER_REPLY;
    } else if(stage==LEDGER_REPLY&&available()) {
        status(receive(),replay<2?"dispatched":"unavailable");
        if(profile=="ledger-limit") { require(replay==20&&limitCount==64); compare(before,snapshot()); stage=LIMIT_SEND; }
        else { compare(opened,snapshot()); require(entries==(kind()=='B'?1:0)); ++replay; stage=LEDGER_SEND; }
    } else if(stage==NO_GRANT_SEND) {
        if(replay==5) { compare(opened,snapshot()); closeInventory(); return; }
        const char kinds[]={'B','D','N','R','L'}; commandId=randomToken(); sendPacket(packet(kinds[replay],commandId,token(capability)?capability:randomToken())); stage=NO_GRANT_REPLY;
    } else if(stage==NO_GRANT_REPLY&&available()) {
        status(receive(),"unavailable"); compare(opened,snapshot()); require(entries==1); ++replay; stage=NO_GRANT_SEND;
    } else if(stage==WAIT_POST_MAP_O&&available()) {
        unavailable(receive()); request('V'); stage=WAIT_POST_CONTEXT;
    } else if(stage==LIMIT_SEND) {
        const char kinds[]={'D','N','R','L','B'}; commandId=randomToken(); std::string p=packet(kinds[limitCount%5],commandId,randomToken()); if(limitCount==0) firstPacket=p; sendPacket(p); stage=limitCount==64?LIMIT_FINAL:LIMIT_REPLY;
    } else if(stage==LIMIT_REPLY&&available()) {
        status(receive(),"unavailable"); require(entries==0&&stanceCalls==0&&selectionCalls==0); compare(before,snapshot());
        if(++limitCount==64) { commandId=firstPacket.substr(65,32); sendPacket(firstPacket); stage=LEDGER_REPLY; replay=20; }
        else stage=LIMIT_SEND;
    } else if(stage==LIMIT_FINAL&&available()) {
        unavailable(receive()); char b; require(recv(peer,&b,1,MSG_DONTWAIT)==0); require(commandIds.size()==65&&entries==0); finish();
    }
}
bool acceptEvent(const SDL_Event &e)
{
    if(!running) return true; if(e.type==SDL_QUIT) return true;
    if(e.type==SDL_ACTIVEEVENT&&(inputs.empty()||inputs.front().type!=SDL_ACTIVEEVENT)) return true;
    const bool scripted=e.type==SDL_ACTIVEEVENT||(e.type==SDL_MOUSEMOTION&&e.motion.which==255)||((e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEBUTTONUP)&&e.button.which==255)||((e.type==SDL_KEYDOWN||e.type==SDL_KEYUP)&&e.key.which==255);
    if(!scripted) {
        // Native InventoryState::init/_refreshMouse warps must reach the real
        // controller. Filtering them would hide immediate-I cancellation bugs.
        if(e.type==SDL_MOUSEMOTION&&inventory&&owner->isState(inventory)) { ++naturalWarps; return true; }
        return false;
    }
    require(!inputs.empty()&&e.type==inputs.front().type); SDL_Event x=inputs.front(); inputs.pop_front();
    if(e.type==SDL_MOUSEMOTION) require(e.motion.x==x.motion.x&&e.motion.y==x.motion.y);
    else if(e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEBUTTONUP) require(e.button.x==x.button.x&&e.button.y==x.button.y&&e.button.button==x.button.button&&e.button.state==x.button.state);
    else if(e.type==SDL_KEYDOWN||e.type==SDL_KEYUP) require(e.key.keysym.sym==x.key.keysym.sym&&e.key.state==x.key.state&&e.key.keysym.mod==KMOD_NONE);
    else require(e.active.state==x.active.state&&e.active.gain==x.active.gain); return true;
}
void afterEvents(Game *game)
{
    if(!running||pendingSetup) return;
    // Opening inventory must never dispatch a stance or battle-selection handler.
    if(kind()=='B') require(stanceCalls==0&&selectionCalls==0);
    if(stage==WAIT_OPEN&&entries==1&&!sentAtEntry) {
        // This is before the new InventoryState's ordinary init, including its
        // generated mouse warps and a keyboard confirmation's remaining key-up.
        require(inits==0); sentAtEntry=true; firstWarp=naturalWarps;
        if(terminal()) Options::FPS=1;
        if(is(LEDGER_B)) { request('O'); stage=WAIT_BARRIER_O; } else { request('I'); stage=WAIT_FIRST_I; }
    } else if(stage==WAIT_ACTION&&inputs.empty()) {
        sendPacket("S"+commandId); stage=WAIT_B_STATUS;
        // Non-B receipts have no inventory callback; their post-read is below.
    } else if(stage==WAIT_CANCELLED&&inputs.empty()&&!cancelStatusSent) { sendPacket("S"+commandId); cancelStatusSent=true; }
    else if(stage==WAIT_HELD&&inputs.empty()) { request(contextKind()); stage=WAIT_CONTEXT; }
}
void beforeFrame(Game *game,State *top)
{
    if(!running||pendingSetup) return;
    if(dynamic_cast<InventoryState*>(top)) require(!inventory||inventory==top);
    if(stage==WARM&&moved&&inputs.empty()&&idle()&&++frames>=2) {
        before=snapshot();
        if(profile=="ledger-limit") { stage=LIMIT_SEND; return; }
        if(is(HELD_Y)) { keyEdge(SDLK_y,true); stage=WAIT_HELD; }
        else if(is(HELD_MOUSE)) { motion(280,8); mouseEdge(280,8,true); stage=WAIT_HELD; }
        else if(profile=="nontu") { game->pushState(new InventoryState(false,state)); request('I'); stage=WAIT_FIRST_I; }
        else if(manualOpen()) { key(Options::keyBattleInventory); stage=WAIT_OPEN; }
        else { request(contextKind()); stage=WAIT_CONTEXT; }
    }
    if(stage==WAIT_POST_MAP_SETTLED&&inputs.empty()&&idle()) { key(Options::keyBattleInventory); stage=WAIT_OPEN; }
    if(stage==WAIT_POST_MAP_INVENTORY&&inputs.empty()&&inventory&&game->isState(inventory)&&inits==1) closeInventory();
    if(stage==WAIT_HELD_ITEM_CLOSE&&inputs.empty()) {
        require(inventory&&game->isState(inventory)); compare(before,snapshot());
        click(160,80,SDL_BUTTON_RIGHT); stage=WAIT_RETURN_ITEM;
    } else if(stage==WAIT_RETURN_ITEM&&inputs.empty()) {
        require(inventory&&game->isState(inventory)); compare(before,snapshot()); closeInventory();
    }
}
void afterFrame(Game *game)
{
    if(!running||pendingSetup) return; ++frameSerial;
    if(stage==WAIT_PALETTE_RESTORED) { request('I'); stage=WAIT_PARTIAL_SUCCESS; }
    // Manual close may restore display scale and defer its tactical palette.
    // Let one ordinary restored tactical flip complete before asking for V;
    // V's existing strict preflight remains unchanged.
    if(stage==WAIT_CLOSE&&inputs.empty()&&idle()) {
        require(entries==1); if(paired()) { if(world%2) compare(manualClosed,snapshot()); else manualClosed=snapshot(); }
        request(postMap()?'O':'V'); stage=postMap()?WAIT_POST_MAP_O:WAIT_POST_CONTEXT;
    }
    if(!game->isState(state)&&!inventory&&(stage==WAIT_PENDING||stage==CONFIRM||stage==WAIT_BLOCKED)) ++modalFrames;
    if(stage==WAIT_CONTEXT&&available()) {
        context(receive(),contextKind()); require(!available()); stage=COMMAND;
        if(disabled()) { before=snapshot(); command(); stage=WAIT_REJECTED; }
    } else if(stage==WAIT_FIRST_I&&available()) {
        if(profile=="nontu"||profile=="debug") { unavailable(receive()); require(inits==1); finish(); return; }
        if(postMap()) { unavailable(receive()); require(entries==1); stage=WAIT_POST_MAP_INVENTORY; return; }
        YAML::Node n=receive(); context(n,'I'); require(inventory&&game->isState(inventory)&&inits==1&&!available());
        if(sentAtEntry) require(naturalWarps>firstWarp);
        if(is(KNOW_A)) { knowledgePixels=lastPixels; knowledgeView=n["view"]; }
        if(is(KNOW_B)) { require(lastPixels==knowledgePixels); require(equal(knowledgeView,n["view"])); }
        if(!manualOpen()&&!postMap()) { sendPacket("S"+commandId); stage=WAIT_B_STATUS; }
        else afterInitialRead();
    } else if(stage==WAIT_REPEAT_I&&available()) {
        context(receive(),'I'); compare(opened,snapshot()); require(inits==firstInitCount&&entries==1);
        if(fixture()==5&&lastPixels!=initialInventoryPixels) primerChanged=true;
        if(++repeat<3||(fixture()==5&&Clock::now()-firstReadTime<std::chrono::milliseconds(400))) request('I');
        else { if(fixture()==5) require(primerChanged); closeInventory(); }
    } else if(stage==WAIT_DELAYED_I&&available()) {
        context(receive(),'I'); require(entries==1);
        if(!is(DEADLINE)) require(lastPixels!=pendingPixels);
        if(is(SELECT)) { require(game->getSavedGame()->getSavedBattle()->getSelectedUnit()==twin&&inits==firstInitCount+1); }
        else { require(inits==firstInitCount); compare(before,snapshot()); }
        Options::FPS=60;
        if(is(HELD_ITEM)) { key(Options::keyBattleInventory); stage=WAIT_HELD_ITEM_CLOSE; }
        else if(is(HOVER)) { motion(8,160); request('I'); repeat=0; stage=WAIT_REPEAT_I; }
        else closeInventory();
    } else if(stage==WAIT_PARTIAL_SUCCESS&&available()) {
        context(receive(),'I'); require(inits==firstInitCount&&entries==1); compare(before,snapshot()); closeInventory();
    } else if(stage==WAIT_LEDGER_CONTEXT&&available()) {
        context(receive(),kind()=='R'||kind()=='L'?'V':'O'); opened=snapshot(); replay=0; stage=LEDGER_SEND;
    } else if(stage==WAIT_POST_CONTEXT&&available()) {
        context(receive(),'V'); require(idle()&&!available()); ++postCount; finishWorld();
    }
}
void inventoryOpenEntered(BattlescapeState *who)
{
    if(!running||pendingSetup||who!=state) return; ++entries; require(entries==1);
}
void inventoryInitialized(InventoryState *who)
{
    if(!running||pendingSetup) return; require(!inventory||inventory==who); inventory=who; ++inits;
}
void kneelClicked(BattlescapeState *who) { if(running&&!pendingSetup&&who==state) { ++stanceCalls; require(stanceCalls==1); } }
void selectionClicked(BattlescapeState *who,bool) { if(running&&!pendingSetup&&who==state) ++selectionCalls; }
int exitStatus()
{
    if(enabled&&completed) {
        if(core()) require(world==PairWorlds+AdversaryCount&&parityCount==PairCount&&oracleCount>PairWorlds*3);
        std::cout << "INVENTORY_OPEN_CONTROL_GATE PASS {\"schema\":\"inventory-open-control-gate/1\",\"master\":\"" << master << "\",\"profile\":\"" << profile << "\",\"passed\":true}" << std::endl;
    }
    if(peer>=0) { close(peer); peer=-1; } return enabled&&!completed?EXIT_FAILURE:EXIT_SUCCESS;
}
} }
#undef require
#undef fields
