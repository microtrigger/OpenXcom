// SPDX-License-Identifier: GPL-3.0-or-later
// Dependency/model shims for run_inventory_paging.py. Actual production
// arrangeGround, stacking, routing, slot-fit and overlap bodies are extracted.
// No game, GUI, renderer, live acceptance or retail assets are executed.
// The runner records exact source/body hashes and all field-name substitutions.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>
#include "../../src/Observer/AgentUi.h"
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
struct Screen { enum { ORIGINAL_WIDTH=320, ORIGINAL_HEIGHT=200 }; };
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
    int x=0,y=0,fuse=-1,quantity=0,pain=0,heal=0,stim=0; bool isAmmo=false;
    BattleItem(std::string l,RuleItem *r):label(l),rules(r){}
    RuleItem *getRules() { return rules; }
    RuleInventory *getSlot() const { return slot; }
    int getSlotX() const { return x; } int getSlotY() const { return y; }
    void setSlot(RuleInventory *s) { slot=s; }
    void setSlotX(int v) { x=v; } void setSlotY(int v) { y=v; }
    int getFuseTimer() const { return fuse; } void setFuseTimer(int n) { fuse=n; }
    BattleItem *getAmmoItem() const { return ammo; }
    int setAmmoItem(BattleItem *item);
    void setIsAmmo(bool value) { isAmmo=value; }
    int getAmmoQuantity() const { return quantity; }
    bool needsAmmo() const;
    BattleUnit *getUnit() const { return unit; }
    int getPainKillerQuantity() const { return pain; }
    int getHealQuantity() const { return heal; }
    int getStimulantQuantity() const { return stim; }
    void setTurnFlag(bool) {}
    void moveToOwner(BattleUnit *);
    bool occupiesSlot(int x,int y,BattleItem *item=nullptr) const;
};
struct Tile {
    std::vector<BattleItem*> items;
    std::vector<BattleItem*> *getInventory() { return &items; }
    void removeItem(BattleItem *i) { items.erase(std::remove(items.begin(),items.end(),i),items.end()); }
    void addItem(BattleItem *i,RuleInventory *slot) { i->slot=slot; items.push_back(i); }
};
struct BattleUnit {
    std::vector<BattleItem*> items; Tile tile; Tile *_tile=&tile; int tu=80, spendCalls=0;
    std::vector<int> spendRequests; UnitStatus status=STATUS_STANDING; Position position;
    std::vector<BattleItem*> *getInventory() { return &items; }
    Tile *getTile() { return &tile; }
    UnitStatus getStatus() const { return status; }
    Position getPosition() const { return position; } void setPosition(Position p) { position=p; }
    BattleItem *getItem(RuleInventory *slot,int x,int y) const;
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
    std::uint64_t _agentInventoryRevision=0; int drawCalls=0;
    Inventory(Game *g,BattleUnit *u):_game(g),_selUnit(u){}
    void mouseOver(Action *,State *); void mouseClick(Action *,State *);
    bool movePointer(int,int); bool activateButton(Uint8,int,int,bool); bool returnHeldItem();
    bool unload(); void setSelectedItem(BattleItem *); void setMouseOverItem(BattleItem *);
    RuleInventory *getSlotInPosition(int *,int *) const;
    void moveItem(BattleItem *,RuleInventory *,int,int);
    static bool overlapItems(BattleUnit *,BattleItem *,RuleInventory *,int,int);
    bool canBeStacked(BattleItem *,BattleItem *);
    bool fitItem(RuleInventory *,BattleItem *,std::string &);
    // Only drawing is shimmed. Ground arrangement is the actual source body.
    void drawItems() { ++drawCalls; }
    void arrangeGround(bool=true);
};
} // namespace OpenXcom

#include "InventoryPagingActual.inc"
#include "InventoryStockLayouts.inc"

using namespace OpenXcom;
static unsigned checks=0;
static void check(bool yes,const std::string &message) {
    ++checks; if (!yes) { std::cerr<<"FAIL "<<message<<"\n"; std::exit(1); }
}

