// Fork modification 2026-10-11: default-off supervised stance pilot; GPL-3.0-or-later.
	#pragma once
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include "../Observer/AgentProjectileWitness.h"
#include "../Observer/AgentProjectileOutput.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include <cstdint>
#endif
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
#include <SDL.h>
#include <string>
#include "OpenGL.h"
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
#include "Zoom.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
#include "../Observer/SnapshotPresentation.h"
#include "../Observer/SnapshotPaint.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
#include "../Observer/PresentedHud.h"
#endif

namespace OpenXcom
{

#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
namespace Observer { class CursorFileObserver; class PresentationObserver; }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
namespace Observer { class StanceController; }
#endif
class Surface;
class Action;

/**
 * A display screen, handles rendering onto the game window.
 * In SDL a Screen is treated like a Surface, so this is just
 * a specialized version of a Surface with functionality more
 * relevant for display screens. Contains a Surface buffer
 * where all the contents are kept, so any filters or conversions
 * can be applied before rendering the screen.
 */
class Screen
{
private:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool _agentLastFlipOk = false, _agentPaletteExact = false;
    std::uint64_t _agentDisplayRevision = 0;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    friend class Game;
    Observer::SnapshotPresentation *_snapshotPresentation = 0; // owned by Game
    Observer::SnapshotStamp _snapshotScreenLife, _snapshotBacking, _snapshotConfiguration, _snapshotAppliedPalette;
    Observer::SnapshotPaintHealth _snapshotPaintHealth;
    bool _snapshotFullPalette = false;
    Observer::SnapshotDisplayFacts snapshotFacts(SDL_Surface *exactPhysical, bool allowPendingPalette = false) const noexcept;
    void snapshotComposedProof() noexcept;
    void snapshotPresentedProof(SDL_Surface *exactPhysical) noexcept;
    void snapshotFinishFrame(bool liveStateFactsCurrent) noexcept;
    void attachSnapshotPresentation(Observer::SnapshotPresentation *p) noexcept { _snapshotPresentation = p; }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    Observer::StanceController *_stanceController = 0;
    bool _stancePanelBlocked = false;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
	Observer::CursorFileObserver *_cursorObserver;
	Observer::PresentationObserver *_presentationObserver;
	bool _observerPaletteExact;
	Observer::HudFrame _presentedHud;
#endif
	SDL_Surface *_screen;
	int _bpp;
	int _baseWidth, _baseHeight;
	double _scaleX, _scaleY;
	int _topBlackBand, _bottomBlackBand, _leftBlackBand, _rightBlackBand, _cursorTopBlackBand, _cursorLeftBlackBand;
	Uint32 _flags;
	SDL_Color deferredPalette[256];
	int _numColors, _firstColor;
	bool _pushPalette;
	OpenGL glOutput;
	Surface *_surface;
	SDL_Rect _clear;
	/// Sets the _flags and _bpp variables based on game options; needed in more than one place now
	void makeVideoFlags();
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
	Zoom::Workspace _renderWorkspace;
#endif
public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    bool agentPresentationReady() const;
    bool agentCaptureOutput(const Surface *source, Observer::AgentProjectileOutput &output) const;
    Observer::AgentProjectileFact agentProjectileFact(const Observer::AgentProjectileWitness &witness,
        const Surface *map, int visibleHeight, const std::function<bool(int, int)> &pixelAdmitted) const;
    std::uint64_t agentDisplayRevision() const { return _agentDisplayRevision; }
#endif
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
	// Immutable after capture. Surface pixels remain owner-writable; pointers,
	// dimensions/layout, source palette and GL state remain owner-exclusive.
	class LoadingPresentation
	{
		friend class Screen;
		SDL_Surface *_source, *_destination;
		Zoom::RenderConfig _config;
		Zoom::GlSnapshot _gl;
		int _top, _bottom, _left, _right, _first, _count;
		bool _zoom, _pending, _captured, _strict;
		SDL_Color _palette[256], _sourcePalette[256];
		int _sourceColors;
	public:
		LoadingPresentation() noexcept;
	};
private:
	static Zoom::RenderStatus capturePresentation(SDL_Surface *source,
		SDL_Surface *destination, int baseWidth, int baseHeight, int top, int bottom,
		int left, int right, const SDL_Color *palette, int first, int count,
		bool pending, OpenGL *gl, LoadingPresentation &out, bool strict) noexcept;
public:
	// This factory alone reads live Options. Do not call it during loading.
	// palette is a table covering absolute indices [first, first+count), not
	// a count-element pointer already advanced to the first selected color.
	static Zoom::RenderStatus captureLoadingPresentation(SDL_Surface *source,
		SDL_Surface *destination, int baseWidth, int baseHeight, int top, int bottom,
		int left, int right, const SDL_Color *palette, int first, int count,
		bool pending, OpenGL *gl, LoadingPresentation &out) noexcept;
	Zoom::RenderStatus captureLoadingPresentation(LoadingPresentation &out) const noexcept;
	// Shared production phases; no observer/control callback occurs inside them.
	static Zoom::RenderStatus copyPhase(const LoadingPresentation &frame,
		Zoom::Workspace &workspace, int *snapshotCopyResult = 0) noexcept;
	static Zoom::RenderStatus palettePhase(const LoadingPresentation &frame) noexcept;
	static Zoom::RenderStatus presentPhase(const LoadingPresentation &frame) noexcept;
	static Zoom::RenderStatus presentLoading(const LoadingPresentation &frame,
		Zoom::Workspace &workspace) noexcept;
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    Screen(const Screen &) = delete;
    Screen(Screen &&) = delete;
    Screen &operator=(const Screen &) = delete;
    Screen &operator=(Screen &&) = delete;
    Observer::SnapshotDisplayFacts snapshotDisplayFacts() const noexcept { return snapshotFacts(_screen); }
#endif
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_STANCE
    void setStanceController(Observer::StanceController *controller) { _stanceController = controller; }
    bool stanceAllowed() const;
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_INVENTORY
    // Continuity only: a normal full palette may still await its first flip.
    bool inventoryWaitingAllowed() const;
#endif
#endif
	static const int ORIGINAL_WIDTH;
	static const int ORIGINAL_HEIGHT;

