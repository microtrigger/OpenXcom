#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# This file is free software: you may redistribute and/or modify it under
# GNU GPL version 3 or, at your option, any later version.
# Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT HUP INT TERM
if [ -n "${SDL_CONFIG:-}" ]; then
  SDL_CFLAGS=$($SDL_CONFIG --cflags)
  SDL_LIBS=$($SDL_CONFIG --libs)
elif [ -n "${SDL_PREFIX:-}" ]; then
  SDL_CFLAGS="-I$SDL_PREFIX/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT"
  SDL_LIBS="-L$SDL_PREFIX/lib -L$SDL_PREFIX/lib/x86_64-linux-gnu -lSDL"
  LD_LIBRARY_PATH="$SDL_PREFIX/lib:$SDL_PREFIX/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
  export LD_LIBRARY_PATH
else
  SDL_CFLAGS=$(sdl-config --cflags)
  SDL_LIBS=$(sdl-config --libs)
fi
# SDL_CONFIG output intentionally undergoes normal flag splitting.
${CXX:-c++} -std=c++11 -Wall -Wextra -Werror -pedantic ${CXXFLAGS:-} $SDL_CFLAGS \
  "$ROOT/tests/cursor-observer/test_presented_cursor.cpp" \
  "$ROOT/src/Observer/PresentedCursor.cpp" \
  "$ROOT/src/Observer/PresentedCursorSurface.cpp" \
  "$ROOT/src/Observer/CursorFileObserver.cpp" \
  $SDL_LIBS -o "$BUILD/test-presented-cursor"
"$BUILD/test-presented-cursor"
python3 "$ROOT/tests/cursor-observer/check_boundary.py"

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror -pedantic ${CXXFLAGS:-} \
  "$ROOT/tests/cursor-observer/test_presentation.cpp" \
  "$ROOT/src/Observer/PresentationObserver.cpp" \
  "$ROOT/src/Observer/PresentedCursor.cpp" -o "$BUILD/test-presentation"
"$BUILD/test-presentation"

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror -pedantic ${CXXFLAGS:-} $SDL_CFLAGS \
  "$ROOT/tests/cursor-observer/test_presented_hud.cpp" \
  "$ROOT/src/Observer/PresentedHud.cpp" \
  "$ROOT/src/Observer/PresentedCursor.cpp" \
  "$ROOT/src/Observer/PresentedCursorSurface.cpp" \
  $SDL_LIBS -o "$BUILD/test-presented-hud"
"$BUILD/test-presented-hud"
