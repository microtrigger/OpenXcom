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
#include "MiniBaseView.h"
#include <cmath>
#include "../Engine/SurfaceSet.h"
#include "../Engine/Action.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProjectileOutput.h"
#endif

namespace OpenXcom
{

/**
 * Sets up a mini base view with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
MiniBaseView::MiniBaseView(int width, int height, int x, int y) : InteractiveSurface(width, height, x, y), _bases(0), _texture(0), _base(0), _hoverBase(0), _red(0), _green(0)
{
}

/**
 *
 */
MiniBaseView::~MiniBaseView()
{
}

/**
 * Changes the current list of bases to display.
 * @param bases Pointer to base list to display.
 */
void MiniBaseView::setBases(std::vector<Base*> *bases)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentRevision;
#endif
	_bases = bases;
	_redraw = true;
}

/**
 * Changes the texture to use for drawing
 * the various base elements.
 * @param texture Pointer to SurfaceSet to use.
 */
void MiniBaseView::setTexture(SurfaceSet *texture)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentRevision;
#endif
	_texture = texture;
}

/**
 * Returns the base the mouse cursor is currently over.
 * @return ID of the base.
 */
size_t MiniBaseView::getHoveredBase() const
{
	return _hoverBase;
}

/**
 * Changes the base that is currently selected on
 * the mini base view.
 * @param base ID of base.
 */
void MiniBaseView::setSelectedBase(size_t base)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    if (_base != base) ++_agentRevision;
#endif
	_base = base;
	_redraw = true;
}

/**
 * Draws the view of all the bases with facilities
 * in varying colors.
 */
