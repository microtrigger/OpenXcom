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
#include "SlideshowState.h"
#include "CutsceneState.h"
#include "../Engine/FileMap.h"
#include "../Engine/Game.h"
#include "../Engine/InteractiveSurface.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Screen.h"
#include "../Engine/Timer.h"
#include "../Interface/Text.h"
#include "../Mod/Mod.h"
#include "../Engine/Options.h"
#include "../Interface/Cursor.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentUi.h"
#include "../Observer/AgentProtocol.h"
#endif

namespace OpenXcom
{

SlideshowState::SlideshowState(const SlideshowHeader &slideshowHeader,
			       const std::vector<SlideshowSlide> *slideshowSlides)
		: _slideshowHeader(slideshowHeader), _slideshowSlides(slideshowSlides), _curScreen(-1)
{
	_wasLetterboxed = CutsceneState::initDisplay();

	// pre-render and queue up all the frames
	for (std::vector<SlideshowSlide>::const_iterator it = _slideshowSlides->begin(); it != _slideshowSlides->end(); ++it)
	{
		InteractiveSurface *slide =
			new InteractiveSurface(Screen::ORIGINAL_WIDTH, Screen::ORIGINAL_HEIGHT, 0, 0);
		slide->loadImage(FileMap::getFilePath(it->imagePath));
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        slide->agentTrackPresentation();
#endif
		slide->onMouseClick((ActionHandler)&SlideshowState::screenClick);
		slide->onKeyboardPress((ActionHandler)&SlideshowState::screenClick, Options::keyOk);
		slide->onKeyboardPress((ActionHandler)&SlideshowState::screenSkip, Options::keyCancel);
		slide->setVisible(false);
		_slides.push_back(slide);
		setPalette(slide->getPalette());
		add(slide);

		// initialize with default rect; may get overridden by
		// category/id definition
		Text *caption = new Text(it->w, it->h, it->x, it->y);

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        caption->agentTrackPresentation();
#endif
		caption->setColor(it->color);
		caption->setText(tr(it->caption));
		caption->setAlign(it->align);
		caption->setWordWrap(true);
		caption->setVisible(false);
		_captions.push_back(caption);
		add(caption);
	}

	centerAllSurfaces();

	int transitionSeconds = _slideshowHeader.transitionSeconds;
	if (_slideshowSlides->front().transitionSeconds > 0)
		transitionSeconds = _slideshowSlides->front().transitionSeconds;
	_transitionTimer = new Timer(transitionSeconds * 1000);
	_transitionTimer->onTimer((StateHandler)&SlideshowState::screenTimer);

	_game->getMod()->playMusic(_slideshowHeader.musicId);
	_game->getCursor()->setVisible(false);
	screenClick(0);
}

SlideshowState::~SlideshowState()
{
	delete _transitionTimer;
}

/**
 * Shows the next screen on a timed basis.
 */
void SlideshowState::screenTimer()
{
	screenClick(0);
}

/**
 * Handle timers.
 */
void SlideshowState::think()
{
	_transitionTimer->think(this, 0);
}

/**
 * Shows the next screen in the slideshow; pops the state when there are no more slides
 */
void SlideshowState::screenClick(Action *action)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    ++_agentSlideRevision;
    _agentDestinationPaintFull = false;
#endif
	if (_curScreen >= 0)
	{
		_slides[_curScreen]->setVisible(false);
		_captions[_curScreen]->setVisible(false);
	}

	++_curScreen;

	// next screen
	if (_curScreen < (int)_slideshowSlides->size())
	{
		int transitionSeconds = _slideshowHeader.transitionSeconds;
		if (_slideshowSlides->at(_curScreen).transitionSeconds > 0)
			transitionSeconds = _slideshowSlides->at(_curScreen).transitionSeconds;
		_transitionTimer->setInterval(transitionSeconds * 1000);
		_transitionTimer->start();
		setPalette(_slides[_curScreen]->getPalette());
		_slides[_curScreen]->setVisible(true);
		_captions[_curScreen]->setVisible(true);
		init();
	}
	else
	{
		screenSkip(action);
	}
}

/**
 * Skips the slideshow
 */
void SlideshowState::screenSkip(Action *)
{
	// slideshow is over.  restore the screen scale and pop the state
	_game->getCursor()->setVisible(true);
	CutsceneState::resetDisplay(_wasLetterboxed);
	_game->popState();
}


