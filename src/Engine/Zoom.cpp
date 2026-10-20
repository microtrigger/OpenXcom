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

#include "Zoom.h"

#include "Surface.h"
#include "Logger.h"
#include "Options.h"
#include "Screen.h"

#include "OpenGL.h"
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
#include <cstdlib>
#include <cstdint>
#include <limits>
#endif

// Scale2X
#include "Scalers/scalebit.h"

// HQX

#include "Scalers/hqx.h"

// xBRZ

#include "Scalers/xbrz.h"

#if (_MSC_VER >= 1400) || (defined(__MINGW32__) && defined(__SSE2__))

#ifndef __SSE2__
#define __SSE2__ true
#endif
// probably Visual Studio (or Intel C++ which should also work)
#include <intrin.h>
#endif

#ifdef __GNUC__
#if (__i386__ || __x86_64__)
#include <cpuid.h>
#endif
#endif

#ifdef __SSE2__
#include <emmintrin.h> // for SSE2 intrinsics; see http://msdn.microsoft.com/en-us/library/has3d153%28v=vs.71%29.aspx
#endif



namespace OpenXcom
{

#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
namespace
{
const int loadingMaximumDimension = 8192;
const std::size_t loadingMaximumSpan = 64u * 1024u * 1024u;

bool loadingDimensions(const SDL_Surface *s) noexcept
{
    return s && s->format && s->w > 0 && s->h > 0 &&
        s->w <= loadingMaximumDimension && s->h <= loadingMaximumDimension;
}

bool loadingSameLayout(const SDL_PixelFormat *a, const SDL_PixelFormat *b) noexcept
{
    return a->BitsPerPixel == b->BitsPerPixel && a->BytesPerPixel == b->BytesPerPixel &&
        a->Rmask == b->Rmask && a->Gmask == b->Gmask && a->Bmask == b->Bmask && a->Amask == b->Amask &&
        a->Rshift == b->Rshift && a->Gshift == b->Gshift && a->Bshift == b->Bshift && a->Ashift == b->Ashift &&
        a->Rloss == b->Rloss && a->Gloss == b->Gloss && a->Bloss == b->Bloss && a->Aloss == b->Aloss;
}

bool loadingChannel(Uint32 mask, unsigned shift, unsigned loss, unsigned bits) noexcept
{
    if (shift >= bits || loss > 8) return false;
    if (!mask) return loss == 8;
    return loss < 8 && 8 - loss <= bits - shift && mask == ((0xffu >> loss) << shift);
}

bool loadingTrueColor(const SDL_PixelFormat *f) noexcept
{
    return !f->palette && f->Rmask && f->Gmask && f->Bmask &&
        !(f->Rmask & (f->Gmask | f->Bmask | f->Amask)) &&
        !(f->Gmask & (f->Bmask | f->Amask)) && !(f->Bmask & f->Amask) &&
        loadingChannel(f->Rmask, f->Rshift, f->Rloss, f->BitsPerPixel) &&
        loadingChannel(f->Gmask, f->Gshift, f->Gloss, f->BitsPerPixel) &&
        loadingChannel(f->Bmask, f->Bshift, f->Bloss, f->BitsPerPixel) &&
        loadingChannel(f->Amask, f->Ashift, f->Aloss, f->BitsPerPixel);
}

bool loadingLayout(const SDL_Surface *s) noexcept
{
    if (!loadingDimensions(s)) return false;
    const SDL_PixelFormat *f = s->format;
    if (f->BytesPerPixel == 1 && f->BitsPerPixel == 8)
        return f->palette && f->palette->colors && f->palette->ncolors == 256;
    return ((f->BytesPerPixel == 2 && f->BitsPerPixel == 16) ||
        (f->BytesPerPixel == 4 && f->BitsPerPixel == 32)) && loadingTrueColor(f);
}

bool loadingRgb32(const SDL_PixelFormat *f) noexcept
{
    return f->BitsPerPixel == 32 && f->BytesPerPixel == 4 &&
        f->Rmask == 0x00ff0000u && f->Gmask == 0x0000ff00u && f->Bmask == 0x000000ffu &&
        (f->Amask == 0 || f->Amask == 0xff000000u);
}

bool loadingDisjoint(const SDL_Surface *a, const SDL_Surface *b) noexcept
{
    const std::uintptr_t ap = reinterpret_cast<std::uintptr_t>(a->pixels);
    const std::uintptr_t bp = reinterpret_cast<std::uintptr_t>(b->pixels);
    const std::size_t as = static_cast<std::size_t>(a->pitch) * a->h;
    const std::size_t bs = static_cast<std::size_t>(b->pitch) * b->h;
    const std::uintptr_t maximum = std::numeric_limits<std::uintptr_t>::max();
    return ap <= maximum - as && bp <= maximum - bs && (ap + as <= bp || bp + bs <= ap);
}

bool loadingScaleSupported(SDL_Surface *src, SDL_Surface *dst, const Zoom::RenderConfig &config) noexcept
{
    if (!loadingSameLayout(src->format, dst->format)) return false;
    if (config.scaler32)
    {
        if (!loadingRgb32(src->format)) return false;
        if (config.xbrz)
            for (int factor = 2; factor <= 6; ++factor)
                if (dst->w == src->w * factor && dst->h == src->h * factor)
                    // The actual xBRZ call selects RGB, whose interpolation
                    // does not preserve an active alpha channel.
                    return src->format->Amask == 0 &&
                        src->pitch == src->w * 4 && dst->pitch == dst->w * 4;
        if (config.hqx)
            for (int factor = 2; factor <= 4; ++factor)
                if (dst->w == src->w * factor && dst->h == src->h * factor) return true;
        return false;
    }
    if (config.scale)
        for (int factor = 2; factor <= 4; ++factor)
            if (dst->w == src->w * factor && dst->h == src->h * factor &&
                !scale_precondition(factor, src->format->BytesPerPixel, src->w, src->h)) return true;
    // The production fallback advances one byte at a time. Never admit RGB.
    return src->format->BitsPerPixel == 8 && src->format->BytesPerPixel == 1;
}

bool loadingGlUnchanged(const Zoom::GlSnapshot &g) noexcept
{
#ifndef __NO_OPENGL
    return g.object && g.surfaceObject && g.surface && g.buffer && g.texture &&
        g.object->buffer_surface == g.surfaceObject && g.object->buffer_surface->getSurface() == g.surface &&
        g.object->buffer == g.buffer && g.object->iwidth == g.width && g.object->iheight == g.height &&
        g.object->iformat == g.format && g.object->ibpp == g.bpp && g.object->gltexture == g.texture &&
        g.object->glprogram == g.program && g.object->linear == g.linear &&
        g.object->shader_support == g.shaderSupport && OpenGL::checkErrors == g.checkErrors &&
        g.width > 0 && g.width <= static_cast<unsigned>(loadingMaximumDimension) &&
        g.height > 0 && g.height <= static_cast<unsigned>(loadingMaximumDimension) &&
        g.bpp == 32 && g.format == GL_UNSIGNED_INT_8_8_8_8_REV &&
        g.surface->pixels == g.buffer && Zoom::readableSurface(g.surface) &&
        g.surface->w == static_cast<int>(g.width) && g.surface->h == static_cast<int>(g.height) &&
        loadingRgb32(g.surface->format);
#else
    (void)g;
    return false;
#endif
}

struct LoadingTemporary
{
    SDL_Surface *surface;
    explicit LoadingTemporary(SDL_Surface *value) noexcept : surface(value) {}
    ~LoadingTemporary() noexcept { if (surface) SDL_FreeSurface(surface); }
    LoadingTemporary(const LoadingTemporary &) = delete;
    LoadingTemporary &operator=(const LoadingTemporary &) = delete;
};
}

Zoom::Workspace::Workspace() noexcept : _sax(0), _say(0), _xCapacity(0), _yCapacity(0),
    _scale4(0), _scale4Capacity(0), _owner(std::this_thread::get_id()), _proclaimed(false) {}
Zoom::Workspace::~Workspace() noexcept
{
    if (!onOwner()) std::abort();
    std::free(_sax);
    std::free(_say);
    std::free(_scale4);
}
bool Zoom::Workspace::onOwner() const noexcept { return _owner == std::this_thread::get_id(); }

Zoom::GlSnapshot::GlSnapshot() noexcept : object(0), surface(0), surfaceObject(0), buffer(0),
    width(0), height(0), format(0), bpp(0), texture(0), program(0), linear(false),
    shaderSupport(false), checkErrors(false) {}

Zoom::RenderConfig Zoom::captureRenderConfig()
{
    RenderConfig result = {Screen::useOpenGL(), false,
        Options::useXBRZFilter, Options::useHQXFilter, Options::useScaleFilter};
    // Resolve the same legacy option precedence without signed multiplication
    // overflow from invalid live dimensions. No Options access follows capture.
    const long long width = Options::displayWidth, height = Options::displayHeight;
    const long long baseWidth = Options::baseXResolution, baseHeight = Options::baseYResolution;
    const int maximum = result.hqx ? 4 : (result.xbrz ? 6 : 0);
    if (width > 0 && height > 0 && baseWidth > 0 && baseHeight > 0)
        for (int factor = 2; factor <= maximum; ++factor)
            if (width == baseWidth * factor && height == baseHeight * factor) result.scaler32 = true;
    return result;
}

Zoom::RenderStatus Zoom::captureGl(OpenGL *gl, GlSnapshot &out) noexcept
{
    out = GlSnapshot();
#ifndef __NO_OPENGL
    if (!gl || !gl->buffer_surface) return RenderStatus::InvalidOpenGL;
    out.object = gl;
    out.surfaceObject = gl->buffer_surface;
    out.surface = gl->buffer_surface->getSurface();
    out.buffer = gl->buffer;
    out.width = gl->iwidth; out.height = gl->iheight;
    out.format = gl->iformat; out.bpp = gl->ibpp;
    out.texture = gl->gltexture; out.program = gl->glprogram;
    out.linear = gl->linear; out.shaderSupport = gl->shader_support;
    out.checkErrors = OpenGL::checkErrors;
    return loadingGlUnchanged(out) ? RenderStatus::Success : RenderStatus::InvalidOpenGL;
#else
    (void)gl;
    return RenderStatus::InvalidOpenGL;
#endif
}

bool Zoom::readableSurface(const SDL_Surface *s) noexcept
{
    if (!loadingLayout(s) || !s->pixels || SDL_MUSTLOCK(s) ||
        (s->flags & (SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_RLEACCEL | SDL_OPENGL | SDL_DOUBLEBUF))) return false;
    const SDL_PixelFormat *f = s->format;
    const unsigned bytes = f->BytesPerPixel;
    const std::size_t row = static_cast<std::size_t>(s->w) * bytes;
    if (s->pitch < row || s->pitch % bytes || reinterpret_cast<std::uintptr_t>(s->pixels) % bytes) return false;
    const std::size_t span = static_cast<std::size_t>(s->pitch) * s->h;
    return span <= loadingMaximumSpan && reinterpret_cast<std::uintptr_t>(s->pixels) <=
        std::numeric_limits<std::uintptr_t>::max() - span;
}

Zoom::RenderStatus Zoom::validateCopy(SDL_Surface *src, SDL_Surface *dst, bool zoom,
    int top, int bottom, int left, int right, const RenderConfig &config, const GlSnapshot &gl,
    bool strictLoading) noexcept
{
    if (!readableSurface(src) || !loadingDimensions(dst)) return RenderStatus::InvalidSurface;
    if (top < 0 || bottom < 0 || left < 0 || right < 0 || top >= dst->h || bottom >= dst->h - top ||
        left >= dst->w || right >= dst->w - left) return RenderStatus::InvalidBands;
    if (config.openGL)
    {
        if (!zoom || !loadingGlUnchanged(gl) || src->w != gl.surface->w || src->h != gl.surface->h ||
            !loadingDisjoint(src, gl.surface)) return RenderStatus::InvalidOpenGL;
        return RenderStatus::Success;
    }
    const bool directPixels = readableSurface(dst);
    const bool sdlOwnedDestination = SDL_MUSTLOCK(dst) ||
        (dst->flags & (SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_RLEACCEL | SDL_DOUBLEBUF));
    // Ordinary flip may pass an actual SDL-owned opaque destination. Never
    // dereference its pixels; SDL alone performs the final blit. Loading stays
    // restricted to directly addressable surfaces. Broken software metadata
    // does not gain admission through this ordinary-only exception.
    if ((!directPixels && (strictLoading || !sdlOwnedDestination || !loadingLayout(dst) || (dst->flags & SDL_OPENGL))) ||
        src == dst || (directPixels && !loadingDisjoint(src, dst))) return RenderStatus::InvalidSurface;
    if (!loadingSameLayout(src->format, dst->format)) return RenderStatus::UnsupportedFormat;
    if (!zoom)
        return src->w == dst->w && src->h == dst->h && !top && !bottom && !left && !right ?
            RenderStatus::Success : RenderStatus::InvalidBands;
    const int width = dst->w - left - right, height = dst->h - top - bottom;
    if ((top || bottom || left || right) && width == src->w && height == src->h) return RenderStatus::Success;
    // A view of the prospective temporary layout, without allocating or pixels.
    SDL_Surface target = *dst;
    target.w = width; target.h = height;
    if (top || bottom || left || right || !directPixels)
        target.pitch = static_cast<Uint16>((static_cast<unsigned>(width) * dst->format->BytesPerPixel + 3u) & ~3u);
    if (static_cast<std::size_t>(target.pitch) * height > loadingMaximumSpan) return RenderStatus::InvalidSurface;
    return loadingScaleSupported(src, &target, config) ? RenderStatus::Success : RenderStatus::UnsupportedFormat;
}

Zoom::RenderStatus Zoom::copy(SDL_Surface *src, SDL_Surface *dst, bool zoom,
    int top, int bottom, int left, int right, const RenderConfig &config,
    const GlSnapshot &gl, Workspace &workspace, int *snapshotCopyResult, bool strictLoading) noexcept
{
    if (snapshotCopyResult) *snapshotCopyResult = -2;
    if (!workspace.onOwner()) return RenderStatus::WrongOwner;
    const RenderStatus admission = validateCopy(src, dst, zoom, top, bottom, left, right, config, gl, strictLoading);
    if (admission != RenderStatus::Success) return admission;
    try
    {
        if (!zoom)
        {
            const int result = SDL_BlitSurface(src, 0, dst, 0);
            if (snapshotCopyResult) *snapshotCopyResult = result;
            return result == 0 ? RenderStatus::Success : RenderStatus::BlitFailure;
        }
        if (config.openGL)
        {
#ifndef __NO_OPENGL
            if (SDL_BlitSurface(src, 0, gl.surface, 0) != 0) return RenderStatus::BlitFailure;
            // These are void dispatch boundaries, not GPU completion/status.
            const auto dispatch = [&]()
            {
                gl.object->refresh(gl.linear, gl.width, gl.height, dst->w, dst->h, top, bottom, left, right);
                SDL_GL_SwapBuffers();
            };
            if (strictLoading)
            {
                Logger::ScopedSuppression suppress;
                dispatch();
            }
            else dispatch(); // Preserve ordinary GL diagnostic evaluation.
            return RenderStatus::Success;
#else
            return RenderStatus::InvalidOpenGL;
#endif
        }
        const bool banded = top || bottom || left || right;
        if (!banded && readableSurface(dst))
        {
            const int result = _zoomSurfaceY(src, dst, 0, 0, config, workspace);
            if (snapshotCopyResult) *snapshotCopyResult = result == 0 ? 0 : -1;
            return result == 0 ? RenderStatus::Success :
                (result == -2 ? RenderStatus::AllocationFailure : RenderStatus::ScaleFailure);
        }
        const int width = dst->w - left - right, height = dst->h - top - bottom;
        SDL_Rect rect = {static_cast<Sint16>(left), static_cast<Sint16>(top),
            static_cast<Uint16>(width), static_cast<Uint16>(height)};
        if (width == src->w && height == src->h)
            return SDL_BlitSurface(src, 0, dst, &rect) == 0 ? RenderStatus::Success : RenderStatus::BlitFailure;
        LoadingTemporary tmp(SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, dst->format->BitsPerPixel,
            dst->format->Rmask, dst->format->Gmask, dst->format->Bmask, dst->format->Amask));
        if (!tmp.surface)
        {
            if (!banded && snapshotCopyResult) *snapshotCopyResult = -1;
            return RenderStatus::AllocationFailure;
        }
        const int scaled = _zoomSurfaceY(src, tmp.surface, 0, 0, config, workspace);
        if (scaled != 0)
        {
            if (!banded && snapshotCopyResult) *snapshotCopyResult = -1;
            return scaled == -2 ? RenderStatus::AllocationFailure : RenderStatus::ScaleFailure;
        }
        if (src->format->palette && SDL_SetPalette(tmp.surface, SDL_LOGPAL | SDL_PHYSPAL,
            src->format->palette->colors, 0, src->format->palette->ncolors) == 0)
        {
            if (!banded && snapshotCopyResult) *snapshotCopyResult = -1;
            return RenderStatus::PaletteFailure;
        }
        const int copied = SDL_BlitSurface(tmp.surface, 0, dst, &rect);
        if (!banded && snapshotCopyResult) *snapshotCopyResult = copied;
        return copied == 0 ? RenderStatus::Success : RenderStatus::BlitFailure;
    }
    catch (...) { return RenderStatus::ExceptionFailure; }
}
#endif


