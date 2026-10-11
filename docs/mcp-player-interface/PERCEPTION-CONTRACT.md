<!-- New project documentation: SPDX-License-Identifier: GPL-3.0-or-later. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Vanilla perceptual parity: selected policy and implementation contract

Policy revision: `vanilla-perceptual-parity/1-practical`, 2026-10-11.
Supersedes `vanilla-perceptual-parity/0-draft` for the limited pilot route below.
[Русская версия](PERCEPTION-CONTRACT.ru.md).

## Decision

The project owner selected vanilla perceptual parity instead of strict last-seen.
Preserve the atmosphere and incomplete information a human receives from the
original-style interface: action fragments in shadow, movement/weapon/door/death
sounds, and cursor/aiming hints when the running game actually presents them.
Do not silently “repair” the game's fog of war by suppressing those clues.
Conversely, do not treat a remembered behavior of the original executable as
proof that this OpenXcom build behaves identically.

The per-field schema and implementation acceptance gates remain to be validated.
The selected `practical-play/1` profile below permits work toward one supervised
stance trial while retaining the original full gate as open. It does not approve
a campaign, bulk probing or a claim of complete UI parity. This documentation
amendment changes no existing prototype. Policy/profile changes must be versioned;
changes to the public projection require a new schema version and validation.

## Practical first-version acceptance profile

Profile: `practical-play/1`, selected 2026-10-11. Documentation/source baseline:
`5cedf820ab22b2c191ad8c6ac602ae1475b59f8a`. This records a scope decision, not a
new runtime result. The profile name is separate from observation wire-schema
versions; this amendment changes no public field or protocol.

The owner accepted ordinary fair play for the experimental first version:
use the normal displayed interface and mixed audio, including genuinely
presented cursor/aiming and shadow clues; do not read hidden state or use
response delays for reconnaissance. The immediate sequence is a supervised
kneel/stand trial, then an actual end-to-end token comparison on the accepted
bounded task. A campaign, broader action set and autonomous batches are outside
this profile.

The practical profile has the following acceptance requirements:

1. **Presentation-only information.** Every exposed field keeps its documented
   presentation source and validation. No hidden-state API, save/process-memory
   enrichment, debug view, hidden object identity/location/statistics, or hidden
   reasons in command results, errors or logs. Ambiguous cues remain ambiguous;
   ordinary cursor interaction does not license bulk scans. Test-only internal
   oracles must remain unavailable to the gameplay observer and controller.
2. **Focused action correctness.** The first trial is limited to normal
   kneel/stand input for the currently selected player-controlled soldier.
   Verify the normal action path, its actual TU cost and game-rule checks,
   presented before/after state, and agreement with ordinary UI execution in
   each claimed game/profile. Requests must not directly mutate stance, TU or
   other game state, suppress normal FOV/reaction-fire/death consequences, or
   advance simulation through a separate observer path. Request an explicit
   desired stance with one-use session/selected-soldier-bound context; do not
   expose hidden preflight or reaction-prediction reasons. Relevant existing
   regressions remain required.
3. **Supervision and safe stopping.** Each pilot stance request requires local
   in-game human confirmation; a tool argument cannot approve its own request.
   This is the first pilot's safeguard, not a permanent approval requirement for
   future joint automation. Keep human takeover available. Verify rejection or
   safe stopping for stale or changed context, manual intervention, invalid
   turn/state, and uncertain
   completion. A retry, lost reply or reconnect must not execute the stance
   change twice; if completion cannot be established, stop for reconciliation
   instead of replaying it. This profile adds no sequential batch or bulk probe.
4. **Bounded evidence and measurement.** Record the tested source/binary,
   game/assets/options, supported commands, checks and failures/not-run cases.
   Pass the focused correctness and information-boundary checks before the
   supervised trial, then retain outcome checks in the paired GUI/MCP benchmark.
   Distinguish requested/pending, dispatched and visually verified outcomes;
   dispatch alone is not successful completion. Count actual model tokens,
   calls and retries; unavailable token telemetry stays unknown. Byte counts and
   native latency are not token-savings evidence.

