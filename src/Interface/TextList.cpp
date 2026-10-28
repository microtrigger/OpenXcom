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
#include "TextList.h"
#include <cstdarg>
#include <cmath>
#include <algorithm>
#include "../Engine/Action.h"
#include "../Engine/Font.h"
#include "../Engine/Palette.h"
#include "../Engine/Options.h"
#include "ArrowButton.h"
#include "ComboBox.h"
#include "ScrollBar.h"
#include "../fmath.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Engine/Unicode.h"
#endif

namespace OpenXcom
{

/**
 * Sets up a blank list with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
TextList::TextList(int width, int height, int x, int y) : InteractiveSurface(width, height, x, y), _big(0), _small(0), _font(0), _scroll(0), _visibleRows(0), _selRow(0), _color(0), _dot(false), _selectable(false), _condensed(false), _contrast(false), _wrap(false), _flooding(false), _fullRowsOnly(false),
																								   _bg(0), _selector(0), _margin(0), _scrolling(true), _arrowPos(-1), _scrollPos(4), _arrowType(ARROW_VERTICAL),
																								   _leftClick(0), _leftPress(0), _leftRelease(0), _rightClick(0), _rightPress(0), _rightRelease(0), _arrowsLeftEdge(0), _arrowsRightEdge(0), _comboBox(0)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	// Even a never-populated list needs its ordinary first draw before its
	// empty presentation can be certified. Coverage itself stays untrusted.
	_redraw = true;
#endif
	_up = new ArrowButton(ARROW_BIG_UP, 13, 14, getX() + getWidth() + _scrollPos, getY());
	_up->setVisible(false);
	_up->setTextList(this);
	_down = new ArrowButton(ARROW_BIG_DOWN, 13, 14, getX() + getWidth() + _scrollPos, getY() + getHeight() - 14);
	_down->setVisible(false);
	_down->setTextList(this);
	int h = std::max(_down->getY() - _up->getY() - _up->getHeight(), 1);
	_scrollbar = new ScrollBar(_up->getWidth(), h, getX() + getWidth() + _scrollPos, _up->getY() + _up->getHeight());
	_scrollbar->setVisible(false);
	_scrollbar->setTextList(this);
}

/**
 * Deletes all the stuff contained in the list.
 */
TextList::~TextList()
{
	for (std::vector< std::vector<Text*> >::iterator u = _texts.begin(); u < _texts.end(); ++u)
	{
		for (std::vector<Text*>::iterator v = (*u).begin(); v < (*u).end(); ++v)
		{
			delete *v;
		}
	}
	for (std::vector<ArrowButton*>::iterator i = _arrowLeft.begin(); i < _arrowLeft.end(); ++i)
	{
		delete *i;
	}
	for (std::vector<ArrowButton*>::iterator i = _arrowRight.begin(); i < _arrowRight.end(); ++i)
	{
		delete *i;
	}
	delete _selector;
	delete _up;
	delete _down;
	delete _scrollbar;
}

/**
 * Changes the position of the surface in the X axis.
 * @param x X position in pixels.
 */
void TextList::setX(int x)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (x != getX()) { ++_agentListRevision; _redraw = true; }
#endif
	Surface::setX(x);
	_up->setX(getX() + getWidth() + _scrollPos);
	_down->setX(getX() + getWidth() + _scrollPos);
	_scrollbar->setX(getX() + getWidth() + _scrollPos);
	if (_selector != 0)
		_selector->setX(getX());
}

/**
 * Changes the position of the surface in the Y axis.
 * @param y Y position in pixels.
 */
void TextList::setY(int y)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (y != getY()) { ++_agentListRevision; _redraw = true; }
#endif
	Surface::setY(y);
	_up->setY(getY());
	_down->setY(getY() + getHeight() - 14);
	_scrollbar->setY(_up->getY() + _up->getHeight());
	if (_selector != 0)
		_selector->setY(getY());
}

/**
 * Gets the arrowsLeftEdge.
 * @return arrowsLeftEdge.
 */
int TextList::getArrowsLeftEdge()
{
	return _arrowsLeftEdge;
}

/**
 * Gets the arrowsRightEdge.
 * @return arrowsRightEdge.
 */
int TextList::getArrowsRightEdge()
{
	return _arrowsRightEdge;
}

/**
 * Unpresses all the arrow buttons.
 * @param state Pointer to running state.
 */
void TextList::unpress(State *state)
{
	InteractiveSurface::unpress(state);
	for (std::vector<ArrowButton*>::iterator i = _arrowLeft.begin(); i < _arrowLeft.end(); ++i)
	{
		(*i)->unpress(state);
	}
	for (std::vector<ArrowButton*>::iterator i = _arrowRight.begin(); i < _arrowRight.end(); ++i)
	{
		(*i)->unpress(state);
	}
}

/**
 * Changes the color of a specific Text object in the list.
 * @param row Row number.
 * @param column Column number.
 * @param color Text color.
 */
void TextList::setCellColor(size_t row, size_t column, Uint8 color)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (_texts[row][column]->getColor() != color) ++_agentListRevision;
#endif
	_texts[row][column]->setColor(color);
	_redraw = true;
}

/**
 * Changes the text color of a whole row in the list.
 * @param row Row number.
 * @param color Text color.
 */
void TextList::setRowColor(size_t row, Uint8 color)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	for (const Text *cell : _texts[row]) if (cell->getColor() != color) { ++_agentListRevision; break; }
#endif
	for (std::vector<Text*>::iterator i = _texts[row].begin(); i < _texts[row].end(); ++i)
	{
		(*i)->setColor(color);
	}
	_redraw = true;
}

/**
 * Returns the text of a specific Text object in the list.
 * @param row Row number.
 * @param column Column number.
 * @return Text string.
 */
std::string TextList::getCellText(size_t row, size_t column) const
{
	return _texts[row][column]->getText();
}

/**
 * Changes the text of a specific Text object in the list.
 * @param row Row number.
 * @param column Column number.
 * @param text Text string.
 */
void TextList::setCellText(size_t row, size_t column, const std::string &text)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (_texts[row][column]->getText() != text) ++_agentListRevision;
#endif
	_texts[row][column]->setText(text);
	_redraw = true;
}

/**
 * Returns the X position of a specific text column in the list.
 * @param column Column number.
 * @return X position in pixels.
 */
int TextList::getColumnX(size_t column) const
{
	return getX() + _texts[0][column]->getX();
}

/**
 * Returns the Y position of a specific text row in the list.
 * @param row Row number.
 * @return Y position in pixels.
 */
int TextList::getRowY(size_t row) const
{
	return getY() + _texts[row][0]->getY();
}

/**
 * Returns the height of a specific text row in the list.
 * @param row Row number.
 * @return height in pixels.
 */
int TextList::getTextHeight(size_t row) const
{
	return _texts[row].front()->getTextHeight();
}

/**
 * Returns the height of a specific text row in the list.
 * @param row Row number.
 * @return height in pixels.
 */
int TextList::getNumTextLines(size_t row) const
{
	return _texts[row].front()->getNumLines();
}

/**
 * Returns the amount of text rows stored in the list.
 * @return Number of rows.
 */
size_t TextList::getTexts() const
{
	return _texts.size();
}

/**
 * Returns the amount of physical rows stored in the list.
 * @return Number of rows.
 */
size_t TextList::getRows() const
{
	return _rows.size();
}

/**
 * Returns the amount of visible rows stored in the list.
 * @return Number of rows.
 */
size_t TextList::getVisibleRows() const
{
	return _visibleRows;
}

/**
 * Adds a new row of text to the list, automatically creating
 * the required Text objects lined up where they need to be.
 * @param cols Number of columns.
 * @param ... Text for each cell in the new row.
 */
void TextList::addRow(int cols, ...)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	++_agentListRevision;
#endif
	va_list args;
	int ncols;
	va_start(args, cols);
	if (cols > 0)
	{
		ncols = cols;
	}
	else
	{
		ncols = 1;
	}

	std::vector<Text*> temp;
	// Positions are relative to list surface.
	int rowX = 0, rowY = 0, rows = 1, rowHeight = 0;
	if (!_texts.empty())
	{
		rowY = _texts.back().front()->getY() + _texts.back().front()->getHeight() + _font->getSpacing();
	}

	for (int i = 0; i < ncols; ++i)
	{
		int width;
		// Place text
		if (_flooding)
		{
			width = 340;
		}
		else
		{
			width = _columns[i];
		}
		Text* txt = new Text(width, _font->getHeight(), _margin + rowX, rowY);
		txt->setPalette(this->getPalette());
		txt->initText(_big, _small, _lang);
		txt->setColor(_color);
		txt->setSecondaryColor(_color2);
		if (_align[i])
		{
			txt->setAlign(_align[i]);
		}
		txt->setHighContrast(_contrast);
		if (_font == _big)
		{
			txt->setBig();
		}
		else
		{
			txt->setSmall();
		}
		if (cols > 0)
			txt->setText(va_arg(args, char*));
		// grab this before we enable word wrapping so we can use it to calculate
		// the total row height below
		int vmargin = _font->getHeight() - txt->getTextHeight();
		// Wordwrap text if necessary
		if (_wrap && txt->getTextWidth() > txt->getWidth())
		{
			txt->setWordWrap(true, true);
			rows = std::max(rows, txt->getNumLines());
		}
		rowHeight = std::max(rowHeight, txt->getTextHeight() + vmargin);

		// Places dots between text
		if (_dot && i < cols - 1)
		{
			std::string buf = txt->getText();
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
			const size_t contentLength = Unicode::convUtf8ToUtf32(buf).size();
			size_t contentStart = 0;
#endif
			unsigned int w = txt->getTextWidth();
			while (w < _columns[i])
			{
				if (_align[i] != ALIGN_RIGHT)
				{
				w += _font->getChar('.')->getCrop()->w + _font->getSpacing();
				buf += '.';
			}
				if (_align[i] != ALIGN_LEFT)
				{
					w += _font->getChar('.')->getCrop()->w + _font->getSpacing();
					buf.insert(0, 1, '.');
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
					++contentStart;
#endif
				}
			}
			txt->setText(buf);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
			txt->agentMarkListContent(contentStart, contentLength);
#endif
		}

		temp.push_back(txt);
		if (_condensed)
		{
			rowX += txt->getTextWidth();
		}
		else
		{
			rowX += _columns[i];
		}
	}

	// ensure all elements in this row are the same height
	for (int i = 0; i < cols; ++i)
	{
		temp[i]->setHeight(rowHeight);
	}

	_texts.push_back(temp);
	for (int i = 0; i < rows; ++i)
	{
		_rows.push_back(_texts.size() - 1);
	}

	// Place arrow buttons
	// Position defined w.r.t. main window, NOT TextList.
	if (_arrowPos != -1)
	{
		ArrowShape shape1, shape2;
		if (_arrowType == ARROW_VERTICAL)
		{
			shape1 = ARROW_SMALL_UP;
			shape2 = ARROW_SMALL_DOWN;
		}
		else
		{
			shape1 = ARROW_SMALL_LEFT;
			shape2 = ARROW_SMALL_RIGHT;
		}
		ArrowButton *a1 = new ArrowButton(shape1, 11, 8, getX() + _arrowPos, getY());
		a1->setListButton();
		a1->setPalette(this->getPalette());
		a1->setColor(_up->getColor());
		a1->onMouseClick(_leftClick, 0);
		a1->onMousePress(_leftPress);
		a1->onMouseRelease(_leftRelease);
		_arrowLeft.push_back(a1);
		ArrowButton *a2 = new ArrowButton(shape2, 11, 8, getX() + _arrowPos + 12, getY());
		a2->setListButton();
		a2->setPalette(this->getPalette());
		a2->setColor(_up->getColor());
		a2->onMouseClick(_rightClick, 0);
		a2->onMousePress(_rightPress);
		a2->onMouseRelease(_rightRelease);
		_arrowRight.push_back(a2);
	}

	_redraw = true;
	va_end(args);
	updateArrows();
}

/**
 * Changes the columns that the list contains.
 * While rows can be unlimited, columns need to be specified
 * since they can have various widths for lining up the text.
 * @param cols Number of columns.
 * @param ... Width of each column.
 */
void TextList::setColumns(int cols, ...)
{
	va_list args;
	va_start(args, cols);

	for (int i = 0; i < cols; ++i)
	{
		_columns.push_back(va_arg(args, int));
	}

	va_end(args);
}

/**
 * Replaces a certain amount of colors in the palette of all
 * the text contained in the list.
 * @param colors Pointer to the set of colors.
 * @param firstcolor Offset of the first color to replace.
 * @param ncolors Amount of colors to replace.
 */
void TextList::setPalette(SDL_Color *colors, int firstcolor, int ncolors)
{
	Surface::setPalette(colors, firstcolor, ncolors);
	for (std::vector< std::vector<Text*> >::iterator u = _texts.begin(); u < _texts.end(); ++u)
	{
		for (std::vector<Text*>::iterator v = u->begin(); v < u->end(); ++v)
		{
			(*v)->setPalette(colors, firstcolor, ncolors);
		}
	}
	for (std::vector<ArrowButton*>::iterator i = _arrowLeft.begin(); i < _arrowLeft.end(); ++i)
	{
		(*i)->setPalette(colors, firstcolor, ncolors);
	}
	for (std::vector<ArrowButton*>::iterator i = _arrowRight.begin(); i < _arrowRight.end(); ++i)
	{
		(*i)->setPalette(colors, firstcolor, ncolors);
	}
	if (_selector != 0)
	{
		_selector->setPalette(colors, firstcolor, ncolors);
	}
	_up->setPalette(colors, firstcolor, ncolors);
	_down->setPalette(colors, firstcolor, ncolors);
	_scrollbar->setPalette(colors, firstcolor, ncolors);
}

/**
 * Changes the resources for the text in the list
 * and calculates the selector and visible amount of rows.
 * @param big Pointer to large-size font.
 * @param small Pointer to small-size font.
 * @param lang Pointer to current language.
 */
void TextList::initText(Font *big, Font *small, Language *lang)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	++_agentListRevision;
#endif
	_big = big;
	_small = small;
	_font = small;
	_lang = lang;

	delete _selector;
	_selector = new Surface(getWidth(), _font->getHeight() + _font->getSpacing(), getX(), getY());
	_selector->setPalette(getPalette());
	_selector->setVisible(false);

	updateVisible();

}

/**
 * Changes the height of the text list.
 * @param height New height in pixels.
 */
void TextList::setHeight(int height)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (height != getHeight()) { ++_agentListRevision; }
#endif
	Surface::setHeight(height);
	setY(getY());
	int h = std::max(_down->getY() - _up->getY() - _up->getHeight(), 1);
	_scrollbar->setHeight(h);
	updateVisible();
}

/**
 * Opt-in ordinary layout policy for lists whose last glyph row must be
 * scrollable fully into view. Other lists retain the historical policy.
 */
void TextList::setFullRowsOnly(bool fullRowsOnly)
{
	if (_fullRowsOnly == fullRowsOnly) return;
	_fullRowsOnly = fullRowsOnly;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	++_agentListRevision;
#endif
	if (_font)
	{
		updateVisible();
		_scroll = _visibleRows && _rows.size() > _visibleRows ? std::min(_scroll, _rows.size() - _visibleRows) : 0;
		updateArrows();
	}
	_redraw = true;
}

/**
 * Changes the color of the text in the list. This doesn't change
 * the color of existing text, just the color of text added from then on.
 * @param color Color value.
 */
void TextList::setColor(Uint8 color)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (color != _color) ++_agentListRevision;
#endif
	_color = color;
	_up->setColor(color);
	_down->setColor(color);
	_scrollbar->setColor(color);
	for (std::vector< std::vector<Text*> >::iterator u = _texts.begin(); u < _texts.end(); ++u)
	{
		for (std::vector<Text*>::iterator v = u->begin(); v < u->end(); ++v)
		{
			(*v)->setColor(color);
		}
	}
}

/**
 * Returns the color of the text in the list.
 * @return Color value.
 */
Uint8 TextList::getColor() const
{
	return _color;
}

