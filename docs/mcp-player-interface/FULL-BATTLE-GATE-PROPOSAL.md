<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original project documentation, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Proposed subsequent hidden-world battle gate

This is a bounded design proposal, not an implemented fixture or a passing gate.
The current paired UI tests do not close stage 5 or authorize a controller.
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
2. Same smoke appearance with different density. Current Map rendering groups
   smoke densities 7/8 into the same sprite group when fire/shade/animation offset
   agree. Verify all eight animation phases rather than assume they match.
   Tile::setSmoke randomizes animation offset, and unit construction also consumes
   randomness: pin both engine RNG and std::rand setup internally. Restrict the
   pair to rendering/animation; advancing a turn may destroy equivalence. Require
   a density-boundary positive control that changes actual pixels.
3. Occlusion and clipping. First establish different real unit/smoke/cursor pixels
   without suppression. Then apply the same normal overlay, viewport clipping or
   offscreen camera, and verify final equality. Exercise the actual hidden-movement
   overlay branch rather than merely label a fixture “offscreen.”
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