**Timing limitation.** Residual latency/scheduling side channels remain an
explicit experimental limitation. This profile neither requires nor establishes
timing equivalence or an absence of physical side channels. Do not classify
hidden worlds, infer hidden entities, or choose gameplay actions from response
latency, timeout patterns or waiting reasons. Ordinary performance measurement
is allowed, but cannot feed hidden-state inference. No new delay padding,
scheduler mitigation or timing calibration is required for this limited pilot.
The prior calibration and its inconclusive UFO control remain unchanged.

**Separate full gate.** The original full non-disclosure/controller acceptance
criteria, including the paired hidden-state, lifecycle, audio and timing work in
[Stage 5](README.md#5-security-gate-paired-hidden-state-tests), remain open.
`practical-play/1` is a limited pilot route, not a pass or a replacement claim for
that full gate. Unresolved non-timing disclosure, rule bypass or loss of control
still blocks the practical trial. Broader fields/actions require their own
scope and validation; full-game parity and campaign readiness are not implied.

Acceptance of this profile permits the next implementation and supervised-test
work; it does not assert that a controller exists, that a trial passed or that
tokens were saved. The fair-play conditions are the project's design and
supported-use contract, not additional copyright-license restrictions; the
[GPL rights and existing notices](../../LICENSE-NOTICES.md) are unchanged.

## Boundary: what reached the player

Fix the source revision, legal asset set, active mods/options, camera/viewport,
volume/stereo settings, and ordinary input history for every test profile.
Permitted observations are what the normal non-debug interface **actually
presents** under that profile:

- Final visible pixels after clipping, shading, overlays and UI composition.
- Audible mixed output after mute/volume, panning, channel allocation and mixing.
- Text/numbers actually displayed by the currently presented interface, where a
  source-mapped semantic extraction has been verified against that presentation.

Engine intent to draw or play a sound is insufficient. A clipped sprite, an
underlying map hidden by an overlay, a failed audio channel, or an inaudible
sound must not acquire a semantic event just because an internal call occurred.
No internal identity, creation order, raw sound-resource ID, true enemy health/TU,
precise hidden sound origin or AI state is allowed. Hidden inventory and map
occupancy are excluded; future own-inventory or plainly displayed position
fields require separate UI-equivalence validation.

An observation is distinct from an inference. “A short sound was audible on the
left” or “the crosshair visibly changed” can be observations. “Probably a door
opened” may be labelled an inference. “Alien 17 is at tile (x,y,z)” cannot be
invented from that clue or completed using hidden state. Public revisions and
IDs may depend only on permitted presentation history, not hidden event counts.

Known terrain follows actual current rendering, including vanilla quirks, rather
than an invented strict-last-seen memory. Any structured terrain field still
needs its own visual equivalence test: a smoke sprite does not license exporting
its hidden numeric density. History contains only previously obtained permitted
observations. Unseen facts stay unknown; ambiguity must remain ambiguity.

## Shadows, animation and sound

Transient legitimate clues can occur during animations and enemy turns. A
stable-player-turn-only observer would miss them. Keep the stable HUD snapshot
as one channel; design a separate bounded presentation-history channel that
records actual displayed frames/audio without advancing the game or reconstructing
missed events from internal state. Do not release raw actor/event identifiers.

Preserve only the spatial precision provided by the interface. Stereo pan is
not an exact world bearing, distance or source tile. Simultaneous or masked
sounds must not become an oracle for the number/identity of hidden sources.
A disconnected observer cannot backfill earlier events from a save or AI log.

## Cursor clues and ordinary interaction

A cursor cue is permitted only when it appears at the actual current hover and
in the current visible UI. Initial implementation should record the final
cursor-region pixels or another proven equivalent presentation record. Derive
semantic appearance labels only after verifying the actual vanilla assets.
Overlay/menu/viewport clipping must hide the same information for the observer
as for the human. An input event or calculated target flag is not a rendered cue.

No `enemy_present(tile)`, arbitrary-cell observation, bulk coordinate scan, free
line-of-fire predictor or internal aim-validity oracle is introduced. Hovering
need not be charged fictional TU when normal gameplay does not charge it, but
any future interactive observation must use the normal UI sequence, a single
current cursor, bounded serialized requests and a documented rate. It remains
subject to the controller gate and manual takeover. Reading observation alone
never moves the cursor or tests a target.

## Pinned source audit: 7d2adea67884f46d57f8fc1a5f69aea470b7f85d

The linked files are byte-identical to the previously audited source. These
references identify the audit basis; they do not claim that earlier prototype
tests were rerun on this revision.

- [`Map.cpp`](https://github.com/microtrigger/OpenXcom/blob/7d2adea67884f46d57f8fc1a5f69aea470b7f85d/src/Battlescape/Map.cpp#L680-L700), cursor back/front drawing: yellow box/crosshair branches require a
  unit with `getVisible()` or debug mode. Tile discovery is not separately tested
  there. This permits preserving a genuinely rendered black-tile hint, but does
  **not** establish that every hidden enemy changes the cursor. Debug is excluded.
- [`Map.cpp`](https://github.com/microtrigger/OpenXcom/blob/7d2adea67884f46d57f8fc1a5f69aea470b7f85d/src/Battlescape/Map.cpp#L185-L229), map drawing: hidden-movement overlay/terrain choice can reveal
  projectile, explosion or dying-unit cues. `drawUnit()` checks unit visibility;
  later clipping/shading still affects whether pixels reach the player.
- [`UnitWalkBState.cpp`](https://github.com/microtrigger/OpenXcom/blob/7d2adea67884f46d57f8fc1a5f69aea470b7f85d/src/Battlescape/UnitWalkBState.cpp#L350-L366): footsteps have visible-and-on-screen checks. Door-open
  sound calls do not have that same local gate. Weapon and death sound call sites
  have their own behavior; do not replace all with one new visibility policy.
- [`Map::getSoundAngle()`](https://github.com/microtrigger/OpenXcom/blob/7d2adea67884f46d57f8fc1a5f69aea470b7f85d/src/Battlescape/Map.cpp#L1717-L1733) produces clamped screen-horizontal pan. It is not an
  exact source-location contract.
- [`Sound.cpp`](https://github.com/microtrigger/OpenXcom/blob/7d2adea67884f46d57f8fc1a5f69aea470b7f85d/src/Engine/Sound.cpp#L77-L93) applies missing-resource/mute/channel/stereo behavior. Capture the
  permitted output after these effects, rather than exporting call arguments.

These are source findings, not completed screen/audio fixtures. Confirm the
specific cursor-in-darkness behavior in this build before advertising it as
reproduced legacy behavior.

## Revised non-disclosure tests

For identical permitted input/camera histories and equivalent final presented
video plus mixed audible histories, observer responses, revisions, identifiers,
ordering and errors must be indistinguishable even if hidden worlds differ.
Differences are allowed only when ordinary presentation differs, and only at
the precision explained by that presented clue. This replaces a blanket test
that incorrectly suppresses all off-LOS changes.

Required cases:

1. Hidden enemy versus empty tile with the same red cursor: identical output.
2. A verified yellow cursor clue: appearance may differ; no enemy ID/stats/origin.
3. Different hidden actors producing the same audible door cue/pan: identical.
4. Mute, zero volume, missing resource, failed channel or masking: no invented
   semantic event for inaudible information.
5. Same smoke appearance with different internal density: identical output.
6. Off-screen/occluded/clipped drawing and hidden-movement overlays: no underlying
   unit-presence information.
7. A genuinely presented brief projectile/death/audio cue between stable HUD
   samples survives only as a presentation-derived observation.
8. Menu/load/disconnect, new sessions, dropped history and manual interaction:
   no stale identity reuse or reconstruction from secrets.
9. Deliberate mutations exporting enemy health, raw sound IDs/source tiles or
   hidden event counters fail the tests.

Timing analysis compares against what was presented; internal tick timing,
waiting reasons and unpresented event counts are not public metadata. Physical
side-channel freedom is not claimed without measurements.

## Next implementation slice

Follow `practical-play/1`: implement and verify the bounded normal stance path,
run the supervised kneel/stand trial, then measure actual end-to-end token use.
Retain the evidence and limitations of the existing observer slices. No new
semantic enemy/terrain/audio field is accepted merely by adding this document.
Full observer/MCP and controller gates remain open separately.
