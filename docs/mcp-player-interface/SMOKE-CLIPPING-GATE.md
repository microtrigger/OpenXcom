<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original project documentation, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Controlled smoke viewport clipping equivalence

This separately compiled, default-off fixture is a bounded next stage-5 gate
for smoke that reaches the production render branch but contributes no pixels
because its sprite is wholly outside the map viewport. It reuses the controlled
rendering and native transport harness from the
[smoke-density gate](SMOKE-DENSITY-GATE.md). It adds no gameplay controller,
entity list, public smoke field, external listener or registration. Passing this
gate would not complete stage 5 or establish full fairness.

## Verification status

One controlled graphical run each for UFO and TFTD passed on 2026-10-11, on the
same frozen binary. Both passed on their first fixture invocation with exit 0:
four fresh worlds, eight complete clipped-pair RGB matches, 32 valid native
responses and eight centered phases with different complete frames and actual
produced cursor crops. No game-runtime failure was excluded. This is one run per
master, not a repeatability distribution.

The optional and default-OFF builds pass. The OFF executable contains no gate or
observer symbols or activation strings. Eight invalid configurations are rejected.
Clipping, smoke-density and original hidden-occupant fixture objects compile under
strict warnings. Structural checks, 16 combined density/clipping wrapper tests,
native cursor/HUD/transport suites, 15 paired UI tests and their two existing
mutants, 18 diagnostic-isolation tests and 208 adapter tests (two skipped) pass.
These separate checks do not establish broader battle or adapter equivalence.

The tested native sources match the listed hashes in the evidence and extend
the published source baseline. Publication may differ in documentation/evidence
only. The tested binary SHA-256 is
70c2a62cef7edbefef540ccaf1a59aeaca5b519223cd4d52f9942a12b471700b.
The [frozen-source and runtime evidence](smoke-clipping-evidence.json) records
these results and the two deliberate mutants. Each mutant was run once for each
master; all four runs were rejected on their first invocation by the intended
assertion with exit 1, without crashes, timeouts, retries or excluded runs. The
HUD leak failed clipped-pair-public-transcript after the final-RGB and centered
positive controls passed. The collapsed group failed
centered-density-pixel-positive-control. Independent source review found no
blocker to these bounded tests.

After the runtime runs, only documentation/evidence and byte-preserving mutant
recipe formatting changed. The final recipes reproduce the exact tested mutant
production sources. No result from the earlier smoke-density gate is used for this
clipping fixture. No hosted CI or wider fairness result is claimed.

## Build and run

Use the ordinary OpenXcom build dependencies and separately installed, legally
obtained UFO or TFTD resources. No proprietary resources, saves, screenshots or
raw battle state are included.

```sh
cmake -S . -B build-smoke-clipping -DCMAKE_BUILD_TYPE=Release -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_CURSOR_OBSERVER=ON -DEXPERIMENTAL_SMOKE_CLIPPING_GATE=ON
cmake --build build-smoke-clipping
python3 tests/battle-observer/run-smoke-clipping.py \
  /absolute/path/build-smoke-clipping/bin/openxcom /absolute/path/data xcom1
# Repeat with xcom2 for the vanilla TFTD resource/rule profile.
```

EXPERIMENTAL_SMOKE_CLIPPING_GATE defaults to OFF and requires the cursor observer.
It rejects combination with the smoke-density, hidden-occupant, observer-effects,
natural-FOV and battlefield-audio gates, or the legacy HUD and mixed-audio
observers. The shared render-pair compile macro retains the ordinary composition
seams; the clipping-profile macro selects these four scenarios. The separate
hidden-occupant and smoke-density profiles retain their own fixtures.

The wrapper supplies the second opt-in, OPENXCOM_SMOKE_CLIPPING_GATE=1. It creates
and removes a disposable user/configuration profile, pins English and exactly one
vanilla master, enforces a 45-second timeout and publishes only the exact aggregate
result or fixed assertion labels. Inherited observer destinations and competing
gate activations are rejected by the native fixture. Do not activate it manually
with a normal user directory. Raw engine stdout/stderr stay buffered in memory
until exit; the timeout bounds duration, not diagnostic-memory growth.

A real, focused SDL graphical window with the physical pointer inside it is
required when loading finishes. Unsupported display or focus conditions fail
the gate. During sampling, external input is excluded except window-close and
application-focus events. This is not a headless renderer test.

## Four fresh worlds

