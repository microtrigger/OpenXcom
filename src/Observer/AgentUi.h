// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentProtocol.h"
#include <vector>
namespace OpenXcom {
class Surface; class Text; class TextButton;
namespace Observer {
struct AgentUiAction {
    std::string id, label, labelSource, argumentKind;
    AgentUiAction(const std::string &i, const std::string &l, const std::string &s, const std::string &a = "none")
        : id(i), label(l), labelSource(s), argumentKind(a) {}
};
struct AgentUiRect {
    int x, y, width, height;
    bool operator==(const AgentUiRect &o) const
    { return x == o.x && y == o.y && width == o.width && height == o.height; }
};
struct AgentUiState {
    std::string screen = "unsupported", visible = "{}";
    std::vector<AgentUiAction> actions;
    // Visible projection regions: private geometry binding and overlay checks.
    std::vector<AgentUiRect> regions;
    // Private stale-context bindings. Never part of the protocol payload.
    const void *actor = 0, *save = 0;
    std::uint64_t actorRevision = 0, selectionRevision = 0, uiRevision = 0;
    std::string payload() const;
    bool sameBinding(const AgentUiState &other) const;
    bool offers(const std::string &id) const;
};
bool agentWidgetVisible(const Surface *widget);
// Conservative check against later-painted surfaces in this explicit state.
// The paint order/geometry is used only for visibility, never serialized.
bool agentWidgetUncovered(const Surface *widget, const std::vector<Surface *> &paintOrder);
void agentIncludeWidget(AgentUiState &ui, const Surface *widget);
bool agentRegionsDisjoint(const AgentUiState &ui);
int agentObservePaint(int result);
bool agentPaintHealthy();
std::string agentTextJson(const Text *widget);
std::string agentButtonLabel(const TextButton *widget);
} }
