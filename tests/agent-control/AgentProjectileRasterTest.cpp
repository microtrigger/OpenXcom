// SPDX-License-Identifier: GPL-3.0-or-later
// Real offscreen Surface/ShaderDraw/SDL pixels. No Game, window, assets or FOV.
#include "../../src/Engine/Surface.h"
#include "../../src/Observer/AgentProjectileOutput.h"
#include "../../src/Observer/AgentPublicEvents.h"
#include <cstdlib>
#include <iostream>
#include <cstring>
using namespace OpenXcom; using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
int main(){
 typedef AgentProjectileFact Fact;
 Surface map(12,8), plain(12,8), sprite(3,3), transparent(3,3), logical(12,8);
 SDL_Color colors[256];for(int i=0;i<256;++i)colors[i]={Uint8(i),Uint8(i),Uint8(i),0};
 for(Surface*s:{&map,&plain,&sprite,&transparent,&logical})s->setPalette(colors);
 sprite.setPixel(0,0,2);sprite.setPixel(1,1,3);sprite.setPixel(2,2,4);
 AgentProjectileWitness witness;AgentProjectileRaster raster;map.agentAttachProjectileRaster(&raster);
 AgentProjectileOutput output;
 SDL_Surface*final=SDL_CreateRGBSurface(0,28,20,8,0,0,0,0);check(final,"SDL final allocation");SDL_SetColors(final,colors,0,256);
 auto begin=[&](){map.clear(1);plain.clear(1);check(raster.begin(witness,map.getSurface()->pixels,12,8,map.getSurface()->pitch),"bounded Map raster begin");};
 auto compose=[&](){
  map.blit(&logical);
  for(int y=0;y<8;++y)for(int x=0;x<12;++x)for(int yy=0;yy<2;++yy)for(int xx=0;xx<2;++xx)
   static_cast<Uint8*>(final->pixels)[(y*2+2+yy)*final->pitch+x*2+2+xx]=logical.getPixel(x,y);
  check(output.capture(true,false,logical.getSurface(),final,map.getSurface(),2,2,2,2),"readable integer scaled output admitted");
 };
 auto fact=[&](){witness.seal(true);compose();return witness.evaluate(true,{0,0,12,8},{},output.palette,[&](int x,int y,std::uint32_t rgb){return output.matches(x,y,rgb);});};
 begin();raster.body(true);sprite.blitNShade(&map,3,2,0);raster.body(false);sprite.blitNShade(&plain,3,2,0);
 check(fact()==Fact::Presented,"actual ordinary body shader contributes");
 for(int y=0;y<8;++y)for(int x=0;x<12;++x)check(map.getPixel(x,y)==plain.getPixel(x,y),"observer preserves every shader output byte");
 check(witness.evaluate(true,{0,0,12,8},{},output.palette,[&](int x,int y,std::uint32_t rgb){return output.matches(x,y,rgb);},[](int,int){return false;})==Fact::Unknown,"unknown overlay cannot rearm episode");
 begin();raster.body(true);sprite.blitNShade(&map,-2,-2,0);raster.body(false);check(fact()==Fact::Presented,"ordinary negative clipping retains only actual one-pixel contribution");
 begin();raster.body(true);sprite.blitNShade(&map,3,2,0,true);raster.body(false);check(fact()==Fact::Presented,"ordinary half-domain body records only actual writes");
 begin();raster.body(true);sprite.blitNShade(&map,-3,-3,0);raster.body(false);check(fact()==Fact::KnownAbsent,"fully clipped shader writes no witness");
 begin();raster.body(true);transparent.blitNShade(&map,3,2,0);raster.body(false);check(fact()==Fact::KnownAbsent,"transparent sprite writes no contribution");
 begin();raster.body(true);sprite.blitNShade(&map,3,2,0);raster.body(false);transparent.blitNShade(&map,3,2,0);check(fact()==Fact::Presented,"transparent later source preserves contribution");
 begin();raster.body(true);sprite.blitNShade(&map,3,2,0);raster.body(false);sprite.blitNShade(&map,3,2,0);check(fact()==Fact::KnownAbsent,"actual same-color later shader retires witness");
 begin();raster.body(true);sprite.blitNShade(&map,3,2,0);raster.body(false);
 for(int i=0;i<3;++i) { map.setPixel(i+3,i+2,map.getPixel(i+3,i+2)); } check(fact()==Fact::KnownAbsent,"same-color direct vapor/flash writes retire witness");
 begin();raster.body(true);sprite.blitNShade(&map,3,2,0);raster.body(false);sprite.blitNShade(&map,3,2,0,false,3);check(fact()==Fact::KnownAbsent,"ColorReplace later writes tracked");
 begin();raster.body(true);sprite.blitNShade(&map,3,2,0);raster.body(false);sprite.blitNShade(&map,3,2,0,GraphSubset(12,8));check(fact()==Fact::KnownAbsent,"range shader later writes tracked");
 begin();raster.body(true);sprite.blitNShade(&map,3,2,0);raster.body(false);sprite.blit(&map);check(fact()==Fact::Unknown,"unsupported SDL Map write fails closed");
 begin();raster.body(true);sprite.blitNShade(&map,3,2,0);raster.body(false);map.clear(1);check(fact()==Fact::Unknown,"unknown bulk clear invalidates draw");
 begin();raster.body(true);sprite.blitNShade(&map,3,2,0);raster.body(false);check(fact()==Fact::Presented,"fresh draw recovers after invalidation");
 const Uint8 saved=static_cast<Uint8*>(final->pixels)[6*final->pitch+8];static_cast<Uint8*>(final->pixels)[6*final->pitch+8]=9;
 check(!output.matches(3,2,0x020202),"one mismatched final replica rejects proof");static_cast<Uint8*>(final->pixels)[6*final->pitch+8]=saved;
 check(output.matches(3,2,0x020202),"restored replica proves expected RGB");
 check(!output.capture(false,false,logical.getSurface(),final,map.getSurface(),2,2,2,2),"failed flip not admitted");
 check(!output.capture(true,true,logical.getSurface(),final,map.getSurface(),2,2,2,2),"filtered/OpenGL output unsupported");
 check(!output.capture(true,false,logical.getSurface(),final,map.getSurface(),2,2,1,2),"fractional scale unsupported");
 check(!output.capture(true,false,logical.getSurface(),final,map.getSurface(),-1,2,2,2),"invalid bands rejected");
 const Uint32 flags=final->flags;for(Uint32 bad:{Uint32(SDL_DOUBLEBUF),Uint32(SDL_HWSURFACE),Uint32(SDL_ASYNCBLIT),Uint32(SDL_RLEACCEL),Uint32(SDL_OPENGL)}){final->flags=flags|bad;check(!output.capture(true,false,logical.getSurface(),final,map.getSurface(),2,2,2,2),"opaque or deferred final output rejected");}final->flags=flags;
 final->locked=1;check(!output.capture(true,false,logical.getSurface(),final,map.getSurface(),2,2,2,2),"locked final output rejected");final->locked=0;
 const SDL_Rect clip=final->clip_rect;final->clip_rect.w--;check(!output.capture(true,false,logical.getSurface(),final,map.getSurface(),2,2,2,2),"clipped final copy cannot prove a fresh write");final->clip_rect=clip;
 final->flags|=SDL_SRCALPHA;check(!output.capture(true,false,logical.getSurface(),final,map.getSurface(),2,2,2,2),"alpha compositing unsupported");final->flags=flags;
 check(!output.capture(true,false,map.getSurface(),final,map.getSurface(),2,2,2,2),"aliased Map/composite buffers rejected");
 const Uint16 pitch=final->pitch;final->pitch=1;check(!output.capture(true,false,logical.getSurface(),final,map.getSurface(),2,2,2,2),"short pitch rejected");final->pitch=pitch;
 colors[2].r=44;map.setPalette(colors);check(!output.capture(true,false,logical.getSurface(),final,map.getSurface(),2,2,2,2),"Map/logical palette mismatch fails closed");colors[2].r=2;map.setPalette(colors);
 SDL_Surface*rgba=SDL_CreateRGBSurface(0,28,20,32,0xff0000,0xff00,0xff,0);check(rgba,"32-bit final allocation");
 for(int y=0;y<8;++y)for(int x=0;x<12;++x)for(int yy=0;yy<2;++yy)for(int xx=0;xx<2;++xx){Uint8 c=logical.getPixel(x,y);Uint32 pixel=SDL_MapRGB(rgba->format,c,c,c);std::memcpy(static_cast<Uint8*>(rgba->pixels)+(y*2+2+yy)*rgba->pitch+(x*2+2+xx)*4,&pixel,4);}
 check(output.capture(true,false,logical.getSurface(),rgba,map.getSurface(),2,2,2,2)&&output.matches(3,2,0x020202),"32-bit actual final RGB supported");SDL_FreeSurface(rgba);
 AgentPublicEvents events;const std::string epoch(32,'a');check(events.start(epoch),"event ring epoch starts");
 check(events.projectile(Fact::Unknown)&&events.newest()==0,"unknown never creates public sequence");
 check(events.projectile(Fact::Presented)&&events.newest()==1,"presented episode retained without polling");
 for(int i=0;i<300;++i)check(events.projectile(Fact::Presented)&&events.newest()==1,"repeat frames create no duplicate event");
 check(events.projectile(Fact::Unknown)&&events.projectile(Fact::Presented)&&events.newest()==1,"pause and covered interval preserve latch");
 AgentPublicEvents::Page page;check(events.read(epoch,0,16,page)&&page.events.size()==1,"late poll retrieves occurrence");
 check(page.events[0].json=="{\"seq\":1,\"kind\":\"projectile_presented\",\"semantics\":\"ordinary_renderer_visible_contribution\",\"source\":null,\"actor\":null,\"faction\":null,\"world_position\":null,\"hit\":null,\"damage\":null}","exact null-attribution wire");
 const std::string retained=page.events[0].json;check(events.read(epoch,0,16,page)&&page.events[0].json==retained,"read replay immutable");
 for(int i=0;i<260;++i){check(events.projectile(Fact::KnownAbsent)&&events.projectile(Fact::Presented),"proven absence rearms episode");}
 check(events.size()==256&&events.newest()==261,"bounded event retention independent of poll");check(events.read(epoch,0,16,page)&&page.gap&&page.oldest==6&&page.events.size()==16,"overflow gap explicit");
 check(!events.read(std::string(32,'b'),0,16,page),"wrong epoch rejected");
 SDL_FreeSurface(final);std::cout<<"AgentProjectileRasterTest PASS "<<checks<<" checks\n";
}