enum Route { Physical, Semantic };
struct Fixture {
    Game game; BattleUnit unit;
    std::vector<std::unique_ptr<RuleInventory>> slots;
    std::vector<std::unique_ptr<RuleItem>> rules;
    std::vector<std::unique_ptr<BattleItem>> items;
    Inventory inv{&game,&unit}; State state; Route route;
    explicit Fixture(const std::string &layout,Route r=Semantic):route(r) {
        loadStockLayout(layout,game.mod,slots);
    }
    RuleInventory &slot(const std::string &id) { return *game.mod.slots.at(id); }
    RuleItem &rule(const std::string &id,int w=1,int h=1,BattleType battle=BT_NONE) {
        rules.emplace_back(new RuleItem(id)); RuleItem &r=*rules.back();
        r.w=w;r.h=h;r.battle=battle;return r;
    }
    BattleItem &item(const std::string &id,RuleItem &rule,const std::string &where="STR_GROUND",int x=0,int y=0) {
        items.emplace_back(new BattleItem(id,&rule)); BattleItem &i=*items.back();
        place(i,slot(where),x,y);return i;
    }
    void place(BattleItem &item,RuleInventory &slot,int x=0,int y=0) {
        unit.tile.removeItem(&item);item.moveToOwner(nullptr);
        item.slot=&slot;item.x=x;item.y=y;
        if (slot.getType()==INV_GROUND) unit.tile.items.push_back(&item);
        else item.moveToOwner(&unit);
    }
    void hover(int x,int y) {
        if (route==Semantic) {if(inv.movePointer(x,y))++state.overCallbacks;}
        else {Action a;a.x=x+inv.getX()+0.75;a.y=y+inv.getY()+0.25;inv.mouseOver(&a,&state);}
    }
    void primary(int x,int y) {
        hover(x,y);
        if (route==Semantic) {if(inv.activateButton(SDL_BUTTON_LEFT,x,y,false))++state.clickCallbacks;}
        else {Action a;a.x=x+inv.getX()+0.75;a.y=y+inv.getY()+0.25;inv.mouseClick(&a,&state);}
    }
    void pickup(BattleItem &i) {
        int x=i.slot->getX()+8,y=i.slot->getY()+8;
        if(i.slot->getType()!=INV_HAND) {x+=(i.x-(i.slot->getType()==INV_GROUND?inv._groundOffset:0))*16;y+=i.y*16;}
        primary(x,y);check(inv._selItem==&i,"ordinary pickup selects expected item");
    }
    void drop(BattleItem &i,RuleInventory &target,int x=0,int y=0) {
        // The ordinary method derives top-left from its 2x3 selection surface.
        // Use the corresponding item-center pointer; never bypass movePointer.
        primary(target.getX()+x*16+i.rules->w*8,target.getY()+y*16+i.rules->h*8);
    }
    std::string snapshot() const {
        std::ostringstream s;s<<unit.tu<<'|'<<unit.spendCalls<<'|'<<inv._groundOffset<<'|';
        s<<state.overCallbacks<<','<<state.clickCallbacks<<'|'<<inv.drawCalls<<'|';
        s<<(inv._selItem?inv._selItem->label:"-")<<'|'<<(inv._mouseOverItem?inv._mouseOverItem->label:"-")<<'|';
        for(const auto &i:items) s<<i->label<<':'<<i->slot->id<<':'<<i->x<<','<<i->y<<':'<<(i->owner?"unit":"-")<<':'<<(i->ammo?i->ammo->label:"-")<<':'<<i->isAmmo<<';';
        for(auto*i:unit.items)s<<"U:"<<i->label<<';';
        for(auto*i:unit.tile.items)s<<"G:"<<i->label<<';';
        for(const auto&x:inv._stackLevel)for(const auto&y:x.second)s<<"S:"<<x.first<<','<<y.first<<'='<<y.second<<';';
        for(const auto&w:inv.warning.messages)s<<"W:"<<w<<';';
        s<<game.mod.sounds[0].plays<<','<<game.mod.sounds[1].plays;
        return s.str();
    }
};

