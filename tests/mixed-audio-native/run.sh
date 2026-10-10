#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# Free software under GNU GPL version 3 or later; without warranty.
# See repository LICENSE.txt and LICENSE-NOTICES.md.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT HUP INT TERM
if [ -n "${SDL_PREFIX:-}" ]; then
  SDL_CFLAGS="-I$SDL_PREFIX/include/SDL -I$SDL_PREFIX/include -D_GNU_SOURCE=1 -D_REENTRANT"
  SDL_LIBS="-L$SDL_PREFIX/lib -L$SDL_PREFIX/lib/x86_64-linux-gnu -lSDL"
  LD_LIBRARY_PATH="$SDL_PREFIX/lib:$SDL_PREFIX/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
  export LD_LIBRARY_PATH
else
  SDL_CFLAGS=$(${SDL_CONFIG:-sdl-config} --cflags)
  SDL_LIBS=$(${SDL_CONFIG:-sdl-config} --libs)
fi
FLAGS="-std=c++11 -O2 -Wall -Wextra -Werror -pedantic -pthread ${CXXFLAGS:-} -UNDEBUG"
${CXX:-c++} $FLAGS "$ROOT/tests/mixed-audio-native/test_slot.cpp" -o "$BUILD/test-slot"
"$BUILD/test-slot"
${CXX:-c++} $FLAGS $SDL_CFLAGS "$ROOT/tests/mixed-audio-native/test_protocol.cpp" \
  "$ROOT/src/Observer/MixedAudioObserver.cpp" "$ROOT/src/Observer/DiagnosticProfile.cpp" -o "$BUILD/test-protocol"
"$BUILD/test-protocol" --startup
# ELF link wrapping is only an independent synthetic oracle, never a game hook.
${CXX:-c++} $FLAGS $SDL_CFLAGS -ffunction-sections -fdata-sections \
  "$ROOT/tests/mixed-audio-native/test_dummy_mixer.cpp" \
  "$ROOT/src/Observer/MixedAudioObserver.cpp" "$ROOT/src/Observer/DiagnosticProfile.cpp" "$ROOT/src/Engine/Sound.cpp" \
  -Wl,--gc-sections -Wl,--wrap=Mix_SetPostMix -lSDL_mixer $SDL_LIBS -o "$BUILD/test-dummy-mixer"
SDL_AUDIODRIVER=dummy "$BUILD/test-dummy-mixer"
python3 "$ROOT/tests/mixed-audio-native/check_boundary.py"
