// Fork modification 2026-10-11: default-off supervised stance pilot; GPL-3.0-or-later.
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
// Fork modification 2026-10-10: opt-in read-only public observation hooks;
// see LICENSE-NOTICES.md. Existing upstream copyright and license retained.
// Fork modification 2026-10-10: default-off render-only test gate.
// SPDX-License-Identifier: GPL-3.0-or-later; upstream notices retained.
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
#include "../../tests/battle-observer/RenderPairGate.h"
#endif
#include "Screen.h"
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentProjectileOutput.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
#include "../Observer/SnapshotImageProof.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
#include "../Observer/CursorFileObserver.h"
#include "../Observer/PresentationObserver.h"
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
#include "../Observer/StanceController.h"
#endif
#include "../Observer/PresentedCursorSurface.h"
#include <cstdlib>
#endif
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <climits>
#include <cstdio>
#include "../lodepng.h"
#include "Exception.h"
#include "Surface.h"
#include "Logger.h"
#include "Action.h"
#include "Options.h"
#include "CrossPlatform.h"
#include "FileMap.h"
#include "Zoom.h"
#include "Timer.h"
#include <SDL.h>

namespace OpenXcom
{

#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
Screen::LoadingPresentation::LoadingPresentation() noexcept : _source(0), _destination(0),
    _config{false, false, false, false, false}, _top(0), _bottom(0), _left(0), _right(0),
    _first(0), _count(0), _zoom(false), _pending(false), _captured(false), _strict(true), _sourceColors(0)
{
    std::fill(_palette, _palette + 256, SDL_Color());
    std::fill(_sourcePalette, _sourcePalette + 256, SDL_Color());
}

Zoom::RenderStatus Screen::captureLoadingPresentation(SDL_Surface *source, SDL_Surface *destination,
    int baseWidth, int baseHeight, int top, int bottom, int left, int right,
    const SDL_Color *palette, int first, int count, bool pending, OpenGL *gl,
    LoadingPresentation &out) noexcept
{
    return capturePresentation(source, destination, baseWidth, baseHeight, top, bottom, left, right,
        palette, first, count, pending, gl, out, true);
}

Zoom::RenderStatus Screen::capturePresentation(SDL_Surface *source, SDL_Surface *destination,
    int baseWidth, int baseHeight, int top, int bottom, int left, int right,
    const SDL_Color *palette, int first, int count, bool pending, OpenGL *gl,
    LoadingPresentation &out, bool strict) noexcept
{
    out = LoadingPresentation();
    out._strict = strict;
    if (!Zoom::readableSurface(source) || !destination || !destination->format ||
        baseWidth <= 0 || baseHeight <= 0 || source->w != baseWidth || source->h != baseHeight)
        return Zoom::RenderStatus::InvalidSurface;
    if (first < 0 || first > 256 || count < 0 || count > 256 - first || (count && !palette))
        return Zoom::RenderStatus::PaletteFailure;
    // Keep independent palette/present phases available to the ordinary
    // wrapper even when the copy phase is rejected (its legacy error policy).
    out._source = source; out._destination = destination;
    out._top = top; out._bottom = bottom; out._left = left; out._right = right;
    out._first = first; out._count = count; out._pending = pending;
    for (int i = first; i < first + count; ++i) out._palette[i] = palette[i];
    out._config = Zoom::captureRenderConfig();
    out._zoom = destination->w != baseWidth || destination->h != baseHeight || out._config.openGL;
    if (out._config.openGL)
    {
        const Zoom::RenderStatus captured = Zoom::captureGl(gl, out._gl);
        if (captured != Zoom::RenderStatus::Success) return captured;
    }
    const Zoom::RenderStatus admission = Zoom::validateCopy(source, destination, out._zoom,
        top, bottom, left, right, out._config, out._gl, strict);
    if (admission != Zoom::RenderStatus::Success) return admission;
    if (pending && count && destination->format->BitsPerPixel == 8 &&
        (!destination->format->palette || destination->format->palette->ncolors < first + count))
        return Zoom::RenderStatus::PaletteFailure;
    if (source->format->palette)
    {
        out._sourceColors = source->format->palette->ncolors;
        for (int i = 0; i < out._sourceColors; ++i) out._sourcePalette[i] = source->format->palette->colors[i];
    }
    out._captured = true;
    return Zoom::RenderStatus::Success;
}

Zoom::RenderStatus Screen::captureLoadingPresentation(LoadingPresentation &out) const noexcept
{
    // Capture is an owner-only, pre-worker operation. No reset or callback.
    return captureLoadingPresentation(_surface ? _surface->getSurface() : 0, _screen,
        _baseWidth, _baseHeight, _topBlackBand, _bottomBlackBand, _leftBlackBand, _rightBlackBand,
        deferredPalette, _firstColor, _numColors, _pushPalette, const_cast<OpenGL *>(&glOutput), out);
}

Zoom::RenderStatus Screen::copyPhase(const LoadingPresentation &frame,
    Zoom::Workspace &workspace, int *snapshotCopyResult) noexcept
{
    if (snapshotCopyResult) *snapshotCopyResult = -2;
    if (!workspace.onOwner()) return Zoom::RenderStatus::WrongOwner;
    if (!frame._captured || !Zoom::readableSurface(frame._source)) return Zoom::RenderStatus::InvalidSurface;
    const SDL_Palette *palette = frame._source->format->palette;
    if ((palette ? palette->ncolors : 0) != frame._sourceColors) return Zoom::RenderStatus::PaletteFailure;
    for (int i = 0; i < frame._sourceColors; ++i)
    {
        const SDL_Color &current = palette->colors[i], &captured = frame._sourcePalette[i];
        if (current.r != captured.r || current.g != captured.g || current.b != captured.b || current.unused != captured.unused)
            return Zoom::RenderStatus::PaletteFailure;
    }
    return Zoom::copy(frame._source, frame._destination, frame._zoom, frame._top, frame._bottom,
        frame._left, frame._right, frame._config, frame._gl, workspace, snapshotCopyResult, frame._strict);
}

Zoom::RenderStatus Screen::palettePhase(const LoadingPresentation &frame) noexcept
{
    if (!frame._destination || !frame._destination->format)
        return Zoom::RenderStatus::InvalidSurface;
    if (!frame._pending || !frame._count || frame._destination->format->BitsPerPixel != 8)
        return Zoom::RenderStatus::Success;
    if ((frame._strict && !Zoom::readableSurface(frame._destination)) || !frame._destination->format->palette ||
        frame._first < 0 || frame._count < 0 || frame._first > 256 - frame._count ||
        frame._destination->format->palette->ncolors < frame._first + frame._count)
        return Zoom::RenderStatus::PaletteFailure;
    // SDL's legacy signature is mutable; it does not modify the color array.
    try
    {
        return SDL_SetColors(frame._destination, const_cast<SDL_Color *>(frame._palette + frame._first),
            frame._first, frame._count) != 0 ? Zoom::RenderStatus::Success : Zoom::RenderStatus::PaletteFailure;
    }
    catch (...) { return Zoom::RenderStatus::ExceptionFailure; }
}

Zoom::RenderStatus Screen::presentPhase(const LoadingPresentation &frame) noexcept
{
    if (!frame._destination || !frame._destination->format || frame._destination->w <= 0 ||
        frame._destination->h <= 0 || frame._destination->w > 8192 || frame._destination->h > 8192)
        return Zoom::RenderStatus::InvalidSurface;
    // GL refresh and swap happened in the same legacy copy phase. SDL_Flip is
    // still dispatched here, as in ordinary Screen::flip; it is not GPU proof.
    try
    {
        return SDL_Flip(frame._destination) == -1 ? Zoom::RenderStatus::FlipFailure : Zoom::RenderStatus::Success;
    }
    catch (...) { return Zoom::RenderStatus::ExceptionFailure; }
}

Zoom::RenderStatus Screen::presentLoading(const LoadingPresentation &frame, Zoom::Workspace &workspace) noexcept
{
    Logger::ScopedSuppression suppress;
    try
    {
        Zoom::RenderStatus result = copyPhase(frame, workspace);
        if (result != Zoom::RenderStatus::Success) return result;
        result = palettePhase(frame);
        if (result != Zoom::RenderStatus::Success) return result;
        return presentPhase(frame);
    }
    catch (...) { return Zoom::RenderStatus::ExceptionFailure; }
}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
namespace {
bool snapshotReadableSurface(const SDL_Surface *s) noexcept
{
    const Uint32 rejected = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_DOUBLEBUF | SDL_OPENGL | SDL_RLEACCEL;
    return s && s->format && !(s->flags & rejected) && !SDL_MUSTLOCK(s) &&
        s->format->BitsPerPixel == 8 && s->format->BytesPerPixel == 1 &&
        s->format->palette && s->format->palette->ncolors == 256 && s->format->palette->colors &&
        s->pixels && s->w > 0 && s->w <= 1920 && s->h > 0 && s->h <= 1200 && s->pitch >= s->w;
}
void snapshotPalette(const SDL_Surface *s, std::array<unsigned char, 768> &out) noexcept
{
    for (std::size_t i = 0; i < 256; ++i)
    {
        const SDL_Color &c = s->format->palette->colors[i];
        out[i * 3] = c.r; out[i * 3 + 1] = c.g; out[i * 3 + 2] = c.b;
    }
}
Observer::SnapshotIndexedView snapshotView(const SDL_Surface *s, const unsigned char *palette) noexcept
{
    Observer::SnapshotIndexedView view;
    view.pixels = static_cast<const unsigned char *>(s->pixels); view.palette = palette; view.paletteBytes = 768;
    view.width = s->w; view.height = s->h; view.pitch = s->pitch;
    // SDL owns the allocation. These bounded, validated Uint16-pitch dimensions
    // cannot overflow size_t; pure helper views additionally verify their span.
    view.bytes = (view.height - 1) * view.pitch + view.width; view.readable = true;
    return view;
}
}
Observer::SnapshotDisplayFacts Screen::snapshotFacts(SDL_Surface *physical, bool allowPendingPalette) const noexcept
{
    Observer::SnapshotDisplayFacts f;
    f.screen = _snapshotScreenLife.value(); f.backing = _snapshotBacking.value();
    f.configuration = _snapshotConfiguration.value(); f.appliedPalette = _snapshotAppliedPalette.value();
    f.physicalAddress = reinterpret_cast<std::uintptr_t>(physical);
    f.baseWidth = _baseWidth; f.baseHeight = _baseHeight;
    f.top = _topBlackBand; f.bottom = _bottomBlackBand; f.left = _leftBlackBand; f.right = _rightBlackBand;
    if (!_surface || !physical || !physical->format) return f;
    f.logicalLife = _surface->snapshotSurfaceLife(); f.logicalBacking = _surface->snapshotSurfaceBacking();
    f.logicalRevision = _surface->snapshotSurfaceRevision();
    f.width = physical->w; f.height = physical->h; f.flags = physical->flags;
    const SDL_Surface *logical = _surface->getSurface();
    f.physicalPixels = reinterpret_cast<std::uintptr_t>(physical->pixels);
    f.physicalFormat = reinterpret_cast<std::uintptr_t>(physical->format);
    f.physicalPalette = reinterpret_cast<std::uintptr_t>(physical->format->palette);
    f.physicalPitch = physical->pitch;
    if (logical && logical->format)
    {
        f.logicalAddress = reinterpret_cast<std::uintptr_t>(logical);
        f.logicalPixels = reinterpret_cast<std::uintptr_t>(logical->pixels);
        f.logicalFormat = reinterpret_cast<std::uintptr_t>(logical->format);
        f.logicalPalette = reinterpret_cast<std::uintptr_t>(logical->format->palette);
        f.logicalPitch = logical->pitch;
    }
    const Uint8 focus = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    const Uint32 unsupported = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_DOUBLEBUF | SDL_OPENGL;
    f.healthy = _snapshotPaintHealth.healthy() && _surface->snapshotPaintHealthy();
    f.logicalTransfersComplete = _surface->snapshotPixelTransfersComplete();
    f.supported = logical && logical->format && logical->format->BitsPerPixel == 8 &&
        logical->format->BytesPerPixel == 1 && logical->format->palette && logical->format->palette->ncolors == 256 && logical->format->palette->colors &&
        !(logical->flags & unsupported) &&
        logical->pixels && logical->w == 320 && logical->h == 200 && logical->pitch >= 320 &&
        physical->format->BitsPerPixel == 8 && physical->format->BytesPerPixel == 1 &&
        physical->format->palette && physical->format->palette->ncolors == 256 && physical->format->palette->colors && physical->pixels &&
        !(physical->flags & unsupported) && physical->pitch >= physical->w &&
        f.width >= 320 && f.width <= 1920 && f.width % 320 == 0 && f.height == 200 * (f.width / 320) &&
        _baseWidth == 320 && _baseHeight == 200 && !f.top && !f.bottom && !f.left && !f.right &&
        (_snapshotFullPalette || (allowPendingPalette && _pushPalette && _firstColor == 0 && _numColors == 256)) &&
        !useOpenGL() && !use32bitScaler() && !Options::useScaleFilter &&
        !Options::debug && (SDL_GetAppState() & focus) == focus;
    return f;
}
void Screen::snapshotComposedProof() noexcept
{
    if (!_snapshotPresentation || !_snapshotPresentation->wantsComposedProof()) return;
    const Observer::SnapshotDisplayFacts facts = snapshotFacts(_screen, true);
    const SDL_Surface *logical = _surface ? _surface->getSurface() : 0;
    std::array<unsigned char, 768> palette;
    Observer::SnapshotIndexedView view;
    if (facts.supported && snapshotReadableSurface(logical) && snapshotReadableSurface(_screen))
    { snapshotPalette(logical, palette); view = snapshotView(logical, palette.data()); }
    _snapshotPresentation->composedProof(facts, view);
}
void Screen::snapshotPresentedProof(SDL_Surface *physical) noexcept
{
    if (!_snapshotPresentation || !_snapshotPresentation->wantsPresentedProof()) return;
    const Observer::SnapshotDisplayFacts facts = snapshotFacts(physical);
    const SDL_Surface *logical = _surface ? _surface->getSurface() : 0;
    std::array<unsigned char, 768> logicalPalette, physicalPalette;
    Observer::SnapshotIndexedView view;
    const unsigned char *logicalColors = 0;
    if (facts.supported && snapshotReadableSurface(logical) && snapshotReadableSurface(physical))
    {
        snapshotPalette(logical, logicalPalette); logicalColors = logicalPalette.data();
        snapshotPalette(physical, physicalPalette); view = snapshotView(physical, physicalPalette.data());
    }
    _snapshotPresentation->presentedProof(facts, view, logicalColors, logicalColors ? 768 : 0);
}
void Screen::snapshotFinishFrame(bool current) noexcept
{
    if (!_snapshotPresentation) return;
    const Observer::SnapshotDisplayFacts facts = snapshotFacts(_screen);
    std::array<unsigned char, 768> palette;
    const unsigned char *colors = 0;
    bool readable = false;
    if (_snapshotPresentation->wantsFinishedProof())
    {
        const SDL_Surface *logical = _surface ? _surface->getSurface() : 0;
        readable = facts.supported && snapshotReadableSurface(logical) && snapshotReadableSurface(_screen);
        if (readable) { snapshotPalette(logical, palette); colors = palette.data(); }
    }
    _snapshotPresentation->finish(current, facts, colors, colors ? 768 : 0, readable);
}
#endif

const int Screen::ORIGINAL_WIDTH = 320;
const int Screen::ORIGINAL_HEIGHT = 200;

static const int VIDEO_WINDOW_POS_LEN = 40;
static char VIDEO_WINDOW_POS[VIDEO_WINDOW_POS_LEN];

static const char* SDL_VIDEO_CENTERED_UNSET = "SDL_VIDEO_CENTERED=";
static const char* SDL_VIDEO_CENTERED_CENTER = "SDL_VIDEO_CENTERED=center";
static const char* SDL_VIDEO_WINDOW_POS_UNSET = "SDL_VIDEO_WINDOW_POS=";

/**
 * Sets up all the internal display flags depending on
 * the current video settings.
 */
void Screen::makeVideoFlags()
{
	_flags = SDL_HWSURFACE|SDL_DOUBLEBUF|SDL_HWPALETTE;
	if (Options::asyncBlit)
	{
		_flags |= SDL_ASYNCBLIT;
	}
	if (useOpenGL())
	{
		_flags = SDL_OPENGL;
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	}
	if (Options::allowResize)
	{
		_flags |= SDL_RESIZABLE;
	}

	// Handle window positioning
	if (!Options::fullscreen && Options::rootWindowedMode)
	{
		snprintf(VIDEO_WINDOW_POS, VIDEO_WINDOW_POS_LEN, "SDL_VIDEO_WINDOW_POS=%d,%d", Options::windowedModePositionX, Options::windowedModePositionY);
		SDL_putenv(VIDEO_WINDOW_POS);
		SDL_putenv((char *)SDL_VIDEO_CENTERED_UNSET);
	}
	else if (Options::borderless)
	{
		SDL_putenv((char *)SDL_VIDEO_WINDOW_POS_UNSET);
		SDL_putenv((char *)SDL_VIDEO_CENTERED_CENTER);
	}
	else
	{
		SDL_putenv((char *)SDL_VIDEO_WINDOW_POS_UNSET);
		SDL_putenv((char *)SDL_VIDEO_CENTERED_UNSET);
	}

	// Handle display mode
	if (Options::fullscreen)
	{
		_flags |= SDL_FULLSCREEN;
	}
	if (Options::borderless)
	{
		_flags |= SDL_NOFRAME;
	}

	_bpp = (use32bitScaler() || useOpenGL()) ? 32 : 8;
	_baseWidth = Options::baseXResolution;
	_baseHeight = Options::baseYResolution;
}


/**
 * Initializes a new display screen for the game to render contents to.
 * The screen is set up based on the current options.
 */
Screen::Screen() :
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
	_cursorObserver(0), _presentationObserver(0), _observerPaletteExact(false),
#endif
	_baseWidth(ORIGINAL_WIDTH), _baseHeight(ORIGINAL_HEIGHT), _scaleX(1.0), _scaleY(1.0), _flags(0), _numColors(0), _firstColor(0), _pushPalette(false), _surface(0)
{
	resetDisplay();
	memset(deferredPalette, 0, 256*sizeof(SDL_Color));
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
	_cursorObserver = new Observer::CursorFileObserver(std::getenv("OPENXCOM_EXPERIMENTAL_CURSOR_DIR"));
	_presentationObserver = new Observer::PresentationObserver(std::getenv("OPENXCOM_PRESENTATION_FD"));
	::unsetenv("OPENXCOM_PRESENTATION_FD");
#endif
}

/**
 * Deletes the buffer from memory. The display screen itself
 * is automatically freed once SDL shuts down.
 */
Screen::~Screen()
{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    _snapshotPresentation = 0;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
	delete _cursorObserver;
	delete _presentationObserver;
#endif
	delete _surface;
}

/**
 * Returns the screen's internal buffer surface. Any
 * contents that need to be shown will be blitted to this.
 * @return Pointer to the buffer surface.
 */
Surface *Screen::getSurface()
{
	_pushPalette = true;
	return _surface;
}

/**
 * Handles screen key shortcuts.
 * @param action Pointer to an action.
 */
void Screen::handle(Action *action)
{
	if (Options::debug)
	{
		if (action->getDetails()->type == SDL_KEYDOWN && action->getDetails()->key.keysym.sym == SDLK_F8)
		{
			switch(Timer::gameSlowSpeed)
			{
				case 1: Timer::gameSlowSpeed = 5; break;
				case 5: Timer::gameSlowSpeed = 15; break;
				default: Timer::gameSlowSpeed = 1; break;
			}
		}
	}

	if (action->getDetails()->type == SDL_KEYDOWN && action->getDetails()->key.keysym.sym == SDLK_RETURN && (SDL_GetModState() & KMOD_ALT) != 0)
	{
		Options::fullscreen = !Options::fullscreen;
		resetDisplay();
	}
	else if (action->getDetails()->type == SDL_KEYDOWN && action->getDetails()->key.keysym.sym == Options::keyScreenshot)
	{
		std::ostringstream ss;
		int i = 0;
		do
		{
			ss.str("");
			ss << Options::getMasterUserFolder() << "screen" << std::setfill('0') << std::setw(3) << i << ".png";
			i++;
		}
		while (CrossPlatform::fileExists(ss.str()));
		screenshot(ss.str());
		return;
	}
}


/**
 * Renders the buffer's contents onto the screen, applying
 * any necessary filters or conversions in the process.
 * If the scaling factor is bigger than 1, the entire contents
 * of the buffer are resized by that factor (eg. 2 = doubled)
 * before being put on screen.
 */
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
void Screen::excludePresentedHud(int x, int y, int width, int height)
{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    if (x < 320 && x + width > 0 && y < 200 && y + height > 144) _stancePanelBlocked = true;
#endif
	for (const Observer::HudDigits &d : _presentedHud)
		if (x < d.x + d.width && x + width > d.x && y < d.y + d.height && y + height > d.y)
		{ _presentedHud = Observer::HudFrame(); return; }
}
bool Screen::wantsPresentedHud() const
{
	return _presentationObserver && _presentationObserver->pending() && _presentationObserver->hudEnabled();
}
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
bool Screen::stanceAllowed() const
{
    const Uint8 required = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    return (SDL_GetAppState() & required) == required && !useOpenGL() && !use32bitScaler() && !Options::debug &&
        _observerPaletteExact && _topBlackBand == 0 && _bottomBlackBand == 0 && _leftBlackBand == 0 && _rightBlackBand == 0 &&
        _baseWidth == 320 && _baseHeight == 200;
}
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
bool Screen::inventoryWaitingAllowed() const
{
    // Inventory construction can normally reset the display before I arrives;
    // State::init then defers its full palette until the first ordinary flip.
    // Waiting/arming may survive that pending palette, never an unsupported
    // profile. Capture still requires stanceAllowed() after successful flip.
    const Uint8 required = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
    return (SDL_GetAppState() & required) == required && !useOpenGL() && !use32bitScaler() && !Options::debug &&
        (_observerPaletteExact || (_firstColor == 0 && _numColors == 256)) &&
        _topBlackBand == 0 && _bottomBlackBand == 0 && _leftBlackBand == 0 && _rightBlackBand == 0 &&
        _baseWidth == 320 && _baseHeight == 200;
}
#endif
#endif
void Screen::beginObserverFrame()
{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    _stancePanelBlocked = false;
#endif
	_presentedHud = Observer::HudFrame();
	if (_presentationObserver) _presentationObserver->beginFrame();
}

void Screen::flip(int cursorX, int cursorY)
#else
void Screen::flip()
#endif
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentLastFlipOk = false;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    snapshotComposedProof();
    int snapshotCopyResult = -2;
#endif
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
    LoadingPresentation frame;
    const Zoom::RenderStatus captured = capturePresentation(_surface->getSurface(), _screen,
        _baseWidth, _baseHeight, _topBlackBand, _bottomBlackBand, _leftBlackBand, _rightBlackBand,
        deferredPalette, _firstColor, _numColors, _pushPalette, &glOutput, frame, false);
    const Zoom::RenderStatus copied = copyPhase(frame, _renderWorkspace,
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
        &snapshotCopyResult
#else
        0
#endif
    );
    (void)captured;
    (void)copied;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    const bool agentCopied = captured == Zoom::RenderStatus::Success && copied == Zoom::RenderStatus::Success;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    // Only the historical direct/unbanded route supplies copy proof. A strict
    // admission failure on such a route is a failure, never successful proof.
    if (copied != Zoom::RenderStatus::Success && !frame._config.openGL &&
        (!frame._zoom || (!frame._top && !frame._bottom && !frame._left && !frame._right)))
        snapshotCopyResult = -1;
#endif
#else
	if (getWidth() != _baseWidth || getHeight() != _baseHeight || useOpenGL())
	{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
		Zoom::flipWithZoom(_surface->getSurface(), _screen, _topBlackBand, _bottomBlackBand, _leftBlackBand, _rightBlackBand, &glOutput, &snapshotCopyResult);
#else
		Zoom::flipWithZoom(_surface->getSurface(), _screen, _topBlackBand, _bottomBlackBand, _leftBlackBand, _rightBlackBand, &glOutput);
#endif
	}
	else
	{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
		snapshotCopyResult = SDL_BlitSurface(_surface->getSurface(), 0, _screen, 0);
#else
		SDL_BlitSurface(_surface->getSurface(), 0, _screen, 0);
#endif
	}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    if (snapshotCopyResult != -2) _snapshotPaintHealth.observe(snapshotCopyResult == 0);
#endif

