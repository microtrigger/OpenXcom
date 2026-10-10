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
if [ -n "${SDL_PREFIX:-}" ]; then
  SDL_CFLAGS="-I$SDL_PREFIX/include/SDL -I$SDL_PREFIX/include"
else
  SDL_CFLAGS=$(${SDL_CONFIG:-sdl-config} --cflags)
fi
${CXX:-c++} -std=c++11 -Wall -Wextra -Werror -pedantic $SDL_CFLAGS \
  "$ROOT/tests/observer/test_public_hud.cpp" \
  "$ROOT/src/Observer/DiagnosticProfile.cpp" "$ROOT/src/Observer/PublicHud.cpp" "$ROOT/src/Observer/HudFileObserver.cpp" \
  -o "$BUILD/test-public-hud"
"$BUILD/test-public-hud"
python3 "$ROOT/tests/observer/check_boundary.py"
