// SPDX-License-Identifier: GPL-3.0-or-later
// Actual offscreen Surface/SDL/shader writes and cache marker lifecycle.
// Marking synthetic cache paint models the reviewed UnitSprite branch; it does
// not execute a Game, UnitSprite, FOV, real flip, combat or retail sprites.
#include "../../src/Engine/Surface.h"
#include "../../src/Observer/AgentProjectileOutput.h"
#include "../../src/Observer/AgentPublicEvents.h"
#include "../../src/Observer/AgentUi.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom; using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(){
 using Fact=AgentProjectileFact;using Role=AgentRendererRole;
 AgentProjectileWitness p,i,c;AgentProjectileRaster raster;
 Surface map(12,8),logical(12,8),sprite(3,3),blank(3,3),cache(3,3);
 SDL_Color colors[256];for(int n=0;n<256;++n)colors[n]={Uint8(n),Uint8(n),Uint8(n),0};
 for(Surface*s:{&map,&logical,&sprite,&blank,&cache})s->setPalette(colors);
 sprite.setPixel(1,1,3);map.agentAttachProjectileRaster(&raster);
 SDL_Surface*final=SDL_CreateRGBSurface(0,12,8,8,0,0,0,0);check(final,"final allocation");SDL_SetColors(final,colors,0,256);
 AgentProjectileOutput output;
 auto begin=[&](){map.clear(1);check(raster.begin({{&p,&i,&c}},map.getSurface()->pixels,12,8,map.getSurface()->pitch),"three bounded witnesses begin");};
 auto paint=[&](Role role,int x=3,int y=2){raster.role(role);sprite.blitNShade(&map,x,y,0);raster.role(Role::Ordinary);};
 auto finish=[&](){for(auto*w:{&p,&i,&c})w->seal(agentPaintHealthy());map.blit(&logical);check(SDL_BlitSurface(logical.getSurface(),nullptr,final,nullptr)==0,"ordinary final copy");check(output.capture(true,false,logical.getSurface(),final,map.getSurface(),0,0,0,0),"output admitted");};
 auto fact=[&](AgentProjectileWitness&w,bool admitted=true){return w.evaluate(admitted,{0,0,12,8},{},output.palette,[&](int x,int y,std::uint32_t rgb){return output.matches(x,y,rgb);});};
 for(Role role:{Role::Projectile,Role::Impact,Role::Collapse}){
  begin();paint(role);finish();
  for(unsigned k=0;k<3;++k)check(fact(*std::array<AgentProjectileWitness*,3>{{&p,&i,&c}}[k])==(int(role)==int(k)?Fact::Presented:Fact::KnownAbsent),"only actual tagged kind contributes");
  check(fact(i,false)==Fact::Unknown&&fact(c,false)==Fact::Unknown,"hidden or failed mode cannot report kind");
  begin();paint(role,-4,-4);finish();check(fact(p)==Fact::KnownAbsent&&fact(i)==Fact::KnownAbsent&&fact(c)==Fact::KnownAbsent,"fully clipped tags produce no event");
  begin();raster.role(role);blank.blitNShade(&map,3,2,0);raster.role(Role::Ordinary);finish();check(fact(p)==Fact::KnownAbsent&&fact(i)==Fact::KnownAbsent&&fact(c)==Fact::KnownAbsent,"transparent effect cannot report");
  begin();paint(role);paint(Role::Ordinary);finish();check(fact(p)==Fact::KnownAbsent&&fact(i)==Fact::KnownAbsent&&fact(c)==Fact::KnownAbsent,"later identical ordinary paint retires every kind");
 }
 begin();paint(Role::Projectile);paint(Role::Impact);paint(Role::Collapse);finish();
 check(fact(p)==Fact::KnownAbsent&&fact(i)==Fact::KnownAbsent&&fact(c)==Fact::KnownAbsent,"same-color cross-kind overwrite reveals no new contribution");
 begin();paint(Role::Projectile,0,0);paint(Role::Impact,3,2);paint(Role::Collapse,7,4);finish();
 check(fact(p)==Fact::Presented&&fact(i)==Fact::Presented&&fact(c)==Fact::Presented,"independent actual contributions coexist");
 check(c.evaluate(true,{0,0,12,8},{},output.palette,[&](int x,int y,std::uint32_t rgb){return output.matches(x,y,rgb);},[](int,int){return false;})==Fact::Unknown,"covered contribution cannot rearm collapse");
 begin();paint(Role::Impact);raster.unknownWrite();finish();check(fact(p)==Fact::Unknown&&fact(i)==Fact::Unknown&&fact(c)==Fact::Unknown,"unknown writer invalidates all kinds");
 // Cache copy provenance. Exact common draw/copy methods execute here; the
 // semantic branch marker is deliberately supplied by this synthetic fixture.
 sprite.agentMarkCollapsePaint();check(sprite.agentCollapsePaint(),"healthy synthetic collapse paint marked");
 cache.clear();sprite.blit(&cache);cache.agentCopyCollapsePaint(sprite);check(cache.agentCollapsePaint(),"complete ordinary cache copy transfers mark");
 Surface genericCopy(cache);check(!genericCopy.agentCollapsePaint(),"generic copy does not inherit role");
 cache.setPixel(1,1,3);check(!cache.agentCollapsePaint(),"same-color later direct write retires cache mark");
 cache.agentMarkCollapsePaint();blank.blitNShade(&cache,0,0,0);check(!cache.agentCollapsePaint(),"later shader destination retires marker conservatively");
 cache.agentMarkCollapsePaint();blank.blit(&cache);check(!cache.agentCollapsePaint(),"later SDL destination retires marker");
 cache.agentMarkCollapsePaint();cache.invalidate();cache.invalidate(false);check(!cache.agentCollapsePaint(),"dirty then clean cannot revive marker");
 cache.agentMarkCollapsePaint();cache.clear();check(!cache.agentCollapsePaint(),"clear retires marker");
 sprite.setX(1);cache.agentCopyCollapsePaint(sprite);check(!cache.agentCollapsePaint(),"partial offset cache copy not assigned");sprite.setX(0);
 auto clip=cache.getSurface()->clip_rect;cache.getSurface()->clip_rect.w--;cache.agentCopyCollapsePaint(sprite);check(!cache.agentCollapsePaint(),"destination clip rejects cache proof");cache.getSurface()->clip_rect=clip;
 sprite.getCrop()->w=1;cache.agentCopyCollapsePaint(sprite);check(!cache.agentCollapsePaint(),"source crop rejects cache proof");sprite.resetCrop();
 sprite.setHidden(true);cache.agentCopyCollapsePaint(sprite);check(!cache.agentCollapsePaint(),"hidden source cannot transfer role");sprite.setHidden(false);
 Surface wrong(4,3);wrong.agentCopyCollapsePaint(sprite);check(!wrong.agentCollapsePaint(),"different sized cache rejected");
 AgentPublicEvents ring;const std::string epoch(32,'a');check(ring.start(epoch),"ring starts");
 check(ring.impact(Fact::Presented)&&ring.collapse(Fact::Presented)&&ring.projectile(Fact::Presented)&&ring.newest()==3,"three kinds retained without polling");
 for(int n=0;n<300;++n)check(ring.impact(Fact::Presented)&&ring.collapse(Fact::Presented)&&ring.newest()==3,"repeat/moving frames not event counts");
 check(ring.impact(Fact::Unknown)&&ring.collapse(Fact::Unknown)&&ring.impact(Fact::Presented)&&ring.collapse(Fact::Presented)&&ring.newest()==3,"pause/unknown keeps independent latches");
 AgentPublicEvents::Page page;check(ring.read(epoch,0,16,page)&&page.events.size()==3,"late polling returns all types");
 const auto first=page.events;
 for(unsigned k=0;k<3;++k){const std::string names[]={"impact_effect_presented","collapse_animation_presented","projectile_presented"};
  check(first[k].json=="{\"seq\":"+std::to_string(k+1)+",\"kind\":\""+names[k]+"\",\"semantics\":\"ordinary_renderer_visible_contribution\",\"source\":null,\"actor\":null,\"faction\":null,\"world_position\":null,\"hit\":null,\"damage\":null}","exact nine-key null-only wire");}
 check(ring.impact(Fact::KnownAbsent)&&ring.impact(Fact::Presented)&&ring.newest()==4,"impact absence rearms only impact");
 check(ring.collapse(Fact::Presented)&&ring.newest()==4,"impact reappearance does not split collapse");
 check(ring.read(epoch,0,16,page)&&page.events[0].json==first[0].json,"immutable retained prefix");
 for(int n=0;n<260;++n)check(ring.collapse(Fact::KnownAbsent)&&ring.collapse(Fact::Presented),"collapse reappearance");
 check(ring.read(epoch,0,16,page)&&page.gap&&page.oldest==9&&page.newest==264&&ring.size()==256,"overflow remains public-only bounded history");
 check(!ring.read(std::string(32,'b'),0,16,page),"wrong epoch rejected");
 agentObservePaint(-1);sprite.agentMarkCollapsePaint();cache.agentCopyCollapsePaint(sprite);check(!sprite.agentCollapsePaint()&&!cache.agentCollapsePaint(),"paint failure prevents source and cache marking");
 SDL_FreeSurface(final);std::cout<<"AgentRendererKindsTest PASS "<<checks<<" checks\n";
}