	// perform any requested palette update
	if (_pushPalette && _numColors && _screen->format->BitsPerPixel == 8)
	{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
        const std::uint64_t snapshotPaletteBefore = _snapshotAppliedPalette.value();
        _snapshotAppliedPalette.renew();
#endif
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
        const bool paletteExact = palettePhase(frame) == Zoom::RenderStatus::Success;
#else
        const bool paletteExact = SDL_SetColors(_screen, &(deferredPalette[_firstColor]), _firstColor, _numColors) != 0;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
        _snapshotPaintHealth.observe(paletteExact);
        if (!paletteExact) _snapshotFullPalette = false;
        else if (_firstColor == 0 && _numColors == 256) _snapshotFullPalette = true;
        if (_snapshotPresentation) _snapshotPresentation->proofPaletteApplied(
            snapshotPaletteBefore, _snapshotAppliedPalette.value(), paletteExact);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
		if (!paletteExact) _observerPaletteExact = false;
		else if (_firstColor == 0 && _numColors == 256) _observerPaletteExact = true;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        if (!paletteExact) _agentPaletteExact = false;
        else if (_firstColor == 0 && _numColors == 256) _agentPaletteExact = true;
#endif
		if (!paletteExact)
		{
			Log(LOG_DEBUG) << "Display palette doesn't match requested palette";
		}
		_numColors = 0;
		_pushPalette = false;
	}



#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
    if (presentPhase(frame) != Zoom::RenderStatus::Success)
#else
    if (SDL_Flip(_screen) == -1)
#endif
    {
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
        _snapshotPaintHealth.observe(false);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
        if (_stanceController) _stanceController->close();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
		if (_presentationObserver) _presentationObserver->close();
#endif
		throw Exception(SDL_GetError());
	}
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentLastFlipOk = agentCopied && (_screen->format->BitsPerPixel != 8 || _agentPaletteExact);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    if (_snapshotPresentation) _snapshotPresentation->flipped(snapshotFacts(_screen), snapshotCopyResult == 0);
    snapshotPresentedProof(_screen);
#endif
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
#ifdef OPENXCOM_EXPERIMENTAL_TIMING_CALIBRATION
	RenderPairGate::timingFlipped();
#endif
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    if (_stanceController) _stanceController->presented(_screen, _baseWidth, _baseHeight, stanceAllowed(), !_stancePanelBlocked);
#endif
    // Only final display bytes after all scaling, palette application and flip.
	// Software single-buffer pixels remain the just-presented image; swapping or
	// asynchronous backends are rejected. No claim about desktop occlusion/freshness.
	if ((_cursorObserver && _cursorObserver->enabled()) ||
		(_presentationObserver && _presentationObserver->pending()))
	{
		const Uint8 requiredFocus = SDL_APPACTIVE | SDL_APPINPUTFOCUS | SDL_APPMOUSEFOCUS;
		const bool focused = (SDL_GetAppState() & requiredFocus) == requiredFocus;
		const bool allowed = focused && !useOpenGL() && !use32bitScaler() && !Options::debug &&
			_observerPaletteExact && _topBlackBand == 0 && _bottomBlackBand == 0 &&
			_leftBlackBand == 0 && _rightBlackBand == 0;
		const Observer::PixelCrop crop = Observer::capturePresentedCursor(_screen, _baseWidth, _baseHeight,
			cursorX, cursorY, allowed);
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
		RenderPairGate::capture(_screen, _baseWidth, _baseHeight, allowed, _presentedHud, crop);
#endif
		if (_cursorObserver) _cursorObserver->observe(crop);
		if (_presentationObserver) _presentationObserver->presented(crop,
			wantsPresentedHud() ? Observer::provePresentedHud(_screen, _baseWidth, _baseHeight, _presentedHud, allowed) : "null");
		_presentedHud = Observer::HudFrame();
	}
#endif
}

