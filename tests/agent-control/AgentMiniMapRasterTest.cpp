// SPDX-License-Identifier: GPL-3.0-or-later
// Actual Surface/shader writes; synthetic final software bytes, no Game/FOV/flip.
#include "../../src/Engine/Surface.h"
#include "../../src/Observer/AgentMiniMapPresentation.h"
#include "../../src/Observer/AgentProjectileOutput.h"
#include <iostream>
#include <cstdlib>
using namespace OpenXcom; using namespace OpenXcom::Observer;
static unsigned checks=0;static void check(bool b,const char*m){++checks;if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
 Surface map(13,12),logical(13,12),glyph(4,4),other(4,4);SDL_Color colors[256];for(int i=0;i<256;++i)colors[i]={Uint8(i),Uint8(i),Uint8(i),0};
 for(Surface*s:{&map,&logical,&glyph,&other})s->setPalette(colors);
 glyph.setPixel(0,0,2);glyph.setPixel(3,3,3);other.setPixel(1,1,4);
 SDL_Surface*final=SDL_CreateRGBSurface(0,26,24,8,0,0,0,0);check(final,"final software buffer");SDL_SetColors(final,colors,0,256);
 AgentMiniMapPresentation p;AgentProjectileOutput output;
 auto begin=[&](){map.agentAttachProjectileRaster(nullptr);map.clear(1);check(p.begin(map.getSurface()->pixels,13,12,map.getSurface()->pitch),"bounded begin");map.agentAttachProjectileRaster(p.raster());};
 auto paint=[&](AgentMiniMapKind k,int x,int y,Surface&s,unsigned mask){p.role(k);s.blitNShade(&map,x,y,0);p.glyph(k,x,y,mask);p.ordinary();};
 auto compose=[&](){p.seal(true);map.blit(&logical);for(int y=0;y<12;++y)for(int x=0;x<13;++x)for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)static_cast<Uint8*>(final->pixels)[(y*2+dy)*final->pitch+x*2+dx]=logical.getPixel(x,y);check(output.capture(true,false,logical.getSurface(),final,map.getSurface(),0,0,0,0),"nearest final proof");};
 auto project=[&](){return p.project(output.palette,[&](int x,int y,std::uint32_t rgb){return output.matches(x,y,rgb);},[](int,int){return true;});};
 for(auto kind:{AgentMiniMapKind::Terrain,AgentMiniMapKind::Unit,AgentMiniMapKind::Item}){begin();paint(kind,4,4,glyph,0x8001);compose();auto out=project();check(out.size()==1&&out[0].kind==kind&&out[0].x==4,"exact ordinary role complete glyph");}
 begin();paint(AgentMiniMapKind::Terrain,4,4,glyph,0x8001);paint(AgentMiniMapKind::Unit,4,4,other,1<<5);compose();check(project().size()==2,"transparent roles coexist at same cell");
 begin();paint(AgentMiniMapKind::Terrain,4,4,glyph,0x8001);paint(AgentMiniMapKind::Unit,4,4,glyph,0x8001);compose();check(project().empty(),"same-color later role retires former role and has no new contrast");
 begin();paint(AgentMiniMapKind::Terrain,4,4,glyph,0x8001);map.setPixel(4,4,map.getPixel(4,4));compose();check(project().empty(),"one retired ink pixel rejects incomplete glyph");
 begin();paint(AgentMiniMapKind::Terrain,0,0,glyph,0x8001);paint(AgentMiniMapKind::Terrain,8,8,glyph,0x8001);p.retire(0,0,5,5);compose();check(project().size()==1&&project()[0].x==8,"reticle bounds retire only intersecting glyphs");
 begin();p.role(AgentMiniMapKind::Terrain);glyph.blitNShade(&map,4,4,16);p.glyph(AgentMiniMapKind::Terrain,4,4,0x8001);p.ordinary();compose();auto dark=output.palette;dark[1]=dark[15];check(p.project(dark,[](int,int,std::uint32_t){return true;},[](int,int){return true;}).empty(),"ordinary shade16 black equals background: no unknown terrain contribution");
 begin();paint(AgentMiniMapKind::Unit,12,4,glyph,0x8001);compose();check(project().empty(),"partial edge glyph rejected");
 begin();paint(AgentMiniMapKind::Item,4,4,glyph,0);compose();check(project().empty(),"unknown blank glyph mask no category");
 begin();paint(AgentMiniMapKind::Item,4,4,glyph,0x8001);compose();auto generation=p.revision();begin();paint(AgentMiniMapKind::Item,4,4,glyph,0x8001);compose();check(p.revision()==generation,"idle redraw does not change private geometry generation");
 begin();compose();begin();paint(AgentMiniMapKind::Item,4,4,glyph,0x8001);compose();check(p.revision()>generation,"draw-role away/back retains private ABA");
 check(p.project(output.palette,[&](int x,int y,std::uint32_t rgb){return output.matches(x,y,rgb);},[](int,int){return false;}).empty(),"covered cell omitted");
 static_cast<Uint8*>(final->pixels)[8*final->pitch+8]=7;check(project().empty(),"one final replica changed rejects glyph");compose();
 auto palette=output.palette;palette[2]=palette[1];palette[3]=palette[1];check(p.project(palette,[](int,int,std::uint32_t){return true;},[](int,int){return true;}).empty(),"equal RGB produces no semantic contribution");
 p.seal(false);check(project().empty(),"failed paint invalidates");
 check(!p.begin(nullptr,12,12,12),"null backing fails");check(!p.begin(map.getSurface()->pixels,641,12,644),"width bound");check(!p.begin(map.getSurface()->pixels,12,401,16),"height bound");
 SDL_FreeSurface(final);std::cout<<"AgentMiniMapRasterTest "<<checks<<" PASS\n";
}
