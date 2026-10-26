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
#include "../Engine/State.h"
#include "../Mod/RuleVideo.h"

namespace OpenXcom
{

class InteractiveSurface;
class Text;
class Timer;

/**
 * Shows slideshow sequences.
 */
class SlideshowState : public State
{
private:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentDestinationPaintFull = false;
    std::uint64_t _agentSlideRevision = 0;
    std::vector<std::uint64_t> _agentStoredPaintRevisions;
    bool agentDestinationFull() const;
    std::vector<std::uint64_t> agentPaintRevisions() const;
#endif
	const SlideshowHeader &_slideshowHeader;
	const std::vector<SlideshowSlide> *_slideshowSlides;
	bool _wasLetterboxed;
	std::vector<InteractiveSurface *> _slides;
	std::vector<Text *>_captions;
	int _curScreen;
	Timer *_transitionTimer;
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit() override;
    bool agentUiState(Observer::AgentUiState &out) const override;
    bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
	/// Creates the Slideshow state.
	SlideshowState(const SlideshowHeader &slideshowHeader, const std::vector<SlideshowSlide> *slideshowRule);
	/// Cleans up the Slideshow state.
	~SlideshowState();
	/// Handle timers.
	void think();
	/// Handler for waiting the screen.
	void screenTimer();
	/// Handler for clicking the screen.
	void screenClick(Action *action);
	/// Handler for skipping the screen.
	void screenSkip(Action *action);
};

}