/**
 * Clears all the contents out of the internal buffer.
 */
void Screen::clear()
{
	_surface->clear();
	if (_screen->flags & SDL_SWSURFACE) memset(_screen->pixels, 0, _screen->h*_screen->pitch);
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
	else _snapshotPaintHealth.observe(SDL_FillRect(_screen, &_clear, 0) == 0);
#else
	else SDL_FillRect(_screen, &_clear, 0);
#endif
}

/**
 * Changes the 8bpp palette used to render the screen's contents.
 * @param colors Pointer to the set of colors.
 * @param firstcolor Offset of the first color to replace.
 * @param ncolors Amount of colors to replace.
 * @param immediately Apply palette changes immediately, otherwise wait for next blit.
 */
void Screen::setPalette(SDL_Color* colors, int firstcolor, int ncolors, bool immediately)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentLastFlipOk = false; ++_agentDisplayRevision;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    _snapshotConfiguration.renew();
#endif
	if (_numColors && (_numColors != ncolors) && (_firstColor != firstcolor))
	{
		// an initial palette setup has not been committed to the screen yet
		// just update it with whatever colors are being sent now
		memmove(&(deferredPalette[firstcolor]), colors, sizeof(SDL_Color)*ncolors);
		_numColors = 256; // all the use cases are just a full palette with 16-color follow-ups
		_firstColor = 0;
	} else
	{
		memmove(&(deferredPalette[firstcolor]), colors, sizeof(SDL_Color) * ncolors);
		_numColors = ncolors;
		_firstColor = firstcolor;
	}

	_surface->setPalette(colors, firstcolor, ncolors);

	// defer actual update of screen until SDL_Flip()
	if (immediately && _screen->format->BitsPerPixel == 8)
	{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
        _snapshotAppliedPalette.renew();
#endif
		const bool paletteExact = SDL_SetColors(_screen, colors, firstcolor, ncolors) != 0;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        if (!paletteExact) _agentPaletteExact = false;
        else if (firstcolor == 0 && ncolors == 256) _agentPaletteExact = true;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
        _snapshotPaintHealth.observe(paletteExact);
        if (!paletteExact) _snapshotFullPalette = false;
        else if (firstcolor == 0 && ncolors == 256) _snapshotFullPalette = true;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
		if (!paletteExact) _observerPaletteExact = false;
		else if (firstcolor == 0 && ncolors == 256) _observerPaletteExact = true;
#endif
		if (!paletteExact) Log(LOG_DEBUG) << "Display palette doesn't match requested palette";
	}

	// Sanity check
	/*
	SDL_Color *newcolors = _screen->format->palette->colors;
	for (int i = firstcolor, j = 0; i < firstcolor + ncolors; i++, j++)
	{
		Log(LOG_DEBUG) << (int)newcolors[i].r << " - " << (int)newcolors[i].g << " - " << (int)newcolors[i].b;
		Log(LOG_DEBUG) << (int)colors[j].r << " + " << (int)colors[j].g << " + " << (int)colors[j].b;
		if (newcolors[i].r != colors[j].r ||
			newcolors[i].g != colors[j].g ||
			newcolors[i].b != colors[j].b)
		{
			Log(LOG_ERROR) << "Display palette doesn't match requested palette";
			break;
		}
	}
	*/
}

