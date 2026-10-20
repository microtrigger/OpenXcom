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
#include "TextButton.h"
#include <SDL.h>
#include <SDL_mixer.h>
#include "Text.h"
#include "../Engine/Sound.h"
#include "../Engine/Action.h"
#include "ComboBox.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Engine/Font.h"
#include "../Observer/AgentUi.h"
#endif

namespace OpenXcom
{

Sound *TextButton::soundPress;

/**
 * Sets up a text button with the specified size and position.
 * The text is centered on the button.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
TextButton::TextButton(int width, int height, int x, int y) : InteractiveSurface(width, height, x, y), _color(0), _group(0), _contrast(false), _geoscapeButton(false), _comboBox(0)
{
	_text = new Text(width, height, 0, 0);
	_text->setSmall();
	_text->setAlign(ALIGN_CENTER);
	_text->setVerticalAlign(ALIGN_MIDDLE);
	_text->setWordWrap(true);
}

/**
 * Deletes the contained Text.
 */
TextButton::~TextButton()
{
	delete _text;
}

bool TextButton::isButtonHandled(Uint8 button)
{
	if (_comboBox != 0)
	{
		return (button == SDL_BUTTON_LEFT);
	}
	else
	{
		return InteractiveSurface::isButtonHandled(button);
	}
}

/**
 * Changes the color for the button and text.
 * @param color Color value.
 */
void TextButton::setColor(Uint8 color)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_color = color;
	_text->setColor(color);
	_redraw = true;
}

/**
 * Returns the color for the button and text.
 * @return Color value.
 */
Uint8 TextButton::getColor() const
{
	return _color;
}

/**
 * Changes the color for the text only.
 * @param color Color value.
 */
void TextButton::setTextColor(Uint8 color)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_text->setColor(color);
	_redraw = true;
}

/**
 * Changes the text to use the big-size font.
 */
void TextButton::setBig()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_text->setBig();
	_redraw = true;
}

/**
 * Changes the text to use the small-size font.
 */
void TextButton::setSmall()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_text->setSmall();
	_redraw = true;
}

/**
 * Returns the font currently used by the text.
 * @return Pointer to font.
 */
Font *TextButton::getFont() const
{
	return _text->getFont();
}

/**
 * Changes the various resources needed for text rendering.
 * The different fonts need to be passed in advance since the
 * text size can change mid-text, and the language affects
 * how the text is rendered.
 * @param big Pointer to large-size font.
 * @param small Pointer to small-size font.
 * @param lang Pointer to current language.
 */
void TextButton::initText(Font *big, Font *small, Language *lang)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_text->initText(big, small, lang);
	_redraw = true;
}

/**
 * Enables/disables high contrast color. Mostly used for
 * Battlescape UI.
 * @param contrast High contrast setting.
 */
void TextButton::setHighContrast(bool contrast)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_contrast = contrast;
	_text->setHighContrast(contrast);
	_redraw = true;
}

/**
 * Changes the text of the button label.
 * @param text Text string.
 */
void TextButton::setText(const std::string &text)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_text->setText(text);
	_redraw = true;
}

/**
 * Returns the text of the button label.
 * @return Text string.
 */
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
std::uint64_t TextButton::agentLabelRevision() const { return _text->agentTextRevision(); }
#endif

std::string TextButton::getText() const
{
	return _text->getText();
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
std::string TextButton::agentLabel(int rightInset) const
{
    // The child Text uses button-local coordinates, including the combo's
    // deliberate negative X offset. Use its actual rendered glyph footprint,
    // rather than an assumed nonnegative font spacing (stock FONT_SMALL is -1).
    if (!Observer::agentWidgetVisible(this) || !_text || !_text->agentContextVisible() ||
        !_text->agentTextComplete() || !_text->getFont() || _text->getText().size() > 1024 ||
        rightInset < 0 || rightInset >= getWidth()) return std::string();
    SDL_Rect ink;
    if (!_text->agentPaintedBounds(ink) || ink.x < 0 || ink.y < 0 ||
        ink.x + ink.w > getWidth() - rightInset || ink.y + ink.h > getHeight()) return std::string();
    return _text->getText();
}

bool TextButton::agentPressed(bool &pressed) const
{
    if (agentLabel().empty()) return false;
    pressed = _agentDrawnPressed;
    return true;
}
#endif

/**
 * Changes the button group this button belongs to.
 * @param group Pointer to the pressed button pointer in the group.
 * Null makes it a regular button.
 */
void TextButton::setGroup(TextButton **group)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_group = group;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	++_agentSelectionRevision;
#endif
	_redraw = true;
}

