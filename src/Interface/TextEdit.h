#pragma once
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
#include "../Engine/InteractiveSurface.h"
#include "Text.h"
#include "../Engine/Unicode.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include <cstdint>
#endif

namespace OpenXcom
{

class Timer;
enum TextEditConstraint { TEC_NONE, TEC_NUMERIC_POSITIVE, TEC_NUMERIC };

/**
 * Editable version of Text.
 * Receives keyboard input to allow the player
 * to change the text himself.
 */
class TextEdit : public InteractiveSurface
{
private:
	Text *_text, *_caret;
	UString _value;
	bool _blink, _modal;
	Timer *_timer;
	UCode _char;
	size_t _caretPos;
	TextEditConstraint _textEditConstraint;
	ActionHandler _change;
	State *_state;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	bool _agentCaretPainted = false;
	SDL_Rect _agentCaretBounds = {};
	std::uint64_t _agentInputRevision = 0;
#endif
	/// Native insertion shared by physical keyboard and bounded replacement.
	bool insertInputCharacter(UCode c);
	/// Checks if a character will exceed the maximum width.
	bool exceedsMaxWidth(UCode c) const;
	/// Checks if character is valid to be inserted at caret position.
	bool isValidChar(UCode c) const;
public:
	/// Creates a new text edit with the specified size and position.
	TextEdit(State *state, int width, int height, int x = 0, int y = 0);
	/// Cleans up the text edit.
	~TextEdit();
	/// Handle focus.
	void handle(Action *action, State *state) override;
	/// Sets focus on this text edit.
	void setFocus(bool focus, bool modal);
	// Override the base class' method properly.
	void setFocus(bool focus) override { setFocus(focus, true); };
	/// Sets the text size to big.
	void setBig();
	/// Sets the text size to small.
	void setSmall();
	/// Initializes the text edit's resources.
	void initText(Font *big, Font *small, Language *lang) override;
	/// Sets the text's string.
	void setText(const std::string &text);
	/// Gets the text edit's string.
	std::string getText() const;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	/// Reads only the complete currently rendered child text.
	bool agentRenderedText(std::string &text) const;
    // Complete visible editor text ink, excluding transparent allocation padding.
    bool agentRenderedBounds(SDL_Rect &bounds) const;
	bool agentEditable() const;
	/// Actual blink-on caret ink, clipped to the editor, in screen coordinates.
	bool agentCaretBounds(SDL_Rect &bounds) const;
	/// Atomic replacement through the same native character/width constraints.
	bool agentReplaceText(const std::string &text);
	std::uint64_t agentInputRevision() const { return _agentInputRevision; }
#endif
	/// Sets the text edit's wordwrap setting.
	void setWordWrap(bool wrap);
	/// Sets the text edit's color invert setting.
	void setInvert(bool invert);
	/// Sets the text edit's high contrast color setting.
	void setHighContrast(bool contrast) override;
	/// Sets the text edit's horizontal alignment.
	void setAlign(TextHAlign align);
	/// Sets the text edit's vertical alignment.
	void setVerticalAlign(TextVAlign valign);
	/// Sets the text edit constraint.
	void setConstraint(TextEditConstraint constraint);
	/// Sets the text edit's color.
	void setColor(Uint8 color) override;
	/// Gets the text edit's color.
	Uint8 getColor() const;
	/// Sets the text edit's secondary color.
	void setSecondaryColor(Uint8 color) override;
	/// Gets the text edit's secondary color.
	Uint8 getSecondaryColor() const;
	/// Sets the text edit's palette.
	void setPalette(SDL_Color *colors, int firstcolor = 0, int ncolors = 256) override;
	/// Handles the timers.
	void think() override;
	/// Plays the blinking animation.
	void blink();
	/// Draws the text edit.
	void draw() override;
	/// Special handling for mouse presses.
	void mousePress(Action *action, State *state) override;
	/// Special handling for keyboard presses.
	void keyboardPress(Action *action, State *state) override;
	/// Hooks an action handler to when the text changes.
	void onChange(ActionHandler handler);
};

}