/**
 * Returns the screen's 8bpp palette.
 * @return Pointer to the palette's colors.
 */
SDL_Color *Screen::getPalette() const
{
	return (SDL_Color*)deferredPalette;
}

/**
 * Returns the width of the screen.
 * @return Width in pixels.
 */
int Screen::getWidth() const
{
	return _screen->w;
}

/**
 * Returns the height of the screen.
 * @return Height in pixels
 */
int Screen::getHeight() const
{
	return _screen->h;
}

/**
 * Resets the screen surfaces based on the current display options,
 * as they don't automatically take effect.
 * @param resetVideo Reset display surface.
 */
void Screen::resetDisplay(bool resetVideo)
{
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    _agentLastFlipOk = false; ++_agentDisplayRevision;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    // Invalidate before any replacement attempt, including a reused SDL address.
    _snapshotBacking.renew();
    _snapshotConfiguration.renew();
    _snapshotFullPalette = false;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    if (_stanceController) _stanceController->invalidate(true);
#endif
    if (_presentationObserver) _presentationObserver->resetPresentation();
	_presentedHud = Observer::HudFrame();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
	_observerPaletteExact = false;
#endif
	int width = Options::displayWidth;
	int height = Options::displayHeight;
#ifdef __linux__
	Uint32 oldFlags = _flags;
#endif
	makeVideoFlags();

	if (!_surface || (_surface->getSurface()->format->BitsPerPixel != _bpp ||
		_surface->getSurface()->w != _baseWidth ||
		_surface->getSurface()->h != _baseHeight)) // don't reallocate _surface if not necessary, it's a waste of CPU cycles
	{
		if (_surface) delete _surface;
		_surface = new Surface(_baseWidth, _baseHeight, 0, 0, Screen::use32bitScaler() ? 32 : 8); // only HQX/XBRZ needs 32bpp for this surface; the OpenGL class has its own 32bpp buffer
		if (_surface->getSurface()->format->BitsPerPixel == 8) _surface->setPalette(deferredPalette);
	}
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
	_snapshotPaintHealth.observe(SDL_SetColorKey(_surface->getSurface(), 0, 0) == 0); // turn off color key!
#else
	SDL_SetColorKey(_surface->getSurface(), 0, 0); // turn off color key!
#endif

	if (resetVideo || _screen->format->BitsPerPixel != _bpp)
	{
#ifdef __linux__
		// Workaround for segfault when switching to opengl
		if (!(oldFlags & SDL_OPENGL) && (_flags & SDL_OPENGL))
		{
			Uint8 cursor = 0;
			char *_oldtitle = 0;
			SDL_WM_GetCaption(&_oldtitle, NULL);
			std::string title(_oldtitle);
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			SDL_InitSubSystem(SDL_INIT_VIDEO);
			SDL_ShowCursor(SDL_ENABLE);
			SDL_EnableUNICODE(1);
			SDL_WM_SetCaption(title.c_str(), 0);
			SDL_SetCursor(SDL_CreateCursor(&cursor, &cursor, 1,1,0,0));
		}
#endif
		Log(LOG_INFO) << "Attempting to set display to " << width << "x" << height << "x" << _bpp << "...";
		_screen = SDL_SetVideoMode(width, height, _bpp, _flags);
		if (_screen == 0)
		{
			Log(LOG_ERROR) << SDL_GetError();
			Log(LOG_INFO) << "Attempting to set display to default resolution...";
			_screen = SDL_SetVideoMode(640, 400, _bpp, _flags);
			if (_screen == 0)
			{
				if (_flags & SDL_OPENGL)
				{
					Options::useOpenGL = false;
				}
				throw Exception(SDL_GetError());
			}
		}
		Log(LOG_INFO) << "Display set to " << getWidth() << "x" << getHeight() << "x" << (int)_screen->format->BitsPerPixel << ".";
	}
	else
	{
		clear();
	}

	Options::displayWidth = getWidth();
	Options::displayHeight = getHeight();
	_scaleX = getWidth() / (double)_baseWidth;
	_scaleY = getHeight() / (double)_baseHeight;
	_clear.x = 0;
	_clear.y = 0;
	_clear.w = getWidth();
	_clear.h = getHeight();

	double pixelRatioY = 1.0;
	if (Options::nonSquarePixelRatio && !Options::allowResize)
	{
		pixelRatioY = 1.2;
	}
	bool cursorInBlackBands;
	if (!Options::keepAspectRatio)
	{
		cursorInBlackBands = false;
	}
	else if (Options::fullscreen)
	{
		cursorInBlackBands = Options::cursorInBlackBandsInFullscreen;
	}
	else if (!Options::borderless)
	{
		cursorInBlackBands = Options::cursorInBlackBandsInWindow;
	}
	else
	{
		cursorInBlackBands = Options::cursorInBlackBandsInBorderlessWindow;
	}

	if (_scaleX > _scaleY && Options::keepAspectRatio)
	{
		int targetWidth = (int)floor(_scaleY * (double)_baseWidth);
		_topBlackBand = _bottomBlackBand = 0;
		_leftBlackBand = (getWidth() - targetWidth) / 2;
		if (_leftBlackBand < 0)
		{
			_leftBlackBand = 0;
		}
		_rightBlackBand = getWidth() - targetWidth - _leftBlackBand;
		_cursorTopBlackBand = 0;

		if (cursorInBlackBands)
		{
			_scaleX = _scaleY;
			_cursorLeftBlackBand = _leftBlackBand;
		}
		else
		{
			_cursorLeftBlackBand = 0;
		}
	}
	else if (_scaleY > _scaleX && Options::keepAspectRatio)
	{
		int targetHeight = (int)floor(_scaleX * (double)_baseHeight * pixelRatioY);
		_topBlackBand = (getHeight() - targetHeight) / 2;
		if (_topBlackBand < 0)
		{
			_topBlackBand = 0;
		}
		_bottomBlackBand = getHeight() - targetHeight - _topBlackBand;
		if (_bottomBlackBand < 0)
		{
			_bottomBlackBand = 0;
		}
		_leftBlackBand = _rightBlackBand = 0;
		_cursorLeftBlackBand = 0;

		if (cursorInBlackBands)
		{
			_scaleY = _scaleX;
			_cursorTopBlackBand = _topBlackBand;
		}
		else
		{
			_cursorTopBlackBand = 0;
		}
	}
	else
	{
		_topBlackBand = _bottomBlackBand = _leftBlackBand = _rightBlackBand = _cursorTopBlackBand = _cursorLeftBlackBand = 0;
	}

	if (useOpenGL())
	{
#ifndef __NO_OPENGL
		OpenGL::checkErrors = Options::checkOpenGLErrors;
		glOutput.init(_baseWidth, _baseHeight);
		glOutput.linear = Options::useOpenGLSmoothing; // setting from shader file will override this, though
		if (!FileMap::isResourcesEmpty())
		{
			if (!glOutput.set_shader(FileMap::getFilePath(Options::useOpenGLShader).c_str()))
			{
				Options::useOpenGLShader = "";
			}
		}
		glOutput.setVSync(Options::vSyncForOpenGL);
#endif
	}

	if (_screen->format->BitsPerPixel == 8)
	{
		setPalette(getPalette());
	}
}

