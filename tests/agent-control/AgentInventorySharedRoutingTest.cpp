// SPDX-License-Identifier: GPL-3.0-or-later
// Dependency/model shims for run_inventory_shared_routing.py. Production method
// definitions are extracted verbatim into InventorySharedRoutingActual.inc.
// This is NOT a linked game, SDL, renderer, target-admission, or live acceptance
// test. See INVENTORY-SHARED-ROUTING.md for the exact substitutions and limits.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using std::floor;
using Uint8 = unsigned char;
static const Uint8 SDL_BUTTON_LEFT = 1, SDL_BUTTON_RIGHT = 3;
static const int KMOD_CTRL = 64;
static int modelModifiers = 0;
static int SDL_GetModState() { return modelModifiers; }

namespace OpenXcom {
enum InventoryType { INV_SLOT, INV_HAND, INV_GROUND };
enum BattleType { BT_NONE, BT_FIREARM, BT_MINDPROBE, BT_PSIAMP, BT_MELEE,
                  BT_CORPSE, BT_GRENADE, BT_PROXIMITYGRENADE, BT_MEDIKIT };
enum UnitStatus { STATUS_STANDING, STATUS_UNCONSCIOUS };
struct Position { int x,y,z; Position(int a=0,int b=0,int c=0):x(a),y(b),z(c){} };
struct State { int overCallbacks=0, clickCallbacks=0; };
struct Action {
    struct Details { struct Button { Uint8 button=SDL_BUTTON_LEFT; } button; } details;
    double x=0,y=0;
    double getAbsoluteXMouse() const { return x; }
    double getAbsoluteYMouse() const { return y; }
    Details *getDetails() { return &details; }
};
struct InteractiveSurface {
    int originX=7, originY=11;
    int getX() const { return originX; }
    int getY() const { return originY; }
    void mouseOver(Action *, State *s) { ++s->overCallbacks; }
    void mouseClick(Action *, State *s) { ++s->clickCallbacks; }
};
struct Surface {
    int x=0,y=0,w=32,h=48,clears=0,sprites=0;
    int getX() const { return x; } int getY() const { return y; }
    int getWidth() const { return w; } int getHeight() const { return h; }
    void setX(int a) { x=a; } void setY(int a) { y=a; }
    void clear() { ++clears; }
};
struct RuleItem {
    std::string type; int w=1,h=1; bool fixed=false; BattleType battle=BT_NONE;
    std::vector<std::string> ammo;
    explicit RuleItem(std::string t):type(t){}
    bool isFixed() const { return fixed; }
    int getInventoryWidth() const { return w; }
    int getInventoryHeight() const { return h; }
    BattleType getBattleType() const { return battle; }
    std::string getType() const { return type; }
    std::vector<std::string> *getCompatibleAmmo() { return &ammo; }
    void drawHandSprite(void *, Surface *s) { ++s->sprites; }
};
struct RuleSlot { int x,y; };
struct RuleInventory {
    static const int SLOT_W=16,SLOT_H=16,HAND_W=2,HAND_H=3;
    std::string id; InventoryType _type; int _x,_y;
    std::vector<RuleSlot> _slots; std::map<std::string,int> _costs;
    RuleInventory(std::string i,InventoryType t,int x,int y):id(i),_type(t),_x(x),_y(y){}
    std::string getId() const { return id; }
    std::vector<RuleSlot> *getSlots() { return &_slots; }
    InventoryType getType() const { return _type; }
    int getX() const { return _x; } int getY() const { return _y; }
    bool checkSlotInPosition(int *,int *) const;
    bool fitItemInSlot(RuleItem *,int,int) const;
    int getCost(RuleInventory *) const;
};
struct BattleUnit;
struct BattleItem {
    std::string label; RuleItem *rules; RuleInventory *slot=nullptr;
    BattleUnit *owner=nullptr,*unit=nullptr; BattleItem *ammo=nullptr;
    int x=0,y=0,fuse=-1,quantity=0;
    BattleItem(std::string l,RuleItem *r):label(l),rules(r){}
    RuleItem *getRules() { return rules; }
    RuleInventory *getSlot() const { return slot; }
    int getSlotX() const { return x; } int getSlotY() const { return y; }
    void setSlot(RuleInventory *s) { slot=s; }
    void setSlotX(int v) { x=v; } void setSlotY(int v) { y=v; }
    int getFuseTimer() const { return fuse; } void setFuseTimer(int n) { fuse=n; }
    BattleItem *getAmmoItem() const { return ammo; }
    void setAmmoItem(BattleItem *a) { ammo=a; }
    int getAmmoQuantity() const { return quantity; }
    bool needsAmmo() const { return rules->battle==BT_FIREARM; }
    BattleUnit *getUnit() const { return unit; }
    int getPainKillerQuantity() const { return 0; }
    int getHealQuantity() const { return 0; }
    int getStimulantQuantity() const { return 0; }
    void setTurnFlag(bool) {}
    void moveToOwner(BattleUnit *);
    bool occupiesSlot(int xx,int yy,BattleItem *except=nullptr) const {
        if (this==except) return false;
        if (slot->getType()==INV_HAND) return true;
        int ww=except ? except->rules->w : 1, hh=except ? except->rules->h : 1;
        return xx<x+rules->w && xx+ww>x && yy<y+rules->h && yy+hh>y;
    }
};
struct Tile {
    std::vector<BattleItem*> items;
    std::vector<BattleItem*> *getInventory() { return &items; }
    void removeItem(BattleItem *i) { items.erase(std::remove(items.begin(),items.end(),i),items.end()); }
    void addItem(BattleItem *i,RuleInventory *slot) { i->slot=slot; items.push_back(i); }
};
struct BattleUnit {
    std::vector<BattleItem*> items; Tile tile; int tu=80, spendCalls=0;
    std::vector<int> spendRequests; UnitStatus status=STATUS_STANDING; Position position;
    std::vector<BattleItem*> *getInventory() { return &items; }
    Tile *getTile() { return &tile; }
    UnitStatus getStatus() const { return status; }
    Position getPosition() const { return position; } void setPosition(Position p) { position=p; }
    BattleItem *getItem(RuleInventory *s,int x,int y) {
        const auto &list=s->getType()==INV_GROUND ? tile.items : items;
        for (auto *i:list) if (i->slot==s && i->occupiesSlot(x,y)) return i;
        return nullptr;
    }
    bool spendTimeUnits(int n) {
        ++spendCalls; spendRequests.push_back(n);
        if (tu<n) return false;
        tu-=n; return true;
    }
};
void BattleItem::moveToOwner(BattleUnit *next) {
    if (owner) owner->items.erase(std::remove(owner->items.begin(),owner->items.end(),this),owner->items.end());
    owner=next;
    if (owner && std::find(owner->items.begin(),owner->items.end(),this)==owner->items.end()) owner->items.push_back(this);
}
struct Sound { int plays=0; void play() { ++plays; } };
struct WarningMessage {
    std::vector<std::string> messages;
    void showMessage(const std::string &s) { messages.push_back(s); }
    std::string last() const { return messages.empty() ? "" : messages.back(); }
};
struct Language { std::string getString(const std::string &s) { return s; } };
struct Mod {
    enum { ITEM_DROP,ITEM_RELOAD };
    std::map<std::string,RuleInventory*> slots; Sound sounds[2];
    std::map<std::string,RuleInventory*> *getInventories() { return &slots; }
    RuleInventory *getInventory(const std::string &id,bool) { return slots.at(id); }
    Sound *getSoundByDepth(int,int type) { return &sounds[type]; }
    void *getSurfaceSet(const std::string &) { return nullptr; }
};
struct PrimeGrenadeState { PrimeGrenadeState(int,bool,BattleItem *){} };
struct Game {
    Mod mod; Language language; int pushes=0,pops=0;
    Mod *getMod() { return &mod; } Language *getLanguage() { return &language; }
    void pushState(PrimeGrenadeState *s) { ++pushes; delete s; }
    void popState() { ++pops; }
};
struct Options { static bool includePrimeStateInSavedLayout; };
bool Options::includePrimeStateInSavedLayout=false;
struct Inventory : InteractiveSurface {
    Game *_game; Surface selection; Surface *_selection=&selection; WarningMessage warning;
    WarningMessage *_warning=&warning; BattleUnit *_selUnit; BattleItem *_selItem=nullptr,*_mouseOverItem=nullptr;
    bool _tu=true,_base=false; int _groundOffset=0,_depth=0;
    std::map<int,std::map<int,int>> _stackLevel;
    std::uint64_t _agentInventoryRevision=0; int drawCalls=0,arrangeCalls=0;
    Inventory(Game *g,BattleUnit *u):_game(g),_selUnit(u){}
    void mouseOver(Action *,State *); void mouseClick(Action *,State *);
    bool movePointer(int,int); bool activateButton(Uint8,int,int,bool); bool returnHeldItem();
    bool unload(); void setSelectedItem(BattleItem *); void setMouseOverItem(BattleItem *);
    RuleInventory *getSlotInPosition(int *,int *) const;
    void moveItem(BattleItem *,RuleInventory *,int,int);
    static bool overlapItems(BattleUnit *,BattleItem *,RuleInventory *,int,int);
    bool canBeStacked(BattleItem *,BattleItem *);
    bool fitItem(RuleInventory *,BattleItem *,std::string &);
    // UI-only draw and ground arrangement model. Actual routing invokes these;
    // this shim counts them and recounts existing cells; it does not pack pages.
    void drawItems() { ++drawCalls; }
    void arrangeGround(bool=false) {
        ++arrangeCalls; _stackLevel.clear();
        for (auto *i:_selUnit->tile.items) if (i!=_selItem) ++_stackLevel[i->x][i->y];
        drawItems();
    }
};
struct BaselineInventory : Inventory {
    using Inventory::Inventory;
    void mouseOver(Action *,State *); void mouseClick(Action *,State *);
};
} // namespace OpenXcom