static void generationChanged(std::uint64_t before,const Inventory &inv,const char *why) {
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    check(inv._agentInventoryRevision>before,why);
#else
    check(inv._agentInventoryRevision==before,std::string("agent-off has no generation side effect: ")+why);
#endif
}

static void stockLayout(const std::string &name) {
    Fixture f(name);check(f.slots.size()==9,"public stock YAML has nine inventory sections");
    RuleInventory &g=f.slot("STR_GROUND");
    check(g._x==0&&g._y==152&&(320-g._x)/16==20&&(200-g._y)/16==3,"public ground is 20 by 3 visible cells");
    check(f.slot("STR_BELT")._slots.size()==6,"public belt preserves six-cell notched geometry");
    check(f.slot("STR_BACK_PACK")._slots.size()==9,"public backpack preserves 3 by 3 cells");
    check(f.slot("STR_RIGHT_HAND").getCost(&g)==2&&g.getCost(&f.slot("STR_RIGHT_HAND"))==8,"directional public TU costs retained");
    RuleItem &small=f.rule("small"),&tall=f.rule("tall",1,2),&gun=f.rule("gun",2,3);
    auto &belt=f.slot("STR_BELT"),&pack=f.slot("STR_BACK_PACK");
    check(belt.fitItemInSlot(&small,1,0)&&!belt.fitItemInSlot(&small,1,1),"actual fit recognizes belt top row and lower notch");
    check(belt.fitItemInSlot(&tall,0,0)&&!belt.fitItemInSlot(&tall,1,0),"actual multi-cell fit cannot bridge belt notch");
    check(pack.fitItemInSlot(&gun,1,0)&&!pack.fitItemInSlot(&gun,2,0),"actual backpack boundary fit");
    check(g.fitItemInSlot(&gun,18,0)&&!g.fitItemInSlot(&gun,19,0)&&g.fitItemInSlot(&gun,20,0),"actual ground fit respects page boundaries");
}

static void stacking(const std::string &name) {
    Fixture f(name);auto &gun=f.rule("gun",2,3,BT_FIREARM),&clip=f.rule("clip"),&other=f.rule("other");gun.ammo={"clip"};
    auto &a=f.item("a",gun),&b=f.item("b",gun),&c=f.item("c",other);
    check(!f.inv.canBeStacked(nullptr,&a)&&!f.inv.canBeStacked(&a,nullptr),"null item cannot stack");
    check(f.inv.canBeStacked(&a,&b)&&!f.inv.canBeStacked(&a,&c),"identical unloaded guns stack; other rules do not");
    auto &aa=f.item("aa",clip),&bb=f.item("bb",clip);f.unit.tile.removeItem(&aa);f.unit.tile.removeItem(&bb);
    aa.quantity=7;bb.quantity=7;check(a.setAmmoItem(&aa)==0&&b.setAmmoItem(&bb)==0,"actual ammo setter accepts matching type");
    check(f.inv.canBeStacked(&a,&b),"same loaded ammunition quantity stacks");bb.quantity=8;
    check(!f.inv.canBeStacked(&a,&b),"different loaded ammunition quantities do not stack");bb.quantity=7;
    b.setAmmoItem(nullptr);check(!f.inv.canBeStacked(&a,&b),"loaded versus unloaded cannot stack");b.setAmmoItem(&bb);
    b.fuse=0;check(!f.inv.canBeStacked(&a,&b),"armed items cannot stack");b.fuse=-1;
    b.unit=&f.unit;check(!f.inv.canBeStacked(&a,&b),"body-associated items cannot stack");b.unit=nullptr;
    b.heal=1;check(!f.inv.canBeStacked(&a,&b),"different medical charges cannot stack");b.heal=0;
    a.setAmmoItem(nullptr);b.setAmmoItem(nullptr);
    f.inv.arrangeGround(false);check(a.x==b.x&&a.y==b.y&&f.inv._stackLevel[a.x][a.y]==2,"real ground packer stacks identical guns");
}

