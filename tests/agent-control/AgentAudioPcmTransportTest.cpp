// SPDX-License-Identifier: GPL-3.0-or-later
// Literal controller drain and actual public event ring; synthetic admitted PCM input.
#include "../../src/Observer/AgentAudioEvents.h"
#include "../../src/Observer/AgentPublicEvents.h"
#include <iostream>
#include <cstdlib>
#include <deque>
namespace OpenXcom { namespace Observer {
// Playback branch is disabled in this PCM-only transport fixture.
struct AgentSoundPlayback {struct Event {};static AgentSoundPlayback &instance(){static AgentSoundPlayback s;return s;}bool take(Event&){return false;}static bool primaryMode(){return false;}static std::string eventBody(const Event&){return "";}};
struct FakeAudio {std::deque<AgentAudioEvents::Event>events;bool take(AgentAudioEvents::Event&e){if(events.empty())return false;e=events.front();events.pop_front();return true;}};
struct FakeGame {FakeAudio audio;FakeAudio&agentAudioEvents(){return audio;}};
const char *audioStatusName(AgentAudioEvents::Event::State state){return state==AgentAudioEvents::Event::Active?"active":state==AgentAudioEvents::Event::Unavailable?"unavailable":"pending";}
struct AgentController {FakeGame game;FakeGame*_game=&game;AgentPublicEvents _publicEvents;bool closed=false;void close(){closed=true;}void drainAudioEvents();};
#include "Drain.generated.inc"
} }
using namespace OpenXcom::Observer;
unsigned checks=0;void check(bool v,const char*m){++checks;if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){AgentController c;const std::string epoch(32,'a');check(c._publicEvents.start(epoch),"start");
for(unsigned n=1;n<=3;++n){AgentAudioEvents::Event e;e.pcmSequence=n;
e.timing.firstRealtimeUs=e.timing.lastRealtimeUs=1000000+n*100000;
e.timing.firstMonotonicUs=e.timing.lastMonotonicUs=1000000+n*100000;
e.timing.firstCallbackFrames=e.timing.lastCallbackFrames=e.timing.lastFrameEndOffset=2205;e.nonzero=true;for(unsigned j=0;j<AgentAudioEvents::WindowBytes;++j)e.pcm[j]=j%256;c.game.audio.events.push_back(e);}c.drainAudioEvents();check(!c.closed,"bounded records admitted");AgentPublicEvents::Page p,q;
check(c._publicEvents.read(epoch,0,16,p)&&p.events.size()==2&&p.next==2,"two PCM windows fit bounded page");check(p.events[0].json.find("\"pcm_sequence\":1")!=std::string::npos,"sequence serialized");check(p.events[0].json.find("000102030405060708090a0b0c0d0e0f")!=std::string::npos,"exact PCM hex");check(c._publicEvents.read(epoch,0,16,q)&&q.events[0].json==p.events[0].json,"immutable reread");std::cout<<"EVENT "<<p.events[0].json<<'\n';check(c._publicEvents.read(epoch,2,16,p)&&p.events.size()==1&&p.next==3,"remaining third PCM");
for(unsigned n=4;n<=270;++n){AgentAudioEvents::Event e;e.pcmSequence=n;
e.timing.firstRealtimeUs=e.timing.lastRealtimeUs=1000000+n*100000;
e.timing.firstMonotonicUs=e.timing.lastMonotonicUs=1000000+n*100000;
e.timing.firstCallbackFrames=e.timing.lastCallbackFrames=e.timing.lastFrameEndOffset=2205;c.game.audio.events.push_back(e);c.drainAudioEvents();}check(c._publicEvents.size()==256,"bounded history");check(c._publicEvents.read(epoch,0,16,p)&&p.gap&&p.oldest==15&&p.events.size()==2,"expired cursor gap and byte page");std::cout<<"PASS "<<checks<<" transport checks; synthetic admitted PCM, no device/game\n";}
