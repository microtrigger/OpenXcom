// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentUi.h"
#include <algorithm>
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
    std::string out = "\"state\":" + agentJsonString(screen) + ",\"visible\":" + visible + ",\"cursor\":" + cursor + ",\"actions\":[";
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
    if (targetLifetimes.size() != o.targetLifetimes.size()) return false;
    for (std::size_t i=0; i<targetLifetimes.size(); ++i)
        if (targetLifetimes[i].expired() || o.targetLifetimes[i].expired() ||
            targetLifetimes[i].owner_before(o.targetLifetimes[i]) ||
            o.targetLifetimes[i].owner_before(targetLifetimes[i])) return false;
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
    unsigned columns, bool selectable, AgentUiState &ui, AgentListProjection &projection,
    const std::function<bool(const AgentUiState &)> &regionAllowed, bool allowMultiColumnSelection, bool allowUnknownTextControls)
{
    projection = AgentListProjection();
    std::vector<AgentListRow> rows;
    if (!list || !owner || !agentWidgetVisible(list) ||
        !(allowUnknownTextControls ? (selectable && columns == 1 && list->agentControlRows(rows)) : list->agentRows(rows))) return false;
    ui.widgets.push_back(std::make_pair(list, list->agentListRevision()));
    projection.rows = "[";
    unsigned count = 0;
    for (const AgentListRow &row : rows)
    {
        if (count == 32) break;
        if (row.cells.size() != columns || (selectable &&
            ((columns != 1 && (!allowMultiColumnSelection || (columns != 2 && columns != 3 && columns != 5))) || ((!allowUnknownTextControls || row.completeText) && row.cells[0].empty())))) continue;
        std::vector<AgentUiRect> regions;
        if (allowUnknownTextControls) regions.push_back({row.bounds.x, row.bounds.y, row.bounds.w, row.bounds.h});
        else for (const SDL_Rect &r : row.regions) regions.push_back({r.x, r.y, r.w, r.h});
        if (!agentRegionsUncovered(regions, owner, paintOrder)) continue;
        AgentUiState candidate;
        candidate.regions = regions;
        if (regionAllowed && !regionAllowed(candidate)) continue;
        const std::size_t oldSize = ui.regions.size();
        ui.regions.insert(ui.regions.end(), regions.begin(), regions.end());
        if (!agentRegionsDisjoint(ui)) { ui.regions.resize(oldSize); continue; }
        bool completeText = row.completeText;
        if (allowUnknownTextControls && completeText)
        {
            AgentUiState label;
            for (const SDL_Rect &r : row.labelRegions) label.regions.push_back({r.x, r.y, r.w, r.h});
            completeText = agentRegionsUncovered(label.regions, owner, paintOrder) &&
                (!regionAllowed || regionAllowed(label));
        }
        if (!count && selectable && completeText) projection.firstLabel = row.cells[0];
        if (count) projection.rows += ",";
        projection.rows += "{\"handle\":\"r" + std::to_string(count++) + "\",\"cells\":[";
        for (unsigned i = 0; i < columns; ++i)
        {
            if (i) projection.rows += ",";
            projection.rows += allowUnknownTextControls && !completeText ? "null" : agentJsonString(row.cells[i]);
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
        if (!agentRegionsUncovered(footprint.regions, arrow, paintOrder) ||
            (regionAllowed && !regionAllowed(footprint))) return false;
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
bool agentProjectInterceptList(const TextList *list, const Surface *owner, const std::vector<Surface *> &paintOrder,
    AgentUiState &ui, AgentListProjection &projection,
    const std::function<bool(const AgentUiState &)> &regionAllowed)
{
    // Explicit four-column interception opt-in; legacy list authority is unchanged.
    const unsigned columns = 4;
    const bool selectable = true, allowUnknownTextControls = false, allowMultiColumnSelection = true;
    projection = AgentListProjection();
    std::vector<AgentListRow> rows;
    if (!list || !owner || !agentWidgetVisible(list) ||
        !(allowUnknownTextControls ? (selectable && columns == 1 && list->agentControlRows(rows)) : list->agentRows(rows))) return false;
    ui.widgets.push_back(std::make_pair(list, list->agentListRevision()));
    projection.rows = "[";
    unsigned count = 0;
    for (const AgentListRow &row : rows)
    {
        if (count == 32) break;
        if (row.cells.size() != columns || (selectable &&
            ((columns != 1 && (!allowMultiColumnSelection || columns != 4)) || ((!allowUnknownTextControls || row.completeText) && row.cells[0].empty())))) continue;
        std::vector<AgentUiRect> regions;
        if (allowUnknownTextControls) regions.push_back({row.bounds.x, row.bounds.y, row.bounds.w, row.bounds.h});
        else for (const SDL_Rect &r : row.regions) regions.push_back({r.x, r.y, r.w, r.h});
        if (!agentRegionsUncovered(regions, owner, paintOrder)) continue;
        AgentUiState candidate;
        candidate.regions = regions;
        if (regionAllowed && !regionAllowed(candidate)) continue;
        const std::size_t oldSize = ui.regions.size();
        ui.regions.insert(ui.regions.end(), regions.begin(), regions.end());
        if (!agentRegionsDisjoint(ui)) { ui.regions.resize(oldSize); continue; }
        bool completeText = row.completeText;
        if (allowUnknownTextControls && completeText)
        {
            AgentUiState label;
            for (const SDL_Rect &r : row.labelRegions) label.regions.push_back({r.x, r.y, r.w, r.h});
            completeText = agentRegionsUncovered(label.regions, owner, paintOrder) &&
                (!regionAllowed || regionAllowed(label));
        }
        if (!count && selectable && completeText) projection.firstLabel = row.cells[0];
        if (count) projection.rows += ",";
        projection.rows += "{\"handle\":\"r" + std::to_string(count++) + "\",\"cells\":[";
        for (unsigned i = 0; i < columns; ++i)
        {
            if (i) projection.rows += ",";
            projection.rows += allowUnknownTextControls && !completeText ? "null" : agentJsonString(row.cells[i]);
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
        if (!agentRegionsUncovered(footprint.regions, arrow, paintOrder) ||
            (regionAllowed && !regionAllowed(footprint))) return false;
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
bool agentProjectArrivalList(const TextList *list, const Surface *owner, const std::vector<Surface *> &paintOrder,
    AgentUiState &ui, AgentListProjection &projection,
    const std::function<bool(const AgentUiState &)> &regionAllowed,
    const std::function<bool(const AgentUiState &)> &textAllowed)
{
    projection = AgentListProjection();
    std::vector<AgentArrivalRow> rows;
    std::uint64_t captureEpoch = 0;
    if (!list || !owner || !agentWidgetVisible(list) ||
        !list->agentArrivalLogicalRows(rows, captureEpoch) || rows.size() > 32) return false;
    ui.widgets.emplace_back(list, list->agentListRevision());
    ui.widgets.emplace_back(list, list->agentPresentationRevision());
    // Private authority only. Preserve this prefix when the provider adds its
    // two scroll flags; never serialize capture epochs or allocation geometry.
    ui.context.insert(ui.context.end(), {captureEpoch,
        static_cast<std::uint64_t>(list->getX()), static_cast<std::uint64_t>(list->getY()),
        static_cast<std::uint64_t>(list->getWidth()), static_cast<std::uint64_t>(list->getHeight())});
    projection.rows = "[";
    unsigned count = 0;
    int lastY = -1;
    for (const AgentArrivalRow &row : rows)
    {
        if (row.bounds.y <= lastY) return false;
        lastY = row.bounds.y;
        bool meaningful = false;
        std::string cells = "[";
        for (unsigned column = 0; column < 3; ++column)
        {
            if (column) cells += ",";
            const AgentPurchaseCell &cell = row.cells[column];
            if (cell.regions.size() > 4) return false;
            bool known = cell.known && cell.text.size() <= 1024 && !cell.regions.empty();
            AgentUiState part;
            for (const SDL_Rect &r : cell.regions)
            {
                if (r.w == 0 || r.h == 0 || r.x < 0 || r.y < 0 ||
                    r.x + r.w > 640 || r.y + r.h > 400) return false;
                part.regions.push_back({r.x, r.y, r.w, r.h});
            }
            bool found = false;
            for (const Surface *later : paintOrder)
            {
                if (later == owner) { found = true; continue; }
                if (!found || !later || !later->getVisible()) continue;
                for (const AgentUiRect &r : part.regions)
                    if (r.x < later->getX() + later->getWidth() && r.x + r.width > later->getX() &&
                        r.y < later->getY() + later->getHeight() && r.y + r.height > later->getY()) known = false;
            }
            known = known && found && (!textAllowed || textAllowed(part));
            // Observation-only strips are not action regions: cosmetic cursor
            // must not reintroduce a text readability gate at Game admission.
            cells += known ? agentJsonString(cell.text) : "null";
            meaningful = meaningful || (known && !cell.text.empty());
        }
        cells += "]";
        if (!meaningful) continue;
        if (count) projection.rows += ",";
        projection.rows += "{\"handle\":\"r" + std::to_string(count++) + "\",\"cells\":" + cells + ",\"selected\":false}";
        ui.rows.push_back(row.logicalIndex);
    }
    projection.rows += "]";
    const Surface *up = nullptr, *down = nullptr;
    list->agentScrollControls(up, down);
    const auto includeArrow = [&](const Surface *arrow) -> bool {
        if (!agentWidgetVisible(arrow)) return false;
        AgentUiState part; agentIncludeWidget(part, arrow);
        if (!agentRegionsUncovered(part.regions, arrow, paintOrder) ||
            (regionAllowed && !regionAllowed(part))) return false;
        const auto before = ui.regions.size();
        ui.regions.insert(ui.regions.end(), part.regions.begin(), part.regions.end());
        if (agentRegionsDisjoint(ui)) return true;
        ui.regions.resize(before); return false;
    };
    projection.up = includeArrow(up); projection.down = includeArrow(down);
    projection.scroll = std::string("{\"up\":") + (projection.up ? "true" : "false") +
        ",\"down\":" + (projection.down ? "true" : "false") + "}";
    return true;
}
bool agentProjectOutfittingList(const TextList *list, const std::vector<Surface *> &paintOrder,
    unsigned columns, bool equipment, AgentUiState &ui, AgentListProjection &projection,
    const std::function<bool(const AgentUiState &)> &allowed)
{
    projection = AgentListProjection();
    std::vector<AgentOutfittingRow> rows;
    if (!list || !list->agentOutfittingRows(rows)) return false;
    ui.widgets.emplace_back(list, list->agentListRevision());
    const size_t baseRegions = ui.regions.size();
    std::vector<AgentUiRect> local;
    size_t work = 0;
    bool healthy = true;
    const auto include = [&](const std::vector<AgentUiRect> &parts, const Surface *owner) -> bool {
        if (parts.empty() || !agentRegionsUncovered(parts, owner, paintOrder)) return false;
        AgentUiState candidate; candidate.regions = parts;
        if (allowed && !allowed(candidate)) return false;
        std::vector<AgentUiRect> united = local;
        for (const AgentUiRect &r : parts)
        {
            if (!agentRegionVisible(r)) return false;
            for (int y = r.y; y < r.y+r.height; ++y)
            {
                if (++work > 2*1024*1024 || united.size() >= 16384) { healthy = false; return false; }
                united.push_back({r.x,y,r.width,1});
            }
        }
        std::sort(united.begin(), united.end(), [](const AgentUiRect &a,const AgentUiRect &b) {
            return a.y != b.y ? a.y < b.y : a.x < b.x;
        });
        std::vector<AgentUiRect> merged;
        for (const AgentUiRect &r : united)
        {
            if (++work > 2*1024*1024) { healthy = false; return false; }
            if (!merged.empty() && merged.back().y == r.y && merged.back().x+merged.back().width >= r.x)
                merged.back().width = std::max(merged.back().x+merged.back().width,r.x+r.width)-merged.back().x;
            else merged.push_back(r);
        }
        AgentUiState test;
        test.regions.assign(ui.regions.begin(),ui.regions.begin()+baseRegions);
        test.regions.insert(test.regions.end(),merged.begin(),merged.end());
        if (!agentRegionsDisjoint(test)) return false;
        local.swap(merged); ui.regions.swap(test.regions); return true;
    };
    unsigned count = 0;
    int lastBottom = -1;
    projection.rows = "[";
    for (const AgentOutfittingRow &row : rows)
    {
        if (count == 32) break;
        if (row.cells.size() != columns || row.bounds.y < lastBottom) return false;
        lastBottom = row.bounds.y+row.bounds.h;
        const auto oldLocal = local;
        const auto oldRegions = ui.regions;
        bool increase = false, decrease = false;
        const auto arrow = [&](const Surface *part) {
            if (!part) return false;
            AgentUiState mark; agentIncludeWidget(mark,part);
            return include(mark.regions,part);
        };
        if (equipment)
        {
            increase = arrow(row.increase); decrease = arrow(row.decrease);
            if (!increase && !decrease) { local=oldLocal;ui.regions=oldRegions;continue; }
        }
        else if (!include({{row.clickBand.x,row.clickBand.y,row.clickBand.w,row.clickBand.h}},list)) continue;
        std::string cells = "[";
        for (unsigned c=0;c<columns;++c)
        {
            if (c) cells += ",";
            const AgentPurchaseCell &cell=row.cells[c];
            std::vector<AgentUiRect> ink;
            for (const SDL_Rect &r:cell.regions) ink.push_back({r.x,r.y,r.w,r.h});
            const bool known=cell.known && cell.text.size()<=1024 && (ink.empty() || include(ink,list));
            if (!healthy) return false;
            cells += known ? agentJsonString(cell.text) : "null";
        }
        cells += "]";
        bool selected=row.selected;
        if (selected)
        {
            const std::vector<AgentUiRect> band={{row.bounds.x,row.bounds.y,row.bounds.w,row.bounds.h}};
            AgentUiState mark;mark.regions=band;
            selected=agentRegionsUncovered(band,list,paintOrder) && (!allowed || allowed(mark));
        }
        if (count) projection.rows += ",";
        projection.rows += "{\"handle\":\"r"+std::to_string(count++)+"\",\"cells\":"+cells+
            ",\"selected\":"+(selected?"true":"false");
        if (equipment)
        {
            projection.rows += ",\"controls\":[";
            if (increase) projection.rows += "\"increase\"";
            if (decrease) projection.rows += increase?",\"decrease\"":"\"decrease\"";
            projection.rows += "]";
            ui.context.push_back((increase?1:0)|(decrease?2:0));
        }
        projection.rows += "}"; ui.rows.push_back(row.logicalIndex);
    }
    projection.rows += "]";
    const Surface *up=nullptr,*down=nullptr;list->agentScrollControls(up,down);
    const auto arrow=[&](const Surface *part) {
        if (!part) return false;
        AgentUiState mark;agentIncludeWidget(mark,part);return include(mark.regions,part);
    };
    projection.up=arrow(up);projection.down=arrow(down);
    projection.scroll=std::string("{\"up\":")+(projection.up?"true":"false")+",\"down\":"+(projection.down?"true":"false")+"}";
    return healthy;
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
    // Same strict rectangle predicate, y-swept to keep opaque ink runs bounded.
    // These are private presentation regions, never public pixel data.
    if (ui.regions.size() > 16384) return false;
    std::vector<AgentUiRect> sorted = ui.regions, active;
    std::sort(sorted.begin(), sorted.end(), [](const AgentUiRect &a, const AgentUiRect &b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    size_t work = 0;
    for (const AgentUiRect &a : sorted)
    {
        active.erase(std::remove_if(active.begin(), active.end(), [&](const AgentUiRect &b) {
            return b.y + b.height <= a.y;
        }), active.end());
        for (const AgentUiRect &b : active)
        {
            if (++work > 2 * 1024 * 1024) return false;
            if (a.x < b.x + b.width && a.x + a.width > b.x &&
                a.y < b.y + b.height && a.y + a.height > b.y) return false;
        }
        active.push_back(a);
    }
    return true;
}

std::string agentProjectNamedButton(const TextButton *button,const Surface *destination,
    const std::vector<Surface *> &paintOrder,AgentUiState &ui,
    const std::function<bool(const AgentUiState &)> &regionAllowed)
{
    std::string label;SDL_Rect ink,point;
    if(!button || !button->agentNamedControl(destination,label,ink,point))return {};
    AgentUiState part;part.regions.push_back({ink.x,ink.y,ink.w,ink.h});
    if(point.x<ink.x || point.x>=ink.x+ink.w || point.y<ink.y || point.y>=ink.y+ink.h)
        part.regions.push_back({point.x,point.y,1,1});
    if(!agentRegionsUncovered(part.regions,button,paintOrder) || (regionAllowed&&!regionAllowed(part)))return {};
    const auto old=ui.regions.size();ui.regions.insert(ui.regions.end(),part.regions.begin(),part.regions.end());
    if(!agentRegionsDisjoint(ui)){ui.regions.resize(old);return {};}
    ui.points.emplace_back(point.x,point.y);ui.widgets.emplace_back(button,button->agentPresentationRevision());
    ui.widgets.emplace_back(button,button->agentLabelRevision());return label;
}
std::string agentButtonLabel(const TextButton *widget)
{
    return agentWidgetVisible(widget) ? widget->agentLabel() : std::string();
}
} }
