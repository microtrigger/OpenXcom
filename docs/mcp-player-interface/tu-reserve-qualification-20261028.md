# TU reserve None / Snap: bounded qualification, 2026-10-28

## Contract

Schema 27 adds `reserve_none` / `reserve_snap` (no arguments) and the nullable
`reserve_indicator` on Battlescape. The indicator comes from the actually drawn
radio-button group, not the saved battle's reservation. All four radio buttons
must be painted to the current screen and uncovered; missing, clipped,
mutated-before-repaint or ambiguous selection is withheld. Aimed and Auto can be
observed, but this slice does not add their commands or kneel reservation.

Dispatch uses `btnReserveClick` with the actual button as sender. Ordinary
player-turn, busy, projectile and panic gates remain. Existing observation
capabilities, overlay checks and reservation/selection revisions remain in force.
The native handler updates its ordinary path preview; no direct model mutation,
TU prediction, hidden state or path-cost inference was added.

## Evidence and limits

- Full Release build with experimental agent control: passed.
- Actual native radio-button/Surface offscreen tests: 17 checks passed, including
  Snap / None, before-repaint and ABA invalidation, wrong destination, clipping,
  hidden controls and a real covering surface. Synthetic geometry and palette;
  not a whole-game test.
- Extracted actual reserve provider, dispatch branch, native handler and button
  availability bodies passed with explicitly synthetic SDL/save/map/battle
  dependencies. Player/enemy, debug-enemy, first-init, panic, busy, projectile,
  invisible and ambiguous selection cases were exercised.
- Native protocol checked schema 27, schema 26 refusal, the two closed commands,
  invalid arguments and unsupported reserve aliases. Five adapter contract tests
  passed.
- Adjacent adapter suites were compared with unchanged baseline: 92 control tests
  retain one pre-existing Geoscape action-matrix failure; 18 gameplay tests retain
  two pre-existing map-context/event-fixture errors. This is **not full-suite PASS**.

## Live UFO smoke

At approximately 05:47–05:49 UTC on 2026-10-28, a new isolated Beginner UFO New
Battle (Small Scout, Farm) went through the ordinary briefing and inventory into
turn 1. No existing campaign or save was loaded or overwritten. Fresh MCP reads
showed `none` / 65 TU, then `snap` / 65 TU after `reserve_snap`, then `none` /
65 TU after `reserve_none`. A subsequent ordinary primary map activation moved
the selected unit from public position (4,29,1) to (4,28,0), with 61 TU shown and
normal input context restored. No success was inferred from a dispatch receipt.

All gameplay decisions and actions in this smoke were MCP. Passive H.264 video
was packet-verified before gameplay and recorded the route; audio was not
recorded or requalified. No screenshots or visual gameplay controls were used.
The technical capture preflight needed packet flushing before recording could
be verified; no gameplay input preceded that gate.

During technical closure at approximately 05:50 UTC, an app-bound Alt-F4 did not
close the owned window. One screenshot was used to diagnose that specific
closure failure, followed by the desktop window-manager Alt-F4; window inventory
then confirmed closure. Thus the **entire session is not labelled pure MCP**.
This was a technical window-close distinction, not a reserve/gameplay fallback.
Use the desktop window-manager shortcut for future normal test-window closure.

This qualifies the bounded None/Snap slice in one live UFO test battle, not a
completed campaign or completion of the broader player-equivalent interface.

## TFTD live qualification (06:17–06:24 UTC, 28 October 2026)

The same published binary was exercised in an isolated TFTD New Battle, Port
Attack / Triton / Beginner / Aquatoid. Fresh player-visible MCP observations
confirmed None/58 TU → Snap/58 TU → None/58 TU. A subsequent ordinary movement
through the adjacent door changed the selected position from (5,17,0) to
(6,17,0), TU 50, with normal input context. These gameplay actions used MCP;
no hidden state or saves were read. Passive H.264 video was verified before
input and finalized (414.8 seconds, no recorded audio).

The first Survey Ship test ended before combat: all aliens died in the crash.
At 06:18:50 an unsupported projection prompted visual inspection; the first
image output was truncated, so it was re-observed at 06:21:22. The visible
“Auto recovery initiated” modal was dismissed once by its OK button at
06:22:04; a malformed initial CUA call failed before input. Fresh MCP observation
then confirmed debriefing, and all subsequent menu and battle actions returned
to MCP. Thus this session is **mixed**, not pure MCP. The concrete uncovered gap
is the auto-recovery modal's missing public projection/action. Normal window-
manager closure after the bounded test was confirmed by window inventory.

This is not a full TFTD mission acceptance or campaign completion. The next
priority is the discovered auto-recovery modal gap, not a new synthetic matrix;
rare matrix work and audio optimization remain deferred.
