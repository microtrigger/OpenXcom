<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original project documentation, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Controlled smoke-density render equivalence

This separately compiled, default-off fixture covers one additional stage-5
render dependency. It reuses the [hidden-occupant gate](HIDDEN-OCCUPANT-GATE.md)'s
controlled rendering and native transport harness. It does not add a controller,
public smoke field, external listener, registration or normal gameplay behavior.
Passing this fixture does not complete stage 5 or establish full fairness.

## Recorded verification

The [frozen-source, build and runtime evidence](smoke-density-evidence.json)
records one frozen-binary run each for UFO and TFTD passing on 2026-10-11. Each run
returned eight exact complete-frame matches for the density pair, 24 non-null
native responses and eight boundary-control phases with different complete
frames and actual cursor crops. Both exited successfully on their first fixture
run. This is one run per master, not a repeatability distribution.

The optional and default-OFF builds pass. The OFF executable contains no gate or
observer symbols/activation strings. Seven incompatible configurations are
rejected, and the original hidden-occupant profile compiles under strict warning
flags with no smoke-profile symbols or assertions. All structural boundary
checks, six wrapper unit tests, the native cursor/HUD/transport suites, 15 paired
UI tests and their two existing killed mutants, 18 diagnostic-isolation tests,
and 208 adapter Python tests (two skipped) pass. The two new smoke-dependency
mutants were each run once per master: all four were rejected on their first
run with the intended assertion and exit status, without a crash or timeout.
The hidden-density HUD leak failed smoke-pair-public-transcript after the RGB
and positive-control preconditions passed; the wrong density group failed
smoke-pair-final-rgb before transcript comparison. Only documentation/evidence
changed after the frozen-binary runs. Independent review covers this bounded
fixture only; no hosted CI, repeatability distribution or wider fairness result
is claimed.

## Build and run

Use the ordinary OpenXcom build dependencies and separately installed, legally
obtained UFO or TFTD resources. No proprietary resources, saves, screenshots,
raw battle state or transport secrets are included in this fixture.

```sh
cmake -S . -B build-smoke -DCMAKE_BUILD_TYPE=Release -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_CURSOR_OBSERVER=ON -DEXPERIMENTAL_SMOKE_DENSITY_GATE=ON
cmake --build build-smoke
python3 tests/battle-observer/run-smoke-density.py \
  /absolute/path/build-smoke/bin/openxcom /absolute/path/data xcom1
# Repeat with xcom2 for the vanilla TFTD resource/rule profile.
```

The option defaults to OFF, requires the cursor observer, and rejects a build
combined with any other battle gate, legacy HUD observer or mixed-audio observer.
The existing render-pair compile macro enables the shared composition seams;
the additional smoke-profile macro selects this fixture instead of the original
hidden-occupant fixture. Building only EXPERIMENTAL_BATTLE_PAIR_GATE retains the
original hidden/empty/visible-occupant test and its original output.

The wrapper supplies the second opt-in, OPENXCOM_SMOKE_DENSITY_GATE=1. It creates
and removes a disposable user/configuration profile, pins English and exactly one
vanilla master, enforces a 45-second timeout and exposes only the exact aggregate
result or fixed assertion labels. Ordinary engine diagnostics remain private.
The trusted engine's raw stdout/stderr are buffered in memory until exit; the
timeout bounds duration, not diagnostic-memory growth.
Inherited observer destinations or other gate activations are rejected rather
than redirected. Do not activate the binary manually with a normal user directory.

A real, focused SDL graphical window with the physical pointer inside it is
required when loading finishes. Unsupported display/focus conditions fail the
gate rather than being mocked. External input is excluded during sampling except
window-close and application-focus events. This is not a headless renderer test.

## Real dependency and fixture provenance

Each scenario constructs fresh SavedGame/SavedBattleGame, real 40×40×1 Tile maps,
Pathfinding/TileEngine, one selected player and one parked invisible hostile.
Both units use the active master's one-tile Sectoid/Aquatoid rule and stay alive.
The roster, order, positions, camera, discovery, HUD and pointer setup are fixed.
The map is discovered but otherwise empty, without terrain assets assigned to
its tiles. This is a controlled setup, not a naturally reachable battle history.

The actual hover tile receives smoke 7, smoke 8, or the boundary-control value 6
through Tile::setSmoke. These authored inputs are fixture descriptions only;
no live hidden values are exported. Both engine RNG and std::rand are seeded
once at the beginning of each fresh scenario, before identical unit construction
and the RNG-consuming smoke setter. They are never reset by a request. After
ordinary BattlescapeState construction/init, the fixture checks equal engine
RNG state, smoke animation offset and shade. Every sampled frame additionally
requires the intended density, no fire, discovery, no occupant, and depth zero.
Both masters therefore test their land-smoke rendering path; TFTD underwater
smoke and its additional animation randomness are outside this fixture.

Production Map selects the ordinary SMOKE.PCK frame using the smoke group plus
its animation offset. The current grouping term is floor(smoke / 6.0 - 0.1):
7 and 8 share a group; 6 is the adjacent lower group. A test-only witness located
immediately after the actual production smoke blit records the visited Tile,
phase, chosen frame number and shade internally. Exactly one such blit and one
ordinary hover-cursor branch visit are required per sampled frame. No sprite
substitution, visibility override, forced observation FOV or private-access
macro is used. The real mod resources are loaded through ordinary StartState.