// The runner generates only source-extracted definitions here. No replacement
// branch bodies are supplied by this fixture.
#include "InventorySharedRoutingActual.inc"

using namespace OpenXcom;
static unsigned checks=0;
static void check(bool yes,const std::string &message) {
    ++checks; if (!yes) { std::cerr<<"FAIL "<<message<<"\n"; std::exit(1); }
}
enum Route { LegacyPhysical,SharedPhysical,SharedSemantic };
struct Fixture {
    Game game; BattleUnit unit;
    RuleInventory right{"STR_RIGHT_HAND",INV_HAND,0,0},left{"STR_LEFT_HAND",INV_HAND,48,0},ground{"STR_GROUND",INV_GROUND,0,80};
    RuleItem gunRule{"gun"},clipRule{"clip"},wrongRule{"wrong"},grenadeRule{"grenade"},fixedRule{"fixed"};
    BattleItem gun{"gun",&gunRule},clip{"clip",&clipRule},wrong{"wrong",&wrongRule},spare{"spare",&clipRule},grenade{"grenade",&grenadeRule},fixed{"fixed",&fixedRule};
    BaselineInventory inv{&game,&unit}; State state; Route route;
    explicit Fixture(Route r):route(r) {
        gunRule.w=2;gunRule.h=3;gunRule.battle=BT_FIREARM;gunRule.ammo={"clip"};clip.quantity=6;
        grenadeRule.battle=BT_GRENADE;fixedRule.fixed=true;
        game.mod.slots={{right.id,&right},{left.id,&left},{ground.id,&ground}};
        for (auto &a:game.mod.slots) for (auto &b:game.mod.slots) a.second->_costs[b.first]=4;
        place(gun,right);place(clip,left);place(wrong,ground,0,0);place(spare,ground,2,0);
        place(grenade,ground,4,0);place(fixed,ground,6,0);inv.arrangeGround();
    }
    void place(BattleItem &item,RuleInventory &slot,int x=0,int y=0) {
        unit.tile.removeItem(&item); item.moveToOwner(nullptr); item.slot=&slot;item.x=x;item.y=y;
        if (slot.getType()==INV_GROUND) unit.tile.items.push_back(&item); else item.moveToOwner(&unit);
    }
    Action action(int x,int y,Uint8 button=SDL_BUTTON_LEFT) {
        Action a;a.x=x+inv.getX()+0.75;a.y=y+inv.getY()+0.25;a.details.button.button=button;return a;
    }
    void hover(int x,int y) {
        Action a=action(x,y);
        if (route==LegacyPhysical) inv.mouseOver(&a,&state);
        else if (route==SharedPhysical) inv.Inventory::mouseOver(&a,&state);
        else if (inv.movePointer(x,y)) ++state.overCallbacks;
    }
    void click(int x,int y,Uint8 button=SDL_BUTTON_LEFT) {
        Action a=action(x,y,button);
        if (route==LegacyPhysical) inv.mouseClick(&a,&state);
        else if (route==SharedPhysical) inv.Inventory::mouseClick(&a,&state);
        else if (inv.activateButton(button,x,y,false)) ++state.clickCallbacks;
    }
    void primary(int x,int y) { hover(x,y);click(x,y); }
    bool cancel() {
        if (route==SharedSemantic) {
            const bool held=inv.returnHeldItem();
            if (held) ++state.clickCallbacks;
            return held;
        }
        bool held=inv._selItem!=nullptr;click(0,0,SDL_BUTTON_RIGHT);return held;
    }
    std::string snapshot() {
        std::ostringstream s;
        s<<unit.tu<<'|'<<unit.spendCalls<<'|'<<state.overCallbacks<<'|'<<state.clickCallbacks<<'|';
        s<<(inv._selItem?inv._selItem->label:"-")<<'|'<<(inv._mouseOverItem?inv._mouseOverItem->label:"-")<<'|';
        s<<inv.selection.x<<','<<inv.selection.y<<'|'<<inv.drawCalls<<'|'<<inv.arrangeCalls<<'|';
        s<<game.pushes<<','<<game.pops<<'|'<<game.mod.sounds[0].plays<<','<<game.mod.sounds[1].plays<<'|';
        for (auto *i:{&gun,&clip,&wrong,&spare,&grenade,&fixed}) {
            s<<i->label<<':'<<(i->slot?i->slot->id:"-")<<':'<<i->x<<','<<i->y<<':'<<(i->owner?"unit":"-")<<':'<<(i->ammo?i->ammo->label:"-")<<':'<<i->fuse<<';';
        }
        for (auto *i:unit.items) s<<"U:"<<i->label<<';';
        for (auto *i:unit.tile.items) s<<"G:"<<i->label<<';';
        for (auto &x:inv._stackLevel) for (auto &y:x.second) s<<"S:"<<x.first<<','<<y.first<<'='<<y.second<<';';
        for (auto &w:inv.warning.messages) s<<"W:"<<w<<';';
        return s.str();
    }
};

