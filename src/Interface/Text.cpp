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
#include "Text.h"
#include "../fmath.h"
#include "../Engine/Font.h"
#include "../Engine/Options.h"
#include "../Engine/Language.h"
#include "../Engine/Unicode.h"
#include "../Engine/ShaderDraw.h"
#include "../Engine/ShaderMove.h"
#include "../Engine/Action.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include <cstring>
#include <limits>
#endif

namespace OpenXcom
{

/**
 * Sets up a blank text with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
Text::Text(int width, int height, int x, int y) : InteractiveSurface(width, height, x, y), _big(0), _small(0), _font(0), _fontOrig(0), _lang(0), _wrap(false), _invert(false), _contrast(false), _indent(false), _scroll(false),_align(ALIGN_LEFT), _valign(ALIGN_TOP), _color(0), _color2(0), _scrollY(0), _extraLineSpacing(0)
{
}

/**
 *
 */
Text::~Text()
{

}

/**
 * Changes the text to use the big-size font.
 */
void Text::setBig()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_font = _big;
	_fontOrig = _big;
	processText();
}

/**
 * Changes the text to use the small-size font.
 */
void Text::setSmall()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_font = _small;
	_fontOrig = _small;
	processText();
}

/**
 * Returns the font currently used by the text.
 * @return Pointer to font.
 */
Font *Text::getFont() const
{
	return _font;
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
void Text::initText(Font *big, Font *small, Language *lang)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_big = big;
	_small = small;
	_lang = lang;
	setSmall();
}

/**
 * Changes the string displayed on screen.
 * @param text Text string.
 */
void Text::setText(const std::string &text)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    if (_text != text) ++_agentTextRevision;
    _agentHeadingComplete = false;
    _agentHeadingText.clear(); _agentHeadingPixels.clear();
    // Any explicit replacement removes the old TextList provenance marker.
    _agentListRange = _agentListComplete = false;
#endif
	_text = text;
	_font = _fontOrig;
	processText();
	// If big text won't fit the space, try small text
	if (!_text.empty())
	{
		if (_font == _big && (getTextWidth() > getWidth() || getTextHeight() > getHeight()) && _text[_text.size() - 1] != '.')
		{
			_font = _small;
			processText();
		}
	}
}

/**
 * Returns the string displayed on screen.
 * @return Text string.
 */
std::string Text::getText() const
{
	return _text;
}

/**
 * Enables/disables text wordwrapping. When enabled, lines of
 * text are automatically split to ensure they stay within the
 * drawing area, otherwise they simply go off the edge.
 * @param wrap Wordwrapping setting.
 * @param indent Indent wrapped text.
 */
void Text::setWordWrap(bool wrap, bool indent)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	if (wrap != _wrap || indent != _indent)
	{
		_wrap = wrap;
		_indent = indent;
		processText();
	}
}

/**
 * Enables/disables color inverting. Mostly used to make
 * button text look pressed along with the button.
 * @param invert Invert setting.
 */
void Text::setInvert(bool invert)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_invert = invert;
	_redraw = true;
}

/**
 * Enables/disables high contrast color. Mostly used for
 * Battlescape UI.
 * @param contrast High contrast setting.
 */
void Text::setHighContrast(bool contrast)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_contrast = contrast;
	_redraw = true;
}

/**
 * Changes the way the text is aligned horizontally
 * relative to the drawing area.
 * @param align Horizontal alignment.
 */
void Text::setAlign(TextHAlign align)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_align = align;
	_redraw = true;
}

/**
 * Returns the way the text is aligned horizontally
 * relative to the drawing area.
 * @return Horizontal alignment.
 */
TextHAlign Text::getAlign() const
{
	return _align;
}

/**
 * Changes the way the text is aligned vertically
 * relative to the drawing area.
 * @param valign Vertical alignment.
 */
void Text::setVerticalAlign(TextVAlign valign)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_valign = valign;
	_redraw = true;
}

/**
 * Returns the way the text is aligned vertically
 * relative to the drawing area.
 * @return Horizontal alignment.
 */
TextVAlign Text::getVerticalAlign() const
{
	return _valign;
}

/**
 * Changes the color used to render the text. Unlike regular graphics,
 * fonts are greyscale so they need to be assigned a specific position
 * in the palette to be displayed.
 * @param color Color value.
 */
void Text::setColor(Uint8 color)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_color = color;
	_color2 = color;
	_redraw = true;
}

/**
 * Returns the color used to render the text.
 * @return Color value.
 */
Uint8 Text::getColor() const
{
	return _color;
}

/**
 * Changes the secondary color used to render the text. The text
 * switches between the primary and secondary color whenever there's
 * a 0x01 in the string.
 * @param color Color value.
 */
void Text::setSecondaryColor(Uint8 color)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_color2 = color;
	_redraw = true;
}

/**
 * Returns the secondary color used to render the text.
 * @return Color value.
 */
Uint8 Text::getSecondaryColor() const
{
	return _color2;
}

int Text::getNumLines() const
{
	return _wrap ? _lineHeight.size() : 1;
}

