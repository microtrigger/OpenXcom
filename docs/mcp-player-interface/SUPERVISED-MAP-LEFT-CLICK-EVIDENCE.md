<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised ordinary map left-click: finite evidence

Date: 2026-10-12. Base: `4591ec9f7851402d38cb7c544255f3a7c495df93`.
See the [pilot contract](SUPERVISED-MAP-LEFT-CLICK-PILOT.md),
[native fixture](../../tests/map-left-click-control/README.md) and
[machine-readable evidence](supervised-map-left-click-evidence.json).

## Verified normal runs

Both **UFO and TFTD** pass, per game:

- 19 independently reconstructed ordinary-UI/controlled pairs
- 36 interruption, stale-context, consent, replay and consequence worlds
- 38 fresh full-view observations after paired outcomes

Ordinary game timers complete movement. Independent literal position, facing,
TU and energy expectations and semantic save/action/FOV/RNG comparisons cover
cardinal/diagonal and multistep movement, initial turning, selected-tile and
visible-hostile no-ops, visible-friendly selection, zero/exact/partial TU and
energy, loaded-weapon snapshot reserves, kneel reservation, stand-and-walk,
first-contact interruption, a slow walk and a regular door. The physical-display
oracle reconstructs all 320×200 RGB pixels and checks canonical wire colors.
The fixture witnesses real refusal warnings and waits for their natural expiry;
it never hides them, retries the action or forces game timers.

A natural walk longer than 1.5 seconds completes its post-L V, including ordinary
Y press/release confirmation. A deliberately slower walk reaches the fixed
8-second observation deadline, fails the view while still moving, and then
finishes normally with its expected position/TU/energy. Manual right-click
cancels the remaining path normally; disconnection and focus-loss invalidation
do not cancel movement. A real hostile reaction-fire queue is witnessed and the
view fails closed. **Completed projectile damage is not established.**

Further cases cover actual modal presentation, held input, manual/selection/
overlay/load/focus races, camera and pose roundtrips, replacement during V,
shared L/R/D/N command IDs, O/V capability separation, mandatory fresh V and
L-only path-preview rejection. Ordinary R remains usable with previews enabled.

## Transport, builds and modal

- 352 Python checks pass normally and with optimization, with the same two
  expected native-only skips each; 37 are focused left-click tests.
- 12 strict wrapper and seven mutation-setup checks pass.
- All six official MCP SDK synthetic-peer smokes pass with SDK 1.29.0 and
  protocol 2025-11-25. The new real-launcher smoke verifies exact 65,063-byte
  indexed PNG transport, a 2.3-second post-L frame, strict arguments and
  L/R/O/N/D interoperability. This is not a combined SDK-to-live-game demo.
- Observer, cursor, paired-observer, mixed-audio and diagnostic-isolation native
  scripts pass within their existing scope.
- Production, isolated gate and default-OFF Release builds pass with GCC 14.2.
  The default-OFF binary is byte-identical to the preceding milestone and has
  no control symbols or activation strings.
- CMake rejects L without TURN and the left-click gate without its aggregate
  prerequisites. These checks do not claim each missing prerequisite separately.

The actual modal was visually inspected in both games: readable three-line
ordinary-click/cost/reserve/reaction text, Confirm/Cancel buttons, target ring
and preserved bottom HUD. Its finite preview emits a distinct non-PASS verdict;
no screenshots or game assets are published.

## Earlier failure and review corrections

Both first native runs stopped at assertion 390: the slow-timeout fixture's
northeast four-tile target projected to x=304, outside the permitted x≤303.
The fixture now uses three diagonal steps at a slower ordinary timer setting,
with independently adjusted literal TU/energy expectations. Production bounds
were not widened. Both second native runs pass all normal cases. Failed runs
remain failures and are not included in the passing totals.

Before runtime, independent review identified two continuity defects: normal
HUD redraws/busy-to-idle transition retired the movement wait, and the matching
Y key-up after confirmation also retired it. Continuity now permits pending
redraw while final capture remains strict. A narrowly tracked one-use matching
Y release completes that consent gesture; other input still invalidates it.
Neither correction changes game simulation or grants another action.

## Sensitivity and prior controls

All 18 actual-source mutants are detected in both games, each on its first
runtime attempt at the intended assertion, without crashes or watchdogs:

- Missing selector refresh, wrong screen pixel, duplicate click and wrong button
- Free TU, free energy, reserved-TU bypass, first-contact bypass and wrong door cost
- Skipped reaction-fire check and path-preview eligibility bypass
- Disabled camera/pose generations, corrupted wire pixel and unseen-modal approval
- Shortened movement wait, disabled timeout and conflicting-ID acceptance

Each binary changes exactly one production translation unit and substitutes one
object; the other 326 objects are unchanged. An independent verifier checks the
frozen source snapshots, actual compiler/linker dependency closure (919 inputs),
source/object/binary fingerprints and all mutation recipes. Fixture assertions,
packets and adapter responses are not modified. Exact fingerprints and runtime
assertion labels are in the machine-readable evidence.

Existing selection (10 pairs/19 adversarial worlds), stance (12 pairs/19 worlds)
and right-click (15 pairs/29 worlds) gates all pass in both games on the final
left-click-enabled gate binary. These results retain their original bounded scope.

## Limits

This is a bounded practical-play/1 pilot, not full controller acceptance.
Timing equality, full fairness, transient audio/visual history, completed reaction
damage, falling or actor-loss outcomes, sliding doors, reserved-door combinations,
preview-path movement, other terrain combinations and a combined SDK/live-game
left-click demonstration remain unverified. Inventory, attacks, end-turn, safe
batches, broader tactical/geoscape completion and actual token benchmarking are
outside this milestone. The click can legitimately no-op, select an unsupported
actor, be interrupted or make its subsequent view unavailable; no hidden preflight
tries to guarantee arrival or success.