/**
 * Replaces a certain amount of colors in the surface's palette.
 * @param colors Pointer to the set of colors.
 * @param firstcolor Offset of the first color to replace.
 * @param ncolors Amount of colors to replace.
 */
void TextButton::setPalette(SDL_Color *colors, int firstcolor, int ncolors)
{
	Surface::setPalette(colors, firstcolor, ncolors);
	_text->setPalette(colors, firstcolor, ncolors);
}

/**
 * Draws the labeled button.
 * The colors are inverted if the button is pressed.
 */
void TextButton::draw()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentNamedPaint=false;
#endif
	Surface::draw();
	SDL_Rect square;

	int mul = 1;
	if (_contrast)
	{
		mul = 2;
	}

	int color = _color + 1 * mul;

	square.x = 0;
	square.y = 0;
	square.w = getWidth();
	square.h = getHeight();

	for (int i = 0; i < 5; ++i)
	{
		drawRect(&square, color);

		if (i % 2 == 0)
		{
			square.x++;
			square.y++;
		}
		square.w--;
		square.h--;

		switch (i)
		{
		case 0:
			color = _color + 5 * mul;
			setPixel(square.w, 0, color);
			break;
		case 1:
			color = _color + 2 * mul;
			break;
		case 2:
			color = _color + 4 * mul;
			setPixel(square.w+1, 1, color);
			break;
		case 3:
			color = _color + 3 * mul;
			break;
		case 4:
			if (_geoscapeButton)
			{
				setPixel(0, 0, _color);
				setPixel(1, 1, _color);
			}
			break;
		}
	}

	bool press;
	if (_group == 0)
		press = isButtonPressed();
	else
		press = (*_group == this);

	if (press)
	{
		if (_geoscapeButton)
		{
			this->invert(_color + 2 * mul);
		}
		else
		{
			this->invert(_color + 3 * mul);
		}
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	_agentDrawnPressed = press;
#endif
	_text->setInvert(press);

	_text->blit(this);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    if (_agentNamedControl) agentCaptureNamedControl();
#endif
}

/**
 * Selects and redraws this button through the ordinary radio input operation.
 * @return False for a button that does not belong to a radio group.
 */
bool TextButton::selectRadio()
{
	if (!_group) return false;
	TextButton *old = *_group;
	*_group = this;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (old != this) ++_agentSelectionRevision;
#endif
	if (old != 0) old->draw();
	draw();
	return true;
}

/**
 * Handles radio selection, sounds and ordinary mouse-press callbacks.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void TextButton::mousePress(Action *action, State *state)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		selectRadio();
	}

	if (isButtonHandled(action->getDetails()->button.button))
	{
		if (soundPress != 0 && _group == 0 &&
			action->getDetails()->button.button != SDL_BUTTON_WHEELUP && action->getDetails()->button.button != SDL_BUTTON_WHEELDOWN)
		{
			soundPress->play(Mix_GroupAvailable(0));
		}

		if (_comboBox)
		{
			_comboBox->toggle();
		}

		draw();
		//_redraw = true;
	}
	InteractiveSurface::mousePress(action, state);
}

/**
 * Sets the button as the released button.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void TextButton::mouseRelease(Action *action, State *state)
{
	if (isButtonHandled(action->getDetails()->button.button))
	{
		draw();
		//_redraw = true;
	}
	InteractiveSurface::mouseRelease(action, state);
}

/**
 * Hooks up the button to work as part of an existing combobox,
 * toggling its state when it's pressed.
 * @param comboBox Pointer to ComboBox.
 */
void TextButton::setComboBox(ComboBox *comboBox)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_comboBox = comboBox;
	if (_comboBox)
	{
		_text->setX(-6);
	}
	else
	{
		_text->setX(0);
	}
}

void TextButton::setWidth(int width)
{
	Surface::setWidth(width);
	_text->setWidth(width);
}

void TextButton::setHeight(int height)
{
	Surface::setHeight(height);
	_text->setHeight(height);
}