/**
 * Returns the rendered text's height. Useful to check if wordwrap applies.
 * @param line Line to get the height, or -1 to get whole text height.
 * @return Height in pixels.
 */
int Text::getTextHeight(int line) const
{
	if (line == -1)
	{
		int height = 0;
		for (std::vector<int>::const_iterator i = _lineHeight.begin(); i != _lineHeight.end(); ++i)
		{
			height += *i;
		}
		return height;
	}
	else
	{
		return _lineHeight[line];
	}
}

/**
 * Returns the rendered text's width.
 * @param line Line to get the width, or -1 to get whole text width.
 * @return Width in pixels.
 */
int Text::getTextWidth(int line) const
{
	if (line == -1)
	{
		int width = 0;
		for (std::vector<int>::const_iterator i = _lineWidth.begin(); i != _lineWidth.end(); ++i)
		{
			if (*i > width)
			{
				width = *i;
			}
		}
		return width;
	}
	else
	{
		return _lineWidth[line];
	}
}

/**
 * Takes care of any text post-processing like converting
 * encoded text to individual codepoints and calculating
 * line metrics for alignment and wordwrapping.
 */
void Text::processText()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	if (_font == 0 || _lang == 0)
	{
		return;
	}

	_processedText = Unicode::convUtf8ToUtf32(_text);
	_lineWidth.clear();
	_lineHeight.clear();
	_scrollY = 0;

	int width = 0, word = 0;
	size_t space = 0, textIndentation = 0;
	bool start = true;
	Font *font = _font;
	UString &str = _processedText;

	// Go through the text character by character
	for (size_t c = 0; c <= str.size(); ++c)
	{
		// End of the line
		if (c == str.size() || Unicode::isLinebreak(str[c]))
		{
			// Add line measurements for alignment later
			_lineWidth.push_back(width);
			_lineHeight.push_back(font->getCharSize('\n').h + _extraLineSpacing);
			width = 0;
			word = 0;
			start = true;

			if (c == str.size())
				break;
			else if (str[c] == Unicode::TOK_NL_SMALL)
				font = _small;
		}
		// Keep track of spaces for wordwrapping
		else if (Unicode::isSpace(str[c]) || Unicode::isSeparator(str[c]))
		{
			// Store existing indentation
			if (c == textIndentation)
			{
				textIndentation++;
			}
			space = c;
			width += font->getCharSize(str[c]).w;
			word = 0;
			start = false;
		}
		// Keep track of the width of the last line and word
		else if (str[c] != Unicode::TOK_COLOR_FLIP)
		{
			int charWidth = font->getCharSize(str[c]).w;

			width += charWidth;
			word += charWidth;

			// Wordwrap if the last word doesn't fit the line
			if (_wrap && width >= getWidth() && (!start || _lang->getTextWrapping() == WRAP_LETTERS))
			{
				size_t indentLocation = c;
				if (_lang->getTextWrapping() == WRAP_WORDS || Unicode::isSpace(str[c]))
				{
					// Go back to the last space and put a linebreak there
					width -= word;
					indentLocation = space;
					if (Unicode::isSpace(str[space]))
					{
						width -= font->getCharSize(str[space]).w;
						str[space] = '\n';
					}
					else
					{
						str.insert(space+1, 1, '\n');
						indentLocation++;
					}
				}
				else if (_lang->getTextWrapping() == WRAP_LETTERS)
				{
					// Go back to the last letter and put a linebreak there
					str.insert(c, 1, '\n');
					width -= charWidth;
				}

				// Keep initial indentation of text
				if (textIndentation > 0)
				{
					str.insert(indentLocation+1, textIndentation, '\t');
					indentLocation += textIndentation;
				}
				// Indent due to word wrap.
				if (_indent)
				{
					str.insert(indentLocation+1, 1, '\t');
					width += font->getCharSize('\t').w;
				}

				_lineWidth.push_back(width);
				_lineHeight.push_back(font->getCharSize('\n').h + _extraLineSpacing);
				if (_lang->getTextWrapping() == WRAP_WORDS)
				{
					width = word;
				}
				else if (_lang->getTextWrapping() == WRAP_LETTERS)
				{
					width = 0;
				}
				start = true;
			}
		}
	}

	_redraw = true;
}

/**
 * Calculates the starting X position for a line of text.
 * @param line The line number (0 = first, etc).
 * @return The X position in pixels.
 */
int Text::getLineX(int line) const
{
	int x = 0;
	switch (_lang->getTextDirection())
	{
	case DIRECTION_LTR:
		switch (_align)
		{
		case ALIGN_LEFT:
			break;
		case ALIGN_CENTER:
			x = (int)ceil((getWidth() + _font->getSpacing() - _lineWidth[line]) / 2.0);
			break;
		case ALIGN_RIGHT:
			x = getWidth() - 1 - _lineWidth[line];
			break;
		}
		break;
	case DIRECTION_RTL:
		switch (_align)
		{
		case ALIGN_LEFT:
			x = getWidth() - 1;
			break;
		case ALIGN_CENTER:
			x = getWidth() - (int)ceil((getWidth() + _font->getSpacing() - _lineWidth[line]) / 2.0);
			break;
		case ALIGN_RIGHT:
			x = _lineWidth[line];
			break;
		}
		break;
	}
	return x;
}

