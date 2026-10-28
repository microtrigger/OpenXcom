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

This qualifies the bounded None/Snap slice in one live UFO test battle. It is
not a completed campaign, a new full-battle acceptance, TFTD live qualification,
or completion of the broader player-equivalent UFO/TFTD interface. Next ordinary
slice: remaining reserve controls or a separately bounded path-preview interface,
with fresh player-visible evidence and normal UI handlers.