void TextButton::setGeoscapeButton(bool geo)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_geoscapeButton = geo;
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace {
bool namedPixels(const SDL_Surface *p)
{
    return p && p->w>0 && p->h>0 && p->w<=640 && p->h<=400 && p->format &&
        p->format->BytesPerPixel==1 && p->format->palette && p->format->palette->colors && p->format->palette->ncolors==256 &&
        (p->flags&SDL_SRCCOLORKEY) && p->format->colorkey==0 && !(p->flags&SDL_SRCALPHA) &&
        p->clip_rect.x==0 && p->clip_rect.y==0 && p->clip_rect.w==p->w && p->clip_rect.h==p->h;
}
}
void TextButton::blit(Surface *surface)
{
    if(!_agentNamedControl){Surface::blit(surface);return;}
    _agentNamedBlitValid=false;
    const bool before=namedPixels(_surface)&&surface&&namedPixels(surface->getSurface());
    Surface::blit(surface);
    _agentNamedBlitValid=before&&namedPixels(_surface)&&surface&&namedPixels(surface->getSurface())&&agentPresentationPaintedTo(surface);
}
void TextButton::agentCaptureNamedControl()
{
    _agentNamedPaint=false;_agentNamedPixels.clear();_agentNamedLabel.clear();
    const auto label=agentLabel();SDL_Rect ink;
    if (label.empty() || getWidth()<7 || getHeight()<7 || !namedPixels(_surface) ||
        !namedPixels(_text->getSurface()) || !_text->agentPaintedBounds(ink) || !ink.w || !ink.h || size_t(ink.w)*ink.h>8192 ||
        ink.x<0 || ink.y<0 || ink.x+ink.w>getWidth() || ink.y+ink.h>getHeight() || !Observer::agentPaintHealthy()) return;
    if (Observer::agentObservePaint(SDL_LockSurface(_surface))<0) return;
    for(int y=ink.y;y<ink.y+ink.h;++y) for(int x=ink.x;x<ink.x+ink.w;++x)
        _agentNamedPixels.push_back(*(static_cast<const Uint8*>(_surface->pixels)+y*_surface->pitch+x));
    SDL_UnlockSurface(_surface);
    for(int i=0;i<256;++i)_agentNamedPalette[i]=_surface->format->palette->colors[i];
    _agentNamedInk=ink;_agentNamedLabel=label;_agentNamedLabelRevision=agentLabelRevision();_agentNamedPaint=true;
}
bool TextButton::agentNamedControl(const Surface *destination,std::string &label,SDL_Rect &ink,SDL_Rect &point) const
{
    label.clear();
    if (!_agentNamedControl || !_agentNamedPaint || !_agentNamedBlitValid || !agentPresentationPaintedTo(destination) ||
        _agentNamedLabelRevision!=agentLabelRevision() || agentLabel()!=_agentNamedLabel ||
        !namedPixels(_surface) || !destination || !namedPixels(destination->getSurface()) ||
        getX()<0 || getY()<0 || getX()+getWidth()>destination->getWidth() || getY()+getHeight()>destination->getHeight()) return false;
    SDL_Surface *target=destination->getSurface();
    for(int i=0;i<256;++i){const auto&a=_agentNamedPalette[i];const auto&b=_surface->format->palette->colors[i];const auto&c=target->format->palette->colors[i];if(a.r!=b.r||a.g!=b.g||a.b!=b.b||a.r!=c.r||a.g!=c.g||a.b!=c.b)return false;}
    const int a=Observer::agentObservePaint(SDL_LockSurface(_surface));const int b=Observer::agentObservePaint(SDL_LockSurface(target));
    if(a<0||b<0){if(a>=0)SDL_UnlockSurface(_surface);if(b>=0)SDL_UnlockSurface(target);return false;}
    bool same=true;size_t n=0;
    for(int y=_agentNamedInk.y;y<_agentNamedInk.y+_agentNamedInk.h;++y)for(int x=_agentNamedInk.x;x<_agentNamedInk.x+_agentNamedInk.w;++x){const auto pixel=_agentNamedPixels[n++];if(*(static_cast<const Uint8*>(_surface->pixels)+y*_surface->pitch+x)!=pixel||*(static_cast<const Uint8*>(target->pixels)+(getY()+y)*target->pitch+getX()+x)!=pixel)same=false;}
    SDL_UnlockSurface(target);SDL_UnlockSurface(_surface);if(!same||!Observer::agentPaintHealthy())return false;
    label=_agentNamedLabel;ink=_agentNamedInk;ink.x+=getX();ink.y+=getY();point={Sint16(getX()+3),Sint16(getY()+3),1,1};return true;
}
#endif

}
