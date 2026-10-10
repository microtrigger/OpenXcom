// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#include "../diagnostic-isolation/fixture_options.h"
#include "../../src/Observer/PresentationObserver.h"
#include <cassert>
#include <cstdlib>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
using namespace OpenXcom::Observer;
struct Fixture {
    int peer; PresentationObserver *producer; std::string epoch, admission;
    Fixture(int sendBuffer = 0) {
        int pair[2]; assert(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) == 0);
        if (sendBuffer) assert(setsockopt(pair[1],SOL_SOCKET,SO_SNDBUF,&sendBuffer,sizeof(sendBuffer)) == 0);
        peer = pair[0]; producer = new PresentationObserver(std::to_string(pair[1]).c_str());
        std::string hello = receive(); assert(hello.size() == 88);
        epoch = hello.substr(24,32); admission = hello.substr(56,32);
    }
    ~Fixture() { delete producer; ::close(peer); }
    std::string receive() { char data[102401]; ssize_t n = recv(peer,data,sizeof(data),MSG_DONTWAIT); return n > 0 ? std::string(data,n) : ""; }
    void request(const std::string &suffix = std::string(32,'a')) { std::string value=epoch+admission+suffix; assert(send(peer,value.data(),value.size(),MSG_NOSIGNAL)==static_cast<ssize_t>(value.size())); }
};
PixelCrop crop() { PixelCrop c; c.screenWidth=320;c.screenHeight=200;c.x=1;c.y=2;c.width=1;c.height=1;c.rgb={1,2,3};return c; }
int main() {
    qualifyNormalDiagnostics();
    unsetenv("OPENXCOM_PRESENTATION_VERSION");
    { Fixture f; f.request(); f.producer->beginFrame(); assert(f.receive().empty()); f.producer->presented(crop()); auto reply=f.receive(); assert(reply.find("presented_after_request")!=std::string::npos); assert(reply.find("010203")!=std::string::npos); f.producer->presented(crop()); assert(f.receive().empty());
      // Old admission token replay closes the channel and cannot produce another ack.
      f.request();f.producer->beginFrame();assert(!f.producer->enabled()); }
    // Mid-composition and post-flip requests must await next beginFrame.
    { Fixture f; f.producer->beginFrame();f.request();f.producer->presented(crop());assert(f.receive().empty());f.producer->beginFrame();f.producer->presented(crop());assert(!f.receive().empty()); }
    { Fixture f; f.producer->beginFrame();f.producer->presented(crop());f.request();assert(f.receive().empty());f.producer->beginFrame();f.producer->presented(crop());assert(!f.receive().empty()); }
    { Fixture f; f.request();f.producer->beginFrame();f.producer->presented(PixelCrop());assert(!f.producer->enabled());assert(f.receive().empty()); }
    { Fixture f; f.request();f.producer->beginFrame();f.producer->beginFrame();assert(!f.producer->enabled()); }
    { Fixture f; f.request(std::string(33,'a'));f.producer->beginFrame();assert(!f.producer->enabled()); }
    { Fixture f; f.request();f.request();f.producer->beginFrame();assert(!f.producer->enabled()); }
    { Fixture f; f.epoch=std::string(32,'0');f.request();f.producer->beginFrame();assert(!f.producer->enabled()); }
    { Fixture f; f.request(std::string(32,'z'));f.producer->beginFrame();assert(!f.producer->enabled()); }
    { Fixture f; f.request();f.producer->beginFrame();::close(f.peer);f.peer=-1;f.producer->presented(crop());assert(!f.producer->enabled()); }
    { PresentationObserver p("1");assert(!p.enabled()); }
    // A maximum crop cannot block when the bounded socket send buffer is full.
    { Fixture f(1024);f.request();f.producer->beginFrame();auto c=crop();c.width=c.height=128;c.rgb.resize(128*128*3);f.producer->presented(c);assert(!f.producer->enabled());assert(f.receive().empty()); }
    { Fixture f;f.request();f.producer->beginFrame();auto c=crop();c.width=c.height=128;c.rgb.resize(128*128*3);f.producer->presented(c);auto reply=f.receive();assert(reply.size()<102400);assert(reply.find("public_revision")==std::string::npos); }
    { Fixture f; f.producer->resetPresentation();assert(f.producer->enabled());f.request();f.producer->beginFrame();f.producer->presented(crop());assert(!f.receive().empty()); }
    { Fixture f; f.request();f.producer->resetPresentation();f.producer->beginFrame();f.producer->presented(crop());assert(!f.receive().empty()); }
    { Fixture f; f.request();f.producer->beginFrame();f.producer->resetPresentation();assert(!f.producer->enabled());assert(f.receive().empty()); }
    { Fixture f; f.request();f.producer->beginFrame();f.producer->close();f.producer->presented(crop());assert(f.receive().empty()); }
    // /1 ignores HUD additions and preserves its exact original envelope shape.
    { Fixture f; assert(!f.producer->hudEnabled()); f.request(); f.producer->beginFrame();
      f.producer->presented(crop(), "{\"time_units\":58}"); auto reply=f.receive();
      assert(reply.find("experimental-presentation/1")!=std::string::npos);
      assert(reply.find("\"hud\"")==std::string::npos); }
    setenv("OPENXCOM_PRESENTATION_VERSION", "2", 1);
    { Fixture f; assert(f.producer->hudEnabled()); f.request(); f.producer->beginFrame();
      f.producer->presented(crop()); auto reply=f.receive();
      assert(reply.find("experimental-presentation/2")!=std::string::npos);
      assert(reply.find("\"hud\":null")!=std::string::npos); }
    { Fixture f; f.request(); f.producer->beginFrame();
      const std::string hud="{\"time_units\":58,\"energy\":100,\"health\":100,\"morale\":100}";
      f.producer->presented(crop(),hud); auto reply=f.receive();
      assert(reply.find("\"hud\":"+hud)!=std::string::npos);
      assert(reply.find("\"epoch\":\""+f.epoch+"\"")!=std::string::npos);
      assert(reply.find("\"challenge\":\""+std::string(32,'a')+"\"")!=std::string::npos); }
    { Fixture f; f.request(); f.producer->beginFrame(); f.producer->resetPresentation();
      f.producer->presented(crop(),"{}"); assert(f.receive().empty()); }
    unsetenv("OPENXCOM_PRESENTATION_VERSION");

}
