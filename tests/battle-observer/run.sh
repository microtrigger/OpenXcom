#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Original test contribution, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md.
# Requires a graphical SDL 1.2 display and separately installed lawful resources.
# No asset copies, campaign saves, observer registration or external listener.
set -eu
if [ "$#" -ne 3 ]; then
  echo "Usage: $0 /absolute/path/openxcom /absolute/path/data xcom1|xcom2" >&2
  exit 2
fi
BINARY=$1; DATA=$2; MASTER=$3
case "$BINARY:$DATA" in /*:/*) ;; *) echo 'Binary and data paths must be absolute' >&2; exit 2;; esac
case "$MASTER" in xcom1|xcom2) ;; *) echo 'Choose xcom1 or xcom2' >&2; exit 2;; esac
PROFILE=$(mktemp -d)
trap 'rm -rf "$PROFILE"' EXIT HUP INT TERM
mkdir "$PROFILE/user" "$PROFILE/config"
printf 'mods:\n  - id: %s\n    active: true\nlanguage: en-US\nplayIntro: false\n' "$MASTER" > "$PROFILE/config/options.cfg"
# Refuse inherited observer destinations in the native runner; do not overwrite them.
export OPENXCOM_BATTLE_PAIR_GATE=1
"$BINARY" -master "$MASTER" -data "$DATA" -user "$PROFILE/user" -cfg "$PROFILE/config"