/**
 * Changes the secondary color of the text in the list.
 * @param color Color value.
 */
void TextList::setSecondaryColor(Uint8 color)
{
	_color2 = color;
}

/**
 * Returns the secondary color of the text in the list.
 * @return Color value.
 */
Uint8 TextList::getSecondaryColor() const
{
	return _color2;
}

/**
 * Enables/disables text wordwrapping. When enabled, rows can
 * take up multiple lines of the list, otherwise every row
 * is restricted to one line.
 * @param wrap Wordwrapping setting.
 */
void TextList::setWordWrap(bool wrap)
{
	_wrap = wrap;
}

/**
 * Enables/disables high contrast color. Mostly used for
 * Battlescape text.
 * @param contrast High contrast setting.
 */
void TextList::setHighContrast(bool contrast)
{
	_contrast = contrast;
	for (std::vector< std::vector<Text*> >::iterator u = _texts.begin(); u < _texts.end(); ++u)
	{
		for (std::vector<Text*>::iterator v = u->begin(); v < u->end(); ++v)
		{
			(*v)->setHighContrast(contrast);
		}
	}
	_scrollbar->setHighContrast(contrast);
}

/**
 * Changes the horizontal alignment of the text in the list. This doesn't change
 * the alignment of existing text, just the alignment of text added from then on.
 * @param align Horizontal alignment.
 * @param col the column to set the alignment for (defaults to -1, meaning "all")
 */
void TextList::setAlign(TextHAlign align, int col)
{
	if (col == -1)
	{
		for (size_t i = 0; i < _columns.size(); ++i)
		{
			_align[i] = align;
		}
	}
	else
	{
		_align[col] = align;
	}
}

/**
 * If enabled, the text in different columns will be separated by dots.
 * Otherwise, it will only be separated by blank space.
 * @param dot True for dots, False for spaces.
 */
void TextList::setDot(bool dot)
{
	_dot = dot;
}

/**
 * If enabled, the list will respond to player input,
 * highlighting selected rows and receiving clicks.
 * @param selectable Selectable setting.
 */
void TextList::setSelectable(bool selectable)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (selectable != _selectable) ++_agentListRevision;
#endif
	_selectable = selectable;
}

/**
 * Changes the text list to use the big-size font.
 */
void TextList::setBig()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (_font != _big) { ++_agentListRevision; }
#endif
	_font = _big;

	delete _selector;
	_selector = new Surface(getWidth(), _font->getHeight() + _font->getSpacing(), getX(), getY());
	_selector->setPalette(getPalette());
	_selector->setVisible(false);

	updateVisible();
}

/**
 * Changes the text list to use the small-size font.
 */
void TextList::setSmall()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (_font != _small) { ++_agentListRevision; }
#endif
	_font = _small;

	delete _selector;
	_selector = new Surface(getWidth(), _font->getHeight() + _font->getSpacing(), getX(), getY());
	_selector->setPalette(getPalette());
	_selector->setVisible(false);

	updateVisible();
}

/**
 * If enabled, the columns will match the text width.
 * Otherwise, they will have a fixed width.
 * @param condensed True for condensed layout, False for table layout.
 */
void TextList::setCondensed(bool condensed)
{
	_condensed = condensed;
}

/**
 * Returns the currently selected row if the text
 * list is selectable.
 * @return Selected row, -1 if none.
 */
unsigned int TextList::getSelectedRow() const
{
	if (_rows.empty() || _selRow >= _rows.size())
	{
		return -1;
	}
	else
	{
		return _rows[_selRow];
	}
}

/**
 * Changes the surface used to draw the background of the selector.
 * @param bg New background.
 */
void TextList::setBackground(Surface *bg)
{
	_bg = bg;
	_scrollbar->setBackground(_bg);
}

/**
 * Changes the horizontal margin placed around the text.
 * @param margin Margin in pixels.
 */
void TextList::setMargin(int margin)
{
	_margin = margin;
}

/**
 * Returns the margin of the text in the list.
 * @return Margin in pixels.
 */
int TextList::getMargin() const
{
	return _margin;
}

/**
 * Changes the color of the arrow buttons in the list.
 * @param color Color value.
 */
void TextList::setArrowColor(Uint8 color)
{
	_up->setColor(color);
	_down->setColor(color);
	_scrollbar->setColor(color);
}

/**
 * Sets the position of the column of arrow buttons
 * in the text list.
 * @param pos X in pixels (-1 to disable).
 * @param type Arrow orientation type.
 */
void TextList::setArrowColumn(int pos, ArrowOrientation type)
{
	_arrowPos = pos;
	_arrowType = type;
	_arrowsLeftEdge = getX() + _arrowPos;
	_arrowsRightEdge = _arrowsLeftEdge + 12 + 11;
}

/**
 * Sets a function to be called every time the left arrows are mouse clicked.
 * @param handler Action handler.
 */
void TextList::onLeftArrowClick(ActionHandler handler)
{
	_leftClick = handler;
	for (std::vector<ArrowButton*>::iterator i = _arrowLeft.begin(); i < _arrowLeft.end(); ++i)
	{
		(*i)->onMouseClick(handler, 0);
	}
}

/**
 * Sets a function to be called every time the left arrows are mouse pressed.
 * @param handler Action handler.
 */
void TextList::onLeftArrowPress(ActionHandler handler)
{
	_leftPress = handler;
	for (std::vector<ArrowButton*>::iterator i = _arrowLeft.begin(); i < _arrowLeft.end(); ++i)
	{
		(*i)->onMousePress(handler);
	}
}

/**
 * Sets a function to be called every time the left arrows are mouse released.
 * @param handler Action handler.
 */
void TextList::onLeftArrowRelease(ActionHandler handler)
{
	_leftRelease = handler;
	for (std::vector<ArrowButton*>::iterator i = _arrowLeft.begin(); i < _arrowLeft.end(); ++i)
	{
		(*i)->onMouseRelease(handler);
	}
}

/**
 * Sets a function to be called every time the right arrows are mouse clicked.
 * @param handler Action handler.
 */
void TextList::onRightArrowClick(ActionHandler handler)
{
	_rightClick = handler;
	for (std::vector<ArrowButton*>::iterator i = _arrowRight.begin(); i < _arrowRight.end(); ++i)
	{
		(*i)->onMouseClick(handler, 0);
	}
}

/**
 * Sets a function to be called every time the right arrows are mouse pressed.
 * @param handler Action handler.
 */
void TextList::onRightArrowPress(ActionHandler handler)
{
	_rightPress = handler;
	for (std::vector<ArrowButton*>::iterator i = _arrowRight.begin(); i < _arrowRight.end(); ++i)
	{
		(*i)->onMousePress(handler);
	}
}

/**
 * Sets a function to be called every time the right arrows are mouse released.
 * @param handler Action handler.
 */
void TextList::onRightArrowRelease(ActionHandler handler)
{
	_rightRelease = handler;
	for (std::vector<ArrowButton*>::iterator i = _arrowRight.begin(); i < _arrowRight.end(); ++i)
	{
		(*i)->onMouseRelease(handler);
	}
}

/**
 * Removes all the rows currently stored in the list.
 */
void TextList::clearList()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (!_texts.empty()) { ++_agentListRevision; }
#endif
	for (std::vector< std::vector<Text*> >::iterator u = _texts.begin(); u < _texts.end(); ++u)
	{
		for (std::vector<Text*>::iterator v = u->begin(); v < u->end(); ++v)
		{
			delete (*v);
		}
		u->clear();
	}
	scrollUp(true, false);
	_texts.clear();
	_rows.clear();
	_redraw = true;
}

/**
 * Scrolls the text in the list up by one row or to the top.
 * @param toMax If true then scrolls to the top of the list. false => one row up
 * @param scrollByWheel If true then use wheel scroll, otherwise scroll normally.
 */
void TextList::scrollUp(bool toMax, bool scrollByWheel)
{
	if (!_scrolling)
		return;
	if (_rows.size() > _visibleRows && _scroll > 0)
	{
		if (toMax)
		{
			scrollTo(0);
		}
		else
		{
			if (scrollByWheel)
			{
				scrollTo(_scroll - std::min((size_t)(Options::mousewheelSpeed), _scroll));
			}
			else
			{
				scrollTo(_scroll - 1);
			}
		}
	}
}

/**
 * Scrolls the text in the list down by one row or to the bottom.
 * @param toMax If true then scrolls to the bottom of the list. false => one row down
 * @param scrollByWheel If true then use wheel scroll, otherwise scroll normally.
 */
void TextList::scrollDown(bool toMax, bool scrollByWheel)
{
	if (!_scrolling)
		return;
	if (_rows.size() > _visibleRows && _scroll < _rows.size() - _visibleRows)
	{
		if (toMax)
		{
			scrollTo(_rows.size() - _visibleRows);
		}
		else
		{
			if (scrollByWheel)
			{
				scrollTo(_scroll + Options::mousewheelSpeed);
			}
			else
			{
				scrollTo(_scroll + 1);
			}
		}
	}
}

/**
 * Updates the visibility of the arrow buttons according to
 * the current scroll position.
 */
void TextList::updateArrows()
{
	const bool show = (!_fullRowsOnly || _visibleRows != 0) && _rows.size() > _visibleRows;
	_up->setVisible(show);
	_down->setVisible(show);
	_scrollbar->setVisible(show);
	_scrollbar->invalidate();
	_scrollbar->blit(this);
}

/**
 * Updates the amount of visible rows according to the
 * current list and font size.
 */
void TextList::updateVisible()
{
	_visibleRows = 0;
	if (_fullRowsOnly)
	{
		const int step = _font->getHeight() + _font->getSpacing();
		const int lastTop = getHeight() - _font->getHeight();
		if (step > 0 && lastTop >= 0)
			_visibleRows = static_cast<size_t>(lastTop / step) + 1;
		_scroll = _visibleRows && _rows.size() > _visibleRows ? std::min(_scroll, _rows.size() - _visibleRows) : 0;
	}
	else
	{
		for (int y = 0; y < getHeight(); y += _font->getHeight() + _font->getSpacing())
		{
			_visibleRows++;
		}
	}
	updateArrows();
}

/**
 * Changes whether the list can be scrolled.
 * @param scrolling True to allow scrolling, false otherwise.
 * @param scrollPos Custom X position for the scroll buttons.
 */
void TextList::setScrolling(bool scrolling, int scrollPos)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (scrolling != _scrolling || scrollPos != _scrollPos) { ++_agentListRevision; }
#endif
	_scrolling = scrolling;
	if (scrollPos != _scrollPos)
	{
		_scrollPos = scrollPos;
		_up->setX(getX() + getWidth() + _scrollPos);
		_down->setX(getX() + getWidth() + _scrollPos);
		_scrollbar->setX(getX() + getWidth() + _scrollPos);
	}
}

/**
 * Draws the text list and all the text contained within.
 */
void TextList::draw()
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	_agentDrawRows.clear();
	_agentDrawPsiRows.clear();
	_agentDrawCommendationRows.clear();
	_agentCommendationCaptureHealthy = true;
	_agentDrawArrivalLogicalRows.clear();
	_agentArrivalLogicalCaptureHealthy = !_agentArrivalLogicalEpochExhausted;
	if (_agentArrivalLogicalCells)
	{
		if (_agentArrivalLogicalDrawEpoch == UINT64_MAX)
		{ _agentArrivalLogicalEpochExhausted = true; _agentArrivalLogicalCaptureHealthy = false; }
		else ++_agentArrivalLogicalDrawEpoch;
	}
	_agentDrawSaveListRows.clear();
	_agentSaveListCaptureHealthy = true;
	_agentDrawReportRows.clear();
	_agentReportCaptureHealthy = !_agentReportEpochExhausted;
	if (_agentReportCells)
	{
		if (_agentReportDrawEpoch == UINT64_MAX) { _agentReportEpochExhausted = true; _agentReportCaptureHealthy = false; }
		else ++_agentReportDrawEpoch;
	}
	_agentDrawPromotionRows.clear();
	_agentPromotionCaptureHealthy = true;
	_agentDrawControls.clear();
	_agentDrawPurchaseRows.clear();
	_agentDrawArrivalRows.clear(); _agentDrawOutfittingRows.clear();
	_agentCoverageHealthy = true;
	_agentCoverageWork = _agentCoverageRuns = 0;
#endif
	Surface::draw();
	int y = 0;
	if (!_rows.empty())
	{
		// for wrapped items, offset the draw height above the visible surface
		// so that the correct row appears at the top
		for (int row = _scroll; row > 0 && _rows[row] == _rows[row - 1]; --row)
		{
			y -= _font->getHeight() + _font->getSpacing();
		}
		for (size_t i = _rows[_scroll]; i < _texts.size() && i < _rows[_scroll] + _visibleRows; ++i)
		{
			for (std::vector<Text*>::iterator j = _texts[i].begin(); j < _texts[i].end(); ++j)
			{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
				if (_agentPsiTrainingCells || _agentCommendationCells || _agentPromotionCells || _agentReportCells || _agentSaveListCells || _agentArrivalLogicalCells || _agentOutfittingColumns==1) (*j)->agentTrackPresentation();
#endif
				(*j)->setY(y);
				(*j)->blit(this);
			}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
			agentCapturePsiRow(i, y);
			agentCaptureCommendationRow(i, y);
			agentCapturePromotionRow(i, y);
			agentCaptureReportRow(i, y);
			agentCaptureSaveListRow(i, y);
			agentCaptureArrivalLogicalRow(i, y);
			agentCaptureRow(i, y);
#endif
			if (!_texts[i].empty())
			{
				y += _texts[i].front()->getHeight() + _font->getSpacing();
			}
			else
			{
				y += _font->getHeight() + _font->getSpacing();
			}
		}
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	// Expected masks are needed only until the last ordinary row write.
	for (AgentPurchaseRow &row : _agentDrawPurchaseRows)
		for (AgentPurchaseCell &cell : row.cells) cell.ink.clear();
	for (AgentArrivalRow &row : _agentDrawArrivalRows)
		for (AgentPurchaseCell &cell : row.cells) cell.ink.clear();
	for (AgentOutfittingRow &row : _agentDrawOutfittingRows)
		for (AgentPurchaseCell &cell : row.cells) cell.ink.clear();
#endif
}

/**
 * Blits the text list and selector.
 * @param surface Pointer to surface to blit onto.
 */
void TextList::blit(Surface *surface)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	_agentPaintOrder.clear();
	_agentPaintRows.clear();
	_agentPaintPsiRows.clear();
	_agentPaintCommendationRows.clear();
	_agentCommendationClip = surface->getSurface()->clip_rect;
	_agentCommendationDestination = surface;
	_agentPaintArrivalLogicalRows.clear();
	_agentArrivalLogicalPaintEpoch = 0;
	_agentArrivalLogicalClip = surface->getSurface()->clip_rect;
	_agentArrivalLogicalDestination = surface;
	_agentPaintSaveListRows.clear();
	_agentSaveListClip = surface->getSurface()->clip_rect;
	_agentSaveListDestination = surface;
	_agentPaintReportRows.clear();
	_agentReportPaintEpoch = 0;
	_agentReportClip = surface->getSurface()->clip_rect;
	_agentReportDestination = surface;
	_agentPaintPromotionRows.clear();
	_agentPromotionClip = surface->getSurface()->clip_rect;
	_agentPromotionDestination = surface;
	_agentPaintControls.clear();
	_agentPaintPurchaseRows.clear();
	_agentPaintArrivalRows.clear(); _agentPaintOutfittingRows.clear();
	_agentListClip = surface->getSurface()->clip_rect;
	_agentPurchaseClip = surface->getSurface()->clip_rect;
	_agentArrivalClip = surface->getSurface()->clip_rect;
	_agentOutfittingClip = surface->getSurface()->clip_rect;
	_agentPsiClip = surface->getSurface()->clip_rect;
	_agentPaintRevision = UINT64_MAX;
	std::vector<std::pair<int, int> > agentSelectorChanged;