static std::vector<std::string> ordinarySequence(Route route) {
    Fixture f(route);std::vector<std::string> trace;
    auto mark=[&](const char *label){trace.push_back(std::string(label)+"="+f.snapshot());};
    f.hover(16,24);check(f.inv._mouseOverItem==&f.gun,"ordinary hover finds hand weapon");mark("hover-gun");
    f.primary(64,24);check(f.inv._selItem==&f.clip,"ordinary pickup holds clip");mark("pickup-clip");
    f.primary(16,24);check(f.gun.ammo==&f.clip && !f.inv._selItem,"ordinary reload consumes held clip");
    check(f.unit.tu==65 && f.unit.spendCalls==1 && f.game.mod.sounds[Mod::ITEM_RELOAD].plays==1,"reload spends 15 TU and plays exactly once");mark("reload");
    f.primary(16,24);check(f.inv._selItem==&f.gun,"loaded weapon pickup");mark("pickup-loaded");
    check(f.inv.unload(),"ordinary unload accepted");
    check(!f.gun.ammo && f.clip.slot==&f.left && f.gun.slot==&f.right && !f.inv._selItem,"ordinary unload puts ammo left and weapon right");
    check(f.unit.tu==57 && f.unit.spendRequests.back()==8,"ordinary unload TU cost");mark("unload");
    f.primary(8,88);check(f.inv._selItem==&f.wrong && f.inv._stackLevel[0][0]==0,"ground pickup removes held stack count");mark("pickup-wrong");
    f.primary(16,24);check(f.inv.warning.last()=="STR_WRONG_AMMUNITION_FOR_THIS_WEAPON" && f.inv._selItem==&f.wrong,"wrong-ammo warning preserves hold");
    check(f.unit.tu==57 && f.unit.spendCalls==2,"wrong ammo does not spend TU");mark("wrong-ammo");
    check(f.cancel() && !f.inv._selItem && f.inv._stackLevel[0][0]==1,"held cancel restores exact ground stack");
    check(f.game.pops==0 && f.game.pushes==0,"held cancel never closes or primes");mark("cancel-wrong");
    f.primary(64,24);f.primary(16,24);check(f.gun.ammo==&f.clip && f.unit.tu==42,"reload after ordinary warning/cancel works");mark("reload-after-cancel");
    f.primary(40,88);f.primary(16,24);check(f.inv.warning.last()=="STR_WEAPON_IS_ALREADY_LOADED" && f.inv._selItem==&f.spare,"already-loaded warning preserves held clip");
    check(f.unit.tu==42,"already-loaded warning spends no TU");mark("already-loaded");
    check(f.cancel(),"cancel already-loaded refusal");mark("cancel-spare");
    f.primary(16,24);check(f.inv.unload(),"second ordinary unload");f.unit.tu=14;
    f.primary(64,24);f.primary(16,24);check(f.inv.warning.last()=="STR_NOT_ENOUGH_TIME_UNITS" && f.inv._selItem==&f.clip && !f.gun.ammo,"reload TU refusal preserves both items");
    check(f.unit.tu==14 && f.unit.spendRequests.back()==15,"failed reload spends nothing");mark("reload-tu-refusal");
    check(f.cancel() && f.clip.slot==&f.left,"held hand cancellation preserves original slot");mark("cancel-hand");
    f.primary(64,24);f.primary(136,88);check(!f.inv._selItem && f.clip.slot==&f.ground && f.clip.x==8 && f.clip.y==0,"normal drop uses painted selection anchor");
    check(f.unit.tu==10 && f.inv._stackLevel[8][0]==1,"normal drop TU and stack accounting");mark("drop-ground");
    f.primary(136,88);f.unit.tu=3;f.primary(64,24);check(f.inv.warning.last()=="STR_NOT_ENOUGH_TIME_UNITS" && f.inv._selItem==&f.clip && f.clip.slot==&f.ground,"drop TU refusal keeps held item in original ground cell");mark("drop-tu-refusal");
    check(f.cancel() && f.inv._stackLevel[8][0]==1,"cancel after drop refusal restores stack");mark("cancel-drop-refusal");
    f.primary(104,88);check(!f.inv._selItem && !f.inv._mouseOverItem,"fixed item cannot hover/pick up");mark("fixed");
    f.primary(300,60);check(!f.inv._selItem && !f.inv._mouseOverItem,"outside slots is ordinary no-op");mark("outside");
    return trace;
}

