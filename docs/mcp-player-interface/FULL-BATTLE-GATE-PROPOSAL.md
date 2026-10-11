<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original project documentation, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Proposed subsequent hidden-world battle gate

This is a bounded design proposal with three implemented render-only cases: the
[hidden-occupant pair](HIDDEN-OCCUPANT-GATE.md), the
[smoke-density pair](SMOKE-DENSITY-GATE.md) and the
[full right-edge smoke-clipping pair](SMOKE-CLIPPING-GATE.md), all default-off.
Other clipping/occlusion, the hidden-movement overlay/queue and offscreen death
remain proposals. These narrow pairs and the paired UI tests do not close stage 5
or authorize a controller. A separate [bounded request-effects gate](OBSERVER-EFFECTS-GATE.md)
now exercises normal kneel/stand execution and engine-RNG comparison; it does
not implement the remaining hidden-world or timing matrix.
Source feasibility is pinned to `b8d9806aa83151c4da1805c691f50903512c7f65`.

## Smallest credible path

Use a separately compiled, default-off test runner after the ordinary game has
loaded legally installed resources and language, before entering a campaign.
Construct fresh SavedGame/SavedBattleGame fixtures internally rather than loading
raw campaign saves. All hidden fields stay inside the game process. The runner
exports only fixture pass/fail, permitted projection comparisons, and carefully
reviewed aggregate evidence. It must never export hidden fixture values, raw
world snapshots, saves, resources or unrestricted debug commands to MCP or the
assistant. Use isolated configuration/output directories and no external listener.

Relevant production paths already exist:

- `SavedBattleGame::initMap` creates real Tile objects; `initUtilities(Mod*)`
  creates Pathfinding and TileEngine. Map drawing requires Pathfinding.
- `BattleUnit(Unit*, faction, id, Armor*, StatAdjustment*, depth)` constructs
  real non-soldier units from rules. Register units in the battle, set positions
  and link actual tile occupancy; keep the selected player and visible HUD fixed.
- `resetUnitTiles` finishes the before-game phase, which matters for death logic.
- Use a real BattlescapeState and its normal init/cache/update work, normal Game
  composition and Screen::flip. Keep ordinary mouse/camera input histories equal.
- Record complete final RGB histories and post-mix audio internally. Only after
  presentation equality holds compare the production observer transcripts.

An asset-free full renderer is not a small extension of the UI fixture. Map needs
Game/Mod/Screen, interface rules, BLANKS/CURSOR resources, fonts and language.
Battle rendering also depends on smoke/unit/item sheets and Battlescape buttons
and icons. Mod::loadAll loads vanilla resources; resource maps and some terrain
surface storage are private without injection APIs. A legally original synthetic
resource harness is possible, but requires explicitly designed test-only
Game/Mod/MapDataSet builders and much broader engine linkage. It cannot certify
vanilla UFO/TFTD asset semantics by itself. Avoid replacing renderers, bypassing
constructors or private-access macro tricks merely to shrink the harness.

## Initial genuine dependency pairs

1. Hidden occupant versus empty current-hover tile. Put a genuine invisible
   hostile in the actual Tile read by Map's cursor/unit renderer in one world;
   leave it empty in the other. Keep discovery, camera, selected player and normal
   hover fixed. Compare a complete cursor animation cycle, complete frames and
   audio, then observer output. Add a visible-occupant positive control with
   genuinely different final cursor pixels. An unused BattleUnit or unrelated
   secret variable would not exercise this dependency.
2. Same smoke appearance with different density: [implemented bounded
   gate](SMOKE-DENSITY-GATE.md). Its first normal run in each UFO/TFTD master
   passes all eight complete-frame 7/8 matches, 24 production-native responses
   and eight density-6 positive phases affecting both frame and cursor crop.
   Both RNG seeds are pinned before unit construction and Tile::setSmoke; an
   actual production-blit witness checks the selected smoke frame and phase.
   The discovered, terrain-empty fixture uses depth 0 in both games. Simulation
   and timers are skipped, and the audio subsystem is disabled. Both real-source
   mutants are rejected in each game: a hidden-density HUD leak fails the public
   transcript comparison; a wrong smoke frame fails the whole-display RGB check.
   All six normal/mutant runs completed on their first attempt without crashes,
   timeouts or exclusions; see [aggregate evidence](smoke-density-evidence.json).
   This implements only the controlled render/transport case, not smoke
   propagation, damage, turns, fire, underwater behavior, audio or timing equivalence.
3. Occlusion and clipping: the [full right-edge smoke-clipping case](SMOKE-CLIPPING-GATE.md)
   is implemented. Its first normal run in each UFO/TFTD master uses four fresh
   worlds: centered smoke 6/7 gives eight different complete frames and actual
   cursor crops, while wholly clipped smoke 6/7 gives eight equal complete frames
   and normalized native transcripts; all 32 native responses are valid. The real
   post-blit witness checks source/destination domains at x=336 in a 320-pixel-wide
   map surface: the actual smoke branch is visited, but its sprite has zero
   horizontal intersection. Camera/input history is fixed within each pair; the
   centered and clipped pairs are independent histories, with no pan between them.
   Both RNG seeds are pinned; the discovered, terrain-empty depth-0 fixture skips
   simulation/timers and disables audio. Both source mutants are rejected in each
   game: an actual-density HUD leak fails `clipped-pair-public-transcript` after
   the visual preconditions pass; collapsing the centered density groups fails
   `centered-density-pixel-positive-control`. The latter tests centered positive-control
   sensitivity, not a clipping bypass. All six normal/mutant runs completed on
   their first attempt without crashes, timeouts or exclusions; see
   [aggregate evidence](smoke-clipping-evidence.json). Partial clipping, other edges,
   occlusion and the actual hidden-movement overlay/ordinary queue remain open;
   merely labelling a fixture “offscreen” does not exercise those branches.
4. Offscreen death, after the simpler pairs. Advance genuine UnitDieBState through
   ordinary battle queues. Do not assume offscreen means silent: death audio is
   not gated identically to unit drawing, and death may change selection, corpse
   inventory or mission status. Start with actors giving the same actual mixed
   audio/pan or a separately pinned mute profile. A real brief presentation clue
   is a positive control, not an unauthorized hidden semantic event.

Keep healthy opposing forces to avoid unintended mission-end transitions.
Fixtures must not call FOV to force an observation or redefine vanilla visibility.
Any initialization required for a fixture belongs to documented test setup, not
an observer request or a shortcut around ordinary rendering.

## Oracle and stopping condition

Fix engine revision, resources/configuration, camera and input history. Assert
complete palette-resolved final frame histories and mixed-audio histories equal
before testing observation equivalence. Equal crops alone are insufficient for
this future gate. Compare packet bytes, lengths/order/counts, public fields,
errors, closure and observer-accessible diagnostics, normalizing only explicitly
validated independent transport-random tokens. Require successful non-null
positive controls and deliberate leaks from actual tested engine dependencies to
be detected. Keep hidden diagnostics private inside the harness.

Latency needs a separate repeated, randomized paired measurement with a declared
noise model; correctness tests are not a timing proof. Post-mix PCM capture itself
does not prove speaker audibility or external window/volume conditions.

Completing these initial cases would still leave the wider required world/event
matrix, UFO/TFTD profiles, load/reconnect/history behavior, sound cases, timing,
independent review and future command-availability tests. Passing one narrow
battle fixture must not be relabelled as complete stage-5 certification.
