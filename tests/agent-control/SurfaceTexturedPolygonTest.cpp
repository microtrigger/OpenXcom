// Native synthetic SDL raster regression; no game, assets, or hidden state.
#include "../../src/Engine/Surface.h"
#include "../../src/Observer/AgentUi.h"
#include <SDL_gfxPrimitives.h>
#include <cstring>
#include <iostream>
#include <cstdlib>
using namespace OpenXcom;
static int checks=0;
static void check(bool ok,const char *message){++checks;if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}}
int main(){
 check(SDL_Init(0)==0,"SDL init");
 Surface actual(64,48), expected(64,48), texture(8,8);
 SDL_Color colors[256];for(int i=0;i<256;++i)colors[i]={Uint8(i),Uint8(i),Uint8(i),0};
 actual.setPalette(colors);expected.setPalette(colors);texture.setPalette(colors);texture.clear(117);
 struct Case {int x0,y0,x1,y1;SDL_Rect clip;};
 const SDL_Rect full={0,0,64,48},small={12,10,20,20},empty={0,0,0,0};
 Case cases[]={{10,10,30,30,full},{-5,-5,20,20,full},{-100,-100,-50,-50,full},{100,100,150,150,full},{64,0,80,20,full},{0,48,20,80,full},{-30,4,-1,20,full},{4,-30,20,-1,full},{5,5,20,20,small},{0,0,10,8,small},{32,10,40,15,small},{0,0,63,47,empty},{0,0,0,0,full},{-32768,-32768,-30000,-30000,full},{30000,30000,32767,32767,full},{-32768,0,32767,10,full}};
 for(auto c:cases){
  SDL_SetClipRect(actual.getSurface(),nullptr);SDL_SetClipRect(expected.getSurface(),nullptr);actual.clear(29);expected.clear(29);
  SDL_SetClipRect(actual.getSurface(),&c.clip);SDL_SetClipRect(expected.getSurface(),&c.clip);
  Sint16 x[]={Sint16(c.x0),Sint16(c.x0),Sint16(c.x1),Sint16(c.x1)},y[]={Sint16(c.y0),Sint16(c.y1),Sint16(c.y1),Sint16(c.y0)};
  texturedPolygon(expected.getSurface(),x,y,4,texture.getSurface(),0,0);
  actual.drawTexturedPolygon(x,y,4,&texture,0,0);
  check(Observer::agentPaintHealthy(),"empty-clip poisoned paint audit");
  check(actual.getSurface()->pitch==expected.getSurface()->pitch,"same pitch");
  check(std::memcmp(actual.getSurface()->pixels,expected.getSurface()->pixels,actual.getSurface()->pitch*actual.getHeight())==0,"pixels equal ordinary SDL_gfx");
 }
 // A genuinely invalid polygon still follows SDL_gfx and poisons the audit.
 SDL_SetClipRect(actual.getSurface(),nullptr);Sint16 x[]={1,5},y[]={1,5};
 actual.drawTexturedPolygon(x,y,2,&texture,0,0);
 check(!Observer::agentPaintHealthy(),"invalid polygon must remain fail closed");
 std::cout<<"PASS "<<checks<<" textured polygon native checks; synthetic pixels only\n";
}
