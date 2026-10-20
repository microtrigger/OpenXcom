// SPDX-License-Identifier: GPL-3.0-or-later
// Current-source native constructors/buildUi, providers, dispatch and ordinary
// handlers are extracted literally by the runner. Real stock-font widgets,
// RuleResearch, ResearchProject, ItemContainer, RNG and Controller are linked.
// Explicit seams: owning State/Game lifecycle and interface initialization,
// Base capacity getters, available-rule enumeration, popup Window background.
// No real Game, video device, full build, campaign simulation or GUI is used.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/LocalizedText.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Engine/Timer.h"
#include "../../src/Engine/RNG.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Interface/ArrowButton.h"
#include "../../src/Savegame/ResearchProject.h"
#include "../../src/Savegame/ItemContainer.h"
#include "../../src/Mod/RuleResearch.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <algorithm>
#include <climits>
#include <iostream>
#include <memory>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include "AgentTableControllerDependencies.generated.h"
#include "AgentTableController.generated.h"
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
extern "C" int SDL_PeepEvents(SDL_Event *, int, SDL_eventaction, Uint32) { return 0; }
namespace {
unsigned checks = 0;
void check(bool value, const char *message) { ++checks; if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(1); } }
}
namespace OpenXcom {
Game *State::_game = nullptr;
#include "AgentBaseTableWidgetState.generated.inc"
#include "AgentBaseTableGameIdentity.generated.inc"
}
namespace ResearchHarness {
struct Base {
    int _scientists = 10, laboratories = 20;
    std::vector<ResearchProject *> _research;
    ItemContainer storage;
    const std::vector<ResearchProject *> &getResearch() const { return _research; }
    int getAvailableScientists() const { return _scientists; }
    int getScientists() const { return _scientists; }
    void setScientists(int value) { _scientists = value; }
    int getAllocatedScientists() const { int value = 0; for (const auto *p : _research) value += p->getAssigned(); return value; }
    int getFreeLaboratories() const { return laboratories - getAllocatedScientists(); }
    ItemContainer *getStorageItems() { return &storage; }
    void addResearch(ResearchProject *project);
    void removeResearch(ResearchProject *project);
    ~Base() { for (auto *project : _research) delete project; }
};
#include "AgentResearchBase.generated.inc"
struct Game;
struct State {
    static Game *_game;
    bool _screen = true;
    InteractiveSurface *_modal = nullptr;
    std::vector<Surface *> _surfaces;
    virtual ~State() { for (auto *surface : _surfaces) delete surface; }
    virtual void init() {}
    virtual bool agentUiState(AgentUiState &) const { return false; }
    virtual bool agentDispatch(const AgentRequest &) { return false; }
    void setInterface(const std::string &) {}
    void centerAllSurfaces() {}
    void add(Surface *surface, const char * = nullptr, const char * = nullptr);
    LocalizedText tr(const std::string &) const;
};
struct Game {
    State *top = nullptr;
    std::vector<State *> stack, retired;
    Surface *_cursor = nullptr, *_fpsCounter = nullptr;
    Font *big = nullptr, *small = nullptr;
    Language *language = nullptr;
    SDL_Color *palette = nullptr;
    Surface background{320, 200};
    unsigned pops = 0, pushes = 0;
    bool pendingInit = false;
    std::vector<RuleResearch *> rules;
    struct Mod { Surface *background = nullptr; Surface *getSurface(const std::string &) { return background; } } mod;
    struct Saved {
        Game *game = nullptr;
        void getAvailableResearchProjects(std::vector<RuleResearch *> &out, Mod *, Base *, bool debug)
        { check(debug, "ordinary availability considers debug mode"); out = game->rules; }
    } saved;
    Mod *getMod() { mod.background = &background; return &mod; }
    Saved *getSavedGame() { saved.game = this; return &saved; }
    bool isState(const void *state) const { return top == state; }
    bool agentOverlayClear(const AgentUiState &) const;
    void pushState(State *state) { ++pushes; stack.push_back(state); top = state; pendingInit = true; }
    void popState() { ++pops; retired.push_back(top); stack.pop_back(); top = stack.empty() ? nullptr : stack.back(); pendingInit = true; }
    void draw(Surface &canvas) {
        if (pendingInit && top) { top->init(); pendingInit = false; }
        canvas.clear();
        for (auto *state : stack) for (auto *surface : state->_surfaces) surface->blit(&canvas);
        if (_cursor) _cursor->blit(&canvas);
        if (_fpsCounter) _fpsCounter->blit(&canvas);
    }
    ~Game() { for (auto *state : stack) delete state; for (auto *state : retired) delete state; }
};
Game *State::_game = nullptr;
void State::add(Surface *surface, const char *, const char *) {
    _surfaces.push_back(surface); surface->setPalette(_game->palette);
    if (auto *text = dynamic_cast<Text *>(surface)) { text->initText(_game->big, _game->small, _game->language); text->setColor(80); }
    if (auto *button = dynamic_cast<TextButton *>(surface)) { button->initText(_game->big, _game->small, _game->language); button->setColor(40); }
    if (auto *list = dynamic_cast<TextList *>(surface)) { list->initText(_game->big, _game->small, _game->language); list->setColor(80); }
    if (auto *arrow = dynamic_cast<ArrowButton *>(surface)) arrow->setColor(40);
}
LocalizedText State::tr(const std::string &id) const { return _game->language->getString(id); }
#include "AgentResearchOverlay.generated.inc"
// Popup lifecycle is an explicit seam; no fake text/list/arrow projection.
enum { POPUP_BOTH = 1 };
struct Window : Surface {
    Window(State *, int w, int h, int x, int y, int = 0) : Surface(w, h, x, y) {}
    void setBackground(Surface *) { clear(40); }
};
class ResearchState : public State {
public:
    Base *_base;
    TextButton *_btnNew, *_btnOk;
    Window *_window;
    Text *_txtTitle, *_txtAvailable, *_txtAllocated, *_txtSpace, *_txtProject, *_txtScientists, *_txtProgress;
    TextList *_lstResearch;
    std::vector<ResearchProject *> _agentProjectMapping;
    std::uint64_t _agentMappingRevision = 0;
    ResearchState(Base *);
    ~ResearchState();
    void btnOkClick(Action *), btnNewClick(Action *), onSelectProject(Action *), selectProject(std::size_t), fillProjectList(), init();
    bool agentUiState(AgentUiState &) const;
    bool agentDispatch(const AgentRequest &);
};
class NewResearchListState : public State {
public:
    Base *_base;
    TextButton *_btnOK;
    Window *_window;
    Text *_txtTitle;
    TextList *_lstResearch;
    std::vector<RuleResearch *> _projects;
    std::uint64_t _agentMappingRevision = 0;
    NewResearchListState(Base *);
    void btnOKClick(Action *), onSelectProject(Action *), selectProject(std::size_t), fillProjectList(), init();
    bool agentUiState(AgentUiState &) const;
    bool agentDispatch(const AgentRequest &);
};
class ResearchInfoState : public State {
public:
    Base *_base;
    TextButton *_btnOk, *_btnCancel;
    ArrowButton *_btnMore, *_btnLess;
    Window *_window;
    Text *_txtTitle, *_txtAvailableScientist, *_txtAvailableSpace, *_txtAllocatedScientist, *_txtMore, *_txtLess;
    ResearchProject *_project;
    RuleResearch *_rule;
    Timer *_timerMore, *_timerLess;
    InteractiveSurface *_surfaceScientists;
    std::uint64_t _agentProjectRevision = 0;
    ResearchInfoState(Base *, RuleResearch *);
    ResearchInfoState(Base *, ResearchProject *);
    ~ResearchInfoState();
    void buildUi(), setAssignedScientist(), btnOkClick(Action *), btnCancelClick(Action *), moreByValue(int), lessByValue(int), more(), less();
    void handleWheel(Action *), morePress(Action *), moreRelease(Action *), moreClick(Action *), lessPress(Action *), lessRelease(Action *), lessClick(Action *);
    bool agentUiState(AgentUiState &) const;
    bool agentDispatch(const AgentRequest &);
};
#include "AgentResearchMethods.generated.inc"
#include "AgentNewResearchMethods.generated.inc"
#include "AgentResearchInfoMethods.generated.inc"
}