#endif
	if (_visible && !_hidden)
	{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
		// Record an actual visible palette-color change against the surface
		// immediately beneath this selector, before the ordinary selector blit.
		SDL_Surface *selectorPixels = _selector ? _selector->getSurface() : nullptr;
		SDL_Surface *targetPixels = surface->getSurface();
		if (_selector && _selector->agentContextVisible() && selectorPixels->format->BytesPerPixel == 1 &&
			targetPixels->format->BytesPerPixel == 1 && selectorPixels->format->palette && targetPixels->format->palette &&
			_selector->getX() >= targetPixels->clip_rect.x && _selector->getY() >= targetPixels->clip_rect.y &&
			_selector->getX() + _selector->getWidth() <= targetPixels->clip_rect.x + targetPixels->clip_rect.w &&
			_selector->getY() + _selector->getHeight() <= targetPixels->clip_rect.y + targetPixels->clip_rect.h)
		{
			const int selectorLock = Observer::agentObservePaint(SDL_LockSurface(selectorPixels));
			const int targetLock = Observer::agentObservePaint(SDL_LockSurface(targetPixels));
			if (selectorLock >= 0 && targetLock >= 0)
			{
				const SDL_Rect clip = targetPixels->clip_rect;
				for (int y = 0; y < _selector->getHeight(); ++y)
					for (int x = 0; x < _selector->getWidth(); ++x)
					{
						const int targetX = _selector->getX() + x, targetY = _selector->getY() + y;
						const Uint8 pixel = _selector->getPixel(x, y);
						if (!pixel || targetX < clip.x || targetY < clip.y ||
							targetX >= clip.x + clip.w || targetY >= clip.y + clip.h ||
							pixel >= selectorPixels->format->palette->ncolors) continue;
						const SDL_Color sourceColor = selectorPixels->format->palette->colors[pixel];
						const Uint32 mapped = SDL_MapRGB(targetPixels->format, sourceColor.r, sourceColor.g, sourceColor.b);
						const Uint8 beneath = surface->getPixel(targetX, targetY);
						if (mapped >= static_cast<Uint32>(targetPixels->format->palette->ncolors) ||
							beneath >= targetPixels->format->palette->ncolors) continue;
						const SDL_Color after = targetPixels->format->palette->colors[mapped];
						const SDL_Color before = targetPixels->format->palette->colors[beneath];
						if (after.r != before.r || after.g != before.g || after.b != before.b)
							agentSelectorChanged.push_back(std::make_pair(targetX - getX(), targetY - getY()));
					}
			}
			if (targetLock >= 0) SDL_UnlockSurface(targetPixels);
			if (selectorLock >= 0) SDL_UnlockSurface(selectorPixels);
		}
#endif
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
		if (_agentPsiTrainingCells || _agentOutfittingColumns==1) _selector->agentTrackPresentation();
#endif
		_selector->blit(surface);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
		if (_selector->getVisible()) _agentPaintOrder.push_back(_selector);
#endif
	}
	Surface::blit(surface);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	_agentPaintOrder.push_back(this);
#endif
	if (_visible && !_hidden)
	{
		if (_arrowPos != -1 && !_rows.empty())
		{
			int y = getY();
			for (int row = _scroll; row > 0 && _rows[row] == _rows[row - 1]; --row)
			{
				y -= _font->getHeight() + _font->getSpacing();
			}
			int maxY = getY() + getHeight();
			for (size_t i = _rows[_scroll]; i < _texts.size() && i < _rows[_scroll] + _visibleRows && y < maxY; ++i)
			{
				_arrowLeft[i]->setY(y);
				_arrowRight[i]->setY(y);

				if (y >= getY())
				{
					// only blit arrows that belong to texts that have their first row on-screen
					_arrowLeft[i]->blit(surface);
					_arrowRight[i]->blit(surface);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
					_agentPaintOrder.push_back(_arrowLeft[i]);
					_agentPaintOrder.push_back(_arrowRight[i]);
#endif
				}

				if (!_texts[i].empty())
				{
					y += _texts[i].front()->getHeight() + _font->getSpacing();
				}
				else
				{
					y += _font->getHeight() + _font->getSpacing();
				}
			}
		}
		_up->blit(surface);
		_down->blit(surface);
		_scrollbar->blit(surface);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
		_agentPaintOrder.push_back(_up); _agentPaintOrder.push_back(_down);
		_agentPaintOrder.push_back(_scrollbar);
		_agentPaintRows = _agentDrawRows;
		_agentPaintControls = _agentDrawControls;
		_agentPaintPurchaseRows = _agentDrawPurchaseRows;
		_agentPaintArrivalRows = _agentDrawArrivalRows;
		_agentPaintOutfittingRows = _agentDrawOutfittingRows;
		_agentPaintPsiRows = _agentDrawPsiRows;
		_agentPaintCommendationRows = _agentDrawCommendationRows;
		_agentPaintPromotionRows = _agentDrawPromotionRows;
		if (_agentReportCells && _agentReportCaptureHealthy && !_agentReportEpochExhausted)
		{
			_agentPaintReportRows = _agentDrawReportRows;
			_agentReportPaintEpoch = _agentReportDrawEpoch;
		}
	_agentPaintSaveListRows = _agentDrawSaveListRows;
		if (_agentArrivalLogicalCells && _agentArrivalLogicalCaptureHealthy && !_agentArrivalLogicalEpochExhausted)
		{
			_agentPaintArrivalLogicalRows = _agentDrawArrivalLogicalRows;
			_agentArrivalLogicalPaintEpoch = _agentArrivalLogicalDrawEpoch;
		}
		if (_agentPsiTrainingCells)
		{
			_agentPsiSelector = _selector;
			_agentPsiSelectorRevision = _selector->agentPresentationRevision();
			for (AgentPsiCapture &capture : _agentPaintPsiRows)
			{
				const SDL_Rect &band = capture.row.bounds;
				capture.row.selected = _selector->agentPresentationPaintedTo(surface) &&
					_selector->getX() == band.x && _selector->getY() == band.y &&
					_selector->getWidth() == band.w && _selector->getHeight() == band.h;
			}
		}
		const bool selectorVisible = _selector && _selector->agentContextVisible();
		for (std::vector<AgentListRow> *group : {&_agentPaintRows, &_agentPaintControls})
		for (AgentListRow &row : *group)
		{
			row.selected = selectorVisible &&
				_selector->getX() == row.bounds.x && _selector->getY() == row.bounds.y &&
				_selector->getWidth() == row.bounds.w && _selector->getHeight() == row.bounds.h &&
				row.bounds.x >= getX() && row.bounds.y >= getY() &&
				row.bounds.x + row.bounds.w <= getX() + getWidth() &&
				row.bounds.y + row.bounds.h <= getY() + getHeight();
			if (row.selected)
			{
				// Every nonzero list pixel paints after the selector, including
				// incomplete/fallback text. Require a changed selector pixel that
				// survives this exact native text layer's transparent-zero mask.
				bool survivingChange = false;
				const int listLock = Observer::agentObservePaint(SDL_LockSurface(getSurface()));
				if (listLock >= 0)
				{
					for (const auto &pixel : agentSelectorChanged)
						if (getPixel(pixel.first, pixel.second) == 0)
						{ survivingChange = true; break; }
					SDL_UnlockSurface(getSurface());
				}
				row.selected = survivingChange;
			}
			if (row.selected)
			{
				SDL_Rect extent = row.bounds;
				for (const SDL_Rect &ink : row.regions)
				{
					const int right = std::max(extent.x + extent.w, ink.x + ink.w);
					const int bottom = std::max(extent.y + extent.h, ink.y + ink.h);
					extent.x = std::min(extent.x, ink.x); extent.y = std::min(extent.y, ink.y);
					extent.w = right - extent.x; extent.h = bottom - extent.y;
				}
				row.regions.assign(1, extent);
			}
		}
		for (AgentPurchaseRow &row : _agentPaintPurchaseRows)
		{
			row.selected = selectorVisible && _selector->getX() == row.bounds.x &&
				_selector->getY() == row.bounds.y && _selector->getWidth() == row.bounds.w &&
				_selector->getHeight() == row.bounds.h;
			bool changed = false;
			if (row.selected)
			{
				const int listLock = Observer::agentObservePaint(SDL_LockSurface(getSurface()));
				if (listLock >= 0)
				{
					size_t selectionWork = 0;
					for (const auto &pixel : agentSelectorChanged)
					{
						const int x = pixel.first + getX(), y = pixel.second + getY();
						if (y < row.bounds.y || y >= row.bounds.y + row.bounds.h || getPixel(pixel.first, pixel.second) != 0) continue;
						bool later = false, covered = false;
						for (const Surface *part : _agentPaintOrder)
						{
							if (++selectionWork > 2 * 1024 * 1024) { covered = true; break; }
							if (part == this) { later = true; continue; }
							if (later && part->getVisible() && x >= part->getX() && y >= part->getY() &&
								x < part->getX()+part->getWidth() && y < part->getY()+part->getHeight()) { covered = true; break; }
						}
						if (selectionWork > 2 * 1024 * 1024) break;
						if (!covered) { changed = true; break; }
					}
					SDL_UnlockSurface(getSurface());
				}
			}
			row.selected = row.selected && changed;
		}
		for (AgentOutfittingRow &row : _agentPaintOutfittingRows)
		{
			row.selected = selectorVisible && _selector->getX() == row.bounds.x &&
				_selector->getY() == row.bounds.y && _selector->getWidth() == row.bounds.w &&
				_selector->getHeight() == row.bounds.h;
			bool changed = false;
			if (row.selected)
			{
				const int listLock = Observer::agentObservePaint(SDL_LockSurface(getSurface()));
				if (listLock >= 0)
				{
					size_t selectionWork = 0;
					for (const auto &pixel : agentSelectorChanged)
					{
						const int x = pixel.first + getX(), y = pixel.second + getY();
						if (y < row.bounds.y || y >= row.bounds.y + row.bounds.h || getPixel(pixel.first, pixel.second) != 0) continue;
						bool later = false, covered = false;
						for (const Surface *part : _agentPaintOrder)
						{
							if (++selectionWork > 2 * 1024 * 1024) { covered = true; break; }
							if (part == this) { later = true; continue; }
							if (later && part->getVisible() && x >= part->getX() && y >= part->getY() &&
								x < part->getX()+part->getWidth() && y < part->getY()+part->getHeight()) { covered = true; break; }
						}
						if (selectionWork > 2 * 1024 * 1024) break;
						if (!covered) { changed = true; break; }
					}
					SDL_UnlockSurface(getSurface());
				}
			}
			row.selected = row.selected && (_agentOutfittingColumns==1 || changed);
		}
		_agentPaintRevision = _agentListRevision;
#endif
	}
}

/**
 * Passes events to arrow buttons.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool TextList::agentCursorMotionSafe() const
{
    if (!InteractiveSurface::agentCursorMotionSafe() ||
        (_up && !_up->agentCursorMotionSafe()) || (_down && !_down->agentCursorMotionSafe()) ||
        (_scrollbar && !_scrollbar->agentCursorMotionSafe())) return false;
    for (const ArrowButton *button : _arrowLeft) if (button && !button->agentCursorMotionSafe()) return false;
    for (const ArrowButton *button : _arrowRight) if (button && !button->agentCursorMotionSafe()) return false;
    return true;
}
#endif

void TextList::handle(Action *action, State *state)
{
	InteractiveSurface::handle(action, state);
	_up->handle(action, state);
	_down->handle(action, state);
	_scrollbar->handle(action, state);
	if (_arrowPos != -1 && !_rows.empty())
	{
		size_t startArrowIdx = _rows[_scroll];
		if (0 < _scroll && _rows[_scroll] == _rows[_scroll - 1])
		{
			// arrows for first partially-visible line of text are off-screen; don't process them
			++startArrowIdx;
		}
		size_t endArrowIdx = _rows[_scroll] + 1;
		size_t endRow = std::min(_rows.size(), _scroll + _visibleRows);
		for (size_t i = _scroll + 1; i < endRow; ++i)
		{
			if (_rows[i] != _rows[i - 1])
			{
				++endArrowIdx;
			}
		}
		for (size_t i = startArrowIdx; i < endArrowIdx; ++i)
		{
			_arrowLeft[i]->handle(action, state);
			_arrowRight[i]->handle(action, state);
		}
	}
}

/**
 * Passes ticks to arrow buttons.
 */
void TextList::think()
{
	InteractiveSurface::think();
	_up->think();
	_down->think();
	_scrollbar->think();
	for (std::vector<ArrowButton*>::iterator i = _arrowLeft.begin(); i < _arrowLeft.end(); ++i)
	{
		(*i)->think();
	}
	for (std::vector<ArrowButton*>::iterator i = _arrowRight.begin(); i < _arrowRight.end(); ++i)
	{
		(*i)->think();
	}
}

/**
 * Ignores any mouse clicks that aren't on a row.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void TextList::mousePress(Action *action, State *state)
{
	bool allowScroll = true;
	if (Options::changeValueByMouseWheel != 0)
	{
		allowScroll = (action->getAbsoluteXMouse() < _arrowsLeftEdge || action->getAbsoluteXMouse() > _arrowsRightEdge);
	}
	if (allowScroll)
	{
		if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP) scrollUp(false, true);
		else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN) scrollDown(false, true);
	}
	if (_selectable)
	{
		if (_selRow < _rows.size())
		{
			InteractiveSurface::mousePress(action, state);
		}
	}
	else
	{
		InteractiveSurface::mousePress(action, state);
	}
}

/*
 * Ignores any mouse clicks that aren't on a row.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void TextList::mouseRelease(Action *action, State *state)
{
	if (_selectable)
	{
		if (_selRow < _rows.size())
		{
			InteractiveSurface::mouseRelease(action, state);
		}
	}
	else
	{
		InteractiveSurface::mouseRelease(action, state);
	}
}

/**
 * Ignores any mouse clicks that aren't on a row.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void TextList::mouseClick(Action *action, State *state)
{
	activateRow(action, state, action->getDetails()->button.button);
}

void TextList::activateRow(Action *action, State *state, Uint8 button)
{
	if (_selectable)
	{
		if (_selRow < _rows.size())
		{
			if (action) InteractiveSurface::mouseClick(action, state);
			if (_comboBox && button == SDL_BUTTON_LEFT)
			{
				_comboBox->setSelected(_selRow);
				_comboBox->toggle();
			}
		}
	}
	else if (action)
	{
		InteractiveSurface::mouseClick(action, state);
	}
}

/**
 * Selects the row the mouse is over.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */

void TextList::selectPhysicalRow(size_t row)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	const bool wasVisible = _selector && _selector->getVisible();
	const size_t previous = _selRow;
#endif
	_selRow = row;
	if (_selRow < _rows.size())
	{
		Text *selText = _texts[_rows[_selRow]].front();
		int y = getY() + selText->getY();
		int actualHeight = selText->getHeight() + _font->getSpacing(); //current line height
		if (y < getY() || y + actualHeight > getY() + getHeight())
		{
			actualHeight /= 2;
		}
		if (y < getY())
		{
			y = getY();
		}
		if (_selector->getHeight() != actualHeight)
		{
			// resizing doesn't work, but recreating does, so let's do that!
			delete _selector;
			_selector = new Surface(getWidth(), actualHeight, getX(), y);
			_selector->setPalette(getPalette());
		}
		_selector->setY(y);
		_selector->copy(_bg);
		if (_contrast)
		{
			_selector->offsetBlock(-5);
		}
		else if (_comboBox)
		{
			_selector->offset(+1, Palette::backPos);
		}
		else
		{
			_selector->offsetBlock(-10);
		}
		_selector->setVisible(true);
	}
	else
	{
		_selector->setVisible(false);
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (_selRow != previous || (_selector && _selector->getVisible()) != wasVisible) ++_agentListRevision;
#endif
}

void TextList::mouseOver(Action *action, State *state)
{
	if (_selectable)
	{
		int rowHeight = _font->getHeight() + _font->getSpacing();
		selectPhysicalRow(std::max(0, (int)(_scroll + (int)floor(action->getRelativeYMouse() / (rowHeight * action->getYScale())))));
	}

	InteractiveSurface::mouseOver(action, state);
}

