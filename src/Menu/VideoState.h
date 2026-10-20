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
#ifdef OPENXCOM_RESPONSIVE_LOADING
#include <memory>
#include <SDL.h>
#endif

namespace OpenXcom
{

/**
 * Shows video cinematics.
 */
class VideoState : public State
{
#ifdef OPENXCOM_RESPONSIVE_LOADING
private:
    struct ResponsiveVideo;
    std::unique_ptr<ResponsiveVideo> _responsiveVideo;
    bool _responsiveFinished, _responsiveInitFailed;
    void responsivePrepareVideo();
    void responsiveReserveDetach() noexcept;
    void responsiveFinish(bool present) noexcept;
    void responsiveStartFade(Uint32);
#ifdef OPENXCOM_RESPONSIVE_TESTING
    friend class LoadingVideoIntegrationTestAccess;
#endif
public:
    void responsiveVideoTick(Uint32 now, bool present) noexcept;
    void responsiveVideoEvent(const SDL_Event &) noexcept;
    void responsiveAudioComplete(bool attach, bool success, bool closeRequested) noexcept;
    bool responsiveVideoActive() const noexcept;
    void responsiveRequestClose() noexcept;
#endif
private:
	const std::vector<std::string> *_videos, *_tracks;
	bool _useUfoAudioSequence;
public:
	/// Creates the Intro state.
	VideoState(const std::vector<std::string> *videos, const std::vector<std::string> *tracks, bool useUfoAudioSequence);
	/// Cleans up the Intro state.
	~VideoState();
	/// Plays the video
	void init();
};

#if defined(OPENXCOM_RESPONSIVE_LOADING) && defined(OPENXCOM_RESPONSIVE_TESTING)
// Read-only access to actual production state for the nonshipping fixture.
// No phase mutation, receipt minting or alternate dispatcher is exposed.
class LoadingVideoIntegrationTestAccess
{
public:
    static const char *phase(const VideoState &) noexcept;
    static std::weak_ptr<void> lease(const VideoState &) noexcept;
    static unsigned route(const VideoState &) noexcept;
    static unsigned frameCount(const VideoState &) noexcept;
    static unsigned headerSpeed(const VideoState &) noexcept;
    static bool closeRequested(const VideoState &) noexcept;
    static bool skipped(const VideoState &) noexcept;
};
#endif

}
