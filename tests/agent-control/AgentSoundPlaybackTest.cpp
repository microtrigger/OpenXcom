// SPDX-License-Identifier: GPL-3.0-or-later
// Production observer + literal Sound methods, synthetic mixer and entropy boundaries.
#include "../../src/Observer/AgentSoundPlayback.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
using OpenXcom::Observer::AgentSoundPlayback;
namespace {int entropyMode=0,reads=0,plays=0,positions=0,queries=0,gain=128,chunkGain=128,rate=22050,channels=2,playResult=2,positionResult=1,playing=0;Uint16 format=AUDIO_S16LSB;unsigned checks=0;
void check(bool v,const char*m){++checks;if(!v){std::cerr<<m<<'\n';std::exit(1);}}
bool nextPlay(AgentSoundPlayback&o,AgentSoundPlayback::Event&e){while(o.take(e))if(e.kind==AgentSoundPlayback::Event::Play)return true;return false;}
}
extern "C" int __real_open(const char*,int,...);
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" int __real_close(int);
extern "C" int __wrap_open(const char*p,int flags,...){if(!std::strcmp(p,"/dev/urandom"))return entropyMode==1?-1:12345;return __real_open(p,flags);}
extern "C" ssize_t __wrap_read(int fd,void*p,size_t n){if(fd!=12345)return __real_read(fd,p,n);++reads;if(entropyMode==2)return -1;std::memset(p,0,n);if(entropyMode!=3)for(unsigned i=0;i<n/16;++i)static_cast<Uint8*>(p)[i*16]=i;return n;}
extern "C" int __wrap_close(int fd){return fd==12345?0:__real_close(fd);}
extern "C" int SDLCALL Mix_QuerySpec(int*r,Uint16*f,int*c){++queries;*r=rate;*f=format;*c=channels;errno=EDOM;return 1;}
extern "C" int SDLCALL Mix_Volume(int,int){return gain;}
extern "C" int SDLCALL Mix_VolumeChunk(Mix_Chunk*,int){return chunkGain;}
extern "C" int SDLCALL Mix_PlayChannelTimed(int,Mix_Chunk*,int,int){++plays;return playResult;}
extern "C" int SDLCALL Mix_SetPosition(int,Sint16,Uint8){++positions;return positionResult;}
extern "C" int SDLCALL Mix_Playing(int){return playing;}
extern "C" char* SDLCALL SDL_GetError(){static char e[]="stub";return e;}
namespace OpenXcom {namespace Options{bool mute=false,StereoSound=true;}struct Sound {Mix_Chunk*_sound;void play(int,int,int)const;void loop();};
struct TestLog {template<class T> TestLog&operator<<(const T&){return *this;}};
#define Log(x) TestLog()
#include "Sound.generated.inc"
#undef Log
}
int main(){using namespace OpenXcom;Uint8 bytes[8]={1,2,3,4,5,6,7,8};Mix_Chunk chunk={0,bytes,8,128};AgentSoundPlayback::Event e;
 unsetenv("OPENXCOM_AGENT_SOUND_MODE");AgentSoundPlayback off;off.poll(true);off.observe(&chunk,2,false,false,0,0);check(!off.take(e)&&reads==0&&queries==0,"ordinary mode no entropy/queries");
 setenv("OPENXCOM_AGENT_SOUND_MODE","playback",1);AgentSoundPlayback&o=AgentSoundPlayback::instance();o.poll(true);check(o.take(e)&&e.kind==AgentSoundPlayback::Event::Status&&e.active,"enabled status");
 Sound sound{&chunk};Sound nullSound{nullptr};nullSound.play(-1,0,0);check(plays==0&&!o.take(e),"native null no playback");Options::mute=true;sound.play(-1,0,0);check(plays==0&&!o.take(e),"native mute no playback or observation");Options::mute=false;playResult=-1;sound.play(-1,10,0);check(!o.take(e)&&queries==0,"failed play no observer query");playResult=2;
 errno=ENOENT;sound.play(-1,400,12);check(nextPlay(o,e)&&e.position&&e.angle==40&&e.distance==12&&errno==ENOENT,"accepted hook/effect/errno parity");const std::string id=e.id;const auto before=e;std::cout<<"EVENT {\"seq\":1,"<<AgentSoundPlayback::eventBody(e)<<"}\n";
 Uint8 copy[8];std::memcpy(copy,bytes,8);Mix_Chunk other={0,copy,8,128};Sound same{&other};same.play(-1,0,0);check(nextPlay(o,e)&&id==e.id,"equal PCM distinct resource same opaque ID");copy[0]=9;same.play(-1,0,0);check(nextPlay(o,e)&&id!=e.id,"changed PCM distinct opaque ID");
 gain=0;sound.play(-1,0,0);check(!o.take(e),"zero channel gain absent");gain=128;chunkGain=0;sound.play(-1,0,0);check(!o.take(e),"zero chunk gain absent");chunkGain=128;
 positionResult=0;sound.play(-1,12,255);check(nextPlay(o,e)&&!e.position,"failed position arguments not disclosed");positionResult=1;playing=1;int pc=plays;sound.loop();check(plays==pc&&!o.take(e),"existing loop not new event");playing=0;sound.loop();check(nextPlay(o,e)&&e.loop&&!e.position,"new accepted loop only");
 o.camera(true,0,0,320,200,7,3);sound.play(-1,0,0);check(nextPlay(o,e)&&e.camera.known&&e.camera.frame==7&&!e.camera.stale,"issued camera copied");o.camera(true,1,1,320,200,8,4);check(e.camera.frame==7&&e.camera.x==0,"later camera cannot rewrite issue");o.invalidateCamera();sound.play(-1,0,0);check(nextPlay(o,e)&&e.camera.stale,"invalidated camera explicit stale");
 sound.play(-1,0,0);o.boundary(false);check(o.take(e)&&e.epoch==2&&e.count==1&&e.active,"load clears pending with boundary");sound.play(-1,0,0);check(nextPlay(o,e)&&e.epoch==2&&e.sequence==1&&id==e.id,"load retains content identity resets sequence");
 for(unsigned i=0;i<70;++i)sound.play(-1,0,0);unsigned windows=0;std::uint64_t lost=0;while(o.take(e)){if(e.kind==AgentSoundPlayback::Event::Play)++windows;else if(e.kind==AgentSoundPlayback::Event::Gap)lost+=e.count;}check(windows==64&&lost==6,"bounded ring exact overflow gap");
 int oldQueries=queries;std::thread t([&](){o.observe(&chunk,2,false,false,0,0);});t.join();check(queries==oldQueries,"foreign thread never queries mixer/chunk");o.poll(true);check(o.take(e)&&!e.active&&!std::strcmp(e.reason,"thread_mismatch"),"foreign observation fails only observer");pc=plays;sound.play(-1,0,0);check(plays==pc+1&&!o.take(e),"game playback continues after observer failure");
 for(int mode:{1,2,3}){entropyMode=mode;AgentSoundPlayback x;x.poll(true);check(x.take(e)&&!e.active&&!std::strcmp(e.reason,"entropy_failure"),"entropy error/collision fail observation");}entropyMode=0;
 {AgentSoundPlayback x;x.poll(true);x.take(e);Uint8 zero[4]={};Mix_Chunk z={0,zero,4,128};x.observe(&z,2,false,false,0,0);check(!x.take(e),"exact digital silence omitted");z.alen=AgentSoundPlayback::MaxChunk+4;x.observe(&z,2,false,false,0,0);check(x.take(e)&&!e.active,"oversize rejected before scan");}
 {AgentSoundPlayback x;x.poll(true);x.take(e);rate=44100;x.observe(&chunk,2,false,false,0,0);check(x.take(e)&&!e.active&&!std::strcmp(e.reason,"unsupported_profile"),"unsupported decoded format fail closed");rate=22050;}
 {AgentSoundPlayback x;x.poll(true);x.take(e);for(unsigned i=0;i<129;++i){Uint8 pcm[4]={static_cast<Uint8>(i+1),static_cast<Uint8>(i>>7),1,0};Mix_Chunk z={0,pcm,4,128};x.observe(&z,2,false,false,0,0);}check(x.take(e)&&!e.active&&!std::strcmp(e.reason,"capacity")&&e.count==128,"dictionary never evicts/reuses ID");}
 {AgentSoundPlayback x;x.poll(true);x.take(e);std::vector<Uint8> pcm(AgentSoundPlayback::MaxChunk,1);Mix_Chunk z={0,pcm.data(),static_cast<Uint32>(pcm.size()),128};for(unsigned i=0;i<33;++i){pcm[0]=i;x.observe(&z,2,false,false,0,0);}check(x.take(e)&&!e.active&&!std::strcmp(e.reason,"capacity")&&e.count==32,"total PCM byte cap no eviction");}
 {AgentSoundPlayback x;x.poll(true);x.take(e);x.boundary(true);check(x.take(e)&&!e.active&&!std::strcmp(e.reason,"audio_reset"),"audio reset terminal");x.poll(true);x.observe(&chunk,2,false,false,0,0);check(!x.take(e),"queryspec cannot resurrect old chunk format");}
 std::cout<<"PASS "<<checks<<" scoped native playback checks; mixer/entropy synthetic, no game\n";
}
