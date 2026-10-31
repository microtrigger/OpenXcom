/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Ufopaedia.h"
#include "ArticleState.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Screen.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Observer/AgentUi.h"
#include <algorithm>
#endif
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Surface.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Mod/RuleItem.h"

namespace OpenXcom
{

	/**
	 * Constructor
	 * @param game Pointer to current game.
	 * @param article_id The article id of this article state instance.
	 */
	ArticleState::ArticleState(const std::string &article_id) : _id(article_id)
	{
		// init background and navigation elements
		_bg = new Surface(320, 200, 0, 0);
		_btnOk = new TextButton(30, 14, 5, 5);
		_btnPrev = new TextButton(30, 14, 40, 5);
		_btnNext = new TextButton(30, 14, 75, 5);
	}

	/**
	 * Destructor
	 */
	ArticleState::~ArticleState()
	{}

	std::string ArticleState::getDamageTypeText(ItemDamageType dt) const
	{
		std::string type;
		switch (dt)
		{
		case DT_AP:
			type = "STR_DAMAGE_ARMOR_PIERCING";
			break;
		case DT_IN:
			type = "STR_DAMAGE_INCENDIARY";
			break;
		case DT_HE:
			type = "STR_DAMAGE_HIGH_EXPLOSIVE";
			break;
		case DT_LASER:
			type = "STR_DAMAGE_LASER_BEAM";
			break;
		case DT_PLASMA:
			type = "STR_DAMAGE_PLASMA_BEAM";
			break;
		case DT_STUN:
			type = "STR_DAMAGE_STUN";
			break;
		case DT_MELEE:
			type = "STR_DAMAGE_MELEE";
			break;
		case DT_ACID:
			type = "STR_DAMAGE_ACID";
			break;
		case DT_SMOKE:
			type = "STR_DAMAGE_SMOKE";
			break;
		default:
			type = "STR_UNKNOWN";
			break;
		}
		return type;
	}

	/**
	 * Set captions and click handlers for the common control elements.
	 */
	void ArticleState::initLayout()
	{
		add(_bg);
		add(_btnOk);
		add(_btnPrev);
		add(_btnNext);

		_btnOk->setText(tr("STR_OK"));
		_btnOk->onMouseClick((ActionHandler)&ArticleState::btnOkClick);
		_btnOk->onKeyboardPress((ActionHandler)&ArticleState::btnOkClick,Options::keyOk);
		_btnOk->onKeyboardPress((ActionHandler)&ArticleState::btnOkClick,Options::keyCancel);
		_btnPrev->setText("<<");
		_btnPrev->onMouseClick((ActionHandler)&ArticleState::btnPrevClick);
		_btnPrev->onKeyboardPress((ActionHandler)&ArticleState::btnPrevClick, Options::keyGeoLeft);
		_btnNext->setText(">>");
		_btnNext->onMouseClick((ActionHandler)&ArticleState::btnNextClick);
		_btnNext->onKeyboardPress((ActionHandler)&ArticleState::btnNextClick, Options::keyGeoRight);
	}

	/**
	 * Returns to the previous screen.
	 * @param action Pointer to an action.
	 */
	void ArticleState::btnOkClick(Action *)
	{
		_game->popState();
	}

	/**
	 * Shows the previous available article.
	 * @param action Pointer to an action.
	 */
	void ArticleState::btnPrevClick(Action *)
	{
		Ufopaedia::prev(_game);
	}