/**
 * Optimized 8-bit zoomer for resizing by a factor of 2. Doesn't flip.
 * Used internally by _zoomSurfaceY() below.
 * source and dest. widths must be multiples of 8 bytes for 64-bit access
 *
 * @param src The surface to zoom (input).
 * @param dst The zoomed surface (output).
 * @return 0 for success or -1 for error.
 */
/*
static int zoomSurface2X_64bit(SDL_Surface *src, SDL_Surface *dst)
{
	Uint64 dataSrc;
	Uint64 dataDst;
	Uint8 *pixelSrc = (Uint8*)src->pixels;
	Uint8 *pixelDstRow = (Uint8*)dst->pixels;
	int sx, sy;
	static bool proclaimed = false;

	if (!proclaimed)
	{
		proclaimed = true;
		Log(LOG_INFO) << "Using somewhat fast 2X zoom routine.";
	}

	for (sy = 0; sy < src->h; ++sy, pixelDstRow += dst->pitch*2)
	{
		Uint64 *pixelDst = (Uint64*)pixelDstRow;
		Uint64 *pixelDst2 = (Uint64*)(pixelDstRow + dst->pitch);
		for (sx = 0; sx < src->w; sx += 8, pixelSrc += 8)
		{
			dataSrc = *((Uint64*) pixelSrc);
			// boo
			(void)SDL_SwapLE64(dataSrc);
 */
