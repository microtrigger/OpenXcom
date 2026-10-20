// SPDX-License-Identifier: GPL-3.0-or-later
// Pure public-record retention tests; no renderer, mixer or simulation execution.
#include "../../src/Observer/AgentPublicEvents.h"
#include <cstdlib>
#include <iostream>
using namespace OpenXcom::Observer;
namespace {
unsigned checks = 0;
void check(bool value, const char *why) { ++checks; if (!value) { std::cerr << "FAIL " << why << '\n'; std::exit(1); } }
const std::string epoch(32, 'a');
const std::string audio = "\"kind\":\"audio_output\",\"duration_ms\":100,\"nonzero\":false,\"source\":null,\"sound\":null,\"position\":null";
}
int main()
{
    AgentPublicEvents ring; AgentPublicEvents::Page page;
    check(!ring.read(epoch,0,16,page) && !ring.append(audio), "unstarted queue refused");
    check(!ring.start("bad") && ring.start(epoch) && !ring.start(epoch), "exact immutable epoch");
    check(ring.read(epoch,0,16,page) && page.oldest==1 && page.newest==0 && page.next==0 && !page.gap && page.events.empty(), "empty cursor domain");
    check(!ring.read(std::string(32,'b'),0,16,page), "wrong epoch rejected");
    check(!ring.read(epoch,1,16,page) && !ring.read(epoch,0,0,page) && !ring.read(epoch,0,17,page), "future or unbounded page rejected");
    check(ring.presentation("battlescape", "{}") && ring.newest()==1, "first public frame admitted");
    for (unsigned i=0;i<1000;++i) check(ring.presentation("battlescape", "{}") && ring.newest()==1, "identical frames do not expose hidden updates");
    check(ring.read(epoch,0,16,page) && page.events.size()==1, "retained frame without intermediate polls");
    const std::string first = page.events[0].json;
    check(ring.append(audio) && ring.newest()==2, "actual completed zero output window is public record");
    check(ring.presentation("battlescape", "{}") && ring.newest()==2, "audio does not duplicate unchanged presentation");
    check(ring.presentation("battlescape", "{\"visible_change\":true}") && ring.newest()==3, "public change admits frame");
    check(ring.presentation("battlescape", "{}") && ring.newest()==4, "public away and back is a new presentation");
    for (unsigned i=0;i<10;++i)
    {
        check(ring.read(epoch,0,16,page) && page.events[0].json==first && page.newest==4, "replayed reads are immutable and nonconsuming");
        check(ring.size()==4, "read never advances ring");
    }
    check(!ring.append(std::string(32769,'x')) && ring.newest()==4, "oversize never increments public sequence");
    check(!ring.append(std::string(6000,static_cast<char>(0xe0))) && ring.newest()==4, "canonical ASCII expansion bounded");
    for(unsigned i=0;i<300;++i) check(ring.append(audio), "bounded audio producer");
    check(ring.size()==256 && ring.newest()==304, "retention independent of polling");
    check(ring.read(epoch,0,16,page) && page.gap && page.oldest==49 && page.newest==304 && page.events.size()==16 && page.next==64, "explicit overflow and contiguous retained prefix");
    check(ring.read(epoch,48,1,page) && !page.gap && page.next==49, "boundary before oldest has no gap");
    check(ring.read(epoch,47,1,page) && page.gap && page.next==49, "one lost event reports gap");
    std::uint64_t cursor=48;
    while(cursor<304) {
        check(ring.read(epoch,cursor,16,page), "bounded pages progress");
        for (const auto &record:page.events) check(record.seq==++cursor, "public sequence contiguous");
        check(page.next==cursor, "cursor is final returned event");
    }
    check(ring.read(epoch,304,16,page) && page.events.empty() && page.next==304 && !page.gap, "caught-up read remains empty");
    AgentPublicEvents large; check(large.start(epoch), "large record ring started");
    const std::string body="\"kind\":\"presentation\",\"state\":\"unsupported\",\"visible\":{\"text\":\""+std::string(30000,'x')+"\"}";
    check(large.append(body) && large.append(body), "single event fits response ceiling");
    check(large.read(epoch,0,16,page) && page.events.size()==1 && page.next==1, "byte-limited page returns progress");
    check(large.read(epoch,1,16,page) && page.events.size()==1 && page.next==2, "next byte-limited page retained");
    std::cout << "AgentPublicEventsTest PASS " << checks << " checks\n";
}