namespace
{

struct PaletteShift
{
	static inline void func(Uint8& dest, Uint8& src, int off, int mul, int mid)
	{
		if (src)
		{
			int inverseOffset = mid ? 2 * (mid - src) : 0;
			dest = off + src * mul + inverseOffset;
		}
	}
};

} //namespace

/**
 * Draws all the characters in the text with a really
 * nasty complex gritty text rendering algorithm logic stuff.
 */
void Text::draw()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentBeginVisibleLines();
    _agentHasGlyph = _agentTextClipped = false;
    _agentHeadingComplete = _agentHeadingHasGlyph = false;
    _agentHeadingPixels.clear(); _agentHeadingText.clear(); _agentHeadingInk = SDL_Rect();
    const size_t headingEnd = _text.find(Unicode::TOK_NL_SMALL);
    if (_agentErrorHeading && !_scroll && headingEnd != std::string::npos && headingEnd > 0 && headingEnd <= 1024)
    {
        _agentHeadingText = _text.substr(0, headingEnd);
        _agentHeadingComplete = true;
    }
    bool headingContent = _agentHeadingComplete;
    _agentListShown.clear(); _agentListHasGlyph = false; _agentListInk = SDL_Rect();
    _agentListComplete = _agentListRange && !_wrap && !_scroll && _lineWidth.size() == 1 &&
        _agentListStart <= _processedText.size() && _agentListLength <= _processedText.size() - _agentListStart &&
        _processedText == Unicode::convUtf8ToUtf32(_text);
#endif
	Surface::draw();
	if (_text.empty() || _font == 0)
	{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        if (_agentVisibleLinesEnabled) agentFinishVisibleLines();
#endif
		return;
	}

	// Show text borders for debugging
	if (Options::debugUi)
	{
		SDL_Rect r;
		r.w = getWidth();
		r.h = getHeight();
		r.x = 0;
		r.y = 0;
		this->drawRect(&r, 5);
		r.w-=2;
		r.h-=2;
		r.x++;
		r.y++;
		this->drawRect(&r, 0);
	}

	int x = 0, y = 0, line = 0, height = 0;
	Font *font = _font;
	int color = _color;
	const UString &s = _processedText;

	height = getTextHeight();

	if (_scroll)
	{
		y = _scrollY;
	}
	else
	{
		switch (_valign)
		{
		case ALIGN_TOP:
			y = 0;
			break;
		case ALIGN_MIDDLE:
			y = (int)ceil((getHeight() - height) / 2.0);
			break;
		case ALIGN_BOTTOM:
			y = getHeight() - height;
			break;
		}
	}

	x = getLineX(line);

	// Set up text color
	int mul = 1;
	if (_contrast)
	{
		mul = 3;
	}

	// Set up text direction
	int dir = 1;
	if (_lang->getTextDirection() == DIRECTION_RTL)
	{
		dir = -1;
	}

	// Invert text by inverting the font palette on index 3 (font palettes use indices 1-5)
	int mid = _invert ? 3 : 0;

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    if (_agentVisibleLinesEnabled)
    {
        if (dir != 1 || Options::debugUi) _agentLinesHealthy = false;
        agentStartVisibleLine(y, font->getHeight() + font->getSpacing());
    }