/* expanded form of data shift:
			dataDst = (dataSrc & 0xFF) | ((dataSrc & 0xFF) << 8) |
				((dataSrc & 0xFF00 ) << 8) | ((dataSrc & 0xFF00)) << 16)  |
				((dataSrc & 0xFF0000) << 16) | ((dataSrc & 0xFF0000) << 24) |
				((dataSrc & 0xFF000000) << 24) | ((dataSrc & 0xFF000000) << 32);
 */
			// compact form, combining terms with equal multipliers (shifts)
/*
			dataDst = (dataSrc & 0xFF) | ((dataSrc & 0xFFFF) << 8) |
				((dataSrc & 0xFFFF00) << 16)  |
				((dataSrc & 0xFFFF0000) << 24) |
				((dataSrc & 0xFF000000) << 32);

			*pixelDst = dataDst;
			*pixelDst2 = dataDst;
			pixelDst++; // forward 8 bytes!
			pixelDst2++;
			dataSrc >>= 32;

			dataDst = (dataSrc & 0xFF) | ((dataSrc & 0xFFFF) << 8) |
				((dataSrc & 0xFFFF00) << 16)  |
				((dataSrc & 0xFFFF0000) << 24) |
				((dataSrc & 0xFF000000) << 32);

			*pixelDst = dataDst;
			*pixelDst2 = dataDst;
			pixelDst++;	// 8 bytes again
			pixelDst2++;
		}
	}

	return 0;
}
 */


