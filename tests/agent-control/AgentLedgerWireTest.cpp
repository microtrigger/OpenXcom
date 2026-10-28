// SPDX-License-Identifier: GPL-3.0-or-later
// Actual Controller poll/rollover/result bodies; deterministic UI seams, not gameplay.
#include "../../src/Observer/AgentProtocol.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cassert>
#include <iostream>
#include <string>
namespace OpenXcom {
struct State{}; struct Screen{std::uint64_t agentDisplayRevision(){return 1;}};
struct Game{State state;Screen screen;State*agentTopState(){return &state;}Screen*getScreen(){return &screen;}};
namespace Observer {
static std::uint64_t now(){return 100;}
static bool diagnosticProfileQualified(){return true;}
struct AgentUiState{bool offers(const std::string&)const{return true;}};
struct Queue { bool activeFlag=false; unsigned stages=0; bool active(){return activeFlag;}bool expired(std::uint64_t){return false;}bool generationMatches(std::uint64_t){return true;}bool stage(const AgentRequest&,const AgentUiState&,State*,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t){++stages;activeFlag=true;return true;} };
struct AgentController {
 Game game;Game*_game=&game;int _fd=-1;bool _ready=true,_dispatchInProgress=false;
 AgentLedger _ledger;Queue _queued;AgentUiState _binding;State*_boundTop=nullptr;
 std::uint64_t _commandGeneration=1,_lastRetiredGeneration=0,_generation=1,_revision=1,_eventSequence=9,_readTime=100,_dispatchTime=100,_contextTime=100,_boundDisplayRevision=1;
 std::string _epoch=std::string(32,'e'),_capability,_readChallenge,_pendingId,_lastRolloverRequest,_lastRolloverResponse,_lastRolloverChallenge,packet;
 unsigned writes=0;bool closed=false;
 void close(){closed=true;_fd=-1;}bool focused(){return true;}void invalidate(bool=true){_capability.clear();_boundTop=nullptr;++_generation;++_revision;}
 bool sendPacket(const std::string&p){packet=p;++writes;return true;}void unavailable(){packet="unavailable";++writes;}
 void drainAudioEvents(){}void rejectQueued(){_queued.activeFlag=false;}void readExpired(const std::string&){unavailable();}void playerBasesPage(const AgentRequest&){unavailable();}void eventPage(const AgentRequest&){unavailable();}
 std::string resultPacket(const std::string&,const std::string&,const std::string& = "null")const;
 void receipt(const std::string&);void rollover(const AgentRequest&,const std::string&);void poll();
};
#include "LedgerController.generated.inc"
}}
int main(){using namespace OpenXcom::Observer;int fds[2];assert(socketpair(AF_UNIX,SOCK_SEQPACKET,0,fds)==0);AgentController c;c._fd=fds[1];
 auto send=[&](const std::string&s){assert(::send(fds[0],s.data(),s.size(),0)==(ssize_t)s.size());c.poll();assert(!c.closed);};
 const std::string id(32,'a'),challenge(32,'b'),epoch=c._epoch,cap(32,'c');
 auto fill=[&](unsigned n){for(unsigned i=0;i<n;++i){std::string k=std::to_string(i);assert(c._ledger.begin(k,k)==AgentLedger::Fresh);auto*e=c._ledger.find(k);e->terminalDispatched=true;e->response="ack";}};
 // Three full bounded generations: direct ledger fixture outcomes, actual wire retirement.
 for(unsigned g=1;g<=3;++g){fill(256);std::string l="L "+challenge+" "+epoch+" "+std::to_string(g)+" 256";send(l);assert(c._commandGeneration==g+1&&c._ledger.size()==0);assert(c.packet.find("\"status\":\"retired\"")!=std::string::npos);const auto receipt=c.packet;
 // Lost response simulation: discard local response then query, never resend E.
 c.packet.clear();send("K "+challenge+" "+epoch+" "+std::to_string(g));assert(c.packet==receipt);send(l);assert(c.packet==receipt&&c._commandGeneration==g+1);
 send("L "+challenge+" "+epoch+" "+std::to_string(g)+" 255");assert(c.packet.find("blocked")!=std::string::npos);send("K "+challenge+" "+epoch+" "+std::to_string(g));assert(c.packet==receipt);
 c._capability=cap;c._boundTop=c._game->agentTopState();send("E "+id+" "+std::to_string(g)+" "+epoch+" "+cap+" "+std::to_string(c._revision)+" reserve_none");assert(c._ledger.size()==0&&c._queued.stages==0&&c._capability.empty());send("S "+std::to_string(g)+" "+id);assert(c.packet=="unavailable"&&c._ledger.size()==0);
 }
 // Wrong epoch, unknown challenge and an older successful transition cannot authorize anything.
 send("K "+id+" "+epoch+" 3");assert(c.packet.find("unknown")!=std::string::npos);send("K "+challenge+" "+epoch+" 2");assert(c.packet.find("unknown")!=std::string::npos);send("L "+challenge+" "+id+" 4 1");assert(c.packet=="unavailable"&&c._commandGeneration==4);
 fill(1);auto rollover=[&](){send("L "+challenge+" "+epoch+" "+std::to_string(c._commandGeneration)+" 1");};
 c._pendingId=id;rollover();assert(c._commandGeneration==4&&c._ledger.size()==1);c._pendingId.clear();c._queued.activeFlag=true;rollover();assert(c._commandGeneration==4);c._queued.activeFlag=false;c._dispatchInProgress=true;rollover();assert(c._commandGeneration==4);c._dispatchInProgress=false;c._readChallenge=id;rollover();assert(c._commandGeneration==4);c._readChallenge.clear();
 c._ledger.find("0")->terminalDispatched=false;c._ledger.find("0")->dispatched=true;c._ledger.find("0")->response="unknown_after_dispatch";rollover();assert(c._commandGeneration==4&&c._ledger.size()==1);c._ledger.find("0")->terminalDispatched=true;
 c._commandGeneration=9007199254740991ULL;rollover();assert(c._ledger.size()==1&&c._commandGeneration==9007199254740991ULL);
 assert(c._eventSequence==9&&c._epoch==epoch&&c._queued.stages==0);
 // Positive control: the same grammar/current grant really reaches queue stage once.
 c._commandGeneration=4;c._capability=cap;c._boundTop=c._game->agentTopState();
 const std::string fresh="E "+id+" 4 "+epoch+" "+cap+" "+std::to_string(c._revision)+" reserve_none";
 send(fresh);assert(c._queued.stages==1&&c._ledger.find(id));send(fresh);assert(c._queued.stages==1&&c.packet.find("pending")!=std::string::npos);
 send("S 3 "+id);assert(c.packet=="unavailable"&&c._queued.stages==1);send("S 4 "+id);assert(c.packet.find("pending")!=std::string::npos);
 ::close(fds[0]);::close(fds[1]);std::cout<<"PASS actual Controller wire: 768 acknowledged fixture receipts, three bounded rollovers, lost ack K, duplicates/conflicts, old E/S rejection, pending/unknown/overflow blocks; no gameplay dispatch\n";
}