#endif
	// Draw each letter one by one
	for (UString::const_iterator c = s.begin(); c != s.end(); ++c)
	{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        const size_t position = c - s.begin();
        const bool listContent = _agentListRange && position >= _agentListStart &&
            position - _agentListStart < _agentListLength;
        if (listContent) _agentListShown.push_back(*c);
#endif
		if (Unicode::isSpace(*c) || *c == '\t')
		{
			x += dir * font->getCharSize(*c).w;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            if (listContent && (x < 0 || x > getWidth())) _agentListComplete = false;
            if (_agentVisibleLinesEnabled && _agentLineCandidate)
            {
                _agentLinePendingSpace = true;
                if (x < 0 || x > getWidth()) _agentCurrentLine.complete = false;
            }
#endif
		}
		else if (Unicode::isLinebreak(*c))
		{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            if (_agentVisibleLinesEnabled) agentFinishVisibleLine();
#endif
			line++;
			y += font->getCharSize(*c).h + _extraLineSpacing;
			x = getLineX(line);
			if (*c == Unicode::TOK_NL_SMALL)
			{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
                if (headingContent) agentCaptureErrorHeading();
                headingContent = false;
#endif
				font = _small;
			}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            if (_agentVisibleLinesEnabled) agentStartVisibleLine(y, font->getHeight() + font->getSpacing());
#endif
		}
		else if (*c == Unicode::TOK_COLOR_FLIP)
		{
			color = (color == _color ? _color2 : _color);
		}
		else
		{
			if (dir < 0)
				x += dir * font->getCharSize(*c).w;
			Surface* chr = font->getChar(*c);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            // Font::getChar substitutes '?' for a missing glyph. Never expose
            // the underlying original codepoint as if the player saw it.
            if (!font->agentHasGlyph(*c))
            {
                _agentTextClipped = true;
                if (headingContent) _agentHeadingComplete = false;
                if (listContent) _agentListComplete = false;
            }
#endif
			chr->setX(x);
			chr->setY(y);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            // Bounds come from this actual font's nontransparent glyph ink,
            // not its padded atlas cell. Keep fallback ink for occlusion while
            // the original missing codepoint remains incomplete/unavailable.
            SDL_Rect ink;
            const UCode painted = font->agentHasGlyph(*c) ? *c : '?';
            if (!font->agentGlyphInk(painted, ink))
            {
                _agentTextClipped = true;
                if (headingContent) _agentHeadingComplete = false;
                if (listContent) _agentListComplete = false;
            }
            else
            {
                const int inkX=x+ink.x, inkY=y+ink.y;
                if (inkX < 0 || inkY < 0 || inkX+ink.w > getWidth() || inkY+ink.h > getHeight())
                {
                    _agentTextClipped=true;
                    if (headingContent) _agentHeadingComplete=false;
                    if (listContent) _agentListComplete=false;
                }
                if (headingContent)
                {
                    if (!_agentHeadingHasGlyph) _agentHeadingInk = SDL_Rect{Sint16(inkX), Sint16(inkY), ink.w, ink.h};
                    else
                    {
                        const int right=std::max(int(_agentHeadingInk.x+_agentHeadingInk.w),inkX+ink.w);
                        const int bottom=std::max(int(_agentHeadingInk.y+_agentHeadingInk.h),inkY+ink.h);
                        _agentHeadingInk.x=std::min(int(_agentHeadingInk.x),inkX);
                        _agentHeadingInk.y=std::min(int(_agentHeadingInk.y),inkY);
                        _agentHeadingInk.w=right-_agentHeadingInk.x; _agentHeadingInk.h=bottom-_agentHeadingInk.y;
                    }
                    _agentHeadingHasGlyph=true;
                }
                if (listContent)
                {
                    if (!_agentListHasGlyph)
                    {
                        _agentListInk.x=inkX; _agentListInk.y=inkY;
                        _agentListInk.w=ink.w; _agentListInk.h=ink.h;
                    }
                    else
                    {
                        const int right=std::max(_agentListInk.x+_agentListInk.w,inkX+ink.w);
                        const int bottom=std::max(_agentListInk.y+_agentListInk.h,inkY+ink.h);
                        _agentListInk.x=std::min((int)_agentListInk.x,inkX);
                        _agentListInk.y=std::min((int)_agentListInk.y,inkY);
                        _agentListInk.w=right-_agentListInk.x; _agentListInk.h=bottom-_agentListInk.y;
                    }
                    _agentListHasGlyph=true;
                }
                const int left=std::max(0,inkX), top=std::max(0,inkY);
                const int right=std::min(getWidth(),inkX+ink.w), bottom=std::min(getHeight(),inkY+ink.h);
                if (left < right && top < bottom)
                {
                    if (!_agentHasGlyph) { _agentInkX1=left; _agentInkY1=top; _agentInkX2=right; _agentInkY2=bottom; }
                    else { _agentInkX1=std::min(_agentInkX1,left); _agentInkY1=std::min(_agentInkY1,top);
                        _agentInkX2=std::max(_agentInkX2,right); _agentInkY2=std::max(_agentInkY2,bottom); }
                    _agentHasGlyph=true;
                }
            }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
            if (_agentVisibleLinesEnabled) agentCaptureVisibleGlyph(*c, font, chr, x, y, color, mul, mid);
#endif
			ShaderDraw<PaletteShift>(ShaderSurface(this, 0, 0), ShaderCrop(chr), ShaderScalar(color), ShaderScalar(mul), ShaderScalar(mid));
			if (dir > 0)
				x += dir * font->getCharSize(*c).w;
		}
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    if (_agentVisibleLinesEnabled) { agentFinishVisibleLine(); agentFinishVisibleLines(); }
#endif
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void Text::agentCaptureErrorHeading()
{
    if (!_agentHeadingComplete || !_agentHeadingHasGlyph || getWidth() > 640 || getHeight() > 400 ||
        _agentHeadingInk.x < 0 || _agentHeadingInk.y < 0 || !_agentHeadingInk.w || !_agentHeadingInk.h ||
        _agentHeadingInk.x+_agentHeadingInk.w > getWidth() || _agentHeadingInk.y+_agentHeadingInk.h > getHeight() ||
        size_t(_agentHeadingInk.w)*_agentHeadingInk.h > 65536 || !_surface || !_surface->format ||
        _surface->format->BitsPerPixel != 8 || !(_surface->flags & SDL_SRCCOLORKEY) ||
        _surface->format->colorkey != 0 || (_surface->flags & SDL_SRCALPHA) ||
        _surface->clip_rect.x || _surface->clip_rect.y || _surface->clip_rect.w != getWidth() ||
        _surface->clip_rect.h != getHeight())
    { _agentHeadingComplete=false; return; }
    if (Observer::agentObservePaint(SDL_LockSurface(_surface)) < 0)
    { _agentHeadingComplete=false; return; }
    // Read only the just-painted typed heading rectangle. Its provenance is
    // the glyph loop above; this is not a screen text detector/classifier.
    for (int y=_agentHeadingInk.y; y<_agentHeadingInk.y+_agentHeadingInk.h; ++y)
        for (int x=_agentHeadingInk.x; x<_agentHeadingInk.x+_agentHeadingInk.w; ++x)
            if (const Uint8 value=getPixel(x,y))
            {
                if (_agentHeadingPixels.size() == 8192) { _agentHeadingComplete=false; break; }
                _agentHeadingPixels.push_back({Sint16(x),Sint16(y),value});
            }
    SDL_UnlockSurface(_surface);
    if (_agentHeadingPixels.empty()) _agentHeadingComplete=false;
}

bool Text::agentErrorHeading(std::string &text, SDL_Rect &ink) const
{
    if (!_agentErrorHeading || !_agentHeadingComplete || !_agentHeadingHasGlyph || _agentHeadingPixels.empty() ||
        !agentContextVisible() || !Observer::agentPaintHealthy() || _agentHeadingText.empty() ||
        _agentHeadingText.size()>1024 || !_surface || !_surface->format || _surface->format->BitsPerPixel!=8 ||
        !(_surface->flags & SDL_SRCCOLORKEY) || _surface->format->colorkey!=0 || (_surface->flags & SDL_SRCALPHA) ||
        _surface->clip_rect.x || _surface->clip_rect.y || _surface->clip_rect.w!=getWidth() ||
        _surface->clip_rect.h!=getHeight()) return false;
    if (Observer::agentObservePaint(SDL_LockSurface(_surface)) < 0) return false;
    bool unchanged=true;
    for (const AgentHeadingPixel &pixel : _agentHeadingPixels)
        if (getPixel(pixel.x,pixel.y)!=pixel.value) { unchanged=false; break; }
    SDL_UnlockSurface(_surface);
    if (!unchanged) return false;
    text=_agentHeadingText; ink=_agentHeadingInk; ink.x+=getX(); ink.y+=getY();
    return true;
}

void Text::agentMarkListContent(size_t start, size_t length)
{
    _agentListRange=true; _agentListComplete=false;
    _agentListStart=start; _agentListLength=length;
    _redraw=true;
}

bool Text::agentListContent(std::string &text, SDL_Rect &ink, bool &hasInk) const
{
    if (!_agentListRange || !_agentListComplete || !agentContextVisible() || _scroll || _wrap ||
        _agentListShown.size() != _agentListLength) return false;
    text=Unicode::convUtf32ToUtf8(_agentListShown);
    ink=_agentListInk; ink.x+=getX(); ink.y+=getY(); hasInk=_agentListHasGlyph;
    return true;
}

bool Text::agentPaintedBounds(SDL_Rect &rect) const
{
    if (!_agentHasGlyph || !agentContextVisible()) return false;
    rect.x = getX() + _agentInkX1; rect.y = getY() + _agentInkY1;
    rect.w = _agentInkX2 - _agentInkX1; rect.h = _agentInkY2 - _agentInkY1;
    return true;
}
#endif

/**
 * Add vertical breathing room without changing shared font metrics or glyphs.
 * Article bodies opt in; unrelated UI retains its legacy raster layout.
 */
void Text::setExtraLineSpacing(int pixels)
{
    pixels = std::max(0, pixels);
    if (_extraLineSpacing == pixels) return;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
    _extraLineSpacing = pixels;
    processText();
}

/**
 * Allows the text to be scrollable via mouse wheel.
 */
void Text::setScrollable(bool scroll)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_scroll = scroll;
}