#if defined(__WORDSIZE) && (__WORDSIZE == 64) || defined(SIZE_MAX) && (SIZE_MAX > 0xFFFFFFFF)
#else
/**
 * Optimized 8-bit zoomer for resizing by a factor of 2. Doesn't flip.
 * 32-bit version for sad old x86 chips which run out of registers
 * with the 64-bit version.
 * Used internally by _zoomSurfaceY() below.
 * source and dest. widths must be multiples of 4 bytes for 32-bit access
 *
 * @param src The surface to zoom (input).
 * @param dst The zoomed surface (output).
 * @return 0 for success or -1 for error.
 */
/*
static int zoomSurface2X_32bit(SDL_Surface *src, SDL_Surface *dst)
{
	Uint32 dataSrc;
	Uint32 dataDst;
	Uint8 *pixelSrc = (Uint8*)src->pixels;
	Uint8 *pixelDstRow = (Uint8*)dst->pixels;
	int sx, sy;
	static bool proclaimed = false;

	if (!proclaimed)
	{
		proclaimed = true;
		Log(LOG_INFO) << "Using 32-bit 2X zoom routine.";
	}


	for (sy = 0; sy < src->h; ++sy, pixelDstRow += dst->pitch*2)
	{
		Uint32 *pixelDst = (Uint32*)pixelDstRow;
		Uint32 *pixelDst2 = (Uint32*)(pixelDstRow + dst->pitch);
		for (sx = 0; sx < src->w; sx += 4, pixelSrc += 4)
		{
			dataSrc = *((Uint32*) pixelSrc);

			// boo
			dataSrc = SDL_SwapLE32(dataSrc);

			dataDst = SDL_SwapLE32( (dataSrc & 0xFF) | ((dataSrc & 0xFFFF) << 8) |
				((dataSrc & 0xFF00) << 16)  );

			*pixelDst = dataDst;
			*pixelDst2 = dataDst;
			pixelDst++; // forward 4 bytes!
			pixelDst2++;

			dataSrc >>= 16;

			dataDst = SDL_SwapLE32( (dataSrc & 0xFF) | ((dataSrc & 0xFFFF) << 8) |
				((dataSrc & 0xFF00) << 16)  );

			*pixelDst = dataDst;
			*pixelDst2 = dataDst;
			pixelDst++; // forward 4 bytes!
			pixelDst2++;
		}

	}

	return 0;
}
 */
#endif

/**
 * Optimized 8-bit zoomer for resizing by a factor of 4. Doesn't flip.
 * Used internally by _zoomSurfaceY() below.
 * source and dest. widths must be multiples of 8 bytes for 64-bit access
 *
 * @param src The surface to zoom (input).
 * @param dst The zoomed surface (output).
 * @return 0 for success or -1 for error.
 */
/*
static int zoomSurface4X_64bit(SDL_Surface *src, SDL_Surface *dst)
{
	Uint64 dataSrc;
	Uint64 dataDst;
	Uint8 *pixelSrc = (Uint8*)src->pixels;
	Uint8 *pixelDstRow = (Uint8*)dst->pixels;
	int sx, sy;
	static bool proclaimed = false;

	if (!proclaimed)
	{
		proclaimed = true;
		Log(LOG_INFO) << "Using modestly fast 4X zoom routine.";
	}

	for (sy = 0; sy < src->h; ++sy, pixelDstRow += dst->pitch*4)
	{
		Uint8 *pixelDst = pixelDstRow;

		for (sx = 0; sx < src->w; sx += 8, pixelSrc += 8)
		{
			dataSrc = *((Uint64*) pixelSrc);
			// boo
			(void)SDL_SwapLE64(dataSrc);
 */
			/* expanded form of data shift:
			dataDst = (dataSrc & 0xFF) | ((dataSrc & 0xFF) << 8) |
				((dataSrc & 0xFF) << 16 | ((datasrc & 0xFF) << 24) |
				((dataSrc & 0xFF00 ) << 24) | ((dataSrc & 0xFF00) << 32)  |
				((dataSrc & 0xFF00 ) << 40) | ((dataSrc & 0xFF00) << 48) ;
				 */
/*
			for (int i = 0; i < 4; ++i)
			{
				// compact form, combining terms with equal multipliers (shifts)
				dataDst = (dataSrc & 0xFF) | ((dataSrc & 0xFF) << 8) |
					((dataSrc & 0xFF) << 16) |
					((dataSrc & 0xFFFF ) << 24) | ((dataSrc & 0xFF00) << 32)  |
					((dataSrc & 0xFF00 ) << 40) | ((dataSrc & 0xFF00) << 48) ;

				*((Uint64*)pixelDst) = dataDst;
				*((Uint64*)(pixelDst + dst->pitch)) = dataDst;
				*((Uint64*)(pixelDst + dst->pitch*2)) = dataDst;
				*((Uint64*)(pixelDst + dst->pitch*3)) = dataDst;
				pixelDst+=8; // forward 8 bytes!
				dataSrc >>= 16;
			}
		}
	}

	return 0;
}
 */


#if defined(__WORDSIZE) && (__WORDSIZE == 64) || defined(SIZE_MAX) && (SIZE_MAX > 0xFFFFFFFF)
#else
/**
 * Optimized 8-bit zoomer for resizing by a factor of 4. Doesn't flip.
 * 32-bit version.
 * Used internally by _zoomSurfaceY() below.
 * source and dest. widths must be multiples of 4 bytes for 32-bit access
 *
 * @param src The surface to zoom (input).
 * @param dst The zoomed surface (output).
 * @return 0 for success or -1 for error.
 */