	/// Creates a new display screen.
	Screen();
	/// Cleans up the display screen.
	~Screen();
	/// Get horizontal offset.
	int getDX() const;
	/// Get vertical offset.
	int getDY() const;
	/// Gets the internal buffer.
	Surface *getSurface();
	/// Handles keyboard events.
	void handle(Action *action);
	/// Renders the screen onto the game window.
#ifdef OPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER
	void beginObserverFrame();
	bool wantsPresentedHud() const;
	void excludePresentedHud(int x, int y, int width, int height);
	void setPresentedHud(const Observer::HudFrame &frame) { _presentedHud = frame; }
	// Coordinates are the UI cursor actually composed into this frame, never a probe.
	void flip(int cursorX = -1, int cursorY = -1);
#else
	void flip();
#endif
	/// Clears the screen.
	void clear();
	/// Sets the screen's 8bpp palette.
	void setPalette(SDL_Color *colors, int firstcolor = 0, int ncolors = 256, bool immediately = false);
	/// Gets the screen's 8bpp palette.
	SDL_Color *getPalette() const;
	/// Gets the screen's width.
	int getWidth() const;
	/// Gets the screen's height.
	int getHeight() const;
	/// Resets the screen display.
	void resetDisplay(bool resetVideo = true);
	/// Gets the screen's X scale.
	double getXScale() const;
	/// Gets the screen's Y scale.
	double getYScale() const;
	/// Gets the screen's top black forbidden to cursor band's height.
	int getCursorTopBlackBand() const;
	/// Gets the screen's left black forbidden to cursor band's width.
	int getCursorLeftBlackBand() const;
	/// Takes a screenshot.
	void screenshot(const std::string &filename) const;
	/// Checks whether a 32bit scaler is requested and works for the selected resolution
	static bool use32bitScaler();
	/// Checks whether OpenGL output is requested
	static bool useOpenGL();
	/// update the game scale as required.
	static void updateScale(int type, int &width, int &height, bool change);
};

}
