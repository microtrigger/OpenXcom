<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Inventory LEFT-click orphaned-release correction

**Verified bounded correction in UFO and TFTD. Experimental CLICK remains default OFF.**

## Affected behavior and correction

The LEFT-click pilot published in `7b8954f3b8ab6e197f6578c9b4dd48bc5c959870`
kept a per-inventory bit for a mouse DOWN owned by its local confirmation
surface. This correctly prevented the matching release from passing through
when the proposal was dismissed. However, if that release never arrived,
a later ordinary DOWN did not retire the old bit. Its subsequent UP could be
consumed instead of reaching the normal inventory click handler.

The affected scope is a surviving inventory and the supported mouse buttons
1–8, including after control disconnection. It can cause a later manual click
to do nothing and leave ordinary pressed-button state stale. This is not an
item transfer or rollback. The published implementation has no corresponding
global Y-release latch or cross-state persistence.

The correction clears only the same button's old per-inventory latch when a
new DOWN not owned by the E proposal starts a new gesture. A DOWN still owned
by the E proposal retains its original matching-UP protection. The production
change is seven lines in `StanceController.cpp`, compiled only with the existing
CLICK flag. It adds no tool, permission, gameplay helper or data field.

## Predeclared comparative regression

The isolated `orphan-left-release` fixture uses the same code on both sides:
exact published production and that production plus the seven-line correction.
It presents an E proposal with a right-hand item held, sends a LEFT DOWN on the
confirmation surface, dismisses with Escape, and deliberately omits the mouse
UP. It verifies that dismissal leaves the item and callback counts unchanged
and that the old button latch remains set. It then sends a fresh ordinary
LEFT DOWN/UP on the original hand.

Predeclared assertion 9005 requires exactly one ordinary click and its normal
`invClick` callback. Assertion 9006 requires the selection to be cleared, the
original right-hand owner/slot and TU preserved. Assertion 9007 excludes extra
initialization, ground rearrangement, closing or destruction and requires the
latch to be clear. The expected old failure is specifically 9005 with an ordinary
failure exit; setup failures, unrelated assertions or timeouts are not evidence
of this defect. The fixed side must pass unchanged criteria.

The fixture omits an event deliberately. It does not claim to reproduce an
actual operating-system focus-loss event. No assertion, target or timeout is
changed after observing the comparative outcome. The existing 54 profiles and
their assertion locations remain intact; this adds a 55th profile.

## Results and limits

Both games produced the predeclared comparative result, once per variant:
the published production failed exactly at 9005 with exit 1; the minimal fixed
production passed with exit 0. All four calls used the identical frozen fixture.
There were no retries, endpoint changes or timeout changes.

The fixed production then passed all **54 prior profiles per game: 108/108**.
Thus each game passed the new defect regression separately and its complete
previous CLICK suite. Each core covered 96 worlds, 48 manual/E pairs and
384 complete-frame comparisons across eight natural primer phases. An independent
retained-image audit verified all 2,206 regression PPM hashes/byte counts and all
768 whole-frame comparisons. The four comparative PPMs were also verified; they
show the shared proposal overlay, not a separate final-outcome screenshot.
The new fixture's ordinary callback, selection, ownership, TU and lifecycle
assertions establish its final outcome.

CLICK pure checks pass 35/35 normally and under Python `-O`. The separate
comparison harness passes 23/23 fake-runner checks in both modes, with actual
process launch prohibited in those pure tests. Native results above are separate.

The compared builds use published production versus exactly one seven-line
production correction. Their source, binary, compilation dependencies and
retained outputs remained pinned throughout. Both have CLICK enabled and no
RIGHT control implementation. The archived source applies existing line-ending
attributes to four unused Windows project files; this is not a compiled-source
difference. The [machine-readable evidence](inventory-click-release-fix-evidence.json)
records source/binary/fixture hashes, all four comparative outcomes and all
108 prior-profile outcomes. Public evidence contains metadata only.

Independent source review occurred before runtime and independent raw-result,
image and final-pin verification followed it. Physical cursor positioning was
an operator attestation; occasional window snapshots do not establish continuous
focus. The repeating defect fixture uses LEFT; the shared corrected predicate
covers supported buttons 1–8, but this report does not claim separate reproductions
for each button. No new SDK or model run is claimed by this correction.

Earlier [LEFT-click evidence](SUPERVISED-INVENTORY-CLICK-EVIDENCE.md) remains
historical and did not include this missing-release then new-gesture sequence.
This correction does not retroactively expand it, or rerun its source-fault
sensitivity suite. Full controller and fairness acceptance remain open.