/**
 * Deselects the row.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void TextList::mouseOut(Action *action, State *state)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (_selectable && _selector && _selector->getVisible()) ++_agentListRevision;
#endif
	if (_selectable)
	{
		_selector->setVisible(false);
	}

	InteractiveSurface::mouseOut(action, state);
}

/*
 * get the scroll depth.
 * @return scroll depth.
 */
size_t TextList::getScroll()
{
	return _scroll;
}

/*
 * set the scroll depth.
 * @param scroll set the scroll depth to this.
 */
void TextList::scrollTo(size_t scroll)
{
	if (_fullRowsOnly && _visibleRows == 0) return;
	if (!_scrolling)
		return;
	if (_rows.size() <= _visibleRows)
		return;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	if (_scroll != Clamp(scroll, (size_t)(0), _rows.size() - _visibleRows)) { ++_agentListRevision; }
#endif
	_scroll = Clamp(scroll, (size_t)(0), _rows.size() - _visibleRows);
	draw(); // can't just set _redraw here because reasons
	updateArrows();
}

/**
 * Hooks up the button to work as part of an existing combobox,
 * updating the selection when it's pressed.
 * @param comboBox Pointer to combobox.
 */
void TextList::setComboBox(ComboBox *comboBox)
{
	_comboBox = comboBox;
}

/**
 * Gets the combobox that this list is attached to, if any.
 * @return the attached combobox.
 */
ComboBox *TextList::getComboBox() const
{
	return _comboBox;
}

void TextList::setBorderColor(Uint8 color)
{
	_up->setColor(color);
	_down->setColor(color);
	_scrollbar->setColor(color);
}

int TextList::getScrollbarColor()
{
	return _scrollbar->getColor();
}

void TextList::setFlooding(bool flooding)
{
	_flooding = flooding;
}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace
{
bool agentListOverlap(const SDL_Rect &a, const SDL_Rect &b)
{
	return a.x < b.x + b.w && a.x + a.w > b.x &&
		a.y < b.y + b.h && a.y + a.h > b.y;
}
}

void TextList::agentCaptureRow(size_t logicalIndex, int y)
{
	// Only inspect the typed Text surfaces just blitted by the ordinary list
	// renderer. No screen/RGB recognition or model data participates. Limits
	// are per draw; exhaustion invalidates all row evidence for that draw.
	const size_t maxWork = 2 * 1024 * 1024, maxRuns = 8192;
	const auto fail = [&]() { _agentCoverageHealthy = false; _agentDrawRows.clear(); _agentDrawControls.clear(); _agentDrawPurchaseRows.clear(); _agentDrawArrivalRows.clear(); _agentDrawOutfittingRows.clear(); };
	if (!_agentCoverageHealthy) return;
	const SDL_Surface *destination = getSurface();
	if (getWidth() <= 0 || getHeight() <= 0 || getWidth() > 640 || getHeight() > 400 ||
		_texts[logicalIndex].size() > 16 || !destination || !destination->format ||
		destination->format->BytesPerPixel != 1 || !(destination->flags & SDL_SRCCOLORKEY) ||
		destination->format->colorkey != 0 || (destination->flags & SDL_SRCALPHA) ||
		destination->clip_rect.x != 0 || destination->clip_rect.y != 0 ||
		destination->clip_rect.w != getWidth() || destination->clip_rect.h != getHeight()) { fail(); return; }
	const auto order = [](const SDL_Rect &a, const SDL_Rect &b) {
		return a.y != b.y ? a.y < b.y : a.x < b.x;
	};
	const auto overlaps = [&](const std::vector<SDL_Rect> &a, const std::vector<SDL_Rect> &b) -> bool {
		int maxHeight = 0;
		for (const SDL_Rect &r : b) maxHeight = std::max(maxHeight, (int)r.h);
		for (const SDL_Rect &part : a)
		{
			const auto first = std::lower_bound(b.begin(), b.end(), (int)part.y - maxHeight + 1,
				[](const SDL_Rect &r, int top) { return r.y < top; });
			for (auto it = first; it != b.end() && it->y < part.y + part.h; ++it)
			{
				if (++_agentCoverageWork > maxWork) { fail(); return true; }
				if (agentListOverlap(part, *it)) return true;
			}
		}
		return false;
	};
	std::vector<std::vector<SDL_Rect> > cells;
	std::vector<std::vector<AgentPurchaseCell::Pixel> > purchaseInk;
	std::vector<SDL_Rect> allInk;
	const bool purchaseCells = !_comboBox && _selectable && _bg && _selector && !_dot &&
		_columns.size() == 4 && _texts[logicalIndex].size() == 4 && _arrowPos != -1 && _arrowType == ARROW_VERTICAL;
	const bool arrivalCells = _agentArrivalCells && !_comboBox && _selectable && _bg && _selector && !_dot &&
		_columns.size() == 3 && _texts[logicalIndex].size() == 3 && _arrowPos == -1;
	const bool outfittingCells = _agentOutfittingColumns && !_comboBox && _selectable && _bg && _selector && !_dot &&
		_columns.size() == _agentOutfittingColumns && _texts[logicalIndex].size() <= _agentOutfittingColumns &&
		(!_agentOutfittingEquipment || (_arrowPos != -1 && _arrowType == ARROW_HORIZONTAL)) &&
		(!_agentOutfittingCrew || (_arrowPos != -1 && _arrowType == ARROW_VERTICAL));
	const bool independentCells = purchaseCells || arrivalCells || outfittingCells;
	for (const Text *cell : _texts[logicalIndex])
	{
		// An explicitly hidden opted-in cell paints no pixels. Its own text
		// stays unknown, while independent sibling cells/arrows retain proof.
		if (independentCells && (!cell->getVisible() || cell->_hidden)) { cells.emplace_back(); purchaseInk.emplace_back(); continue; }
		SDL_Surface *pixels = cell->getSurface();
		if ((!cell->agentContextVisible() && !(independentCells && !cell->_redraw)) || !pixels || !pixels->format ||
			pixels->format->BytesPerPixel != 1 || !(pixels->flags & SDL_SRCCOLORKEY) ||
			pixels->format->colorkey != 0 || (pixels->flags & SDL_SRCALPHA) ||
			cell->getWidth() <= 0 || cell->getHeight() <= 0 || cell->getWidth() > 640 || cell->getHeight() > 400)
		{ fail(); return; }
		const size_t work = static_cast<size_t>(cell->getWidth()) * cell->getHeight();
		if (work > maxWork - std::min(maxWork, _agentCoverageWork)) { fail(); return; }
		_agentCoverageWork += work;
		std::vector<SDL_Rect> runs;
		std::vector<AgentPurchaseCell::Pixel> expected;
		if (independentCells && (!pixels->format->palette || !pixels->format->palette->colors ||
			pixels->format->palette->ncolors != 256)) { fail(); return; }
		const int locked = Observer::agentObservePaint(SDL_LockSurface(pixels));
		if (locked < 0) { fail(); return; }
		const SDL_Rect crop = cell->_crop;
		const bool cropped = independentCells && (crop.w || crop.h);
		for (int sy = 0; sy < cell->getHeight(); ++sy)
		{
			if (cropped && (sy < crop.y || sy >= crop.y + crop.h)) continue;
			const int dy = cell->getY() + sy - (cropped ? crop.y : 0);
			if (dy < 0 || dy >= getHeight()) continue;
			const Uint8 *line = static_cast<const Uint8 *>(pixels->pixels) + sy * pixels->pitch;
			int begin = -1;
			for (int sx = 0; sx <= cell->getWidth(); ++sx)
			{
				const int dx = cell->getX() + sx - (cropped ? crop.x : 0);
				const bool opaque = sx < cell->getWidth() && (!cropped || (sx >= crop.x && sx < crop.x + crop.w)) && dx >= 0 && dx < getWidth() && line[sx] != 0;
				if (independentCells && opaque)
				{
					const SDL_Color color = pixels->format->palette->colors[line[sx]];
					expected.push_back({static_cast<Sint16>(dx), static_cast<Sint16>(dy), color.r, color.g, color.b});
				}
				if (opaque && begin < 0) begin = dx;
				if (!opaque && begin >= 0)
				{
					SDL_Rect run = {static_cast<Sint16>(getX() + begin), static_cast<Sint16>(getY() + dy),
						static_cast<Uint16>(dx - begin), 1};
					runs.push_back(run); begin = -1;
					if (++_agentCoverageRuns > maxRuns) break;
				}
			}
			if (_agentCoverageRuns > maxRuns) break;
		}
		SDL_UnlockSurface(pixels);
		if (_agentCoverageRuns > maxRuns) { fail(); return; }
		allInk.insert(allInk.end(), runs.begin(), runs.end());
		cells.push_back(runs);
		purchaseInk.push_back(std::move(expected));
	}
	std::sort(allInk.begin(), allInk.end(), order);
	const auto unchanged = [&](const std::vector<AgentPurchaseCell::Pixel> &ink) -> bool {
		// Compare only the typed cell's original opaque mask. This is an
		// unchanged-paint proof, not recognition of text in the destination.
		SDL_Surface *target = getSurface();
		if (!target->format->palette || !target->format->palette->colors || target->format->palette->ncolors != 256 ||
			ink.size() > maxWork - std::min(maxWork, _agentCoverageWork)) { fail(); return false; }
		_agentCoverageWork += ink.size();
		if (Observer::agentObservePaint(SDL_LockSurface(target)) < 0) { fail(); return false; }
		bool same = true;
		for (const AgentPurchaseCell::Pixel &pixel : ink)
		{
			const Uint8 index = *(static_cast<const Uint8 *>(target->pixels) + pixel.y * target->pitch + pixel.x);
			const SDL_Color color = target->format->palette->colors[index];
			// Index zero would become transparent when the list is blitted.
			if (!index || color.r != pixel.r || color.g != pixel.g || color.b != pixel.b) { same = false; break; }
		}
		SDL_UnlockSurface(target);
		return same;
	};
	// A later row/column may cover only one cell. Keep the four independent
	// witnesses separate; this opt-in does not relax the existing list guards.
	for (AgentPurchaseRow &prior : _agentDrawPurchaseRows)
		for (AgentPurchaseCell &cell : prior.cells)
		{
			if (!cell.known) continue;
			const bool overlap = overlaps(cell.regions, allInk);
			if (!_agentCoverageHealthy) return;
			if (!overlap) continue;
			const bool preserved = unchanged(cell.ink);
			if (!_agentCoverageHealthy) return;
			if (!preserved) { cell.known = false; cell.regions.clear(); cell.ink.clear(); }
		}
	for (AgentArrivalRow &prior : _agentDrawArrivalRows)
		for (AgentPurchaseCell &cell : prior.cells)
		{
			if (!cell.known) continue;
			const bool overlap = overlaps(cell.regions, allInk);
			if (!_agentCoverageHealthy) return;
			if (!overlap) continue;
			const bool preserved = unchanged(cell.ink);
			if (!_agentCoverageHealthy) return;
			if (!preserved) { cell.known = false; cell.regions.clear(); cell.ink.clear(); }
		}
	for (AgentOutfittingRow &prior : _agentDrawOutfittingRows)
	{
		for (AgentPurchaseCell &cell : prior.cells)
		{
			if (!cell.known) continue;
			const bool overlap = overlaps(cell.regions, allInk);
			if (!_agentCoverageHealthy) return;
            // Facility captions are native visible-row semantics, not a
            // full-glyph reconstruction. Neighboring native ink is not a
            // foreign overlay. Other list opt-ins retain their old proof.
            if (_agentOutfittingColumns!=1 && overlap && !unchanged(cell.ink)) { cell.known=false;cell.regions.clear();cell.ink.clear(); }
		}
		// A row control needs at least one surviving ordinary typed write.
		std::vector<SDL_Rect> surviving;
		for (const SDL_Rect &run : prior.controlInk)
		{
			int left = run.x, right = run.x + run.w;
			auto it = std::lower_bound(allInk.begin(), allInk.end(), (int)run.y,
				[](const SDL_Rect &part, int top) { return part.y < top; });
			for (; it != allInk.end() && it->y == run.y && left < right; ++it)
			{
				if (++_agentCoverageWork > maxWork) { fail(); return; }
				if (it->x + it->w <= left) continue;
				if (it->x >= right) break;
				if (it->x > left) surviving.push_back({static_cast<Sint16>(left), run.y, static_cast<Uint16>(it->x-left), 1});
				left = std::max(left, it->x + it->w);
			}
			if (left < right) surviving.push_back({static_cast<Sint16>(left), run.y, static_cast<Uint16>(right-left), 1});
			if (surviving.size() > maxRuns) { fail(); return; }
		}
		prior.controlInk.swap(surviving);
	}
	if (!_agentCoverageHealthy) return;
	const int purchaseHeight = _font->getHeight() + _font->getSpacing();
	if (!_comboBox && _selectable && _bg && _selector && !_dot && _columns.size() == 4 &&
		_texts[logicalIndex].size() == 4 && _arrowPos != -1 && _arrowType == ARROW_VERTICAL &&
		purchaseHeight > 0 && y >= 0 && y + purchaseHeight <= getHeight())
	{
		AgentPurchaseRow row;
		row.logicalIndex = logicalIndex;
		row.bounds = {static_cast<Sint16>(getX()), static_cast<Sint16>(getY() + y),
			static_cast<Uint16>(getWidth()), static_cast<Uint16>(_texts[logicalIndex].front()->getHeight() + _font->getSpacing())};
		for (size_t column = 0; column < 4; ++column)
		{
			const Text *cell = _texts[logicalIndex][column];
			SDL_Rect rect = {};
			if (!cell->agentContextVisible() || !cell->getFont() || cell->getNumLines() != 1 ||
				!cell->agentTextComplete() || cell->getTextWidth() > cell->getWidth() ||
				cell->getTextHeight() > cell->getHeight() || cell->getText().size() > 1024) continue;
			const bool ink = cell->agentPaintedBounds(rect);
			if (ink && (rect.x < 0 || rect.y < 0 || rect.x + rect.w > getWidth() ||
				rect.y + rect.h > getHeight() || cells[column].empty())) continue;
			// Later columns have already been blitted too. Require their final
			// destination to preserve every opaque source RGB, even at overlap.
			const bool preserved = unchanged(purchaseInk[column]);
			if (!_agentCoverageHealthy) return;
			if (!preserved) continue;
			UString shown = Unicode::convUtf8ToUtf32(cell->getText());
			shown.erase(std::remove(shown.begin(), shown.end(), Unicode::TOK_COLOR_FLIP), shown.end());
			row.cells[column].known = true;
			row.cells[column].text = Unicode::convUtf32ToUtf8(shown);
			row.cells[column].regions = cells[column];
			row.cells[column].ink = std::move(purchaseInk[column]);
		}
		_agentDrawPurchaseRows.push_back(row);
	}

	// Arrivals opt in explicitly. A complete cell remains independent of an
	// adjacent clipped label; no substring or hidden destination is recovered.
	if (arrivalCells && purchaseHeight > 0 && y >= 0 && y + purchaseHeight <= getHeight() &&
		std::count(_rows.begin(), _rows.end(), logicalIndex) == 1)
	{
		AgentArrivalRow row;
		row.logicalIndex = logicalIndex;
		row.bounds = {static_cast<Sint16>(getX()), static_cast<Sint16>(getY() + y),
			static_cast<Uint16>(getWidth()), static_cast<Uint16>(purchaseHeight)};
		for (size_t column = 0; column < 3; ++column)
		{
			const Text *cell = _texts[logicalIndex][column];
			SDL_Rect rect = {};
			if (!cell->agentContextVisible() || !cell->getFont() || cell->getNumLines() != 1 ||
				!cell->agentTextComplete() || cell->getTextWidth() > cell->getWidth() ||
				cell->getTextHeight() > cell->getHeight() || cell->getText().size() > 1024) continue;
			const bool ink = cell->agentPaintedBounds(rect);
			if (ink && (rect.x < 0 || rect.y < 0 || rect.x + rect.w > getWidth() ||
				rect.y + rect.h > getHeight() || cells[column].empty())) continue;
			const bool preserved = unchanged(purchaseInk[column]);
			if (!_agentCoverageHealthy) return;
			if (!preserved) continue;
			UString shown = Unicode::convUtf8ToUtf32(cell->getText());
			shown.erase(std::remove(shown.begin(), shown.end(), Unicode::TOK_COLOR_FLIP), shown.end());
			row.cells[column].known = true;
			row.cells[column].text = Unicode::convUtf32ToUtf8(shown);
			row.cells[column].regions = cells[column];
			row.cells[column].ink = std::move(purchaseInk[column]);
		}
		_agentDrawArrivalRows.push_back(row);
	}

	if (outfittingCells && purchaseHeight > 0 && y >= 0 && y + purchaseHeight <= getHeight() &&
		std::count(_rows.begin(), _rows.end(), logicalIndex) == 1)
	{
		AgentOutfittingRow row;
		row.logicalIndex = logicalIndex;
		row.bounds = {static_cast<Sint16>(getX()), static_cast<Sint16>(getY()+y),
			static_cast<Uint16>(getWidth()), static_cast<Uint16>(purchaseHeight)};
		row.clickBand = row.bounds;
		// The ordinary crew callback excludes the vertical reorder hit band.
		if (_agentOutfittingCrew) row.clickBand.w = std::max(0, _arrowsLeftEdge - getX());
		row.cells.resize(_agentOutfittingColumns);
		for (const SDL_Rect &run : allInk)
			if (run.y >= row.clickBand.y && run.y < row.clickBand.y + row.clickBand.h &&
				run.x >= row.clickBand.x && run.x + run.w <= row.clickBand.x + row.clickBand.w)
				row.controlInk.push_back(run);
		for (size_t column = 0; column < _texts[logicalIndex].size(); ++column)
		{
			const Text *cell = _texts[logicalIndex][column];
            if (_agentOutfittingColumns==1) {
                if(cell->agentContextVisible() && cell->getText().size()<=1024) {
                    row.cells[column].known=true;
                    UString shown=Unicode::convUtf8ToUtf32(cell->getText());
                    shown.erase(std::remove(shown.begin(),shown.end(),Unicode::TOK_COLOR_FLIP),shown.end());
                    row.cells[column].text=Unicode::convUtf32ToUtf8(shown);
                    row.cells[column].regions.push_back(row.clickBand);
                }
                continue;
            }
			SDL_Rect rect = {};
			if (!cell->agentContextVisible() || !cell->getFont() || cell->getNumLines() != 1 ||
				!cell->agentTextComplete() || cell->getTextWidth() > cell->getWidth() ||
				cell->getTextHeight() > cell->getHeight() || cell->getText().size() > 1024) continue;
			const bool ink = cell->agentPaintedBounds(rect);
			if (ink && (rect.x < 0 || rect.y < 0 || rect.x+rect.w > getWidth() ||
				rect.y+rect.h > getHeight() || cells[column].empty())) continue;
			const bool preserved = unchanged(purchaseInk[column]);
			if (!_agentCoverageHealthy) return;
			if (!preserved) continue;
			UString shown = Unicode::convUtf8ToUtf32(cell->getText());
			shown.erase(std::remove(shown.begin(), shown.end(), Unicode::TOK_COLOR_FLIP), shown.end());
			row.cells[column].known = true;
			row.cells[column].text = Unicode::convUtf32ToUtf8(shown);
			row.cells[column].regions = cells[column];
			row.cells[column].ink = std::move(purchaseInk[column]);
		}
		_agentDrawOutfittingRows.push_back(row);
	}

	// A control needs surviving typed ink, not a complete string. Retire every
	// later opaque write, including same-color writes, from earlier witnesses.
	for (AgentListRow &prior : _agentDrawControls)
	{
		std::vector<SDL_Rect> surviving;
		for (const SDL_Rect &run : prior.regions)
		{
			int left = run.x, right = run.x + run.w;
			const auto first = std::lower_bound(allInk.begin(), allInk.end(), (int)run.y,
				[](const SDL_Rect &part, int top) { return part.y < top; });
			for (auto it = first; it != allInk.end() && it->y == run.y && left < right; ++it)
			{
				if (++_agentCoverageWork > maxWork) { fail(); return; }
				if (it->x + it->w <= left) continue;
				if (it->x >= right) break;
				if (it->x > left)
				{
					SDL_Rect part = {static_cast<Sint16>(left), run.y, static_cast<Uint16>(it->x - left), 1};
					surviving.push_back(part);
					if (++_agentCoverageRuns > maxRuns) { fail(); return; }
				}
				left = std::max(left, it->x + it->w);
			}
			if (left < right)
			{
				SDL_Rect part = {static_cast<Sint16>(left), run.y, static_cast<Uint16>(right - left), 1};
				surviving.push_back(part);
			}
		}
		prior.regions.swap(surviving);
	}
	const int lineHeight = _font->getHeight() + _font->getSpacing();
	if (!_comboBox && _selectable && _bg && _selector && !_dot && _columns.size() == 1 &&
		_texts[logicalIndex].size() == 1 && _texts[logicalIndex][0]->getNumLines() == 1 &&
		lineHeight > 0 && y >= 0 && y + lineHeight <= getHeight() &&
		std::count(_rows.begin(), _rows.end(), logicalIndex) == 1)
	{
		AgentListRow control;
		control.logicalIndex = logicalIndex; control.selected = false; control.completeText = false;
		control.cells.resize(1);
		control.bounds = {static_cast<Sint16>(getX()), static_cast<Sint16>(getY() + y),
			static_cast<Uint16>(getWidth()), static_cast<Uint16>(lineHeight)};
		for (const SDL_Rect &run : allInk)
			if (run.y >= control.bounds.y && run.y < control.bounds.y + control.bounds.h)
				control.regions.push_back(run);
		if (!control.regions.empty()) _agentDrawControls.push_back(control);
	}
	// Even an incomplete later row can overwrite an earlier complete row.
	for (auto prior = _agentDrawRows.begin(); prior != _agentDrawRows.end(); )
	{
		const bool covered = overlaps(prior->regions, allInk);
		if (!_agentCoverageHealthy) return;
		if (covered) prior = _agentDrawRows.erase(prior); else ++prior;
	}
	if (_comboBox && static_cast<size_t>(std::find(_rows.begin(), _rows.end(), logicalIndex) - _rows.begin()) != logicalIndex) return;
	if (y < 0 || y + lineHeight > getHeight() || _texts[logicalIndex].empty() ||
		std::count(_rows.begin(), _rows.end(), logicalIndex) != 1) return;
	AgentListRow row;
	row.logicalIndex = logicalIndex; row.selected = false;
	row.bounds.x = getX(); row.bounds.y = getY() + y;
	row.bounds.w = getWidth(); row.bounds.h = lineHeight;
	for (size_t column = 0; column < _texts[logicalIndex].size(); ++column)
	{
		const Text *cell = _texts[logicalIndex][column];
		if (!cell->agentContextVisible() || !cell->getFont() || cell->getNumLines() != 1) return;
		// Later decorated pixels still occlude prior content, including leaders.
		if (overlaps(row.regions, cells[column]) || !_agentCoverageHealthy) return;
		std::string shownText;
		SDL_Rect rect = {};
		bool hasInk = false;
		if (cell->_agentListRange)
		{
			if (!cell->agentListContent(shownText, rect, hasInk)) return;
		}
		else
		{
			if (!cell->agentTextComplete() || cell->getTextWidth() > cell->getWidth() ||
				cell->getTextHeight() > cell->getHeight()) return;
			shownText = cell->getText(); hasInk = cell->agentPaintedBounds(rect);
		}
		if (shownText.size() > 1024) return;
		if (hasInk)
		{
			if (rect.x < 0 || rect.y < 0 || rect.x + rect.w > getWidth() || rect.y + rect.h > getHeight()) return;
			if (cell->_agentListRange)
			{
				// Original dotted-content proof remains conservative. Decorative
				// leader pixels must never become a broader text export API.
				rect.x += getX(); rect.y += getY();
				row.regions.push_back(rect);
			}
			else
			{
				if (cells[column].empty()) return;
				row.regions.insert(row.regions.end(), cells[column].begin(), cells[column].end());
			}
			std::sort(row.regions.begin(), row.regions.end(), order);
		}
		UString shown = Unicode::convUtf8ToUtf32(shownText);
		shown.erase(std::remove(shown.begin(), shown.end(), Unicode::TOK_COLOR_FLIP), shown.end());
		row.cells.push_back(Unicode::convUtf32ToUtf8(shown));
	}
	row.cells.resize(_columns.size(), std::string());
	_agentDrawRows.push_back(row);
}