static void paging(const std::string &name) {
    Fixture f(name);auto &gun=f.rule("gun",2,3,BT_FIREARM),&clip=f.rule("clip");gun.ammo={"clip"};
    std::vector<BattleItem*>guns,small;
    for(int n=0;n<10;++n) {
        auto &g=f.item("gun"+std::to_string(n),gun);
        auto &a=f.item("ammo"+std::to_string(n),clip);f.unit.tile.removeItem(&a);a.quantity=n+1;
        check(g.setAmmoItem(&a)==0,"unique ammunition quantities are loaded through actual setter");guns.push_back(&g);
    }
    for(int n=0;n<3;++n)small.push_back(&f.item("small"+std::to_string(n),f.rule("small-rule"+std::to_string(n))));
    f.inv._groundOffset=999;auto before=f.inv._agentInventoryRevision;f.inv.arrangeGround();
    generationChanged(before,f.inv,"actual arrangeGround advances private generation");
    check(f.inv._groundOffset==0,"selection-style large offset wraps to first page");
    check(f.unit.tile.items.size()==13,"arrangement preserves all thirteen physical items");
    for(unsigned n=0;n<guns.size();++n)check(guns[n]->x==int(n*2)&&guns[n]->y==0,"ten nonstacking 2x3 guns occupy complete first page");
    for(unsigned n=0;n<small.size();++n)check(small[n]->x==20&&small[n]->y==int(n),"three 1x1 items occupy second page column");
    std::set<int>pages;for(auto*i:f.unit.tile.items)pages.insert(i->x/20);
    check(pages==std::set<int>({0,1}),"real packing produces exactly two populated pages");
    f.inv.arrangeGround();check(f.inv._groundOffset==20,"ordinary next page advances by twenty slots");
    before=f.inv._agentInventoryRevision;f.inv.arrangeGround(false);
    check(f.inv._groundOffset==20,"alterOffset=false preserves current second page");
    generationChanged(before,f.inv,"same-page rearrangement invalidates old private generation");
    f.inv.arrangeGround();check(f.inv._groundOffset==0,"ordinary next page wraps from second page");
    f.inv.arrangeGround();check(f.inv._groundOffset==20,"ordinary page can revisit second page");
    // Current-page coordinates are translated through real movePointer/primary.
    f.primary(8,160);check(f.inv._selItem==small[0],"same screen cell on page two selects physical x=20");
    check(f.inv.returnHeldItem(),"held return restores page-two item");
    f.inv.arrangeGround();f.primary(8,160);check(f.inv._selItem==guns[0],"same screen cell after wrap selects physical x=0");
    check(f.inv.returnHeldItem(),"held return after page wrap succeeds");
    // Preserve the actual legacy >= behavior at exact page capacity, rather
    // than silently replacing it with an idealized two-page stub.
    for(auto*i:small)f.unit.tile.removeItem(i);
    f.inv._groundOffset=0;f.inv.arrangeGround();
    check(f.inv._groundOffset==20,"exact-full page retains ordinary >= advance to empty next page");
    f.inv.arrangeGround();check(f.inv._groundOffset==0,"exact-full empty page wraps normally");
    Fixture one(name);one.item("only",one.rule("only"));one.inv.arrangeGround(false);
    before=one.inv._agentInventoryRevision;one.inv.arrangeGround();
    check(one.inv._groundOffset==0,"single-item page arrow is a visible no-op");
    generationChanged(before,one.inv,"single-page no-op still changes private generation");
    one.inv._selUnit=nullptr;one.inv._groundOffset=20;one.inv.arrangeGround();
    check(one.inv._groundOffset==0&&one.inv._stackLevel.empty(),"no selected unit resets offset and stack record safely");
}

