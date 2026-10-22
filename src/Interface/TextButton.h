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
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include <cstdint>
#endif

namespace OpenXcom
{

class Text;
class Font;
class Language;
class Sound;
class ComboBox;

/**
 * Coloured button with a text label.
 * Drawn to look like a 3D-shaped box with text on top,
 * responds to mouse clicks. Can be attached to a group of
 * buttons to turn it into a radio button (only one button
 * pushed at a time).
 */
class TextButton : public InteractiveSurface
{
private:
	Uint8 _color;
	Text *_text;
	TextButton **_group;
	bool _contrast, _geoscapeButton;
	ComboBox *_comboBox;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	bool _agentDrawnPressed = false;
    bool _agentNamedControl = false, _agentNamedPaint = false, _agentNamedBlitValid = false;
    SDL_Rect _agentNamedInk = {};
    std::string _agentNamedLabel;
    std::vector<Uint8> _agentNamedPixels;
    SDL_Color _agentNamedPalette[256] = {};
    std::uint64_t _agentNamedLabelRevision = 0;
    void agentCaptureNamedControl();
	std::uint64_t _agentSelectionRevision = 0;
#endif
	// for use by RuleInterface
	void setSecondaryColor(Uint8 color) { setTextColor(color); }
protected:
	bool isButtonHandled(Uint8 button = 0);
public:
	static Sound *soundPress;
	/// Creates a new text button with the specified size and position.
	TextButton(int width, int height, int x = 0, int y = 0);
	/// Cleans up the text button.
	~TextButton();
	/// Sets the text button's color.
	void setColor(Uint8 color);
	/// Gets the text button's color.
	Uint8 getColor() const;
	/// Sets the text button's text color.
	void setTextColor(Uint8 color);
	/// Sets the text size to big.
	void setBig();
	/// Sets the text size to small.
	void setSmall();
	/// Gets the text button's current font.
	Font *getFont() const;
	/// Initializes the text edit's resources.
	void initText(Font *big, Font *small, Language *lang);
	/// Sets the text button's high contrast color setting.
	void setHighContrast(bool contrast);
	/// Sets the text button's text.
	void setText(const std::string &text);
	/// Gets the text button's text.
	std::string getText() const;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
	/// Gets only a fully rendered label, excluding an optional covered right edge.
	std::string agentLabel(int rightInset = 0) const;
    /// Current caption only if its layout fits; no glyph-raster prerequisite.
    std::string agentLayoutLabel() const;
    // Opt-in named control only. A complete drawn label and one fixed ordinary
    // interior point are qualified separately from the decorative outer border.
    void blit(Surface *surface) override;
    void agentEnableNamedControl() { _agentNamedControl=true; agentTrackPresentation(); }
    bool agentNamedControl(const Surface *destination, std::string &label, SDL_Rect &ink, SDL_Rect &point) const;
	/// Reads the pressed appearance captured by the completed ordinary draw.
	bool agentPressed(bool &pressed) const;
	std::uint64_t agentLabelRevision() const;
	std::uint64_t agentSelectionRevision() const { return _agentSelectionRevision; }
#endif
	/// Sets the text button's group.
	void setGroup(TextButton **group);
	/// Selects this grouped radio button through the ordinary input operation.
	bool selectRadio();
	/// Sets the text button's palette.
	void setPalette(SDL_Color *colors, int firstcolor = 0, int ncolors = 256);
	/// Draws the text button.
	void draw();
	/// Special handling for mouse presses.
	void mousePress(Action *action, State *state);
	/// Special handling for mouse releases.
	void mouseRelease(Action *action, State *state);
	/// Attaches this button to a combobox.
	void setComboBox(ComboBox *comboBox);
	void setWidth(int width);
	void setHeight(int height);
	void setGeoscapeButton(bool geo);
};

}
