#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
"""Source tripwires supplement runtime tests; they are not a proof of safety."""
from pathlib import Path
import re

if not __debug__:
    raise SystemExit("FAIL: boundary checks require Python assertions; run without -O/PYTHONOPTIMIZE")

root = Path(__file__).resolve().parents[2]
observer = (root / "src/Observer/MixedAudioObserver.cpp").read_text()
slot = (root / "src/Observer/MixedAudioSlot.h").read_text()
for forbidden in ("Options::", "getMod(", "getSavedGame(", "BattleUnit", "TileEngine",
                  "Mix_HookMusic(", "Mix_OpenAudio(", "Mix_Volume", "Mix_Play", "RNG::"):
    assert forbidden not in observer, forbidden
callback = slot.split("void callback(", 1)[1].split("// Caller must", 1)[0]
for forbidden in ("new ", "delete ", "printf", "send(", "recv(", "SDL_", "Mix_",
                  "Clock::", "malloc", "mutex", "yield", "random", "sleep", "Options"):
    assert forbidden not in callback, forbidden
assert "ownership == ARMED" in callback and "return;" in callback
assert "std::memcpy(_pcm + _offset, stream, take)" in callback
assert "std::memory_order_acquire" in slot and "std::memory_order_release" in slot
assert observer.index("Mix_SetPostMix(NULL, NULL)") < observer.index("_slot.closeAfterDetach()")
assert "!_slot.lockFree()" in observer
assert "peerSize != sizeof(sa_family_t)" in observer
assert "localSize != sizeof(sa_family_t)" in observer
assert "packet.size() < 102400" in observer
assert "rate != 22050 || format != AUDIO_S16LSB || channels != 2" in observer
assert "std::chrono::seconds(30)" in observer
assert "std::chrono::seconds(2)" in observer
assert "std::chrono::seconds(1)" in observer
cmake = (root / "CMakeLists.txt").read_text()
assert re.search(r'option\s*\(\s*EXPERIMENTAL_MIXED_AUDIO_OBSERVER\s+"[^"]*"\s+OFF', cmake)
game = (root / "src/Engine/Game.cpp").read_text()
assert game.index("_mixedAudioObserver.close();") < game.index("Mix_CloseAudio();")
assert "publishCatalogSupported(_mod->isMusicCatalogSoftwareMixed())" in game
assert "_mixedAudioObserver.poll();" in game
start = (root / "src/Menu/StartState.cpp").read_text().split("void StartState::init()", 1)[1]
assert start.index("resetMixedAudioObserver(true)") < start.index("Mix_CloseAudio()")
flc = (root / "src/Engine/FlcPlayer.cpp").read_text()
for name, operation in (("initAudio", "Mix_OpenAudio"), ("deInitAudio", "Mix_CloseAudio")):
    body = flc.split("void FlcPlayer::" + name, 1)[1]
    assert body.index("resetMixedAudioObserver()") < body.index(operation)
profile = (root / "src/Observer/MixedAudioMusicProfile.h").read_text()
assert "case MUS_MID:" not in profile and "case MUS_CMD:" not in profile
mod = (root / "src/Mod/Mod.cpp").read_text().split("bool Mod::isMusicCatalogSoftwareMixed()", 1)[1].split("#endif", 1)[0]
assert "_musics.begin()" in mod and "isSoftwareMixedAudio()" in mod
print("PASS native mixed-audio boundary tripwires")
