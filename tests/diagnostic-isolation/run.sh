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
  SDL_CFLAGS="-I$SDL_PREFIX/include/SDL -I$SDL_PREFIX/include"
else
  SDL_CFLAGS=$(${SDL_CONFIG:-sdl-config} --cflags)
fi
${CXX:-c++} -std=c++11 -Wall -Wextra -Werror -pedantic ${CXXFLAGS:-} -UNDEBUG $SDL_CFLAGS \
  "$ROOT/tests/diagnostic-isolation/test_profile.cpp" \
  "$ROOT/src/Observer/DiagnosticProfile.cpp" "$ROOT/src/Observer/PresentationObserver.cpp" \
  "$ROOT/src/Observer/MixedAudioObserver.cpp" "$ROOT/src/Observer/PresentedCursor.cpp" \
  "$ROOT/src/Observer/PublicHud.cpp" "$ROOT/src/Observer/HudFileObserver.cpp" \
  "$ROOT/src/Observer/CursorFileObserver.cpp" -o "$BUILD/test-profile"
for stage in uninitialized startup admission publication; do
  for variant in 0 1 2 3 4 5; do "$BUILD/test-profile" "$stage" "$variant"; done
done
OPENXCOM_DIAGNOSTIC_TEST_CHILD="$BUILD/test-profile" PYTHONDONTWRITEBYTECODE=1 \
  python3 "$ROOT/tools/mcp-observer/test_diagnostic_isolation.py"
python3 "$ROOT/tests/diagnostic-isolation/check_boundary.py"
printf '%s\n' 'PASS: effective diagnostic profile, terminal rejection, historical retirement and isolated Logger fallback'