bool TextList::agentRows(std::vector<AgentListRow> &rows) const
{
	rows.clear();
	if (!Observer::agentWidgetVisible(this) || _agentPaintRevision != _agentListRevision ||
		!_agentCoverageHealthy || !Observer::agentPaintHealthy()) return false;
	for (const AgentListRow &row : _agentPaintRows)
	{
		// Only ink actually admitted by the last destination blit is public.
		// Restoring the current clip without repainting cannot revive a row.
		bool clipped = false;
		for (const SDL_Rect &region : row.regions)
			if (region.x < _agentListClip.x || region.y < _agentListClip.y ||
				region.x + region.w > _agentListClip.x + _agentListClip.w ||
				region.y + region.h > _agentListClip.y + _agentListClip.h) clipped = true;
		if (clipped) continue;
		bool covered = false, later = false;
		for (const Surface *part : _agentPaintOrder)
		{
			if (part == this) { later = true; continue; }
			if (!later || !part->getVisible()) continue;
			SDL_Rect cover;
			cover.x = part->getX(); cover.y = part->getY();
			cover.w = part->getWidth(); cover.h = part->getHeight();
			for (const SDL_Rect &region : row.regions)
				if (agentListOverlap(region, cover)) covered = true;
		}
		if (!covered) rows.push_back(row);
	}
	return true;
}

void TextList::agentEnableCommendationCells()
{
	if (!_agentCommendationCells)
	{
		_agentCommendationCells = true; ++_agentListRevision; _redraw = true;
		agentTrackPresentation();
	}
}

void TextList::agentCaptureCommendationRow(size_t logicalIndex, int y)
{
	if (!_agentCommendationCells) return;
	if (!_font || _comboBox || _dot || _wrap || _condensed ||
		_flooding || _arrowPos != -1 || _columns.size() != 2 || logicalIndex >= _texts.size() ||
		_texts[logicalIndex].size() != 2 || getWidth() <= 0 || getWidth() > 640 ||
		getHeight() <= 0 || getHeight() > 400)
	{ _agentCommendationCaptureHealthy = false; return; }
	// Use the ordinary draw allocation, not caption metrics or the selection grid.
	const int allocationHeight = _texts[logicalIndex][0]->getHeight();
	const int height = allocationHeight + _font->getSpacing();
	// Stock small-font allocation9/spacing-1 advances by8. Observe that actual
	// row strip, excluding the spare bottom pixel shared with the following row.
	// Other negative overlap and malformed column allocations remain refused.
	if (_margin < 0 || _margin > 640 || _font->getSpacing() < -1 || allocationHeight <= 0)
	{ _agentCommendationCaptureHealthy = false; return; }
	int expectedX = _margin;
	for (size_t col = 0; col < 2; ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		if (!_columns[col] || _columns[col] > 640 || cell->getX() != expectedX ||
			cell->getWidth() != static_cast<int>(_columns[col]) || cell->getY() != y ||
			cell->getHeight() != allocationHeight)
		{ _agentCommendationCaptureHealthy = false; return; }
		expectedX += static_cast<int>(_columns[col]);
	}
	if (height <= 0) { _agentCommendationCaptureHealthy = false; return; }
	if (y < 0 || y + height > getHeight()) return;
	if (_agentDrawCommendationRows.size() >= 128) { _agentCommendationCaptureHealthy = false; return; }
	AgentCommendationCapture capture = {};
	capture.row.logicalIndex = logicalIndex; // Private cell identity only; never a public row handle.
	capture.row.bounds = {static_cast<Sint16>(getX()), static_cast<Sint16>(getY()+y),
		static_cast<Uint16>(getWidth()), static_cast<Uint16>(height)};
	capture.row.cells.resize(2);
	for (size_t col = 0; col < 2; ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		capture.cells[col] = cell;
		capture.textRevisions[col] = cell->agentTextRevision();
		capture.presentationRevisions[col] = cell->agentPresentationRevision();
		capture.geometry[col] = {static_cast<Sint16>(cell->getX()), static_cast<Sint16>(cell->getY()),
			static_cast<Uint16>(cell->getWidth()), static_cast<Uint16>(cell->getHeight())};
		// Native margin8 clips the second allocation by8px. Preserve its logical
		// caption when the allocation intersects the viewport; never inspect glyphs.
		const int left = std::max(0, cell->getX());
		const int right = std::min(getWidth(), cell->getX()+cell->getWidth());
		AgentPurchaseCell &value = capture.row.cells[col];
		value.known = cell->agentPresentationPaintedTo(this) && cell->getY() == y &&
			cell->getHeight() == allocationHeight && left < right &&
			cell->getText().size() <= 1024;
		if (value.known)
		{
			value.text = cell->getText();
			value.regions.push_back({static_cast<Sint16>(getX()+left), static_cast<Sint16>(getY()+y),
				static_cast<Uint16>(right-left), static_cast<Uint16>(height)});
		}
	}
	_agentDrawCommendationRows.push_back(capture);
}

bool TextList::agentCommendationRows(std::vector<AgentOutfittingRow> &rows) const
{
	rows.clear();
	if (!_agentCommendationCells || !_agentCommendationCaptureHealthy || !Observer::agentWidgetVisible(this) ||
		getWidth() <= 0 || getWidth() > 640 || getHeight() <= 0 || getHeight() > 400 ||
		!agentPresentationPaintedTo(_agentCommendationDestination) ||
		_agentPaintRevision != _agentListRevision || !Observer::agentPaintHealthy() ||
		_comboBox || _dot || _wrap || _condensed || _flooding || _columns.size() != 2 ||
		_arrowPos != -1 || _agentPaintCommendationRows.size() > 128) return false;
	for (const AgentCommendationCapture &capture : _agentPaintCommendationRows)
	{
		AgentOutfittingRow row = capture.row;
		if (row.logicalIndex >= _texts.size() || _texts[row.logicalIndex].size() != 2)
		{ rows.clear(); return false; }
		for (size_t col = 0; col < 2; ++col)
		{
			const Text *cell = _texts[row.logicalIndex][col];
			const SDL_Rect &g = capture.geometry[col];
			if (cell != capture.cells[col] || cell->_redraw ||
				(capture.row.cells[col].known && !cell->agentPresentationPaintedTo(this)) ||
				cell->agentTextRevision() != capture.textRevisions[col] ||
				cell->agentPresentationRevision() != capture.presentationRevisions[col] ||
				cell->getX() != g.x || cell->getY() != g.y || cell->getWidth() != g.w || cell->getHeight() != g.h)
			{ rows.clear(); return false; }
		}
		if (row.bounds.y < _agentCommendationClip.y ||
			row.bounds.y+row.bounds.h > _agentCommendationClip.y+_agentCommendationClip.h ||
			row.bounds.x >= _agentCommendationClip.x+_agentCommendationClip.w ||
			row.bounds.x+row.bounds.w <= _agentCommendationClip.x) continue;
		for (AgentPurchaseCell &cell : row.cells)
		{
			for (SDL_Rect &region : cell.regions)
			{
				const int left = std::max<int>(region.x, _agentCommendationClip.x);
				const int right = std::min<int>(region.x+region.w, _agentCommendationClip.x+_agentCommendationClip.w);
				if (left >= right) { cell.known = false; break; }
				region.x = left; region.w = right-left;
				const Observer::AgentUiRect band = {region.x,region.y,region.w,region.h};
				if (!Observer::agentRegionsUncovered({band}, this, _agentPaintOrder)) cell.known = false;
			}
			if (!cell.known) { cell.text.clear(); cell.regions.clear(); }
		}
		rows.push_back(row);
	}
	return true;
}

