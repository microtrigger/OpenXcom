// SPDX-License-Identifier: GPL-3.0-or-later
// Source-authored candidate, 2026-10-15; not compiled or executed by this file.
// Link actual AdlibMusic.cpp, Music.cpp, Options.cpp, Adlib/adlplayer.cpp and
// Adlib/fmopl.cpp. No implementation inclusion, private access, alternate OPL
// allocator, copied constructor or fake pair. OPL's actual malloc/free leaves
// are faulted; actual engine setup/tick/mute and OPLDestroy execute in the model.
// ELF wraps: malloc, free, Mix_HookMusic, Mix_HaltMusic, Mix_FreeMusic,
// Mix_QuerySpec, Mix_VolumeMusic, Mix_GetMusicType, Mix_PlayMusic, Mix_LoadMUS,
// Mix_LoadMUS_RW, SDL_RWFromConstMem, SDL_FreeRW, SDL_GetError, and the declared
// Game admission/volume and unused Unicode/CrossPlatform leaves below. Verify
// all mangled names from fresh objects before command release. The Game gate
// is an explicit CONDITIONAL caller boundary here, never native-capability or
// actual Game ownership evidence; the real Game fixture must prove admission.
// Every case is a separate bounded process. malloc failure 1..4 covers each
// actual shared-table allocation, 5 first chip, 6 second chip; map failure 1..7
// covers all seven std::map nodes. No native codec or real-time claim follows.
#include "../../src/Engine/AdlibMusic.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/Exception.h"
#include "../../src/Engine/Adlib/fmopl.h"
#include "../../src/Engine/Adlib/adlplayer.h"
#include <SDL_mixer.h>
#include <atomic>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <thread>
#if !defined(OPENXCOM_RESPONSIVE_LOADING)
#error "Adlib failure fixture requires the responsive profile"
#endif
extern FM_OPL *opl[2];
extern "C" void *__real_malloc(std::size_t);
extern "C" void __real_free(void *);
namespace {
[[noreturn]] void fail(const char *what) noexcept {
    std::fprintf(stderr,"LOADING_ADLIB_FAILURE FAIL %s\n",what);std::_Exit(3);
}
void check(bool ok,const char *what) noexcept {if(!ok)fail(what);}
unsigned mallocCalls=0, failMalloc=0, live=0, newCalls=0, failNew=0;
void *allocated[16]={};
bool countNew=false, gateAllowed=true;
thread_local bool inCallback=false;
unsigned gates=0, hooks=0, halts=0, frees=0, queries=0, volumeReads=0;
int actualRate=44100, actualCount=1, actualChannels=2;
Uint16 actualFormat=AUDIO_S16SYS;
void *hookData=nullptr;
void (SDLCALL *hook)(void*,Uint8*,int)=nullptr;
void playbackCounters() {gates=hooks=halts=frees=queries=volumeReads=0;}
void loaded(OpenXcom::AdlibMusic &track) {
    // Actual old-format parser: tempo 60, zero samples/subtracks/instruments.
    // All four bytes read by init_music_data are owned for the track's lifetime.
    char *data=new char[4];data[0]=60;data[1]=data[2]=data[3]=0;track.load(data,4);
}
void successPair() {check(opl[0] && opl[1] && opl[0]!=opl[1],"complete distinct actual OPL pair");}
void balanced() {check(!opl[0] && !opl[1] && live==0,"no published half pair or leaked OPL table/chip");}
void normalCase(const char *name) {
    if (!std::strncmp(name,"malloc-",7)) {
        const unsigned n=static_cast<unsigned>(std::strtoul(name+7,nullptr,10));check(n>=1 && n<=6,"finite malloc fault");
        failMalloc=n;bool threw=false;
        try {OpenXcom::AdlibMusic track;}catch(const OpenXcom::Exception&){threw=true;}
        check(threw && mallocCalls==n,"actual requested OPL allocation failed");balanced();
        failMalloc=0;mallocCalls=0;
        {OpenXcom::AdlibMusic retry;successPair();check(mallocCalls==6,"balanced table lock permits fresh complete allocation");}
        balanced();
    } else if (!std::strncmp(name,"map-",4)) {
        const unsigned n=static_cast<unsigned>(std::strtoul(name+4,nullptr,10));check(n>=1 && n<=7,"finite map fault");
        failNew=n;countNew=true;bool threw=false;
        try {OpenXcom::AdlibMusic track;}catch(const std::bad_alloc&){threw=true;}
        countNew=false;check(threw && newCalls==n && mallocCalls==0,"map rollback precedes OPL acquisition");balanced();
        newCalls=0;countNew=true;
        {OpenXcom::AdlibMusic retry;countNew=false;successPair();check(newCalls==7,"failed map not partially published");}
        balanced();
    } else if (!std::strcmp(name,"pair-reuse") || !std::strcmp(name,"pair-rate-reject")) {
        {OpenXcom::AdlibMusic first;successPair();FM_OPL *a=opl[0],*b=opl[1];const unsigned calls=mallocCalls;
            if (!std::strcmp(name,"pair-reuse")) {
                OpenXcom::AdlibMusic second;check(opl[0]==a && opl[1]==b && mallocCalls==calls,"prior complete pair reused without allocation");
            } else {
                OpenXcom::Options::audioSampleRate=48000;bool threw=false;
                try {OpenXcom::AdlibMusic incompatible;}catch(const OpenXcom::Exception&){threw=true;}
                check(threw && opl[0]==a && opl[1]==b && mallocCalls==calls,"rate refusal preserves complete pair");
                OpenXcom::Options::audioSampleRate=44100;
            }
        }balanced();
    } else if (!std::strcmp(name,"unsupported-rate")) {
        OpenXcom::Options::audioSampleRate=12345;bool threw=false;
        try {OpenXcom::AdlibMusic track;}catch(const OpenXcom::Exception&){threw=true;}
        check(threw && !mallocCalls,"unsupported rate before OPL allocation");balanced();
    } else if (!std::strcmp(name,"invalid-adoption") || !std::strcmp(name,"repeated-adoption")) {
        {OpenXcom::AdlibMusic track;bool threw=false;
            if (!std::strcmp(name,"invalid-adoption")) {
                try{track.load(nullptr,0);}catch(const OpenXcom::Exception&){threw=true;}
            } else {
                loaded(track);char *extra=new char[4];extra[0]=60;
                try{track.load(extra,4);}catch(const OpenXcom::Exception&){threw=true;}
                delete[] extra; // Rejection does not adopt caller allocation.
            }
            check(threw,"invalid/repeated adoption rejects without transfer");
        }balanced();
#ifndef __NO_MUSIC
    } else if (!std::strcmp(name,"unloaded-play") || !std::strcmp(name,"gate-reject") ||
               !std::strcmp(name,"rate-reject") || !std::strcmp(name,"format-reject") ||
               !std::strcmp(name,"count-reject") || !std::strcmp(name,"channels-reject")) {
        {OpenXcom::AdlibMusic track;if(std::strcmp(name,"unloaded-play"))loaded(track);
            if(!std::strcmp(name,"gate-reject"))gateAllowed=false;
            if(!std::strcmp(name,"rate-reject"))actualRate=48000;
            if(!std::strcmp(name,"format-reject"))actualFormat=AUDIO_S8;
            if(!std::strcmp(name,"count-reject"))actualCount=2;
            if(!std::strcmp(name,"channels-reject"))actualChannels=1;
            playbackCounters();bool threw=false;
            try{static_cast<OpenXcom::Music*>(&track)->play();}catch(const OpenXcom::Exception&){threw=true;}
            check(threw && gates==1 && !hooks && !halts && !frees,"play rejection precedes all playback mutation");
            if(!std::strcmp(name,"gate-reject") || !std::strcmp(name,"unloaded-play"))check(!queries,"early rejection before mixer query");
            gateAllowed=true;
        }balanced();
    } else if (!std::strcmp(name,"callback-restart") || !std::strcmp(name,"callback-invalid") || !std::strcmp(name,"callback-render")) {
        {OpenXcom::AdlibMusic track;loaded(track);check(static_cast<OpenXcom::Music*>(&track)->isSoftwareMixedAudio(),"observer-independent actual virtual Adlib capability");static_cast<OpenXcom::Music*>(&track)->play();check(hook==&OpenXcom::AdlibMusic::player && hookData==&track,"real player installed with owned track");
            const bool render=std::strcmp(name,"callback-render")==0;
            if(!render){func_mute();check(!func_is_music_playing(),"actual engine stopped for loop restart");}
            OpenXcom::Options::musicAlwaysLoop=!render;playbackCounters();
            const unsigned allocations=mallocCalls;
            std::thread callback([&]{alignas(INT16) Uint8 stream[16]={};inCallback=true;
                if(render || !std::strcmp(name,"callback-restart"))OpenXcom::AdlibMusic::player(&track,stream,sizeof(stream));
                else {
                    OpenXcom::AdlibMusic::player(&track,stream,-4);
                    OpenXcom::AdlibMusic::player(&track,stream,3);
                    OpenXcom::AdlibMusic::player(nullptr,stream,sizeof(stream));
                    OpenXcom::AdlibMusic::player(&track,nullptr,sizeof(stream));
                    OpenXcom::AdlibMusic::player(&track,stream,0);
                }
                inCallback=false;});callback.join();
            check(!gates && !hooks && !halts && !frees && !queries && mallocCalls==allocations,"callback has no owner gate, allocation or mixer mutation/reentry");
            if(render || !std::strcmp(name,"callback-restart"))check(func_is_music_playing() && volumeReads==1,"actual direct-engine restart/render");
            else check(!func_is_music_playing() && !volumeReads,"invalid callback input returns before volume/native reads");
        }balanced();
#else
    } else if (!std::strcmp(name,"no-music-play")) {
        {OpenXcom::AdlibMusic track;playbackCounters();track.play();Uint8 stream[4]={};track.player(nullptr,stream,4);
            check(!gates && !hooks && !halts && !queries,"no-music playback remains inert");}
        balanced();
#endif
    } else fail("unknown case");
}
}
void *operator new(std::size_t size) {
    if(inCallback)fail("callback allocation");
    if(countNew && ++newCalls==failNew){failNew=0;countNew=false;throw std::bad_alloc();}
    void *p=__real_malloc(size?size:1);if(!p)throw std::bad_alloc();return p;
}
void operator delete(void *p) noexcept {__real_free(p);}
void *operator new[](std::size_t size) {return ::operator new(size);}
void operator delete[](void *p) noexcept {::operator delete(p);}
extern "C" {
void *__wrap_malloc(std::size_t size) {
    if(inCallback)fail("callback malloc");++mallocCalls;if(mallocCalls==failMalloc)return nullptr;
    void *p=__real_malloc(size);if(p){unsigned i=0;while(i<16 && allocated[i])++i;check(i<16,"bounded actual OPL allocations");allocated[i]=p;++live;}return p;
}
void __wrap_free(void *p) {
    if(!p)return;unsigned i=0;while(i<16 && allocated[i]!=p)++i;check(i<16,"free exactly owned OPL allocation");allocated[i]=nullptr;--live;__real_free(p);
}
void __wrap_Mix_HookMusic(void (SDLCALL *fn)(void*,Uint8*,int),void *data) {check(!inCallback,"callback mixer hook reentry");++hooks;hook=fn;hookData=data;}
int __wrap_Mix_HaltMusic() {check(!inCallback,"callback mixer halt reentry");++halts;return 0;}
void __wrap_Mix_FreeMusic(Mix_Music *music) {check(!inCallback && !music,"only actual Adlib blank base handle");++frees;}
int __wrap_Mix_QuerySpec(int *rate,Uint16 *format,int *channels) {check(!inCallback,"callback mixer query");++queries;*rate=actualRate;*format=actualFormat;*channels=actualChannels;return actualCount;}
int __wrap_Mix_VolumeMusic(int value) {check(inCallback && value==-1,"read-only callback volume leaf");++volumeReads;return 128;}
Mix_MusicType __wrap_Mix_GetMusicType(const Mix_Music*) {fail("unexpected base Music classifier");}
int __wrap_Mix_PlayMusic(Mix_Music*,int) {fail("unexpected base Music playback");}
Mix_Music *__wrap_Mix_LoadMUS(const char*) {fail("unexpected base file loader");}
Mix_Music *__wrap_Mix_LoadMUS_RW(SDL_RWops*) {fail("unexpected base memory loader");}
SDL_RWops *__wrap_SDL_RWFromConstMem(const void*,int) {fail("unexpected base RWops loader");}
void __wrap_SDL_FreeRW(SDL_RWops*) {fail("unexpected base RWops free");}
char *__wrap_SDL_GetError() {fail("unexpected base mixer error path");}
bool __wrap__ZN8OpenXcom4Game30responsiveMusicPlaybackAllowedEPKNS_5MusicE(const OpenXcom::Music*) noexcept {check(!inCallback,"callback owner gate");++gates;return gateAllowed;}
double __wrap__ZN8OpenXcom4Game14volumeExponentEi(int) {check(inCallback,"volume exponent only on callback");return 1.0;}
// Inert vtable-linked paths are forbidden, rather than silently mocked as success.
std::string __wrap__ZN8OpenXcom13CrossPlatform3nowB5cxx11Ev() {fail("unexpected logging");}
std::string __wrap__ZN8OpenXcom7Unicode14convPathToUtf8ERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE(const std::string&) {fail("unexpected base path conversion");}
}
int main(int argc,char **argv) {
    check(argc==2,"one bounded case argument");
    OpenXcom::Options::audioSampleRate=44100;OpenXcom::Options::musicVolume=128;
    OpenXcom::Options::musicAlwaysLoop=false;OpenXcom::Options::mute=false;
    normalCase(argv[1]);std::printf("LOADING_ADLIB_FAILURE PASS %s\n",argv[1]);return 0;
}
