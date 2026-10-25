// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AgentProtocol.h"
#include "AgentProjectileWitness.h"
#include <vector>
#include <array>
#include <utility>
#include <functional>
#include <memory>
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
    // Diagnostic-only copy of the already-public, point-paired map positions.
    std::vector<std::array<int,3>> mapTargetPositions;
    std::string screen = "unsupported", visible = "{}";
    // Public logical-canvas cursor only; no desktop coordinates or world state.
    std::string cursor = "null";
    // Already-admitted public viewport, copied only after successful presentation.
    bool publicCameraKnown = false;
    AgentUiRect publicCamera = {0,0,0,0};
    std::uint64_t publicCameraRevision = 0;
    // Read-only tactical presentation is not an action target in /5.
    // A provider may bind control authority to its separate visible controls.
    std::string controlVisible;
    // Exact complete displayed report text; no cause, translation key or actor.
    // Empty means unknown/not a report, never proof that an old report ended.
    std::string displayedMessage;
    bool displayedMessageAbsent = false;
    // Transient final-frame evidence; excluded from payload and action binding.
    AgentProjectileFact projectileFact = AgentProjectileFact::Unknown;
    AgentProjectileFact impactFact = AgentProjectileFact::Unknown;
    AgentProjectileFact collapseFact = AgentProjectileFact::Unknown;
    AgentProjectileFact thrownItemFact = AgentProjectileFact::Unknown;
    AgentProjectileFact largeExplosionFact = AgentProjectileFact::Unknown;
    std::vector<AgentUiAction> actions;
    // Visible projection regions: private geometry binding and overlay checks.
    std::vector<AgentUiRect> regions;
    // Private stale-context bindings. Never part of the protocol payload.
    const void *actor = 0, *save = 0;
    // Widget pointers and generations bind local row/control slots, never serialized.
    std::vector<std::pair<const void *, std::uint64_t> > widgets;
    std::vector<std::size_t> rows;
    std::vector<std::pair<int, int> > points;
    // Private lifetime identities for already painted targets; never serialized.
    std::vector<std::weak_ptr<const void> > targetLifetimes;
    std::vector<std::uint64_t> context; // private gameplay freshness only
    std::uint64_t actorRevision = 0, selectionRevision = 0, uiRevision = 0;
    std::string payload() const;
    std::string authorityPayload() const;
    bool sameBinding(const AgentUiState &other) const;
    bool offers(const std::string &id) const;
};
struct AgentListProjection {
    std::string firstLabel;
    std::string rows = "[]", scroll = "{\"up\":false,\"down\":false}";
    bool up = false, down = false;
};
// Called only by explicitly supported state providers, never generic discovery.
// An optional filter can only omit each candidate row/arrow region; it never
// replaces native paint, clipping, sibling coverage or final disjoint guards.
bool agentProjectList(const TextList *list, const Surface *owner, const std::vector<Surface *> &paintOrder,
    unsigned columns, bool selectable, AgentUiState &ui, AgentListProjection &projection,
    const std::function<bool(const AgentUiState &)> &regionAllowed = std::function<bool(const AgentUiState &)>(),
    bool allowMultiColumnSelection = false, bool allowUnknownTextControls = false);
// Target-bound interception-only four-column selectable list, no unknown labels.
bool agentProjectInterceptList(const TextList *list, const Surface *owner, const std::vector<Surface *> &paintOrder,
    AgentUiState &ui, AgentListProjection &projection,
    const std::function<bool(const AgentUiState &)> &regionAllowed);
// Explicit arrivals-only nullable three-cell projection; legacy lists unchanged.
bool agentProjectArrivalList(const TextList *list, const Surface *owner, const std::vector<Surface *> &paintOrder,
    AgentUiState &ui, AgentListProjection &projection,
    const std::function<bool(const AgentUiState &)> &regionAllowed,
    const std::function<bool(const AgentUiState &)> &textAllowed);
bool agentProjectOutfittingList(const TextList *list, const std::vector<Surface *> &paintOrder,
    unsigned columns, bool equipment, AgentUiState &ui, AgentListProjection &projection,
    const std::function<bool(const AgentUiState &)> &regionAllowed);
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
std::string agentCompositeTextJson(const Text *text, int width, int height, const std::vector<Surface *> &paintOrder);
std::string agentButtonLabel(const TextButton *widget);
// Explicit named TextButton opt-in; never used for icons or anonymous controls.
std::string agentProjectNamedButton(const TextButton *button,const Surface *destination,
    const std::vector<Surface *> &paintOrder,AgentUiState &ui,
    const std::function<bool(const AgentUiState &)> &regionAllowed);
} }
