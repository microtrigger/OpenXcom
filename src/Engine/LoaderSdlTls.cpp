// Copyright 2026 OpenXcom contributors. GPL-3.0-or-later; see LICENSE.txt and LICENSE-NOTICES.md.
#include "LoaderThreadLinux.h"
// This translation unit alone selects SDL2 headers. Never put its include
// directory on the SDL1 application or reinterpret an SDL1 function type.
#include <SDL_thread.h>
#include <SDL_version.h>

#if SDL_MAJOR_VERSION != 2 || !SDL_VERSION_ATLEAST(2, 0, 16)
#error "Loader TLS cleanup requires the public SDL2 >= 2.0.16 interface"
#endif

extern "C" void OpenXcom_LoaderSdlTlsCleanup() noexcept
{
    SDL_TLSCleanup();
}
