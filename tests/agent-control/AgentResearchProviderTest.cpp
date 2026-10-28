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
        request = "E " + token(command++) + " 1 " + epoch + " " + state["capability"].as<std::string>() + " " +
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
        for (const auto &cell : row["cells"])
            check((screen == "new_research" && cell.IsNull()) || (cell.IsScalar() && cell.as<std::string>().size() <= 1024),
                "research cell is complete bounded text or explicitly unknown new-research label");
    }
    if (const auto *a = action(ui, "select_research_project"))
        check(!v["title"].IsNull() && v["rows"].size() && a->argumentKind == "project_row" &&
            (screen == "new_research" ? (a->label == "Select research project" && a->labelSource == "visible_control_semantics") :
                (a->label == v["rows"][0]["cells"][0].as<std::string>() && a->labelSource == "visible_text")),
            "row action uses audited control semantics only for new-research and exact first text elsewhere");
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

void recordCase(TableChannel &channel, const std::string &name)
{
    check(!channel.schedule.empty() && YAML::Load(channel.last)["type"].as<std::string>() == "state",
        "named case references an actual controller state packet");
    const std::string file = channel.prefix + "-wire-" + std::to_string(channel.schedule.size() - 1) + ".json";
    channel.save(channel.prefix + "-case-" + name + ".json",
        "{\"case\":" + agentJsonString(name) + ",\"state_file\":" + agentJsonString(file) + "}");
}

