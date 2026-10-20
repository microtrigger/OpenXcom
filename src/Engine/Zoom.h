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
#include <SDL.h>
#include "OpenGL.h"
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
#include <cstddef>
#include <thread>
#endif

namespace OpenXcom
{


class Zoom
{

	public:
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
	// Capture only before the worker starts. Rendering never resolves Options.
	struct RenderConfig
	{
		bool openGL, scaler32, xbrz, hqx, scale;
	};
	enum class RenderStatus
	{
		Success, InvalidSurface, InvalidBands, UnsupportedFormat, InvalidOpenGL,
		WrongOwner, AllocationFailure, ScaleFailure, BlitFailure, PaletteFailure,
		FlipFailure, ExceptionFailure
	};
	// One owner thread for construction, every use, and destruction. HQX/xBRZ
	// process-wide lazy tables are likewise restricted to that rendering owner.
	class Workspace
	{
		friend class Zoom;
		Uint32 *_sax, *_say;
		std::size_t _xCapacity, _yCapacity;
		void *_scale4;
		std::size_t _scale4Capacity;
		const std::thread::id _owner;
		bool _proclaimed;
	public:
		Workspace() noexcept;
		~Workspace() noexcept;
		Workspace(const Workspace &) = delete;
		Workspace &operator=(const Workspace &) = delete;
		bool onOwner() const noexcept;
	};
	// The caller retains the GL object, Surface and pixels and freezes all GL
	// fields/checkErrors until retirement. The snapshot detects later changes.
	struct GlSnapshot
	{
		OpenGL *object;
		SDL_Surface *surface;
		void *surfaceObject, *buffer;
		unsigned width, height, format, bpp, texture, program;
		bool linear, shaderSupport, checkErrors;
		GlSnapshot() noexcept;
	};
	static RenderConfig captureRenderConfig();
	static RenderStatus captureGl(OpenGL *gl, GlSnapshot &snapshot) noexcept;
	static bool readableSurface(const SDL_Surface *surface) noexcept;
	// strictLoading=false is reserved for ordinary presentation to an actual
	// SDL-owned opaque destination. Only SDL blits touch that destination;
	// scaling uses the same checked software-temporary route as banded output.
	static RenderStatus validateCopy(SDL_Surface *src, SDL_Surface *dst, bool zoom,
		int top, int bottom, int left, int right, const RenderConfig &config,
		const GlSnapshot &gl, bool strictLoading = true) noexcept;
	static RenderStatus copy(SDL_Surface *src, SDL_Surface *dst, bool zoom,
		int top, int bottom, int left, int right, const RenderConfig &config,
		const GlSnapshot &gl, Workspace &workspace, int *snapshotCopyResult = 0,
		bool strictLoading = true) noexcept;
	static int _zoomSurfaceY(SDL_Surface *src, SDL_Surface *dst, int flipx, int flipy,
		const RenderConfig &config, Workspace &workspace);
#endif
	/// Flip screen given src and dst; might use software or OpenGL.
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
	static void flipWithZoom(SDL_Surface *src, SDL_Surface *dst, int topBlackBand, int bottomBlackBand, int leftBlackBand, int rightBlackBand, OpenGL *glOut, int *snapshotCopyResult = 0);
#else
	static void flipWithZoom(SDL_Surface *src, SDL_Surface *dst, int topBlackBand, int bottomBlackBand, int leftBlackBand, int rightBlackBand, OpenGL *glOut);
#endif
	/// Copy src to dst, resizing as needed. Please don't use flipx or flipy as the optimized functions ignore these parameters.
	static int _zoomSurfaceY(SDL_Surface * src, SDL_Surface * dst, int flipx, int flipy);
	/// Check for SSE2 instructions using CPUID.
	static bool haveSSE2();

private:

};

}