void TextList::agentEnablePromotionCells()
{
	if (!_agentPromotionCells)
	{
		_agentPromotionCells = true; ++_agentListRevision; _redraw = true;
		agentTrackPresentation();
	}
}

void TextList::agentCapturePromotionRow(size_t logicalIndex, int y)
{
	if (!_agentPromotionCells) return;
	if (!_font || _comboBox || _dot || _wrap || _condensed ||
		_flooding || _arrowPos != -1 || _columns.size() != 3 || logicalIndex >= _texts.size() ||
		_texts[logicalIndex].size() != 3 || getWidth() <= 0 || getWidth() > 640 ||
		getHeight() <= 0 || getHeight() > 400)
	{ _agentPromotionCaptureHealthy = false; return; }
	// Use the ordinary draw allocation, not caption metrics or the selection grid.
	const int allocationHeight = _texts[logicalIndex][0]->getHeight();
	const int height = allocationHeight + _font->getSpacing();
	// Stock small-font allocation9/spacing-1 advances by8. Observe that actual
	// row strip, excluding the spare bottom pixel shared with the following row.
	// Other negative overlap and malformed column allocations remain refused.
	if (_margin < 0 || _margin > 640 || _font->getSpacing() < -1 || allocationHeight <= 0)
	{ _agentPromotionCaptureHealthy = false; return; }
	int expectedX = _margin;
	for (size_t col = 0; col < 3; ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		if (!_columns[col] || _columns[col] > 640 || cell->getX() != expectedX ||
			cell->getWidth() != static_cast<int>(_columns[col]) || cell->getY() != y ||
			cell->getHeight() != allocationHeight)
		{ _agentPromotionCaptureHealthy = false; return; }
		expectedX += static_cast<int>(_columns[col]);
	}
	if (height <= 0) { _agentPromotionCaptureHealthy = false; return; }
	if (y < 0 || y + height > getHeight()) return;
	if (_agentDrawPromotionRows.size() >= 128) { _agentPromotionCaptureHealthy = false; return; }
	AgentPromotionCapture capture = {};
	capture.row.logicalIndex = logicalIndex; // Private cell identity only; never a public row handle.
	capture.row.bounds = {static_cast<Sint16>(getX()), static_cast<Sint16>(getY()+y),
		static_cast<Uint16>(getWidth()), static_cast<Uint16>(height)};
	capture.row.cells.resize(3);
	for (size_t col = 0; col < 3; ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		capture.cells[col] = cell;
		capture.textRevisions[col] = cell->agentTextRevision();
		capture.presentationRevisions[col] = cell->agentPresentationRevision();
		capture.geometry[col] = {static_cast<Sint16>(cell->getX()), static_cast<Sint16>(cell->getY()),
			static_cast<Uint16>(cell->getWidth()), static_cast<Uint16>(cell->getHeight())};
		// Native margin8 clips the third allocation by8px. Preserve its logical
		// caption when the allocation intersects the viewport; never inspect glyphs.
		const int left = std::max(0, cell->getX());
		const int right = std::min(getWidth(), cell->getX()+cell->getWidth());
		AgentPurchaseCell &value = capture.row.cells[col];
		value.known = cell->agentPresentationPaintedTo(this) && cell->getY() == y &&
			cell->getHeight() == allocationHeight && left < right &&
			cell->getText().size() <= 1024;
		if (value.known)
		{
			value.text = cell->getText();
			value.regions.push_back({static_cast<Sint16>(getX()+left), static_cast<Sint16>(getY()+y),
				static_cast<Uint16>(right-left), static_cast<Uint16>(height)});
		}
	}
	_agentDrawPromotionRows.push_back(capture);
}

bool TextList::agentPromotionRows(std::vector<AgentOutfittingRow> &rows) const
{
	rows.clear();
	if (!_agentPromotionCells || !_agentPromotionCaptureHealthy || !Observer::agentWidgetVisible(this) ||
		getWidth() <= 0 || getWidth() > 640 || getHeight() <= 0 || getHeight() > 400 ||
		!agentPresentationPaintedTo(_agentPromotionDestination) ||
		_agentPaintRevision != _agentListRevision || !Observer::agentPaintHealthy() ||
		_comboBox || _dot || _wrap || _condensed || _flooding || _columns.size() != 3 ||
		_arrowPos != -1 || _agentPaintPromotionRows.size() > 128) return false;
	for (const AgentPromotionCapture &capture : _agentPaintPromotionRows)
	{
		AgentOutfittingRow row = capture.row;
		if (row.logicalIndex >= _texts.size() || _texts[row.logicalIndex].size() != 3)
		{ rows.clear(); return false; }
		for (size_t col = 0; col < 3; ++col)
		{
			const Text *cell = _texts[row.logicalIndex][col];
			const SDL_Rect &g = capture.geometry[col];
			if (cell != capture.cells[col] || cell->_redraw ||
				(capture.row.cells[col].known && !cell->agentPresentationPaintedTo(this)) ||
				cell->agentTextRevision() != capture.textRevisions[col] ||
				cell->agentPresentationRevision() != capture.presentationRevisions[col] ||
				cell->getX() != g.x || cell->getY() != g.y || cell->getWidth() != g.w || cell->getHeight() != g.h)
			{ rows.clear(); return false; }
		}
		if (row.bounds.y < _agentPromotionClip.y ||
			row.bounds.y+row.bounds.h > _agentPromotionClip.y+_agentPromotionClip.h ||
			row.bounds.x >= _agentPromotionClip.x+_agentPromotionClip.w ||
			row.bounds.x+row.bounds.w <= _agentPromotionClip.x) continue;
		for (AgentPurchaseCell &cell : row.cells)
		{
			for (SDL_Rect &region : cell.regions)
			{
				const int left = std::max<int>(region.x, _agentPromotionClip.x);
				const int right = std::min<int>(region.x+region.w, _agentPromotionClip.x+_agentPromotionClip.w);
				if (left >= right) { cell.known = false; break; }
				region.x = left; region.w = right-left;
				const Observer::AgentUiRect band = {region.x,region.y,region.w,region.h};
				if (!Observer::agentRegionsUncovered({band}, this, _agentPaintOrder)) cell.known = false;
			}
			if (!cell.known) { cell.text.clear(); cell.regions.clear(); }
		}
		rows.push_back(row);
	}
	return true;
}

void TextList::agentEnableReportCells(unsigned columns)
{
	if (columns < 1 || columns > 5) return;
	if (!_agentReportCells)
	{
		_agentReportColumns = columns;
		_agentReportCells = true; ++_agentListRevision; _redraw = true;
		agentTrackPresentation();
	}
}

void TextList::agentCaptureReportRow(size_t logicalIndex, int y)
{
	if (!_agentReportCells) return;
	if (!_font || _comboBox || _wrap || _condensed || _arrowPos != -1 || _columns.size() != _agentReportColumns || logicalIndex >= _texts.size() ||
		_texts[logicalIndex].size() != _agentReportColumns || getWidth() <= 0 || getWidth() > 640 ||
		getHeight() <= 0 || getHeight() > 400)
	{ _agentReportCaptureHealthy = false; return; }
	// Use the ordinary draw allocation, not caption metrics or the selection grid.
	const int allocationHeight = _texts[logicalIndex][0]->getHeight();
	const int height = allocationHeight + _font->getSpacing();
	// Stock small-font allocation9/spacing-1 advances by8. Observe that actual
	// row strip, excluding the spare bottom pixel shared with the following row.
	// Other negative overlap and malformed column allocations remain refused.
	if (_margin < 0 || _margin > 640 || _font->getSpacing() < -1 || allocationHeight <= 0)
	{ _agentReportCaptureHealthy = false; return; }
	// Flooding deliberately allocates 340px children; bind actual drawn geometry.
	for (size_t col = 0; col < _agentReportColumns; ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		if (!_columns[col] || _columns[col] > 640 || cell->getX() < -640 || cell->getX() > 640 ||
			cell->getWidth() <= 0 || cell->getWidth() > 640 || cell->getY() != y ||
			cell->getHeight() != allocationHeight)
		{ _agentReportCaptureHealthy = false; return; }
	}
	if (height <= 0) { _agentReportCaptureHealthy = false; return; }
	if (y < 0 || y + height > getHeight()) return;
	if (_agentDrawReportRows.size() >= 128) { _agentReportCaptureHealthy = false; return; }
	AgentReportCapture capture = {};
	capture.row.logicalIndex = logicalIndex; // Private cell identity only; never a public row handle.
	capture.row.bounds = {static_cast<Sint16>(getX()), static_cast<Sint16>(getY()+y),
		static_cast<Uint16>(getWidth()), static_cast<Uint16>(height)};
	capture.row.cells.resize(_agentReportColumns);
	for (size_t col = 0; col < _agentReportColumns; ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		capture.cells[col] = cell;
		capture.textRevisions[col] = cell->agentTextRevision();
		capture.presentationRevisions[col] = cell->agentPresentationRevision();
		capture.geometry[col] = {static_cast<Sint16>(cell->getX()), static_cast<Sint16>(cell->getY()),
			static_cast<Uint16>(cell->getWidth()), static_cast<Uint16>(cell->getHeight())};
		// Preserve the current child caption only where its actual allocation
		// intersects the viewport. Flooding width and dot padding are native UI.
		const int left = std::max(0, cell->getX());
		const int right = std::min(getWidth(), cell->getX()+cell->getWidth());
		AgentPurchaseCell &value = capture.row.cells[col];
		value.known = cell->agentPresentationPaintedTo(this) && cell->getY() == y &&
			cell->getHeight() == allocationHeight && left < right &&
			cell->getText().size() <= 1024;
		if (value.known)
		{
			value.text = cell->getText();
			// Rendering toggles color for this token; it is not visible text.
			value.text.erase(std::remove(value.text.begin(), value.text.end(), Unicode::TOK_COLOR_FLIP), value.text.end());
			value.regions.push_back({static_cast<Sint16>(getX()+left), static_cast<Sint16>(getY()+y),
				static_cast<Uint16>(right-left), static_cast<Uint16>(height)});
		}
	}
	_agentDrawReportRows.push_back(capture);
}

bool TextList::agentReportRows(std::vector<AgentOutfittingRow> &rows, std::uint64_t &captureEpoch) const
{
	rows.clear(); captureEpoch = 0;
	if (!_agentReportPaintEpoch || _agentReportPaintEpoch != _agentReportDrawEpoch || _agentReportEpochExhausted || !_agentReportCells || !_agentReportCaptureHealthy || !Observer::agentWidgetVisible(this) ||
		getWidth() <= 0 || getWidth() > 640 || getHeight() <= 0 || getHeight() > 400 ||
		!agentPresentationPaintedTo(_agentReportDestination) ||
		_agentPaintRevision != _agentListRevision || !Observer::agentPaintHealthy() ||
		_comboBox || _wrap || _condensed || _columns.size() != _agentReportColumns ||
		_arrowPos != -1 || _agentPaintReportRows.size() > 128) return false;
	for (const AgentReportCapture &capture : _agentPaintReportRows)
	{
		AgentOutfittingRow row = capture.row;
		if (row.logicalIndex >= _texts.size() || _texts[row.logicalIndex].size() != _agentReportColumns)
		{ rows.clear(); return false; }
		for (size_t col = 0; col < _agentReportColumns; ++col)
		{
			const Text *cell = _texts[row.logicalIndex][col];
			const SDL_Rect &g = capture.geometry[col];
			if (cell != capture.cells[col] || cell->_redraw ||
				(capture.row.cells[col].known && !cell->agentPresentationPaintedTo(this)) ||
				cell->agentTextRevision() != capture.textRevisions[col] ||
				cell->agentPresentationRevision() != capture.presentationRevisions[col] ||
				cell->getX() != g.x || cell->getY() != g.y || cell->getWidth() != g.w || cell->getHeight() != g.h)
			{ rows.clear(); return false; }
		}
		if (row.bounds.y < _agentReportClip.y ||
			row.bounds.y+row.bounds.h > _agentReportClip.y+_agentReportClip.h ||
			row.bounds.x >= _agentReportClip.x+_agentReportClip.w ||
			row.bounds.x+row.bounds.w <= _agentReportClip.x) continue;
		for (AgentPurchaseCell &cell : row.cells)
		{
			for (SDL_Rect &region : cell.regions)
			{
				const int left = std::max<int>(region.x, _agentReportClip.x);
				const int right = std::min<int>(region.x+region.w, _agentReportClip.x+_agentReportClip.w);
				if (left >= right) { cell.known = false; break; }
				region.x = left; region.w = right-left;
				const Observer::AgentUiRect band = {region.x,region.y,region.w,region.h};
				if (!Observer::agentRegionsUncovered({band}, this, _agentPaintOrder)) cell.known = false;
			}
			if (!cell.known) { cell.text.clear(); cell.regions.clear(); }
		}
		rows.push_back(row);
	}
	captureEpoch = _agentReportPaintEpoch;
	return true;
}

void TextList::agentEnableSaveListCells(bool newSlot)
{
	if (!_agentSaveListCells)
	{
		_agentSaveListNewSlot = newSlot; _agentSaveListCells = true; ++_agentListRevision; _redraw = true;
		agentTrackPresentation();
	}
}

void TextList::agentCaptureSaveListRow(size_t logicalIndex, int y)
{
	if (!_agentSaveListCells) return;
	if (!_font || _comboBox || _dot || _wrap || _condensed ||
		_flooding || _arrowPos != -1 || _columns.size() != 3 || logicalIndex >= _texts.size() ||
		_texts[logicalIndex].size() != ((_agentSaveListNewSlot && logicalIndex == 0) ? 1u : 3u) || getWidth() <= 0 || getWidth() > 640 ||
		getHeight() <= 0 || getHeight() > 400)
	{ _agentSaveListCaptureHealthy = false; return; }
	// Use the ordinary draw allocation, not caption metrics or the selection grid.
	const int allocationHeight = _texts[logicalIndex][0]->getHeight();
	const int height = allocationHeight + _font->getSpacing();
	// Stock small-font allocation9/spacing-1 advances by8. Observe that actual
	// row strip, excluding the spare bottom pixel shared with the following row.
	// Other negative overlap and malformed column allocations remain refused.
	if (_margin < 0 || _margin > 640 || _font->getSpacing() < -1 || allocationHeight <= 0)
	{ _agentSaveListCaptureHealthy = false; return; }
	int expectedX = _margin;
	for (size_t col = 0; col < _texts[logicalIndex].size(); ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		if (!_columns[col] || _columns[col] > 640 || cell->getX() != expectedX ||
			cell->getWidth() != static_cast<int>(_columns[col]) || cell->getY() != y ||
			cell->getHeight() != allocationHeight)
		{ _agentSaveListCaptureHealthy = false; return; }
		expectedX += static_cast<int>(_columns[col]);
	}
	if (height <= 0) { _agentSaveListCaptureHealthy = false; return; }
	if (y < 0 || y + height > getHeight()) return;
	if (_agentDrawSaveListRows.size() >= 128) { _agentSaveListCaptureHealthy = false; return; }
	AgentSaveListCapture capture = {};
	capture.row.logicalIndex = logicalIndex; // Private cell identity only; never a public row handle.
	capture.row.bounds = {static_cast<Sint16>(getX()), static_cast<Sint16>(getY()+y),
		static_cast<Uint16>(getWidth()), static_cast<Uint16>(height)};
	capture.row.cells.resize(_texts[logicalIndex].size());
	for (size_t col = 0; col < _texts[logicalIndex].size(); ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		capture.cells[col] = cell;
		capture.textRevisions[col] = cell->agentTextRevision();
		capture.presentationRevisions[col] = cell->agentPresentationRevision();
		capture.geometry[col] = {static_cast<Sint16>(cell->getX()), static_cast<Sint16>(cell->getY()),
			static_cast<Uint16>(cell->getWidth()), static_cast<Uint16>(cell->getHeight())};
		// Native margin8 clips the third allocation by8px. Preserve its logical
		// caption when the allocation intersects the viewport; never inspect glyphs.
		const int left = std::max(0, cell->getX());
		const int right = std::min(getWidth(), cell->getX()+cell->getWidth());
		AgentPurchaseCell &value = capture.row.cells[col];
		value.known = cell->agentPresentationPaintedTo(this) && cell->getY() == y &&
			cell->getHeight() == allocationHeight && left < right &&
			cell->getText().size() <= 1024;
		if (value.known)
		{
			value.text = cell->getText();
			value.regions.push_back({static_cast<Sint16>(getX()+left), static_cast<Sint16>(getY()+y),
				static_cast<Uint16>(right-left), static_cast<Uint16>(height)});
		}
	}
	_agentDrawSaveListRows.push_back(capture);
}

