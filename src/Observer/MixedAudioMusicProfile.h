// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// Free software under GNU GPL version 3 or later; without warranty.
// See repository LICENSE.txt and LICENSE-NOTICES.md.
#pragma once
#include <SDL_mixer.h>
namespace OpenXcom { namespace Observer {
inline bool softwareMixedMusicType(Mix_MusicType type)
{
    // Audited SDL_mixer 1.2.12 software decoder paths only.
    switch (type)
    {
    case MUS_WAV: case MUS_MOD: case MUS_OGG: case MUS_MP3:
    case MUS_MP3_MAD: case MUS_FLAC: case MUS_MODPLUG:
        return true;
    default: // Includes MUS_NONE, MUS_MID (may be native), MUS_CMD and unknown.
        return false;
    }
}
} }