## Sampling, presentation and transport oracle

There are eight complete frames per scenario, covering animation phases 0–7.
The first frame follows ordinary initialization. Later frames call the existing
BattlescapeState::animate exactly once. Normal state think, battle simulation,
timer-driven thinking and frame-rate waiting are deliberately skipped during
this render-only interval. The ordinary Game composition, HUD/widget blits,
cursor composition and successful Screen::flip are retained. Fresh scenario
setup deletes the old BattlescapeState before replacing its battle/save.

At the final presentation boundary, the harness independently palette-resolves
every pixel of the complete 640×400 display into RGB. These histories stay only
in process memory. Before comparing any transcript it requires:

- Exact whole-display RGB equality for the 7/8 pair at all eight phases
- Identical cursor and HUD-candidate geometry/eligibility for the pair
- The same actual selected smoke frame for the pair at every phase
- Different complete RGB and different actual produced cursor crop between 7
  and boundary control 6 at every phase, with matching eligibility and dimensions
- Different selected production smoke frames for the boundary control

The eight-phase positive-control requirement is fixed before resource-dependent
runs. A failure is a failure or unsupported profile; it must not be silently
relaxed to a weaker crop, phase subset or replacement sprite.

The in-process client owns a connected Unix socketpair. Every request is queued
before the production beginObserverFrame; every response is produced by the
normal PresentationObserver after the successful flip. All 24 responses must
have non-null cursor and four-field HUD payloads, valid HUD bounds and exact
crop bytes. Epoch/challenge binding, admission chaining, fresh-token uniqueness,
one response per request, absence of extra packets and an open channel are
checked throughout all three scenarios.

Only after the visual preconditions hold does the harness compare the pair's
complete request/response byte transcripts, including packet lengths, order
and count. Normalization is restricted to the three explicitly validated
32-character transport-random token fields, with a width-preserving consistent
bijection across each complete trace. Raw epoch continuity and token freshness
are validated before normalization. Comparator self-checks separately alter
one captured RGB byte and one recorded HUD value and require rejection; they
are not substitutes for the engine-dependency mutants below.

## Audio, diagnostics and limits

The wrapper chooses SDL's dummy audio driver for initialization. Before resources
load, the native gate closes the actual SDL_mixer device and shuts down the whole
SDL audio subsystem, then pins mute. Mixer closure, subsystem shutdown and mute
are checked throughout. The sampling profile has no game audio output; this is
not post-mix PCM equivalence, audible-cue testing or an audio observer interface.

Only the fixed PASS aggregate or generic source assertion labels leave the
harness. The wrapper does not print ordinary resource-loader diagnostics. It
never serializes hidden smoke, unit or world state, RGB histories, raw observer
packets, nonces, saves or resources. Successful response payloads are inspected
internally and the production observer contract remains unchanged.

This does not test smoke propagation, damage, turns, fire, underwater smoke,
occlusion, hidden movement, death/projectiles, natural scenario provenance,
load/reconnect/history, timing distributions, physical side channels, other
platforms/scales/assets or future command availability. The Python adapter is
not exercised by this native battle fixture. Existing adapter and paired-UI
suites cover separate boundaries. No runtime result should be inferred from
source checks or a successful build.

## Structural checks and deliberate mutants

```sh
python3 tests/battle-observer/check_smoke_density_boundary.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/battle-observer/test_smoke_density_wrapper.py
python3 tests/battle-observer/check_boundary.py
# Adapter regressions use their documented working directory:
(cd tools/mcp-observer && PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -v)
```

The first command is a structural tripwire. The wrapper unit tests mock only
subprocess results to check filtering, exact-result acceptance, timeout and
profile cleanup; they do not run or simulate the renderer.

Two checked patch files introduce deliberate negative-test faults into disposable
source copies only. Neither fault belongs in a production binary or published
observer source. Apply one patch per isolated worktree from the frozen gate
revision, build with the same smoke options, and run both masters using the same
wrapper and graphical conditions:

```sh
git worktree add --detach ../smoke-mutant-hud <frozen-gate-revision>
cd ../smoke-mutant-hud
git apply --unidiff-zero tests/battle-observer/smoke-density-mutants/hidden-density-hud.patch
# Configure/build/run as above with a separate build directory.
```

- hidden-density-hud.patch captures actual Tile::getSmoke at the production smoke
  branch and substitutes it for the native response's time_units value, without
  changing displayed pixels. It must be killed by smoke-pair-public-transcript,
  after all same-RGB and boundary-control preconditions have passed.
- wrong-density-group.patch changes the actual selected production smoke frame
  only for density 8. It must be killed by smoke-pair-final-rgb before transcript
  comparison. Merely failing the structural tripwire does not count as a runtime
  mutant kill.

Record the frozen native source hashes, compiler/build options, final binary
hashes, exact aggregate runs, mutant labels and unsupported/failing profiles
alongside any reported result. Rebuild and rerun after native-source changes;
documentation-only edits do not substitute a different tested binary.