bool TextList::agentSaveListRows(std::vector<AgentOutfittingRow> &rows) const
{
	rows.clear();
	if (!_agentSaveListCells || !_agentSaveListCaptureHealthy || !Observer::agentWidgetVisible(this) ||
		getWidth() <= 0 || getWidth() > 640 || getHeight() <= 0 || getHeight() > 400 ||
		!agentPresentationPaintedTo(_agentSaveListDestination) ||
		_agentPaintRevision != _agentListRevision || !Observer::agentPaintHealthy() ||
		_comboBox || _dot || _wrap || _condensed || _flooding || _columns.size() != 3 ||
		_arrowPos != -1 || _agentPaintSaveListRows.size() > 128) return false;
	for (const AgentSaveListCapture &capture : _agentPaintSaveListRows)
	{
		AgentOutfittingRow row = capture.row;
		if (row.logicalIndex >= _texts.size() || _texts[row.logicalIndex].size() != capture.row.cells.size())
		{ rows.clear(); return false; }
		for (size_t col = 0; col < capture.row.cells.size(); ++col)
		{
			const Text *cell = _texts[row.logicalIndex][col];
			const SDL_Rect &g = capture.geometry[col];
			if (cell != capture.cells[col] || cell->_redraw ||
				(capture.row.cells[col].known && !cell->agentPresentationPaintedTo(this)) ||
				cell->agentTextRevision() != capture.textRevisions[col] ||
				cell->agentPresentationRevision() != capture.presentationRevisions[col] ||
				cell->getX() != g.x || cell->getY() != g.y || cell->getWidth() != g.w || cell->getHeight() != g.h)
			{ rows.clear(); return false; }
		}
		if (row.bounds.y < _agentSaveListClip.y ||
			row.bounds.y+row.bounds.h > _agentSaveListClip.y+_agentSaveListClip.h ||
			row.bounds.x >= _agentSaveListClip.x+_agentSaveListClip.w ||
			row.bounds.x+row.bounds.w <= _agentSaveListClip.x) continue;
		for (AgentPurchaseCell &cell : row.cells)
		{
			for (SDL_Rect &region : cell.regions)
			{
				const int left = std::max<int>(region.x, _agentSaveListClip.x);
				const int right = std::min<int>(region.x+region.w, _agentSaveListClip.x+_agentSaveListClip.w);
				if (left >= right) { cell.known = false; break; }
				region.x = left; region.w = right-left;
				const Observer::AgentUiRect band = {region.x,region.y,region.w,region.h};
				if (!Observer::agentRegionsUncovered({band}, this, _agentPaintOrder)) cell.known = false;
			}
			if (!cell.known) { cell.text.clear(); cell.regions.clear(); }
		}
		rows.push_back(row);
	}
	return true;
}

bool TextList::agentSelectSaveListRow(size_t logicalIndex)
{
    std::vector<AgentOutfittingRow> rows;
    if (!agentSaveListRows(rows)) return false;
    for (const auto &row : rows) if (row.logicalIndex == logicalIndex)
    {
        const auto physical = std::find(_rows.begin(), _rows.end(), logicalIndex);
        if (physical == _rows.end() || std::count(_rows.begin(), _rows.end(), logicalIndex) != 1) return false;
        selectPhysicalRow(physical - _rows.begin()); return true;
    }
    return false;
}

void TextList::agentEnableArrivalLogicalCells()
{
	if (!_agentArrivalLogicalCells)
	{
		_agentArrivalLogicalCells = true; ++_agentListRevision; _redraw = true;
		agentTrackPresentation();
		_up->agentTrackPresentation(); _down->agentTrackPresentation();
		_scrollbar->agentTrackPresentation();
	}
}

void TextList::agentCaptureArrivalLogicalRow(size_t logicalIndex, int y)
{
	if (!_agentArrivalLogicalCells) return;
	if (!_font || _comboBox || _dot || _wrap || _condensed ||
		_flooding || _arrowPos != -1 || _columns.size() != 3 || logicalIndex >= _texts.size() ||
		_texts[logicalIndex].size() != 3 || getWidth() <= 0 || getWidth() > 640 ||
		getHeight() <= 0 || getHeight() > 400)
	{ _agentArrivalLogicalCaptureHealthy = false; return; }
	// Use the ordinary draw allocation, not caption metrics or the selection grid.
	const int allocationHeight = _texts[logicalIndex][0]->getHeight();
	const int height = allocationHeight + _font->getSpacing();
	// Stock small-font allocation9/spacing-1 advances by8. Observe that actual
	// row strip, excluding the spare bottom pixel shared with the following row.
	// Other negative overlap and malformed column allocations remain refused.
	if (_margin < 0 || _margin > 640 || _font->getSpacing() < -1 || allocationHeight <= 0)
	{ _agentArrivalLogicalCaptureHealthy = false; return; }
	int expectedX = _margin;
	for (size_t col = 0; col < 3; ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		if (!_columns[col] || _columns[col] > 640 || cell->getX() != expectedX ||
			cell->getWidth() != static_cast<int>(_columns[col]) || cell->getY() != y ||
			cell->getHeight() != allocationHeight)
		{ _agentArrivalLogicalCaptureHealthy = false; return; }
		expectedX += static_cast<int>(_columns[col]);
	}
	if (height <= 0) { _agentArrivalLogicalCaptureHealthy = false; return; }
	if (y < 0 || y + height > getHeight()) return;
	if (_agentDrawArrivalLogicalRows.size() >= 128) { _agentArrivalLogicalCaptureHealthy = false; return; }
	AgentArrivalLogicalCapture capture = {};
	capture.row.logicalIndex = logicalIndex; // Private cell identity only; never a public row handle.
	capture.row.bounds = {static_cast<Sint16>(getX()), static_cast<Sint16>(getY()+y),
		static_cast<Uint16>(getWidth()), static_cast<Uint16>(height)};
	for (size_t col = 0; col < 3; ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		capture.cells[col] = cell;
		capture.textRevisions[col] = cell->agentTextRevision();
		capture.presentationRevisions[col] = cell->agentPresentationRevision();
		capture.geometry[col] = {static_cast<Sint16>(cell->getX()), static_cast<Sint16>(cell->getY()),
			static_cast<Uint16>(cell->getWidth()), static_cast<Uint16>(cell->getHeight())};
		// Native margin2/columns155,41,98 clips the final allocation by25px. Preserve its logical
		// caption when the allocation intersects the viewport; never inspect glyphs.
		const int left = std::max(0, cell->getX());
		const int right = std::min(getWidth(), cell->getX()+cell->getWidth());
		AgentPurchaseCell &value = capture.row.cells[col];
		value.known = cell->agentPresentationPaintedTo(this) && cell->getY() == y &&
			cell->getHeight() == allocationHeight && left < right &&
			cell->getText().size() <= 1024;
		if (value.known)
		{
			UString shown = Unicode::convUtf8ToUtf32(cell->getText());
			shown.erase(std::remove(shown.begin(), shown.end(), Unicode::TOK_COLOR_FLIP), shown.end());
			value.text = Unicode::convUtf32ToUtf8(shown);
			value.regions.push_back({static_cast<Sint16>(getX()+left), static_cast<Sint16>(getY()+y),
				static_cast<Uint16>(right-left), static_cast<Uint16>(height)});
		}
	}
	_agentDrawArrivalLogicalRows.push_back(capture);
}

bool TextList::agentArrivalLogicalRows(std::vector<AgentArrivalRow> &rows, std::uint64_t &captureEpoch) const
{
	rows.clear(); captureEpoch = 0;
	if (!_agentArrivalLogicalCells || !_agentArrivalLogicalCaptureHealthy || _agentArrivalLogicalEpochExhausted ||
		!_agentArrivalLogicalPaintEpoch || _agentArrivalLogicalPaintEpoch != _agentArrivalLogicalDrawEpoch || !Observer::agentWidgetVisible(this) ||
		getWidth() <= 0 || getWidth() > 640 || getHeight() <= 0 || getHeight() > 400 ||
		!agentPresentationPaintedTo(_agentArrivalLogicalDestination) ||
		_agentPaintRevision != _agentListRevision || !Observer::agentPaintHealthy() ||
		_comboBox || _dot || _wrap || _condensed || _flooding || _columns.size() != 3 ||
		_arrowPos != -1 || _agentPaintArrivalLogicalRows.size() > 128) return false;
	for (const AgentArrivalLogicalCapture &capture : _agentPaintArrivalLogicalRows)
	{
		AgentArrivalRow row = capture.row;
		if (row.logicalIndex >= _texts.size() || _texts[row.logicalIndex].size() != 3)
		{ rows.clear(); return false; }
		for (size_t col = 0; col < 3; ++col)
		{
			const Text *cell = _texts[row.logicalIndex][col];
			const SDL_Rect &g = capture.geometry[col];
			if (cell != capture.cells[col] || cell->_redraw ||
				(capture.row.cells[col].known && !cell->agentPresentationPaintedTo(this)) ||
				cell->agentTextRevision() != capture.textRevisions[col] ||
				cell->agentPresentationRevision() != capture.presentationRevisions[col] ||
				cell->getX() != g.x || cell->getY() != g.y || cell->getWidth() != g.w || cell->getHeight() != g.h)
			{ rows.clear(); return false; }
		}
		if (row.bounds.y < _agentArrivalLogicalClip.y ||
			row.bounds.y+row.bounds.h > _agentArrivalLogicalClip.y+_agentArrivalLogicalClip.h ||
			row.bounds.x >= _agentArrivalLogicalClip.x+_agentArrivalLogicalClip.w ||
			row.bounds.x+row.bounds.w <= _agentArrivalLogicalClip.x) continue;
		for (AgentPurchaseCell &cell : row.cells)
		{
			for (SDL_Rect &region : cell.regions)
			{
				const int left = std::max<int>(region.x, _agentArrivalLogicalClip.x);
				const int right = std::min<int>(region.x+region.w, _agentArrivalLogicalClip.x+_agentArrivalLogicalClip.w);
				if (left >= right) { cell.known = false; break; }
				region.x = left; region.w = right-left;
				const Observer::AgentUiRect band = {region.x,region.y,region.w,region.h};
				if (!Observer::agentRegionsUncovered({band}, this, _agentPaintOrder)) cell.known = false;
			}
			if (!cell.known) { cell.text.clear(); cell.regions.clear(); }
		}
		rows.push_back(row);
	}
	captureEpoch = _agentArrivalLogicalPaintEpoch;
	return true;
}

void TextList::agentEnablePsiTrainingCells()
{
	if (!_agentPsiTrainingCells)
	{
		_agentPsiTrainingCells = true; ++_agentListRevision; _redraw = true;
		// Nested controls are not separate State surfaces; track their ordinary blits.
		_up->agentTrackPresentation(); _down->agentTrackPresentation();
		_scrollbar->agentTrackPresentation();
	}
}

void TextList::agentCapturePsiRow(size_t logicalIndex, int y)
{
	if (!_agentPsiTrainingCells || !_font || _comboBox || !_selectable || !_bg || !_selector ||
		_dot || _wrap || _condensed || _flooding || _arrowPos != -1 || _columns.size() != 4 ||
		logicalIndex >= _texts.size() || _texts[logicalIndex].size() != 4) return;
	// The native one-line click grid is independent of caption length/font fallback.
	const int step = _font->getHeight() + _font->getSpacing();
	const auto physical = std::find(_rows.begin(), _rows.end(), logicalIndex);
	if (step <= 0 || step > 400 || physical == _rows.end() ||
		std::count(_rows.begin(), _rows.end(), logicalIndex) != 1 ||
		y != (static_cast<int>(physical - _rows.begin()) - static_cast<int>(_scroll)) * step ||
		y < 0 || y + step > getHeight() || getWidth() <= 0 || getWidth() > 640 || getHeight() > 400) return;
	const SDL_Surface *target = getSurface();
	if (!target || target->clip_rect.x != 0 || target->clip_rect.y != 0 ||
		target->clip_rect.w != getWidth() || target->clip_rect.h != getHeight()) return;
	AgentPsiCapture capture;
	capture.row.logicalIndex = logicalIndex;
	capture.row.bounds = {static_cast<Sint16>(getX()), static_cast<Sint16>(getY()+y),
		static_cast<Uint16>(getWidth()), static_cast<Uint16>(step)};
	capture.row.clickBand = capture.row.bounds;
	capture.row.cells.resize(4);
	int x = _margin;
	for (size_t col = 0; col < 4; ++col)
	{
		const Text *cell = _texts[logicalIndex][col];
		capture.cells[col] = cell;
		capture.textRevisions[col] = cell->agentTextRevision();
		capture.presentationRevisions[col] = cell->agentPresentationRevision();
		capture.geometry[col] = {static_cast<Sint16>(cell->getX()), static_cast<Sint16>(cell->getY()),
			static_cast<Uint16>(cell->getWidth()), static_cast<Uint16>(cell->getHeight())};
		AgentPurchaseCell &value = capture.row.cells[col];
		// Only allocation/paint/visibility; no measured text width, glyphs or pixels.
		value.known = cell->agentPresentationPaintedTo(this) &&
			cell->getX() == x && cell->getY() == y && x >= 0 && _columns[col] > 0 &&
			_columns[col] <= static_cast<size_t>(getWidth()) &&
			x + static_cast<int>(_columns[col]) <= getWidth() && cell->getText().size() <= 1024;
		if (value.known)
		{
			value.text = cell->getText();
			value.regions.push_back({static_cast<Sint16>(getX()+x), static_cast<Sint16>(getY()+y),
				static_cast<Uint16>(_columns[col]), static_cast<Uint16>(step)});
		}
		x += static_cast<int>(_columns[col]);
	}
	_agentDrawPsiRows.push_back(capture);
}

bool TextList::agentPsiTrainingRows(std::vector<AgentOutfittingRow> &rows) const
{
	rows.clear();
	if (!_agentPsiTrainingCells || !Observer::agentWidgetVisible(this) ||
		_agentPaintRevision != _agentListRevision || !Observer::agentPaintHealthy() ||
		_comboBox || !_selectable || _dot || _wrap || _condensed || _flooding ||
		_columns.size() != 4 || _arrowPos != -1 || _selector != _agentPsiSelector ||
		!_selector || _selector->agentPresentationRevision() != _agentPsiSelectorRevision) return false;
	const auto inClip = [&](const SDL_Rect &r) { return r.w && r.h &&
		r.x >= _agentPsiClip.x && r.y >= _agentPsiClip.y &&
		r.x+r.w <= _agentPsiClip.x+_agentPsiClip.w && r.y+r.h <= _agentPsiClip.y+_agentPsiClip.h; };
	for (const AgentPsiCapture &capture : _agentPaintPsiRows)
	{
		AgentOutfittingRow row = capture.row;
		if (row.logicalIndex >= _texts.size() || _texts[row.logicalIndex].size() != 4) return false;
		for (size_t col = 0; col < 4; ++col)
		{
			const Text *cell = _texts[row.logicalIndex][col];
			const SDL_Rect &g = capture.geometry[col];
			if (cell != capture.cells[col] || cell->_redraw ||
				(capture.row.cells[col].known && !cell->agentPresentationPaintedTo(this)) ||
				cell->agentTextRevision() != capture.textRevisions[col] ||
				cell->agentPresentationRevision() != capture.presentationRevisions[col] ||
				cell->getX() != g.x || cell->getY() != g.y || cell->getWidth() != g.w || cell->getHeight() != g.h)
			{ rows.clear(); return false; }
		}
		const Observer::AgentUiRect band = {row.bounds.x,row.bounds.y,row.bounds.w,row.bounds.h};
		if (!inClip(row.bounds) || !Observer::agentRegionsUncovered({band}, this, _agentPaintOrder)) continue;
		rows.push_back(row);
	}
	return true;
}