/*
static int zoomSurface4X_32bit(SDL_Surface *src, SDL_Surface *dst)
{
	Uint32 dataSrc;
	Uint32 dataDst;
	Uint8 *pixelSrc = (Uint8*)src->pixels;
	Uint8 *pixelDstRow = (Uint8*)dst->pixels;
	int sx, sy;
	static bool proclaimed = false;

	if (!proclaimed)
	{
		proclaimed = true;
		Log(LOG_INFO) << "Using 32-bit 4X zoom routine.";
	}

	for (sy = 0; sy < src->h; ++sy, pixelDstRow += dst->pitch*4)
	{
		Uint32 *pixelDst = (Uint32*)pixelDstRow;
		Uint32 *pixelDst2 = (Uint32*)(pixelDstRow + dst->pitch);
		Uint32 *pixelDst3 = (Uint32*)(pixelDstRow + 2*dst->pitch);
		Uint32 *pixelDst4 = (Uint32*)(pixelDstRow + 3*dst->pitch);
		for (sx = 0; sx < src->w; sx += 4, pixelSrc += 4)
		{
			dataSrc = *((Uint32*) pixelSrc);
			// boo
			dataSrc = SDL_SwapLE32(dataSrc);

			for (int i = 0; i < 4; ++i)
			{
				dataDst = SDL_SwapLE32( (dataSrc & 0xFF) | ((dataSrc & 0xFF) << 8) |
					((dataSrc & 0xFF) << 16) | ((dataSrc & 0xFF ) << 24) );

				*pixelDst = dataDst;
				*pixelDst2 = dataDst;
				*pixelDst3 = dataDst;
				*pixelDst4 = dataDst;
				pixelDst++; // forward 4 bytes!
				pixelDst2++;
				pixelDst3++;
				pixelDst4++;
				dataSrc >>= 8;
			}
		}
	}

	return 0;
}
 */
#endif

/**
 * Optimized 8-bit zoomer for resizing by a factor of 4. Doesn't flip.
 * 32-bit version.
 * Used internally by _zoomSurfaceY() below.
 * source and dest. widths must be multiples of 4 bytes for 32-bit access
 *
 * @param src The surface to zoom (input).
 * @param dst The zoomed surface (output).
 * @return 0 for success or -1 for error.
 */
/*
static int zoomSurface2X_XAxis_32bit(SDL_Surface *src, SDL_Surface *dst)
{
	Uint32 dataSrc;
	Uint32 dataDst;
	Uint8 *pixelSrc;
	Uint8 *pixelDstRow = (Uint8*)dst->pixels;
	Uint8 *pixelSrcRow = (Uint8*)src->pixels;
	int sx;
	int dsty;
	static bool proclaimed = false;

	static Uint32 *say = 0;
	Uint32 *csay;
	int csy;

	if (!proclaimed)
	{
		proclaimed = true;
		Log(LOG_INFO) << "Using mediocre scaling routine due to screen height.";
	}

	if ((say = (Uint32 *) realloc(say, (dst->h + 1) * sizeof(Uint32))) == NULL) {
		say = 0;
		return (-1);
	}

	csy = 0;
	csay = say;
	for (int y = 0; y < dst->h; y++) {
		csy += src->h;
		*csay = 0;
		while (csy >= dst->h) {
			csy -= dst->h;
			(*csay)++;
		}
		(*csay) *= src->pitch;
		csay++;
	}

	for (dsty = 0; dsty < dst->h; ++dsty, pixelDstRow += dst->pitch)
	{
		if (!say[dsty]) continue;

		Uint32 *pixelDst = (Uint32*)pixelDstRow;
		pixelSrc = pixelSrcRow;
		pixelSrcRow += say[dsty];

		for (sx = 0; sx < src->w; sx += 4, pixelSrc += 4)
		{
			dataSrc = *((Uint32*) pixelSrc);
			// boo
			dataSrc = SDL_SwapLE32(dataSrc);

			for (int i = 0; i < 2; ++i)
			{
				dataDst = SDL_SwapLE32( (dataSrc & 0xFF) | ((dataSrc & 0xFFFF) << 8) |
					((dataSrc & 0xFF00) << 16) );

				int j = 0;
				do
				{
					if (dsty + j >= dst->h) break;

					*(pixelDst + (dst->pitch/sizeof(Uint32))*j) = dataDst;
				} while (say[dsty + ++j] == 0); // fill in all relevant rows

				dataSrc >>= 16;
				pixelDst++; // forward 4 bytes!
			}
		}
	}

	return 0;
}
 */


/**
 * Optimized 8-bit zoomer for resizing by a factor of 2. Doesn't flip.
 * 32-bit version.
 * Used internally by _zoomSurfaceY() below.
 * source and dest. widths must be multiples of 4 bytes for 32-bit access
 *
 * @param src The surface to zoom (input).
 * @param dst The zoomed surface (output).
 * @return 0 for success or -1 for error.
 */
/*
static int zoomSurface4X_XAxis_32bit(SDL_Surface *src, SDL_Surface *dst)
{
	Uint32 dataSrc;
	Uint32 dataDst;
	Uint8 *pixelSrc;
	Uint8 *pixelDstRow = (Uint8*)dst->pixels;
	Uint8 *pixelSrcRow = (Uint8*)src->pixels;
	int sx;
	int dsty;
	static bool proclaimed = false;

	static Uint32 *say = 0;
	Uint32 *csay;
	int csy;

	if (!proclaimed)
	{
		proclaimed = true;
		Log(LOG_INFO) << "Using mediocre scaling routine due to screen height.";
	}

	if ((say = (Uint32 *) realloc(say, (dst->h + 1) * sizeof(Uint32))) == NULL) {
		say = 0;
		return (-1);
	}

	csy = 0;
	csay = say;
	for (int y = 0; y < dst->h; y++) {
		csy += src->h;
		*csay = 0;
		while (csy >= dst->h) {
			csy -= dst->h;
			(*csay)++;
		}
		(*csay) *= src->pitch;
		csay++;
	}

	for (dsty = 0; dsty < dst->h; ++dsty, pixelDstRow += dst->pitch)
	{
		if (!say[dsty]) continue;

		Uint32 *pixelDst = (Uint32*)pixelDstRow;
		pixelSrc = pixelSrcRow;
		pixelSrcRow += say[dsty];

		for (sx = 0; sx < src->w; sx += 4, pixelSrc += 4)
		{
			dataSrc = *((Uint32*) pixelSrc);
			// boo
			dataSrc = SDL_SwapLE32(dataSrc);

			for (int i = 0; i < 4; ++i)
			{
				dataDst = SDL_SwapLE32( (dataSrc & 0xFF) | ((dataSrc & 0xFF) << 8) |
					((dataSrc & 0xFF) << 16) | ((dataSrc & 0xFF ) << 24) );

				int j = 0;
				do
				{
					if (dsty + j >= dst->h) break;

					*(pixelDst + (dst->pitch/sizeof(Uint32))*j) = dataDst;
				} while (say[dsty + ++j] == 0); // fill in all relevant rows

				dataSrc >>= 8;
				pixelDst++; // forward 4 bytes!
			}
		}
	}

	return 0;
}
 */

