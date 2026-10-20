// SPDX-License-Identifier: GPL-3.0-or-later
// SOURCE-ONLY acceptance candidate: actual Game/VideoState/FlcPlayer, actual
// LoaderSession and Linux reap, and the shared enumerated native-leaf model.
// Build loading_game_integration_unit.cpp with
// OPENXCOM_LOADING_GAME_FIXTURE_SUPPORT_ONLY, never its independent main.
// Do not replace Music/Sound, the video state machine, PCM or controller logic.
// Each named case is a fresh process. A timeout/signal is never a passing oracle.
// The fixture creates at most three small synthetic FLC files under the shared
// command-created monitored root. Exact artifact paths/caps need command review.
#include "loading_game_integration_fixture.h"
#include "../../src/Menu/VideoState.h"
#include "../../src/Engine/FlcPlayer.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Music.h"
#include "../../src/Mod/Mod.h"
#include "../../src/Menu/MainMenuState.h"
#include "../../src/Menu/OptionsBaseState.h"
#include "../../src/Menu/ModListState.h"
#include <SDL_mixer.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
namespace {
using namespace OpenXcom;
using namespace LoadingGameFixture;
using G=LoadingGameIntegrationAccess;
using V=LoadingVideoIntegrationTestAccess;
using Route=FlcPlayer::AudioLease::Route;
void put16(std::vector<Uint8>&b,std::size_t at,unsigned x){b[at]=Uint8(x);b[at+1]=Uint8(x>>8);}
void put32(std::vector<Uint8>&b,std::size_t at,unsigned x){for(unsigned i=0;i<4;++i)b[at+i]=Uint8(x>>(i*8));}
std::string syntheticFile(const char *name,bool audio,bool malformed=false,unsigned frames=4,bool palette=false) {
    std::vector<Uint8> b(128,0);put16(b,4,0xaf12);put16(b,6,frames);put16(b,8,320);put16(b,10,200);put16(b,12,8);put16(b,16,20);
    if(audio){std::size_t p=b.size();b.resize(p+16+2205,128);put32(b,p,2205);put16(b,p+4,0xaaaa);put16(b,p+8,22050);}
    for(unsigned i=0;i<frames;++i){
        const unsigned bytes=palette?35:22;std::size_t p=b.size();b.resize(p+bytes,0);put32(b,p,bytes);put16(b,p+4,0xf1fa);put16(b,p+6,palette?2:1);
        std::size_t chunk=p+16;if(palette){put32(b,chunk,13);put16(b,chunk+4,4);put16(b,chunk+6,1);b[chunk+8]=0;b[chunk+9]=1;b[chunk+10]=Uint8(i+1);b[chunk+11]=32;b[chunk+12]=64;chunk+=13;}
        put32(b,chunk,6);put16(b,chunk+4,13);
    }
    put32(b,0,b.size());if(malformed)put32(b,128,0);
    require(b.size() <= 16384, "bounded synthetic FLC size");
    return createFixtureFile(name, b.data(), b.size());
}
bool active(Game &game,VideoState *video){return G::topState(game)==video;}
bool phase(VideoState &video,const char *name){return !std::strcmp(V::phase(video),name);}
void step(Game &game,unsigned millis=1){model().clock.fetch_add(millis);tick(game);}
template<class Pred>void until(Game &game,Pred done,const char *what,unsigned millis=1){
    const auto deadline=monotonicNanoseconds()+10000000000ULL;
    while(!done()){require(monotonicNanoseconds()<deadline,what);step(game,millis);}
}
struct Scenario {
    std::unique_ptr<Game> game;
    std::vector<std::string> files,tracks;
    VideoState *video=nullptr;
    unsigned opens=0,closes=0,unpauses=0,plays=0;
    std::size_t attachBackend=0;
    Scenario(bool music=false,bool ufo=false,bool mute=false,bool closed=false) {
        reset();configureVideoAssets(music,ufo);if(ufo)setSoundFolder({"intro.cat"});
        model().failOpen=closed;game=makeGame();loadToPublished(*game);
        Options::mute=mute||closed;opens=model().opens;closes=model().closes;unpauses=model().unpauses;plays=model().musicPlays;
    }
    void begin(bool ufo=false) {
        attachBackend=backends().size();video=new VideoState(&files,&tracks,ufo);game->pushState(video);step(*game);
    }
    void attach() {
        until(*game,[&]{return !active(*game,video)||phase(*video,"Playing")||phase(*video,"NeedDetach");},"video attach completion");
        require(active(*game,video)&&phase(*video,"Playing"),"actual video playing after reap");
        require(backends()[attachBackend]->actualReaps==1,"attach requires actual Linux reap");
    }
    void removed(unsigned millis=20) {
        until(*game,[&]{return !active(*game,video);},"video state removed",millis);
        // The exact retirement grant can outlive the finishing callback. Only
        // the next real boundary may release it and resume ordinary menu audio.
        step(*game);
    }
    void finishCase() {
        require(model().opens==opens && model().closes==closes,"video never opens/closes fixed device");
        require(model().activeOwnerViolations==0 && model().workerSubsystemViolations==0,"video owner/worker exclusion");
        require(model().genericTailCalls==0,"no generic Game tail during video");
        finish(*game);game.reset();releaseAllGates();
    }
};
void routeCase(const char *name) {
    const bool explicitTrack=!std::strcmp(name,"explicit");const bool ufo=!std::strcmp(name,"ufo");
    const bool mute=!std::strcmp(name,"silent");const bool empty=!std::strcmp(name,"no-audio");
    Scenario s(explicitTrack||ufo,ufo,mute);s.files.push_back(syntheticFile("route.flc",!empty));
    if(explicitTrack||ufo)s.tracks.push_back("TRACK");s.begin(ufo);s.attach();
    unsigned expected=unsigned(mute?Route::Silent:ufo?Route::UfoSequence:explicitTrack?Route::ExplicitTrack:Route::InternalPCM);
    require(V::route(*s.video)==expected,"explicit immutable route");
    require(model().unpauses==s.unpauses+1,"one post-reap video unpause");
    if(explicitTrack||ufo)require(model().musicPlays>s.plays,"actual Music method reached native leaf");
    if(mute||empty)require(model().hooks==0,"no PCM hook for silent/empty source");
    const auto lease=V::lease(*s.video);require(!lease.expired(),"active lease strongly retained");
    until(*s.game,[&]{return !active(*s.game,s.video);},"normal video callback returned",100);
    require(!lease.expired(),"exact retirement grant retained through finishing callback return");
    step(*s.game);require(lease.expired(),"all lease borrows released at following owner boundary");
    require(model().unpauses==s.unpauses+2,"video start and later menu resume are distinct grants");s.finishCase();
}
void pendingAttach(const char *name) {
    const bool quit=!std::strcmp(name,"quit-attach");Scenario s(true,false);s.files.push_back(syntheticFile("pending.flc",true));s.tracks.push_back("TRACK");s.begin();
    require(active(*s.game,s.video)&&phase(*s.video,"AwaitAttach"),"attach reservation returned through callback");
    auto control=backends()[s.attachBackend];control->tls.hold();const auto lease=V::lease(*s.video);
    step(*s.game);waitFor(control->tlsEntered,1,"attach TLS held after terminal");
    if(quit)enqueueQuit();else enqueueKey(SDLK_ESCAPE);
    for(unsigned i=0;i<128;++i)step(*s.game);
    require(G::hasOperation(*s.game)&&control->actualReaps==0&&!lease.expired(),"terminal is not reaped");
    require(model().musicPlays==s.plays&&model().unpauses==s.unpauses,"no early or late attach playback");
    control->tls.release();s.removed();
    require(model().musicPlays==s.plays&&model().unpauses==s.unpauses+(quit?0u:1u),"latched close/skip prevents video playback; only detached menu may resume");
    require(lease.expired(),"pending attach detached and retired");
    // Quit may already have started final cleanup; count video calls before that
    // transition rather than confusing final device close with a video close.
    if(quit){finish(*s.game);s.game.reset();}else s.finishCase();
}
void heldDetach() {
    Scenario s; s.files.push_back(syntheticFile("held.flc",true));s.begin();s.attach();
    const auto lease=V::lease(*s.video);CallbackHold callback;const unsigned drains=model().drains;
    enqueueKey(SDLK_ESCAPE);step(*s.game); // event -> NeedDetach -> reservation on this/next tick
    until(*s.game,[&]{return G::hasOperation(*s.game);},"detach reservation");
    auto controls=backends();auto control=controls[controls.size()-2];control->tls.hold();step(*s.game);
    // The independently held callback prevents the native drain return. Pump
    // many real owner ticks, but never use elapsed time as the retirement oracle.
    for(unsigned i=0;i<128;++i)step(*s.game);
    require(G::hasOperation(*s.game)&&!lease.expired()&&model().drains==drains,"callback blocks detach proof");
    callback.release();waitFor(control->tlsEntered,1,"detach terminal before real TLS retirement");
    for(unsigned i=0;i<64;++i)step(*s.game);
    require(control->actualReaps==0&&!lease.expired(),"detached is still not checked reap");
    control->tls.release();s.removed();require(lease.expired(),"actual detach reap releases lease");s.finishCase();
}
void skipPhase(const char *name) {
    const std::string mode=name;const bool quit=mode.find("quit-")==0;
    const std::string target=mode.substr(mode.find('-')+1);
    const bool ufo=target=="hold";Scenario s(ufo,ufo);s.files.push_back(syntheticFile("phase.flc",true));s.begin(ufo);
    if(target=="prepare") { // close before the reserved attach can commit
        require(phase(*s.video,"AwaitAttach"),"prepare reserves without inline worker");
    } else {
        s.attach();
        if(target=="hold"||target=="fade")until(*s.game,[&]{return active(*s.game,s.video)&&phase(*s.video,target=="hold"?"Hold":"Fade");},"target video phase",100);
        else require(target=="play"||target=="detach","known phase case");
    }
    if(target=="detach") {
        enqueueKey(SDLK_ESCAPE);until(*s.game,[&]{return G::hasOperation(*s.game);},"detach starts");
        auto c=backends();c[c.size()-2]->tls.hold();
        if(quit)enqueueQuit();else enqueueKey(SDLK_ESCAPE);releaseAllGates();
    } else if(quit)enqueueQuit();else enqueueKey(SDLK_ESCAPE);
    s.removed();if(quit){finish(*s.game);s.game.reset();}else s.finishCase();
}
void invalidFiles(const char *name) {
    Scenario s(true,false);
    if(!std::strcmp(name,"empty")){}
    else if(!std::strcmp(name,"malformed")){s.files.push_back(syntheticFile("bad.flc",true,true));s.tracks.push_back("TRACK");}
    else if(!std::strcmp(name,"missing")){s.files.push_back(fixturePath("absent.flc"));s.tracks.push_back("TRACK");}
    else {
        s.files.push_back(syntheticFile("first.flc",true));s.files.push_back(fixturePath("absent.flc"));s.files.push_back(syntheticFile("second.flc",false));
        s.tracks={"TRACK","TRACK","TRACK"};
    }
    s.begin();s.removed(100);
    if(std::strcmp(name,"multiple"))require(model().musicPlays==s.plays&&model().unpauses==s.unpauses,"failed input never starts external playback");
    else require(model().musicPlays==s.plays+2,"only two valid files start tracks");
    s.finishCase();
}
void resizeFocusMute(const char *name) {
    Scenario s;s.files.push_back(syntheticFile("events.flc",true));s.begin();s.attach();auto lease=V::lease(*s.video);
    const unsigned pauses=model().pauses,unpauses=model().unpauses,resets=model().windowResets;
    if(!std::strcmp(name,"mute-toggle"))Options::mute=true;
    else {
        SDL_Event event={};event.type=SDL_VIDEORESIZE;event.resize.w=800;event.resize.h=600;enqueue(event);
        event={};event.type=SDL_ACTIVEEVENT;event.active.gain=0;event.active.state=SDL_APPACTIVE;model().focus=0;enqueue(event);
        step(*s.game,100);require(model().windowResets==resets,"resize deferred while surface borrowed");
        require(model().pauses==pauses&&model().unpauses==unpauses,"focus never generic-pauses video");
    }
    enqueueKey(SDLK_ESCAPE);s.removed();require(lease.expired()&&model().pauses>pauses,"cleanup independent of mute/focus");s.finishCase();
}
void failedDetach(const char *name) {
    Scenario s;s.files.push_back(syntheticFile("failure.flc",true));s.begin();s.attach();auto lease=V::lease(*s.video);
    setNextBackendFault(!std::strcmp(name,"detach-nothread")?BackendControl::Fault::NoThreadOnce:BackendControl::Fault::SetupFailureOnce);
    enqueueKey(SDLK_ESCAPE);until(*s.game,[&]{return G::hasOperation(*s.game);},"failed detach reserved");
    auto c=backends();auto control=c[c.size()-2];
    until(*s.game,[&]{return control->starts.load()==1;},"failed detach start receipt");
    if(!std::strcmp(name,"detach-setup"))
        until(*s.game,[&]{return control->actualReaps.load()==1;},"setup-failed thread actual reap");
    else require(control->entries==0 && control->actualReaps==0,"NoThread has no worker or reap");
    for(unsigned i=0;i<64;++i)step(*s.game);
    require(G::hasOperation(*s.game)&&!lease.expired(),"failed detach retained rather than owner cleanup");
    require(model().closes==s.closes&&model().opens==s.opens,"failure cannot switch device");
    // Exact context is intentionally process-lifetime retained after setup
    // ambiguity. This process exits cleanly only after checking retention; it
    // does not run Game destructors or mislabel a retained context as retired.
    require(control->starts==1,"one failed detach start");
    std::puts("LOADING_VIDEO_INTEGRATION PASS retained-detach conditional-native");
    require(std::fflush(stdout)==0,"complete retained result stream");std::_Exit(0);
}
void popDuringVideo(const char *name) {
    Scenario s;s.files.push_back(syntheticFile("pop-first.flc",true));s.files.push_back(syntheticFile("pop-second.flc",true));s.begin();
    const bool attach=!std::strcmp(name,"pop-attach")||!std::strcmp(name,"pop-tls");
    auto lease=V::lease(*s.video);
    if(attach) {
        auto c=backends()[s.attachBackend];c->tls.hold();
        if(!std::strcmp(name,"pop-tls")){step(*s.game);waitFor(c->tlsEntered,1,"pop during actual attach TLS");}
        s.game->popState();require(active(*s.game,s.video)&&!lease.expired(),"pop cannot free active attach");
        if(!std::strcmp(name,"pop-attach")){step(*s.game);waitFor(c->tlsEntered,1,"attach retirement held after pop");}
        for(unsigned i=0;i<64;++i)step(*s.game);
        require(active(*s.game,s.video)&&!lease.expired()&&c->actualReaps==0,"pop waits for checked reap");c->tls.release();
    } else {
        s.attach();lease=V::lease(*s.video);
        if(!std::strcmp(name,"pop-detach")) {
            enqueueKey(SDLK_ESCAPE);until(*s.game,[&]{return G::hasOperation(*s.game);},"pop detach reserved");
            auto controls=backends();auto c=controls[controls.size()-2];c->tls.hold();step(*s.game);waitFor(c->tlsEntered,1,"pop during detach TLS");
            s.game->popState();for(unsigned i=0;i<64;++i)step(*s.game);
            require(active(*s.game,s.video)&&!lease.expired()&&c->actualReaps==0,"pop cannot bypass detached TLS lifetime");c->tls.release();
        } else s.game->popState();
    }
    s.removed();require(lease.expired()&&!s.game->isQuitting(),"video-only pop retires borrows without app quit");
    require(model().unpauses==s.unpauses+(attach?1u:2u),"pop only resumes menu after retirement, never the next clip");s.finishCase();
}
void silentUfo(bool closed) {
    Scenario s(false,true,true,closed);s.files.push_back(syntheticFile("silent-ufo.flc",false,false,220));s.begin(true);s.attach();
    require(V::route(*s.video)==unsigned(Route::Silent),"muted/verified-zero UFO has explicit Silent route");
    until(*s.game,[&]{return V::frameCount(*s.video)>=212;},"silent UFO source speed command",25);
    require(V::headerSpeed(*s.video)==7,"silent UFO preserves frame211 speed command");
    require(model().musicPlays==s.plays&&model().hooks==0,"silent sequence never starts music or PCM");
    if(closed)require(model().pauses==0&&model().unpauses==0&&model().channelHalts==0,"verified-zero UFO never touches stale native playback");
    enqueueKey(SDLK_ESCAPE);s.removed();s.finishCase();
}
void menuAfterVideo() {
    Scenario s(true,false);s.files.push_back(syntheticFile("menu-return.flc",true));s.begin();s.attach();s.removed(100);
    require(model().audioStatus==SDL_AUDIO_PLAYING,"ordinary menu device resumed after exact retirement grant");
    const unsigned plays=model().musicPlays;
    s.game->getMod()->getMusic("TRACK")->play(0);
    require(model().musicPlays==plays+1,"actual ordinary menu track is audible-domain admitted");
    s.game->setState(new MainMenuState());auto *options=new OptionsBaseState(OPT_MENU);s.game->pushState(options);Options::reload=true;
    require(Options::audioSampleRate==22050&&Options::audioBitDepth==16&&Options::audioChunkSize==1024&&!Options::mute,
        "post-video reload keeps the exact startup audio plan");
    const auto generation=s.game->responsiveAudioGeneration();const std::size_t reloadBackend=backends().size();
    const unsigned opens=model().opens,closes=model().closes,unpauses=model().unpauses;options->btnOkClick(nullptr);
    require(G::hasOperation(*s.game),"actual menu accepts reload after video");
    until(*s.game,[&]{return !G::hasOperation(*s.game);},"post-video reload publication");
    require(model().opens==opens&&model().closes==closes&&model().resourceLoads==2&&model().audioStatus==SDL_AUDIO_PLAYING&&
        s.game->responsiveAudioGeneration()==generation,"unchanged-plan post-video reload reuses the exact device generation");
    require(Options::audioSampleRate==22050&&Options::audioBitDepth==16&&Options::audioChunkSize==1024&&!Options::mute&&
        backends()[reloadBackend]->actualReaps==1&&model().unpauses==unpauses+1,"post-video reload actual reap precedes one owner unpause");
    finish(*s.game);s.game.reset();
}
void resizeTransition(bool detach) {
    Scenario s;s.files.push_back(syntheticFile("transition-resize.flc",true));s.begin();
    if(detach){s.attach();enqueueKey(SDLK_ESCAPE);until(*s.game,[&]{return G::hasOperation(*s.game);},"resize detach reservation");}
    auto controls=backends();auto control=controls[controls.size()-2];control->tls.hold();
    auto lease=V::lease(*s.video);step(*s.game);waitFor(control->tlsEntered,1,"transition resize actual TLS hold");
    const unsigned resets=model().windowResets;SDL_Event event={};event.type=SDL_VIDEORESIZE;event.resize.w=960;event.resize.h=720;enqueue(event);
    for(unsigned i=0;i<64;++i)step(*s.game);
    require(model().windowResets==resets&&!lease.expired()&&control->actualReaps==0,"resize cannot reset an active/TLS-borrowed surface");
    control->tls.release();
    if(!detach){s.attach();require(model().windowResets==resets,"attach completion keeps resize deferred through playback");enqueueKey(SDLK_ESCAPE);}
    s.removed();require(lease.expired()&&Options::displayWidth==960&&Options::displayHeight==720,"resize applied after detach/reap and finish return");
    require(model().windowResets>resets,"actual deferred resetDisplay called");s.finishCase();
}
void noHookFailure(const char *name) {
    const std::string mode=name;const bool close=mode=="prep-close";
    const bool wrong=mode=="prep-wrong-reason"||mode=="prep-changed-spec";
    Scenario s;s.files.push_back(syntheticFile("prep-failure.flc",true));
    if(mode=="prep-invalid-pcm")Options::musicVolume=129; // finite gain outside admitted [0,1]
    if(mode=="prep-allocation")model().failVideoStorageAllocation=true;
    if(mode=="prep-conversion"||close)model().failConversion=true;
    s.begin();require(phase(*s.video,"AwaitAttach"),"preparation failure is a reserved actual operation");
    auto lease=V::lease(*s.video);auto control=backends()[s.attachBackend];control->tls.hold();
    if(mode=="prep-wrong-reason")model().failHaltMusic=true;
    if(mode=="prep-changed-spec")model().frequency=48000;
    step(*s.game);waitFor(control->tlsEntered,1,"failed preparation terminal before real TLS reap");
    if(close)enqueueQuit();
    for(unsigned i=0;i<64;++i)step(*s.game);
    require(!lease.expired()&&model().unpauses==s.unpauses&&model().musicPlays==s.plays,"failed preparation cannot publish before reap");
    control->tls.release();
    if(wrong) {
        until(*s.game,[&]{return control->actualReaps.load()==1;},"rejected no-hook claim actual reap");
        for(unsigned i=0;i<64;++i)step(*s.game);
        require(G::hasOperation(*s.game)&&!lease.expired()&&model().unpauses==s.unpauses,"wrong reason/spec cannot mint menu resume grant");
        require(model().opens==s.opens&&model().closes==s.closes,"rejected no-hook grant retains native context");
        std::printf("LOADING_VIDEO_INTEGRATION PASS %s retained-nohook conditional-native\n",name);
        require(std::fflush(stdout)==0,"complete retained nohook stream");std::_Exit(0);
    }
    s.removed();require(lease.expired()&&model().musicPlays==s.plays,"pre-hook failure releases only reaped unhooked lease");
    require(model().unpauses==s.unpauses+(close?0u:1u),"only qualified non-close nohook failure resumes menu");
    if(mode=="prep-allocation")require(model().allocationFailures==1,"exact converted-storage allocation fault");
    if(close){finish(*s.game);s.game.reset();}else s.finishCase();
}
void missingUfo(bool external) {
    Scenario s(external,false);s.files.push_back(syntheticFile("missing-ufo.flc",true));if(external)s.tracks.push_back("TRACK");
    s.begin(true);s.attach();require(V::route(*s.video)==unsigned(external?Route::ExplicitTrack:Route::InternalPCM),"requested UFO without assets uses declared fallback");
    require(!saw("sound-play"),"missing UFO does not borrow Sound sequence");
    if(external)require(model().musicPlays==s.plays+1,"missing UFO preserves explicit track");
    else require(model().hooks==1,"missing UFO preserves actual internal PCM hook");
    s.removed(100);s.finishCase();
}
void ufoSound() {
    Scenario s(true,true);s.files.push_back(syntheticFile("ufo-sound.flc",false,false,220));s.begin(true);s.attach();
    until(*s.game,[&]{return V::frameCount(*s.video)>=151;},"actual UFO Sound sequence frame",25);
    require(saw("sound-play")&&model().musicPlays>s.plays,"actual UFO Music and Sound methods both reached playback leaves");
    const unsigned halts=model().channelHalts;auto lease=V::lease(*s.video);enqueueKey(SDLK_ESCAPE);s.removed();
    require(model().channelHalts>halts&&lease.expired(),"UFO Sound borrower halted and actually reaped before release");s.finishCase();
}
void clockWrap() {
    Scenario s;s.files.push_back(syntheticFile("clock-wrap.flc",false,false,8));s.begin();s.attach();
    require(V::frameCount(*s.video)==0,"no frame before actual first step");
    model().clock=0xfffffff0u;tick(*s.game);require(V::frameCount(*s.video)==1,"first frame before clock wrap");
    model().clock=0xfffffffau;tick(*s.game);require(V::frameCount(*s.video)==1,"pre-wrap elapsed interval waits");
    model().clock=3u;tick(*s.game);require(V::frameCount(*s.video)==1,"wrapped elapsed nineteen milliseconds waits");
    model().clock=4u;tick(*s.game);require(V::frameCount(*s.video)==2,"wrapped elapsed twenty milliseconds advances exactly once");
    const unsigned flips=model().flips;tick(*s.game);require(V::frameCount(*s.video)==2&&model().flips==flips,"no double presentation after wrapped deadline");
    enqueueKey(SDLK_ESCAPE);s.removed();s.finishCase();
}
void setActive(bool active) {
    model().focus=active?Uint8(SDL_APPACTIVE|SDL_APPINPUTFOCUS):Uint8(0);
    SDL_Event event={};event.type=SDL_ACTIVEEVENT;event.active.gain=active?1:0;event.active.state=SDL_APPACTIVE;enqueue(event);
}
void minimized(bool fade) {
    Scenario s;s.files.push_back(syntheticFile("minimized.flc",false,false,12,true));s.begin();s.attach();
    if(fade)until(*s.game,[&]{return phase(*s.video,"Fade");},"visible video reaches fade",25);
    setActive(false);const unsigned flips=model().flips,pauses=model().pauses,unpauses=model().unpauses;
    const unsigned palettes=model().physicalPaletteChanges,frames=V::frameCount(*s.video);
    for(unsigned i=0;i<3;++i)step(*s.game,50);
    require(model().flips==flips&&model().physicalPaletteChanges==palettes,"minimized active video/fade has no physical presentation");
    require(model().pauses==pauses&&model().unpauses==unpauses,"minimized timeline never generic-pauses audio");
    if(!fade)require(V::frameCount(*s.video)>frames,"minimized actual decoder continues advancing");
    setActive(true);step(*s.game,50);require(model().flips==flips+1,"restored video/fade presents exactly once");
    const unsigned restored=model().flips;step(*s.game,0);require(model().flips==restored,"restore does not double-present before next deadline");
    // Finish minimized. The palette interval above ends before the safe display
    // configuration restoration, which may reset the display after retirement.
    setActive(false);step(*s.game,1);const unsigned finalFlips=model().flips;enqueueKey(SDLK_ESCAPE);s.removed();
    require(model().flips==finalFlips,"minimized final cleanup suppresses final frame flip");s.finishCase();
}
struct StartupRun {
    Route expected;
    std::uint64_t deadline;
    std::weak_ptr<void> lease;
    bool sawVideo=false,sawMenu=false,reloadReserved=false,quitRequested=false;
    unsigned videoOpens=0,videoCloses=0,videoPlays=0;
    std::uint64_t videoGeneration=0;
    std::size_t reloadBackend=0;
    unsigned reloadSaves=0,reloadMaps=0,reloadConstructed=0,reloadDestroyed=0,reloadUnpauses=0;
    StartupRun(Route route,std::uint64_t limit):expected(route),deadline(limit) {}
};
StartupRun *startupRun=nullptr;
void startupPoll(Game &game) {
    require(startupRun&&monotonicNanoseconds()<startupRun->deadline,"bounded actual Game::run intro/reload");
    model().clock.fetch_add(25);
    if(G::hasOperation(game)||G::workerOwnsEpoch(game))return;
    State *top=G::topState(game);
    if(auto *video=dynamic_cast<VideoState*>(top)) {
        if(phase(*video,"Playing")) {
            require(V::route(*video)==unsigned(startupRun->expected),"startup RuleVideo selects exact route");
            if(!startupRun->sawVideo){startupRun->sawVideo=true;startupRun->lease=V::lease(*video);startupRun->videoOpens=model().opens;startupRun->videoCloses=model().closes;startupRun->videoPlays=model().musicPlays;startupRun->videoGeneration=game.responsiveAudioGeneration();}
        }
        return;
    }
    auto *menu=dynamic_cast<MainMenuState*>(top);if(!menu||!startupRun->sawVideo)return;
    require(startupRun->lease.expired(),"startup video borrows fully retired before actual menu access");
    require(model().audioStatus==SDL_AUDIO_PLAYING,"actual startup menu has resumed audio after finishing callback");
    require(Options::audioSampleRate==22050&&Options::audioBitDepth==16&&Options::audioChunkSize==1024&&!Options::mute,
        "full-run reload keeps the exact startup audio plan");
    require(game.responsiveAudioGeneration()==startupRun->videoGeneration,"unchanged-plan reload preserves exact device generation");
    if(!startupRun->reloadReserved) {
        startupRun->sawMenu=true;
        require(model().opens==startupRun->videoOpens&&model().closes==startupRun->videoCloses,"startup video kept fixed device generation");
        if(startupRun->expected==Route::UfoSequence)require(saw("sound-play"),"full UFO intro reached real Sound command");
        const unsigned plays=model().musicPlays;game.getMod()->getMusic("FIXTURE_CATALOG")->play(0);
        require(model().musicPlays==plays+1,"actual ordinary menu Music after intro");
        require(queuedEvents()==0,"handler admission uses empty event queue");
        menu->btnModsClick(nullptr);auto *mods=dynamic_cast<ModListState*>(G::topState(game));require(mods,"actual MainMenu opens actual ModList");
        startupRun->reloadBackend=backends().size();startupRun->reloadSaves=model().saves;startupRun->reloadMaps=model().maps;
        startupRun->reloadConstructed=model().modConstructed;startupRun->reloadDestroyed=model().modDestroyed;startupRun->reloadUnpauses=model().unpauses;
        Options::reload=true;mods->btnOkClick(nullptr);
        require(G::hasOperation(game),"actual ModList OK reserves reload without inline worker");startupRun->reloadReserved=true;return;
    }
    require(model().resourceLoads==2&&model().opens==startupRun->videoOpens&&model().closes==startupRun->videoCloses,
        "actual Game::run completed one unchanged-plan menu reload without device replacement");
    require(model().saves==startupRun->reloadSaves+1&&model().maps==startupRun->reloadMaps+1&&
        model().modConstructed==startupRun->reloadConstructed+1&&model().modDestroyed==startupRun->reloadDestroyed+1,
        "actual menu reload persists, remaps and replaces the resource graph once");
    const auto controls=backends();
    require(startupRun->reloadBackend<controls.size()&&controls[startupRun->reloadBackend]->actualReaps==1&&
        model().unpauses==startupRun->reloadUnpauses+1,"actual reload reap precedes its single owner unpause");
    startupRun->quitRequested=true;game.quit();
}
void startupIntro(const char *name) {
    const bool ufo=!std::strcmp(name,"startup-ufo"),external=!std::strcmp(name,"startup-explicit");
    reset();configureVideoAssets(external||ufo,ufo);if(ufo)setSoundFolder({"intro.cat"});
    const std::string path=syntheticFile("startup-intro.flc",!ufo,false,ufo?220:6);
    configureIntro({path},external?std::vector<std::string>{"TRACK"}:std::vector<std::string>{},ufo);
    auto game=makeGame();StartupRun state{ufo?Route::UfoSequence:external?Route::ExplicitTrack:Route::InternalPCM,monotonicNanoseconds()+10000000000ULL};
    startupRun=&state;setPollHook(&startupPoll);
    require(game->reserveResponsiveLoad(Game::LoadingOrigin::Startup)==Game::LoadingAdmission::Accepted,"actual full-run startup reservation");
    game->run();setPollHook(nullptr);startupRun=nullptr;
    require(state.sawVideo&&state.sawMenu&&state.reloadReserved&&state.quitRequested,"actual startup Cutscene/video/menu/reload path completed");
    require(G::finalClosed(*game)&&state.lease.expired()&&model().openCount==0,"actual Game::run final cleanup and video retirement");
    require(model().opens==1&&model().closes==1&&model().resourceLoads==2,"startup and unchanged-plan reload share one device until final checked close");
    require(model().activeOwnerViolations==0&&model().workerSubsystemViolations==0,"full-run owner/worker exclusion");
    game.reset();releaseAllGates();
}
void laterAttachFailure(const char *name) {
    const bool noThread=!std::strcmp(name,"later-attach-nothread"),entered=!std::strcmp(name,"later-attach-entered");
    Scenario s;s.files={syntheticFile("later-first.flc",true),syntheticFile("later-second.flc",true)};s.begin();s.attach();
    auto firstLease=V::lease(*s.video);
    until(*s.game,[&]{return active(*s.game,s.video)&&phase(*s.video,"Prepare");},"first clip detached before second preparation",100);
    require(!firstLease.expired(),"first exact detached grant retained between clips");
    const unsigned unpauses=model().unpauses,pauses=model().pauses;const std::size_t next=backends().size();
    if(!entered)setNextBackendFault(noThread?BackendControl::Fault::NoThreadOnce:BackendControl::Fault::SetupFailureOnce);
    step(*s.game);require(phase(*s.video,"AwaitAttach"),"second attach reserved separately");
    auto secondLease=V::lease(*s.video);auto control=backends()[next];
    if(!noThread)control->tls.hold();if(entered)model().failHaltMusic=true;
    step(*s.game);
    if(!noThread){waitFor(control->tlsEntered,1,"later failed attach TLS held");for(unsigned i=0;i<32;++i)step(*s.game);
        require(control->actualReaps==0&&!firstLease.expired()&&!secondLease.expired()&&model().unpauses==unpauses,"old grant cannot bypass later retirement");control->tls.release();}
    if(entered){until(*s.game,[&]{return control->actualReaps.load()==1;},"entered later failure actual reap");for(unsigned i=0;i<32;++i)step(*s.game);
        require(G::hasOperation(*s.game)&&!firstLease.expired()&&!secondLease.expired()&&model().unpauses==unpauses,"entered uncertain attach cannot reuse earlier resume grant");
        std::puts("LOADING_VIDEO_INTEGRATION PASS later-attach-entered retained-nohook conditional-native");
        require(std::fflush(stdout)==0,"complete later retained result");std::_Exit(0);}
    s.removed();require(firstLease.expired()&&secondLease.expired(),"unentered retired attach releases both exact grants");
    require(control->starts==1&&control->entries==unsigned(!noThread)&&control->actualReaps==unsigned(!noThread),"NoThread versus setup actual retirement distinction");
    require(model().pauses==pauses&&model().unpauses==unpauses+1,"unentered failure performs no native mutation and later resumes menu once");s.finishCase();
}
void renderFailure() {
    Scenario s;s.files.push_back(syntheticFile("render.flc",true));s.begin();s.attach();auto lease=V::lease(*s.video);
    model().failFlip=true;step(*s.game,100);model().failFlip=false;
    enqueueKey(SDLK_ESCAPE);s.removed();require(lease.expired(),"render failure cannot suppress detach");s.finishCase();
}
}
int main(int argc,char **argv) {
    require(argc==2,"exact case argument");const char *name=argv[1];
    try {
        if(!std::strcmp(name,"internal")||!std::strcmp(name,"explicit")||!std::strcmp(name,"ufo")||!std::strcmp(name,"silent")||!std::strcmp(name,"no-audio"))routeCase(name);
        else if(!std::strcmp(name,"skip-attach")||!std::strcmp(name,"quit-attach"))pendingAttach(name);
        else if(!std::strcmp(name,"held-detach"))heldDetach();
        else if(!std::strcmp(name,"empty")||!std::strcmp(name,"malformed")||!std::strcmp(name,"missing")||!std::strcmp(name,"multiple"))invalidFiles(name);
        else if(!std::strcmp(name,"mute-toggle")||!std::strcmp(name,"resize-focus"))resizeFocusMute(name);
        else if(!std::strcmp(name,"detach-nothread")||!std::strcmp(name,"detach-setup"))failedDetach(name);
        else if(!std::strcmp(name,"render-failure"))renderFailure();
        else if(!std::strcmp(name,"later-attach-nothread")||!std::strcmp(name,"later-attach-setup")||!std::strcmp(name,"later-attach-entered"))laterAttachFailure(name);
        else if(!std::strcmp(name,"muted-ufo")||!std::strcmp(name,"closed-ufo"))silentUfo(!std::strcmp(name,"closed-ufo"));
        else if(!std::strcmp(name,"menu-after-video"))menuAfterVideo();
        else if(!std::strcmp(name,"missing-ufo-internal")||!std::strcmp(name,"missing-ufo-explicit"))missingUfo(!std::strcmp(name,"missing-ufo-explicit"));
        else if(!std::strcmp(name,"ufo-sound"))ufoSound();
        else if(!std::strcmp(name,"clock-wrap"))clockWrap();
        else if(!std::strcmp(name,"minimized-play")||!std::strcmp(name,"minimized-fade"))minimized(!std::strcmp(name,"minimized-fade"));
        else if(!std::strcmp(name,"startup-internal")||!std::strcmp(name,"startup-explicit")||!std::strcmp(name,"startup-ufo"))startupIntro(name);
        else if(!std::strcmp(name,"resize-attach")||!std::strcmp(name,"resize-detach"))resizeTransition(!std::strcmp(name,"resize-detach"));
        else if(!std::strcmp(name,"prep-invalid-pcm")||!std::strcmp(name,"prep-allocation")||!std::strcmp(name,"prep-conversion")||!std::strcmp(name,"prep-close")||!std::strcmp(name,"prep-wrong-reason")||!std::strcmp(name,"prep-changed-spec"))noHookFailure(name);
        else if(!std::strcmp(name,"pop-attach")||!std::strcmp(name,"pop-detach")||!std::strcmp(name,"pop-tls")||!std::strcmp(name,"pop-play"))popDuringVideo(name);
        else if(!std::strcmp(name,"skip-prepare")||!std::strcmp(name,"quit-prepare")||!std::strcmp(name,"skip-play")||!std::strcmp(name,"quit-play")||
                !std::strcmp(name,"skip-hold")||!std::strcmp(name,"quit-hold")||!std::strcmp(name,"skip-fade")||!std::strcmp(name,"quit-fade")||
                !std::strcmp(name,"skip-detach")||!std::strcmp(name,"quit-detach"))skipPhase(name);
        else fail("unknown video case");
        std::printf("LOADING_VIDEO_INTEGRATION PASS %s conditional-native actual-Game\n",name);return 0;
    }catch(...){fail("unexpected exception");}
}
