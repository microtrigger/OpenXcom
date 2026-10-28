// SPDX-License-Identifier: GPL-3.0-or-later
// Offscreen native widget qualification; no game resources or hidden game state.
#include "../../src/Interface/BattlescapeButton.h"
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Options.h"
#include "../../src/Observer/AgentUi.h"
#include <iostream>
#include <cstdlib>
using namespace OpenXcom;
struct Button : BattlescapeButton {
    Button(int x) : BattlescapeButton(12,12,x,5) {}
    Surface *alternate() { return _altSurface; }
};
static unsigned checks=0;
static void check(bool value,const char *message) { ++checks; if (!value) { std::cerr<<message<<'\n';std::exit(1); } }
int main() {
    check(SDL_Init(0)==0,"SDL offscreen init");
    Options::baseXResolution=80;Options::baseYResolution=40;
    Surface canvas(80,40),other(80,40);Button none(5),snap(20),aimed(35),aut(50);BattlescapeButton *group=&none;
    for(Button *b:{&none,&snap,&aimed,&aut}) { b->drawRect(0,0,12,12,32);b->initSurfaces();b->setGroup(&group); }
    check(!none.agentTogglePainted(&canvas),"unpainted denied");
    none.blit(&canvas);snap.blit(&canvas);
    check(none.agentTogglePainted(&canvas)&&snap.agentTogglePainted(&canvas),"full native paint admitted");
    check(none.agentShownToggled()&&!snap.agentShownToggled(),"None radio selected");
    check(!none.agentTogglePainted(&other),"wrong destination denied");
    SDL_Event event={};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;Action click(&event,1,1,0,0);
    snap.mousePress(&click,nullptr);
    check(!none.agentTogglePainted(&canvas)&&!snap.agentTogglePainted(&canvas),"mutation before repaint denied");
    none.blit(&canvas);snap.blit(&canvas);
    check(snap.agentTogglePainted(&canvas)&&snap.agentShownToggled()&&!none.agentShownToggled(),"Snap painted");
    none.mousePress(&click,nullptr);snap.mousePress(&click,nullptr);
    check(!snap.agentTogglePainted(&canvas),"toggle ABA before repaint denied");
    snap.blit(&canvas);
    SDL_Rect clip={0,0,2,2};SDL_SetClipRect(canvas.getSurface(),&clip);snap.blit(&canvas);
    check(!snap.agentTogglePainted(&canvas),"clipped destination denied");
    SDL_SetClipRect(canvas.getSurface(),nullptr);
    check(!snap.agentTogglePainted(&canvas),"clip restoration not paint proof");snap.blit(&canvas);
    SDL_SetClipRect(snap.alternate()->getSurface(),&clip);snap.blit(&canvas);
    check(!snap.agentTogglePainted(&canvas),"clipped alternate denied");
    SDL_SetClipRect(snap.alternate()->getSurface(),nullptr);snap.blit(&canvas);
    snap.setVisible(false);snap.blit(&canvas);check(!snap.agentTogglePainted(&canvas),"hidden main with alternate denied");
    snap.setVisible(true);check(!snap.agentTogglePainted(&canvas),"unhide needs repaint");snap.blit(&canvas);
    Surface cover(12,12,20,5);std::vector<Surface*> order={&none,&snap,&cover};
    check(!Observer::agentWidgetUncovered(&snap,order),"later actual overlay denied");
    cover.setVisible(false);check(Observer::agentWidgetUncovered(&snap,order),"hidden overlay clear");
    none.mousePress(&click,nullptr);none.blit(&canvas);snap.blit(&canvas);
    check(none.agentTogglePainted(&canvas)&&none.agentShownToggled()&&!snap.agentShownToggled(),"None restored through radio handler");
    for(Button *from:{&none,&snap,&aimed,&aut})for(Button *to:{&none,&snap,&aimed,&aut}) {
        from->mousePress(&click,nullptr);
        for(Button *b:{&none,&snap,&aimed,&aut})b->blit(&canvas);
        to->mousePress(&click,nullptr);
        check(!to->agentTogglePainted(&canvas),"each reserve mutation needs repaint");
        for(Button *b:{&none,&snap,&aimed,&aut})b->blit(&canvas);
        unsigned selected=0;
        for(Button *b:{&none,&snap,&aimed,&aut}) {
            check(b->agentTogglePainted(&canvas),"all four painted reserve controls admitted");
            if(b->agentShownToggled())++selected;
        }
        check(group==to&&to->agentShownToggled()&&selected==1,"all four native radio transitions exact");
    }
    Button kneel(65);kneel.drawRect(0,0,12,12,32);kneel.initSurfaces();kneel.allowToggleInversion();
    check(!kneel.agentStandaloneTogglePainted(&canvas),"standalone prepaint denied");
    kneel.blit(&canvas);
    check(kneel.agentStandaloneTogglePainted(&canvas)&&!kneel.agentShownToggled(),"standalone off shown");
    check(!kneel.agentTogglePainted(&canvas),"standalone never admitted as radio");
    check(!none.agentStandaloneTogglePainted(&canvas),"radio never admitted as standalone");
    check(!kneel.agentStandaloneTogglePainted(&other),"standalone wrong destination denied");
    kneel.toggle(true);check(!kneel.agentStandaloneTogglePainted(&canvas),"standalone mutation needs paint");kneel.blit(&canvas);
    check(kneel.agentStandaloneTogglePainted(&canvas)&&kneel.agentShownToggled(),"standalone on shown");
    kneel.toggle(false);kneel.toggle(true);check(!kneel.agentStandaloneTogglePainted(&canvas),"standalone ABA denied");kneel.blit(&canvas);
    SDL_SetClipRect(canvas.getSurface(),&clip);kneel.blit(&canvas);SDL_SetClipRect(canvas.getSurface(),nullptr);
    check(!kneel.agentStandaloneTogglePainted(&canvas),"standalone clipped frame denied");kneel.blit(&canvas);
    SDL_SetClipRect(kneel.alternate()->getSurface(),&clip);kneel.blit(&canvas);SDL_SetClipRect(kneel.alternate()->getSurface(),nullptr);
    check(!kneel.agentStandaloneTogglePainted(&canvas),"standalone clipped source denied");kneel.blit(&canvas);
    kneel.setVisible(false);kneel.blit(&canvas);check(!kneel.agentStandaloneTogglePainted(&canvas),"standalone hidden denied");kneel.setVisible(true);
    check(!kneel.agentStandaloneTogglePainted(&canvas),"standalone unhide needs paint");kneel.blit(&canvas);
    kneel.allowClickInversion();kneel.blit(&canvas);check(!kneel.agentStandaloneTogglePainted(&canvas),"momentary click is not toggle");kneel.allowToggleInversion();
    check(!kneel.agentStandaloneTogglePainted(&canvas),"mode restoration needs paint");kneel.blit(&canvas);
    kneel.toggle(false);kneel.blit(&canvas);check(kneel.agentStandaloneTogglePainted(&canvas)&&!kneel.agentShownToggled(),"standalone off restored");
    check(Observer::agentPaintHealthy()&&!SDL_WasInit(SDL_INIT_VIDEO),"healthy no GUI");
    std::cout<<"AgentReserveButtonTest PASS "<<checks<<" checks\n";
}
