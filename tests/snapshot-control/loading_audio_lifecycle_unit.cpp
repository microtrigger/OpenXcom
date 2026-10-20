// SPDX-License-Identifier: GPL-3.0-or-later
// Source-authored candidate, 2026-10-15. No execution is authorized by this file.
// Link actual LoadingAudioHandoff.cpp, Options.cpp, and (music ON) adlplayer.cpp /
// fmopl.cpp; observer ON additionally links actual MixedAudioObserver.cpp and
// DiagnosticProfile.cpp. Section GC may discard unused video entry points;
// the separate video fixture must link and exercise them with actual AudioLease.
// No replacement handoff, copied state machine, private access or test dispatch.
// ELF --wrap leaves: SDL_WasInit, SDL_AudioDriverName, SDL_PauseAudio,
// SDL_LockAudio, SDL_UnlockAudio, SDL_GetAudioStatus, Mix_QuerySpec,
// Mix_SetPostMix, Mix_HookMusic, Mix_HaltChannel, Mix_HaltMusic, Mix_OpenAudio,
// Mix_CloseAudio; music ON _Z9func_mutev (forwarded to actual engine); observer
// ON _ZN8OpenXcom8Observer26diagnosticProfileQualifiedEv. Symbol names and exact
// object/provider closure require fresh command review, not this comment.
// Each named case is a separate process. Model leaves establish conditional
// ordering only; they do not qualify native providers, UI latency or Game reap.
#include "../../src/Engine/LoadingAudioHandoff.h"
#include "../../src/Engine/Options.h"
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
#include "../../src/Observer/MixedAudioObserver.h"
#include <sys/socket.h>
#include <unistd.h>
#endif
#ifndef __NO_MUSIC
#include "../../src/Engine/Adlib/fmopl.h"
extern FM_OPL *opl[2];
#endif
#include <SDL_mixer.h>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#if !defined(OPENXCOM_RESPONSIVE_LOADING) || !defined(OPENXCOM_LOADING_AUDIO_HANDOFF)
#error "Lifecycle fixture requires the responsive helper profile"
#endif
namespace {
using H = OpenXcom::LoadingAudioHandoff;
const std::thread::id owner = std::this_thread::get_id();
static_assert(!std::is_copy_constructible<H::ClosedProof>::value, "closed proof is exclusive");
static_assert(!std::is_copy_constructible<H::CloseReceipt>::value, "close receipt is exclusive");
static_assert(!std::is_default_constructible<H::ResourceMutationPermit>::value, "no public permit mint");
[[noreturn]] void fail(const char *what) noexcept {
    std::fprintf(stderr,"LOADING_AUDIO_LIFECYCLE FAIL %s\n",what); std::_Exit(3);
}
void check(bool ok,const char *what) noexcept { if (!ok) fail(what); }
enum E { Query, Status, Open, Pause, Lock, Unlock, Postmix, Hook, Channels, Music, Mute, Destroy, Close };
struct Model {
    int count=0, rate=44100, channels=2, openResult=0, negotiated=44100, afterOpen=1, afterClose=0;
    Uint16 format=AUDIO_S16SYS;
    SDL_audiostatus status=SDL_AUDIO_STOPPED;
    bool subsystem=true, driver=true;
    int haltChannels=0, haltMusic=0;
    E events[128]; unsigned used=0, mutations=0, opens=0, closes=0;
    std::mutex trace, audio, gate;
    std::condition_variable changed;
    bool pauseSeen=false, callbackHeld=false, releaseCallback=false;
} m;
thread_local bool locked=false;
// Only the intended failing operation on its own thread may satisfy death.
thread_local bool deathArmed=false;
void record(E event,bool mutation=false) noexcept {
    check(std::this_thread::get_id()!=owner,"owner native operation");
    std::lock_guard<std::mutex> lock(m.trace);
    check(m.used<128,"bounded trace"); m.events[m.used++]=event;
    if (mutation) ++m.mutations;
}
H::AudioPlan plan() { return H::AudioPlan{44100,AUDIO_S16SYS,2,1024}; }
H::Generation known() {
    H::Generation g; g.id=7; g.provenance=H::Provenance::KnownSinglePublished;
    g.spec=H::ActualMixerSpec{1,44100,AUDIO_S16SYS,2};
    m.count=1; m.status=SDL_AUDIO_PLAYING; return g;
}
void seal(H &h) { h.sealForWorker(); }
void destroyed() { record(Destroy); }
void noMutation() { check(m.mutations==0 && m.opens==0 && m.closes==0,"rejection before mutation"); }
void freshOrder() {
    unsigned i=0; while (i<m.used && m.events[i]!=Open) ++i;
    check(i+3<m.used && m.events[i+1]==Pause && m.events[i+2]==Lock && m.events[i+3]==Unlock,
          "auto-unpause is followed immediately by pause and same-mutex drain");
}
void normalCase(const char *name) {
    if (!std::strcmp(name,"closed-zero") || !std::strcmp(name,"closed-no-subsystem")) {
        if (!std::strcmp(name,"closed-no-subsystem")) m.subsystem=false;
        H h; seal(h); H::Generation g;
        std::thread work([&]{auto proof=h.verifyClosed(g,true); check(proof.validFor(0),"known closed proof");
            auto receipt=h.terminalCloseNeverOpened(proof,g); check(receipt.validFor(0),"terminal closed receipt");
            check(!proof.valid(),"terminal consumes closed proof");}); work.join();
        noMutation(); if (!m.subsystem) check(!m.used,"no native calls without subsystem");
    } else if (!std::strcmp(name,"fresh-close") || !std::strcmp(name,"open-failure-closed") ||
               !std::strcmp(name,"open-failure-retained") || !std::strcmp(name,"negotiated-rate") ||
               !std::strcmp(name,"fresh-multiple")) {
        if (!std::strcmp(name,"open-failure-closed")) {m.openResult=-1;m.afterOpen=0;}
        if (!std::strcmp(name,"open-failure-retained")) {m.openResult=-1;m.afterOpen=1;}
        if (!std::strcmp(name,"negotiated-rate")) m.negotiated=48000;
        if (!std::strcmp(name,"fresh-multiple")) m.afterOpen=2;
        H old,fresh; seal(old);seal(fresh);H::Generation g;
        std::thread work([&]{auto proof=old.verifyClosed(g,true); check(proof.valid(),"closed before open");
            auto ready=fresh.openFresh(proof,plan(),g);check(!proof.valid(),"open consumes proof");
            if (!std::strcmp(name,"fresh-close")) {
                check(ready.outcome.disposition==H::Disposition::ResourceReady && ready.permit.validFor(fresh),"fresh permit");
                check(g.id==1 && g.provenance==H::Provenance::KnownSinglePaused,"published actual fresh generation");
                destroyed();auto closed=fresh.closePrepared(ready.permit,g,true,true);
                check(!ready.permit.validFor(fresh) && closed.validFor(1),"close receipt consumes permit");
                auto transfer=closed.releaseClosedProof();check(transfer.validFor(1),"move-only reopen proof");
                check(!closed.releaseClosedProof().valid(),"no duplicate proof transfer");
            } else if (!std::strcmp(name,"open-failure-closed")) {
                check(ready.outcome.disposition==H::Disposition::ClosedReady && ready.outcome.reason==H::Reason::OpenFailedClosed,
                      "failed open independently confirmed closed");
                check(!ready.permit.validFor(fresh) && g.id==0 && g.provenance==H::Provenance::SilentClosedNoNativeResources,"no resource permit on silent failure");
            } else {
                check(ready.outcome.disposition==H::Disposition::FailedRetained && !ready.permit.validFor(fresh),"uncertain/mismatch retained");
                check(ready.outcome.reason==(!std::strcmp(name,"open-failure-retained") ? H::Reason::OpenFailedRetained : H::Reason::GenerationMismatch),"typed open failure");
            }
        });work.join();
        check(m.opens==1,"one native open attempt");
        if (m.openResult==0) freshOrder();
        check(m.closes==((!std::strcmp(name,"fresh-close") || !std::strcmp(name,"negotiated-rate")) ? 1u : 0u),"no guessed count-decrement close");
        if (!std::strcmp(name,"open-failure-closed")) check(m.mutations==1,"failed zero open has no stale halt/pause");
    } else if (!std::strcmp(name,"reload") || !std::strcmp(name,"resources-retained") ||
               !std::strcmp(name,"close-count-changed") || !std::strcmp(name,"close-unverified")) {
        H::Generation g=known();H old,fresh;seal(old);seal(fresh);
        std::thread work([&]{auto ready=old.prepareKnown(g,plan());check(ready.permit.validFor(old),"old mutation permit");
            if (!std::strcmp(name,"close-count-changed")) m.count=2;
            if (!std::strcmp(name,"close-unverified")) m.afterClose=1;
            const bool empty=std::strcmp(name,"resources-retained")!=0;if(empty)destroyed();
            auto receipt=old.closePrepared(ready.permit,g,empty,false);
            check(!ready.permit.validFor(old),"close always consumes permit");
            if (!std::strcmp(name,"reload")) {
                check(receipt.validFor(7),"old close binding");auto proof=receipt.releaseClosedProof();
                auto newer=fresh.openFresh(proof,plan(),g);check(newer.permit.validFor(fresh) && g.id==8,"second generation");
                newer.permit.invalidate();check(!proof.valid(),"reload proof consumed");
            } else check(!receipt.valid(),"failed close cannot certify release");
        });work.join();
        if (!std::strcmp(name,"resources-retained") || !std::strcmp(name,"close-count-changed")) check(m.closes==0,"refusal before native close");
        else check(m.closes==1,"exactly one close, no decrement loop");
        if (!std::strcmp(name,"reload")) freshOrder();
    } else if (!std::strcmp(name,"reject-unknown") || !std::strcmp(name,"reject-zero") ||
               !std::strcmp(name,"reject-multiple") || !std::strcmp(name,"reject-rate") ||
               !std::strcmp(name,"reject-driver") || !std::strcmp(name,"reject-plan") ||
               !std::strcmp(name,"closed-resources") || !std::strcmp(name,"overflow")) {
        H::Generation g=known();
        if (!std::strcmp(name,"reject-unknown"))g.provenance=H::Provenance::Invalidated;
        if (!std::strcmp(name,"reject-zero"))m.count=0;
        if (!std::strcmp(name,"reject-multiple"))m.count=2;
        if (!std::strcmp(name,"reject-rate"))m.rate=48000;
        if (!std::strcmp(name,"reject-driver"))m.driver=false;
        H h,fresh;seal(h);seal(fresh);
        std::thread work([&]{
            if (!std::strcmp(name,"closed-resources")) {g=H::Generation();m.count=0;m.status=SDL_AUDIO_STOPPED;
                auto p=h.verifyClosed(g,false);check(!p.valid() && p.outcome().reason==H::Reason::NativeResourcesRemain,"closed retains resources");}
            else if (!std::strcmp(name,"overflow")) {g=H::Generation();g.id=std::numeric_limits<std::uint64_t>::max();m.count=0;m.status=SDL_AUDIO_STOPPED;
                auto p=h.verifyClosed(g,true);auto a=fresh.openFresh(p,plan(),g);check(a.outcome.reason==H::Reason::CounterOverflow && !a.permit.validFor(fresh),"generation overflow rejection");}
            else {auto p=plan();if(!std::strcmp(name,"reject-plan"))p.channels=1;
                auto a=h.prepareKnown(g,p);check(a.outcome.disposition==H::Disposition::RejectedRetained && !a.permit.validFor(h),"preflight refusal");}
        });work.join();noMutation();
    } else if (!std::strcmp(name,"halt-channels") || !std::strcmp(name,"halt-music")) {
        H::Generation g=known();H h;seal(h);
        if (!std::strcmp(name,"halt-channels"))m.haltChannels=-1;else m.haltMusic=-1;
        std::thread work([&]{auto a=h.prepareKnown(g,plan());check(!a.permit.validFor(h),"halt failure no permit");
            check(a.outcome.lastCompletedPhase==(m.haltChannels ? H::Phase::HooksDetached : H::Phase::ChannelsHalted),"failure retains distinct completed progress");});work.join();
    } else if (!std::strcmp(name,"callback-drain")) {
        H::Generation g=known();H h;seal(h);std::atomic<bool> complete(false);
        std::thread callback([&]{std::unique_lock<std::mutex> audio(m.audio);std::unique_lock<std::mutex> lock(m.gate);
            m.callbackHeld=true;m.changed.notify_all();m.changed.wait(lock,[]{return m.releaseCallback;});});
        {std::unique_lock<std::mutex> lock(m.gate);m.changed.wait(lock,[]{return m.callbackHeld;});}
        std::thread work([&]{auto a=h.prepareKnown(g,plan());check(a.permit.validFor(h),"drained permit");destroyed();a.permit.invalidate();complete.store(true);});
        {std::unique_lock<std::mutex> lock(m.gate);m.changed.wait(lock,[]{return m.pauseSeen;});
            check(!complete.load(),"pause is not callback return");
            {std::lock_guard<std::mutex> trace(m.trace);for(unsigned i=0;i<m.used;++i)check(m.events[i]!=Postmix && m.events[i]!=Destroy,"no detach/destruction before actual callback return");}
            m.releaseCallback=true;m.changed.notify_all();}
        callback.join();work.join();check(complete.load(),"callback release enables completion");
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
    } else if (!std::strcmp(name,"observer-terminal")) {
        int pair[2];check(socketpair(AF_UNIX,SOCK_SEQPACKET,0,pair)==0,"observer socketpair");
        char descriptor[32];std::snprintf(descriptor,sizeof(descriptor),"%d",pair[0]);
        OpenXcom::Observer::MixedAudioObserver observer(descriptor);check(observer.enabled(),"actual bootstrap descriptor");
        H::Generation g=known();H old(&observer);seal(old);
        std::thread work([&]{auto a=old.prepareKnown(g,plan());check(a.outcome.observer==H::ObserverDisposition::BootstrapPreserved,"reload preserves bootstrap");
            destroyed();auto c=old.closePrepared(a.permit,g,true,true);check(c.valid(),"terminal close");});work.join();
        check(!observer.enabled(),"terminal worker closes bootstrap socket without owner close API");close(pair[1]);
#endif
    } else fail("unknown case");
}
void deathCase(const char *name) {
    std::set_terminate([]{std::fprintf(stderr,"LOADING_AUDIO_LIFECYCLE TERMINATE %s\n",deathArmed?"ARMED":"UNARMED");std::_Exit(deathArmed?86:87);});
    H::Generation g=known();H h;seal(h);
    if (!std::strcmp(name,"death-owner")) {deathArmed=true;auto unexpected=h.prepareKnown(g,plan());deathArmed=false;(void)unexpected;fail("armed entry returned");}
    else if (!std::strcmp(name,"death-repeat")) {std::thread work([&]{auto a=h.prepareKnown(g,plan());a.permit.invalidate();deathArmed=true;auto unexpected=h.prepareKnown(g,plan());deathArmed=false;(void)unexpected;fail("armed entry returned");});work.join();}
    else if (!std::strcmp(name,"death-continuation-repeat")) {std::thread work([&]{auto a=h.prepareKnown(g,plan());destroyed();auto c=h.closePrepared(a.permit,g,true,false);check(c.valid(),"death setup completed");deathArmed=true;auto unexpected=h.closePrepared(a.permit,g,true,false);deathArmed=false;(void)unexpected;fail("armed continuation returned");});work.join();}
    else if (!std::strcmp(name,"death-continuation-before-entry")) {H::ClosedProof p;std::thread work([&]{deathArmed=true;auto unexpected=h.terminalCloseNeverOpened(p,g);deathArmed=false;(void)unexpected;fail("armed premature continuation returned");});work.join();}
    else if (!std::strcmp(name,"death-concurrent-entry")) {
        m.audio.lock();
        std::thread first([&]{auto a=h.prepareKnown(g,plan());a.permit.invalidate();});
        {std::unique_lock<std::mutex> lock(m.gate);m.changed.wait(lock,[]{return m.pauseSeen;});}
        std::thread second([&]{deathArmed=true;auto unexpected=h.prepareKnown(g,plan());deathArmed=false;(void)unexpected;fail("armed entry returned");});second.join();
        m.audio.unlock();first.join();
    } else if (!std::strcmp(name,"death-cross-worker-continuation")) {
        H::PrepareResult *ready=nullptr;bool published=false;
        std::thread first([&]{auto a=h.prepareKnown(g,plan());
            std::unique_lock<std::mutex> lock(m.gate);ready=&a;published=true;m.changed.notify_all();
            m.changed.wait(lock,[]{return m.releaseCallback;});a.permit.invalidate();});
        {std::unique_lock<std::mutex> lock(m.gate);m.changed.wait(lock,[&]{return published;});}
        std::thread second([&]{deathArmed=true;auto unexpected=h.closePrepared(ready->permit,g,true,false);deathArmed=false;(void)unexpected;fail("armed cross-worker continuation returned");});second.join();
        {std::lock_guard<std::mutex> lock(m.gate);m.releaseCallback=true;m.changed.notify_all();}first.join();
    }
    else fail("unknown death case");
    fail("death operation returned");
}
}
extern "C" {
Uint32 __wrap_SDL_WasInit(Uint32) {check(std::this_thread::get_id()==owner,"owner snapshot");return m.subsystem?SDL_INIT_AUDIO:0;}
char *__wrap_SDL_AudioDriverName(char *out,int len) {check(len>=6,"driver bound");std::strcpy(out,m.driver?"dummy":"other");return out;}
int __wrap_Mix_QuerySpec(int *rate,Uint16 *format,int *channels) {record(Query);if(m.count>0){*rate=m.rate;*format=m.format;*channels=m.channels;}return m.count;}
SDL_audiostatus __wrap_SDL_GetAudioStatus() {record(Status);return m.status;}
void __wrap_SDL_PauseAudio(int pause) {check(pause==1,"only pause");record(Pause,true);m.status=SDL_AUDIO_PAUSED;std::lock_guard<std::mutex> lock(m.gate);m.pauseSeen=true;m.changed.notify_all();}
void __wrap_SDL_LockAudio() {record(Lock);m.audio.lock();locked=true;}
void __wrap_SDL_UnlockAudio() {check(locked,"matching audio unlock");locked=false;m.audio.unlock();record(Unlock);}
void __wrap_Mix_SetPostMix(void (SDLCALL *fn)(void*,Uint8*,int),void *data) {check(!locked && !fn && !data,"postmix detach outside lock");record(Postmix,true);}
void __wrap_Mix_HookMusic(void (SDLCALL *fn)(void*,Uint8*,int),void *data) {check(!locked && !fn && !data,"music detach outside lock");record(Hook,true);}
int __wrap_Mix_HaltChannel(int channel) {check(!locked && channel==-1,"all channels");record(Channels,true);return m.haltChannels;}
int __wrap_Mix_HaltMusic() {check(!locked,"halt outside lock");record(Music,true);return m.haltMusic;}
int __wrap_Mix_OpenAudio(int rate,Uint16 format,int channels,int chunk) {check(rate==44100 && format==AUDIO_S16SYS && channels==2 && chunk==1024,"approved open plan");record(Open,true);++m.opens;m.count=m.afterOpen;m.rate=m.negotiated;m.status=m.count?SDL_AUDIO_PLAYING:SDL_AUDIO_STOPPED;return m.openResult;}
void __wrap_Mix_CloseAudio() {check(!locked && m.status==SDL_AUDIO_PAUSED,"close paused outside lock");record(Close,true);++m.closes;m.count=m.afterClose;m.status=m.count?SDL_AUDIO_PAUSED:SDL_AUDIO_STOPPED;}
#ifndef __NO_MUSIC
void __real__Z9func_mutev();
void __wrap__Z9func_mutev() {check(!opl[0] && !opl[1],"fixture retains legitimate null engine pair");record(Mute,true);__real__Z9func_mutev();}
#endif
#ifdef OPENXCOM_EXPERIMENTAL_MIXED_AUDIO_OBSERVER
bool __wrap__ZN8OpenXcom8Observer26diagnosticProfileQualifiedEv() {check(std::this_thread::get_id()==owner,"no worker observer diagnostics");return true;}
#endif
}
int main(int argc,char **argv) {
    check(argc==2,"one bounded case argument");
    // Actual Options storage: cleanup must not use mute as acquisition proof.
    OpenXcom::Options::mute=true;
    if (!std::strncmp(argv[1],"death-",6)) deathCase(argv[1]);else normalCase(argv[1]);
    std::printf("LOADING_AUDIO_LIFECYCLE PASS %s\n",argv[1]);return 0;
}
