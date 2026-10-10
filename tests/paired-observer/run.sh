#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
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
# GNU/ELF section GC omits unused asset loading routines and their dependencies.
# Production Surface is compiled intact; no engine methods are stubbed/replaced.
build() {
  ${CXX:-c++} -std=c++11 -Wall -Wextra -Werror -pedantic ${CXXFLAGS:-} \
    -UNDEBUG -DOPENXCOM_EXPERIMENTAL_CURSOR_OBSERVER -ffunction-sections -fdata-sections \
    $SDL_CFLAGS -I"$ROOT/src/Observer" \
    "$ROOT/tests/paired-observer/native_fixture.cpp" \
    "$ROOT/src/Interface/NumberText.cpp" "$ROOT/src/Engine/Surface.cpp" \
    "$2" "$3" "$ROOT/src/Observer/PresentedCursor.cpp" \
    "$ROOT/src/Observer/PresentedCursorSurface.cpp" \
    -Wl,--gc-sections $SDL_LIBS -o "$1"
}
build "$BUILD/native" "$ROOT/src/Observer/PresentedHud.cpp" "$ROOT/src/Observer/PresentationObserver.cpp"
PYTHONOPTIMIZE=0 PYTHONDONTWRITEBYTECODE=1 python3 "$ROOT/tests/paired-observer/test_pairs.py" "$BUILD/native"

# Positive controls mutate temporary copies only, then run the same real gate.
PYTHONOPTIMIZE=0 python3 - "$ROOT" "$BUILD" <<'PY'
from pathlib import Path
import sys
root, build = map(Path, sys.argv[1:])
hud = (root / 'src/Observer/PresentedHud.cpp').read_text()
needle = '    std::ostringstream out; out.imbue(std::locale::classic());'
assert hud.count(needle) == 1
hud = hud.replace('#include "PresentedHud.h"', '#include "PresentedHud.h"\nextern unsigned int pairedSourceValue;')
hud = hud.replace(needle, '    values[0] = ::pairedSourceValue; // intentional test-only source leak\n' + needle)
(build / 'leaky-hud.cpp').write_text(hud)
producer = (root / 'src/Observer/PresentationObserver.cpp').read_text()
needle = ' || request.substr(32, 32) != _admission'
assert producer.count(needle) == 1
(build / 'replay-producer.cpp').write_text(producer.replace(needle, ''))
PY
build "$BUILD/leaky" "$BUILD/leaky-hud.cpp" "$ROOT/src/Observer/PresentationObserver.cpp"
PYTHONOPTIMIZE=0 PYTHONDONTWRITEBYTECODE=1 python3 "$ROOT/tests/paired-observer/test_pairs.py" "$BUILD/leaky" --expect-source-leak
build "$BUILD/replay" "$ROOT/src/Observer/PresentedHud.cpp" "$BUILD/replay-producer.cpp"
PYTHONOPTIMIZE=0 PYTHONDONTWRITEBYTECODE=1 python3 "$ROOT/tests/paired-observer/test_pairs.py" "$BUILD/replay" --expect-replay-leak