void MiniBaseView::draw()
{
	Surface::draw();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    std::array<const void *, 8> mapping{};
#endif
	for (size_t i = 0; i < MAX_BASES; ++i)
	{
		// Draw base squares
		if (i == _base)
		{
			SDL_Rect r;
			r.x = i * (MINI_SIZE + 2);
			r.y = 0;
			r.w = MINI_SIZE + 2;
			r.h = MINI_SIZE + 2;
			drawRect(&r, 1);
		}
		_texture->getFrame(41)->setX(i * (MINI_SIZE + 2));
		_texture->getFrame(41)->setY(0);
		_texture->getFrame(41)->blit(this);

		// Draw facilities
		if (i < _bases->size())
		{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            mapping[i] = _bases->at(i);
#endif
			SDL_Rect r;
			lock();
			for (std::vector<BaseFacility*>::iterator f = _bases->at(i)->getFacilities()->begin(); f != _bases->at(i)->getFacilities()->end(); ++f)
			{
				int color;
				if ((*f)->getBuildTime() == 0)
					color = _green;
				else
					color = _red;

				r.x = i * (MINI_SIZE + 2) + 2 + (*f)->getX() * 2;
				r.y = 2 + (*f)->getY() * 2;
				r.w = (*f)->getRules()->getSize() * 2;
				r.h = (*f)->getRules()->getSize() * 2;
				drawRect(&r, color+3);
				r.x++;
				r.y++;
				r.w--;
				r.h--;
				drawRect(&r, color+5);
				r.x--;
				r.y--;
				drawRect(&r, color+2);
				r.x++;
				r.y++;
				r.w--;
				r.h--;
				drawRect(&r, color+3);
				r.x--;
				r.y--;
				setPixel(r.x, r.y, color+1);
			}
			unlock();
		}
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentCaptureSlots(mapping);
#endif
}

/**
 * Selects the base the mouse is over.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void MiniBaseView::mouseOver(Action *action, State *state)
{
	hoverAt((int)floor(action->getRelativeXMouse() / action->getXScale()));
	InteractiveSurface::mouseOver(action, state);
}

void MiniBaseView::hoverAt(int localX)
{
    _hoverBase = (int)floor(static_cast<double>(localX) / (MINI_SIZE + 2));
}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void MiniBaseView::agentCaptureSlots(const std::array<const void *, 8> &mapping)
{
    const auto oldPixels = _agentPixels;
    const auto oldDrawn = _agentDrawn;
    const auto oldSelected = _agentSelectedInk;
    _agentDrawn.fill(false); _agentSelectedInk.fill(false); _agentSelectedContrast.fill(false);
    const Surface *frame = _texture ? _texture->getFrame(41) : nullptr;
    bool frameOk = frame && frame->agentContextVisible() && Observer::agentPaintHealthy();
    // Stock BASEBITS frames allocate 32x40; this mini-square's ink is 16x16.
    // Transparent padding is harmless, but any omitted source ink is refused.
    if (frameOk) for (int y=0;y<frame->getHeight();++y) for (int x=0;x<frame->getWidth();++x)
        if ((x>=16 || y>=16) && frame->getPixel(x,y)) frameOk=false;
    for (unsigned slot=0; slot<8; ++slot)
    {
        if (!frameOk || int(slot*16+16)>getWidth() || getHeight()<16) continue;
        bool ink = false, selected = false;
        for (int y=0; y<16; ++y) for (int x=0; x<16; ++x)
        {
            const Uint8 pixel = getPixel(slot*16+x,y);
            _agentPixels[slot][y*16+x] = pixel; ink = ink || pixel != 0;
            const bool edge = x==0 || y==0 || x==15 || y==15;
            if (!selected && slot==_base && edge && pixel==1 && frame->getPixel(x,y)==0)
            { selected=true; _agentSelectedPoint[slot]=y*16+x; }
        }
        _agentDrawn[slot]=ink; _agentSelectedInk[slot]=selected;
    }
    if (oldPixels!=_agentPixels || oldDrawn!=_agentDrawn || oldSelected!=_agentSelectedInk || mapping!=_agentMapping) ++_agentRevision;
    _agentMapping=mapping;
}
void MiniBaseView::blit(Surface *surface)
{
    _agentSelectedContrast.fill(false);
    if (_visible && !_hidden && _redraw) draw();
    if (surface && agentContextVisible() && surface->getSurface()->format->BitsPerPixel==8 &&
        getSurface()->format->BitsPerPixel==8 && surface->getSurface()->format->palette && getSurface()->format->palette)
    {
        const SDL_Palette *to=surface->getSurface()->format->palette, *from=getSurface()->format->palette;
        for (unsigned slot=0;slot<8;++slot) if (_agentSelectedInk[slot])
            for (int y=0;y<16;++y) for (int x=0;x<16;++x)
            {
                const int dx=getX()+slot*16+x,dy=getY()+y;
                if (unsigned(y*16+x)!=_agentSelectedPoint[slot] || _agentPixels[slot][y*16+x]!=1 ||
                    dx<0 || dy<0 || dx>=surface->getWidth() || dy>=surface->getHeight()) continue;
                const unsigned previous=surface->getPixel(dx,dy);
                if (previous>=unsigned(to->ncolors) || from->ncolors<=1) continue;
                const SDL_Color a=from->colors[1],b=to->colors[previous];
                if (a.r!=b.r || a.g!=b.g || a.b!=b.b) _agentSelectedContrast[slot]=true;
            }
    }
    Surface::blit(surface);
}
std::vector<AgentMiniBaseSlot> MiniBaseView::agentSlots(const Observer::AgentProjectileOutput &output) const
{
    std::vector<AgentMiniBaseSlot> slots;
    if (!agentContextVisible() || !Observer::agentPaintHealthy()) return slots;
    for (unsigned slot=0;slot<8;++slot)
    {
        if (!_agentDrawn[slot] || int(slot*16+16)>getWidth() || getHeight()<16) continue;
        bool complete=true;
        for (int y=0;y<16;++y) for (int x=0;x<16;++x)
        {
            const Uint8 pixel=_agentPixels[slot][y*16+x];
            if (getPixel(slot*16+x,y)!=pixel || (pixel && !output.matches(getX()+slot*16+x,getY()+y,output.palette[pixel]))) complete=false;
        }
        if (complete) slots.push_back({slot,_agentSelectedInk[slot] && _agentSelectedContrast[slot]});
    }
    return slots;
}
#endif

void MiniBaseView::setColor(Uint8 color)
{
	_green = color;
}
void MiniBaseView::setSecondaryColor(Uint8 color)
{
	_red = color;
}

}
