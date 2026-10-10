<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Original documentation contribution, 2026-10-10. See LICENSE-NOTICES.md. -->
# Bounded observer-request functional noninterference gate

This separate, default-OFF test compares two fresh tactical worlds in one
observer-enabled executable. World A has a connected, idle `/2` observer and
sends no requests. World B sends exactly one valid request at each of three
checkpoints. The test checks same-binary **per-request functional effects**;
it does not compare initialization effects or observer-OFF versus observer-ON
binaries. It is not a full fairness, timing, audio, or libc RNG proof.

## Build and run

Build with the ordinary project prerequisites and:

```sh
cmake -S . -B build-effects -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_CURSOR_OBSERVER=ON -DEXPERIMENTAL_OBSERVER_EFFECTS_GATE=ON
cmake --build build-effects
python3 tests/battle-observer/check_effects_boundary.py
python3 tests/battle-observer/run-effects.py /absolute/path/openxcom /absolute/path/data xcom1
python3 tests/battle-observer/run-effects.py /absolute/path/openxcom /absolute/path/data xcom2
```

Use the actual built executable path and separately installed lawful original
resources. A graphical, focused SDL display with the supported software,
single-buffer, 8-bit, integer-scale/no-bands presentation is required. The runner
creates a disposable profile and does not load campaign saves or write snapshots.
It has a 45-second process limit, while each tactical world must complete within
15 monotonic seconds. Missing resources, focus, inputs, timer progress or valid
responses fail closed with a generic result. No external observer destination may
be inherited. This option requires the cursor observer and cannot be combined
with the separate render-pair gate in one build.

The expected result is one fixed `OBSERVER_EFFECTS_GATE PASS` aggregate identifying
the validated active master (`xcom1` or `xcom2`); the runner checks it matches the
requested master. Engine
logs are not forwarded by the runner. No runtime state, hashes, pixel frames,
packets, nonces or hidden values are emitted. Public files contain only source
and documentation, never game assets or binaries.

## Fixture and actual execution

After normal resource and language loading, each fresh `SavedGame` owns a
`SavedBattleGame` with a 40×40×1 land map at depth zero. A real `STR_SOLDIER`
`Soldier` belongs to a real `Base`, uses the rules' normal armor, and becomes the
selected player `BattleUnit`. A healthy, unarmed hostile is parked well away.
Real BLANKS MCD data is loaded with its normal patch. Object 1 must be the
engine's scorched-earth floor: walkable, level zero, static, non-door,
non-grav-lift, nonexplosive. It fills the grid. Normal occupancy rebuilding and
FOV/lighting setup run; visibility is not fabricated. Datasets are reloaded for
world B because battle destruction unloads them. A blank `State` separates
worlds so the previous battlescape is deleted before its save is replaced.

The ordinary `Game::run` loop performs state logic, timers, FPS scheduling,
rendering and flip. The fixture neither suppresses nor forces timers, invokes
`animate` directly, forces frame deadlines, or calls kneeling methods. Only
fixed test input is injected using `SDL_PushEvent`: a map mouse position, then
ordinary kneel shortcut keydown/up, then the same shortcut keydown/up to stand.
Unscripted gameplay input is ignored; window focus and quit events remain real.
This is an isolated test script, not a gameplay MCP controller.

The three checkpoints are standing at initial TU, kneeling at initial TU minus
4, and standing at initial TU minus 12. Each phase requires at least 250 ms of
monotonic elapsed time, an actual `BattlescapeState::think`, an actual normal
animation-timer callback and an actual battle-state-timer callback. The state
must be stable: top battlescape, no queued popup, player turn and selected
identity intact, panic handled, healthy standing-status units with correct
occupancy/backlinks, no falling/busy action/path preview/waypoints/projectile/
explosion. The kneel flag and exact TU costs prove that the ordinary actions
completed. The two runs need not execute the same number of frames or callbacks.

At the next normally scheduled precomposition point, A queues nothing and B
queues one valid request. Captures occur only after normal successful flip and,
for B, validation of a non-null HUD/crop response. The same Screen-lifetime
stream may remain open across worlds: hello, epoch, request challenge and fresh
admission-token continuity are checked, with no extra packets or closure.

## Internal comparison boundary

Before traffic, both initialized setups must compare equal. Each of the three
post-flip snapshots then compares A with B. Snapshots remain in memory only:

- Whole `SavedBattleGame::save()` semantic YAML. Mapping insertion order is
  irrelevant; sequence order, node types/tags and scalar values are preserved.
- The engine `RNG::getSeed()` state, read without consuming randomness. Setup
  resets the engine seed and libc seed identically; neither is reset between
  checkpoints. Snapshotting also verifies it did not consume engine RNG.
- Gameplay unit state, occupancy and backlinks, visible-unit/visible-tile/FOV
  lists and spotted lists, tile visibility and shading, selection/side/turn and
  falling state that serialization alone does not cover.
- Every initialized `BattleAction` member, including this revision's initialized
  `diff`, with fixture-stable unit IDs rather than pointer addresses. An item
  pointer must be null in this unarmed fixture.

Exclusions are declared in advance: timer timestamps, callback/frame counts,
animation phase, render caches and surfaces, wall-clock timing, audio state
outside the verified disabled subsystem, and libc RNG state. Depth zero avoids
Map's underwater bubble-animation random path. Static floors without smoke/fire
avoid tile animation randomness; initial alien recoloring happens inside the
identically seeded setup. This source audit is **not dynamic verification of
libc RNG state**. Audio is disabled before resource loading, so this gate makes
no audio-equivalence claim. Pixel equality is neither required nor claimed.

The fixture covers this narrow stable, unarmed, land-based kneel/stand history
for both vanilla masters. It does not cover walking, firing, AI turns, modal
states, underwater animation, arbitrary missions or a comprehensive engine
state closure. Extra frame processing may still change excluded presentation
state, and the observer may still affect performance.

## Sensitivity and interpretation

Small in-memory comparator controls verify map-order independence, sequence
ordering and changed scalar detection. They are not engine mutations.

A separate temporary request-path positive control must consume engine RNG
inside `PresentationObserver::beginFrame()` after admitting a valid request.
Build it outside the publication tree, then run the same asset-dependent gate:
setup equality should succeed, but the first B post-flip comparison must fail.
Keep the normal source and binary unchanged and verify their hashes before
recording normal-run evidence or publication. Do not publish the mutant. A structural checker or build alone is not evidence that the
asset-dependent gate or this positive control ran.

The production observer and `Screen` code have no effects-gate test hook. The
new option and all effects-gate seams disappear from a default-OFF executable.
The earlier render-pair gate remains a separate visual/transcript check; its
results do not establish this functional-effects property.

## Recorded bounded result (2026-10-10)

The final opt-in binary passed once for each vanilla master, `xcom1` and `xcom2`,
including setup equality, three actual checkpoints and exactly three requests.
The separate temporary RNG-consumption mutant was rejected once in each master
by the engine-RNG equality assertion. The normal binary was not changed for
these controls. Opt-in/default-OFF builds, default-OFF string/symbol absence,
structural checks and existing native/Python regressions pass. See
[aggregate evidence and source/binary identification](observer-effects-evidence.json).

These are one-run-per-profile results, not repeatability or timing distributions.
No hosted CI, broader scenario matrix, full state closure or full fairness is
claimed. Stage 5 and the controller acceptance gate remain open.
