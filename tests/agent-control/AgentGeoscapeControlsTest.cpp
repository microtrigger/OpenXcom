// SPDX-License-Identifier: GPL-3.0-or-later
// Literal current Geoscape provider/dispatch/ordinary-handler bodies with real
// TextButton radio groups, stock fonts, paint and AgentUi binding. Owning Game,
// navigation state allocation, timers and saved-base selection are test seams.
// Actual AgentController uses a private socket and the provider callback. Its
// Screen/lifecycle, audio, monotonic time and physical input are explicit shims.
// No Game, video subsystem, simulation, save, fixture or event queue is run.
#define State AgentControllerTestState
#include "GeoscapeControllerDependencies.generated.h"
#include "../../src/Observer/AgentController.h"
#undef State
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/Logger.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Sound.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/TextButton.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
using namespace OpenXcom;
using namespace OpenXcom::Observer;
namespace {
unsigned checks = 0;
void check(bool value, const char *message)
{
    ++checks;
    if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
bool has(const std::string &text, const std::string &part) { return text.find(part) != std::string::npos; }
std::string token(unsigned value)
{
    std::ostringstream out; out << std::hex << std::setw(32) << std::setfill('0') << value; return out.str();
}
std::string field(const std::string &text, const std::string &name)
{
    auto start = text.find("\"" + name + "\":"); check(start != std::string::npos, "packet field exists");
    start += name.size() + 3;
    if (text[start] == '"') return text.substr(start + 1, text.find('"', start + 1) - start - 1);
    return text.substr(start, text.find_first_of(",}", start) - start);
}
bool pendingInput = false;
Uint8 focus = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
Uint8 heldButtons = 0;
SDLMod modifiers = KMOD_NONE;
}
namespace OpenXcom { namespace Observer {
std::uint64_t agentTestTime = 10000;
bool diagnosticProfileQualified() { return true; }
} }
extern "C" Uint8 SDL_GetAppState() { return focus; }
extern "C" SDLMod SDL_GetModState() { return modifiers; }
extern "C" Uint8 SDL_GetMouseState(int *, int *) { return heldButtons; }
extern "C" void SDL_PumpEvents() {}
extern "C" int SDL_PeepEvents(SDL_Event *, int, SDL_eventaction action, Uint32)
{
    check(action == SDL_PEEKEVENT, "controller only peeks physical input"); return pendingInput ? 1 : 0;
}
// These unrelated ungrouped/combo branches must stay unreachable in this test.
extern "C" int Mix_GroupAvailable(int) { std::abort(); }
namespace OpenXcom {
void Sound::play(int, int, int) const { std::abort(); }
void State::setModal(InteractiveSurface *) { std::abort(); }
}
namespace GeoscapeHarness {
using namespace OpenXcom;
struct Base {};
struct Globe {};
struct Timer { bool running = false; bool isRunning() const { return running; } };
struct NavigationState {
    std::string name; Base *base; Globe *globe;
    explicit NavigationState(const std::string &n, Base *b = nullptr, Globe *g = nullptr) : name(n), base(b), globe(g) {}
    virtual ~NavigationState() {}
};
struct BasescapeState : NavigationState { BasescapeState(Base *b, Globe *g) : NavigationState("basescape", b, g) {} };
struct PauseState : NavigationState { explicit PauseState(int) : NavigationState("pause") {} };
struct FundingState : NavigationState { FundingState() : NavigationState("funding") {} };
enum { OPT_GEOSCAPE = 0 };
struct SavedGame {
    std::vector<Base *> bases; Base *selected = nullptr;
    std::vector<Base *> *getBases() { return &bases; }
    Base *getSelectedBase() { return selected; }
};
struct Game {
    SavedGame save; std::vector<std::unique_ptr<NavigationState>> stack;
    SavedGame *getSavedGame() { return &save; }
    void pushState(NavigationState *state) { stack.emplace_back(state); }
};
class GeoscapeState {
public:
    TextButton *_btnIntercept, *_btnBases, *_btnGraphs, *_btnUfopaedia, *_btnOptions, *_btnFunding;
    TextButton *_timeSpeed, *_btn5Secs, *_btn1Min, *_btn5Mins, *_btn30Mins, *_btn1Hour, *_btn1Day;
    Text *_txtHour, *_txtMin, *_txtSec, *_txtWeekday, *_txtDay, *_txtMonth, *_txtYear, *_txtFunds;
    Surface *_modal = nullptr;
    std::vector<Surface *> _surfaces;
    std::list<int> _dogfights;
    Game *_game; Globe *_globe;
    Timer *_zoomInEffectTimer, *_zoomOutEffectTimer;
    bool agentUiState(AgentUiState &) const;
    bool agentDispatch(const std::string &);
    bool agentDispatch(const AgentRequest &);
    void btnBasesClick(Action *); void btnOptionsClick(Action *); void btnFundingClick(Action *);
    void btnTimerClick(Action *); void timerReset();
    bool selectGeoscapeSpeed(TextButton *); bool buttonsDisabled();
};
#include "GeoscapeProvider.generated.inc"
#include "GeoscapeOrdinary.generated.inc"
}
struct Fixture {
    GeoscapeHarness::GeoscapeState state;
    GeoscapeHarness::Game game;
    GeoscapeHarness::Globe globe;
    GeoscapeHarness::Timer zoomIn, zoomOut;
    Surface canvas{320, 200}, cover{31, 13, 0, 0};
    std::vector<std::unique_ptr<TextButton>> buttons;
    std::vector<std::unique_ptr<Text>> fields;
    Fixture(Font &big, Font &small, Font &geoBig, Font &geoSmall, Language &language)
    {
        state._game = &game; state._globe = &globe;
        state._zoomInEffectTimer = &zoomIn; state._zoomOutEffectTimer = &zoomOut;
        const char *labels[] = {"INTERCEPT", "BASES", "GRAPHS", "UFOPAEDIA", "OPTIONS", "FUNDING", "5 Secs", "1 Min", "5 Mins", "30 Mins", "1 Hour", "1 Day"};
        for (unsigned i = 0; i < 12; ++i)
        {
            buttons.emplace_back(new TextButton(i < 6 ? 63 : 31, i < 6 ? 11 : 13,
                i < 6 ? 257 : (i % 2 ? 289 : 257), i < 6 ? i * 12 : 112 + ((i - 6) / 2) * 14));
            auto *button = buttons.back().get(); button->initText(&geoBig, &geoSmall, &language);
            if (i >= 6) button->setBig();
            button->setColor(40); button->setGeoscapeButton(true); button->setText(labels[i]);
            state._surfaces.push_back(button);
        }
        state._btnIntercept = buttons[0].get(); state._btnBases = buttons[1].get(); state._btnGraphs = buttons[2].get();
        state._btnUfopaedia = buttons[3].get(); state._btnOptions = buttons[4].get(); state._btnFunding = buttons[5].get();
        state._btn5Secs = buttons[6].get(); state._btn1Min = buttons[7].get(); state._btn5Mins = buttons[8].get();
        state._btn30Mins = buttons[9].get(); state._btn1Hour = buttons[10].get(); state._btn1Day = buttons[11].get();
        state._timeSpeed = state._btn5Secs;
        for (unsigned i = 6; i < 12; ++i) buttons[i]->setGroup(&state._timeSpeed);
        const int dimensions[][4] = {{20,16,259,74}, {20,16,283,74}, {11,8,307,80}, {59,8,259,87},
            {29,8,259,94}, {29,8,288,94}, {59,8,259,101}, {59,8,259,73}};
        const char *values[] = {"9", "34", "05", "FRIDAY", "1st", "Jan", "1999", "$600"};
        for (unsigned i = 0; i < 8; ++i)
        {
            const auto &d = dimensions[i]; fields.emplace_back(new Text(d[0], d[1], d[2], d[3]));
            auto *text = fields.back().get(); text->initText(&big, &small, &language); text->setColor(40);
            text->setText(values[i]); if (i < 2) text->setBig(); if (i == 0) text->setAlign(ALIGN_RIGHT);
            if (i > 2) text->setAlign(ALIGN_CENTER);
            if (i == 7) text->setVisible(false);
            state._surfaces.push_back(text);
        }
        state._txtHour = fields[0].get(); state._txtMin = fields[1].get(); state._txtSec = fields[2].get();
        state._txtWeekday = fields[3].get(); state._txtDay = fields[4].get(); state._txtMonth = fields[5].get();
        state._txtYear = fields[6].get(); state._txtFunds = fields[7].get();
        cover.setVisible(false); state._surfaces.push_back(&cover); paint();
    }
    void paint() { for (auto *surface : state._surfaces) surface->blit(&canvas); }
    AgentUiState project()
    {
        AgentUiState ui; check(state.agentUiState(ui), "current exact Geoscape provider projects");
        check(agentRegionsDisjoint(ui), "Geoscape proof footprints are disjoint"); return ui;
    }
    bool dispatch(const std::string &action, const std::string &choice = "")
    {
        AgentRequest request; request.action = action; request.choice = choice; return state.agentDispatch(request);
    }
};
struct Channel {
    AgentControllerTestState state; OpenXcom::Game game; std::unique_ptr<AgentController> controller;
    int peer = -1; std::string epoch, grant, request, prefix, pending; unsigned serial = 100;
    std::vector<std::string> schedule;
    explicit Channel(Fixture &fixture, const std::string &name) : prefix(name)
    {
        pendingInput = false; focus = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
        heldButtons = 0; modifiers = KMOD_NONE; agentTestTime += 10000;
        state.project = [&fixture](AgentUiState &ui) { return fixture.state.agentUiState(ui); };
        state.handler = [&fixture](const AgentRequest &r) { check(fixture.state.agentDispatch(r), "native provider dispatch succeeds"); };
        game.top = &state;
        int pair[2]; check(!socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair), "private controller socketpair"); peer = pair[1];
        setenv("OPENXCOM_AGENT_CONTROL", "1", 1); setenv("OPENXCOM_AGENT_CONTROL_FD", std::to_string(pair[0]).c_str(), 1);
        unsetenv("OPENXCOM_STANCE_FD"); controller.reset(new AgentController(&game)); epoch = field(receive(), "epoch");
        send("H openxcom-agent-control/17"); controller->poll(); check(has(receive(), "\"ready\""), "wire17 handshake"); read();
    }
    ~Channel()
    {
        controller.reset(); if (peer >= 0) ::close(peer);
        std::string json = "[";
        for (const auto &entry : schedule) { if (json.size() > 1) json += ","; json += entry; }
        save(prefix + "-schedule.json", json + "]");
    }
    void save(const std::string &name, const std::string &text)
    {
        if (const char *directory = std::getenv("OPENXCOM_AGENT_TEST_PACKET_DIR"))
        { std::ofstream file(std::string(directory) + "/" + name); file << text; check(bool(file), "actual wire packet artifact saved"); }
    }
    void send(const std::string &packet) { pending = packet; check(::send(peer, packet.data(), packet.size(), 0) == ssize_t(packet.size()), "whole packet sent"); }
    std::string receive()
    {
        char buffer[49152]; auto n = recv(peer, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (n < 0) { check(errno == EAGAIN || errno == EWOULDBLOCK, "nonblocking socket receive"); return ""; }
        if (!n) return "EOF";
        const std::string text(buffer, n), name = prefix + "-wire-" + std::to_string(schedule.size()) + ".json";
        save(name, text);
        schedule.push_back("{\"request\":" + (pending.empty() ? std::string("null") : agentJsonString(pending)) +
            ",\"response_file\":" + agentJsonString(name) + "}");
        pending.clear(); return text;
    }
    void frame() { controller->beginFrame(); controller->presented(); }
    void read() { send("R " + token(serial++)); controller->poll(); frame(); grant = receive(); check(field(grant, "capability").size() == 32, "fresh Geoscape capability"); }
    void queue(const std::string &action = "choose_geoscape_speed", const std::string &choice = "s3")
    {
        request = "E " + token(serial++) + " 1 " + epoch + " " + field(grant, "capability") + " " + field(grant, "revision") + " " + action + (choice.empty() ? "" : " " + choice);
        send(request); controller->poll(); check(receive().empty(), "E queues without immediate dispatch");
    }
    void refused() { frame(); check(has(receive(), "\"unavailable\"") && !state.calls, "stale or covered context refuses without dispatch"); }
};
int main()
{
    Logger::logFile() = "/tmp/openxcom-geoscape-controls-test.log";
    check(SDL_Init(0) == 0 && SDL_GetVideoSurface() == nullptr, "eventless offscreen initialization");
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    Font big, small, geoBig, geoSmall; Language language;
    for (const auto &node : YAML::LoadFile("Language/Font.dat")["fonts"])
    {
        if (node["id"].as<std::string>() == "FONT_BIG") big.load(node);
        if (node["id"].as<std::string>() == "FONT_SMALL") small.load(node);
        if (node["id"].as<std::string>() == "FONT_GEO_BIG") geoBig.load(node);
        if (node["id"].as<std::string>() == "FONT_GEO_SMALL") geoSmall.load(node);
    }
    {
        Fixture f(big, small, geoBig, geoSmall, language); auto initial = f.project();
        check(initial.rows == std::vector<std::size_t>({0,1,2,3,4,5}), "all six shown radio controls have compact handles");
        check(initial.offers("choose_geoscape_speed") && initial.actions.front().label == "5 Secs" &&
            initial.actions.front().labelSource == "visible_text" && initial.actions.front().argumentKind == "geoscape_speed", "speed descriptor uses first actual painted label");
        check(has(initial.visible, "\"speed\":\"5 Secs\"") && has(initial.visible, "\"hour\":\"9\"") &&
            has(initial.visible, "\"funds\":null"), "prior clock/funds/selected-speed fields preserved");
        check(initial.offers("open_bases") && initial.offers("open_pause_menu") && initial.offers("open_funding"), "only supported painted navigation offered");
        check(!initial.offers("intercept") && !initial.offers("open_graphs") && !initial.offers("open_options"), "unimplemented strategic navigation remains unavailable");
        for (unsigned i = 0; i < 6; ++i)
        {
            check(f.dispatch("choose_geoscape_speed", "s" + std::to_string(i)) && f.state._timeSpeed == f.buttons[i + 6].get(), "native slot uses actual ordinary radio group");
            unsigned pressedCount = 0;
            for (unsigned j = 0; j < 6; ++j) { bool pressed = false; check(f.buttons[j+6]->agentPressed(pressed), "every speed redraw is proven"); pressedCount += pressed; }
            check(pressedCount == 1, "ordinary group redraw preserves exactly one selected speed");
        }
        const auto beforeRepeat = f.project(); check(f.dispatch("choose_geoscape_speed", "s5"), "same speed selection accepted");
        check(beforeRepeat.sameBinding(f.project()), "same selected speed is idempotent");
        f.state.timerReset(); check(f.state._timeSpeed == f.state._btn5Secs, "timer reset shares ordinary first-speed selection");
        SDL_Event key = {}; key.type = SDL_KEYDOWN; key.key.keysym.sym = SDLK_4;
        Action action(&key, 1.0, 1.0, 0, 0); action.setSender(f.state._btn30Mins); f.state.btnTimerClick(&action);
        check(f.state._timeSpeed == f.state._btn30Mins && key.type == SDL_KEYDOWN, "ordinary keyboard selects without forging mouse input");
        SDL_Event mouse = {}; mouse.type = SDL_MOUSEBUTTONDOWN; mouse.button.button = SDL_BUTTON_LEFT;
        Action mouseAction(&mouse, 1.0, 1.0, 0, 0); f.state._btn1Hour->mousePress(&mouseAction, nullptr);
        check(f.state._timeSpeed == f.state._btn1Hour, "ordinary mouse uses the same real radio operation");
        f.state.btnTimerClick(nullptr); action.setSender(f.state._btnBases); f.state.btnTimerClick(&action);
        check(f.state._timeSpeed == f.state._btn1Hour, "null or foreign speed sender does not mutate group");
        for (const char *bad : {"", "s", "s6", "s00", "s-1", "b0", "0", "s0 extra"})
            check(!f.dispatch("choose_geoscape_speed", bad), "typed dispatch itself rejects malformed local speed");
        f.state.timerReset(); const auto beforeTick = f.project();
        f.state._txtHour->setText("10"); f.state._txtSec->setText("50"); f.state._txtDay->setText("2nd"); f.paint();
        auto afterTick = f.project();
        check(beforeTick.visible != afterTick.visible && beforeTick.sameBinding(afterTick) && beforeTick.regions == afterTick.regions, "glyph-width and clock-string changes retain exact control authority");
        f.state._btn1Min->selectRadio(); f.state._btn5Secs->selectRadio(); f.paint();
        check(!beforeTick.sameBinding(f.project()), "real speed away/back invalidates private group generations");
        f.state._btn5Secs->setVisible(false); f.paint(); auto compact = f.project();
        check(compact.rows == std::vector<std::size_t>({1,2,3,4,5}) && compact.actions.front().label == "1 Min" &&
            has(compact.visible, "\"speed\":null"), "hidden selected radio is omitted with sequential local handles and null speed");
        check(f.dispatch("choose_geoscape_speed", "s0") && f.state._timeSpeed == f.state._btn1Min &&
            !f.dispatch("choose_geoscape_speed", "s5"), "compacted s0 resolves to current shown control only");
        f.state._btn5Secs->setVisible(true); f.paint();
        f.cover.setX(257); f.cover.setY(112); f.cover.setVisible(true); f.paint();
        check(f.project().rows == std::vector<std::size_t>({1,2,3,4,5}), "later painted cover removes exact covered speed");
        f.cover.setVisible(false); f.state._btn5Secs->setText("THIS LABEL CANNOT FIT"); f.paint();
        check(f.project().rows.size() == 5, "clipped speed label cannot supply a target");
        f.state._btn5Secs->setText("5 Secs"); f.paint();
        f.state._btn1Day->setX(319); f.paint(); check(f.project().rows.size() == 5, "offscreen speed cannot supply a target");
        f.state._btn1Day->setX(289); f.paint();
        TextButton *rogue = f.state._btn5Secs; f.state._btn5Secs->setGroup(&rogue); f.paint();
        AgentUiState contradictory; check(!f.state.agentUiState(contradictory), "two independently painted selected speeds fail closed");
        f.state._btn5Secs->setGroup(&f.state._timeSpeed); f.paint();
        for (auto *button : {f.state._btnBases, f.state._btnOptions, f.state._btnFunding})
        {
            const std::string actionName = button == f.state._btnBases ? "open_bases" : button == f.state._btnOptions ? "open_pause_menu" : "open_funding";
            button->setVisible(false); check(!f.project().offers(actionName) && !f.dispatch(actionName), "hidden navigation cannot dispatch");
            button->setVisible(true); f.paint();
            f.cover.setX(button->getX()); f.cover.setY(button->getY()); f.cover.setVisible(true); f.paint();
            check(!f.project().offers(actionName) && !f.dispatch(actionName), "covered navigation cannot dispatch");
            f.cover.setVisible(false); f.paint();
        }
        f.state._modal = &f.cover; check(!f.state.agentUiState(contradictory) && !f.dispatch("open_bases"), "top modal suppresses Geoscape controls"); f.state._modal = nullptr;
        f.state._dogfights.push_back(1); check(!f.state.agentUiState(contradictory), "separately painted dogfight suppresses Geoscape controls"); f.state._dogfights.clear();
        GeoscapeHarness::Base base; f.game.save.bases.push_back(&base); f.game.save.selected = &base;
        f.state._btn1Day->selectRadio(); check(f.dispatch("open_bases") && f.game.stack.size() == 1 &&
            f.game.stack.back()->name == "basescape" && f.game.stack.back()->base == &base &&
            f.game.stack.back()->globe == &f.globe && f.state._timeSpeed == f.state._btn5Secs, "ordinary Bases handler retains selected base, globe and timer reset");
        f.game.save.bases.clear(); check(f.dispatch("open_bases") && f.game.stack.back()->base == nullptr, "ordinary no-base branch stays unchanged");
        check(f.dispatch("open_pause_menu") && f.game.stack.back()->name == "pause", "Options routes through ordinary Geoscape pause handler");
        check(f.dispatch("open_funding") && f.game.stack.back()->name == "funding", "Funding handler remains available");
        const auto pushes = f.game.stack.size(); f.zoomIn.running = true;
        check(f.dispatch("open_bases") && f.dispatch("open_pause_menu") && f.dispatch("open_funding") &&
            f.game.stack.size() == pushes, "ordinary zoom guard may no-op; dispatch never claims navigation success");
    }
    {
        Fixture f(big, small, geoBig, geoSmall, language); Channel c(f, "clock-success");
        c.send("V " + token(90) + " " + c.epoch + " 0 16"); c.controller->poll();
        check(has(c.receive(), "\"events\""), "V event read preserves the existing Geoscape grant");
        f.state._txtHour->setText("10"); f.state._txtSec->setText("10"); f.paint(); c.frame();
        check(c.receive().empty(), "clock-only intervening frame does not publish an unsolicited grant");
        c.queue(); f.state._txtSec->setText("15"); f.paint(); c.frame();
        check(c.state.calls == 1 && c.receive().empty() && f.state._timeSpeed == f.state._btn30Mins, "clock ticks before E and after queue preserve one speed dispatch");
        c.frame(); const auto receipt = c.receive(); check(has(receipt, "\"dispatched\"") && has(receipt, "\"second\":\"15\""), "strict later result shows current painted clock");
        c.send(c.request); c.controller->poll(); check(c.receive() == receipt && c.state.calls == 1, "same identity replays receipt without repeating selection");
    }
    for (unsigned scenario = 0; scenario < 3; ++scenario)
    {
        const char *names[] = {"selection-before-e", "aba-before-e", "overlay-before-e"};
        Fixture f(big, small, geoBig, geoSmall, language); Channel c(f, names[scenario]);
        Surface cursor(1, 1, 259, 73);
        if (scenario < 2) f.state._btn1Min->selectRadio();
        if (scenario == 1) f.state._btn5Secs->selectRadio();
        if (scenario == 2) c.game._cursor = &cursor;
        f.paint(); c.queue(); c.refused();
    }
    for (unsigned scenario = 0; scenario < 12; ++scenario)
    {
        Fixture f(big, small, geoBig, geoSmall, language); Channel c(f, "refusal-" + std::to_string(scenario)); Surface cursor(1, 1, 259, 73); AgentControllerTestState other;
        c.queue();
        switch (scenario)
        {
            case 0: f.state._btn1Min->selectRadio(); f.state._btn5Secs->selectRadio(); break;
            case 1: f.state._btn30Mins->setVisible(false); break;
            case 2: f.cover.setX(257); f.cover.setY(126); f.cover.setVisible(true); break;
            case 3: f.state._modal = &f.cover; break;
            case 4: f.state._dogfights.push_back(1); break;
            case 5: c.game.screen.revision += 2; break;
            case 6: pendingInput = true; break;
            case 7: heldButtons = SDL_BUTTON_LMASK; break;
            case 8: modifiers = KMOD_LALT; break;
            case 9: c.game._cursor = &cursor; break;
            case 10: c.game.top = &other; c.controller->invalidate(); c.game.top = &c.state; c.controller->invalidate(); break;
            case 11: f.state._btn30Mins->setX(258); break;
        }
        f.paint(); c.refused();
    }
    {
        Fixture f(big, small, geoBig, geoSmall, language); Channel c(f, "bases-clock-success"); c.queue("open_bases", "");
        f.state._txtSec->setText("55"); f.paint(); c.frame();
        check(c.state.calls == 1 && f.game.stack.size() == 1, "clock tick also preserves stable Bases navigation");
    }
    check(agentPaintHealthy(), "all real offscreen paint operations succeeded");
    check(SDL_GetVideoSurface() == nullptr && !(SDL_WasInit(SDL_INIT_VIDEO)), "no GUI or game initialized");
    std::cout << "AgentGeoscapeControlsTest PASS " << checks << " checks\n";
}