static std::vector<std::string> bodyRouting(const std::string &name,Route route) {
    Fixture f(name,route);f.unit.tu=1000;
    auto &small=f.rule("small"),&tall=f.rule("tall",1,2),&gunRule=f.rule("gun",2,3,BT_FIREARM),&clipRule=f.rule("clip"),&wrongRule=f.rule("wrong");gunRule.ammo={"clip"};
    auto &object=f.item("object",small,"STR_RIGHT_HAND");
    std::vector<std::string>trace;auto mark=[&](){trace.push_back(f.snapshot());};
    for(const std::string id:{"STR_RIGHT_SHOULDER","STR_LEFT_SHOULDER","STR_RIGHT_LEG","STR_LEFT_LEG","STR_BELT","STR_BACK_PACK"}) {
        auto &target=f.slot(id);int tu=f.unit.tu,cost=object.slot->getCost(&target);f.pickup(object);f.drop(object,target);
        check(!f.inv._selItem&&object.slot==&target&&object.x==0&&object.y==0,"ordinary primary places small item in "+id);
        check(f.unit.tu==tu-cost,"body move charges exact public directional cost");mark();
    }
    f.pickup(object);f.drop(object,f.slot("STR_GROUND"),7,1);
    check(!f.inv._selItem&&object.slot==&f.slot("STR_GROUND")&&object.x==7&&object.y==1,"body-to-ground ordinary drop uses item center anchor");mark();
    auto &longItem=f.item("tall",tall,"STR_RIGHT_HAND");
    f.pickup(longItem);auto warnings=f.inv.warning.messages.size();int tu=f.unit.tu,calls=f.unit.spendCalls;
    f.drop(longItem,f.slot("STR_BELT"),1,0);
    check(f.inv._selItem==&longItem&&longItem.slot==&f.slot("STR_RIGHT_HAND"),"tall item remains held when belt footprint spans notch");
    check(f.inv.warning.messages.size()==warnings&&f.unit.tu==tu&&f.unit.spendCalls==calls,"ordinary shape no-fit is silent and costs no TU");mark();
    f.drop(longItem,f.slot("STR_BELT"),0,0);check(!f.inv._selItem&&longItem.slot==&f.slot("STR_BELT"),"same tall item fits complete belt end column");mark();
    auto &gun=f.item("gun",gunRule,"STR_RIGHT_HAND"),&clip=f.item("clip",clipRule,"STR_LEFT_HAND"),&wrong=f.item("wrong",wrongRule,"STR_BACK_PACK",2,0);
    clip.quantity=6;f.pickup(wrong);f.drop(wrong,f.slot("STR_RIGHT_HAND"));
    check(f.inv.warning.last()=="STR_WRONG_AMMUNITION_FOR_THIS_WEAPON"&&f.inv._selItem==&wrong&&!gun.ammo,"body pickup reaches ordinary wrong-ammunition refusal");mark();f.inv.returnHeldItem();
    f.pickup(clip);f.drop(clip,f.slot("STR_RIGHT_HAND"));
    check(gun.ammo==&clip&&!f.inv._selItem&&clip.isAmmo,"ordinary hand reload uses actual ammo setter");mark();
    auto &spare=f.item("spare",clipRule,"STR_BACK_PACK",0,0);f.pickup(spare);tu=f.unit.tu;
    f.drop(spare,f.slot("STR_RIGHT_HAND"));
    check(f.inv.warning.last()=="STR_WEAPON_IS_ALREADY_LOADED"&&f.inv._selItem==&spare&&f.unit.tu==tu,"body-carried spare reaches ordinary already-loaded refusal");mark();f.inv.returnHeldItem();
    f.place(spare,f.slot("STR_LEFT_HAND"));f.pickup(gun);tu=f.unit.tu;
    check(!f.inv.unload()&&f.inv.warning.last()=="STR_BOTH_HANDS_MUST_BE_EMPTY"&&gun.ammo==&clip&&f.unit.tu==tu,"unload refuses occupied second hand without mutation");mark();
    f.place(spare,f.slot("STR_BACK_PACK"),0,0);f.unit.tu=7;
    check(!f.inv.unload()&&f.inv.warning.last()=="STR_NOT_ENOUGH_TIME_UNITS"&&gun.ammo==&clip&&f.unit.tu==7,"ordinary unload TU refusal preserves loaded weapon");mark();
    f.unit.tu=8;check(f.inv.unload()&&!gun.ammo&&!clip.isAmmo&&clip.slot==&f.slot("STR_LEFT_HAND")&&gun.slot==&f.slot("STR_RIGHT_HAND")&&!f.inv._selItem&&f.unit.tu==0,"ordinary unload spends eight TU and clears real ammo state");mark();
    f.pickup(gun);check(!f.inv.unload()&&f.inv.warning.last()=="STR_NO_AMMUNITION_LOADED","ordinary empty-weapon unload warning");mark();f.inv.returnHeldItem();
    f.pickup(clip);f.unit.tu=14;f.drop(clip,f.slot("STR_RIGHT_HAND"));
    check(f.inv.warning.last()=="STR_NOT_ENOUGH_TIME_UNITS"&&f.inv._selItem==&clip&&!gun.ammo&&f.unit.tu==14,"ordinary reload TU refusal preserves clip");mark();f.inv.returnHeldItem();
    f.pickup(clip);f.unit.tu=0;f.drop(clip,f.slot("STR_RIGHT_SHOULDER"));
    check(f.inv.warning.last()=="STR_NOT_ENOUGH_TIME_UNITS"&&f.inv._selItem==&clip&&clip.slot==&f.slot("STR_LEFT_HAND")&&f.unit.tu==0,"body destination ordinary drop TU refusal");mark();
    f.inv.returnHeldItem();check(!f.inv.unload(),"ordinary unload without hold returns false");
    return trace;
}

