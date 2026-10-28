// SPDX-License-Identifier: GPL-3.0-or-later
// Serialization/budget fixture only: synthetic large closed project table DTOs.
// Actual AgentController/socket/parser/ledger/event paging; existing explicit
// Game/Screen/State/input/time seams. No renderer/provider or gameplay proof.
#include "AgentPurchaseBudgetDependencies.generated.h"
#include "../../src/Observer/AgentController.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

using namespace OpenXcom;
using namespace OpenXcom::Observer;
namespace OpenXcom { namespace Observer {
std::uint64_t agentTestTime = 10000;
bool diagnosticProfileQualified() { return true; }
} }
extern "C" Uint8 SDL_GetAppState() { return SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS; }
extern "C" SDLMod SDL_GetModState() { return KMOD_NONE; }
extern "C" Uint8 SDL_GetMouseState(int *, int *) { return 0; }
extern "C" void SDL_PumpEvents() {}
extern "C" int SDL_PeepEvents(SDL_Event *, int, SDL_eventaction action, Uint32)
{ if (action != SDL_PEEKEVENT) std::abort(); return 0; }
namespace {
unsigned checks = 0;
void check(bool value, const char *message)
{ ++checks; if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(1); } }
std::string token(unsigned value)
{ std::ostringstream out; out << std::hex << std::setw(32) << std::setfill('0') << value; return out.str(); }
std::string field(const std::string &packet, const std::string &key)
{
    std::size_t start = packet.find("\"" + key + "\":");
    check(start != std::string::npos, "packet field exists"); start += key.size() + 3;
    if (packet[start] == '"') return packet.substr(start + 1, packet.find('"', start + 1) - start - 1);
    return packet.substr(start, packet.find_first_of(",}", start) - start);
}
std::size_t occurrences(const std::string &text, const std::string &needle)
{
    std::size_t count = 0, pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) { ++count; pos += needle.size(); }
    return count;
}
std::string maximumVisible(unsigned variant)
{
    const std::string title(1024,static_cast<char>('A'+variant)),cell(180,'x');
    std::string out="{\"title\":"+agentJsonString(title)+",\"funds\":null,\"total\":null,\"space_used\":null,\"headings\":[null,null],\"category\":null,\"rows\":[";
    for(unsigned i=0;i<32;++i){if(i)out+=",";out+="{\"handle\":\"r"+std::to_string(i)+"\",\"cells\":[";for(unsigned c=0;c<4;++c){if(c)out+=",";out+=agentJsonString(cell);}out+="],\"selected\":false,\"controls\":[\"increase\",\"decrease\"]}";}
    out+="],\"scroll\":{\"up\":true,\"down\":true},\"scrollbar\":null,\"buttons\":[{\"id\":\"cancel\",\"label\":\"Cancel\"}]}";
    check(occurrences(out,"\"handle\":\"r")==32,"32 four-cell row controls");return out;
}

