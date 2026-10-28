// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/Observer/AgentProtocol.h"
#include <cassert>
#include <string>
#include <vector>
#include <iostream>
namespace OpenXcom {struct SavedGame{};struct Language{};struct Mod{};
struct Game {SavedGame *save=nullptr;Language language;SavedGame*getSavedGame(){return save;}Language*getLanguage(){return &language;}Mod*getMod(){return nullptr;}};
namespace Observer {
static unsigned counter=0;static bool randomToken(std::string &s){s=std::string(31,'a')+char('a'+(++counter%6));return true;}
struct AgentPlayerBases {static std::vector<std::string> capture;static std::string failure;bool collect(SavedGame*,Language*,std::vector<std::string>&v,std::string&r,const Mod*){v=capture;r=failure;return r.empty();}void reset(){}};
std::vector<std::string> AgentPlayerBases::capture;std::string AgentPlayerBases::failure;
struct AgentController {
 Game*_game;AgentPlayerBases _playerBases;SavedGame*_basesSave=nullptr;std::vector<std::string>_basesRecords;std::string _basesEpoch,_basesSnapshot,_epoch=std::string(32,'e');std::uint64_t _basesGeneration=0;
 std::string packet;bool closed=false;void close(){closed=true;}bool sendPacket(const std::string&p){packet=p;return true;}
 void resetPlayerBases();void playerBasesPage(const AgentRequest&);
};
#include "Controller.generated.inc"
}}
int main(){using namespace OpenXcom;using namespace OpenXcom::Observer;
 Game g;SavedGame save;g.save=&save;AgentController c;c._game=&g;AgentRequest r;r.snapshot="-";r.challenge=std::string(32,'c');r.limit=1;
 AgentPlayerBases::capture={"{\"kind\":\"fixture1\"}","{\"kind\":\"fixture2\"}"};c.playerBasesPage(r);
 assert(c.packet.find("openxcom-player-bases/6")!=std::string::npos);
 assert(c._basesGeneration==1&&c.packet.find("\"complete\":false")!=std::string::npos&&c.packet.find("\"next_offset\":1")!=std::string::npos);const auto token=c._basesSnapshot;
 c.playerBasesPage(r);assert(c._basesGeneration==1&&c._basesSnapshot==token);
 AgentPlayerBases::capture.push_back("{\"kind\":\"new\"}");r.snapshot=token;r.after=1;c.playerBasesPage(r);assert(c._basesGeneration==1&&c.packet.find("\"total\":2")!=std::string::npos&&c.packet.find("\"complete\":true")!=std::string::npos);
 r.snapshot="-";r.after=0;c.playerBasesPage(r);assert(c._basesGeneration==2&&c._basesSnapshot!=token);
 r.snapshot=token;c.playerBasesPage(r);assert(c.packet.find("snapshot_expired")!=std::string::npos);
 r.snapshot="-";AgentPlayerBases::capture.clear();c.playerBasesPage(r);assert(c.packet.find("\"complete\":true")!=std::string::npos&&c.packet.find("\"total\":0")!=std::string::npos);
 const auto epoch=c._basesEpoch;c.resetPlayerBases();c.playerBasesPage(r);assert(c._basesEpoch!=epoch&&c._basesGeneration==1);
 AgentPlayerBases::failure="no_campaign";c.playerBasesPage(r);assert(c.packet.find("\"status\":\"unavailable\"")!=std::string::npos&&c.packet.find("\"total\":null")!=std::string::npos);
 assert(!c.closed);std::cout<<"PASS owner-read frozen pages, stable/change generation, reset, explicit failures; no UI fields exist in seam\n";}
