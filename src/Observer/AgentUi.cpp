// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentUi.h"
#include "../Engine/Surface.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/NumberText.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/ComboBox.h"
namespace OpenXcom { namespace Observer {
namespace { thread_local bool paintHealthy = true; }
int agentObservePaint(int result) { if (result < 0) paintHealthy = false; return result; }
bool agentPaintHealthy() { return paintHealthy; }
std::string AgentUiState::payload() const
{
    std::string out = "\"state\":" + agentJsonString(screen) + ",\"visible\":" + visible + ",\"actions\":[";
    for (std::size_t i = 0; i < actions.size(); ++i)
    {
        if (i) out += ",";
        out += "{\"id\":" + agentJsonString(actions[i].id) + ",\"label\":" + agentJsonString(actions[i].label) +
            ",\"label_source\":" + agentJsonString(actions[i].labelSource) + ",\"argument_kind\":" + agentJsonString(actions[i].argumentKind) + "}";
    }
    return out + "]";
}
std::string AgentUiState::authorityPayload() const
{
    if (controlVisible.empty()) return payload();
    AgentUiState controls = *this;
    controls.visible = controlVisible;
    return controls.payload();
}
bool AgentUiState::sameBinding(const AgentUiState &o) const
{
    return actor == o.actor && save == o.save && actorRevision == o.actorRevision &&
        selectionRevision == o.selectionRevision && uiRevision == o.uiRevision && widgets == o.widgets &&
        rows == o.rows && points == o.points && context == o.context && regions == o.regions && authorityPayload() == o.authorityPayload();
}
bool AgentUiState::offers(const std::string &id) const
{
    for (const auto &action : actions) if (action.id == id) return true;
    return false;
}
bool agentProjectList(const TextList *list, const Surface *owner, const std::vector<Surface *> &paintOrder,
    unsigned columns, bool selectable, AgentUiState &ui, AgentListProjection &projection)
{
    projection = AgentListProjection();
    std::vector<AgentListRow> rows;
    if (!list || !owner || !agentWidgetVisible(list) || !list->agentRows(rows)) return false;
    ui.widgets.push_back(std::make_pair(list, list->agentListRevision()));
    projection.rows = "[";
    unsigned count = 0;
    for (const AgentListRow &row : rows)
    {
        if (count == 32) break;
        if (row.cells.size() != columns || (selectable && (columns != 1 || row.cells[0].empty()))) continue;
        std::vector<AgentUiRect> regions;
        for (const SDL_Rect &r : row.regions) regions.push_back({r.x, r.y, r.w, r.h});
        if (!agentRegionsUncovered(regions, owner, paintOrder)) continue;
        const std::size_t oldSize = ui.regions.size();
        ui.regions.insert(ui.regions.end(), regions.begin(), regions.end());
        if (!agentRegionsDisjoint(ui)) { ui.regions.resize(oldSize); continue; }
        if (count) projection.rows += ",";
        projection.rows += "{\"handle\":\"r" + std::to_string(count++) + "\",\"cells\":[";
        for (unsigned i = 0; i < columns; ++i)
        {
            if (i) projection.rows += ",";
            projection.rows += agentJsonString(row.cells[i]);
        }
        projection.rows += std::string("],\"selected\":") + (selectable && row.selected ? "true" : "false") + "}";
        ui.rows.push_back(row.logicalIndex);
    }
    projection.rows += "]";
    const Surface *up = nullptr, *down = nullptr;
    list->agentScrollControls(up, down);
    const auto includeArrow = [&](const Surface *arrow) -> bool
    {
        if (!agentWidgetVisible(arrow)) return false;
        AgentUiState footprint;
        agentIncludeWidget(footprint, arrow);
        if (!agentRegionsUncovered(footprint.regions, arrow, paintOrder)) return false;
        const std::size_t oldSize = ui.regions.size();
        ui.regions.insert(ui.regions.end(), footprint.regions.begin(), footprint.regions.end());
        if (!agentRegionsDisjoint(ui)) { ui.regions.resize(oldSize); return false; }
        return true;
    };
    projection.up = includeArrow(up); projection.down = includeArrow(down);
    projection.scroll = std::string("{\"up\":") + (projection.up ? "true" : "false") +
        ",\"down\":" + (projection.down ? "true" : "false") + "}";
    return true;
}
bool agentWidgetVisible(const Surface *widget)
{
    return widget && widget->agentContextVisible() && widget->getX() >= 0 && widget->getY() >= 0 &&
        widget->getWidth() > 0 && widget->getHeight() > 0 &&
        widget->getX() <= Options::baseXResolution - widget->getWidth() &&
        widget->getY() <= Options::baseYResolution - widget->getHeight();
}
std::string agentTextJson(const Text *widget)
{
    // Do not disclose an underlying full string if the drawn widget clips it.
    if (!agentWidgetVisible(widget) || !widget->getFont() || !widget->agentTextComplete() || widget->getTextWidth() > widget->getWidth() ||
        widget->getTextHeight() > widget->getHeight() || widget->getText().size() > 1024) return "null";
    return agentJsonString(widget->getText());
}
std::string agentCompositeTextJson(const Text *text, int width, int height, const std::vector<Surface *> &paintOrder)
{
    if (!agentWidgetUncovered(text, paintOrder)) return "null";
    const std::string value = agentTextJson(text);
    SDL_Rect ink;
    if (value == "null" || (text->agentPaintedBounds(ink) &&
        (ink.x < 0 || ink.y < 0 || ink.x + ink.w > width || ink.y + ink.h > height))) return "null";
    return value;
}
void agentIncludeWidget(AgentUiState &ui, const Surface *widget)
{
    if (agentWidgetVisible(widget))
    {
        AgentUiRect rect = {widget->getX(), widget->getY(), widget->getWidth(), widget->getHeight()};
        if (const Text *text = dynamic_cast<const Text *>(widget))
        {
            SDL_Rect ink;
            if (!text->agentPaintedBounds(ink)) return;
            rect.x = ink.x; rect.y = ink.y; rect.width = ink.w; rect.height = ink.h;
        }
        ui.regions.push_back(rect);
    }
}
bool agentWidgetUncovered(const Surface *widget, const std::vector<Surface *> &paintOrder)
{
    if (!agentWidgetVisible(widget)) return false;
    AgentUiState footprint;
    agentIncludeWidget(footprint, widget);
    return agentRegionsUncovered(footprint.regions, widget, paintOrder);
}
std::vector<Surface *> agentFlattenPaintOrder(const std::vector<Surface *> &paintOrder)
{
    std::vector<Surface *> flattened;
    for (Surface *surface : paintOrder)
    {
        if (ComboBox *combo = dynamic_cast<ComboBox *>(surface)) combo->agentPaintOrder(flattened);
        else if (TextList *list = dynamic_cast<TextList *>(surface)) list->agentPaintOrder(flattened);
        else flattened.push_back(surface);
    }
    return flattened;
}
bool agentRegionVisible(const AgentUiRect &r)
{
    return r.x >= 0 && r.y >= 0 && r.width > 0 && r.height > 0 &&
        r.x <= Options::baseXResolution - r.width && r.y <= Options::baseYResolution - r.height;
}
bool agentRegionsUncovered(const std::vector<AgentUiRect> &regions, const Surface *owner,
    const std::vector<Surface *> &paintOrder)
{
    for (const AgentUiRect &r : regions) if (!agentRegionVisible(r)) return false;
    bool found = false;
    for (const Surface *later : paintOrder)
    {
        if (later == owner) { found = true; continue; }
        if (!found || !later || !later->getVisible()) continue;
        AgentUiRect cover = {later->getX(), later->getY(), later->getWidth(), later->getHeight()};
        // Complete, already drawn text can use its actual glyph bounds rather
        // than its allocation rectangle. Unknown/dirty surfaces retain the
        // conservative allocation bounds; never inspect their contents.
        if (const Text *text = dynamic_cast<const Text *>(later))
        {
            if (text->agentContextVisible())
            {
                SDL_Rect ink;
                if (!text->agentPaintedBounds(ink)) continue;
                cover = {ink.x, ink.y, ink.w, ink.h};
            }
        }
        if (const NumberText *number = dynamic_cast<const NumberText *>(later))
        {
            SDL_Rect ink;
            if (number->agentPaintedBounds(ink)) cover = {ink.x, ink.y, ink.w, ink.h};
        }
        for (const AgentUiRect &part : regions)
            if (part.x < cover.x + cover.width && part.x + part.width > cover.x &&
                part.y < cover.y + cover.height && part.y + part.height > cover.y) return false;
    }
    return found;
}
bool agentRegionsDisjoint(const AgentUiState &ui)
{
    for (std::size_t i = 0; i < ui.regions.size(); ++i)
        for (std::size_t j = i + 1; j < ui.regions.size(); ++j)
        {
            const AgentUiRect &a = ui.regions[i], &b = ui.regions[j];
            if (a.x < b.x + b.width && a.x + a.width > b.x &&
                a.y < b.y + b.height && a.y + a.height > b.y) return false;
        }
    return true;
}
std::string agentButtonLabel(const TextButton *widget)
{
    return agentWidgetVisible(widget) ? widget->agentLabel() : std::string();
}
} }
