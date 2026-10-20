// SPDX-License-Identifier: GPL-3.0-or-later
// Pure DTO/binding tests. This is not GUI/presentation integration evidence.
#include "../../src/Observer/AgentUi.h"
#include <cstdlib>
#include <iostream>
#include <thread>
using namespace OpenXcom::Observer;
namespace {
unsigned checks = 0;
void check(bool value, const char *message)
{
    ++checks;
    if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
}
int main()
{
    AgentUiState empty;
    check(empty.screen == "unsupported" && empty.actions.empty(), "unsupported default has no actions");
    check(empty.payload() == "\"state\":\"unsupported\",\"visible\":{},\"actions\":[]", "bounded unsupported projection");
    int actorA = 0, actorB = 0, saveA = 0, saveB = 0;
    AgentUiState original;
    original.screen = "battlescape";
    original.visible = "{\"selected_name\":\"Same displayed name\",\"kneel_indicator\":\"standing\"}";
    original.actions.push_back(AgentUiAction("kneel", "Kneel", "visible_control_semantics"));
    original.actor = &actorA; original.save = &saveA;
    original.actorRevision = 900123; original.selectionRevision = 900456; original.uiRevision = 900789;
    AgentUiState other = original;
    check(original.sameBinding(other), "identical live context");
    check(original.offers("kneel") && !original.offers("stand") && !original.offers("toggle_kneel"), "desired stance only");
    other.actor = &actorB;
    check(!original.sameBinding(other), "same displayed name cannot replace bound actor");
    check(original.payload() == other.payload(), "actor identity never serialized");
    other = original; other.save = &saveB;
    check(!original.sameBinding(other) && original.payload() == other.payload(), "save binding private");
    other = original; ++other.selectionRevision;
    check(!original.sameBinding(other) && original.payload() == other.payload(), "away-and-back selection private");
    other = original; ++other.actorRevision;
    check(!original.sameBinding(other) && original.payload() == other.payload(), "actor revision private");
    other = original; ++other.uiRevision;
    check(!original.sameBinding(other) && original.payload() == other.payload(), "UI ABA generation private");
    // Model the ordinary init sequence: paint HUD, then reapply reserve mode.
    // Reserve mode increments a private generation without changing these
    // displayed fields. A fresh grant samples the current generation; it does
    // not refresh hidden values into the public HUD. This tests DTO matching,
    // not execution of BattlescapeState::init or a live presentation.
    AgentUiState paintedBeforeReserve = original;
    AgentUiState currentAfterReserve = paintedBeforeReserve;
    ++currentAfterReserve.selectionRevision;
    AgentUiState freshGrant = currentAfterReserve;
    check(freshGrant.sameBinding(currentAfterReserve), "fresh grant after init reserve write is current");
    check(!paintedBeforeReserve.sameBinding(currentAfterReserve), "pre-reserve grant remains stale");
    check(freshGrant.payload() == paintedBeforeReserve.payload(), "reserve generation does not refresh or reveal HUD values");
    ++currentAfterReserve.selectionRevision;
    check(!freshGrant.sameBinding(currentAfterReserve), "post-grant reserve write invalidates fresh grant");
    currentAfterReserve = freshGrant;
    currentAfterReserve.selectionRevision += 2;
    check(currentAfterReserve.actor == freshGrant.actor && !freshGrant.sameBinding(currentAfterReserve),
        "away-and-back selected actor cannot restore fresh grant");
    other = original; other.visible = "{\"selected_name\":null,\"kneel_indicator\":\"standing\"}";
    check(!original.sameBinding(other), "visible text change invalidates");
    other = original; other.actions.clear();
    check(!original.sameBinding(other) && !other.offers("kneel"), "hidden control not offered");
    other = original; other.widgets.push_back(std::make_pair(&actorA, 1234));
    check(!original.sameBinding(other) && original.payload() == other.payload(), "list widget bindings private");
    AgentUiState listGrant = other;
    ++other.widgets[0].second;
    check(!listGrant.sameBinding(other) && listGrant.payload() == other.payload(), "list scroll/content ABA invalidates");
    other = listGrant; other.rows.push_back(123);
    check(!listGrant.sameBinding(other) && listGrant.payload() == other.payload(), "local slot row identity private");
    listGrant = other; other.rows[0] = 124;
    check(!listGrant.sameBinding(other), "same label replacement row invalidates slot");
    const std::string payload = original.payload();
    check(payload.find("900123") == std::string::npos && payload.find("900456") == std::string::npos &&
        payload.find("900789") == std::string::npos && payload.find("actor") == std::string::npos &&
        payload.find("save") == std::string::npos && payload.find("Revision") == std::string::npos,
        "public payload omits all private identifiers/revisions");
    check(payload.find("\"label_source\":\"visible_control_semantics\"") != std::string::npos, "truthful icon label provenance");
    AgentUiState geometry = original;
    AgentUiRect first = {10, 10, 20, 10}, touching = {30, 10, 10, 10}, overlap = {29, 10, 10, 10};
    geometry.regions.push_back(first);
    geometry.regions.push_back(touching);
    check(agentRegionsDisjoint(geometry), "touching nonoverlapping regions allowed");
    check(geometry.payload() == original.payload(), "private geometry never serialized");
    check(!geometry.sameBinding(original), "geometry change invalidates binding");
    geometry.regions[1] = overlap;
    check(!agentRegionsDisjoint(geometry), "later painted semantic region overlap rejected");
    geometry.regions[1].y = 20;
    check(agentRegionsDisjoint(geometry), "vertical touching boundary allowed");
    check(agentPaintHealthy(), "owner paint starts healthy");
    check(agentObservePaint(0) == 0 && agentPaintHealthy(), "normal blit return preserved");
    bool workerHealthy = true;
    std::thread worker([&workerHealthy]() { agentObservePaint(-1); workerHealthy = agentPaintHealthy(); });
    worker.join();
    check(!workerHealthy && agentPaintHealthy(), "loader thread cannot race owner paint latch");
    check(agentObservePaint(-1) == -1 && !agentPaintHealthy(), "failed constituent blit poisons observation");
    check(agentObservePaint(0) == 0 && !agentPaintHealthy(), "later successful blit cannot revive stale cached paint");
    std::cout << "AgentUiBindingTest PASS " << checks << " checks\n";
}