struct Harness {
    int peer = -1;
    State state;
    Game game;
    std::unique_ptr<AgentController> controller;
    std::string directory, epoch, grant, sequence = "[", phase = "main";
    unsigned packetCount = 0;
    explicit Harness(const char *path) : directory(path)
    {
        int fds[2]; check(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0, "private real packet socketpair");
        peer = fds[1];
        setenv("OPENXCOM_AGENT_CONTROL", "1", 1);
        setenv("OPENXCOM_AGENT_CONTROL_FD", std::to_string(fds[0]).c_str(), 1);
        unsetenv("OPENXCOM_STANCE_FD");
        game.top = &state; state.ui.screen = "purchase";
        state.ui.regions = {{0, 0, 640, 400}};
        state.ui.actions.emplace_back("cancel_purchase", "Cancel", "visible_text", "none");
        state.ui.actions.emplace_back("scroll_purchase", "Scroll purchase list", "visible_control_semantics", "purchase_scroll");
        controller.reset(new AgentController(&game));
        const auto hello = receive("hello.json"); epoch = field(hello, "epoch");
        check(agentToken(epoch), "native random epoch is canonical");
        send("hello.request", "H " + std::string(agentProtocolSchema())); controller->poll();
        check(field(receive("ready.json"), "status") == "ready", "exact wire21 handshake");
    }
    ~Harness() { controller.reset(); if (peer >= 0) ::close(peer); }
    void save(const std::string &name, const std::string &bytes)
    {
        std::ofstream out(directory + "/" + name, std::ios::binary); out.write(bytes.data(), bytes.size());
        check(bool(out), "exact bytes persisted");
    }
    void record(const char *direction, const std::string &name, const std::string &bytes)
    {
        save(name, bytes);
        if (packetCount++) sequence += ",";
        sequence += "{\"order\":" + std::to_string(packetCount) + ",\"direction\":" + agentJsonString(direction) +
            ",\"phase\":" + agentJsonString(phase) + ",\"file\":" + agentJsonString(name) +
            ",\"bytes\":" + std::to_string(bytes.size()) + "}";
    }
    void send(const std::string &name, const std::string &packet)
    {
        check(::send(peer, packet.data(), packet.size(), 0) == static_cast<ssize_t>(packet.size()), "whole native request sent");
        record("client_to_server", name, packet);
    }
    std::string receive(const std::string &name)
    {
        char bytes[49153];
        const ssize_t size = recv(peer, bytes, sizeof(bytes), MSG_DONTWAIT);
        check(size > 0 && size <= 49152, "native response fits transport cap and arrives");
        const std::string packet(bytes, size); record("server_to_client", name, packet); return packet;
    }
    void silent()
    {
        char byte;
        check(recv(peer, &byte, 1, MSG_DONTWAIT) < 0 && (errno == EAGAIN || errno == EWOULDBLOCK),
            "no unsolicited or predispatch response");
    }
    void frame() { controller->beginFrame(); controller->presented(); check(controller->enabled(), "controller remains open"); }
    std::string events(const std::string &name, unsigned challenge, unsigned after)
    {
        send(name + ".request", "V " + token(challenge) + " " + epoch + " " + std::to_string(after) + " 16");
        controller->poll(); return receive(name + ".json");
    }
};
}
int main(int argc, char **argv)
{
    check(argc == 2, "packet output directory supplied");
    Harness h(argv[1]);
    std::vector<std::string> visible, records;
    for (unsigned i = 0; i < 3; ++i)
    {
        visible.push_back(maximumVisible(i)); h.state.ui.visible = visible.back();
        const std::string record = "{\"seq\":" + std::to_string(i + 1) +
            ",\"kind\":\"presentation\",\"state\":\"purchase\",\"visible\":" + visible.back() + "}";
        check(record.size() + 40 < AgentPublicEvents::MaxEventBytes, "maximal valid presentation fits per-event cap");
        records.push_back(record); h.save("frame-" + std::to_string(i + 1) + ".visible.json", visible.back());
        h.frame(); h.silent();
    }
    check(visible[0] != visible[1] && visible[1] != visible[2], "three differing maximum presentations retained");
    check(records[0].size() + records[1].size() + 2 > AgentPublicEvents::PageBytes,
        "two maximum records exceed native byte-page budget");
    h.send("state.request", "R " + token(99)); h.controller->poll(); h.frame();
    h.grant = h.receive("state.json");
    check(field(h.grant, "event_sequence") == "3" && agentToken(field(h.grant, "capability")), "latest frame supplies independent fresh grant");
    check(h.grant.find(visible.back()) != std::string::npos, "maximum native state preserves complete exact DTO");
    std::vector<std::string> pages;
    for (unsigned after = 0; after < 3; ++after)
    {
        const auto page = h.events("events-" + std::to_string(after), 100 + after, after);
        pages.push_back(page);
        check(field(page, "oldest") == "1" && field(page, "newest") == "3" && field(page, "gap") == "false",
            "all maximum records retained without gap");
        check(field(page, "after") == std::to_string(after) && field(page, "next_cursor") == std::to_string(after + 1),
            "byte-budget partial page cursor points to its last actual record");
        check(occurrences(page, "\"kind\":\"presentation\"") == 1 && page.find(records[after]) != std::string::npos,
            "limit16 returns one whole maximum record, never skips or splits");
        if (after < 2) check(field(page, "next_cursor") != field(page, "newest"), "partial byte-budget page admits later remainder");
        const auto repeat = h.events("events-" + std::to_string(after) + "-repeat", 100 + after, after);
        check(repeat == page, "nonconsuming identical request replays byte-identical retained record");
    }
    const auto empty = h.events("events-complete", 103, 3);
    check(field(empty, "next_cursor") == "3" && empty.find("\"events\":[]") != std::string::npos,
        "complete cursor returns empty correct remainder");
    check(h.events("events-first-replay", 100, 0) == pages.front(), "earliest retained page remains identical after remainder reads");
    check(h.state.calls == 0, "R/V transport exercise dispatches no action");
    // Separate terminal probe: admitted E then S=pending proves V preserved the
    // native grant. Never present another frame, so no callback can dispatch.
    h.phase = "grant_probe";
    h.send("grant-probe.request", "E " + token(201) + " 1 " + h.epoch + " " + field(h.grant, "capability") +
        " " + field(h.grant, "revision") + " cancel_purchase");
    h.controller->poll(); h.silent();
    h.send("grant-probe-receipt.request", "S 1 " + token(201)); h.controller->poll();
    check(field(h.receive("grant-probe-pending.json"), "status") == "pending", "native grant survived all independent V pages");
    check(h.state.calls == 0, "grant proof is queue admission only with zero callbacks");
    h.controller->close(); check(h.state.calls == 0, "closing drops queued probe without gameplay");
    h.save("sequence.json", h.sequence + "]\n");
    std::cout << "AgentPurchaseBudgetControllerTest PASS " << checks << " checks; state=" << h.grant.size()
        << " event=" << records.front().size() << " page=" << pages.front().size() << " bytes; dispatches=0\n";
}