The scenario order is centered smoke 6, centered smoke 7, clipped smoke 6 and
clipped smoke 7. Each scenario constructs fresh SavedGame/SavedBattleGame,
40×40×1 Tile maps, Pathfinding/TileEngine, one selected player and one parked
invisible hostile. Both units use the active master's one-tile Sectoid/Aquatoid
rule and stay alive. The map is discovered but otherwise empty, without terrain
assets assigned to its tiles. This is controlled fixture provenance, not a
naturally reachable battle history.

In every world, Tile::setSmoke sets the real smoke at (20,22,0). The engine RNG
and std::rand are seeded once at the start of each fresh scenario, before the
identical unit construction and RNG-consuming smoke setter, never by a request.
After normal BattlescapeState construction/init, the fixture requires equal
engine RNG state, smoke animation offset and shade. Every sampled frame checks
the authored density, no fire, discovery, no occupant on the smoke tile and depth
zero. Both masters therefore exercise land smoke; TFTD underwater smoke is out
of scope.

The centered pair uses the ordinary initialized camera and hovers the smoke
tile. The clipped pair uses ordinary Camera::centerOnPosition at (14,27,0),
with the hover cursor at that same tile. This places the real smoke's blit at
x=336 in the 320-pixel-wide map surface. Ordinary Map hit-testing establishes
both cursor positions; no selector field is directly assigned.

These are four separate fresh-world histories. Within each pair, setup and
camera are held fixed while density differs. The centered histories are positive
controls, not earlier camera states of the clipped histories. No comparison
claims equivalence across different camera histories, and no pan from centered
to clipped is performed.

## Production clipping witness

Production Map chooses a real SMOKE.PCK frame using its ordinary density grouping
term, floor(smoke / 6.0 - 0.1), plus the animation offset. Densities 6 and 7 select
adjacent groups. The gate retains the production smoke blit and records a
test-only witness immediately after that actual blitNShade call, using the actual
Tile, phase, frame number, shade, source sprite, destination surface and blit
coordinates. Exactly one smoke-blit visit and one ordinary hover-cursor branch
visit are required in each sampled frame. No sprite replacement, visibility
override, forced observation FOV or private-access macro is used.

The witness reconstructs the unmasked production ShaderMove source domain,
translated source image and destination image from those actual post-blit
arguments, then checks their GraphSubset intersection. It requires a full source
domain, a 32-pixel-wide smoke sprite, a 320-pixel-wide destination at (0,0), and
matching geometry within each density pair.

For clipped worlds, x=336 must remain inside Map's ordinary traversal margin
(x < 320 + 32), while the translated smoke image starts beyond the destination's
right edge and has zero horizontal intersection. Its vertical extent must remain
inside the destination. Thus the real branch is visited and the real selected
sprite is blitted, but its shader domain is wholly clipped horizontally. The
centered sprite must instead lie fully within the destination with full source
width and height in the intersection. A tile skipped by traversal, a substituted
sprite or an assumed coordinate without these actual domains does not satisfy
this witness.

## Presentation and transcript oracle

Each scenario records eight complete frames, covering animation phases 0–7.
The first follows normal initialization; each later frame calls the existing
BattlescapeState::animate once. Normal state think, battle simulation,
timer-driven thinking and frame-rate waiting are deliberately skipped during
this render-only interval. Ordinary Game composition, HUD/widget blits, cursor
composition and successful Screen::flip are retained. The old BattlescapeState
is deleted before its battle/save is replaced for the next scenario.

At the final presentation boundary, the harness independently palette-resolves
every pixel of the complete 640×400 display into RGB. All histories remain in
process memory. After all four histories are complete, verification requires:

1. Exact complete-display RGB equality for clipped smoke 6 versus clipped smoke
   7 at all eight phases, before pair eligibility or transcript comparison
2. Different complete RGB and different actual produced cursor crops for
   centered smoke 6 versus centered smoke 7 at every phase
3. Matching cursor and HUD-candidate geometry/eligibility within each pair
4. Different selected production smoke frames for density 6 versus 7 in both
   pairs, with the same density selecting the same frame in both camera setups
5. Matching actual source/destination geometry within each pair
6. Equal complete normalized request/response byte transcripts for the clipped
   pair, including packet lengths, order and count

The full eight-phase requirement is fixed before resource-dependent runs.
Failures must be reported as failures or unsupported profiles; they must not be
silently weakened to a phase subset, crop-only comparison or replacement sprite.