#ifdef __SSE2__
/**
 * Optimized 8-bit zoomer for resizing by a factor of 4. Doesn't flip.
 * Used internally by _zoomSurfaceY() below.
 * This is an SSE2 version written with Intel intrinsics.
 * source and dest. widths must be multiples of 16 bytes for 128-bit access
 * and it would help if they were aligned properly... :(
 *
 * @param src The surface to zoom (input).
 * @param dst The zoomed surface (output).
 * @return 0 for success or -1 for error.
 */
/*
static int zoomSurface4X_SSE2(SDL_Surface *src, SDL_Surface *dst)
{
	__m128i dataSrc;
	__m128i dataDst;
	Uint8 *pixelSrc = (Uint8*)src->pixels;
	Uint8 *pixelDstRow = (Uint8*)dst->pixels;
	int sx, sy;
	static bool proclaimed = false;

	if (!proclaimed)
	{
		proclaimed = true;
		Log(LOG_INFO) << "Using SSE2 4X zoom routine.";
	}

	for (sy = 0; sy < src->h; ++sy, pixelDstRow += dst->pitch*4)
	{
		__m128i *pixelDst =  (__m128i*)pixelDstRow;
		__m128i *pixelDst2 = (__m128i*)((Uint8*)pixelDstRow + dst->pitch);
		__m128i *pixelDst3 = (__m128i*)((Uint8*)pixelDstRow + dst->pitch*2);
		__m128i *pixelDst4 = (__m128i*)((Uint8*)pixelDstRow + dst->pitch*3);
		for (sx = 0; sx < src->w; sx += 16, pixelSrc += 16)
		{
			dataSrc = *((__m128i*) pixelSrc);

			__m128i halfDone = _mm_unpacklo_epi8(dataSrc, dataSrc);
			dataDst = _mm_unpacklo_epi8(halfDone, halfDone);
 */
/* #define WRITE_DST if ((char*)pixelDst4 + 128 > (char*)dst->pixels+(dst->w*dst->pitch)) { Log(LOG_ERROR) << "HELL"; exit(0); } \ */
#define WRITE_DST			*(pixelDst++) = dataDst; \
			*(pixelDst2++) = dataDst; \
			*(pixelDst3++) = dataDst; \
			*(pixelDst4++) = dataDst; \

/*
			WRITE_DST;

			dataDst = _mm_unpackhi_epi8(halfDone, halfDone);

			WRITE_DST;

			halfDone = _mm_unpackhi_epi8(dataSrc, dataSrc);
			dataDst = _mm_unpacklo_epi8(halfDone, halfDone);

			WRITE_DST;

			dataDst = _mm_unpackhi_epi8(halfDone, halfDone);

			WRITE_DST;
		}
	}

	return 0;
}
 */

/**
 * Optimized 8-bit zoomer for resizing by a factor of 2. Doesn't flip.
 * Used internally by _zoomSurfaceY() below.
 * This is an SSE2 version written with Intel intrinsics.
 * source and dest. widths must be multiples of 16 bytes for 128-bit access
 * and it would help if they were aligned properly... :(
 *
 * @param src The surface to zoom (input).
 * @param dst The zoomed surface (output).
 * @return 0 for success or -1 for error.
 */
/*
static int zoomSurface2X_SSE2(SDL_Surface *src, SDL_Surface *dst)
{
	__m128i dataSrc;
	__m128i dataDst;
	Uint8 *pixelSrc = (Uint8*)src->pixels;
	Uint8 *pixelDstRow = (Uint8*)dst->pixels;
	int sx, sy;
	static bool proclaimed = false;

	if (!proclaimed)
	{
		proclaimed = true;
		Log(LOG_INFO) << "Using SSE2 2X zoom routine.";
	}

	for (sy = 0; sy < src->h; ++sy, pixelDstRow += dst->pitch*2)
	{
		__m128i *pixelDst =  (__m128i*)pixelDstRow;
		__m128i *pixelDst2 = (__m128i*)((Uint8*)pixelDstRow + dst->pitch);

		for (sx = 0; sx < src->w; sx += 16, pixelSrc += 16)
		{
			dataSrc = *((__m128i*) pixelSrc);

			dataDst = _mm_unpacklo_epi8(dataSrc, dataSrc);

#undef WRITE_DST
#define WRITE_DST			*(pixelDst++) = dataDst; \
			*(pixelDst2++) = dataDst; \

			WRITE_DST;

			dataDst = _mm_unpackhi_epi8(dataSrc, dataSrc);

			WRITE_DST;
		}
	}

	return 0;
}
 */

/**
 * Checks the SSE2 feature bit returned by the CPUID instruction
 * @return Does the CPU support SSE2?
 */
bool Zoom::haveSSE2()
{
#ifdef __GNUC__
	unsigned int CPUInfo[4] = {0, 0, 0, 0};
	#if (__e2k__) // e2k - MCST Elbrus 2000 architecture
		#ifdef __SSE2__
			CPUInfo[3] = 0x04000000;
		#endif
	#else // i386/x86_64
		__get_cpuid(1, CPUInfo, CPUInfo+1, CPUInfo+2, CPUInfo+3);
	#endif
#elif _WIN32
	int CPUInfo[4];
	__cpuid(CPUInfo, 1);
#else
	unsigned int CPUInfo[4] = {0, 0, 0, 0};
#endif

	return (CPUInfo[3] & 0x04000000) ? true : false;
}

#endif