#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
bool SlideshowState::agentDestinationFull() const
{
    const SDL_Surface *destination = _game->getScreen()->getSurface()->getSurface();
    return destination && destination->clip_rect.x == 0 && destination->clip_rect.y == 0 &&
        destination->clip_rect.w == destination->w && destination->clip_rect.h == destination->h;
}

std::vector<std::uint64_t> SlideshowState::agentPaintRevisions() const
{
    std::vector<std::uint64_t> revisions = {_agentSlideRevision};
    // Bind only the active presentation. No queued slide data becomes public.
    for (const Surface *surface : _surfaces)
        revisions.push_back(surface->agentPresentationRevision());
    if (_curScreen >= 0 && static_cast<std::size_t>(_curScreen) < _captions.size())
        revisions.push_back(_captions[_curScreen]->agentTextRevision());
    return revisions;
}

void SlideshowState::blit()
{
    _agentDestinationPaintFull = false;
    const bool before = agentDestinationFull();
    State::blit();
    _agentStoredPaintRevisions = agentPaintRevisions();
    _agentDestinationPaintFull = before && agentDestinationFull() && Observer::agentPaintHealthy();
}

bool SlideshowState::agentUiState(Observer::AgentUiState &out) const
{
    using namespace Observer;
    if (!_game->isState(const_cast<SlideshowState *>(this)) || _modal || !agentPaintHealthy() ||
        !_agentDestinationPaintFull || !agentDestinationFull() ||
        _agentStoredPaintRevisions != agentPaintRevisions() || _curScreen < 0 ||
        static_cast<std::size_t>(_curScreen) >= _slides.size() ||
        static_cast<std::size_t>(_curScreen) >= _captions.size()) return false;
    const InteractiveSurface *slide = _slides[_curScreen];
    const Text *caption = _captions[_curScreen];
    const Surface *destination = _game->getScreen()->getSurface();
    if (!agentWidgetVisible(slide) || !slide->agentPresentationPaintedTo(destination)) return false;
    out = AgentUiState();
    out.screen = "slideshow";
    out.actor = slide; // Private current native input target, never serialized.
    out.uiRevision = _agentSlideRevision;
    out.regions.push_back({slide->getX(), slide->getY(), slide->getWidth(), slide->getHeight()});
    const auto painted = agentFlattenPaintOrder(_surfaces);
    bool found = false;
    for (const Surface *later : painted)
    {
        if (later == slide) { found = true; continue; }
        // The current caption is deliberate content on the clickable slide, not
        // another interactive control. Any other later visible overlap blocks.
        if (!found || !later || !later->getVisible() || later == caption) continue;
        const auto &rect = out.regions.front();
        if (rect.x < later->getX() + later->getWidth() && rect.x + rect.width > later->getX() &&
            rect.y < later->getY() + later->getHeight() && rect.y + rect.height > later->getY()) return false;
    }
    if (!found || !agentRegionsDisjoint(out) || !_game->agentOverlayClear(out)) return false;
    out.widgets.emplace_back(slide, slide->agentPresentationRevision());
    out.widgets.emplace_back(caption, caption->agentPresentationRevision());
    out.widgets.emplace_back(caption, caption->agentTextRevision());
    std::string text = "null";
    if (agentWidgetVisible(caption) && caption->agentPresentationPaintedTo(destination) &&
        caption->getText().size() <= 1024 && agentRegionsUncovered(
            {{caption->getX(), caption->getY(), caption->getWidth(), caption->getHeight()}}, caption, painted))
    {
        AgentUiState captionRegion;
        captionRegion.regions.push_back({caption->getX(), caption->getY(), caption->getWidth(), caption->getHeight()});
        if (_game->agentNonCursorOverlayClear(captionRegion)) text = agentJsonString(caption->getText());
    }
    out.visible = "{\"caption\":" + text + ",\"buttons\":[{\"id\":\"advance\",\"label\":\"Advance current slide\"}]}";
    out.actions.emplace_back("advance_slideshow", "Advance current slide", "visible_control_semantics");
    return true;
}

bool SlideshowState::agentDispatch(const Observer::AgentRequest &request)
{
    Observer::AgentUiState current;
    if (!agentUiState(current) || !current.offers(request.action)) return false;
    if (request.action == "advance_slideshow") { screenClick(nullptr); return true; }
    return false;
}
#endif

}
