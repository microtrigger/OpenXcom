<!-- New project documentation: SPDX-License-Identifier: GPL-3.0-or-later. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Vanilla perceptual parity: selected policy and implementation contract

Version: `vanilla-perceptual-parity/0-draft`, 2026-10-10.
[Русская версия](PERCEPTION-CONTRACT.ru.md).

## Decision

The project owner selected vanilla perceptual parity instead of strict last-seen.
Preserve the atmosphere and incomplete information a human receives from the
original-style interface: action fragments in shadow, movement/weapon/door/death
sounds, and cursor/aiming hints when the running game actually presents them.
Do not silently “repair” the game's fog of war by suppressing those clues.
Conversely, do not treat a remembered behavior of the original executable as
proof that this OpenXcom build behaves identically.

This selects the policy direction. The per-field schema and implementation
acceptance gates below remain to be validated. It does not approve a campaign,
controller, bulk probing, or a claim of complete UI parity. The existing four-
number HUD prototype stays unchanged. Changes to the contract or public
projection require a new schema version and renewed validation.

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

First verify cursor assets and final-frame capture/occlusion using controlled
UFO and TFTD UI fixtures, then add a read-only current-hover presentation record
with independent review. No new semantic enemy/terrain/audio field is accepted
merely by adding this document. Full observer/MCP and controller gates remain.
