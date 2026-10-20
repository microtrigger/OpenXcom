// SPDX-License-Identifier: GPL-3.0-or-later
// Source-only candidate. Link ACTUAL FlcPlayer and LoadingAudioHandoff. Native
// leaves below are a conditional model, not converter/device/provider proof.
// This fixture never substitutes a player, lease, handoff, receipt or scheduler.
// Required native wraps: SDL_WasInit, SDL_AudioDriverName, Mix_QuerySpec,
// SDL_GetAudioStatus, SDL_PauseAudio, SDL_LockAudio, SDL_UnlockAudio,
// Mix_SetPostMix, Mix_HookMusic, Mix_HaltChannel, Mix_HaltMusic,
// SDL_BuildAudioCVT, SDL_ConvertAudio, malloc/calloc/realloc/free and
// fopen/fprintf/fwrite. Global new/delete provide finite storage-failure
// injection and callback allocation guards. Music profiles forward func_mute.
// Per-process commands: no argument (all normal cases), --death-owner-prepare,
// --death-repeat-prepare, --death-receipt-owner. Native library/model and object
// ownership must be independently checked in the later command package.
#include "../../src/Engine/FlcPlayer.h"
#include "../../src/Engine/LoadingAudioHandoff.h"
#ifndef __NO_MUSIC
#include "../../src/Engine/Adlib/fmopl.h"
extern FM_OPL *opl[2];
#endif
#include <SDL_mixer.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <new>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#if !defined(OPENXCOM_RESPONSIVE_LOADING) || !defined(OPENXCOM_RESPONSIVE_TESTING)
#error This fixture requires the responsive nonshipping profile.
#endif
namespace OpenXcom {
class LoadingVideoAudioTestAccess {
public:
    using Lease = FlcPlayer::AudioLease;
    static std::shared_ptr<Lease> lease(Lease::Route route, std::uint64_t generation,
        std::vector<Uint8> source, unsigned rate, double gain) {
        return std::shared_ptr<Lease>(new Lease(route,generation,std::move(source),rate,gain));
    }
    static const std::vector<Uint8> &pcm(const Lease &lease) { return lease._pcm; }
    static std::size_t cursor(const Lease &lease) { return lease._cursor; }
    static unsigned frameBytes(const Lease &lease) { return lease._frameBytes; }
    static FlcPlayer::ResponsiveFailure inspect(const std::vector<Uint8> &file,
        unsigned &rate,unsigned &first,std::size_t &frames,std::size_t &audio) {
        FlcPlayer::ResponsiveData data;
        auto status=FlcPlayer::inspectResponsive(file.data(),file.size(),data);
        rate=data.sourceRate;first=data.firstAudioBytes;frames=data.frames.size();audio=data.audio.size();return status;
    }
    static void decodeSS2(const Uint8 *chunk,unsigned width,unsigned height,std::vector<Uint8> &pixels) {
        FlcPlayer player; SDL_Surface surface={};
        surface.w=int(width);surface.h=int(height);surface.pitch=Uint16(width);
        surface.pixels=pixels.data();player._mainScreen=&surface;player._offset=0;
        player._headerWidth=Uint16(width);player._headerHeight=Uint16(height);
        player._chunkData=const_cast<Uint8*>(chunk);player.fliSS2();
    }
    static Uint32 delay(Uint16 type,Uint16 speed,Uint16 overrideDelay,bool sourceTiming,
        unsigned rate,unsigned first) {
        FlcPlayer player;player._headerType=type;player._headerSpeed=speed;
        player._responsiveSourceTiming=sourceTiming;player._responsiveSourceRate=rate;
        player._responsiveFirstAudioBytes=first;
        FlcPlayer::ResponsiveFrame frame={0,16,0,overrideDelay};
        return player.responsiveFrameDelay(frame);
    }
};
}
namespace {
using F=OpenXcom::FlcPlayer;using A=F::AudioLease;using H=OpenXcom::LoadingAudioHandoff;
using Access=OpenXcom::LoadingVideoAudioTestAccess;
const std::thread::id owner=std::this_thread::get_id();
thread_local bool callbackGuard=false;
thread_local int failAllocation=-1;
thread_local unsigned allocationAttempt=0;
[[noreturn]] void fail(const char *s) noexcept {std::fprintf(stderr,"LOADING_VIDEO_AUDIO FAIL %s\n",s);std::_Exit(3);}
void require(bool ok,const char *s) noexcept {if(!ok)fail(s);}
static_assert(!std::is_constructible<A::AttachReceipt,A*,std::uint64_t,A::Route,bool>::value,"no valid receipt forgery");
static_assert(!std::is_constructible<A::DetachReceipt,A*,std::uint64_t,A::Route>::value,"no valid detach forgery");
static_assert(std::is_nothrow_default_constructible<A::AttachReceipt>::value,"invalid receipt is harmless");
struct Model {
    std::atomic<int> count{1},status{SDL_AUDIO_PLAYING};
    int rate=44100,channels=2;Uint16 format=AUDIO_S16SYS;
    std::atomic<unsigned> pauses{0},locks{0},drains{0},hooks{0},unhooks{0},halts{0},conversions{0};
    std::atomic<bool> lockEntered{false},callbackHeld{false},callbackRelease{false},callbackReturned{false};
    std::mutex audio;
    bool badBuild=false,badLength=false,badMultiplier=false;
    int sourceRate=0,destRate=0;Uint16 destFormat=0;
    Sint16 intermediate[256]={};unsigned captured=0;
    void (SDLCALL *hook)(void*,Uint8*,int)=nullptr;void *userdata=nullptr;
} m;
thread_local bool audioLocked=false;
thread_local bool deathArmed=false;
const char *deathName=nullptr;
void workerOnly() {
    if(callbackGuard){callbackGuard=false;fail("callback attempted native lock/control operation");}
    require(std::this_thread::get_id()!=owner,"native worker boundary");
}
void callback(A *lease,Uint8 *buffer,int bytes) {
    callbackGuard=true;A::callback(lease,buffer,bytes);callbackGuard=false;
}
void waitFor(const std::atomic<bool> &flag) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!flag.load(std::memory_order_acquire)) {if(std::chrono::steady_clock::now()>deadline)fail("watchdog");std::this_thread::yield();}
}
H::Generation generation() {H::Generation g;g.id=9;g.provenance=H::Provenance::KnownSinglePublished;g.spec={1,m.rate,m.format,2};return g;}
H::AudioPlan plan() {return H::AudioPlan{m.rate,m.format,2,1024};}
void put16(std::vector<Uint8> &v,std::size_t p,unsigned x){v[p]=Uint8(x);v[p+1]=Uint8(x>>8);}
void put32(std::vector<Uint8> &v,std::size_t p,unsigned x){for(unsigned i=0;i<4;++i)v[p+i]=Uint8(x>>(8*i));}
std::vector<Uint8> file(unsigned rate=22050,bool audio=true) {
    std::vector<Uint8> b(128,0);put16(b,4,0xaf12);put16(b,6,2);put16(b,8,4);put16(b,10,2);put16(b,12,8);put16(b,16,20);
    if(audio){std::size_t p=b.size();b.resize(p+20);put32(b,p,4);put16(b,p+4,0xaaaa);put16(b,p+8,rate);b[p+16]=0;b[p+17]=128;b[p+18]=255;b[p+19]=64;}
    for(unsigned i=0;i<2;++i){std::size_t p=b.size();b.resize(p+16);put32(b,p,16);put16(b,p+4,0xf1fa);}
    put32(b,0,b.size());return b;
}
F::ResponsiveFailure inspect(const std::vector<Uint8> &b) {unsigned rate=0,first=0;std::size_t frames=0,audio=0;return Access::inspect(b,rate,first,frames,audio);}
void signedOpcodeDecoder() {
    for(bool negativeSkip: {false,true}) {
        const unsigned width=negativeSkip?2u:256u,height=negativeSkip?2u:1u;
        std::vector<Uint8> b(128+16+6+2+(negativeSkip?2:0)+2+(negativeSkip?4:128*4),0);
        put32(b,0,b.size());put16(b,4,0xaf12);put16(b,6,1);put16(b,8,width);put16(b,10,height);put16(b,12,8);
        put32(b,128,b.size()-128);put16(b,132,0xf1fa);put16(b,134,1);
        put32(b,144,b.size()-144);put16(b,148,7);put16(b,150,1);
        std::size_t at=152;if(negativeSkip){put16(b,at,0xffff);at+=2;}
        put16(b,at,negativeSkip?1u:128u);at+=2;
        for(unsigned i=0;i<(negativeSkip?1u:128u);++i){b[at++]=0;b[at++]=1;b[at++]=37;b[at++]=91;}
        require(inspect(b)==F::ResponsiveFailure::None,"SS2 opcode accepted by actual parser");
        std::vector<Uint8> pixels(width*height,0);Access::decodeSS2(b.data()+144,width,height,pixels);
        for(unsigned i=0;i<width;++i)require(pixels[(negativeSkip?width:0)+i]==(i%2?91:37),"validated SS2 opcode matches actual decoder");
        if(negativeSkip)require(pixels[0]==0&&pixels[1]==0,"negative skip leaves first row untouched");
    }
}
void parsing() {
    auto b=file();unsigned rate=0,first=0;std::size_t frames=0,audio=0;
    require(Access::inspect(b,rate,first,frames,audio)==F::ResponsiveFailure::None && rate==22050 && first==4 && frames==2 && audio==4,"mixed record extents");
    auto empty=file(0,false);require(inspect(empty)==F::ResponsiveFailure::None,"no-audio is valid");
    auto bad=b;put32(bad,128,0);require(inspect(bad)==F::ResponsiveFailure::Record,"zero audio rejected");
    bad=b;put16(bad,136,0);require(inspect(bad)==F::ResponsiveFailure::Rate,"zero source rate");
    bad=b;put32(bad,0,bad.size()+1);require(inspect(bad)==F::ResponsiveFailure::Header,"header file extent");
    bad=empty;put32(bad,128,0);require(inspect(bad)==F::ResponsiveFailure::Record,"zero frame rejected");
    bad=empty;put16(bad,132,0xf100);put32(bad,128,8);require(inspect(bad)==F::ResponsiveFailure::Record,"short prefix rejected");
    bad=empty;bad.resize(128+16+8);put32(bad,0,bad.size());put32(bad,128,24);put16(bad,134,1);
    put32(bad,144,8);put16(bad,148,15);bad[150]=1;bad[151]=0;
    require(inspect(bad)==F::ResponsiveFailure::Record,"BRUN zero run cannot spin");
    bad=empty;put16(bad,134,1);require(inspect(bad)==F::ResponsiveFailure::Record,"chunk header cannot escape frame");
    bad=b;const std::size_t p=bad.size();bad.resize(p+17);put32(bad,p,1);put16(bad,p+4,0xaaaa);put16(bad,p+8,44100);put32(bad,0,bad.size());
    require(inspect(bad)==F::ResponsiveFailure::Rate,"rate cannot change mid-video");
    require(Access::delay(0xaf11,7,0,false,0,0)==100,"FLI rational ticks");
    require(Access::delay(0xaf11,7,23,false,0,0)==23,"FLI override");
    require(Access::delay(0xaf12,20,0,true,22050,2205)==100,"FLC source timing");
    require(Access::delay(0xaf12,20,0,false,22050,2205)==20,"external source timing");
    require(Access::delay(0xaf12,0,0,true,0,0)==14,"no-audio fallback");
}
A::PrepareStatus prepare(A &lease,int rate=44100,Uint16 format=AUDIO_S16SYS,int channels=2) {
    A::PrepareStatus status=A::PrepareStatus::InvalidPCM;
    std::thread work([&]{status=lease.prepareForDevice(rate,format,channels);});work.join();return status;
}
void conversion() {
    auto lease=Access::lease(A::Route::InternalPCM,9,{0,128,255},22050,0.5);
    require(prepare(*lease)==A::PrepareStatus::Ready,"golden prepare");
    require(m.captured==3 && m.intermediate[0]==-15360 && m.intermediate[1]==0 && m.intermediate[2]==15240,"captured U8 gain truncation");
    require(Access::pcm(*lease).size()==24 && Access::frameBytes(*lease)==4,"stereo resample length");
    Sint16 out[12]={};std::memcpy(out,Access::pcm(*lease).data(),sizeof out);
    const Sint16 expected[12]={-15360,-15360,-15360,-15360,0,0,0,0,15240,15240,15240,15240};
    require(!std::memcmp(out,expected,sizeof out),"conditional golden PCM");
    Uint8 dest[40];std::memset(dest,0x5a,sizeof dest);
    callback(lease.get(),dest,-1);callback(lease.get(),dest,0);callback(lease.get(),nullptr,4);
    require(Access::cursor(*lease)==0 && dest[0]==0x5a,"signed/null validation before memory");
    callback(nullptr,dest,4);require(dest[0]==0&&dest[3]==0&&Access::cursor(*lease)==0,"null userdata silences bounded output");
    std::memset(dest,0x5a,sizeof dest);
    callback(lease.get(),dest,3);require(Access::cursor(*lease)==0 && dest[0]==0 && dest[2]==0 && dest[3]==0x5a,"odd extent silenced without cursor");
    callback(lease.get(),dest,8);require(Access::cursor(*lease)==8 && !std::memcmp(dest,expected,8),"first callback slice");
    std::memset(dest,0x5a,sizeof dest);callback(lease.get(),dest,24);
    require(Access::cursor(*lease)==24 && !std::memcmp(dest,reinterpret_cast<const Uint8*>(expected)+8,16),"second callback advances destination/source");
    for(unsigned i=16;i<24;++i)require(dest[i]==0,"partial final silence");
    callback(lease.get(),dest,4);require(Access::cursor(*lease)==24 && !dest[0] && !dest[3],"EOF silence");
    for(unsigned rate: {11025u,22050u,44100u,48000u}){
        auto x=Access::lease(A::Route::InternalPCM,9,{0,128,255,64},rate,1.0);require(prepare(*x)==A::PrepareStatus::Ready,"supported conversion rate");}
    auto silent=Access::lease(A::Route::InternalPCM,9,{0,255},22050,0);require(prepare(*silent)==A::PrepareStatus::Ready,"zero gain");
    for(auto x:Access::pcm(*silent))require(x==0,"zero gain PCM");
    auto s8=Access::lease(A::Route::InternalPCM,9,{0,255},44100,1);require(prepare(*s8,44100,AUDIO_S8)==A::PrepareStatus::Ready && Access::frameBytes(*s8)==2,"signed8 stereo conversion");
    auto invalid=Access::lease(A::Route::InternalPCM,9,{1},0,1);require(prepare(*invalid)==A::PrepareStatus::InvalidPCM,"invalid source rate");
    invalid=Access::lease(A::Route::InternalPCM,9,{1},22050,std::numeric_limits<double>::quiet_NaN());require(prepare(*invalid)==A::PrepareStatus::InvalidPCM,"nonfinite gain");
    invalid=Access::lease(A::Route::InternalPCM,9,{1},22050,1);require(prepare(*invalid,44100,AUDIO_S16SYS,1)==A::PrepareStatus::InvalidSpec,"mono device rejected");
    m.badMultiplier=true;invalid=Access::lease(A::Route::InternalPCM,9,{1},22050,1);require(prepare(*invalid)==A::PrepareStatus::ConversionFailure,"bounded converter multiplier");m.badMultiplier=false;
    m.badLength=true;invalid=Access::lease(A::Route::InternalPCM,9,{1},22050,1);require(prepare(*invalid)==A::PrepareStatus::ConversionFailure,"whole-frame converted extent");m.badLength=false;
}
void allocationRollback() {
    for(int failure: {0,1}) {
        auto lease=Access::lease(A::Route::InternalPCM,9,{0,128,255,64},22050,1);
        A::PrepareStatus status=A::PrepareStatus::InvalidPCM;unsigned attempts=0;
        std::thread work([&]{allocationAttempt=0;failAllocation=failure;
            status=lease->prepareForDevice(44100,AUDIO_S16SYS,2);
            failAllocation=-1;attempts=allocationAttempt;});work.join();
        require(status==A::PrepareStatus::AllocationFailure&&attempts==unsigned(failure+1),"exact first/second PCM storage allocation failure");
        require(lease->canRelease()&&!lease->hasPCM()&&Access::pcm(*lease).empty()&&Access::cursor(*lease)==0,"allocation failure has no published PCM/hook");
        auto next=Access::lease(A::Route::InternalPCM,9,{0,128,255,64},22050,1);
        require(prepare(*next)==A::PrepareStatus::Ready,"fresh independent lease after allocation rollback");
    }
}
void attachRoute(A::Route route,bool empty=false) {
    auto lease=Access::lease(route,9,empty?std::vector<Uint8>():std::vector<Uint8>{0,128,255,64},22050,1);
    auto g=generation();H attach,detach;attach.sealForWorker();detach.sealForWorker();
    unsigned beforeHooks=m.hooks;
    std::thread first([&]{auto p=attach.prepareKnown(g,plan());require(p.permit.validFor(attach),"attach permit");auto r=attach.attachVideo(p.permit,g,*lease);
        require(r.outcome.disposition==H::Disposition::VideoAttached && r.receipt.validFor(9,lease.get()),"bound attach receipt");
        require(!p.permit.validFor(attach),"attach consumes permit");require(!r.receipt.validFor(10,lease.get()),"generation identity");
        require(r.receipt.hookInstalled()==(route==A::Route::InternalPCM && !empty),"exact route hook fact");});first.join();
    require(!lease->canRelease() && lease->attachmentState()==A::AttachmentState::Attached,"no-PCM routes remain borrowers");
    require(m.hooks-beforeHooks==unsigned(route==A::Route::InternalPCM&&!empty),"only internal PCM hooks");lease->notePlaybackStarted();
    std::thread second([&]{auto p=detach.prepareKnown(g,plan());auto r=detach.detachVideo(p.permit,g,*lease);
        require(r.outcome.disposition==H::Disposition::VideoDetached && r.receipt.validFor(9,lease.get()),"typed detached receipt");});second.join();
    require(lease->canRelease() && lease->attachmentState()==A::AttachmentState::Detached && !m.hook,"release only after actual worker join");
}
void closedAndRejected() {
    m.count=0;m.status=SDL_AUDIO_STOPPED;H::Generation g;
    auto lease=Access::lease(A::Route::Silent,0,{},0,0);H a,d;a.sealForWorker();d.sealForWorker();unsigned mutations=m.pauses+m.halts+m.unhooks;
    std::thread one([&]{auto p=a.verifyClosed(g,true);auto r=a.attachVideoClosed(p,g,*lease);require(r.receipt.validFor(0,lease.get())&&!r.receipt.hookInstalled(),"closed silent attached");});one.join();
    require(!lease->canRelease(),"closed lease still retained through detach/reap");
    std::thread two([&]{auto p=d.verifyClosed(g,true);auto r=d.detachVideoClosed(p,g,*lease);require(r.receipt.validFor(0,lease.get()),"closed silent detached");});two.join();
    require(lease->canRelease() && mutations==m.pauses+m.halts+m.unhooks,"closed route makes no stale native setters");
    m.count=1;m.status=SDL_AUDIO_PLAYING;auto known=generation();
    auto wrong=Access::lease(A::Route::InternalPCM,10,{128},22050,1);H mismatch;mismatch.sealForWorker();
    std::thread three([&]{auto p=mismatch.prepareKnown(known,plan());auto r=mismatch.attachVideo(p.permit,known,*wrong);require(!r.receipt.validFor(10,wrong.get())&&r.outcome.reason==H::Reason::InvalidLease,"wrong generation rejected");});three.join();require(wrong->canRelease(),"identity rejection before attach");
    auto invalid=Access::lease(A::Route::InternalPCM,9,{128},0,1);H bad;bad.sealForWorker();
    std::thread four([&]{auto p=bad.prepareKnown(known,plan());auto r=bad.attachVideo(p.permit,known,*invalid);require(r.outcome.reason==H::Reason::VideoInvalidPCM&&!r.receipt.validFor(9,invalid.get()),"conversion rejection has no receipt");});four.join();require(invalid->canRelease(),"never-attached failure safe after join");
}
void heldCallback() {
    auto lease=Access::lease(A::Route::InternalPCM,9,{0,128,255,64},22050,1);auto g=generation();H a,d;a.sealForWorker();d.sealForWorker();
    std::thread attach([&]{auto p=a.prepareKnown(g,plan());auto r=a.attachVideo(p.permit,g,*lease);require(r.receipt.validFor(9,lease.get()),"held attach");});attach.join();
    m.lockEntered=false;m.callbackHeld=false;m.callbackRelease=false;m.callbackReturned=false;std::atomic<bool> detached(false);
    std::thread callbackThread([&]{std::lock_guard<std::mutex> lock(m.audio);m.callbackHeld.store(true,std::memory_order_release);waitFor(m.callbackRelease);Uint8 bytes[8];callback(lease.get(),bytes,sizeof bytes);m.callbackReturned.store(true,std::memory_order_release);});
    waitFor(m.callbackHeld);
    std::thread detach([&]{auto p=d.prepareKnown(g,plan());require(m.callbackReturned.load(std::memory_order_acquire),"callback returned before detach");auto r=d.detachVideo(p.permit,g,*lease);require(r.receipt.validFor(9,lease.get()),"held detached receipt");detached.store(true,std::memory_order_release);});
    waitFor(m.lockEntered);require(!detached.load(std::memory_order_acquire),"drain blocked by actual callback mutex");
    m.callbackRelease.store(true,std::memory_order_release);callbackThread.join();detach.join();require(detached&&lease->canRelease(),"callback and worker independently joined");
}
[[noreturn]] void terminated() noexcept {
    if(!deathArmed)std::_Exit(87); // no fixture-state access on an unarmed path
    if(!deathName)std::_Exit(87);
    std::fprintf(stderr,"LOADING_VIDEO_AUDIO expected fail-stop %s\n",deathName);std::fflush(stderr);std::_Exit(86);
}
}
extern "C" {
void *__real_malloc(std::size_t);void *__real_calloc(std::size_t,std::size_t);
void *__real_realloc(void*,std::size_t);void __real_free(void*);
FILE *__real_fopen(const char*,const char*);
std::size_t __real_fwrite(const void*,std::size_t,std::size_t,FILE*);
}
void *operator new(std::size_t size) {
    if(callbackGuard){callbackGuard=false;fail("callback allocated C++ storage");}
    if(failAllocation>=0 && allocationAttempt++==unsigned(failAllocation))throw std::bad_alloc();
    void *p=__real_malloc(size?size:1);if(!p)throw std::bad_alloc();return p;
}
void *operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void *p) noexcept {if(callbackGuard){callbackGuard=false;fail("callback freed C++ storage");}__real_free(p);}
void operator delete[](void *p) noexcept {::operator delete(p);}
void operator delete(void *p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void *p,std::size_t) noexcept {::operator delete(p);}
extern "C" {
void *__wrap_malloc(std::size_t n){if(callbackGuard){callbackGuard=false;fail("callback malloc");}return __real_malloc(n);}
void *__wrap_calloc(std::size_t n,std::size_t size){if(callbackGuard){callbackGuard=false;fail("callback calloc");}return __real_calloc(n,size);}
void *__wrap_realloc(void *p,std::size_t n){if(callbackGuard){callbackGuard=false;fail("callback realloc");}return __real_realloc(p,n);}
void __wrap_free(void *p){if(callbackGuard){callbackGuard=false;fail("callback free");}__real_free(p);}
FILE *__wrap_fopen(const char *name,const char *mode){if(callbackGuard){callbackGuard=false;fail("callback logging fopen");}return __real_fopen(name,mode);}
int __wrap_fprintf(FILE *stream,const char *format,...){if(callbackGuard){callbackGuard=false;fail("callback logging fprintf");}va_list args;va_start(args,format);int n=std::vfprintf(stream,format,args);va_end(args);return n;}
std::size_t __wrap_fwrite(const void *p,std::size_t size,std::size_t n,FILE *stream){if(callbackGuard){callbackGuard=false;fail("callback logging fwrite");}return __real_fwrite(p,size,n,stream);}
Uint32 SDLCALL __wrap_SDL_WasInit(Uint32 mask){return mask&SDL_INIT_AUDIO;}
char * SDLCALL __wrap_SDL_AudioDriverName(char *out,int n){require(n>=6,"driver buffer");std::memcpy(out,"dummy",6);return out;}
int SDLCALL __wrap_Mix_QuerySpec(int *rate,Uint16 *format,int *channels){workerOnly();if(m.count){*rate=m.rate;*format=m.format;*channels=m.channels;}return m.count;}
SDL_audiostatus SDLCALL __wrap_SDL_GetAudioStatus(){workerOnly();return SDL_audiostatus(m.status.load());}
void SDLCALL __wrap_SDL_PauseAudio(int pause){workerOnly();require(pause==1,"worker pause only");++m.pauses;m.status=SDL_AUDIO_PAUSED;}
void SDLCALL __wrap_SDL_LockAudio(){workerOnly();m.lockEntered.store(true,std::memory_order_release);m.audio.lock();audioLocked=true;++m.locks;}
void SDLCALL __wrap_SDL_UnlockAudio(){require(audioLocked,"matched drain unlock");audioLocked=false;++m.drains;m.audio.unlock();}
void SDLCALL __wrap_Mix_SetPostMix(void(SDLCALL *callback)(void*,Uint8*,int),void *context){workerOnly();require(!audioLocked&&!callback&&!context,"postmix outside drain");}
void SDLCALL __wrap_Mix_HookMusic(void(SDLCALL *callback)(void*,Uint8*,int),void *context){workerOnly();require(!audioLocked,"hook setter outside drain");m.hook=callback;m.userdata=context;if(callback){require(context,"hook lease");++m.hooks;}else{require(!context,"null hook context");++m.unhooks;}}
int SDLCALL __wrap_Mix_HaltChannel(int channel){workerOnly();require(channel==-1&&!audioLocked,"halt all channels");++m.halts;return 0;}
int SDLCALL __wrap_Mix_HaltMusic(){workerOnly();require(!audioLocked,"halt music outside drain");++m.halts;return 0;}
int SDLCALL __wrap_SDL_BuildAudioCVT(SDL_AudioCVT *cvt,Uint16 source,Uint8 channels,int sourceRate,Uint16 dest,Uint8 destChannels,int destRate){
    workerOnly();require(source==AUDIO_S16SYS&&channels==1&&destChannels==2,"exact converter source");
    m.sourceRate=sourceRate;m.destRate=destRate;m.destFormat=dest;cvt->len_mult=m.badMultiplier?65:64;cvt->len_ratio=2.0*destRate/sourceRate;return m.badBuild?-1:1;}
int SDLCALL __wrap_SDL_ConvertAudio(SDL_AudioCVT *cvt){
    workerOnly();++m.conversions;require(cvt->len>0&&!(cvt->len%2),"intermediate extent");
    const unsigned inputs=unsigned(cvt->len)/2;m.captured=std::min(inputs,256u);std::memcpy(m.intermediate,cvt->buf,m.captured*2);
    std::vector<Sint16> input(inputs);std::memcpy(input.data(),cvt->buf,cvt->len);
    const unsigned frames=unsigned((std::uint64_t(inputs)*m.destRate)/m.sourceRate);
    const unsigned frameBytes=m.destFormat==AUDIO_S8?2u:4u;
    require(std::uint64_t(frames)*frameBytes<=std::uint64_t(cvt->len)*cvt->len_mult,"model output cap");
    for(unsigned i=0;i<frames;++i){Sint16 value=input[std::min(unsigned((std::uint64_t(i)*m.sourceRate)/m.destRate),inputs-1)];
        for(unsigned c=0;c<2;++c)if(frameBytes==4)std::memcpy(cvt->buf+i*4+c*2,&value,2);else cvt->buf[i*2+c]=Uint8(static_cast<Sint8>(value/256));}
    cvt->len_cvt=m.badLength?1:int(frames*frameBytes);return 0;}
#ifndef __NO_MUSIC
void __real__Z9func_mutev();
void __wrap__Z9func_mutev(){workerOnly();require(!opl[0]&&!opl[1],"actual Adlib null-safe premise");__real__Z9func_mutev();}
#endif
}
int main(int argc,char **argv) {
    std::set_terminate(terminated);
    try {
        if(argc==2){deathName=argv[1];auto lease=Access::lease(A::Route::InternalPCM,9,{128},22050,1);
            if(!std::strcmp(argv[1],"--death-owner-prepare")){deathArmed=true;lease->prepareForDevice(44100,AUDIO_S16SYS,2);deathArmed=false;fail("owner prepare returned");}
            else if(!std::strcmp(argv[1],"--death-repeat-prepare")){std::thread t([&]{require(lease->prepareForDevice(44100,AUDIO_S16SYS,2)==A::PrepareStatus::Ready,"first preparation");deathArmed=true;lease->prepareForDevice(44100,AUDIO_S16SYS,2);deathArmed=false;fail("repeat prepare returned");});t.join();}
            else if(!std::strcmp(argv[1],"--death-receipt-owner")){A::AttachReceipt invalid;deathArmed=true;lease->acceptAttach(invalid);deathArmed=false;fail("receipt owner returned");}
            else fail("unknown death mode");fail("death mode returned");
        }
        require(argc==1,"arguments");parsing();signedOpcodeDecoder();conversion();allocationRollback();
        attachRoute(A::Route::InternalPCM);attachRoute(A::Route::InternalPCM,true);
        attachRoute(A::Route::ExplicitTrack);attachRoute(A::Route::UfoSequence);attachRoute(A::Route::Silent);
        closedAndRejected();heldCallback();
        std::puts("LOADING_VIDEO_AUDIO PASS parser PCM callback four-routes typed-detach conditional-native");return 0;
    }catch(...){fail("unexpected exception");}
}