bool TextList::agentSelectPsiTrainingRow(size_t logicalIndex)
{
	std::vector<AgentOutfittingRow> rows;
	if (!agentPsiTrainingRows(rows)) return false;
	for (const AgentOutfittingRow &row : rows) if (row.logicalIndex == logicalIndex)
	{
		const auto physical = std::find(_rows.begin(), _rows.end(), logicalIndex);
		if (physical == _rows.end() || std::count(_rows.begin(), _rows.end(), logicalIndex) != 1) return false;
		selectPhysicalRow(physical - _rows.begin()); return true;
	}
	return false;
}

// Facility-only entry to the existing independent cell capture machinery.
// Public generic outfitting opt-in still rejects one column; no other state
// gains this route implicitly. The normal renderer/layout is unchanged.
void TextList::agentEnableFacilityCells()
{
    if (_agentOutfittingColumns != 1 || _agentOutfittingEquipment || _agentOutfittingCrew) {
        _agentOutfittingColumns=1; _agentOutfittingEquipment=false; _agentOutfittingCrew=false;
        ++_agentListRevision; _redraw=true; agentTrackPresentation();
    }
}
bool TextList::agentFacilityRows(const Surface *destination, std::vector<AgentOutfittingRow> &rows) const
{
    rows.clear();
    if (_agentOutfittingColumns!=1 || _agentOutfittingEquipment || _agentOutfittingCrew ||
        _columns.size()!=1 || _comboBox || !_selectable || !_bg || !_selector || _arrowPos!=-1 ||
        _agentPaintRevision!=_agentListRevision || !_agentCoverageHealthy || !Observer::agentPaintHealthy() ||
        !Observer::agentWidgetVisible(this) || !agentPresentationPaintedTo(destination)) return false;
    for (AgentOutfittingRow row:_agentPaintOutfittingRows) {
        if(row.cells.size()!=1 || !row.cells[0].known || row.cells[0].text.empty() || row.logicalIndex>=_texts.size() ||
            _texts[row.logicalIndex].size()!=1 || std::count(_rows.begin(),_rows.end(),row.logicalIndex)!=1)continue;
        const Text *cell=_texts[row.logicalIndex][0];
        if(!cell->agentContextVisible() || !cell->agentPresentationPaintedTo(this) || cell->getY()+getY()!=row.bounds.y ||
            row.bounds.x!=getX() || row.bounds.w!=getWidth())continue;
        UString shown=Unicode::convUtf8ToUtf32(cell->getText());
        shown.erase(std::remove(shown.begin(),shown.end(),Unicode::TOK_COLOR_FLIP),shown.end());
        if(Unicode::convUtf32ToUtf8(shown)!=row.cells[0].text)continue;
        const auto &r=row.clickBand;
        if(!r.w || !r.h || r.x<_agentOutfittingClip.x || r.y<_agentOutfittingClip.y ||
            r.x+r.w>_agentOutfittingClip.x+_agentOutfittingClip.w || r.y+r.h>_agentOutfittingClip.y+_agentOutfittingClip.h ||
            r.x<getX() || r.y<getY() || r.x+r.w>getX()+getWidth() || r.y+r.h>getY()+getHeight())continue;
        if(!Observer::agentRegionsUncovered({{r.x,r.y,r.w,r.h}},this,_agentPaintOrder))continue;
        row.selected=row.selected && _selector->agentPresentationPaintedTo(destination);
        rows.push_back(row);
    }
    return true;
}
bool TextList::agentSelectFacilityRow(const Surface *destination,size_t logicalIndex)
{
    std::vector<AgentOutfittingRow> rows;if(!agentFacilityRows(destination,rows))return false;
    for(const auto &row:rows)if(row.logicalIndex==logicalIndex){
        const auto physical=std::find(_rows.begin(),_rows.end(),logicalIndex);
        if(physical==_rows.end() || std::count(_rows.begin(),_rows.end(),logicalIndex)!=1)return false;
        selectPhysicalRow(physical-_rows.begin());return true;
    }
    return false;
}

void TextList::agentEnableOutfitting(unsigned columns, bool equipment, bool crew)
{
	if ((columns != 2 && columns != 3 && columns != 5) || (equipment && (columns != 3 || crew))) return;
	if (_agentOutfittingColumns != columns || _agentOutfittingEquipment != equipment || _agentOutfittingCrew != crew)
	{ _agentOutfittingColumns = columns; _agentOutfittingEquipment = equipment; _agentOutfittingCrew = crew; ++_agentListRevision; _redraw = true; }
}

bool TextList::agentOutfittingRows(std::vector<AgentOutfittingRow> &rows) const
{
	rows.clear();
	if (!_agentOutfittingColumns || !Observer::agentWidgetVisible(this) || _agentPaintRevision != _agentListRevision ||
		!_agentCoverageHealthy || !Observer::agentPaintHealthy() || _comboBox || _columns.size() != _agentOutfittingColumns) return false;
	const auto inClip = [&](const SDL_Rect &r) { return r.w && r.h && r.x >= _agentOutfittingClip.x && r.y >= _agentOutfittingClip.y &&
		r.x+r.w <= _agentOutfittingClip.x+_agentOutfittingClip.w && r.y+r.h <= _agentOutfittingClip.y+_agentOutfittingClip.h; };
	for (AgentOutfittingRow row : _agentPaintOutfittingRows)
	{
		if (_agentOutfittingEquipment)
		{
			if (row.logicalIndex >= _arrowLeft.size() || row.logicalIndex >= _arrowRight.size()) continue;
			const auto arrow = [&](const Surface *part) -> const Surface * {
				if (!Observer::agentWidgetUncovered(part, _agentPaintOrder)) return nullptr;
				const SDL_Rect r = {static_cast<Sint16>(part->getX()), static_cast<Sint16>(part->getY()),
					static_cast<Uint16>(part->getWidth()), static_cast<Uint16>(part->getHeight())};
				return inClip(r) && r.x >= row.bounds.x && r.y == row.bounds.y && r.x+r.w <= row.bounds.x+row.bounds.w &&
					r.y+r.h <= row.bounds.y+row.bounds.h ? part : nullptr;
			};
			row.decrease = arrow(_arrowLeft[row.logicalIndex]); row.increase = arrow(_arrowRight[row.logicalIndex]);
			if (!row.increase && !row.decrease) continue;
		}
		else
		{
			const Observer::AgentUiRect band = {row.clickBand.x,row.clickBand.y,row.clickBand.w,row.clickBand.h};
			if (row.controlInk.empty() || !inClip(row.clickBand) || !Observer::agentRegionsUncovered({band}, this, _agentPaintOrder)) continue;
		}
		for (AgentPurchaseCell &cell : row.cells)
		{
			std::vector<Observer::AgentUiRect> regions;
			for (const SDL_Rect &r : cell.regions) { if (!inClip(r)) cell.known = false; regions.push_back({r.x,r.y,r.w,r.h}); }
			if (!Observer::agentRegionsUncovered(regions, this, _agentPaintOrder)) cell.known = false;
			if (!cell.known) cell.regions.clear();
		}
		if (!inClip(row.bounds)) row.selected = false;
		rows.push_back(row);
	}
	return true;
}

bool TextList::agentSelectOutfittingRow(size_t logicalIndex)
{
	std::vector<AgentOutfittingRow> rows;
	if (!agentOutfittingRows(rows)) return false;
	for (const AgentOutfittingRow &row : rows) if (row.logicalIndex == logicalIndex)
	{
		const auto physical = std::find(_rows.begin(), _rows.end(), logicalIndex);
		if (physical == _rows.end() || std::count(_rows.begin(), _rows.end(), logicalIndex) != 1) return false;
		selectPhysicalRow(physical - _rows.begin()); return true;
	}
	return false;
}

void TextList::agentEnableArrivalCells()
{
	if (!_agentArrivalCells) { _agentArrivalCells = true; ++_agentListRevision; _redraw = true; }
}

bool TextList::agentArrivalRows(std::vector<AgentArrivalRow> &rows) const
{
	rows.clear();
	if (!_agentArrivalCells || !Observer::agentWidgetVisible(this) || _agentPaintRevision != _agentListRevision ||
		!_agentCoverageHealthy || !Observer::agentPaintHealthy() || _comboBox || _columns.size() != 3 || _arrowPos != -1) return false;
	for (AgentArrivalRow row : _agentPaintArrivalRows)
	{
		for (AgentPurchaseCell &cell : row.cells)
		{
			std::vector<Observer::AgentUiRect> regions;
			for (const SDL_Rect &r : cell.regions)
			{
				if (r.x < _agentArrivalClip.x || r.y < _agentArrivalClip.y ||
					r.x + r.w > _agentArrivalClip.x + _agentArrivalClip.w ||
					r.y + r.h > _agentArrivalClip.y + _agentArrivalClip.h) cell.known = false;
				regions.push_back({r.x, r.y, r.w, r.h});
			}
			if (!Observer::agentRegionsUncovered(regions, this, _agentPaintOrder)) cell.known = false;
			if (!cell.known) cell.regions.clear();
		}
		rows.push_back(row);
	}
	return true;
}

bool TextList::agentPurchaseRows(std::vector<AgentPurchaseRow> &rows) const
{
	rows.clear();
	if (!Observer::agentWidgetVisible(this) || _agentPaintRevision != _agentListRevision ||
		!_agentCoverageHealthy || !Observer::agentPaintHealthy() || _comboBox || _columns.size() != 4 ||
		_arrowPos == -1 || _arrowType != ARROW_VERTICAL) return false;
	const auto inClip = [&](const SDL_Rect &r) { return r.x >= _agentPurchaseClip.x && r.y >= _agentPurchaseClip.y &&
		r.x + r.w <= _agentPurchaseClip.x + _agentPurchaseClip.w && r.y + r.h <= _agentPurchaseClip.y + _agentPurchaseClip.h; };
	const auto arrow = [&](const Surface *part, const AgentPurchaseRow &row) -> const Surface * {
		if (!Observer::agentWidgetUncovered(part, _agentPaintOrder)) return nullptr;
		const SDL_Rect rect = {static_cast<Sint16>(part->getX()), static_cast<Sint16>(part->getY()),
			static_cast<Uint16>(part->getWidth()), static_cast<Uint16>(part->getHeight())};
		if (!inClip(rect) || rect.x < getX() || rect.y != row.bounds.y || rect.y + rect.h > row.bounds.y + row.bounds.h ||
			rect.x + rect.w > getX() + getWidth()) return nullptr;
		return part;
	};
	for (AgentPurchaseRow row : _agentPaintPurchaseRows)
	{
		if (row.logicalIndex >= _arrowLeft.size() || row.logicalIndex >= _arrowRight.size()) continue;
		row.increase = arrow(_arrowLeft[row.logicalIndex], row);
		row.decrease = arrow(_arrowRight[row.logicalIndex], row);
		if (!row.increase && !row.decrease) continue;
		for (AgentPurchaseCell &cell : row.cells)
		{
			std::vector<Observer::AgentUiRect> regions;
			for (const SDL_Rect &r : cell.regions)
			{
				if (!inClip(r)) cell.known = false;
				regions.push_back({r.x, r.y, r.w, r.h});
			}
			if (!Observer::agentRegionsUncovered(regions, this, _agentPaintOrder)) cell.known = false;
			if (!cell.known) cell.regions.clear();
		}
		if (!inClip(row.bounds)) row.selected = false;
		rows.push_back(row);
	}
	return true;
}

ScrollBar *TextList::agentPurchaseScrollbar() const
{
	return Observer::agentWidgetVisible(this) && _agentPaintRevision == _agentListRevision &&
		_scrolling && !_comboBox && _columns.size() == 4 && _arrowPos != -1 ? _scrollbar : nullptr;
}

bool TextList::agentControlRows(std::vector<AgentListRow> &rows) const
{
	rows.clear();
	std::vector<AgentListRow> complete;
	if (_comboBox || !_selectable || !_bg || !_selector || _columns.size() != 1 || !agentRows(complete)) return false;
	for (AgentListRow row : _agentPaintControls)
	{
		if (row.regions.empty()) continue;
		const Observer::AgentUiRect band = {row.bounds.x, row.bounds.y, row.bounds.w, row.bounds.h};
		if (!Observer::agentRegionsUncovered({band}, this, _agentPaintOrder)) continue;
		for (const AgentListRow &label : complete)
			if (label.logicalIndex == row.logicalIndex && label.cells.size() == 1 && !label.cells[0].empty())
			{ row.completeText = true; row.cells = label.cells; row.labelRegions = label.regions; break; }
		rows.push_back(row);
	}
	std::sort(rows.begin(), rows.end(), [](const AgentListRow &a, const AgentListRow &b) { return a.bounds.y < b.bounds.y; });
	return true;
}

void TextList::agentScrollControls(const Surface *&up, const Surface *&down) const
{
	up = down = nullptr;
	if (!Observer::agentWidgetVisible(this) || _agentPaintRevision != _agentListRevision || !_scrolling) return;
	// Ordinary UI draws both arrows at a boundary. Preserve those no-op clicks,
	// but a later child (including overlapping arrows in a tiny list) wins.
	if (Observer::agentWidgetUncovered(_up, _agentPaintOrder)) up = _up;
	if (Observer::agentWidgetUncovered(_down, _agentPaintOrder)) down = _down;
	if (!_comboBox && ((_columns.size() == 4 && _arrowPos != -1) || _agentOutfittingColumns || _agentPsiTrainingCells))
	{
		const auto clipped = [&](const Surface *part) { return part &&
			(part->getX() < _agentPurchaseClip.x || part->getY() < _agentPurchaseClip.y ||
			part->getX()+part->getWidth() > _agentPurchaseClip.x+_agentPurchaseClip.w ||
			part->getY()+part->getHeight() > _agentPurchaseClip.y+_agentPurchaseClip.h); };
		if (clipped(up)) up = nullptr;
		if (clipped(down)) down = nullptr;
	}
}

void TextList::agentPaintOrder(std::vector<Surface *> &out) const
{
	out.insert(out.end(), _agentPaintOrder.begin(), _agentPaintOrder.end());
}

bool TextList::agentSelectProjectRow(size_t logicalIndex, bool allowUnknownText)
{
	if (_comboBox || !_selectable || !_bg || !_selector || !_font) return false;
	std::vector<AgentListRow> rows;
	if (!(allowUnknownText ? agentControlRows(rows) : agentRows(rows))) return false;
	for (const AgentListRow &row : rows)
	{
		if (row.logicalIndex != logicalIndex) continue;
		if (row.cells.empty() || ((!allowUnknownText || row.completeText) && row.cells[0].empty()) ||
			std::count(_rows.begin(), _rows.end(), logicalIndex) != 1) return false;
		const auto physical = std::find(_rows.begin(), _rows.end(), logicalIndex);
		if (physical == _rows.end()) return false;
		selectPhysicalRow(physical - _rows.begin());
		return true;
	}
	return false;
}

bool TextList::agentChooseVisibleRow(size_t logicalIndex, Action *action, State *state)
{
	// Only the audited combo route is enabled. An arbitrary list callback may
	// require actual input details; semantic actions cannot fabricate them.
	if (!_comboBox || !_comboBox->agentPopupOpen() || !_selectable || !_bg || !_click.empty()) return false;
	std::vector<AgentListRow> rows;
	if (!agentRows(rows)) return false;
	for (const AgentListRow &row : rows)
	{
		if (row.logicalIndex != logicalIndex) continue;
		const auto physical = std::find(_rows.begin(), _rows.end(), logicalIndex);
		if (physical == _rows.end() || static_cast<size_t>(physical - _rows.begin()) != logicalIndex) return false;
		selectPhysicalRow(physical - _rows.begin());
		activateRow(action, state, SDL_BUTTON_LEFT);
		return true;
	}
	return false;
}
#endif

}