An in-process client owns a connected Unix socketpair. It queues each request
before the production beginObserverFrame, and the normal PresentationObserver
produces each response after a successful flip. All 32 responses must contain
non-null cursor and four-field HUD payloads, bounded HUD values and the exact
actual produced crop bytes. Epoch/challenge binding, admission chaining,
fresh-token uniqueness, one response per request, absence of extra packets and
an open channel are checked throughout all four scenarios.

Only the clipped pair's transcripts are compared for equality. Normalization is
restricted to the three explicitly validated 32-character transport-random token
fields, using a width-preserving consistent bijection across each complete
trace. Raw epoch continuity and token freshness are checked before normalization.
Comparator self-checks alter one captured RGB byte and one recorded HUD value
and require rejection; these checks are separate from real engine-dependency
mutants.

## Audio and disclosure boundaries

The wrapper selects SDL's dummy audio driver for initialization. Before resources
load, the native gate closes the real SDL_mixer device, shuts down the entire SDL
audio subsystem and pins mute. These conditions are checked throughout sampling.
This profile has no game audio output. It tests neither post-mix PCM equivalence
nor audible cues.

Only the fixed PASS aggregate or generic source assertion labels leave the
harness. Hidden values, entity/world state, RGB histories, raw observer packets,
nonces, saves and resources are never serialized by the fixture. The authored
coordinates and densities above describe static fixture inputs. The production
observer contract and its public fields remain unchanged.

## Structural checks and deliberate mutants

```sh
python3 tests/battle-observer/check_smoke_clipping_boundary.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/battle-observer/test_smoke_clipping_wrapper.py
python3 tests/battle-observer/check_smoke_density_boundary.py
python3 tests/battle-observer/check_boundary.py
# Adapter regressions have their own boundary and working directory:
(cd tools/mcp-observer && PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -v)
```

The clipping boundary check is a structural tripwire. Wrapper unit tests mock
subprocess results to check exact aggregate acceptance, diagnostic filtering,
failure handling, timeout and profile cleanup. Neither runs or simulates the
renderer, and neither constitutes dynamic clipping evidence.

The deliberate hidden-density-hud.patch fault belongs only in a disposable
source copy. Both patch recipes use zero-context diffs and require
--unidiff-zero when applying them. This formatting was verified to recreate the
exact tested mutant source bytes. It captures actual Tile::getSmoke at the production smoke branch
and substitutes it for the native response's time_units value without changing
displayed pixels. Apply it from the frozen gate revision, build with the same
clipping options and run both masters through the same wrapper and graphical
conditions:

```sh
git worktree add --detach ../smoke-clipping-mutant <frozen-gate-revision>
cd ../smoke-clipping-mutant
git apply --unidiff-zero tests/battle-observer/smoke-clipping-mutants/hidden-density-hud.patch
# Configure, build and run as above with a separate build directory.
```

The required runtime rejection is clipped-pair-public-transcript, after all
clipped full-RGB and centered positive-control preconditions pass. A structural
check rejecting the patch, a crash, a timeout or failure of an earlier assertion
does not count as the intended mutant kill. Values 6 and 7 remain plausible,
schema-valid HUD integers; this is a real dependency leak rather than an invalid
schema that the transport would reject. This native test does not run the Python
adapter.

The separate collapsed-centered-group.patch changes the actual production frame
group selected for density 7 to that selected for density 6. Apply it alone in
another disposable source copy. It must fail centered-density-pixel-positive-control:
the centered worlds no longer demonstrate different real complete frames and
cursor crops. This verifies the fixed all-eight-phase positive control, while
the HUD mutant verifies transcript sensitivity after clipped RGB equality.
This second mutant does not test a clipping bypass. Both mutants were rejected
by their intended assertions in one run per master, as recorded in the evidence.

## Limits and recording results

This tests complete right-edge viewport exclusion for one real smoke tile in a
fixed render-only setup. It does not test partially clipped sprites, other edges,
occlusion, smoke propagation or damage, turns, fire, underwater smoke, hidden
movement, death/projectiles, natural scenario provenance, camera-history
equivalence, load/reconnect/history, timing distributions, physical side
channels, other platforms/scales/assets or future command availability.

The Python adapter is not exercised by this native battle fixture. Separate
adapter and paired-UI suites address their own boundaries. The aggregate states
simulation=false, timing_equal=false and full_fairness=false; none of those
claims is strengthened by a PASS.

Record frozen native source hashes, compiler/build options, final binary hashes,
exact aggregates, mutant assertion labels and unsupported/failing profiles
alongside any reported dynamic result. Rebuild and rerun after native-source
changes. Update verification status only from recorded runs; documentation-only
edits do not substitute a different tested binary.
