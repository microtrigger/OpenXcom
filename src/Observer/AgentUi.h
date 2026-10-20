// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentProtocol.h"
#include <vector>
#include <utility>
namespace OpenXcom {
class Surface; class Text; class TextButton; class TextList;
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
    // Widget pointers and generations bind local row/control slots, never serialized.
    std::vector<std::pair<const void *, std::uint64_t> > widgets;
    std::vector<std::size_t> rows;
    std::uint64_t actorRevision = 0, selectionRevision = 0, uiRevision = 0;
    std::string payload() const;
    bool sameBinding(const AgentUiState &other) const;
    bool offers(const std::string &id) const;
};
struct AgentListProjection {
    std::string rows = "[]", scroll = "{\"up\":false,\"down\":false}";
    bool up = false, down = false;
};
// Called only by explicitly supported state providers, never generic discovery.
bool agentProjectList(const TextList *list, const Surface *owner, const std::vector<Surface *> &paintOrder,
    unsigned columns, bool selectable, AgentUiState &ui, AgentListProjection &projection);
bool agentWidgetVisible(const Surface *widget);
// Conservative check against later-painted surfaces in this explicit state.
// The paint order/geometry is used only for visibility, never serialized.
bool agentWidgetUncovered(const Surface *widget, const std::vector<Surface *> &paintOrder);
// Expands only the supported composites in their actual blit order.
std::vector<Surface *> agentFlattenPaintOrder(const std::vector<Surface *> &paintOrder);
bool agentRegionsUncovered(const std::vector<AgentUiRect> &regions, const Surface *owner,
    const std::vector<Surface *> &paintOrder);
bool agentRegionVisible(const AgentUiRect &region);
void agentIncludeWidget(AgentUiState &ui, const Surface *widget);
bool agentRegionsDisjoint(const AgentUiState &ui);
int agentObservePaint(int result);
bool agentPaintHealthy();
std::string agentTextJson(const Text *widget);
std::string agentButtonLabel(const TextButton *widget);
} }
