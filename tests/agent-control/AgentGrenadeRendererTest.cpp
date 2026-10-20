// SPDX-License-Identifier: GPL-3.0-or-later
// Exact two Map branch bodies; real Surface/shader writes/final-copy witness.
// Game, camera, FOV predicates, projectile/explosion objects are explicit shims.
// No full Map/Game, AI, RNG, assets, SDL_Flip or real battle is executed.
#include "../../src/Engine/Surface.h"
#include "../../src/Observer/AgentProjectileOutput.h"
#include "../../src/Observer/AgentPublicEvents.h"
#include "../../src/Observer/AgentUi.h"
#include <cstdlib>
#include <iostream>
#include <list>
using namespace OpenXcom;using namespace OpenXcom::Observer;
static unsigned checks=0;static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
namespace Fixture {
struct Position {int x=0,y=0,z=0;Position(int a=0,int b=0,int c=0):x(a),y(b),z(c){}};
struct TileEngine {bool body=true,shadow=true;unsigned visibleCalls=0;int castedShade(Position){return 1;}bool isVoxelVisible(Position p){++visibleCalls;return p.z==1?shadow:body;}};
struct Saved {TileEngine engine;TileEngine*getTileEngine(){return &engine;}};
struct Camera {int x=24,y=32;void convertVoxelToScreen(Position,Position*out){*out=Position(x,y,0);}};
struct Projectile {Surface*sprite;bool item=true;Position pos;bool getItem(){return item;}Surface*getSprite(){return sprite;}Position getPosition(){return pos;}};
struct Explosion {bool big=true,hit=false;int frame=0;Position getPosition()const{return Position();}bool isBig()const{return big;}bool isHit()const{return hit;}int getCurrentFrame()const{return frame;}};
struct Set {Surface*s;Surface*getFrame(int){return s;}};
struct Mod {Set set;Set*getSurfaceSet(const std::string&){return &set;}};
struct Game {Mod mod;Mod*getMod(){return &mod;}};
struct Map {
 Saved*_save;Camera*_camera;Game*_game;Projectile*_projectile;AgentProjectileRaster _agentProjectileRaster;
 bool _projectileInFOV=true,_explosionInFOV=true,_flashScreen=false;std::list<Explosion*>_explosions;
 void thrown(Surface*surface){Surface*tmpSurface=nullptr;Position bulletPositionScreen;int itX=0,itY=0,itZ=0;
#include "AgentThrownRenderer.generated.inc"
 }
 void explosion(Surface*surface){Surface*tmpSurface=nullptr;Position bulletPositionScreen;
#include "AgentLargeExplosionRenderer.generated.inc"
 }
};
}
int main(){
 using Fact=AgentProjectileFact;using Role=AgentRendererRole;
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");
 Surface map(64,64),logical(64,64),sprite(3,3),blank(3,3);SDL_Color colors[256];for(int n=0;n<256;++n)colors[n]={Uint8(n),Uint8(n),Uint8(n),0};for(Surface*s:{&map,&logical,&sprite,&blank})s->setPalette(colors);sprite.setPixel(1,1,50);
 SDL_Surface*final=SDL_CreateRGBSurface(0,128,128,8,0,0,0,0);check(final,"integer output surface");SDL_SetColors(final,colors,0,256);
 AgentProjectileWitness p,i,c,t,e;std::array<AgentProjectileWitness*,5> ws={{&p,&i,&c,&t,&e}};AgentProjectileOutput output;
 Fixture::Saved save;Fixture::Camera camera;Fixture::Game game;Fixture::Projectile projectile;projectile.sprite=&sprite;Fixture::Map view;view._save=&save;view._camera=&camera;view._game=&game;view._projectile=&projectile;game.mod.set.s=&sprite;map.agentAttachProjectileRaster(&view._agentProjectileRaster);
 Fixture::Explosion explosion;view._explosions.push_back(&explosion);
 auto begin=[&](){map.clear(1);check(view._agentProjectileRaster.begin(ws,map.getSurface()->pixels,64,64,map.getSurface()->pitch),"five bounded witnesses begin");};
 auto finish=[&](bool success=true){for(auto*w:ws)w->seal(agentPaintHealthy());map.blit(&logical);for(int y=0;y<128;++y)for(int x=0;x<128;++x)static_cast<Uint8*>(final->pixels)[y*final->pitch+x]=logical.getPixel(x/2,y/2);return output.capture(success,false,logical.getSurface(),final,map.getSurface(),0,0,0,0);};
 auto fact=[&](AgentProjectileWitness&w,bool admitted=true){return w.evaluate(admitted,{0,0,64,64},{},output.palette,[&](int x,int y,std::uint32_t rgb){return output.matches(x,y,rgb);});};
 begin();view.thrown(&map);check(finish()&&fact(t)==Fact::Presented&&fact(e)==Fact::KnownAbsent,"ordinary visible thrown body contributes");
 save.engine.body=false;begin();view.thrown(&map);check(finish()&&fact(t)==Fact::KnownAbsent,"ordinary shadow-only draws no thrown event");save.engine.body=true;
 view._projectileInFOV=false;begin();view.thrown(&map);check(finish()&&fact(t)==Fact::KnownAbsent,"ordinary outer FOV gate retained");view._projectileInFOV=true;
 projectile.item=false;begin();view.thrown(&map);check(finish()&&fact(t)==Fact::KnownAbsent,"non-item projectile excluded from thrown branch");projectile.item=true;
 camera.x=-100;begin();view.thrown(&map);check(finish()&&fact(t)==Fact::KnownAbsent,"offscreen thrown body absent");camera.x=24;
 begin();view.explosion(&map);check(finish()&&fact(e)==Fact::Presented&&fact(t)==Fact::KnownAbsent,"ordinary X1 body contributes");
 camera.x=-100;begin();view.explosion(&map);check(finish()&&fact(e)==Fact::KnownAbsent,"fully offscreen X1 contributes nothing");camera.x=24;
 Fixture::Explosion overlap;view._explosions.push_back(&overlap);begin();view.explosion(&map);check(finish()&&fact(e)==Fact::Presented,"overlapping X1 draws retain one presence fact without object grouping");view._explosions.pop_back();
 const auto fovCalls=save.engine.visibleCalls;begin();view.explosion(&map);finish();check(save.engine.visibleCalls==fovCalls,"large effect adds no voxel visibility oracle");
 explosion.frame=-1;begin();view.explosion(&map);check(finish()&&fact(e)==Fact::KnownAbsent,"negative X1 frame excluded");explosion.frame=0;
 view._flashScreen=true;begin();view.explosion(&map);check(finish()&&fact(e)==Fact::KnownAbsent&&!view._flashScreen,"ordinary flash-only frame excluded");
 view._explosionInFOV=false;begin();view.explosion(&map);check(finish()&&fact(e)==Fact::KnownAbsent,"aggregate ordinary explosion gate retained");view._explosionInFOV=true;
 explosion.big=false;explosion.hit=true;begin();view.explosion(&map);check(finish()&&fact(e)==Fact::KnownAbsent&&fact(i)==Fact::KnownAbsent,"HIT is not a large or impact category");
 explosion.hit=false;begin();view.explosion(&map);check(finish()&&fact(e)==Fact::KnownAbsent&&fact(i)==Fact::Presented,"SMOKE remains separate impact kind");explosion.big=true;
 for(unsigned role=0;role<5;++role){
  for(unsigned later=0;later<5;++later){begin();view._agentProjectileRaster.role(Role(role));sprite.blitNShade(&map,4,4,0);view._agentProjectileRaster.role(Role(later));sprite.blitNShade(&map,4,4,0);view._agentProjectileRaster.role(Role::Ordinary);finish();for(unsigned k=0;k<5;++k)check(fact(*ws[k])==((role==later&&k==role)?Fact::Presented:Fact::KnownAbsent),"same-kind presence survives; other-kind equal-color writes retire prior ownership");}
  begin();view._agentProjectileRaster.role(Role(role));blank.blitNShade(&map,4,4,0);view._agentProjectileRaster.role(Role::Ordinary);finish();check(fact(*ws[role])==Fact::KnownAbsent,"transparent candidate absent");
  begin();view._agentProjectileRaster.role(Role(role));sprite.blitNShade(&map,4,4,0);view._agentProjectileRaster.role(Role::Ordinary);map.setPixel(5,5,map.getPixel(5,5));finish();check(fact(*ws[role])==Fact::KnownAbsent,"later same-color flash/direct paint retires witness");
  begin();view._agentProjectileRaster.role(Role(role));sprite.blitNShade(&map,4,4,0);view._agentProjectileRaster.role(Role::Ordinary);finish();check(fact(*ws[role],false)==Fact::Unknown,"unadmitted final viewport remains unknown");
  check(ws[role]->evaluate(true,{0,0,64,64},{},output.palette,[&](int x,int y,std::uint32_t rgb){return output.matches(x,y,rgb);},[](int,int){return false;})==Fact::Unknown,"later unproven HUD/cursor pixel cannot rearm");
  begin();view._agentProjectileRaster.role(Role(role));sprite.blitNShade(&map,4,4,0);view._agentProjectileRaster.role(Role::Ordinary);check(!finish(false)&&fact(*ws[role])==Fact::Unknown,"failed final presentation rejected");
 }
 AgentPublicEvents ring;std::string epoch(32,'a');check(ring.start(epoch),"public ring starts");check(ring.thrownItem(Fact::Presented)&&ring.largeExplosion(Fact::Presented)&&ring.newest()==2,"independent kinds retained without polling");
 for(int n=0;n<300;++n)check(ring.thrownItem(Fact::Presented)&&ring.largeExplosion(Fact::Presented)&&ring.newest()==2,"repeat frames do not count objects or attacks");
 check(ring.thrownItem(Fact::Unknown)&&ring.largeExplosion(Fact::Unknown)&&ring.thrownItem(Fact::Presented)&&ring.largeExplosion(Fact::Presented)&&ring.newest()==2,"pause/unknown preserves both latches");
 AgentPublicEvents::Page page;check(ring.read(epoch,0,16,page)&&page.events.size()==2,"late reader retains both kinds");
 for(unsigned n=0;n<2;++n){std::string kind=n?"large_explosion_effect_presented":"thrown_item_presented";check(page.events[n].json=="{\"seq\":"+std::to_string(n+1)+",\"kind\":\""+kind+"\",\"semantics\":\"ordinary_renderer_visible_contribution\",\"source\":null,\"actor\":null,\"faction\":null,\"world_position\":null,\"hit\":null,\"damage\":null}","exact presence-only nine-key event");}
 const auto prefix=page.events;check(ring.thrownItem(Fact::KnownAbsent)&&ring.thrownItem(Fact::Presented)&&ring.newest()==3,"known absence rearms only thrown episode");check(ring.largeExplosion(Fact::Presented)&&ring.newest()==3,"thrown event does not split large effect");
 check(ring.read(epoch,0,16,page)&&page.events[0].json==prefix[0].json,"retained prefix immutable");
 for(int n=0;n<260;++n)check(ring.largeExplosion(Fact::KnownAbsent)&&ring.largeExplosion(Fact::Presented),"independent large-effect reappearance");
 check(ring.read(epoch,0,16,page)&&page.gap&&page.oldest==8&&page.newest==263&&ring.size()==256,"public-only sequence and bounded explicit retention gap");check(!ring.read(std::string(32,'b'),0,16,page),"wrong epoch rejected");
 SDL_FreeSurface(final);std::cout<<"AgentGrenadeRendererTest PASS "<<checks<<" checks\n";
}