static void extraBranches(Route route) {
    Fixture f(route);
    int before=f.state.overCallbacks;
    f.inv._selUnit=nullptr;f.hover(16,24);f.click(16,24);
    check(f.state.overCallbacks==before && f.state.clickCallbacks==0,"no selected unit suppresses physical/semantic callback tails");
    check(f.inv.selection.x==0 && f.inv.selection.y==0,"no unit still updates ordinary pointer origin");
    f.inv._selUnit=&f.unit;
    check(!f.inv.unload(),"unload no-held is refused");
    f.primary(16,24);check(!f.inv.unload() && f.inv.warning.last()=="STR_NO_AMMUNITION_LOADED","unloaded gun warns through ordinary unload");f.cancel();
    f.primary(64,24);f.primary(16,24);f.primary(16,24);
    f.place(f.spare,f.left);check(!f.inv.unload() && f.inv.warning.last()=="STR_BOTH_HANDS_MUST_BE_EMPTY","occupied other hand blocks unload");
    f.place(f.spare,f.ground,2,0);f.unit.tu=7;check(!f.inv.unload() && f.inv.warning.last()=="STR_NOT_ENOUGH_TIME_UNITS" && f.gun.ammo==&f.clip,"unload TU refusal preserves loaded weapon");f.cancel();
    f.grenade.fuse=2;f.primary(72,88);check(f.inv.warning.last()=="STR_GRENADE_IS_ACTIVATED","pickup live grenade keeps ordinary warning");
    int pops=f.game.pops,pushes=f.game.pushes;f.cancel();check(f.grenade.fuse==2 && f.game.pops==pops && f.game.pushes==pushes,"held grenade cancel cannot prime/unprime or close");
}

