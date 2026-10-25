// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <SDL_mixer.h>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>
#include <string>
namespace OpenXcom { namespace Observer {
// Process-lifetime, owner-thread-only observation. No borrowed resource identity.
class AgentSoundPlayback {
public:
 enum { Entries=128, MaxBytes=8*1024*1024, MaxChunk=256*1024, Queue=64 };
 struct Camera {bool known=false,stale=true;int x=0,y=0,width=0,height=0;std::uint64_t frame=0,revision=0,age=0;};
 struct Event {enum Kind {Play,Status,Gap}kind=Play;std::uint64_t epoch=0,sequence=0,ms=0,count=0;char id[33]={};int channelGain=0,chunkGain=0,angle=0,distance=0;bool loop=false,position=false;Camera camera;const char*reason="enabled";bool active=false;};
 AgentSoundPlayback();
 static AgentSoundPlayback &instance();
 static bool primaryMode();
 void poll(bool controllerEnabled);
 void observe(const Mix_Chunk*,int channel,bool loop,bool positioned,int angle,int distance) noexcept;
 void boundary(bool audioReset);
 void camera(bool known,int x,int y,int width,int height,std::uint64_t frame,std::uint64_t revision);
 void invalidateCamera();
 bool take(Event&);
 std::string currentJson() const;
 static std::string eventBody(const Event&);
private:
 struct Entry {std::vector<Uint8>pcm;};
 std::atomic<bool>_enabled,_foreign;
 bool _started=false,_terminal=false,_statusPending=false;std::thread::id _owner;
 std::array<std::array<Uint8,16>,Entries>_tokens;
 std::vector<Entry>_entries;std::size_t _bytes=0;
 std::array<Event,Queue>_queue;unsigned _read=0,_size=0;
 std::uint64_t _epoch=0,_sequence=0,_expected=1,_discarded=0;
 const char*_reason="enabled";
 Camera _camera;std::chrono::steady_clock::time_point _origin,_cameraAt;
 bool owner();bool entropy();void fail(const char*);std::uint64_t elapsed() const;
};
} }
