<!-- New project documentation: SPDX-License-Identifier: GPL-3.0-or-later. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Experimental read-only HUD slice, 2026-10-10

This is a preparatory, deliberately narrow prototype, not the complete MCP
observer. The owner has now selected [vanilla perceptual parity](PERCEPTION-CONTRACT.md);
detailed field validation and the full-observer gates remain open. This engine
slice has no network listener or gameplay controller. A separate
[historical-only stdio adapter](../../tools/mcp-observer/README.md) can read its
files; it cannot attest current state or authorize gameplay.

## Allowed output and source boundary

Only the four already-displayed selected-player HUD numbers are copied:
`time_units`, `energy`, `health`, `morale`. Their sources are the corresponding
`NumberText::getValue()` widgets in `BattlescapeState::samplePublicHud()`.
The normal game populates them in `updateSoldierInfo()`; the observer does not
recalculate FOV or query live soldier statistics. A selected unit must have
current `FACTION_PLAYER`, not be out, and all four widgets must be visible.
Otherwise the sampled `selected_hud` is null. Identity, name, position, turn,
enemies, terrain, items, inventory, sounds, AI and RNG are absent.

Sampling is at the end of the native Battlescape `think()`, after ordinary
state/timer handling. It requires this to be the top game state, the running
battle timer, completed initialization, player side, no debug mode, completed
panic handling, no pending popup, no queued battle action, no falling flag or
falling-unit list, no projectile and no explosion. A blocked sample preserves
the existing observation without adding a reason, event, counter or timestamp.
These gates are conservative source-based safeguards, not a proof of every
possible game-state transition.

The payload's `public_revision` increments only when the allowed serialized
observation changes. Polling or rendering the same HUD does not increment it.
Sessions use an OS-generated nonce, never the game's RNG, and are recreated
when a Battlescape is recreated, including load. There are no entity IDs.

## Freshness and transport limitations

Every result explicitly says `semantics: last_observation` and
`freshness: unverified`. `sampled: true` means a sample was obtained previously;
it does **not** mean the process is alive or these values are current. During
menus/actions the file retains the last completed stable sample. Normal state
destruction invalidates it; a crash or publication failure can leave old bytes.
Consumers may display a labelled historical sample, but must fail closed for
any decision needing current state. This transport cannot authorize gameplay.
No wall-clock freshness timeout, process counter, hidden busy reason or
heartbeat is exported. Publication timing and I/O overhead have not been
measured for side-channel correlations or simulation noninterference.

The temporary transport is an atomic JSON file, not stage 4's production IPC.
It needs both CMake `EXPERIMENTAL_HUD_OBSERVER=ON` and the explicit environment
variable `OPENXCOM_EXPERIMENTAL_HUD_DIR`. Without the build flag (the default),
observer sources and hooks are not compiled. The directory must already exist,
be owned by the process user, have mode 0700 and not be a symlink. An exclusive
private lock prevents two writers. A fresh exclusive 0600 file is renamed to
`snapshot.json`; pre-existing pending files fail closed. Failures disable
sampling without logging game data. No files are added to game saves.

## Build and test

Use a C++11-capable compiler, CMake and the dependencies required by the project:
SDL 1.2, SDL_mixer, SDL_image, SDL_gfx, yaml-cpp and the selected graphics backend
headers/libraries. Despite the CMake module name `FindSDL2`, this engine's
interface uses SDL 1.2. [Pinned reference build inputs](build-dependencies.json)
provide package versions/checksums; they are not a machine inventory.

Typical configured-system commands:

```sh
# Independent, asset-free unit and structural tests:
sh tests/observer/run.sh
# Full builds; both modes were compiled:
sh scripts/build-observer-prototype.sh build-off OFF
sh scripts/build-observer-prototype.sh build-hud ON
# An alternate dependency prefix is supported through CMake:
CMAKE=/path/to/cmake BUILD_JOBS=4 sh scripts/build-observer-prototype.sh \
  build-hud ON -DCMAKE_PREFIX_PATH=/path/to/dependencies/usr
# Explicit opt-in; use isolated user/config directories and your legal assets:
mkdir -m 700 /path/to/private-observer
OPENXCOM_EXPERIMENTAL_HUD_DIR=/path/to/private-observer \
  build-hud/bin/openxcom -data /path/to/assets \
  -user /path/to/test-user -config /path/to/test-config -master xcom1
```

Use `xcom2` for TFTD with its own user/config directories. Do not commit assets,
saves, private paths or dependency binaries. The build helper records source
input checksums, including untracked source files, and binary/cache provenance.
Keep dependency versions, configure arguments and toolchain beside it.

## Validation and honest remaining gates

- Clean vanilla revision `630130c5c9ac236b9e1d8496005fb23e84e397ca`: full build
  passed twice, including a clean rebuild with identical SHA-256
  `9ad7867d7f555b764630e5d04bd667c544045348b4099c633b3b9cf3f0b80f34`.
  This establishes repeatability for the tested build configuration, not universal
  reproducibility across toolchains. Engine/build source diff to the documentation branch base
  `84c9f5a70625d7b9cb20f825ceb37cb84ef0179e` was empty.
- Experimental branch with observer ON and OFF: full builds passed. The OFF
  executable has no `OpenXcom::Observer` symbols. Different build-directory and
  embedded source paths mean separate binary hashes are not an equivalence proof.
- Vanilla UFO and TFTD: menu, mission entry, quicksave, ordinary kneel and
  quickload passed in isolated profiles. UFO TU 58→54→58; TFTD 55→51→55.
- UFO observer loaded the same vanilla-produced save: HUD 58/62/33/100 matched
  the four emitted numbers; ordinary kneel changed only TU to 54 and revision
  1→2. Actual reload restored 58 TU with a different session and revision 1.
  More than a minute idle left the complete snapshot unchanged.
- TFTD observer loaded its vanilla-produced save: HUD 55/61/38/100 matched;
  ordinary kneel changed TU to 51 and revision 1→2. Actual reload restored 55 TU with a new session and revision 1.
- Audio smoke checks: baseline and HUD binaries for both UFO and TFTD each
  produced nonsilent 48 kHz stereo game audio output (11.39–12.00 seconds per recording). This verifies output, not
  perceptual quality, spatial-cue accuracy or complete event coverage.
- Asset-free unit tests pass: cache idempotence, revision changes, absent-HUD
  field exclusion, session validation/reset, shutdown, file modes, competing
  writers, directory/lock symlinks, lock hardlinks and publication failure.
- Structural allowlist checks pass. AddressSanitizer and UndefinedBehaviorSanitizer
  pass with leak detection disabled; LeakSanitizer was not validated.

The unchanged-HUD loop is **not** a paired-hidden-engine-state test. Full paired
scenarios, temporal side channels, actual RNG/resource noninterference,
Geoscape, enemies, presentation-derived terrain, broad transition coverage,
spatial/audio-cue equivalence and the final game matrix are still pending.
Binary equivalence requires a separately verified source/build match. Stages 0, 3–5 and the controller acceptance gate remain open.

Machine-readable records: [source digest and validation checksums](build-evidence.json),
[official dependency package versions and hashes](build-dependencies.json).