int main() {
    const auto legacy=ordinarySequence(LegacyPhysical);
    const auto physical=ordinarySequence(SharedPhysical);
    const auto semantic=ordinarySequence(SharedSemantic);
    check(legacy.size()==physical.size() && physical.size()==semantic.size(),"same sequence checkpoint count");
    for (std::size_t i=0;i<legacy.size();++i) {
        check(legacy[i]==physical[i],"/10 physical versus /11 physical at checkpoint "+std::to_string(i));
        check(legacy[i]==semantic[i],"/10 physical versus /11 semantic core at checkpoint "+std::to_string(i));
    }
    extraBranches(LegacyPhysical);extraBranches(SharedPhysical);extraBranches(SharedSemantic);
    Fixture semanticOnly(SharedSemantic);semanticOnly.inv._tu=false;
    const auto before=semanticOnly.snapshot();
    check(!semanticOnly.cancel(),"held-only cancel refuses empty hold");
    check(before==semanticOnly.snapshot(),"empty cancel has no mutation, callback, close, or prime");
    Fixture emptyTuCancel(SharedSemantic);
    const auto beforeTuCancel=emptyTuCancel.snapshot();
    check(!emptyTuCancel.cancel() && beforeTuCancel==emptyTuCancel.snapshot(),"empty cancel in TU mode cannot close inventory");
    // Control experiment: the preserved ordinary right-click branch can close
    // or prime when empty, so semantic cancel must never call that branch.
    Fixture ordinaryRight(SharedPhysical);ordinaryRight.click(0,0,SDL_BUTTON_RIGHT);
    check(ordinaryRight.game.pops==1,"ordinary empty right-click closes in TU mode");
    ordinaryRight.inv._tu=false;ordinaryRight.click(72,88,SDL_BUTTON_RIGHT);
    check(ordinaryRight.game.pushes==1,"ordinary empty right-click can open grenade prime state");
    // Held drop uses the selection anchor set by hover, not the activation
    // arguments; this catches any shortcut that bypasses movePointer.
    Fixture anchor(SharedSemantic);anchor.primary(64,24);anchor.hover(136,88);anchor.click(16,24);
    check(anchor.clip.slot==&anchor.ground && anchor.clip.x==8,"shared activation preserves ordinary held-selection anchor");
    // Same-width clips stack through the ordinary branch. Ground weapon reload
    // must call the real reload branch and then the declared arrangement shim.
    for (Route route:{LegacyPhysical,SharedPhysical,SharedSemantic}) {
        Fixture stacked(route);stacked.primary(64,24);stacked.primary(40,88);
        check(!stacked.inv._selItem && stacked.clip.slot==&stacked.ground && stacked.clip.x==2 && stacked.inv._stackLevel[2][0]==2,"ordinary ground stacking keeps both items");
        Fixture grounded(route);grounded.place(grounded.gun,grounded.ground,8,0);grounded.inv.arrangeGround();
        grounded.primary(64,24);const int arrangements=grounded.inv.arrangeCalls;grounded.primary(136,88);
        check(grounded.gun.ammo==&grounded.clip && !grounded.inv._selItem && grounded.unit.tu==65,"ordinary reload into ground weapon");
        check(grounded.inv.arrangeCalls==arrangements+1,"ground reload requests ordinary arrangement once");
    }
    std::cout<<"AgentInventorySharedRoutingTest PASS "<<checks<<" checks, "<<legacy.size()<<" three-way sequence checkpoints\n";
    return 0;
}
