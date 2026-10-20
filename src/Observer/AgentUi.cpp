// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentUi.h"
#include "../Engine/Surface.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
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
bool AgentUiState::sameBinding(const AgentUiState &o) const
{
    return actor == o.actor && save == o.save && actorRevision == o.actorRevision &&
        selectionRevision == o.selectionRevision && uiRevision == o.uiRevision && regions == o.regions && payload() == o.payload();
}
bool AgentUiState::offers(const std::string &id) const
{
    for (const auto &action : actions) if (action.id == id) return true;
    return false;
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
    bool found = false;
    for (const Surface *later : paintOrder)
    {
        if (later == widget) { found = true; continue; }
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
        for (const AgentUiRect &part : footprint.regions)
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
