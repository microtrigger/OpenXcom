// SPDX-License-Identifier: GPL-3.0-or-later
// Literal production projection/freshness methods; explicit Tile/Game/Camera seams.
#include "../../src/Battlescape/CameraProjection.h"
#include <iostream>
#include <cstdlib>
#include <algorithm>
using namespace OpenXcom;
namespace OpenXcom {
struct Position { int x,y,z; Position(int a=0,int b=0,int c=0):x(a),y(b),z(c){} bool operator!=(const Position&o)const{return x!=o.x||y!=o.y||z!=o.z;}bool operator==(const Position&o)const{return !(*this!=o);} };
enum TilePart {O_FLOOR,O_WESTWALL,O_NORTHWALL,O_OBJECT};
enum {FACTION_PLAYER,FACTION_HOSTILE,FACTION_NEUTRAL,STATUS_STANDING,STATUS_WALKING,STATUS_FLYING};
struct MapData { bool door=false,ufo=false; bool isDoor()const{return door;}bool isUFODoor()const{return ufo;} };
struct BattleUnit { bool visible=false,cache=true; int faction=FACTION_HOSTILE,status=STATUS_STANDING; Position position; bool getVisible()const{return faction==FACTION_PLAYER||visible;} int getFaction()const{return faction;}int getStatus()const{return status;} const void* getCache(int)const{return cache?this:nullptr;}Position getPosition()const{return position;} };
struct Tile { bool discovered[3]={false,false,false},sprites[4]={true,true,true,true};MapData*data[4]={nullptr,nullptr,nullptr,nullptr};BattleUnit*unit=nullptr;bool isDiscovered(int i)const{return discovered[i];}MapData*getMapData(TilePart i)const{return data[i];}bool isVoid()const{return !data[0]&&!data[1]&&!data[2]&&!data[3];}const void*getSprite(int i)const{return data[i]&&sprites[i]?this:nullptr;}BattleUnit*getUnit()const{return unit;} };
struct Save { bool debug=false;Tile tile;BattleUnit*selected=nullptr;bool getDebugMode()const{return debug;}BattleUnit*getSelectedUnit()const{return selected;}Tile*getTile(Position p){return p.x==0&&p.y==0&&p.z==0?&tile:nullptr;} };
struct Camera { unsigned revision=4;unsigned agentViewRevision()const{return revision;} };
namespace Observer { bool healthy=true;bool agentPaintHealthy(){return healthy;} }
struct Map {
 bool _redraw=false,_agentDrawSnapshotReady=true,visible=true;Save store;Save*_save=&store;Camera camera;Camera*_camera=&camera;unsigned _agentDrawViewRevision=4;int _agentDrawSurfaceHeight=200,_visibleMapHeight=140;AgentMapSnapshot _agentDrawSnapshot;std::vector<AgentMapCell>_agentTerrainCells;BattleUnit*_agentContextActor=nullptr;bool _agentContextOwnPosition=false;AgentMapPosition _agentContextPosition;
 bool agentContextVisible()const{return visible;}int getX()const{return 0;}int getY()const{return 0;}int getWidth()const{return 320;}int getHeight()const{return 200;}
 AgentMapCell agentDescribeTerrain(Tile*,const AgentMapPosition&)const;void agentRecordTerrain(Tile*,const Position&);bool agentMapSnapshot(AgentMapSnapshot&)const;
 void seal(){_agentDrawSnapshot.x=0;_agentDrawSnapshot.y=0;_agentDrawSnapshot.width=320;_agentDrawSnapshot.height=140;_agentDrawSnapshot.cells={agentDescribeTerrain(&store.tile,AgentMapPosition())};_agentContextActor=store.selected;}
};
#include "MapContext.generated.inc"
}
unsigned checks=0;void check(bool ok,const char*why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}}
int main(){
 Map m;MapData floor,wall,object,door;door.door=true;Tile&t=m.store.tile;AgentMapPosition p;AgentMapSnapshot out;
 auto unknown=m.agentDescribeTerrain(&t,p);check(unknown.parts[0]=="unknown"&&unknown.parts[1]=="unknown","undiscovered absence unknown");
 t.data[0]=&floor;t.data[1]=&wall;t.data[3]=&object;check(m.agentDescribeTerrain(&t,p)==unknown,"undiscovered occupied terrain observational equality");
 t.discovered[0]=true;check(m.agentDescribeTerrain(&t,p)==unknown,"wall discovery alone does not disclose ordinary wall");
 t.data[1]=&door;check(m.agentDescribeTerrain(&t,p).parts[1]=="door","native discovered wall door branch");
 t.sprites[1]=false;check(m.agentDescribeTerrain(&t,p).parts[1]=="unknown","missing door sprite does not disclose");t.sprites[1]=true;
 t.discovered[1]=true;t.data[2]=&door;check(m.agentDescribeTerrain(&t,p).parts[2]=="door","north discovery1 door counterpart");t.data[2]=&wall;check(m.agentDescribeTerrain(&t,p).parts[2]=="unknown","north discovery1 regular wall unknown");t.data[2]=nullptr;
 AgentMapPosition edge;edge.x=-1;auto outside=m.agentDescribeTerrain(nullptr,edge);check(outside.position.x==-1&&!outside.rendered&&outside.unit=="unknown"&&outside.parts[0]=="unknown"&&outside.parts[3]=="unknown","literal offmap null tile all unknown");
 t.discovered[2]=true;auto known=m.agentDescribeTerrain(&t,p);check(known.parts[0]=="floor"&&known.parts[1]=="door"&&known.parts[2]=="empty"&&known.parts[3]=="object","four known parts distinguish absent from present");
 t.data[1]=&wall;check(m.agentDescribeTerrain(&t,p).parts[1]=="wall","generic wall never fabricated window");
 BattleUnit hidden;t.unit=&hidden;auto before=m.agentDescribeTerrain(&t,p);hidden.faction=FACTION_NEUTRAL;check(m.agentDescribeTerrain(&t,p)==before,"hidden unit faction does not alter output");t.unit=nullptr;check(m.agentDescribeTerrain(&t,p)==before,"hidden occupancy and empty unit output identical");
 t.unit=&hidden;hidden.visible=true;check(m.agentDescribeTerrain(&t,p).unit=="visible","visible nonown generic marker");hidden.faction=FACTION_PLAYER;check(m.agentDescribeTerrain(&t,p).unit=="own","own marker");hidden.status=STATUS_WALKING;check(m.agentDescribeTerrain(&t,p).unit=="unknown","moving marker conservatively withheld");hidden.status=STATUS_STANDING;hidden.position.x=1;check(m.agentDescribeTerrain(&t,p).unit=="unknown","secondary multi-tile footprint is not origin marker");hidden.position.x=0;
 m._agentTerrainCells.resize(1);m.agentRecordTerrain(&t,Position(1,0,0));check(!m._agentTerrainCells[0].rendered,"nonmatching level/position never populated");m.agentRecordTerrain(&t,Position());check(m._agentTerrainCells[0].rendered,"actual matching render branch populates");
 m.seal();check(m.agentMapSnapshot(out),"current snapshot admitted");m._redraw=true;check(!m.agentMapSnapshot(out),"dirty map rejected");m._redraw=false;m.camera.revision++;check(!m.agentMapSnapshot(out),"camera remap rejected");m.camera.revision--;m.visible=false;check(!m.agentMapSnapshot(out),"hidden widget rejected");m.visible=true;m.store.debug=true;check(!m.agentMapSnapshot(out),"debug path rejected");m.store.debug=false;
 t.data[3]=nullptr;check(!m.agentMapSnapshot(out),"changed public topology invalidates snapshot");m.seal();hidden.visible=false;hidden.faction=FACTION_HOSTILE;m.seal();hidden.faction=FACTION_NEUTRAL;check(m.agentMapSnapshot(out),"unseen enemy change does not alter readiness");
 BattleUnit own;own.faction=FACTION_PLAYER;m.store.selected=&own;m.seal();m._agentContextOwnPosition=true;m._agentContextPosition=AgentMapPosition();check(m.agentMapSnapshot(out),"own position current");own.position.x=1;check(!m.agentMapSnapshot(out),"selected movement invalidates snapshot");own.position.x=0;m.store.selected=nullptr;check(!m.agentMapSnapshot(out),"selection replacement rejected");
 std::cout<<"PASS "<<checks<<" semantic context checks; literal projection methods, explicit model/render seams\n";
}