namespace {
std::string token(unsigned number)
{
    std::ostringstream value; value << std::hex << std::setw(32) << std::setfill('0') << number;
    return value.str();
}
struct TableChannel {
    AgentTableControllerState state, unsupportedParent;
    AgentTableControllerGame game;
    std::unique_ptr<AgentTableTestController> controller;
    int peer = -1;
    std::string epoch, last, request, prefix, pending;
    unsigned challenge = 10, command = 100;
    std::vector<std::string> schedule;
    TableChannel(const std::string &name) : prefix(name)
    {
        int descriptors[2]; check(!socketpair(AF_UNIX, SOCK_SEQPACKET, 0, descriptors), "actual anonymous controller socket");
        peer = descriptors[1]; setenv("OPENXCOM_AGENT_CONTROL", "1", 1);
        setenv("OPENXCOM_AGENT_CONTROL_FD", std::to_string(descriptors[0]).c_str(), 1); unsetenv("OPENXCOM_STANCE_FD");
        game.top = &state; unsupportedParent.project = [](AgentUiState &) { return false; };
        controller.reset(new AgentTableTestController(&game));
        const std::string hello = receive(); check(!hello.empty(), "actual controller hello");
        epoch = YAML::Load(hello)["epoch"].as<std::string>();
        send(std::string("H ") + agentProtocolSchema()); controller->poll();
        check(!receive().empty(), "actual controller ready handshake");
    }
    ~TableChannel()
    {
        controller->close(); if (peer >= 0) ::close(peer);
        std::string json = "["; for (const auto &entry : schedule) { if (json.size() > 1) json += ","; json += entry; }
        save(prefix + "-schedule.json", json + "]");
    }
    void save(const std::string &name, const std::string &value)
    {
        const char *directory = std::getenv("OPENXCOM_AGENT_TEST_PACKET_DIR");
        if (directory) { std::ofstream stream(std::string(directory) + "/" + name); stream << value; check(bool(stream), "public packet persisted"); }
    }
    void send(const std::string &value)
    { pending = value; check(::send(peer, value.data(), value.size(), 0) == ssize_t(value.size()), "send actual controller request"); }
    std::string receive()
    {
        char buffer[65536]; const auto length = ::recv(peer, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (length <= 0) return {};
        const std::string value(buffer, length), name = prefix + "-wire-" + std::to_string(schedule.size()) + ".json";
        save(name, value);
        schedule.push_back("{\"request\":" + (pending.empty() ? std::string("null") : agentJsonString(pending)) +
            ",\"response_file\":" + agentJsonString(name) + "}");
        pending.clear(); return value;
    }
    void frame() { controller->beginFrame(); controller->presented(); }
    std::string read()
    {
        send("R " + token(challenge++)); controller->poll(); frame(); last = receive();
        check(!last.empty() && YAML::Load(last)["type"].as<std::string>() == "state", "real provider to controller state packet");
        return last;
    }
    void queue(const std::string &action, const std::string &choice = "")
    {
        const YAML::Node state = YAML::Load(last);
        request = "E " + token(command++) + " " + epoch + " " + state["capability"].as<std::string>() + " " +
            state["revision"].as<std::string>() + " " + action + (choice.empty() ? "" : " " + choice);
        send(request); controller->poll(); check(receive().empty(), "queued E waits for native frame");
    }
    std::string events()
    {
        send("V " + token(challenge++) + " " + epoch + " 0 16"); controller->poll();
        const std::string value = receive(); check(!value.empty(), "actual retained events response"); return value;
    }
};
}
namespace {
const AgentUiAction *action(const AgentUiState &ui, const std::string &id)
{ for (const auto &a : ui.actions) if (a.id == id) return &a; return nullptr; }
void validateList(const AgentUiState &ui, const std::string &screen, unsigned columns, unsigned summary)
{
    const YAML::Node v = YAML::Load(ui.visible);
    check(ui.screen == screen && v.IsMap() && v.size() == 6, "exact six-field research list DTO");
    check(v["summary"].size() == summary && v["headings"].size() == columns, "render-order summaries and headings");
    check(v["rows"].IsSequence() && v["rows"].size() <= 32, "bounded projected research rows");
    for (std::size_t i = 0; i < v["rows"].size(); ++i) {
        auto row = v["rows"][i];
        check(row.size() == 3 && row["handle"].as<std::string>() == "r" + std::to_string(i), "compact public row handles");
        check(row["cells"].size() == columns, "exact painted research column count");
        for (const auto &cell : row["cells"]) check(cell.IsScalar() && cell.as<std::string>().size() <= 1024, "bounded complete research cell");
    }
    if (const auto *a = action(ui, "select_research_project"))
        check(!v["title"].IsNull() && v["rows"].size() && a->label == v["rows"][0]["cells"][0].as<std::string>() &&
            a->labelSource == "visible_text" && a->argumentKind == "project_row", "row action exact first admitted text and title");
    check(agentRegionsDisjoint(ui), "all research regions disjoint");
}
void validateInfo(const AgentUiState &ui)
{
    const YAML::Node v = YAML::Load(ui.visible);
    check(ui.screen == "research_info" && v.size() == 5, "exact five-field research info DTO");
    const char *order[] = {"increase", "decrease", "accept", "cancel"}; unsigned next = 0;
    std::string firstDirection;
    for (const auto &b : v["buttons"]) {
        const std::string id = b["id"].as<std::string>(), label = b["label"].as<std::string>();
        while (next < 4 && id != order[next]) ++next;
        check(next < 4 && b.size() == 2 && !label.empty() && label.size() <= 1024, "canonical exact info buttons"); ++next;
        if (firstDirection.empty() && (id == "increase" || id == "decrease")) firstDirection = label;
    }
    if (const auto *a = action(ui, "adjust_scientists"))
        check(!v["title"].IsNull() && !v["allocated"].IsNull() && a->label == firstDirection &&
            a->argumentKind == "staff_direction" && a->labelSource == "visible_text", "staff action uses first admitted directional text");
    check(agentRegionsDisjoint(ui), "all info regions disjoint");
}
std::string perform(TableChannel &channel, ResearchHarness::Game &game, Surface &canvas,
    const std::string &id, const std::string &choice = "")
{
    channel.read(); const unsigned before = channel.state.calls;
    channel.queue(id, choice); channel.frame();
    check(channel.state.calls == before + 1 && channel.receive().empty(), "exactly one queued native dispatch before receipt");
    game.draw(canvas); channel.frame(); const auto receipt = channel.receive();
    check(!receipt.empty() && YAML::Load(receipt)["status"].as<std::string>() == "dispatched", "receipt after later painted native state");
    channel.send(channel.request); channel.controller->poll();
    check(channel.receive() == receipt && channel.state.calls == before + 1, "duplicate command exact cached receipt and no second mutation");
    channel.events(); return receipt;
}
}
int main(int argc, char **argv)
{
    check(argc == 2, "stock source translation root");
    check(SDL_Init(0) == 0 && SDL_GetVideoSurface() == nullptr, "offscreen SDL only");
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    Font big, small;
    for (const auto &font : YAML::LoadFile("Language/Font.dat")["fonts"])
    { if (font["id"].as<std::string>() == "FONT_BIG") big.load(font); if (font["id"].as<std::string>() == "FONT_SMALL") small.load(font); }
    SDL_Color palette[256]; for (int i = 0; i < 256; ++i) palette[i] = {Uint8(i), Uint8(i), Uint8(i), 0};
    for (const std::string master : {"xcom1", "xcom2"}) {
        Language language; language.loadFile(std::string(argv[1]) + "/bin/standard/" + master + "/Language/en-US.yml");
        std::vector<std::unique_ptr<RuleResearch>> rules;
        const std::string ruleName = master == "xcom1" ? "STR_LASER_PISTOL" : "STR_GAUSS_PISTOL";
        for (unsigned i = 0; i < 40; ++i) {
            rules.emplace_back(new RuleResearch(ruleName));
            rules.back()->load(YAML::Load("cost: 100\nneedItem: true\ndestroyItem: true"), i + 1);
        }
        RuleResearch indirect("INDIRECT_SECRET"); indirect.load(YAML::Load("cost: 0\nrequires: [SECRET]"), 100);
        ResearchHarness::Base base;
        for (unsigned i = 0; i < rules.size(); ++i) base.addResearch(new ResearchProject(rules[i].get(), 100));
        base.storage.addItem(ruleName, 5);
        ResearchHarness::Game game; game.big = &big; game.small = &small; game.language = &language; game.palette = palette;
        ResearchHarness::State::_game = &game;
        for (auto &rule : rules) game.rules.push_back(rule.get());
        game.rules.push_back(&indirect);
        Surface canvas(320, 200); canvas.setPalette(palette); game.background.setPalette(palette); game.background.clear(40);
        auto *research = new ResearchHarness::ResearchState(&base); game.pushState(research); research->init(); game.pendingInit = false;
        AgentUiState ui;
        check(!research->agentUiState(ui), "unpainted research list withheld");
        game.draw(canvas); check(research->agentUiState(ui), "painted research provider available");
        validateList(ui, "research", 3, 3); check(ui.rows.size() >= 8, "meaningful multi-column stock-font research rows");
        check(ui.offers("select_research_project") && ui.offers("open_new_research") && ui.offers("close_research_list"), "native research actions available");
        const auto first = ui;
        const auto seed = RNG::getSeed(); const int stock = base.storage.getItem(ruleName), staff = base._scientists;
        research->agentUiState(ui);
        check(ui.sameBinding(first) && seed == RNG::getSeed() && stock == base.storage.getItem(ruleName) && staff == base._scientists,
            "provider read is stable and consumes no RNG, item or staff");
        std::swap(base._research[0], base._research[1]); check(!research->agentUiState(ui), "stale model mapping withheld before row dispatch");
        std::swap(base._research[0], base._research[1]);
        research->fillProjectList(); game.draw(canvas); research->agentUiState(ui);
        check(!first.sameBinding(ui), "same-content ordinary list refill rejects ABA");
        Surface cover(8, 8, research->_lstResearch->getX() + 3, research->_lstResearch->getY()); cover.setPalette(palette); cover.clear(90);
        game._cursor = &cover; game.draw(canvas); check(research->agentUiState(ui), "row overlay keeps valid research screen");
        check(!ui.rows.empty() && ui.rows[0] > 0, "covered first row omitted and handles compacted"); validateList(ui, "research", 3, 3);
        const std::size_t remapped = ui.rows.front();
        AgentRequest rowRequest; rowRequest.action = "select_research_project"; rowRequest.choice = "r0";
        check(research->agentDispatch(rowRequest), "covered-row compact handle can activate");
        auto *remappedInfo = dynamic_cast<ResearchHarness::ResearchInfoState *>(game.top);
        check(remappedInfo && remappedInfo->_project == base.getResearch()[remapped], "compact handle preserves exact underlying project mapping");
        remappedInfo->btnOkClick(nullptr);
        game._cursor = nullptr; game.draw(canvas);
        // Native hover and the ordinary callback share the very same row helper.
        SDL_Event hoverEvent = {}; hoverEvent.type = SDL_MOUSEMOTION;
        Action hoverAction(&hoverEvent, 1, 1, 0, 0);
        hoverAction.setMouseAction(research->_lstResearch->getX() + 3, research->_lstResearch->getY() + 17,
            research->_lstResearch->getX(), research->_lstResearch->getY());
        research->_lstResearch->mouseOver(&hoverAction, nullptr);
        const auto ordinaryRow = research->_lstResearch->getSelectedRow(); research->onSelectProject(nullptr);
        remappedInfo = dynamic_cast<ResearchHarness::ResearchInfoState *>(game.top);
        check(remappedInfo && remappedInfo->_project == base.getResearch()[ordinaryRow], "ordinary mouse selection uses the shared exact mapping helper");
        remappedInfo->btnOkClick(nullptr); game.draw(canvas);
        const std::string savedCell = research->_lstResearch->getCellText(0, 0);
        research->_lstResearch->setCellText(0, 0, std::string(1025, 'X')); game.draw(canvas);
        check(research->agentUiState(ui) && (ui.rows.empty() || ui.rows.front() != 0), "overlong/clipped native row is never exposed");
        research->_lstResearch->setCellText(0, 0, savedCell); game.draw(canvas);
        std::vector<ResearchProject *> parked; parked.swap(base._research); research->fillProjectList(); game.draw(canvas);
        check(research->agentUiState(ui) && ui.rows.empty() && !ui.offers("select_research_project") && ui.offers("close_research_list"), "empty native research list keeps only visible controls");
        parked.swap(base._research); research->fillProjectList(); game.draw(canvas);
        Surface titleCover(310, 17, 5, 8); titleCover.setPalette(palette); titleCover.clear(90); research->_surfaces.push_back(&titleCover);
        game.draw(canvas); check(research->agentUiState(ui) && !ui.offers("select_research_project") && ui.offers("close_research_list"), "later title cover suppresses row action only");
        research->_surfaces.pop_back(); game.draw(canvas);
        research->_modal = research->_btnOk; check(!research->agentUiState(ui), "modal research unavailable"); research->_modal = nullptr;
        game.top = nullptr; check(!research->agentUiState(ui), "non-top research unavailable"); game.top = research;
        AgentRequest bad; bad.action = "select_research_project"; bad.choice = "r00";
        check(!research->agentDispatch(bad), "noncanonical direct row rejected"); bad.choice = "r32"; check(!research->agentDispatch(bad), "out-of-domain direct row rejected");
        TableChannel channel(master + "-research");
        channel.state.project = [&](AgentUiState &out) { return game.top && game.top->agentUiState(out); };
        channel.state.handler = [&](const AgentRequest &request) { check(game.top && game.top->agentDispatch(request), "actual native research dispatcher"); };
        channel.read(); channel.events();
        Surface fullCover(320, 200); fullCover.setPalette(palette); fullCover.clear(90); game._cursor = &fullCover; game.draw(canvas);
        check(!research->agentUiState(ui), "fully covered research has no typed-state witness");
        auto covered = YAML::Load(channel.read());
        check(covered["state"].as<std::string>() == "unsupported" && covered["capability"].IsNull(), "Controller exports unsupported/null for full research overlay");
        game._cursor = nullptr; game.draw(canvas); channel.read();
        channel.queue("select_research_project", "r0"); research->fillProjectList(); game.draw(canvas); channel.frame();
        auto rejected = channel.receive(); check(!rejected.empty() && YAML::Load(rejected)["status"].as<std::string>() == "unavailable" && channel.state.calls == 0,
            "Controller rejects list/mapping refill ABA before mutation");
        channel.read(); channel.queue("scroll_visible_list", "down");
        research->_lstResearch->scrollDown(false, false); game.draw(canvas); research->_lstResearch->scrollUp(false, false); game.draw(canvas); channel.frame();
        rejected = channel.receive(); check(!rejected.empty() && YAML::Load(rejected)["status"].as<std::string>() == "unavailable" && channel.state.calls == 0,
            "Controller rejects native scroll away/back ABA");
        const auto activeBefore = base.getResearch().size(); const auto seedBeforeExisting = RNG::getSeed();
        perform(channel, game, canvas, "select_research_project", "r0");
        auto *info = dynamic_cast<ResearchHarness::ResearchInfoState *>(game.top);
        check(info && info->_project == base.getResearch()[0] && base.getResearch().size() == activeBefore && RNG::getSeed() == seedBeforeExisting,
            "existing selection opens exact mapped project without creation or RNG");
        check(info->agentUiState(ui), "painted info projection"); validateInfo(ui);
        check(ui.offers("adjust_scientists") && ui.offers("accept_research") && ui.offers("cancel_research_project"), "native info controls available");
        game._cursor = &fullCover; game.draw(canvas); check(!info->agentUiState(ui), "fully covered research info has no typed-state witness");
        covered = YAML::Load(channel.read()); check(covered["state"].as<std::string>() == "unsupported" && covered["capability"].IsNull(), "Controller exports unsupported/null for full info overlay");
        game._cursor = nullptr; game.draw(canvas); info->agentUiState(ui);
        const auto beforeInfo = ui; const int beforeAssigned = info->_project->getAssigned();
        perform(channel, game, canvas, "adjust_scientists", "increase");
        check(info->_project->getAssigned() == beforeAssigned + 1 && base._scientists == staff - 1 && !info->_timerMore->isRunning() && !info->_timerLess->isRunning(),
            "semantic increase is one native scientist and no timer or hold");
        perform(channel, game, canvas, "adjust_scientists", "decrease");
        info->agentUiState(ui); check(info->_project->getAssigned() == beforeAssigned && base._scientists == staff && !beforeInfo.sameBinding(ui),
            "scientist away/back has fresh binding despite same displayed count");
        channel.read(); channel.queue("adjust_scientists", "increase");
        info->moreByValue(1); game.draw(canvas); info->lessByValue(1); game.draw(canvas); channel.frame();
        rejected = channel.receive(); check(!rejected.empty() && YAML::Load(rejected)["status"].as<std::string>() == "unavailable",
            "Controller rejects staff change away/back before dispatch");
        Surface arrowCover(13, 14, 195, 100); arrowCover.setPalette(palette); arrowCover.clear(90); game._cursor = &arrowCover; game.draw(canvas);
        info->agentUiState(ui); validateInfo(ui); check(ui.context.size() == 2 && !ui.context[0] && ui.context[1] && action(ui, "adjust_scientists")->label == info->_txtLess->getText(),
            "covered increase omitted, decrease supplies descriptor");
        bad.action = "adjust_scientists"; bad.choice = "increase"; check(!info->agentDispatch(bad), "unoffered direction cannot execute");
        game._cursor = nullptr; game.draw(canvas);
        Surface allocatedCover(210, 17, 61, 80); allocatedCover.setPalette(palette); allocatedCover.clear(90); game._fpsCounter = &allocatedCover; game.draw(canvas);
        info->agentUiState(ui); check(!ui.offers("adjust_scientists") && ui.offers("accept_research"), "covered allocation removes staff authority only");
        game._fpsCounter = nullptr; game.draw(canvas);
        Surface infoTitleCover(210, 17, 61, 40); infoTitleCover.setPalette(palette); infoTitleCover.clear(90); info->_surfaces.push_back(&infoTitleCover); game.draw(canvas);
        info->agentUiState(ui); check(!ui.offers("adjust_scientists") && !ui.offers("accept_research") && !ui.offers("cancel_research_project"), "missing title removes destructive info authority");
        info->_surfaces.pop_back(); game.draw(canvas);
        base._scientists = 0; info->setAssignedScientist(); game.draw(canvas); const int noOpAssigned = info->_project->getAssigned();
        perform(channel, game, canvas, "adjust_scientists", "increase");
        check(info->_project->getAssigned() == noOpAssigned && base._scientists == 0, "exhausted scientists preserve ordinary no-op");
        base._scientists = staff; base.laboratories = base.getAllocatedScientists(); info->setAssignedScientist(); game.draw(canvas);
        perform(channel, game, canvas, "adjust_scientists", "increase");
        check(info->_project->getAssigned() == noOpAssigned && base._scientists == staff, "exhausted laboratory capacity preserves ordinary no-op");
        base.laboratories = 20; info->setAssignedScientist(); game.draw(canvas);
        perform(channel, game, canvas, "adjust_scientists", "decrease");
        check(info->_project->getAssigned() == 0 && base._scientists == staff, "zero allocation preserves ordinary decrease no-op");
        perform(channel, game, canvas, "accept_research");
        check(game.top == research && base.getResearch().size() == activeBefore, "ordinary research OK only pops dialog");
        perform(channel, game, canvas, "open_new_research");
        auto *choices = dynamic_cast<ResearchHarness::NewResearchListState *>(game.top);
        check(choices && choices->agentUiState(ui), "native new-research list opens"); validateList(ui, "new_research", 1, 0);
        check(ui.rows.size() >= 6 && choices->_projects.size() == rules.size() && YAML::Load(ui.visible)["headings"][0].IsNull(), "ordinary availability excludes indirect-only rules and no invented heading");
        check(ui.offers("select_research_project") && ui.offers("close_research_list"), "new-research mapped row and ordinary close");
        game._cursor = &fullCover; game.draw(canvas); check(!choices->agentUiState(ui), "fully covered new-research has no typed-state witness");
        covered = YAML::Load(channel.read()); check(covered["state"].as<std::string>() == "unsupported" && covered["capability"].IsNull(), "Controller exports unsupported/null for full new-research overlay");
        game._cursor = nullptr; game.draw(canvas);
        const auto beforeNewSeed = RNG::getSeed();
        perform(channel, game, canvas, "select_research_project", "r0"); info = dynamic_cast<ResearchHarness::ResearchInfoState *>(game.top);
        check(info && info->_rule == choices->_projects[0] && base.getResearch().size() == activeBefore + 1 && base.storage.getItem(ruleName) == stock - 1 && RNG::getSeed() != beforeNewSeed,
            "ordinary new selection immediately creates project, consumes item and RNG");
        perform(channel, game, canvas, "adjust_scientists", "increase");
        check(info->_project->getAssigned() == 1 && base._scientists == staff - 1, "new project's first scientist assigned ordinarily");
        perform(channel, game, canvas, "cancel_research_project");
        check(game.top == choices && base.getResearch().size() == activeBefore && base.storage.getItem(ruleName) == stock && base._scientists == staff,
            "ordinary Cancel removes unfinished project and refunds item and staff");
        perform(channel, game, canvas, "select_research_project", "r0"); info = dynamic_cast<ResearchHarness::ResearchInfoState *>(game.top);
        perform(channel, game, canvas, "adjust_scientists", "increase");
        info->_project->setSpent(info->_project->getCost());
        perform(channel, game, canvas, "cancel_research_project");
        check(game.top == choices && base.getResearch().size() == activeBefore && base.storage.getItem(ruleName) == stock - 1 && base._scientists == staff,
            "ordinary finished-project Cancel returns staff without refunding a consumed item");
        perform(channel, game, canvas, "close_research_list"); check(game.top == research, "new-research close returns one state");
        perform(channel, game, canvas, "close_research_list"); check(!game.top, "research close returns one state");
        check(SDL_GetVideoSurface() == nullptr && !(SDL_WasInit(SDL_INIT_VIDEO)), "no GUI or game execution");
        if (master == "xcom2") {
            agentObservePaint(-1);
            for (ResearchHarness::State *failed : std::vector<ResearchHarness::State *>{research, choices, info}) {
                game.top = failed;
                check(!failed->agentUiState(ui), "failed paint blocks each research provider before model reads");
                bad.action = failed == info ? "accept_research" : "close_research_list";
                check(!failed->agentDispatch(bad), "failed paint blocks each research dispatcher");
            }
            game.top = nullptr;
        }
        std::cout << master << " research native routes PASS\n";
    }
    check(!agentPaintHealthy(), "failed global paint latch is fail-closed");
    std::cout << "AgentResearchProviderTest PASS " << checks << " checks\n";
}
