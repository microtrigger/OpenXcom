// SPDX-License-Identifier: GPL-3.0-or-later
#include "AgentProjectileOutput.h"
#include <cstring>
#include <limits>
namespace OpenXcom { namespace Observer {
bool AgentProjectileOutput::layout(const SDL_Surface *s)
{
    if (!s || !s->format || !s->pixels || s->locked || s->w <= 0 || s->h <= 0 ||
        s->w > 4096 || s->h > 4096 || SDL_MUSTLOCK(s) ||
        s->clip_rect.x || s->clip_rect.y || s->clip_rect.w != s->w || s->clip_rect.h != s->h ||
        (s->flags & SDL_SRCALPHA) || ((s->flags & SDL_SRCCOLORKEY) && s->format->colorkey != 0) ||
        (s->flags & (SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_RLEACCEL | SDL_OPENGL | SDL_DOUBLEBUF))) return false;
    const int bytes = s->format->BytesPerPixel;
    if (!((bytes == 1 && s->format->BitsPerPixel == 8) || (bytes == 4 && s->format->BitsPerPixel == 32)) ||
        s->pitch < s->w * bytes || s->pitch % bytes) return false;
    if (bytes == 1 && (!s->format->palette || s->format->palette->ncolors != 256 || !s->format->palette->colors)) return false;
    if (bytes == 4 && (!s->format->Rmask || !s->format->Gmask || !s->format->Bmask ||
        (s->format->Rmask & s->format->Gmask) || (s->format->Rmask & s->format->Bmask) ||
        (s->format->Gmask & s->format->Bmask))) return false;
    if (bytes == 4)
    {
        const SDL_PixelFormat *f = s->format;
        if (f->palette || f->Rshift > 24 || f->Gshift > 24 || f->Bshift > 24 || f->Rloss || f->Gloss || f->Bloss ||
            f->Rmask != (0xffu << f->Rshift) || f->Gmask != (0xffu << f->Gshift) || f->Bmask != (0xffu << f->Bshift) ||
            (f->Amask && (f->Ashift > 24 || f->Aloss || f->Amask != (0xffu << f->Ashift) ||
            (f->Amask & (f->Rmask | f->Gmask | f->Bmask))))) return false;
    }
    const std::size_t span = std::size_t(s->pitch) * s->h;
    return reinterpret_cast<std::uintptr_t>(s->pixels) <= std::numeric_limits<std::uintptr_t>::max() - span;
}
bool AgentProjectileOutput::rgb(const SDL_Surface *s, int x, int y, std::uint32_t &out)
{
    if (x < 0 || y < 0 || x >= s->w || y >= s->h) return false;
    const unsigned char *p = static_cast<const unsigned char *>(s->pixels) + std::size_t(y) * s->pitch + x * s->format->BytesPerPixel;
    Uint8 r, g, b;
    if (s->format->BytesPerPixel == 1)
    { const SDL_Color &c = s->format->palette->colors[*p]; r = c.r; g = c.g; b = c.b; }
    else { Uint32 pixel; std::memcpy(&pixel, p, sizeof(pixel)); SDL_GetRGB(pixel, s->format, &r, &g, &b); }
    out = (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | b; return true;
}
bool AgentProjectileOutput::capture(bool flipped, bool filtered, const SDL_Surface *logical,
    const SDL_Surface *final, const SDL_Surface *map, int top, int bottom, int left, int right)
{
    _logical = _final = nullptr;
    if (!flipped || filtered || !layout(logical) || !layout(final) || !layout(map) ||
        logical->format->BytesPerPixel != 1 || map->format->BytesPerPixel != 1 ||
        logical->w > 640 || logical->h > 400 || map->w > 640 || map->h > 400 ||
        top < 0 || bottom < 0 || left < 0 || right < 0 || top >= final->h || bottom >= final->h - top ||
        left >= final->w || right >= final->w - left) return false;
    const auto disjoint = [](const SDL_Surface *a, const SDL_Surface *b) {
        const auto aa = reinterpret_cast<std::uintptr_t>(a->pixels), bb = reinterpret_cast<std::uintptr_t>(b->pixels);
        return aa + std::size_t(a->pitch) * a->h <= bb || bb + std::size_t(b->pitch) * b->h <= aa;
    };
    if (!disjoint(map, logical) || !disjoint(map, final) || !disjoint(logical, final)) return false;
    const int width = final->w - left - right, height = final->h - top - bottom;
    if (width % logical->w || height % logical->h) return false;
    _scaleX = width / logical->w; _scaleY = height / logical->h;
    if (_scaleX < 1 || _scaleX > 8 || _scaleY < 1 || _scaleY > 8) return false;
    for (int i = 0; i < 256; ++i)
    {
        const SDL_Color &m = map->format->palette->colors[i], &l = logical->format->palette->colors[i];
        if (m.r != l.r || m.g != l.g || m.b != l.b) return false;
        palette[i] = (std::uint32_t(m.r) << 16) | (std::uint32_t(m.g) << 8) | m.b;
    }
    _logical = logical; _final = final; _left = left; _top = top; return true;
}
bool AgentProjectileOutput::matches(int x, int y, std::uint32_t expected) const
{
    if (!_logical || !_final || expected > 0xffffff) return false;
    std::uint32_t actual;
    if (!rgb(_logical, x, y, actual) || actual != expected) return false;
    for (int yy = 0; yy < _scaleY; ++yy)
        for (int xx = 0; xx < _scaleX; ++xx)
            if (!rgb(_final, _left + x * _scaleX + xx, _top + y * _scaleY + yy, actual) || actual != expected) return false;
    return true;
}
} }