/**
 * Wrapper around various software and OpenGL screen buffer pushing functions which zoom.
 * Basically called just from Screen::flip()
 *
 * @param src The surface to zoom (input).
 * @param dst The zoomed surface (output).
 * @param topBlackBand Size of top black band in pixels (letterboxing).
 * @param bottomBlackBand Size of bottom black band in pixels (letterboxing).
 * @param leftBlackBand Size of left black band in pixels (letterboxing).
 * @param rightBlackBand Size of right black band in pixels (letterboxing).
 * @param glOut OpenGL output.
 */
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
void Zoom::flipWithZoom(SDL_Surface *src, SDL_Surface *dst, int topBlackBand, int bottomBlackBand, int leftBlackBand, int rightBlackBand, OpenGL *glOut, int *snapshotCopyResult)
#else
void Zoom::flipWithZoom(SDL_Surface *src, SDL_Surface *dst, int topBlackBand, int bottomBlackBand, int leftBlackBand, int rightBlackBand, OpenGL *glOut)
#endif
{
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
    Workspace workspace;
    const RenderConfig config = captureRenderConfig();
    GlSnapshot gl;
    if (config.openGL) captureGl(glOut, gl);
    copy(src, dst, true, topBlackBand, bottomBlackBand, leftBlackBand, rightBlackBand, config, gl, workspace,
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
        snapshotCopyResult
#else
        0
#endif
        , false
    );
#else
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
    // -2 means no supported copy result was observed; it is not a paint error.
    if (snapshotCopyResult) *snapshotCopyResult = -2;
#endif
	int dstWidth = dst->w - leftBlackBand - rightBlackBand;
	int dstHeight = dst->h - topBlackBand - bottomBlackBand;
	if (Screen::useOpenGL())
	{
#ifndef __NO_OPENGL
		if (glOut->buffer_surface)
		{
			SDL_BlitSurface(src, 0, glOut->buffer_surface->getSurface(), 0); // TODO; this is less than ideal...

			glOut->refresh(glOut->linear, glOut->iwidth, glOut->iheight, dst->w, dst->h, topBlackBand, bottomBlackBand, leftBlackBand, rightBlackBand);
			SDL_GL_SwapBuffers();
		}
#endif
	}
	else if (topBlackBand <= 0 && bottomBlackBand <= 0 && leftBlackBand <= 0 && rightBlackBand <= 0)
	{
#ifdef OPENXCOM_EXPERIMENTAL_SUPERVISED_SNAPSHOT
        const int result = _zoomSurfaceY(src, dst, 0, 0);
        if (snapshotCopyResult) *snapshotCopyResult = result;
#else
		_zoomSurfaceY(src, dst, 0, 0);
#endif
	}
	else if (dstWidth == src->w && dstHeight == src->h)
	{
		SDL_Rect dstrect = {(Sint16)leftBlackBand, (Sint16)topBlackBand, (Uint16)src->w, (Uint16)src->h};
		SDL_BlitSurface(src, NULL, dst, &dstrect);
	}
	else
	{
		SDL_Surface *tmp = SDL_CreateRGBSurface(dst->flags, dstWidth, dstHeight, dst->format->BitsPerPixel, 0, 0, 0, 0);
		_zoomSurfaceY(src, tmp, 0, 0);
		if (src->format->palette != NULL)
		{
			SDL_SetPalette(tmp, SDL_LOGPAL|SDL_PHYSPAL, src->format->palette->colors, 0, src->format->palette->ncolors);
		}
		SDL_Rect dstrect = {(Sint16)leftBlackBand, (Sint16)topBlackBand, (Uint16)tmp->w, (Uint16)tmp->h};
		SDL_BlitSurface(tmp, NULL, dst, &dstrect);
		SDL_FreeSurface(tmp);
	}
#endif
}


/**
 * Internal 8-bit Zoomer without smoothing.
 * Source code originally from SDL_gfx (LGPL) with permission by author.
 *
 * Zooms 8bit palette/Y 'src' surface to 'dst' surface.
 * Assumes src and dst surfaces are of 8-bit depth.
 * Assumes dst surface was allocated with the correct dimensions.
 *
 * @param src The surface to zoom (input).
 * @param dst The zoomed surface (output).
 * @param flipx Flag indicating if the image should be horizontally flipped.
 * @param flipy Flag indicating if the image should be vertically flipped.
 * @return 0 for success or -1 for error.
 */
int Zoom::_zoomSurfaceY(SDL_Surface * src, SDL_Surface * dst, int flipx, int flipy)
{
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
    Workspace workspace;
    return _zoomSurfaceY(src, dst, flipx, flipy, captureRenderConfig(), workspace);
}