	/**
	 * Shows the next available article. Loops to the first.
	 * @param action Pointer to an action.
	 */
	void ArticleState::btnNextClick(Action *)
	{
		Ufopaedia::next(_game);
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void ArticleState::agentRegisterText(Text *title,Text *body)
{
    _agentTitle=title;_agentBody=body;
    if (title) title->agentTrackPresentation();
    if (body) body->agentEnableViewportLines();
    _btnOk->agentTrackPresentation();_btnPrev->agentTrackPresentation();_btnNext->agentTrackPresentation();
}
void ArticleState::agentRegisterCraftStats(Text *text)
{
    _agentCraftStats=text;
    if(text)text->agentTrackPresentation();
}
void ArticleState::agentRegisterTransparentLayer(Surface *surface)
{
    if (!surface) return;
    surface->agentTrackPresentation();_agentTransparentLayers.push_back(surface);
}
bool ArticleState::agentDestinationFull() const
{
    const Surface *surface=_game->getScreen()->getSurface();
    const SDL_Surface *p=surface?surface->getSurface():nullptr;
    return p && p->w>0 && p->h>0 && p->w<=640 && p->h<=400 && p->clip_rect.x==0 &&
        p->clip_rect.y==0 && p->clip_rect.w==p->w && p->clip_rect.h==p->h;
}
std::vector<std::uint64_t> ArticleState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> values;
    for (const Surface *s:_surfaces)
    {
        values.push_back(s->getVisible());values.push_back(s->getX());values.push_back(s->getY());
        values.push_back(s->getWidth());values.push_back(s->getHeight());values.push_back(s->agentPresentationRevision());
        if (const Text *t=dynamic_cast<const Text *>(s)) values.push_back(t->agentTextRevision());
        if (const TextButton *b=dynamic_cast<const TextButton *>(s)) values.push_back(b->agentLabelRevision());
        if (const TextList *l=dynamic_cast<const TextList *>(s)) values.push_back(l->agentListRevision());
    }
    return values;
}
void ArticleState::blit()
{
    _agentArticlePaintFull=false;_agentStoredPaintRevisions.clear();
    const bool before=agentDestinationFull();
    for (Surface *s:_surfaces) s->agentForgetPresentationPaint();
    for (Surface *s:Observer::agentFlattenPaintOrder(_agentTransparentLayers))
    { s->agentTrackPresentation();s->agentForgetPresentationPaint(); }
    State::blit();
    _agentStoredPaintRevisions=agentPaintRevisions();
    _agentArticlePaintFull=before && agentDestinationFull() && Observer::agentPaintHealthy();
}
bool ArticleState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<ArticleState *>(this)) || _modal || !_agentTitle || !_agentBody ||
        !_agentArticlePaintFull || !agentDestinationFull() || !agentPaintHealthy() ||
        _agentStoredPaintRevisions!=agentPaintRevisions()) return false;
    out=AgentUiState();out.screen="ufopaedia_article";
    const Surface *logical=_game->getScreen()->getSurface();
    const auto painted=agentFlattenPaintOrder(_surfaces);
    const auto transparent=agentFlattenPaintOrder(_agentTransparentLayers);
    size_t work=0;
    const auto allowed=[&](const AgentUiRect &region) {
        AgentUiState part;part.regions.push_back(region);return _game->agentOverlayClear(part);
    };
    const auto clearOwned=[&](const Surface *layer,const AgentUiRect &region) {
        if (!layer->agentPresentationPaintedTo(logical)) return false;
        const SDL_Surface *p=layer->getSurface();
        if (!p || !p->format || p->format->BytesPerPixel!=1 || !(p->flags&SDL_SRCCOLORKEY) ||
            p->format->colorkey!=0 || (p->flags&SDL_SRCALPHA) || p->w>640 || p->h>400) return false;
        const int left=std::max(region.x,layer->getX()),top=std::max(region.y,layer->getY());
        const int right=std::min(region.x+region.width,layer->getX()+layer->getWidth());
        const int bottom=std::min(region.y+region.height,layer->getY()+layer->getHeight());
        if (left>=right || top>=bottom) return true;
        if (size_t(right-left)*(bottom-top)>2097152-std::min(size_t(2097152),work)) return false;
        work+=size_t(right-left)*(bottom-top);
        SDL_Surface *pixels=layer->getSurface();
        if (agentObservePaint(SDL_LockSurface(pixels))<0) return false;
        bool clear=true;
        for (int y=top;y<bottom && clear;++y) for (int x=left;x<right;++x)
            if (*(static_cast<const Uint8 *>(pixels->pixels)+(y-layer->getY())*pixels->pitch+x-layer->getX()))
            { clear=false;break; }
        SDL_UnlockSurface(pixels);return clear;
    };
    const auto uncovered=[&](const AgentUiRect &region,const Surface *owner) {
        if (!agentRegionVisible(region) || !allowed(region)) return false;
        bool found=false;
        for (const Surface *later:painted)
        {
            if (later==owner) {found=true;continue;}
            if (!found || !later || !later->getVisible()) continue;
            if (++work>2097152) return false;
            if (agentRegionsUncovered({region},owner,{const_cast<Surface *>(owner),const_cast<Surface *>(later)})) continue;
            if (std::find(transparent.begin(),transparent.end(),later)!=transparent.end() && clearOwned(later,region)) continue;
            return false;
        }
        return found;
    };
    const auto include=[&](const Surface *widget) {
        if (!widget->agentPresentationPaintedTo(logical)) return false;
        AgentUiState part;agentIncludeWidget(part,widget);
        if (part.regions.empty()) return false;
        for (const auto &r:part.regions) if (!uncovered(r,widget)) return false;
        const auto before=out.regions.size();out.regions.insert(out.regions.end(),part.regions.begin(),part.regions.end());
        if (agentRegionsDisjoint(out)) return true;
        out.regions.resize(before);return false;
    };
    for (const Surface *widget:{static_cast<const Surface *>(_agentTitle),static_cast<const Surface *>(_agentBody),
        static_cast<const Surface *>(_btnOk),static_cast<const Surface *>(_btnPrev),static_cast<const Surface *>(_btnNext)})
        out.widgets.emplace_back(widget,widget->agentPresentationRevision());
    for (const Surface *layer:transparent)
    {
        out.widgets.emplace_back(layer,layer->agentPresentationRevision());
        if (const TextList *list=dynamic_cast<const TextList *>(layer)) out.widgets.emplace_back(list,list->agentListRevision());
    }
    std::string buttons="[";
    const auto button=[&](TextButton *widget,const char *id,const char *action) {
        out.widgets.emplace_back(widget,widget->agentLabelRevision());
        const auto label=agentButtonLabel(widget);
        if (label.empty() || label.size()>1024 || !include(widget)) return;
        if (buttons.size()>1) buttons+=",";
        buttons+="{\"id\":"+agentJsonString(id)+",\"label\":"+agentJsonString(label)+"}";
        out.actions.emplace_back(action,label,"visible_text");
    };
    button(_btnOk,"ok","close_ufopaedia_article");
    button(_btnPrev,"previous","previous_ufopaedia_article");
    button(_btnNext,"next","next_ufopaedia_article");buttons+="]";
    std::string title="null";
    if (_agentTitle->getText().size()<=1024)
    {
        const auto shown=agentTextJson(_agentTitle);
        if (shown!="null" && _agentTitle->agentPresentationPaintedTo(logical) &&
            (_agentTitle->getText().empty() || include(_agentTitle))) title=shown;
    }
    std::string body="null";
    std::vector<Text::AgentVisibleLine> lines;
    bool scroll=false;
    const AgentUiRect control={_agentBody->getX(),_agentBody->getY(),_agentBody->getWidth(),_agentBody->getHeight()};
    auto beforeBodyRegions=out.regions.size();
    if (_agentBody->agentViewportLines(logical,lines))
    {
        if (_agentBody->agentScrollable() && !lines.empty() && uncovered(control,_agentBody))
        {
            out.regions.push_back(control);
            if (agentRegionsDisjoint(out)) scroll=true;else out.regions.resize(beforeBodyRegions);
        }
        // Body-text fallback must retain an independently certified scroll control.
        beforeBodyRegions=out.regions.size();
        size_t total=0,count=0;bool bounded=true;body="[";
        for (const auto &line:lines)
        {
            const AgentUiRect r={_agentBody->getX()+line.layout.x,_agentBody->getY()+line.layout.y,line.layout.w,line.layout.h};
            if (++count>32) {bounded=false;break;}
            if (body.size()>1) body+=",";
            if (line.complete && !line.text.empty() && line.text.size()<=1024 && uncovered(r,_agentBody))
            {
                total+=line.text.size();body+=agentJsonString(line.text);
                if (!scroll) out.regions.push_back(r);
            }
            else body+="null";
            if (total>8192 || work>2097152) {bounded=false;break;}
        }
        body+="]";
        if (!bounded || !agentRegionsDisjoint(out)) {body="null";out.regions.resize(beforeBodyRegions);}
    }
    out.context.push_back(scroll?1:0);
    if (scroll) out.actions.emplace_back("scroll_article_text","Scroll article text","visible_control_semantics","scroll_direction");
    // CRAFT uses a single native Text, not a two-column stats TextList.
    // Preserve exact logical lines (including color markers); never query RuleCraft.
    std::string craftStats="null";
    if(_agentCraftStats && std::find(_surfaces.begin(),_surfaces.end(),_agentCraftStats)!=_surfaces.end()) {
        out.widgets.emplace_back(_agentCraftStats,_agentCraftStats->agentTextRevision());
        out.widgets.emplace_back(_agentCraftStats,_agentCraftStats->agentPresentationRevision());
        const std::string &text=_agentCraftStats->getText();
        if(!_agentCraftStats->agentScrollable() && agentTextJson(_agentCraftStats)!="null" &&
            text.size()<=8192 && _agentCraftStats->agentPresentationPaintedTo(logical)) {
            std::string value="[";size_t start=0,count=0;bool valid=true;
            for(;;) {
                const size_t end=text.find('\n',start);
                const std::string line=text.substr(start,end==std::string::npos?end:end-start);
                if(++count>32 || line.size()>1024) {valid=false;break;}
                if(count>1)value+=",";value+=agentJsonString(line);
                if(end==std::string::npos)break;
                start=end+1;
            }
            const AgentUiRect region={_agentCraftStats->getX(),_agentCraftStats->getY(),_agentCraftStats->getWidth(),_agentCraftStats->getHeight()};
            if(valid && uncovered(region,_agentCraftStats)) {
                const auto before=out.regions.size();out.regions.push_back(region);
                if(agentRegionsDisjoint(out))craftStats=value+"]";
                else out.regions.resize(before);
            }
        }
    }
    const auto finish=[&]() {return "{\"title\":"+title+",\"body_lines\":"+body+",\"buttons\":"+buttons+",\"details_status\":\""+(craftStats!="null"?std::string("partial"):std::string("unavailable"))+"\",\"craft_stats\":"+craftStats+"}";};
    out.visible=finish();
    const auto bounded=[&]() {size_t bytes=0;for(unsigned char c:out.payload()) if((bytes+=c<128?1:6)>30000)return false;return true;};
    if (!bounded()) {body="null";craftStats="null";out.regions.resize(beforeBodyRegions);out.visible=finish();}
    return bounded() && !out.regions.empty() && agentRegionsDisjoint(out) && _game->agentOverlayClear(out);
}
bool ArticleState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action=="close_ufopaedia_article") {btnOkClick(nullptr);return true;}
    if (request.action=="previous_ufopaedia_article") {btnPrevClick(nullptr);return true;}
    if (request.action=="next_ufopaedia_article") {btnNextClick(nullptr);return true;}
    if (request.action=="scroll_article_text" && current.context.size()==1 && current.context[0] &&
        (request.choice=="up" || request.choice=="down"))
    { _agentBody->scrollLine(request.choice=="down");return true; }
    return false;
}
#endif

}
