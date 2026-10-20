// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "AgentUi.h"
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Interface/Text.h"
#include "../Interface/TextEdit.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
namespace OpenXcom { namespace Observer {
// Scoped utilities for the eight explicit outfitting states, not discovery.
template<class GameType> inline bool agentOutfittingDestinationFull(const GameType *game)
{
    const Surface *surface=game->getScreen()->getSurface();
    const SDL_Surface *p=surface?surface->getSurface():nullptr;
    return p && p->w>0 && p->h>0 && p->clip_rect.x==0 && p->clip_rect.y==0 && p->clip_rect.w==p->w && p->clip_rect.h==p->h;
}
inline std::vector<std::uint64_t> agentOutfittingPaintRevisions(const std::vector<Surface *> &surfaces)
{
    std::vector<std::uint64_t> result;
    for (const Surface *s:surfaces)
    {
        result.push_back(s->getVisible());result.push_back(s->getX());result.push_back(s->getY());
        result.push_back(s->getWidth());result.push_back(s->getHeight());
        if (const Text *p=dynamic_cast<const Text *>(s)) result.push_back(p->agentTextRevision());
        if (const TextButton *p=dynamic_cast<const TextButton *>(s)) result.push_back(p->agentLabelRevision());
        if (const TextEdit *p=dynamic_cast<const TextEdit *>(s)) result.push_back(p->agentInputRevision());
        if (const TextList *p=dynamic_cast<const TextList *>(s)) result.push_back(p->agentListRevision());
    }
    return result;
}
class AgentOutfittingView
{
    std::function<bool(const AgentUiState &)> _allowed;
    AgentUiState &_ui;
public:
    const std::vector<Surface *> painted;
    std::string buttons="[";
    template<class GameType> AgentOutfittingView(GameType *game,const std::vector<Surface *> &surfaces,AgentUiState &ui):
        _allowed([game](const AgentUiState &part){return game->agentOverlayClear(part);}),_ui(ui),painted(agentFlattenPaintOrder(surfaces)) {}
    bool allowed(const AgentUiState &part) const { return _allowed(part); }
    bool include(const Surface *widget)
    {
        AgentUiState part;agentIncludeWidget(part,widget);
        if (part.regions.empty() || !agentRegionsUncovered(part.regions,widget,painted) || !allowed(part)) return false;
        const auto before=_ui.regions.size();_ui.regions.insert(_ui.regions.end(),part.regions.begin(),part.regions.end());
        if (agentRegionsDisjoint(_ui)) return true;
        _ui.regions.resize(before);return false;
    }
    std::string text(const Text *widget)
    {
        _ui.widgets.emplace_back(widget,widget->agentTextRevision());
        const auto value=agentTextJson(widget);
        if (value=="null" || !agentWidgetUncovered(widget,painted)) return "null";
        SDL_Rect ink;if (!widget->agentPaintedBounds(ink)) return widget->getText().empty()?value:"null";
        return include(widget)?value:"null";
    }
    std::string edit(const TextEdit *widget)
    {
        _ui.widgets.emplace_back(widget,widget->agentInputRevision());
        std::string value;SDL_Rect ink;
        if (!widget->agentRenderedText(value) || value.size()>1024 || !widget->agentRenderedBounds(ink)) return "null";
        AgentUiState part;part.regions.push_back({ink.x,ink.y,ink.w,ink.h});
        if (!agentRegionsUncovered(part.regions,widget,painted) || !allowed(part)) return "null";
        const auto before=_ui.regions.size();_ui.regions.insert(_ui.regions.end(),part.regions.begin(),part.regions.end());
        if (!agentRegionsDisjoint(_ui)) { _ui.regions.resize(before);return "null"; }
        return agentJsonString(value);
    }
    void button(const TextButton *widget,const char *id,const char *action)
    {
        _ui.widgets.emplace_back(widget,widget->agentLabelRevision());
        const auto label=agentButtonLabel(widget);
        if (label.empty() || label.size()>1024 || !agentWidgetUncovered(widget,painted) || !include(widget)) return;
        if (buttons.size()>1) buttons+=",";
        buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";
        _ui.actions.emplace_back(action,label,"visible_text");
    }
    bool list(const TextList *widget,unsigned columns,bool equipment,AgentListProjection &projection)
    {
        const bool ok=agentProjectOutfittingList(widget,painted,columns,equipment,_ui,projection,
            [this](const AgentUiState &part){return allowed(part);});
        if (!ok) return false;
        _ui.context.push_back(projection.up);_ui.context.push_back(projection.down);
        if (projection.up || projection.down) _ui.actions.emplace_back("scroll_visible_list","Scroll visible list","visible_control_semantics","scroll_direction");
        return true;
    }
    bool finish() const
    {
        if (_ui.visible.size()>9000 || _ui.regions.empty() || !agentRegionsDisjoint(_ui) || !allowed(_ui)) return false;
        size_t ascii=0;for (unsigned char c:_ui.payload()) ascii+=c>=128?6:1;
        return ascii<=30000;
    }
};
inline bool agentOutfittingScroll(TextList *list,const AgentUiState &ui,const AgentRequest &request)
{
    if (request.action!="scroll_visible_list" || ui.context.size()<2) return false;
    const size_t n=ui.context.size();
    if (request.choice=="up" && ui.context[n-2]) { list->scrollUp(false,false);return true; }
    if (request.choice=="down" && ui.context[n-1]) { list->scrollDown(false,false);return true; }
    return false;
}
inline bool agentOutfittingRow(const AgentUiState &ui,const AgentRequest &request,size_t &row,size_t *slot=nullptr)
{
    for (size_t i=0;i<ui.rows.size();++i) if (request.choice=="r"+std::to_string(i))
    { row=ui.rows[i];if(slot)*slot=i;return true; }
    return false;
}
} }
#endif
