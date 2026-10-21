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
#include "ImageButton.h"
#include "../Engine/Action.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#endif

namespace OpenXcom
{

/**
 * Sets up an image button with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
ImageButton::ImageButton(int width, int height, int x, int y) : InteractiveSurface(width, height, x, y), _color(0), _group(0), _inverted(false)
{
}

/**
 *
 */
ImageButton::~ImageButton()
{

}

/**
 * Changes the color for the image button.
 * @param color Color value.
 */
void ImageButton::setColor(Uint8 color)
{
	_color = color;
}

/**
 * Returns the color for the image button.
 * @return Color value.
 */
Uint8 ImageButton::getColor() const
{
	return _color;
}

/**
 * Changes the button group this image button belongs to.
 * @param group Pointer to the pressed button pointer in the group.
 * Null makes it a regular button.
 */
void ImageButton::setGroup(ImageButton **group)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	_group = group;
	if (_group != 0 && *_group == this)
		invert(_color + 3);
}

/**
 * Sets the button as the pressed button if it's part of a group,
 * and inverts the colors when pressed.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void ImageButton::mousePress(Action *action, State *state)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
    if (_group && *_group) (*_group)->agentPresentationMutation();
#endif
	if (_group != 0)
	{
		if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
		{
			(*_group)->invert((*_group)->getColor() + 3);
			*_group = this;
			invert(_color + 3);
		}
	}
	else if (!_inverted && isButtonPressed() && isButtonHandled(action->getDetails()->button.button))
	{
		_inverted = true;
		invert(_color + 3);
	}
	InteractiveSurface::mousePress(action, state);
}

/*
 * Sets the button as the released button if it's part of a group.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void ImageButton::mouseRelease(Action *action, State *state)
{
	if (_inverted && isButtonHandled(action->getDetails()->button.button))
	{
		_inverted = false;
		invert(_color + 3);
	}
	InteractiveSurface::mouseRelease(action, state);
}

/**
 * Invert a button explicitly either ON or OFF and keep track of the state using our internal variables.
 * @param press Set this button as pressed.
 */
void ImageButton::toggle(bool press)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    agentPresentationMutation();
#endif
	if (_inverted != press)
	{
		_inverted = !_inverted;
		invert(_color + 3);
	}
}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
void ImageButton::blit(Surface *destination)
{
    if (!_agentRadioTracked) { Surface::blit(destination); return; }
    _agentRadioPaint=false;
    Surface::blit(destination);
    if (!agentPresentationPaintedTo(destination) || !_group || !*_group) return;
    _agentPaintGroup=_group;_agentPaintSelected=*_group;_agentRadioPaint=true;
}
bool ImageButton::agentRadioControl(const Surface *destination,bool &selected) const
{
    selected=false;
    if (!_agentRadioTracked || !_agentRadioPaint || !_group || _group!=_agentPaintGroup || *_group!=_agentPaintSelected ||
        !agentPresentationPaintedTo(destination) || !destination || getX()<0 || getY()<0 ||
        getX()+getWidth()>destination->getWidth() || getY()+getHeight()>destination->getHeight()) return false;
    // Native radio selection retained from the painted frame, not inferred from icon pixels.
    selected=*_group==this;return true;
}
bool ImageButton::agentActivateRadio(State *state)
{
    if (!_agentRadioTracked || !_group || !*_group || !state || !agentContextVisible()) return false;
    SDL_Event event={};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=SDL_BUTTON_LEFT;
    Action action(&event,1,1,0,0);
    mousePress(&action,state); // Same radio inversion + group update + registered callback as ordinary UI.
    return true;
}
#endif

}
