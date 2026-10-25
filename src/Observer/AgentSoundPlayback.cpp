// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentSoundPlayback.h"
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
namespace OpenXcom { namespace Observer {
namespace {AgentSoundPlayback processObserver;const std::uint64_t maximum=9007199254740991ULL;}
AgentSoundPlayback::AgentSoundPlayback():_enabled(false),_foreign(false){}
AgentSoundPlayback &AgentSoundPlayback::instance(){return processObserver;}
bool AgentSoundPlayback::primaryMode(){const char*m=std::getenv("OPENXCOM_AGENT_SOUND_MODE");return m&&!std::strcmp(m,"playback");}
bool AgentSoundPlayback::owner(){if(!_enabled.load(std::memory_order_acquire))return false;if(std::this_thread::get_id()!=_owner){_foreign.store(true,std::memory_order_release);return false;}return true;}
std::uint64_t AgentSoundPlayback::elapsed()const{return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-_origin).count();}
bool AgentSoundPlayback::entropy(){
 const int fd=open("/dev/urandom",O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0)return false;
 const ssize_t n=read(fd,_tokens.data(),sizeof(_tokens));::close(fd);if(n!=sizeof(_tokens))return false;
 for(unsigned i=0;i<Entries;++i)for(unsigned j=0;j<i;++j)if(_tokens[i]==_tokens[j])return false;
 return true;
}
void AgentSoundPlayback::fail(const char*why){_discarded=_sequence>=_expected?_sequence-_expected+1:0;_size=0;_read=0;_expected=_sequence+1;_terminal=true;_enabled.store(false,std::memory_order_release);_reason=why;_statusPending=true;_camera.stale=true;}
void AgentSoundPlayback::poll(bool enabled){
 if(!_started){if(!enabled||!primaryMode())return;_started=true;_owner=std::this_thread::get_id();_origin=std::chrono::steady_clock::now();_epoch=1;
  if(!_foreign.is_lock_free()||!_enabled.is_lock_free()||!entropy()){fail("entropy_failure");return;}
  _statusPending=true;_enabled.store(true,std::memory_order_release);
 }
 if(std::this_thread::get_id()!=_owner){_foreign.store(true,std::memory_order_release);return;}
 if(_terminal)return;
 if(!enabled){fail("controller_disabled");return;}
 if(_foreign.exchange(false,std::memory_order_acq_rel)){fail("thread_mismatch");return;}
}
void AgentSoundPlayback::boundary(bool reset){
 if(!owner())return;
 if(reset){fail("audio_reset");return;}
 if(_epoch==maximum){fail("counter_exhausted");return;}
 _discarded=_sequence>=_expected?_sequence-_expected+1:0;_size=0;_read=0;_sequence=0;_expected=1;++_epoch;_reason="save_load";_statusPending=true;_camera.stale=true;
}
void AgentSoundPlayback::camera(bool known,int x,int y,int w,int h,std::uint64_t frame,std::uint64_t revision){
 if(!owner())return;
 if(!known||x<0||y<0||x>4095||y>4095||w<=0||h<=0||w>640||h>400||!frame||frame>maximum||revision>maximum){_camera.stale=true;return;}
 _camera.known=true;_camera.stale=false;_camera.x=x;_camera.y=y;_camera.width=w;_camera.height=h;_camera.frame=frame;_camera.revision=revision;_cameraAt=std::chrono::steady_clock::now();
}
void AgentSoundPlayback::invalidateCamera(){if(owner())_camera.stale=true;}
void AgentSoundPlayback::observe(const Mix_Chunk*chunk,int channel,bool loop,bool positioned,int angle,int distance)noexcept{
 const int savedErrno=errno;
 struct RestoreErrno {int value;~RestoreErrno(){errno=value;}} restore{savedErrno};
 if(!owner()||channel<0)return;
 try{
  int rate=0,channels=0;Uint16 format=0;
  if(!Mix_QuerySpec(&rate,&format,&channels)||rate!=22050||format!=AUDIO_S16LSB||channels!=2){fail("unsupported_profile");return;}
  if(!chunk||!chunk->abuf||!chunk->alen||chunk->alen>MaxChunk||chunk->alen%4){fail("invalid_chunk");return;}
  const int gain=Mix_Volume(channel,-1),cg=Mix_VolumeChunk(const_cast<Mix_Chunk*>(chunk),-1);
  if(gain<0||gain>128||cg<0||cg>128){fail("invalid_chunk");return;}if(!gain||!cg)return;
  bool nonzero=false;for(unsigned i=0;i<chunk->alen;++i)if(chunk->abuf[i]){nonzero=true;break;}if(!nonzero)return;
  std::size_t index=0;for(;index<_entries.size();++index)if(_entries[index].pcm.size()==chunk->alen&&!std::memcmp(_entries[index].pcm.data(),chunk->abuf,chunk->alen))break;
  if(index==_entries.size()){
   if(index==Entries||chunk->alen>MaxBytes-_bytes){fail("capacity");return;}
   Entry e;e.pcm.assign(chunk->abuf,chunk->abuf+chunk->alen);_entries.push_back(std::move(e));_bytes+=chunk->alen;
  }
  if(_sequence==maximum||elapsed()>maximum){fail("counter_exhausted");return;}
  Event e;e.epoch=_epoch;e.sequence=++_sequence;e.ms=elapsed();e.channelGain=gain;e.chunkGain=cg;e.loop=loop;e.position=positioned;
  if(positioned){int a=static_cast<Sint16>(angle)%360;if(a<0)a+=360;e.angle=a;e.distance=static_cast<Uint8>(distance);}
  const char*hex="0123456789abcdef";for(unsigned i=0;i<16;++i){e.id[i*2]=hex[_tokens[index][i]>>4];e.id[i*2+1]=hex[_tokens[index][i]&15];}
  e.camera=_camera;if(e.camera.known){e.camera.age=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-_cameraAt).count();e.camera.stale=e.camera.stale||e.camera.age>250;}
  if(_size<Queue){_queue[(_read+_size)%Queue]=e;++_size;}
 }catch(...){fail("capacity");}
}
bool AgentSoundPlayback::take(Event&e){
 if(!_started||std::this_thread::get_id()!=_owner)return false;e=Event();
 if(_statusPending){_statusPending=false;e.kind=Event::Status;e.epoch=_epoch;e.active=!_terminal;e.reason=_reason;e.count=_discarded;_discarded=0;return true;}
 if(_size){const Event&n=_queue[_read];if(n.sequence>_expected){e.kind=Event::Gap;e.epoch=_epoch;e.count=n.sequence-_expected;_expected=n.sequence;return true;}
  e=n;_read=(_read+1)%Queue;--_size;_expected=e.sequence+1;return true;}
 if(_sequence>=_expected){e.kind=Event::Gap;e.epoch=_epoch;e.count=_sequence-_expected+1;_expected=_sequence+1;return true;}return false;
}
std::string AgentSoundPlayback::currentJson()const{return std::string("{\"playback_epoch\":")+std::to_string(_epoch)+",\"status\":\""+(!_started?"inactive":_terminal?"unavailable":"active")+"\"}";}
std::string AgentSoundPlayback::eventBody(const Event&e){
 std::string prefix="\"kind\":\"";if(e.kind==Event::Status)return prefix+"sound_playback_status\",\"playback_epoch\":"+std::to_string(e.epoch)+",\"status\":\""+(e.active?"active":"unavailable")+"\",\"reason\":\""+e.reason+"\",\"discarded_pending\":"+std::to_string(e.count);
 if(e.kind==Event::Gap)return prefix+"sound_playback_gap\",\"playback_epoch\":"+std::to_string(e.epoch)+",\"dropped_events\":"+std::to_string(e.count);
 std::string camera="null";if(e.camera.known)camera="{\"viewport\":{\"x\":"+std::to_string(e.camera.x)+",\"y\":"+std::to_string(e.camera.y)+",\"width\":"+std::to_string(e.camera.width)+",\"height\":"+std::to_string(e.camera.height)+"},\"frame\":"+std::to_string(e.camera.frame)+",\"revision\":"+std::to_string(e.camera.revision)+",\"age_ms\":"+std::to_string(e.camera.age)+",\"stale\":"+(e.camera.stale?"true":"false")+"}";
 return prefix+"sound_play_issued\",\"playback_epoch\":"+std::to_string(e.epoch)+",\"sound_sequence\":"+std::to_string(e.sequence)+",\"monotonic_ms\":"+std::to_string(e.ms)+",\"sound_id\":\""+e.id+"\",\"channel_gain_128\":"+std::to_string(e.channelGain)+",\"chunk_gain_128\":"+std::to_string(e.chunkGain)+",\"loop_start\":"+(e.loop?"true":"false")+",\"position\":"+(e.position?"{\"angle_degrees\":"+std::to_string(e.angle)+",\"distance_effect\":"+std::to_string(e.distance)+"}":"null")+",\"camera\":"+camera+",\"semantics\":\"accepted_positive_gain_playback\"";
}
} }