int Zoom::_zoomSurfaceY(SDL_Surface *src, SDL_Surface *dst, int flipx, int flipy,
    const RenderConfig &config, Workspace &workspace)
{
    if (!workspace.onOwner() || flipx || flipy || !readableSurface(src) || !readableSurface(dst) ||
        !loadingDisjoint(src, dst) || !loadingScaleSupported(src, dst, config)) return -1;
    // The existing HQX initializer and xBRZ lazy tables have process lifetime.
    // All helper-profile scaler use is confined to this one rendering owner.
    static const std::thread::id rendererOwner = std::this_thread::get_id();
    if (rendererOwner != std::this_thread::get_id()) return -1;
#endif
	int x, y;
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
    Uint32 *&sax = workspace._sax, *&say = workspace._say;
#else
	static Uint32 *sax, *say;
#endif
	Uint32 *csax, *csay;
	int csx, csy;
	Uint8 *sp, *dp, *csp;
	int dgap;
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
    bool &proclaimed = workspace._proclaimed;
    if (config.scaler32)
#else
    static bool proclaimed = false;
    if (Screen::use32bitScaler())
#endif
	{
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
        if (config.xbrz)
#else
		if (Options::useXBRZFilter)
#endif
		{
			// check the resolution to see which scale we need
			for (size_t factor = 2; factor <= 6; factor++)
			{
				if (dst->w == src->w * (int)factor && dst->h == src->h * (int)factor)
				{
					xbrz::scale(factor, (uint32_t*)src->pixels, (uint32_t*)dst->pixels, src->w, src->h, xbrz::RGB);
					return 0;
				}
			}
		}

#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
        if (config.hqx)
#else
        if (Options::useHQXFilter)
#endif
		{
			static bool initDone = false;

			if (!initDone)
			{
				hqxInit();
				initDone = true;
			}

			// HQX_API void HQX_CALLCONV hq2x_32_rb( uint32_t * src, uint32_t src_rowBytes, uint32_t * dest, uint32_t dest_rowBytes, int width, int height );

			if (dst->w == src->w * 2 && dst->h == src->h * 2)
			{
				hq2x_32_rb((uint32_t*)src->pixels, src->pitch, (uint32_t*)dst->pixels, dst->pitch, src->w, src->h);
				return 0;
			}

			if (dst->w == src->w * 3 && dst->h == src->h * 3)
			{
				hq3x_32_rb((uint32_t*)src->pixels, src->pitch, (uint32_t*)dst->pixels, dst->pitch, src->w, src->h);
				return 0;
			}

			if (dst->w == src->w * 4 && dst->h == src->h * 4)
			{
				hq4x_32_rb((uint32_t*)src->pixels, src->pitch, (uint32_t*)dst->pixels, dst->pitch, src->w, src->h);
				return 0;
			}
		}
	}

#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
    if (config.scale)
#else
    if (Options::useScaleFilter)
#endif
	{
		// check the resolution to see which of scale2x, scale3x, etc. we need
		for (size_t factor = 2; factor <= 4; factor++)
		{
			if (dst->w == src->w * (int)factor && dst->h == src->h * (int)factor && !scale_precondition(factor, src->format->BytesPerPixel, src->w, src->h))
			{
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
                if (factor == 4)
                {
                    // The existing scale4x_buf is two cascaded Scale2x passes,
                    // using six rolling intermediate rows. Its public void
                    // scale(4) silently returns on malloc failure. Retain a
                    // checked full intermediate instead and call the same real
                    // Scale2x implementation twice, including its border rule.
                    const unsigned width = static_cast<unsigned>(src->w) * 2;
                    const unsigned height = static_cast<unsigned>(src->h) * 2;
                    const unsigned pitch = width * src->format->BytesPerPixel;
                    const std::size_t bytes = static_cast<std::size_t>(pitch) * height;
                    if (bytes > loadingMaximumSpan) return -1;
                    if (bytes > workspace._scale4Capacity)
                    {
                        void *grown = std::realloc(workspace._scale4, bytes);
                        if (!grown) return -2;
                        workspace._scale4 = grown;
                        workspace._scale4Capacity = bytes;
                    }
                    scale(2, workspace._scale4, pitch, src->pixels, src->pitch,
                        src->format->BytesPerPixel, src->w, src->h);
                    scale(2, dst->pixels, dst->pitch, workspace._scale4, pitch,
                        src->format->BytesPerPixel, width, height);
                    return 0;
                }
#endif
				scale(factor, dst->pixels, dst->pitch, src->pixels, src->pitch, src->format->BytesPerPixel, src->w, src->h);
				return 0;
			}
		}
	}

	// if we're scaling by a factor of 2 or 4, try to use a more efficient function
	/*
	if (src->format->BytesPerPixel == 1 && dst->format->BytesPerPixel == 1)
	{

#ifdef __SSE2__
		static bool _haveSSE2 = haveSSE2();

		if (_haveSSE2 &&
			!((ptrdiff_t)src->pixels % 16) &&
			!((ptrdiff_t)dst->pixels % 16)) // alignment check
		{
			if (dst->w == src->w * 2 && dst->h == src->h * 2) return  zoomSurface2X_SSE2(src, dst);
			else if (dst->w == src->w * 4 && dst->h == src->h * 4) return  zoomSurface4X_SSE2(src, dst);
		} else
		{
			static bool complained = false;

			if (!complained)
			{
				complained = true;
				Log(LOG_ERROR) << "Misaligned surface buffers.";
			}
		}
#endif

// __WORDSIZE is defined on Linux, SIZE_MAX on Windows
#if defined(__WORDSIZE) && (__WORDSIZE == 64) || defined(SIZE_MAX) && (SIZE_MAX > 0xFFFFFFFF)
		if (dst->w == src->w * 2 && dst->h == src->h * 2) return  zoomSurface2X_64bit(src, dst);
		else if (dst->w == src->w * 4 && dst->h == src->h * 4) return  zoomSurface4X_64bit(src, dst);
#else
		if (sizeof(void *) == 8)
		{
			if (dst->w == src->w * 2 && dst->h == src->h * 2) return  zoomSurface2X_64bit(src, dst);
			else if (dst->w == src->w * 4 && dst->h == src->h * 4) return  zoomSurface4X_64bit(src, dst);
		}
		else
		{
			if (dst->w == src->w * 2 && dst->h == src->h * 2) return  zoomSurface2X_32bit(src, dst);
			else if (dst->w == src->w * 4 && dst->h == src->h * 4) return  zoomSurface4X_32bit(src, dst);
		}
#endif

		// maybe X is scaled by 2 or 4 but not Y?
		if (dst->w == src->w * 4) return zoomSurface4X_XAxis_32bit(src, dst);
		else if (dst->w == src->w * 2) return zoomSurface2X_XAxis_32bit(src, dst);
	}
	*/
	if (!proclaimed)
	{
		Log(LOG_INFO) << "Using software scaling routine. For best results, try an OpenGL filter.";
		proclaimed = true;
	}

	/*
	* Allocate memory for row increments
	*/
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
    // Commit each growth only on success. Partial growth retains both owned
    // allocations for reuse and eventual workspace destruction.
    const std::size_t xCount = static_cast<std::size_t>(dst->w) + 1;
    const std::size_t yCount = static_cast<std::size_t>(dst->h) + 1;
    if (xCount > workspace._xCapacity)
    {
        void *grown = std::realloc(sax, xCount * sizeof(Uint32));
        if (!grown) return -2;
        sax = static_cast<Uint32 *>(grown);
        workspace._xCapacity = xCount;
    }
    if (yCount > workspace._yCapacity)
    {
        void *grown = std::realloc(say, yCount * sizeof(Uint32));
        if (!grown) return -2;
        say = static_cast<Uint32 *>(grown);
        workspace._yCapacity = yCount;
    }
#else
    if ((sax = (Uint32 *) realloc(sax, (dst->w + 1) * sizeof(Uint32))) == NULL) {
		sax = 0;
		return (-1);
	}
	if ((say = (Uint32 *) realloc(say, (dst->h + 1) * sizeof(Uint32))) == NULL) {
		say = 0;
		//free(sax);
		return (-1);
	}
#endif

	/*
	* Pointer setup
	*/
	sp = csp = (Uint8 *) src->pixels;
	dp = (Uint8 *) dst->pixels;
	dgap = dst->pitch - dst->w;

	if (flipx) csp += (src->w-1);
	if (flipy) csp  = ( (Uint8*)csp + src->pitch*(src->h-1) );

	/*
	* Precalculate row increments
	*/
	csx = 0;
	csax = sax;
	for (x = 0; x < dst->w; x++) {
		csx += src->w;
		*csax = 0;
		while (csx >= dst->w) {
			csx -= dst->w;
			(*csax)++;
		}
		(*csax) *= (flipx ? -1 : 1);
		csax++;
	}
	csy = 0;
	csay = say;
	for (y = 0; y < dst->h; y++) {
		csy += src->h;
		*csay = 0;
		while (csy >= dst->h) {
			csy -= dst->h;
			(*csay)++;
		}
		(*csay) *= src->pitch * (flipy ? -1 : 1);
		csay++;
	}
	/*
	* Draw
	*/
	csay = say;
	for (y = 0; y < dst->h; y++) {
		csax = sax;
		sp = csp;
		for (x = 0; x < dst->w; x++) {
			/*
			* Draw
			*/
			*dp = *sp;
			/*
			* Advance source pointers
			*/
			sp += (*csax);
			csax++;
			/*
			* Advance destination pointer
			*/
			dp++;
		}
		/*
		* Advance source pointer (for row)
		*/
		csp += (*csay);
		csay++;

		/*
		* Advance destination pointers
		*/
		dp += dgap;
	}

	/*
	* Never remove temp arrays
	*/
	//free(sax);
	//free(say);

	return 0;
}


}

