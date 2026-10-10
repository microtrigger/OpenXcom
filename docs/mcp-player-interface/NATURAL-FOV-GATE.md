<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Original documentation contribution, 2026-10-10. See LICENSE-NOTICES.md. -->
# Bounded natural-FOV and pause-menu lifecycle gate

This separate, default-OFF fixture requests six actual presentations reached by
ordinary mouse/keyboard input and the ordinary game loop. It tests sampled
presentation accuracy through natural FOV membership changes and a pause-menu
round trip. It does not add observation fields or gameplay control to MCP.
Stage 5 remains open and the controller remains disabled.

## Build and run

With the project's ordinary dependencies already available:

```sh
cmake -S . -B build-natural-fov -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_CURSOR_OBSERVER=ON -DEXPERIMENTAL_NATURAL_FOV_GATE=ON
cmake --build build-natural-fov
python3 tests/battle-observer/check_natural_fov_boundary.py
python3 tests/battle-observer/run-natural-fov.py /absolute/path/openxcom /absolute/path/data xcom1
python3 tests/battle-observer/run-natural-fov.py /absolute/path/openxcom /absolute/path/data xcom2
```

Supply the real built executable and separately obtained lawful game resources.
Use a focused graphical SDL display, supported software single-buffer 8-bit
presentation, 640×400 physical pixels, 320×200 logical pixels, integer scale two
and no bands. The runner creates and removes a disposable profile, loads no
campaign saves, suppresses ordinary engine logs and emits only fixed aggregate
results or fixed source-assertion labels. No assets, binaries, frames, packet
contents or runtime values belong in published results. The native fixture has
a 20-second monotonic lifecycle limit; the wrapper has a 45-second process limit.

The gate requires the cursor observer. It cannot share a binary with the render
pair, request-effects or legacy HUD-observer options. With the new option OFF,
all its seams and fixture code disappear. No production observer, Screen,
NumberText, FOV or visibility implementation changes are required.

## Setup and six samples

After normal resource/language loading, a fresh 40×40×1 land battle uses the
real, patched static BLANKS object-1 walkable floor. A real Base/Soldier with
normal armor occupies (20,20,0), initially facing direction 0. A healthy,
unarmed, stationary Sectoid/Aquatoid occupies (22,20,0). Normal occupancy,
lighting and FOV initialization run once at setup. No visibility, direction,
position, TU or FOV setters are used by the fixture after setup.

| Sample | Ordinary input | Direction | Player current FOV membership | Hostile retained visibility | TU cost |
|---|---|---:|---|---|---:|
| 1 | Hover hostile tile | 0 | absent | false | 0 |
| 2 | Right-click (22,18,0), hover hostile | 1 | present | true | 1 |
| 3 | Right-click (20,18,0), hover hostile | 0 | absent | true | 2 |
| 4 | Right-click (22,18,0), hover hostile | 1 | present | true | 3 |
| 5 | Battle-options key | 1 | present | true | 3 |
| 6 | Cancel key, hover hostile | 1 | present | true | 3 |

Mouse positions come from Camera projection and normal Map hit testing; the
fixture checks the real selector, unchanged camera, ordinary cursor and exact
input sequence. It pins right-click away from drag-scroll and strafe behavior.
Input passes through SDL_PushEvent and Game dispatch, with real focus/quit
signals permitted and unrelated input ignored. No direct action-handler call,
forced animation, timer acceleration, frame scheduling override or AI/end-turn
execution is used. Each phase waits at least 250 real milliseconds; battle
samples additionally witness ordinary think, animation and state-timer callbacks.
After Cancel, a second normal Battlescape initialization and fresh timer
callbacks must occur. The active pause state and lack of battle callbacks are
checked while the menu is open. Audio uses an initialized, muted SDL dummy mixer.
The runner and test-only preparation select the dummy driver before startup;
each sample checks the driver, initialized mixer, mute flag and absence of active
channels/music. No physical audio output or audio-equivalence result is claimed.
Ordinary popup callbacks remain intact. An initial normal run and two diagnostic
attempts with the old closed-mixer profile failed: vanilla Window::popup calls
Mix_GroupAvailable before Sound::play's mute guard. Final evidence uses the
initialized-dummy profile and new binary hashes, with no production audio or
Window changes.

Turning goes through Map click, secondaryAction and UnitTurnBState. The ordinary
turn charges one TU and recalculates FOV per step. The first-contact abort still
allows this one-step turn. TileEngine clears the viewer's current list but does
not clear a stationary enemy's remembered visibility when the player turns
away. The retained sprite/cursor cue is vanilla behavior and is preserved.
Visibility can be reset by other normal paths, including end-turn and nonplayer
walking; those are outside this fixture. The tested overlay is PauseState,
not the hidden-movement enemy-turn overlay.

## Independent displayed-pixel oracle

Immediately after the real Screen::flip returns, the test reads
SDL_GetVideoSurface, validates its physical dimensions/format and resolves the
entire current palette-indexed display into private RGB memory. It does not
read the logical Screen surface or reuse production capture/serialization.

Crop bounds are independently calculated from the actual cursor times two,
using literal min/max limits around 64 pixels in each direction. Each returned
bound and every decoded RGB byte must match rectangle indexing of the final
physical RGB image. The initial and first reveal samples must differ, but this
is only a presentation-change witness: orientation, discovery and cursor
animation may contribute. The sampled pause crop must also differ from the
preceding battle crop and match the currently displayed menu presentation.

For battle samples, all four HUD values must be present. The oracle independently
resolves widget constructor defaults plus State::add interface-rule geometry
and color overrides. It then matches final RGB ink against exhaustive templates
for 0–999. Glyph row masks are adapted directly from upstream NumberText
(Copyright 2010–2016 OpenXcom Developers; GPL-3.0-or-later), and a structural
check reconstructs its setPixel alphabet to catch drift. The oracle reads no
widget getValue or producer HudFrame and calls neither production HUD decoder.
While PauseState is topmost, the entire HUD must be null; the normal recovered
battle presentation must provide four independently verified values again.

Exact field sets reject added contact/state metadata or duplicate/unknown keys.
Epoch, fresh admissions, request challenges, six-request order and count,
no unsolicited replies, and endpoint closure after normal Game destruction are
checked. Frames, crops, private fixture state and packets remain internal.

## Mutation controls and interpretation

Use three temporary, isolated source mutants, never the frozen normal source:

1. Change a serialized production crop RGB byte: independent pixel comparison
   must fail.
2. Change the production HUD's returned time-units number: comparison against
   displayed glyphs must fail.
3. Substitute a non-null HUD for the production pause response: the overlay
   availability assertion must fail.

The mutation binary may reuse the frozen normal objects except its one mutated
production object, compiled with identical flags and relinked separately.
Mutant source and output stay outside the published file allowlist.

Both normal master profiles passed once, and all three mutants were rejected
once per master by the intended independent assertions. See the
[aggregate evidence](natural-fov-evidence.json) for executed profiles,
source/binary identification and controls. One run per master is six sampled presentations,
not capture of every transient frame, a repeated robustness result, a paired
hidden-world noninterference proof or a timing/audio test. The slice is land-only,
contains no AI/end-turn behavior, and does not establish full fairness or close
Stage 5. Other palettes, scales, overlays, camera movement, unsupported rendering
profiles and general controller operation remain outside this result.