/**
 * Handles scrolling.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void Text::mousePress(Action* action, State* state)
{
	InteractiveSurface::mousePress(action, state);
	if (_scroll &&
		(action->getDetails()->button.button == SDL_BUTTON_WHEELUP ||
		action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN))
	{
        scrollLine(action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN);
	}
}

void Text::scrollLine(bool down)
{
    if (!_scroll) return;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
    int scrollArea = getHeight() - getTextHeight();
    if (scrollArea < 0)
    {
        int scrollAmount = _font->getHeight() + _font->getSpacing() + _extraLineSpacing;
        if (down) scrollAmount = -scrollAmount;
        _scrollY = Clamp(_scrollY + scrollAmount, scrollArea, 0);
        _redraw = true;
    }
}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace {
bool agentArticlePixels(const SDL_Surface *p, bool destination = false)
{
    return p && p->format && p->w > 0 && p->h > 0 && p->w <= 640 && p->h <= 400 &&
        p->format->BytesPerPixel == 1 && p->format->palette && p->format->palette->colors &&
        p->format->palette->ncolors == 256 &&
        ((p->flags & SDL_SRCCOLORKEY) ? p->format->colorkey == 0 : destination) &&
        !(p->flags & SDL_SRCALPHA) &&
        p->clip_rect.x == 0 && p->clip_rect.y == 0 && p->clip_rect.w == p->w && p->clip_rect.h == p->h;
}
bool agentArticleColors(const SDL_Color *a, const SDL_Color *b)
{
    for (unsigned i=0; i<256; ++i)
        if (a[i].r != b[i].r || a[i].g != b[i].g || a[i].b != b[i].b) return false;
    return true;
}
}
void Text::agentEnableVisibleLines()
{
    if (_agentVisibleLinesEnabled) return;
    agentTrackPresentation();
    _agentVisibleLinesEnabled = true;
    agentPresentationMutation();
    _redraw = true;
}
void Text::agentEnableViewportLines()
{
    agentEnableVisibleLines();
    _agentViewportMode = true;
    _redraw = true;
}
void Text::agentBeginVisibleLines()
{
    if (!_agentVisibleLinesEnabled) return;
    _agentLineBlitValid = false; _agentLineDestination = nullptr;
    _agentVisibleLines.clear(); _agentCurrentLine = AgentVisibleLine();
    _agentLineWork = _agentLinePixelCount = 0;
    _agentLineCandidate = _agentLinePendingSpace = false;
    _agentLinesDrawing = true;
    _agentLineDrawRevision = agentPresentationRevision();
    if (_agentViewportMode)
    {
        _agentLinesHealthy = _agentLineDrawRevision && _font && _lang && !_crop.w && !_crop.h &&
            getWidth()>0 && getHeight()>0 && getWidth()<=640 && getHeight()<=400 && Observer::agentPaintHealthy();
        return; // no glyph ownership, palette snapshots, or raster reads
    }
    _agentLinesHealthy = _agentLineDrawRevision && _font && _lang &&
        agentArticlePixels(_surface) && !_crop.w && !_crop.h && Observer::agentPaintHealthy();
    _agentLineOwners.clear();
    if (!_agentLinesHealthy) return;
    _agentLineOwners.assign(getWidth()*getHeight(), -1);
    std::copy(_surface->format->palette->colors, _surface->format->palette->colors+256, _agentLinePalette);
}
void Text::agentStartVisibleLine(int y, int height)
{
    _agentCurrentLine = AgentVisibleLine(); _agentLinePendingSpace = false;
    _agentCurrentLine.layout = SDL_Rect{0, Sint16(std::max(0,y)), Uint16(_agentViewportMode?0:getWidth()), Uint16(std::max(0,std::min(getHeight(),y+height)-std::max(0,y)))};
    _agentLineCandidate = _agentLinesHealthy && height > 0 && y < getHeight() && y+height > 0;
    if (_agentLineCandidate && (y < 0 || y+height > getHeight())) _agentCurrentLine.complete = false;
}
void Text::agentCaptureVisibleGlyph(UCode code, Font *font, Surface *glyph, int x, int y, int color, int mul, int mid)
{
    if (!_agentLinesHealthy || !_agentLineCandidate) return;
    AgentVisibleLine &line = _agentCurrentLine;
    if (_agentViewportMode && !font->agentHasGlyph(code)) code = '?';
    if (code < 32 || code == 127 || !font->agentHasGlyph(code)) line.complete = false;
    if (_agentLinePendingSpace && !line.text.empty() && line.text.size() <= 1024) line.text += ' ';
    _agentLinePendingSpace = false;
    if (line.text.size() <= 1024 && code >= 32 && code != 127)
        line.text += Unicode::convUtf32ToUtf8(UString(1, code));
    if (line.text.size() > 1024) line.complete = false;
    if (_agentViewportMode)
    {
        const int right=x+font->getCharSize(code).w;
        if (x<0 || right>getWidth()) line.complete=false;
        if (!line.glyphs) {line.layout.x=Sint16(std::max(0,x));line.layout.w=Uint16(std::max(0,std::min(getWidth(),right)-line.layout.x));}
        else {const int left=std::min(int(line.layout.x),std::max(0,x));const int end=std::max(int(line.layout.x+line.layout.w),std::min(getWidth(),right));line.layout.x=Sint16(left);line.layout.w=Uint16(std::max(0,end-left));}
        ++line.glyphs;
        return; // UI layout text is independent of glyph readability
    }
    const unsigned glyphId = line.glyphs++;
    const SDL_Rect crop = *glyph->getCrop();
    SDL_Surface *source = glyph->getSurface();
    if (!source || !source->format || source->format->BytesPerPixel != 1 || !crop.w || !crop.h ||
        crop.x < 0 || crop.y < 0 || crop.x+crop.w > source->w || crop.y+crop.h > source->h ||
        size_t(crop.w)*crop.h > 2097152-std::min(size_t(2097152),_agentLineWork))
    { _agentLinesHealthy = false; return; }
    _agentLineWork += size_t(crop.w)*crop.h;
    if (Observer::agentObservePaint(SDL_LockSurface(source)) < 0) { _agentLinesHealthy=false; return; }
    bool any = false;
    for (int sy=0; sy<crop.h; ++sy)
    for (int sx=0; sx<crop.w; ++sx)
    {
        const Uint8 value = *(static_cast<const Uint8 *>(source->pixels)+(crop.y+sy)*source->pitch+crop.x+sx);
        if (!value) continue;
        any = true;
        const int dx=x+sx, dy=y+sy;
        const Uint8 output = color + value*mul + (mid ? 2*(mid-value) : 0);
        if (dx<0 || dy<0 || dx>=getWidth() || dy>=getHeight() || !output)
        { line.complete=false; continue; }
        if (++_agentLinePixelCount > 65536) { _agentLinesHealthy=false; break; }
        const size_t index=dy*getWidth()+dx;
        const int prior=_agentLineOwners[index];
        if (prior>=0 && size_t(prior)!=_agentVisibleLines.size())
        {
            line.complete=false;
            if (size_t(prior)<_agentVisibleLines.size()) _agentVisibleLines[prior].complete=false;
        }
        _agentLineOwners[index]=int(_agentVisibleLines.size());
        line.pixels.push_back({Sint16(dx),Sint16(dy),output,glyphId,false});
    }
    SDL_UnlockSurface(source);
    if (!any) line.complete=false;
}
void Text::agentFinishVisibleLine()
{
    if (!_agentLinesHealthy || !_agentLineCandidate) return;
    if (_agentViewportMode)
    {
        if (!_agentCurrentLine.text.empty())
        {
            if (_agentVisibleLines.size()>=32) { _agentLinesHealthy=false; return; }
            _agentVisibleLines.push_back(std::move(_agentCurrentLine));
        }
        _agentLineCandidate=false;
        return;
    }
    if (!_agentCurrentLine.pixels.empty())
    {
        if (_agentVisibleLines.size() >= 32) { _agentLinesHealthy=false; return; }
        // Ordinary kerning/shadows compose inside one typed visual line.
        // Retain the final owner at each pixel, never a guessed glyph. Every
        // nonempty glyph must retain a contribution after this composition.
        const size_t area=size_t(getWidth())*getHeight();
        if (area>2097152-std::min(size_t(2097152),_agentLineWork))
        { _agentLinesHealthy=false;return; }
        _agentLineWork+=area;
        std::vector<AgentVisiblePixel> composed;
        std::vector<unsigned char> seen(area,0);
        std::vector<bool> glyphs(_agentCurrentLine.glyphs,false);
        for (auto it=_agentCurrentLine.pixels.rbegin();it!=_agentCurrentLine.pixels.rend();++it)
        {
            const size_t index=it->y*getWidth()+it->x;
            if (seen[index]) continue;
            seen[index]=1;glyphs[it->glyph]=true;composed.push_back(*it);
        }
        for (bool glyph:glyphs) if (!glyph) _agentCurrentLine.complete=false;
        std::reverse(composed.begin(),composed.end());
        _agentCurrentLine.pixels.swap(composed);
        _agentVisibleLines.push_back(std::move(_agentCurrentLine));
    }
    _agentLineCandidate=false;
}
void Text::agentFinishVisibleLines()
{
    _agentLinesDrawing=false;
    _agentLineOwners.clear();
    if (_agentViewportMode)
    {
        if (_agentLineDrawRevision!=agentPresentationRevision() || !Observer::agentPaintHealthy()) _agentLinesHealthy=false;
        if (!_agentLinesHealthy) _agentVisibleLines.clear();
        return;
    }
    if (!_agentLinesHealthy || _agentLineDrawRevision!=agentPresentationRevision() ||
        !agentArticlePixels(_surface) || _crop.w || _crop.h ||
        !agentArticleColors(_agentLinePalette,_surface->format->palette->colors) || !Observer::agentPaintHealthy())
    { _agentLinesHealthy=false;_agentVisibleLines.clear();return; }
    if (Observer::agentObservePaint(SDL_LockSurface(_surface))<0)
    { _agentLinesHealthy=false;_agentVisibleLines.clear();return; }
    for (AgentVisibleLine &line:_agentVisibleLines)
        for (const AgentVisiblePixel &pixel:line.pixels)
            if (*(static_cast<const Uint8 *>(_surface->pixels)+pixel.y*_surface->pitch+pixel.x)!=pixel.value)
                line.complete=false;
    SDL_UnlockSurface(_surface);
}
bool Text::agentVisibleLineSource() const
{
    return _agentVisibleLinesEnabled && _agentLinesHealthy && !_agentLinesDrawing &&
        agentPresentationRevision() && _agentLineDrawRevision==agentPresentationRevision() &&
        agentContextVisible() && agentArticlePixels(_surface) &&
        agentArticleColors(_agentLinePalette,_surface->format->palette->colors) && Observer::agentPaintHealthy();
}
void Text::blit(Surface *surface)
{
    if (!_agentVisibleLinesEnabled) { Surface::blit(surface); return; }
    if (_agentViewportMode)
    {
        Surface::blit(surface);
        if (_agentLinesHealthy && !_agentLinesDrawing && !_redraw && agentPresentationPaintedTo(surface))
            _agentLineDrawRevision=agentPresentationRevision();
        return;
    }
    _agentLineBlitValid=false;_agentLineDestination=nullptr;
    if (_visible && !_hidden && _redraw) draw();
    // Placement/visibility/crop changes do not redraw the ordinary bitmap.
    // A fresh actual blit may reuse that same completed typed raster; pending
    // Text redraws and unsupported raster/palette changes may not be rebound.
    if (_visible && !_hidden && !_redraw && _agentLinesHealthy && !_agentLinesDrawing &&
        _agentLineDrawRevision && agentPresentationRevision() && agentArticlePixels(_surface) &&
        !_crop.w && !_crop.h && agentArticleColors(_agentLinePalette,_surface->format->palette->colors))
        _agentLineDrawRevision=agentPresentationRevision();
    SDL_Surface *target=surface?surface->getSurface():nullptr;
    const bool qualified=surface && agentVisibleLineSource() && agentArticlePixels(target, true) &&
        getX()>=0 && getY()>=0 && getX()+getWidth()<=target->w && getY()+getHeight()<=target->h &&
        agentArticleColors(_agentLinePalette,target->format->palette->colors);
    if (qualified)
    {
        if (Observer::agentObservePaint(SDL_LockSurface(target))>=0)
        {
            for (AgentVisibleLine &line:_agentVisibleLines)
                for (AgentVisiblePixel &pixel:line.pixels)
                {
                    const Uint8 before=*(static_cast<const Uint8 *>(target->pixels)+(getY()+pixel.y)*target->pitch+getX()+pixel.x);
                    const SDL_Color &a=_agentLinePalette[before], &b=_agentLinePalette[pixel.value];
                    pixel.contributed=a.r!=b.r || a.g!=b.g || a.b!=b.b;
                }
            SDL_UnlockSurface(target);
        }
    }
    Surface::blit(surface); // unchanged ordinary renderer; proof never repairs its input
    if (!qualified || !agentVisibleLineSource() || !agentArticlePixels(target, true) ||
        !agentArticleColors(_agentLinePalette,target->format->palette->colors)) return;
    _agentLineDestination=surface;_agentLineBlitRevision=agentPresentationRevision();_agentLineBlitValid=true;
}
bool Text::agentViewportLines(const Surface *destination, std::vector<AgentVisibleLine> &lines) const
{
    lines.clear();
    if (!_agentViewportMode || !_agentVisibleLinesEnabled || !_agentLinesHealthy || _agentLinesDrawing || _redraw ||
        !_agentLineDrawRevision || _agentLineDrawRevision!=agentPresentationRevision() ||
        !agentPresentationPaintedTo(destination) || _crop.w || _crop.h || !Observer::agentPaintHealthy()) return false;
    const SDL_Surface *target=destination->getSurface();
    if (!target || getX()<0 || getY()<0 || getX()+getWidth()>target->w || getY()+getHeight()>target->h) return false;
    lines=_agentVisibleLines;
    return true;
}
bool Text::agentVisibleLines(const Surface *destination,std::vector<AgentVisibleLine> &lines) const
{
    lines.clear();
    if (_agentViewportMode || !_agentLineBlitValid || destination!=_agentLineDestination ||
        _agentLineBlitRevision!=agentPresentationRevision() || !agentVisibleLineSource()) return false;
    SDL_Surface *target=destination?destination->getSurface():nullptr;
    if (!agentArticlePixels(target, true) || getX()<0 || getY()<0 || getX()+getWidth()>target->w || getY()+getHeight()>target->h ||
        !agentArticleColors(_agentLinePalette,target->format->palette->colors)) return false;
    const int a=Observer::agentObservePaint(SDL_LockSurface(_surface));
    const int b=Observer::agentObservePaint(SDL_LockSurface(target));
    if (a<0 || b<0) { if(b>=0)SDL_UnlockSurface(target);if(a>=0)SDL_UnlockSurface(_surface);return false; }
    for (AgentVisibleLine line:_agentVisibleLines)
    {
        std::vector<bool> contributed(line.glyphs,false);
        std::vector<AgentVisiblePixel> surviving;
        for (const AgentVisiblePixel &pixel:line.pixels)
        {
            const Uint8 source=*(static_cast<const Uint8 *>(_surface->pixels)+pixel.y*_surface->pitch+pixel.x);
            const Uint8 final=*(static_cast<const Uint8 *>(target->pixels)+(getY()+pixel.y)*target->pitch+getX()+pixel.x);
            if (source!=pixel.value || final!=pixel.value) { line.complete=false;continue; }
            if (pixel.contributed) contributed[pixel.glyph]=true;
            surviving.push_back(pixel);
        }
        line.pixels.swap(surviving);
        bool visible=false;
        for (bool glyph:contributed) { if (!glyph) line.complete=false; else visible=true; }
        if (visible) lines.push_back(std::move(line));
    }
    SDL_UnlockSurface(target);SDL_UnlockSurface(_surface);
    return Observer::agentPaintHealthy();
}
bool Text::agentVisibleLineControl(const Surface *destination) const
{
    if (!_scroll) return false;
    std::vector<AgentVisibleLine> lines;
    return agentVisibleLines(destination,lines) && !lines.empty();
}
#endif

}