static void privateBindingAndPayload(const std::string &name) {
    using namespace OpenXcom::Observer;
    Fixture f(name);f.item("private-offscreen-item",f.rule("private-rule"));f.inv.arrangeGround(false);
    AgentUiState shown;shown.screen="inventory";shown.visible="{\"targets\":[{\"handle\":\"v0\",\"area\":\"ground\",\"item\":\"unknown\"}]}";
    shown.actor=&f.unit;shown.save=&f.game;shown.points={{8,160}};
    shown.widgets.push_back({&f.inv,f.inv._agentInventoryRevision});
    const auto original=shown;const auto payload=shown.payload();f.inv.arrangeGround();
    shown.widgets[0].second=f.inv._agentInventoryRevision;
    check(shown.payload()==payload&&shown.authorityPayload()==original.authorityPayload(),"actual DTO serialization excludes private no-op generation");
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    check(!shown.sameBinding(original),"actual sameBinding rejects no-op paging stale generation");
#else
    check(shown.sameBinding(original),"agent-off packing does not invent a binding generation");
#endif
    shown.context={994481,13,2};shown.rows={888337};shown.points={{288,184}};shown.actorRevision=556731;
    check(shown.payload()==payload,"actual DTO serializer omits private context, rows, coordinates and actor revisions");
    for(const char *secret:{"private-offscreen-item","private-rule","994481","888337","556731","ground_offset","page_count","stack_count","inventory_revision"})
        check(shown.payload().find(secret)==std::string::npos,std::string("no private field in actual DTO payload: ")+secret);
}

int main() {
    for(const std::string name:{"xcom1","xcom2"}) {
        stockLayout(name);stacking(name);paging(name);
        const auto physical=bodyRouting(name,Physical),semantic=bodyRouting(name,Semantic);
        check(physical.size()==semantic.size(),"physical and semantic body route checkpoint counts match");
        for(unsigned n=0;n<physical.size();++n)check(physical[n]==semantic[n],"physical versus semantic body route parity checkpoint "+std::to_string(n));
        privateBindingAndPayload(name);
        std::cout<<name<<" actual ground packing/body routing PASS "<<physical.size()<<" paired checkpoints\n";
    }
    std::cout<<"AgentInventoryPagingTest PASS "<<checks<<" checks\n";
}