/**
 * Returns the screen's X scale.
 * @return Scale factor.
 */
double Screen::getXScale() const
{
	return _scaleX;
}

/**
 * Returns the screen's Y scale.
 * @return Scale factor.
 */
double Screen::getYScale() const
{
	return _scaleY;
}

/**
 * Returns the screen's top black forbidden to cursor band's height.
 * @return Height in pixel.
 */
int Screen::getCursorTopBlackBand() const
{
	return _cursorTopBlackBand;
}

/**
 * Returns the screen's left black forbidden to cursor band's width.
 * @return Width in pixel.
 */
int Screen::getCursorLeftBlackBand() const
{
	return _cursorLeftBlackBand;
}

/**
 * Saves a screenshot of the screen's contents.
 * @param filename Filename of the PNG file.
 */
void Screen::screenshot(const std::string &filename) const
{
	SDL_Surface *screenshot = SDL_AllocSurface(0, getWidth() - getWidth()%4, getHeight(), 24, 0xff, 0xff00, 0xff0000, 0);

	if (useOpenGL())
	{
#ifndef __NO_OPENGL
		GLenum format = GL_RGB;

		for (int y = 0; y < getHeight(); ++y)
		{
			glReadPixels(0, getHeight()-(y+1), getWidth() - getWidth()%4, 1, format, GL_UNSIGNED_BYTE, ((Uint8*)screenshot->pixels) + y*screenshot->pitch);
		}
		glErrorCheck();
#endif
	}
	else
	{
		SDL_BlitSurface(_screen, 0, screenshot, 0);
	}

	unsigned error = lodepng::encode(filename, (const unsigned char *)(screenshot->pixels), getWidth() - getWidth()%4, getHeight(), LCT_RGB);
	if (error)
	{
		Log(LOG_ERROR) << "Saving to PNG failed: " << lodepng_error_text(error);
	}

	SDL_FreeSurface(screenshot);
}


