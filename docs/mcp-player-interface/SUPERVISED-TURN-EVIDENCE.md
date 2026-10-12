<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised ordinary right-click: finite evidence

Date: 2026-10-12. Base: `83765f78ca936efc7ff73f662ecb0cb411e4e224`.
See the [pilot contract](SUPERVISED-TURN-PILOT.md),
[native fixture](../../tests/turn-control/README.md) and
[machine-readable evidence](supervised-turn-evidence.json).

## Verified normal runs

The frozen native gate passes in **both UFO and TFTD**, per game:

- 15 independently reconstructed ordinary-UI/controlled pairs
- 29 interruption, stale-context, confirmation and replay worlds
- 30 fresh full-view observations after paired outcomes

The ordinary game timers complete actions. Literal expected facing/TU checks and
semantic save/action/FOV/RNG comparisons cover clockwise/counterclockwise turns,
wraparound, already-facing no-op, zero/exact/partial TU, snapshot and kneel
reservations, first-contact interruption and a real regular door. The independent
physical-display oracle reconstructs all 320×200 RGB pixels and verifies the
canonical wire palette/indices. Receipt alone never establishes completion.

The gate witnesses ordinary warning visibility on TU/reservation refusals, then
waits for normal warning expiry before requesting the post-outcome view. It does
not hide warnings, force timers or retry actions. The production client can still
fail closed if it requests its view while that warning is visible.

Adversarial cases include selection/manual/focus/load/context races, held or
pre-presentation confirmation input, camera and pose changes away and back,
ordinary animation retaining a valid grant, cross-action ledger/type separation,
mandatory post-R V, disconnection and a state replacement after V was admitted
during a natural busy turn. The following stable scene can be freshly observed.

Existing selection (10 pairs/19 adversarial) and stance (12 pairs/19 adversarial)
gates also pass in both games on the final turn-enabled gate binary.

## Transport and builds

- 315 Python checks pass normally and with optimization; each run has the same
  two expected native-only skips. This includes 41 focused turn checks.
- Eight wrapper and four mutation-setup tests pass.
- The new official MCP SDK synthetic-peer smoke and all four existing smokes pass
  with SDK 1.29.0, protocol 2025-11-25. This is not a combined live-game turn demo.
- A canonical 256-color noisy image uses 129,832 native JSON bytes, 65,063 indexed
  PNG bytes and 87,509 complete MCP JSON bytes. The 131,072-byte limits are unchanged.
- Observer, cursor, paired-observer, diagnostic-isolation and mixed-audio native
  scripts pass. These retain their original bounded scope.
- Production turn-enabled, isolated gate and default-OFF Release builds pass with
  GCC 14.2. The default-OFF binary is byte-identical to the preceding
  selection milestone and has no control symbols/activation strings.
- CMake rejects turn without stance, and the turn gate without its required
  turn/selection/stance-gate prerequisites.

The actual modal was separately inspected in both games: three readable lines,
Confirm/Cancel buttons, a visible target ring and intact bottom HUD. The finite
preview never reports a runtime gate PASS; no screenshots/assets are published.

## Failed attempts and corrections

No earlier failed attempt is counted as a pass:

1. Both first runs stopped at the fixture's coordinate-bound assertion: a
   southeast two-tile target projected to y=128, outside the permitted y≤127.
   The fixture now uses adjacent targets. Production bounds were not widened.
2. Both second runs rejected the shape of a post-outcome context response.
3. A diagnostic-only third run localized that response to the zero-TU case in
   both games. A real warning remained visible after the action queue became
   idle. The fixture now witnesses the warning and its natural disappearance.
4. Both fourth runs pass all final normal cases.

Before runtime, review also caught the map's pending-redraw flag incorrectly
invalidating a grant during harmless animation. Ongoing map binding now checks
visibility/crop/geometry separately from fresh final-frame capture. Review of
lifecycle handling additionally corrected fresh V admission after an earlier
state had been replaced, while preserving rejection of replacement during an
outstanding V. These fixes do not loosen normal gameplay validation.

## Sensitivity controls

All nine actual-source mutant binaries are detected in both games, each on its
first runtime attempt at its intended assertion, with no crashes or watchdogs:

| Fault | Intended detection |
| --- | --- |
| Missing selector refresh | Wrong actual normal-action target |
| Wrong requested pixel | Wrong actual normal-action target |
| Duplicate normal turn | Two queued native turn states |
| Free turn | Independent expected TU cost |
| Reserved-TU bypass | Independent expected facing/TU |
| Disabled camera generation | Stale roundtrip grant unexpectedly admitted |
| Disabled pose generation | Stale roundtrip grant unexpectedly admitted |
| Approval before modal presentation | Forbidden handler callback |
| Corrupted serialized image pixel | Independent physical/canonical image oracle |

Each binary replaces one compiled translation unit and retains 325 other frozen
objects. Compiler/linker dependencies, flags and source inputs are verified;
no assertions or fixture outcomes are weakened in mutant copies. The aggregate
records all 18 runs, intended assertion labels and binary hashes. A separate
pending-V lifecycle source mutant remains outside this evidence.

## Limits

This is finite `practical-play/1` evidence, not completed Stage 5 or full controller
acceptance. Sliding/UFO doors, reserved-door refusal, broader battle profiles and
an independent pending-V lifecycle source mutation are not covered. No timing
isolation, physical audibility, audio/video synchronization, live SDK-to-game turn
demonstration, model performance or token savings are claimed. Movement, inventory,
attacks and end-turn remain separate work. No game resources or new binaries are
included in this change.
