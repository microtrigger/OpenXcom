// SPDX-License-Identifier: GPL-3.0-or-later
// Actual offscreen Text/TextButton/TextList, stock font and native table geometry.
// Providers, ordinary close bodies and global overlay guard are extracted verbatim.
// No production Game, Base, storage, transfer, window or video device is created.
#include "../../src/Engine/Action.h"
#include "../../src/Engine/Font.h"
#include "../../src/Engine/Game.h"
#include "../../src/Engine/Language.h"
#include "../../src/Engine/LocalizedText.h"
#include "../../src/Engine/Options.h"
#include "../../src/Engine/State.h"
#include "../../src/Engine/Surface.h"
#include "../../src/Interface/Text.h"
#include "../../src/Interface/TextButton.h"
#include "../../src/Interface/TextList.h"
#include "../../src/Observer/AgentUi.h"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <algorithm>
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
void check(bool condition, const char *why)
{ ++checks; if (!condition) { std::cerr << "FAIL " << why << '\n'; std::exit(1); } }
void hover(TextList &list, int localY)
{
    SDL_Event event = {}; event.type = SDL_MOUSEMOTION;
    Action action(&event, 1, 1, 0, 0);
    action.setMouseAction(list.getX() + 3, list.getY() + localY, list.getX(), list.getY());
    list.mouseOver(&action, nullptr);
}
}
namespace OpenXcom {
Game *State::_game = nullptr;
#include "AgentBaseTableWidgetState.generated.inc"
#include "AgentBaseTableGameIdentity.generated.inc"
}
namespace BaseTables {
struct Game {
    const void *top = nullptr;
    Surface *_cursor = nullptr, *_fpsCounter = nullptr;
    unsigned pops = 0;
    bool isState(const void *state) const { return top == state; }
    void popState() { ++pops; top = nullptr; }
    bool agentOverlayClear(const Observer::AgentUiState &ui) const;
};
#include "AgentBaseTableOverlay.generated.inc"
struct StoresState {
    Game *_game = nullptr; Surface *_modal = nullptr;
    TextButton *_btnOk = nullptr;
    Text *_txtTitle = nullptr, *_txtItem = nullptr, *_txtQuantity = nullptr, *_txtSpaceUsed = nullptr;
    TextList *_lstStores = nullptr; std::vector<Surface *> _surfaces;
    bool agentUiState(Observer::AgentUiState &out) const;
    bool agentDispatch(const Observer::AgentRequest &request);
    void btnOkClick(Action *);
};
struct TransfersState {
    Game *_game = nullptr; Surface *_modal = nullptr;
    TextButton *_btnOk = nullptr;
    Text *_txtTitle = nullptr, *_txtItem = nullptr, *_txtQuantity = nullptr, *_txtArrivalTime = nullptr;
    TextList *_lstTransfers = nullptr; std::vector<Surface *> _surfaces;
    bool agentUiState(Observer::AgentUiState &out) const;
    bool agentDispatch(const Observer::AgentRequest &request);
    void btnOkClick(Action *);
};
#include "AgentStoresMethods.generated.inc"
#include "AgentTransfersMethods.generated.inc"
}
namespace {
void validate(const AgentUiState &ui, const std::string &screen, const std::string &close)
{
    const YAML::Node v = YAML::Load(ui.visible);
    check(ui.screen == screen && v.IsMap() && v.size() == 4, "exact table state and bounded four-field DTO");
    check(v["headings"].IsSequence() && v["headings"].size() == 3, "three nullable displayed headings");
    check(v["scroll"].IsMap() && v["scroll"].size() == 2, "only native up/down scroll flags");
    check(v["rows"].IsSequence() && v["rows"].size() <= 32, "rows bounded at 32");
    for (std::size_t i = 0; i < v["rows"].size(); ++i)
    {
        const YAML::Node row = v["rows"][i];
        check(row.IsMap() && row.size() == 3 && row["handle"].as<std::string>() == "r" + std::to_string(i), "fresh contiguous public row handles only");
        check(row["cells"].size() == 3 && !row["selected"].as<bool>(), "three columns and selected=false for a read-only table");
        for (const auto &cell : row["cells"]) check(cell.IsScalar() && cell.as<std::string>().size() <= 1024, "bounded visible UTF-8 cell");
    }
    for (const AgentUiAction &action : ui.actions)
        check(action.id == close || action.id == "scroll_visible_list", "no generic back, row activation or model mutation action");
    check(agentRegionsDisjoint(ui), "admitted text, rows and controls are disjoint");
}
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
template<class Table, class Draw> void controllerRoute(Table &state, TextList &list, BaseTables::Game &nativeGame,
    const Draw &draw, const std::string &screen, const std::string &close, const std::string &master)
{
    TableChannel channel(master + "-" + screen);
    channel.state.project = [&](AgentUiState &out) { return state.agentUiState(out); };
    channel.state.handler = [&](const AgentRequest &request) {
        check(state.agentDispatch(request), "controller invokes exact native table dispatcher");
        if (nativeGame.top != &state) channel.game.top = &channel.unsupportedParent;
    };
    const std::string first = channel.read();
    check(YAML::Load(first)["state"].as<std::string>() == screen && YAML::Load(first)["visible"]["rows"].size() >= 4,
        "actual nonempty table travels through controller R");
    const std::string events = channel.events();
    bool retained = false;
    for (const auto &event : YAML::Load(events)["events"])
        if (event["kind"].as<std::string>() == "presentation" && event["state"].as<std::string>() == screen &&
            event["visible"]["rows"].size() >= 4) retained = true;
    check(retained, "actual nonempty table retained independently in controller V");
    const auto before = list.getScroll(); channel.queue("scroll_visible_list", "down"); channel.frame();
    check(channel.state.calls == 1 && list.getScroll() == before + 1 && channel.receive().empty(),
        "controller queues exactly one native scroll without a pre-action receipt");
    draw(); channel.frame(); const std::string scrolled = channel.receive();
    check(!scrolled.empty() && YAML::Load(scrolled)["status"].as<std::string>() == "dispatched", "later painted frame supplies scroll receipt");
    channel.send(channel.request); channel.controller->poll();
    check(channel.receive() == scrolled && channel.state.calls == 1 && list.getScroll() == before + 1,
        "exact duplicate reuses receipt without repeating native scroll");
    channel.read(); channel.events();
    channel.queue("scroll_visible_list", "down");
    list.scrollUp(false, false); draw(); list.scrollDown(false, false); draw(); channel.frame();
    const std::string stale = channel.receive();
    check(!stale.empty() && YAML::Load(stale)["status"].as<std::string>() == "unavailable" && channel.state.calls == 1,
        "actual list away/back revision rejects stale queued authority");
    channel.read(); channel.queue(close); channel.frame();
    check(nativeGame.pops == 1 && nativeGame.top == nullptr && channel.state.calls == 2 && channel.receive().empty(),
        "controller invokes the exact ordinary one-pop close once");
    channel.frame(); const std::string closed = channel.receive();
    check(!closed.empty() && YAML::Load(closed)["status"].as<std::string>() == "dispatched" &&
        YAML::Load(closed)["observation"]["state"].as<std::string>() == "unsupported", "close later-frame receipt honestly observes an unprovided parent");
    channel.send(channel.request); channel.controller->poll();
    check(channel.receive() == closed && nativeGame.pops == 1 && channel.state.calls == 2, "duplicate close cannot pop another state");
    channel.events();
}
template<class Table> void exercise(Table &state, TextList &list, Text &title, Text &quantity,
    TextButton &ok, Surface &background, Surface &canvas, BaseTables::Game &game,
    const std::string &screen, const std::string &close, const std::string &master)
{
    const auto draw = [&]() { for (Surface *surface : state._surfaces) surface->blit(&canvas); };
    AgentUiState ui;
    check(!state.agentUiState(ui), "unpainted table unavailable");
    draw(); check(state.agentUiState(ui), "painted native-shaped table available");
    validate(ui, screen, close);
    check(ui.rows.size() >= 4, "meaningful stock-font rows, not a title-only green result");
    std::vector<AgentListRow> native;
    check(list.agentRows(native) && !native.empty(), "actual native list publishes paint witnesses");
    const auto initial = ui;
    const std::string initialFirstName = list.getCellText(0, 0), initialFirstValue = list.getCellText(0, 2);
    const YAML::Node columns = YAML::Load(ui.visible)["rows"];
    for (std::size_t i = 0; i < ui.rows.size(); ++i)
        check(columns[i]["cells"][0].as<std::string>() == list.getCellText(ui.rows[i], 0) &&
            columns[i]["cells"][1].as<std::string>() == std::to_string(10 + ui.rows[i]) &&
            columns[i]["cells"][2].as<std::string>() == (screen == "base_stores" ? "0.3" : "12"),
            "all three columns preserve exact stock widget labels, quantities and displayed value");
    const YAML::Node initialVisible = YAML::Load(ui.visible);
    check(!initialVisible["title"].IsNull() && !initialVisible["title"].as<std::string>().empty(), "displayed native title available");
    check(ui.offers(close) && ui.offers("scroll_visible_list"), "only visible OK and scroll are offered");
    const std::string shownOk = ok.agentLabel();
    for (const AgentUiAction &a : ui.actions) if (a.id == close)
        check(a.label == shownOk && a.labelSource == "visible_text" && a.argumentKind == "none", "exact painted OK descriptor");
    check(game.agentOverlayClear(ui), "initial projection passes unchanged global overlay guard");
    draw(); check(state.agentUiState(ui) && ui.sameBinding(initial), "idle redraw preserves binding");
    game.top = &game; check(!state.agentUiState(ui), "non-top table unavailable"); game.top = &state;
    state._modal = &ok; check(!state.agentUiState(ui), "unexpected modal blocks provider"); state._modal = nullptr;

    // The real setSelectable(true) highlight stays active, but grants no row action.
    hover(list, 2); check(!state.agentUiState(ui), "selector hover waits for actual paint");
    draw(); check(state.agentUiState(ui), "painted selector does not disable read-only table");
    check(list.agentRows(native) && !native.empty(), "hover keeps meaningful native rows");
    bool selected = false; for (const auto &row : native) selected = selected || row.selected;
    check(selected && ui.rows.size() >= 4, "actual ordinary selector remains painted and rows survive");
    validate(ui, screen, close);
    check(ui.visible.find("\"selected\":true") == std::string::npos, "selector does not leak as selectable action authority");
    list.mouseOut(nullptr, nullptr); draw(); check(state.agentUiState(ui), "ordinary mouse-out restores table");
    const auto beforeScroll = list.agentListRevision();
    AgentRequest request; request.action = "scroll_visible_list"; request.choice = "up";
    check(state.agentDispatch(request), "visible native top-arrow accepts its ordinary no-op");
    draw(); check(list.getScroll() == 0 && list.agentListRevision() == beforeScroll, "true top boundary has no change");
    request.choice = "down"; check(state.agentDispatch(request), "native scroll down dispatched");
    check(!state.agentUiState(ui), "scroll wait-for-paint blocks stale rows");
    draw(); check(state.agentUiState(ui) && list.getScroll() == 1 && !ui.sameBinding(initial), "one native row step changes private binding");
    request.choice = "up"; check(state.agentDispatch(request), "native scroll returns one row"); draw();
    list.scrollTo(999); draw(); check(state.agentUiState(ui), "native bottom page painted");
    const auto bottom = list.getScroll(), bottomRevision = list.agentListRevision();
    request.choice = "down"; check(state.agentDispatch(request), "visible native bottom-arrow accepts no-op");
    draw(); check(list.getScroll() == bottom && list.agentListRevision() == bottomRevision, "true bottom boundary has no change");
    request.choice = "sideways"; check(!state.agentDispatch(request), "unknown direction cannot scroll");
    request.action = "choose_visible_row"; request.choice = "r0"; check(!state.agentDispatch(request), "table never activates a row");
    list.scrollTo(0); draw(); check(state.agentUiState(ui), "restore native first page");

    // Optional predicate leaves existing callers exactly unchanged.
    AgentUiState legacy, permitted; AgentListProjection a, b;
    const auto order = agentFlattenPaintOrder(state._surfaces);
    check(agentProjectList(&list, &list, order, 3, false, legacy, a), "default legacy list projection succeeds");
    check(agentProjectList(&list, &list, order, 3, false, permitted, b,
        [](const AgentUiState &) { return true; }) && legacy.sameBinding(permitted) && a.rows == b.rows && a.scroll == b.scroll,
        "default-empty region predicate preserves exact existing caller behavior");

    check(list.agentRows(native) && native.size() >= 4 && !native.front().regions.empty(), "overlay fixture has real glyph bounds");
    const std::size_t coveredIndex = native.front().logicalIndex;
    const SDL_Rect ink = native.front().regions.front();
    Surface overlay(1, 1, ink.x, ink.y); overlay.clear(3); overlay.blit(&canvas);
    game._cursor = &overlay;
    check(state.agentUiState(ui) && ui.rows.size() + 1 == initial.rows.size() &&
        std::find(ui.rows.begin(), ui.rows.end(), coveredIndex) == ui.rows.end() && ui.offers(close), "cursor omits only intersecting row");
    check(game.agentOverlayClear(ui), "regional omission leaves global guard intact");
    validate(ui, screen, close);
    game._cursor = nullptr; game._fpsCounter = &overlay;
    check(state.agentUiState(ui) && ui.rows.size() + 1 == initial.rows.size() && game.agentOverlayClear(ui), "FPS omits only intersecting row");
    game._fpsCounter = nullptr; state._surfaces.push_back(&overlay); draw();
    check(state.agentUiState(ui) && ui.rows.size() + 1 == initial.rows.size(), "later surface omits only intersecting row");
    state._surfaces.pop_back();

    const Surface *up = nullptr, *down = nullptr; list.agentScrollControls(up, down);
    check(up && down, "both actual arrow footprints available");
    overlay.setX(up->getX()); overlay.setY(up->getY()); overlay.blit(&canvas); game._cursor = &overlay;
    check(state.agentUiState(ui), "arrow overlay preserves rest of table");
    YAML::Node v = YAML::Load(ui.visible);
    check(!v["scroll"]["up"].as<bool>() && v["scroll"]["down"].as<bool>() && ui.rows.size() == initial.rows.size(), "up overlay omits only up control");
    request.action = "scroll_visible_list"; request.choice = "up"; check(!state.agentDispatch(request), "covered up cannot dispatch");
    request.choice = "down"; check(state.agentDispatch(request), "uncovered down can still dispatch");
    game._cursor = nullptr; list.scrollTo(0); draw();
    overlay.setX(down->getX()); overlay.setY(down->getY()); overlay.blit(&canvas); game._fpsCounter = &overlay;
    check(state.agentUiState(ui), "down overlay preserves table"); v = YAML::Load(ui.visible);
    check(v["scroll"]["up"].as<bool>() && !v["scroll"]["down"].as<bool>(), "FPS hides only down arrow");
    game._fpsCounter = nullptr;

    SDL_Rect titleInk = {}; check(title.agentPaintedBounds(titleInk), "title glyph witness available");
    overlay.setX(titleInk.x); overlay.setY(titleInk.y); overlay.blit(&canvas); game._cursor = &overlay;
    check(state.agentUiState(ui) && YAML::Load(ui.visible)["title"].IsNull() && ui.rows.size() == initial.rows.size(), "cursor over title omits only title");
    game._cursor = nullptr;
    SDL_Rect quantityInk = {}; check(quantity.agentPaintedBounds(quantityInk), "heading glyph witness available");
    overlay.setX(quantityInk.x); overlay.setY(quantityInk.y); overlay.blit(&canvas); game._fpsCounter = &overlay;
    check(state.agentUiState(ui) && YAML::Load(ui.visible)["headings"][1].IsNull() && ui.rows.size() == initial.rows.size(), "FPS over heading omits only heading");
    game._fpsCounter = nullptr;
    overlay.setX(ok.getX()); overlay.setY(ok.getY()); overlay.blit(&canvas); game._cursor = &overlay;
    request.action = close;
    check(state.agentUiState(ui) && !ui.offers(close) && !state.agentDispatch(request) && game.pops == 0, "covered OK is withheld without generic back");
    game._cursor = nullptr; state._surfaces.push_back(&overlay); draw();
    check(state.agentUiState(ui) && !ui.offers(close), "later surface also blocks ordinary close");
    state._surfaces.pop_back();

    list.setCellText(0, 0, std::string(1025, 'W')); draw();
    check(state.agentUiState(ui) && std::find(ui.rows.begin(), ui.rows.end(), 0) == ui.rows.end() && ui.rows.size() >= 4, "oversized clipped row omitted without leaking source");
    list.setCellText(0, 0, "A\xF0\x9F\x8C\x8C"); draw();
    check(state.agentUiState(ui) && std::find(ui.rows.begin(), ui.rows.end(), 0) == ui.rows.end(), "missing-glyph row omitted");
    list.setCellText(0, 0, "ROW"); list.setCellText(0, 2, std::string(40, '9')); draw();
    check(state.agentUiState(ui) && std::find(ui.rows.begin(), ui.rows.end(), 0) == ui.rows.end(), "last column native clipping cannot leak full numeric text");
    list.setCellText(0, 2, "1"); draw();
    check(state.agentUiState(ui) && std::find(ui.rows.begin(), ui.rows.end(), 0) != ui.rows.end(), "fully drawn three-column row restored");
    const int x = list.getX(); list.setX(-1); draw(); check(!state.agentUiState(ui), "viewport-clipped list unavailable"); list.setX(x); draw();
    list.setVisible(false); check(!state.agentUiState(ui), "hidden list unavailable"); list.setVisible(true); draw();
    const std::string oldTitle = title.getText(); title.setText(std::string(1025, 'W')); draw();
    check(state.agentUiState(ui) && YAML::Load(ui.visible)["title"].IsNull(), "overlong clipped title omitted"); title.setText(oldTitle); draw();
    ok.setText(std::string(1025, 'W')); draw(); check(state.agentUiState(ui) && !ui.offers(close), "overlong clipped OK cannot grant close"); ok.setText(shownOk); draw();
    ok.setVisible(false); draw(); check(state.agentUiState(ui) && !ui.offers(close), "hidden OK omitted"); ok.setVisible(true); draw();
    list.setCellText(0, 0, initialFirstName); list.setCellText(0, 2, initialFirstValue); draw();
    controllerRoute(state, list, game, draw, screen, close, master);
    // Separate isolated close-unit scenario reuses widgets after restoring only
    // this test's owning-state pointer; no production stack or model exists.
    game.top = &state; game.pops = 0;
    list.clearList(); list.setHeight(list.getHeight()); draw();
    check(state.agentUiState(ui) && YAML::Load(ui.visible)["rows"].size() == 0 && !ui.offers("scroll_visible_list") && ui.offers(close),
        "empty painted list is an honest empty DTO with only visible close");
    list.addRow(3, "ROW", "1", "2"); draw();
    check(state.agentUiState(ui) && YAML::Load(ui.visible)["rows"].size() == 1 && !ui.offers("scroll_visible_list"),
        "short native list has no unpainted arrow authority");
    request.action = close;
    check(state.agentDispatch(request) && game.pops == 1 && game.top == nullptr, "exact ordinary close body pops one state");
    check(!state.agentDispatch(request) && game.pops == 1, "closed table cannot replay direct dispatch");
    std::cout << master << ' ' << screen << " meaningful_initial_rows=" << initial.rows.size()
        << " native_pitch=8" << " PASS\n";
    (void)background;
}
}
int main(int argc, char **argv)
{
    check(argc == 2, "source root provided for public stock translations");
    check(SDL_Init(0) == 0 && SDL_GetVideoSurface() == nullptr, "offscreen SDL only");
    Options::baseXResolution = 320; Options::baseYResolution = 200;
    Font big, small;
    for (const auto &font : YAML::LoadFile("Language/Font.dat")["fonts"])
    { if (font["id"].as<std::string>() == "FONT_BIG") big.load(font); if (font["id"].as<std::string>() == "FONT_SMALL") small.load(font); }
    check(small.getHeight() == 9 && small.getSpacing() == -1, "actual stock nine-pixel font with eight-pixel list pitch");
    SDL_Color palette[256]; for (int i = 0; i < 256; ++i) palette[i] = {Uint8(i), Uint8(i), Uint8(i), 0};
    for (const std::string master : {"xcom1", "xcom2"})
    {
        Language language; language.loadFile(std::string(argv[1]) + "/bin/standard/" + master + "/Language/en-US.yml");
        for (const bool stores : {true, false})
        {
            Surface canvas(320, 200), background(320, 200);
            TextButton ok(stores ? 300 : 288, 16, stores ? 10 : 16, stores ? 176 : 166);
            Text title(stores ? 310 : 278, 17, stores ? 5 : 21, stores ? 8 : 18);
            Text item(stores ? 142 : 114, 9, stores ? 10 : 16, stores ? 32 : 34);
            Text quantity(stores ? 88 : 54, 9, 152, stores ? 32 : 34);
            Text last(stores ? 74 : 112, 9, stores ? 240 : 212, stores ? 32 : 34);
            TextList list(stores ? 288 : 273, stores ? 128 : 112, stores ? 8 : 14, stores ? 40 : 50);
            const std::vector<Surface *> surfaces = {&background, &ok, &title, &item, &quantity, &last, &list};
            canvas.setPalette(palette); for (Surface *surface : surfaces) surface->setPalette(palette);
            background.drawRect(0, 0, 320, 200, 40);
            ok.initText(&big, &small, &language); ok.setText(language.getString("STR_OK"));
            for (Text *text : {&title, &item, &quantity, &last}) { text->initText(&big, &small, &language); text->setColor(80); }
            title.setBig(); title.setAlign(ALIGN_CENTER); title.setText(language.getString(stores ? "STR_STORES" : "STR_TRANSFERS"));
            item.setText(language.getString("STR_ITEM")); quantity.setText(language.getString("STR_QUANTITY_UC"));
            last.setText(language.getString(stores ? "STR_SPACE_USED_UC" : "STR_ARRIVAL_TIME_HOURS"));
            list.initText(&big, &small, &language); list.setColor(80);
            if (stores) list.setColumns(3, 162, 92, 32); else list.setColumns(3, 155, 75, 46);
            list.setSelectable(true); list.setBackground(&background); list.setMargin(2);
            const char *ufo[] = {"STR_RIFLE", "STR_RIFLE_CLIP", "STR_PISTOL", "STR_PISTOL_CLIP", "STR_GRENADE", "STR_SMOKE_GRENADE"};
            const char *tftd[] = {"STR_GAUSS_RIFLE", "STR_GAUSS_RIFLE_CLIP", "STR_GAUSS_PISTOL", "STR_GAUSS_PISTOL_CLIP", "STR_DYE_GRENADE", "STR_MAGNA_BLAST_GRENADE"};
            for (unsigned i = 0; i < 40; ++i)
            {
                const std::string name = language.getString(master == "xcom1" ? ufo[i % 6] : tftd[i % 6]);
                const std::string quantity = std::to_string(10 + i), value = stores ? "0.3" : "12";
                list.addRow(3, name.c_str(), quantity.c_str(), value.c_str());
            }
            BaseTables::Game game;
            if (stores) {
                BaseTables::StoresState state; state._game = &game; game.top = &state; state._surfaces = surfaces;
                state._btnOk = &ok; state._txtTitle = &title; state._txtItem = &item; state._txtQuantity = &quantity; state._txtSpaceUsed = &last; state._lstStores = &list;
                exercise(state, list, title, quantity, ok, background, canvas, game, "base_stores", "close_base_stores", master);
            } else {
                BaseTables::TransfersState state; state._game = &game; game.top = &state; state._surfaces = surfaces;
                state._btnOk = &ok; state._txtTitle = &title; state._txtItem = &item; state._txtQuantity = &quantity; state._txtArrivalTime = &last; state._lstTransfers = &list;
                exercise(state, list, title, quantity, ok, background, canvas, game, "base_transfers", "close_base_transfers", master);
            }
        }
    }
    check(agentPaintHealthy(), "all offscreen native paint succeeded");
    check(SDL_GetVideoSurface() == nullptr && !(SDL_WasInit(SDL_INIT_VIDEO)), "no game or GUI ran");
    agentObservePaint(-1);
    BaseTables::Game failedGame; BaseTables::StoresState failedStores; BaseTables::TransfersState failedTransfers;
    failedStores._game = &failedGame; failedTransfers._game = &failedGame; AgentUiState failedUi; AgentRequest failedRequest;
    failedGame.top = &failedStores; failedRequest.action = "close_base_stores";
    check(!failedStores.agentUiState(failedUi) && !failedStores.agentDispatch(failedRequest), "failed paint blocks Stores before fields or close");
    failedGame.top = &failedTransfers; failedRequest.action = "close_base_transfers";
    check(!failedTransfers.agentUiState(failedUi) && !failedTransfers.agentDispatch(failedRequest), "failed paint blocks Transfers before fields or close");
    std::cout << "AgentBaseTableProviderTest PASS " << checks << " checks\n";
}