/**
 * Check whether a 32bpp scaler has been selected.
 * @return if it is enabled with a compatible resolution.
 */
bool Screen::use32bitScaler()
{
	int w = Options::displayWidth;
	int h = Options::displayHeight;
	int baseW = Options::baseXResolution;
	int baseH = Options::baseYResolution;
	int maxScale = 0;

	if (Options::useHQXFilter)
	{
		maxScale = 4;
	}
	else if (Options::useXBRZFilter)
	{
		maxScale = 6;
	}

	for (int i = 2; i <= maxScale; i++)
	{
		if (w == baseW * i && h == baseH * i)
		{
			return true;
		}
	}
	return false;
}

/**
 * Check if OpenGL is enabled.
 * @return if it is enabled.
 */
bool Screen::useOpenGL()
{
#ifdef __NO_OPENGL
	return false;
#else
	return Options::useOpenGL;
#endif
}

/**
 * Gets the Horizontal offset from the mid-point of the screen, in pixels.
 * @return the horizontal offset.
 */
int Screen::getDX() const
{
	return (_baseWidth - ORIGINAL_WIDTH) / 2;
}

/**
 * Gets the Vertical offset from the mid-point of the screen, in pixels.
 * @return the vertical offset.
 */
int Screen::getDY() const
{
	return (_baseHeight - ORIGINAL_HEIGHT) / 2;
}

