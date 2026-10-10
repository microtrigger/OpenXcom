#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# New contribution maintained in this OpenXcom fork, 2026-10-10.
# This file is free software: you may redistribute and/or modify it under
# GNU GPL version 3 or, at your option, any later version.
# Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
# Reusable build entry point. Dependencies must already be installed/provided.
set -eu
if [ "$#" -lt 1 ]; then
  echo "Usage: $0 BUILD_DIR [OFF|ON] [extra CMake arguments...]" >&2
  exit 2
fi
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=$1; shift
MODE=${1:-OFF}; if [ "$#" -gt 0 ]; then shift; fi
case "$MODE" in ON|OFF) ;; *) echo 'Observer mode must be OFF or ON' >&2; exit 2;; esac
REV=$(git -C "$ROOT" rev-parse --short=12 HEAD)
DIRTY=$(git -C "$ROOT" status --porcelain --untracked-files=normal)
if [ -n "$DIRTY" ]; then REV="$REV-working"; fi
"${CMAKE:-cmake}" -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_HUD_OBSERVER="$MODE" \
  -DOPENXCOM_VERSION_STRING=".$REV" "$@"
"${CMAKE:-cmake}" --build "$BUILD" --parallel "${BUILD_JOBS:-2}"
python3 "$ROOT/scripts/observer-build-provenance.py" "$ROOT" "$BUILD" "$MODE"