void stockNewResearchOrder(const std::string &source, const std::string &master,
    Font &big, Font &small, Language &language, SDL_Color *palette)
{
    const unsigned startChecks = checks;
    // Keep the existing repeated-pistol lifecycle cases below. These are the
    // actual stock new-game choices, whose adjacent coarse ink boxes overlap.
    // TFTD's real research key is STR_GAUSS_TECH, not STR_GAUSS_TECHNOLOGY.
    const std::vector<std::string> keys = master == "xcom1" ?
        std::vector<std::string>{"STR_LASER_WEAPONS", "STR_MOTION_SCANNER", "STR_MEDI_KIT"} :
        std::vector<std::string>{"STR_GAUSS_TECH", "STR_PARTICLE_DISTURBANCE_SENSOR", "STR_MEDI_KIT"};
    const std::vector<std::string> labels = master == "xcom1" ?
        std::vector<std::string>{"Laser Weapons", "Motion Scanner", "Medi-Kit"} :
        std::vector<std::string>{"Gauss Technology", "Particle Disturbance Sensor", "Medi-Kit"};
    std::vector<std::unique_ptr<RuleResearch>> rules;
    for (const auto &node : YAML::LoadFile(source + "/bin/standard/" + master + "/research.rul")["research"]) {
        const auto name = node["name"].as<std::string>();
        if (std::find(keys.begin(), keys.end(), name) == keys.end()) continue;
        rules.emplace_back(new RuleResearch(name));
        rules.back()->load(node, rules.size());
        check(!node["dependencies"] && !rules.back()->needItem() && rules.back()->getRequirements().empty(),
            "stock initial choices need no prerequisite, item or indirect unlock");
    }
    check(rules.size() == keys.size(), "all three exact stock research definitions exist");
    ResearchHarness::Base base;
    ResearchHarness::Game game; game.big = &big; game.small = &small; game.language = &language; game.palette = palette;
    ResearchHarness::State::_game = &game;
    for (std::size_t i = 0; i < rules.size(); ++i) {
        check(rules[i]->getName() == keys[i] && std::string(language.getString(keys[i])) == labels[i],
            "actual stock rule file and English translations retain initial choice order");
        game.rules.push_back(rules[i].get());
    }
    Surface canvas(320, 200); canvas.setPalette(palette); game.background.setPalette(palette); game.background.clear(40);
    auto *choices = new ResearchHarness::NewResearchListState(&base); game.pushState(choices);
    choices->init(); game.pendingInit = false;
    AgentUiState ui;
    check(!choices->agentUiState(ui), "unpainted actual stock choices withheld");
    game.draw(canvas);
    if (master == "xcom2") {
        // The full stock TFTD pair is genuinely overpainted, unlike UFO.
        // Its surviving control is selectable, but the first label stays null.
        check(choices->agentUiState(ui), "full stock TFTD provider remains available");
        validateList(ui, "new_research", 1, 0);
        check(ui.rows == std::vector<std::size_t>({0, 1, 2}) && ui.visible.find(labels[0]) == std::string::npos,
            "full stock TFTD retains first row-control without revealing its overwritten label");
        const auto full = YAML::Load(ui.visible);
        check(full["rows"].size() == 3 && full["rows"][0]["cells"][0].IsNull() &&
            full["rows"][1]["cells"][0].as<std::string>() == labels[1] && full["rows"][2]["cells"][0].as<std::string>() == labels[2],
            "full stock TFTD exposes null, PDS and Medi-Kit in ordinary control order");
        std::vector<AgentListRow> completeRows, controlRows;
        check(choices->_lstResearch->agentRows(completeRows) && completeRows.size() == 2 &&
            completeRows[0].logicalIndex == 1 && completeRows[1].logicalIndex == 2,
            "raw complete-label path still rejects genuinely overwritten Gauss text");
        check(choices->_lstResearch->agentControlRows(controlRows) && controlRows.size() == 3 &&
            controlRows[0].logicalIndex == 0 && !controlRows[0].completeText && controlRows[0].cells[0].empty() &&
            controlRows[1].logicalIndex == 1 && controlRows[2].logicalIndex == 2,
            "typed control witness retains first row with explicitly absent internal text");
        TextList firstOnly(198, 88, 53, 54), secondOnly(198, 88, 53, 54);
        for (TextList *list : {&firstOnly, &secondOnly}) {
            list->setPalette(palette); list->initText(&big, &small, &language); list->setColor(80);
            list->setColumns(1, 190); list->setSelectable(true); list->setBackground(choices->_window);
            list->setMargin(8); list->setAlign(ALIGN_CENTER);
        }
        firstOnly.addRow(1, labels[0].c_str());
        secondOnly.addRow(1, ""); secondOnly.addRow(1, labels[1].c_str());
        Surface separateCanvas(320, 200); separateCanvas.setPalette(palette);
        firstOnly.blit(&separateCanvas); secondOnly.blit(&separateCanvas);
        std::vector<AgentListRow> firstNative, secondNative;
        check(firstOnly.agentRows(firstNative) && firstNative.size() == 1 && firstNative[0].cells[0] == labels[0],
            "isolated actual Gauss text has complete stock glyphs and fits the ordinary cell");
        check(secondOnly.agentRows(secondNative) && secondNative.size() == 2 && secondNative[1].cells[0] == labels[1],
            "isolated actual Particle Disturbance Sensor text is complete and fits the ordinary cell");
        const unsigned firstIndices[] = {84, 84, 85, 84, 84, 84};
        const unsigned secondIndices[] = {85, 84, 84, 84, 84, 85};
        unsigned collisions = 0, changed = 0;
        for (int y = 0; y < firstOnly.getHeight(); ++y) for (int x = 0; x < firstOnly.getWidth(); ++x) {
            const auto a = firstOnly.getPixel(x, y), b = secondOnly.getPixel(x, y);
            if (a && b) {
                check(collisions < 6 && x + firstOnly.getX() == 187 + int(collisions) && y + firstOnly.getY() == 62 &&
                    a == firstIndices[collisions] && b == secondIndices[collisions],
                    "full TFTD true-overlap pixels retain exact coordinates and before/after indices");
                std::cout << "TFTD true ink overlap (" << x + firstOnly.getX() << ',' << y + firstOnly.getY()
                    << ") indices " << unsigned(a) << " -> " << unsigned(b) << '\n';
                ++collisions; if (a != b) ++changed;
            }
        }
        check(collisions == 6 && changed == 3, "stock TFTD has six real opaque collisions including three changed colors");
        {
            TableChannel fullChannel(master + "-stock-full-overlap");
            fullChannel.state.project = [&](AgentUiState &out) { return game.top && game.top->agentUiState(out); };
            fullChannel.state.handler = [&](const AgentRequest &request) {
                check(game.top && game.top->agentDispatch(request), "actual full-stock TFTD native dispatcher");
            };
            fullChannel.read(); recordCase(fullChannel, "tftd_full_unknown_first_control");
            perform(fullChannel, game, canvas, "select_research_project", "r0");
            auto *info = dynamic_cast<ResearchHarness::ResearchInfoState *>(game.top);
            check(info && info->_rule == rules[0].get() && base.getResearch().size() == 1,
                "full stock TFTD r0 activates the genuine first control through the ordinary Gauss handler");
            check(info->agentUiState(ui) && YAML::Load(ui.visible)["title"].as<std::string>() == labels[0],
                "Gauss name becomes visible only after the actual detail dialog paints its full title");
            fullChannel.read(); recordCase(fullChannel, "tftd_full_unknown_first_opens_gauss");
            info->btnCancelClick(nullptr); game.draw(canvas);
            check(game.top == choices && base.getResearch().empty(), "full TFTD negative-route cleanup is ordinary cancel");
        }
        // This is explicitly isolated availability, never claimed as the full
        // TFTD stock chooser. It tests Gauss's genuine positive native route.
        game.rules.resize(1); choices->fillProjectList();
        choices->_lstResearch->mouseOut(nullptr, nullptr); game.draw(canvas);
    }
    const std::vector<std::size_t> expectedRows = master == "xcom1" ? std::vector<std::size_t>{0, 1, 2} : std::vector<std::size_t>{0};
    const auto allExpected = [&]() {
        check(choices->agentUiState(ui), "painted actual stock new-research provider available");
        validateList(ui, "new_research", 1, 0);
        const auto visible = YAML::Load(ui.visible);
        check(ui.rows == expectedRows && visible["rows"].size() == expectedRows.size(),
            "complete UFO stock list or explicitly isolated Gauss admits every genuinely uncovered row");
        for (std::size_t i = 0; i < expectedRows.size(); ++i)
            check(visible["rows"][i]["handle"].as<std::string>() == "r" + std::to_string(i) &&
                visible["rows"][i]["cells"][0].as<std::string>() == labels[i],
                "actual stock rows have exact visible labels and compact handles in order");
        check(action(ui, "select_research_project") && action(ui, "select_research_project")->label == "Select research project" &&
            action(ui, "select_research_project")->labelSource == "visible_control_semantics",
            "new-research selection uses the fixed visible-control semantic descriptor");
    };
    allExpected();
    const auto initialUi = ui;
    std::vector<AgentListRow> paintedRows;
    check(choices->_lstResearch->agentRows(paintedRows) && paintedRows.size() == expectedRows.size(),
        "stock rows originate in actual TextList paint witnesses");
    const auto extent = [](const AgentListRow &row) {
        SDL_Rect result = row.regions.front();
        for (const auto &part : row.regions) {
            const int right = std::max(result.x + result.w, part.x + part.w);
            const int bottom = std::max(result.y + result.h, part.y + part.h);
            result.x = std::min(result.x, part.x); result.y = std::min(result.y, part.y);
            result.w = right - result.x; result.h = bottom - result.y;
        }
        return result;
    };
    for (const auto &row : paintedRows) check(!row.regions.empty(), "every stock row has actual painted ink");
    const auto firstExtent = extent(paintedRows[0]);
    if (master == "xcom1") {
        const auto secondExtent = extent(paintedRows[1]);
        check(firstExtent.x < secondExtent.x + secondExtent.w && firstExtent.x + firstExtent.w > secondExtent.x &&
            firstExtent.y < secondExtent.y + secondExtent.h && firstExtent.y + firstExtent.h > secondExtent.y,
            "UFO stock regression genuinely has overlapping adjacent coarse glyph extents");
    }
    check(agentRegionsDisjoint(ui), "admitted stock rows retain disjoint actual ink");
    TableChannel channel(master + "-stock-new-research");
    channel.state.project = [&](AgentUiState &out) { return game.top && game.top->agentUiState(out); };
    channel.state.handler = [&](const AgentRequest &request) { check(game.top && game.top->agentDispatch(request), "actual stock native dispatcher"); };
    channel.read(); recordCase(channel, master == "xcom1" ? "ufo_stock_all_three" : "tftd_gauss_isolated");
    const auto ink = paintedRows[0].regions.front();
    check(choices->_lstResearch->getPixel(ink.x - choices->_lstResearch->getX(), ink.y - choices->_lstResearch->getY()) != 0,
        "overlay target is a real nontransparent first-row pixel");
    Surface inkCover(1, 1, ink.x, ink.y); inkCover.setPalette(palette); inkCover.clear(90);
    for (unsigned layer = 0; layer < 3; ++layer) {
        if (layer == 0) game._cursor = &inkCover;
        if (layer == 1) game._fpsCounter = &inkCover;
        if (layer == 2) choices->_surfaces.push_back(&inkCover);
        game.draw(canvas);
        const std::vector<std::size_t> uncovered(expectedRows.begin() + 1, expectedRows.end());
        check(choices->agentUiState(ui) && ui.rows == uncovered,
            "one actual-ink pixel covered by cursor, FPS or later sibling omits exactly that stock row");
        if (!uncovered.empty())
            check(YAML::Load(ui.visible)["rows"][0]["cells"][0].as<std::string>() == labels[1] &&
                action(ui, "select_research_project")->label == "Select research project",
                "true overlap compacts handles without inventing a label-based control description");
        else check(!ui.offers("select_research_project"), "covered isolated Gauss has no row authority");
        channel.read(); recordCase(channel, master + std::string(layer == 0 ? "_cursor_ink_overlay" :
            layer == 1 ? "_fps_ink_overlay" : "_sibling_ink_overlay"));
        game._cursor = nullptr; game._fpsCounter = nullptr;
        if (layer == 2) choices->_surfaces.pop_back();
    }
    int gapX = -1, gapY = -1;
    for (int y = firstExtent.y; y < firstExtent.y + firstExtent.h && gapX < 0; ++y)
        for (int x = firstExtent.x; x < firstExtent.x + firstExtent.w; ++x) {
            bool occupied = false;
            for (const auto &row : paintedRows) for (const auto &part : row.regions)
                occupied |= x >= part.x && x < part.x + part.w && y >= part.y && y < part.y + part.h;
            if (!occupied) { gapX = x; gapY = y; break; }
        }
    check(gapX >= 0 && choices->_lstResearch->getPixel(gapX - choices->_lstResearch->getX(), gapY - choices->_lstResearch->getY()) == 0,
        "stock coarse extent contains a truly transparent pixel between glyph strokes");
    Surface gapCover(1, 1, gapX, gapY); gapCover.setPalette(palette); gapCover.clear(90);
    game._cursor = &gapCover; game.draw(canvas);
    check(choices->_lstResearch->agentRows(paintedRows) && paintedRows.size() == expectedRows.size(),
        "transparent glyph gap preserves raw complete text evidence");
    check(choices->agentUiState(ui) && ui.rows == std::vector<std::size_t>(expectedRows.begin() + 1, expectedRows.end()),
        "overlay in a glyph gap still suppresses the obstructed complete click-band control");
    channel.read(); recordCase(channel, master + "_click_band_gap_overlay");
    game._cursor = nullptr; game.draw(canvas); allExpected();
    check(ui.sameBinding(initialUi), "unchanged actual stock paint has stable private binding");

    choices->_lstResearch->setCellText(0, 0, std::string(1025, 'X')); game.draw(canvas);
    check(choices->_lstResearch->agentRows(paintedRows) &&
        (paintedRows.empty() || paintedRows.front().logicalIndex != 0), "clipped stock text has no complete-label witness");
    check(choices->agentUiState(ui) && ui.rows == expectedRows && YAML::Load(ui.visible)["rows"][0]["cells"][0].IsNull(),
        "clipped first stock label remains unknown while its unobstructed typed row control survives");
    channel.read(); recordCase(channel, master + "_clipped_first_label_unknown");
    choices->_lstResearch->setCellText(0, 0, labels[0]); game.draw(canvas); allExpected();
    channel.read();
    const auto seedBeforeRejection = RNG::getSeed();
    channel.queue("select_research_project", "r0"); choices->fillProjectList(); game.draw(canvas); channel.frame();
    auto rejected = channel.receive();
    check(!rejected.empty() && YAML::Load(rejected)["status"].as<std::string>() == "unavailable" &&
        channel.state.calls == 0 && base.getResearch().empty() && RNG::getSeed() == seedBeforeRejection,
        "stale stock mapping refill rejects r0 before native project creation or RNG");
    channel.read(); channel.queue("select_research_project", "r0");
    choices->_lstResearch->setCellText(0, 0, labels[2]); game.draw(canvas);
    choices->_lstResearch->setCellText(0, 0, labels[0]); game.draw(canvas); channel.frame();
    rejected = channel.receive();
    check(!rejected.empty() && YAML::Load(rejected)["status"].as<std::string>() == "unavailable" &&
        channel.state.calls == 0 && base.getResearch().empty() && RNG::getSeed() == seedBeforeRejection,
        "stock row text away/back rejects the stale capability despite identical restored pixels");
    allExpected();
    Surface fullCover(320, 200); fullCover.setPalette(palette); fullCover.clear(90);
    game._cursor = &fullCover; game.draw(canvas);
    const auto covered = YAML::Load(channel.read()); recordCase(channel, master + "_full_overlay");
    check(covered["state"].as<std::string>() == "unsupported" && covered["capability"].IsNull(),
        "fully covered stock chooser still exports unsupported and no capability");
    game._cursor = nullptr; game.draw(canvas); allExpected();

    const auto beforeSelectionSeed = RNG::getSeed(); const auto beforePushes = game.pushes;
    perform(channel, game, canvas, "select_research_project", "r0");
    auto *info = dynamic_cast<ResearchHarness::ResearchInfoState *>(game.top);
    check(info && info->_rule == rules[0].get() && base.getResearch().size() == 1 &&
        info->_project == base.getResearch().front() && game.pushes == beforePushes + 1 && RNG::getSeed() != beforeSelectionSeed,
        "fresh stock r0 invokes ordinary first-rule creation exactly once");
    const auto afterSelectionSeed = RNG::getSeed();
    channel.send(channel.request); channel.controller->poll();
    const auto duplicate = channel.receive();
    check(!duplicate.empty() && YAML::Load(duplicate)["status"].as<std::string>() == "dispatched" &&
        game.top == info && base.getResearch().size() == 1 && game.pushes == beforePushes + 1 && RNG::getSeed() == afterSelectionSeed,
        "duplicate stock r0 does not create another project, push another state or consume RNG");
    info->btnCancelClick(nullptr); game.draw(canvas);
    check(game.top == choices && base.getResearch().empty(), "ordinary stock cancel cleans up the created project");
    choices->_lstResearch->mouseOut(nullptr, nullptr); game.draw(canvas); allExpected();
    SDL_Event hoverEvent = {}; hoverEvent.type = SDL_MOUSEMOTION;
    Action hoverAction(&hoverEvent, 1, 1, 0, 0);
    hoverAction.setMouseAction(choices->_lstResearch->getX() + 3, choices->_lstResearch->getY() + 1,
        choices->_lstResearch->getX(), choices->_lstResearch->getY());
    choices->_lstResearch->mouseOver(&hoverAction, nullptr);
    check(choices->_lstResearch->getSelectedRow() == 0, "ordinary stock hover selects the first physical row");
    choices->onSelectProject(nullptr);
    info = dynamic_cast<ResearchHarness::ResearchInfoState *>(game.top);
    check(info && info->_rule == rules[0].get() && base.getResearch().size() == 1,
        "ordinary mouse callback and semantic r0 choose the identical first stock rule");
    info->btnCancelClick(nullptr); game.draw(canvas);
    check(SDL_GetVideoSurface() == nullptr && !(SDL_WasInit(SDL_INIT_VIDEO)), "stock-order regression uses no GUI or real Game");
    std::cout << master << (master == "xcom1" ? " full stock new-research order PASS " :
        " full stock unknown-label control plus isolated Gauss positive PASS ") << checks - startChecks << " checks\n";
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
        stockNewResearchOrder(argv[1], master, big, small, language, palette);
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