/**
 * Changes a given scale, and if necessary, switch the current base resolution.
 * @param type the new scale level.
 * @param width reference to which x scale to adjust.
 * @param height reference to which y scale to adjust.
 * @param change should we change the current scale.
 */
void Screen::updateScale(int type, int &width, int &height, bool change)
{
	double pixelRatioY = 1.0;

	if (Options::nonSquarePixelRatio)
	{
		pixelRatioY = 1.2;
	}

	switch (type)
	{
	case SCALE_15X:
		width = Screen::ORIGINAL_WIDTH * 1.5;
		height = Screen::ORIGINAL_HEIGHT * 1.5;
		break;
	case SCALE_2X:
		width = Screen::ORIGINAL_WIDTH * 2;
		height = Screen::ORIGINAL_HEIGHT * 2;
		break;
	case SCALE_SCREEN_DIV_3:
		width = Options::displayWidth / 3.0;
		height = Options::displayHeight / pixelRatioY / 3.0;
		break;
	case SCALE_SCREEN_DIV_2:
		width = Options::displayWidth / 2.0;
		height = Options::displayHeight / pixelRatioY  / 2.0;
		break;
	case SCALE_SCREEN:
		width = Options::displayWidth;
		height = Options::displayHeight / pixelRatioY;
		break;
	case SCALE_ORIGINAL:
	default:
		width = Screen::ORIGINAL_WIDTH;
		height = Screen::ORIGINAL_HEIGHT;
		break;
	}

	// don't go under minimum resolution... it's bad, mmkay?
	width = std::max(width, Screen::ORIGINAL_WIDTH);
	height = std::max(height, Screen::ORIGINAL_HEIGHT);

	if (change && (Options::baseXResolution != width || Options::baseYResolution != height))
	{
		Options::baseXResolution = width;
		Options::baseYResolution = height;
	}
}

}

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace OpenXcom {
bool Screen::agentPresentationReady() const
{
    return _agentLastFlipOk && !useOpenGL() && _surface && _screen &&
        !(_pushPalette && _numColors && _screen->format->BitsPerPixel == 8) && _baseWidth > 0 && _baseHeight > 0;
}
}
#endif

#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace OpenXcom {
bool Screen::agentCaptureOutput(const Surface *source, Observer::AgentProjectileOutput &output) const
{
    return source && _surface && agentPresentationReady() &&
        _surface->getWidth() == _baseWidth && _surface->getHeight() == _baseHeight &&
        output.capture(_agentLastFlipOk, useOpenGL() || use32bitScaler() || Options::useScaleFilter || Options::useHQXFilter || Options::useXBRZFilter,
            _surface->getSurface(), _screen, source->getSurface(),
            _topBlackBand, _bottomBlackBand, _leftBlackBand, _rightBlackBand);
}
Observer::AgentProjectileFact Screen::agentProjectileFact(const Observer::AgentProjectileWitness &witness,
    const Surface *map, int visibleHeight, const std::function<bool(int, int)> &pixelAdmitted) const
{
    Observer::AgentProjectileOutput output;
    if (!agentCaptureOutput(map, output)) return Observer::AgentProjectileFact::Unknown;
    return witness.evaluate(true, {0, 0, map->getWidth(), visibleHeight}, {}, output.palette,
        [&](int x, int y, std::uint32_t rgb) { return output.matches(map->getX() + x, map->getY() + y, rgb); }, pixelAdmitted);
}
}
#endif
