// SPDX-License-Identifier: GPL-3.0-or-later
// Real offscreen Surface/SDL layers; verbatim Inventory projection methods.
// Owning Inventory/Game, rules and input dispatch are explicit test shims.
#include "../../src/Observer/AgentInventoryPaint.h"
#include "../../src/Engine/Options.h"
#include <iostream>
#include <cstdlib>
namespace OpenXcom { namespace Options { int baseXResolution=320,baseYResolution=200; } }
using namespace OpenXcom; using namespace OpenXcom::Observer;
static unsigned checks=0;
static void check(bool b,const char*m){++checks;if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
namespace OpenXcom {
class Inventory : public Surface {
public:
 struct AgentTarget {AgentUiRect rect;std::string area,label="null";AgentUiRect labelInk={0,0,0,0};std::vector<std::pair<int,unsigned>>labelPixels;};
 Surface grid{320,200},items{320,200},selection{32,48},warning{224,24,48,176};
 Surface *_grid=&grid,*_items=&items,*_selection=&selection,*_warning=&warning;
 std::vector<AgentTarget>_agentTargets;
 std::vector<unsigned char>_agentShownItemInk,_agentShownGridInk;
 std::uint64_t _agentInventoryRevision=0,_agentShownRevision=0;
 bool _agentFrameReady=false,_agentGridReady=false,_agentGridUnambiguous=false,_agentHeldPresented=false;
 Inventory():Surface(320,200){warning.setVisible(false);}
 bool agentProjectionReady()const; bool agentOverlayClear(const AgentUiRect&,bool=true)const;
 bool agentItemsVisible(const AgentUiRect&)const;bool agentGridVisible(const AgentTarget&)const;
 void agentCaptureTarget(int,int,int,int,const std::string&);void agentCaptureItems(const Surface*);
};
#include "AgentInventoryPaintMethods.generated.inc"
}
int main(){
 check(SDL_Init(0)==0&&!SDL_GetVideoSurface(),"offscreen only");
 Options::baseXResolution=320;Options::baseYResolution=200;
 Inventory inv;Surface background(320,200),outside(640,400);
 SDL_Color colors[256];for(int i=0;i<256;++i)colors[i]={Uint8(i),Uint8(i),Uint8(i),0};
 for(Surface*s:{static_cast<Surface*>(&inv),&inv.grid,&inv.items,&inv.selection,&inv.warning,&background,&outside})s->setPalette(colors);
 check(SDL_SetColorKey(background.getSurface(),0,0)==0 && agentInventoryDestination(&background) && !agentInventoryLayer(&background),"Screen-shaped opaque destination is distinct from keyed source");
 background.clear(1);inv.agentCaptureTarget(0,63,32,48,"hand");inv.agentCaptureTarget(128,63,32,48,"hand");inv.agentCaptureTarget(0,152,16,16,"ground");
 check(inv._agentTargets.size()==3,"bounded stock controls");
 inv.agentCaptureTarget(1,63,32,48,"hand");inv.agentCaptureTarget(0,151,16,16,"ground");inv.agentCaptureTarget(400,152,16,16,"ground");
 check(inv._agentTargets.size()==3,"unsupported geometry cannot create targets");
 auto border=[&](const Inventory::AgentTarget&t){const auto&r=t.rect;SDL_Rect q={Sint16(r.x),Sint16(r.y),Uint16(r.width+(t.area=="ground")),Uint16(r.height+(t.area=="ground"))};inv.grid.drawRect(&q,20);++q.x;++q.y;q.w-=2;q.h-=2;inv.grid.drawRect(&q,0);};
 for(const auto&t:inv._agentTargets)border(t);
 inv._agentGridReady=true;
 auto paint=[&](){inv.clear();inv.grid.blit(&inv);inv.items.blit(&inv);inv.selection.blit(&inv);inv.warning.blit(&inv);inv.agentCaptureItems(&background);inv.blit(&background);inv._agentFrameReady=agentPaintHealthy();inv._agentShownRevision=inv._agentInventoryRevision;};
 check(!inv.agentProjectionReady(),"no frame before ordinary blits");paint();
 check(inv.agentProjectionReady(),"actual full-layer composition ready");
 for(const auto&t:inv._agentTargets)check(inv.agentGridVisible(t),"actual contrasting four grid edges");
 check(!inv.agentItemsVisible(inv._agentTargets[0].rect),"blank item layer has no presented contribution");
 // Reproduce /11's live failure and verify malformed destinations stay closed.
 auto absent=[&](){for(const auto&t:inv._agentTargets)check(!inv.agentGridVisible(t),"unproved destination withholds every grid target");};
 SDL_SetColorKey(background.getSurface(),SDL_SRCCOLORKEY,0);paint();absent();
 check(!agentInventoryDestination(&background)&&agentInventoryLayer(&background),"keyed destination no longer masquerades as Screen output");
 SDL_SetColorKey(background.getSurface(),0,0);background.invalidate();paint();absent();background.invalidate(false);
 background.getCrop()->w=1;paint();absent();background.resetCrop();
 auto destinationClip=background.getSurface()->clip_rect;--background.getSurface()->clip_rect.w;paint();absent();background.getSurface()->clip_rect=destinationClip;
 SDL_SetAlpha(background.getSurface(),SDL_SRCALPHA,128);paint();absent();SDL_SetAlpha(background.getSurface(),0,255);
 background.setX(1);paint();absent();background.setX(0);
 background.setHidden(true);paint();absent();background.setHidden(false);
 check(SDL_LockSurface(background.getSurface())==0,"destination lock test setup");inv.agentCaptureItems(&background);absent();SDL_UnlockSurface(background.getSurface());
 Surface truecolor(320,200,0,0,32);SDL_SetColorKey(truecolor.getSurface(),0,0);inv.agentCaptureItems(&truecolor);absent();
 SDL_SetColorKey(inv.items.getSurface(),0,0);paint();absent();SDL_SetColorKey(inv.items.getSurface(),SDL_SRCCOLORKEY,0);
 SDL_SetColorKey(inv.items.getSurface(),SDL_SRCCOLORKEY,7);paint();absent();SDL_SetColorKey(inv.items.getSurface(),SDL_SRCCOLORKEY,0);
 background.clear(1);paint();for(const auto&t:inv._agentTargets)check(inv.agentGridVisible(t),"same opaque Screen-shaped destination restored after negative cases");

 inv.items.setPixel(10,75,30);background.clear(1);paint();
 check(inv.agentItemsVisible(inv._agentTargets[0].rect),"actual item-layer contribution admitted");
 check(!inv.agentItemsVisible(inv._agentTargets[1].rect),"other hand never inferred from inventory model");
 inv.items.setPixel(10,75,1);background.clear(1);paint();check(!inv.agentItemsVisible(inv._agentTargets[0].rect),"same RGB item paint contributes no visible item witness");
 inv.items.setPixel(10,75,30);inv.selection.setPixel(10,12,40);inv.selection.setX(0);inv.selection.setY(63);background.clear(1);paint();
 AgentUiRect held;check(inv._agentHeldPresented&&agentInventoryInk(&inv.selection,held)&&held.x==10&&held.y==75&&held.width==1&&held.height==1,"held footprint uses actual painted ink only");
 check(!inv.agentItemsVisible(inv._agentTargets[0].rect),"held contribution occludes prior item witness");
 check(!inv.agentOverlayClear(inv._agentTargets[0].rect)&&inv.agentOverlayClear(inv._agentTargets[1].rect),"covered target withheld while another remains available");
 inv.selection.setPixel(10,12,30);background.clear(1);paint();check(!inv._agentHeldPresented,"same-color held write does not prove holding");
 inv.selection.clear();inv.selection.setPixel(31,47,40);inv.selection.setX(300);inv.selection.setY(180);background.clear(1);paint();check(!inv._agentHeldPresented,"clipped held sprite withheld, never reconstructed");
 inv.selection.clear();inv.selection.setPixel(0,0,40);inv.selection.setX(60);inv.selection.setY(180);inv.warning.setVisible(true);inv.warning.clear(50);background.clear(1);paint();
 check(!inv._agentHeldPresented,"warning over held sprite prevents cancel authority");
 inv.warning.setVisible(false);background.clear(1);paint();check(inv._agentHeldPresented,"held sprite proven after actual overlay removal");
 inv.selection.clear();inv.items.clear();inv.grid.clear(0);background.clear(1);paint();for(const auto&t:inv._agentTargets)check(!inv.agentGridVisible(t),"transparent grid cannot authorize target");
 for(const auto&t:inv._agentTargets){border(t);}
 inv.grid.setHidden(true);paint();check(!inv.agentProjectionReady(),"hidden grid refuses projection");inv.grid.setHidden(false);
 inv.items.invalidate();check(!inv.agentProjectionReady(),"dirty item layer refuses");inv.items.invalidate(false);background.clear(1);paint();check(inv.agentProjectionReady(),"actual clean composition restored");
 ++inv._agentInventoryRevision;check(!inv.agentProjectionReady(),"post-paint mutation cannot reuse earlier frame");background.clear(1);paint();check(inv.agentProjectionReady(),"new frame carries new private generation");
 const auto old=inv._agentInventoryRevision;for(int i=0;i<100;++i){background.clear(1);paint();check(inv._agentInventoryRevision==old,"idle repaint never changes private input generation");}
 auto clip=inv.items.getSurface()->clip_rect;--inv.items.getSurface()->clip_rect.w;check(!inv.agentProjectionReady(),"unknown partial component clip refuses");inv.items.getSurface()->clip_rect=clip;
 inv.getCrop()->w=1;check(!inv.agentProjectionReady(),"cropped composite refuses");inv.resetCrop();
 inv.setX(1);check(!inv.agentProjectionReady(),"unsupported shifted viewport refuses");inv.setX(0);
 Surface cover(10,10,8,8);cover.setPalette(colors);AgentUiRect region={0,0,20,20};check(agentInventoryClear(&cover,region),"transparent actual layer does not obscure text");cover.setPixel(0,0,7);check(!agentInventoryClear(&cover,region),"actual later opaque ink obscures underlying field");cover.setVisible(false);check(agentInventoryClear(&cover,region),"hidden later layer contributes nothing");
 check(!agentInventoryInside({-1,0,1,1})&&!agentInventoryInside({319,199,2,2})&&agentInventoryInside({319,199,1,1}),"actual viewport boundaries");
 check(agentPaintHealthy()&&!SDL_GetVideoSurface()&&!SDL_WasInit(SDL_INIT_VIDEO),"no GUI or failed paint in offscreen fixture");
 std::cout<<"AgentInventoryPaintTest PASS "<<checks<<" checks\n";
}
