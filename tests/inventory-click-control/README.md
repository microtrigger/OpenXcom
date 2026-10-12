# Finite ordinary inventory LEFT-click gate

This default-OFF local operator fixture tests the bounded H/E inventory click
pilot. It is neither a model benchmark nor a full-fairness verdict. Compile
`EXPERIMENTAL_INVENTORY_CLICK_CONTROL_GATE` and prerequisites; the separate
runtime opt-in is `OPENXCOM_INVENTORY_CLICK_CONTROL_GATE=1`.

`run-inventory-click-control.py` runs 55 finite, isolated profiles for `xcom1`
or `xcom2`. Its output is bounded and child diagnostics stay private. Never run
it alongside another native GUI operator. Pure checks need no game or assets:

```
python3 -m unittest discover -s tests/inventory-click-control -p 'test*.py'
python3 -O -m unittest discover -s tests/inventory-click-control -p 'test*.py'
```

## What the bounded gate checks

`core` reconstructs 96 worlds per game: 24 fixture pairs, each with
maximize-info off and on. Each pair compares genuine SDL pointer motion and
one ordinary LEFT click against one locally confirmed E command. Independent
fixtures cover empty/fixed pickup, hand and ground 1x1/tall pickup, hand-ground,
ground-hand and hand-hand placement, same-slot placement, zero TU, overlap and
bottom-edge refusal, ground stacks and page offset, compatible/incompatible/
already-loaded/insufficient-TU reload, ground-weapon reload, primed pickup,
and pointer-centering at the ground boundary.

The semantic oracle records complete battle save and runtime, simulation RNG,
TU, item ownership/ammunition/fuse/slot, held identity, page and stack cache,
ordinary callback counts, UI text, and relevant pressed-button state. It takes
a separate baseline before the initial I request. Libc RNG state is excluded.
Dispatch must enter ordinary mouseOver then mouseClick, preserve the ordinary
invClick callback, and establish the exact surviving inventory's mandatory I
obligation before the click entry. No init/close/rearrange shortcuts are allowed.

Every observed H/I view is independently checked against the complete physical
SDL display. Final manual/E image parity compares eight whole natural primer
phases after any warning naturally retires. Transient warning frames are
retained at natural joint primer/fade phases; this does not claim exact paired
transient-warning phase alignment. No image masks, timer resets, or forced
redraws are used. A separate, narrower component-composition oracle checks
already-rendered banner/text/button and ring pixels against the physical
screen, including the ordinary cursor, plus exact button hit-testing and
transparent marker-center geometry. It is not a whole-underlay parity claim.

Negative profiles cover held cancellation and manual right-cancel after a
refused placement; consumed button/key/wheel/modifier events; unseen/queued and
held consent; real TTLs and disconnect; stale input/actor/reinit/held selection;
widget actor/mode, visibility, position, other-control overlap and slot geometry;
warning occlusion including a visible crop; focus/display/palette/modal changes;
command-kind separation, receipt-only duplicates/conflicts and shared ledger64;
required-I refusal versus actual I transport-send failure; lost/reopened
inventory, replaced parent/save; and runtime/CLOSE/Ground activation boundaries.
Hidden/moved overlay-child tests retain a preexisting ordinary right/X2 pressed
state and require the invalidating DOWN/UP pair to leave the held item unchanged.

## Orphaned release regression

The additional `orphan-left-release` profile presents an E proposal while an
item is held, owns a LEFT DOWN, cancels with Escape and deliberately omits UP.
A new ordinary LEFT DOWN/UP must then reach the standard inventory click and
callback exactly once, restore the item to its original hand without TU cost,
and avoid init, rearrangement or closing. This models an omitted event rather
than an actual operating-system focus-loss reproduction. The original 54
profiles and their numbered assertions remain unchanged. See the separate
[correction evidence](../../docs/mcp-player-interface/INVENTORY-CLICK-RELEASE-FIX-EVIDENCE.md).

## Recorded native results

The frozen r3 baseline passed all 54 profiles in each game: **108/108** with
unchanged source, binary and compilation dependencies. Each game's core ran
96 worlds and passed 384 unmasked whole-frame manual/E comparisons across the
eight natural primer phases. This is finite native fixture evidence, separate
from the real official-SDK smoke and from any model or full-fairness evaluation.

The predeclared 35 production-source faults were each run once per game after
that baseline. Results were identical in both games: **32 detected, 2 survived,
1 inconclusive**, or **64 detected / 4 survived / 2 inconclusive** across 70
runs. Every run retained unchanged-input verification; independent raw-output
review agreed with all classifications. No endpoints were expanded or runs
repeated to improve those results.

- `close-prerequisite-bypass` survived. Other read-admission and binding checks
  still require CLOSE activation; this is a source-based explanation, not a
  runtime trace identifying the particular rejecting guard.
- `stale-reinit` survived. Ordinary `init()` refreshes the mouse through SDL
  warps, so input invalidation can revoke the context independently of the
  removed UI-generation comparison. The fixture proves ordinary reinit refusal,
  but does not isolate that comparison's sensitivity.
- `click-kind-confusion` was inconclusive. It reached an unexpected click-banner
  geometry assertion (745), rather than the predeclared receipt-kind assertion
  (233). An ordinary failure exit at that undeclared endpoint is not detection.

Earlier failed revisions remain retained: initialized primer phase zero,
precursor motion before the intended invalidating button edge, and pointer
pixels overdrawn by the banner exposed test-oracle assumptions. The final gate
corrects those assumptions without timer forcing or image masks. A separate
earlier TFTD result was excluded because original frozen inputs changed during
its run; the full r3 baseline above was rerun with verified unchanged inputs.

## Sensitivity and evidence limits

`make-inventory-click-mutant.py` predeclares 35 real production-source faults.
It requires a separate disposable checkout and rejects aliases, duplicate
anchors, repeated mutations and combined faults. It never mutates gate/oracle
code. The unseen-consent recipe is explicitly composite (three independent
presentation guards), not an individual-guard sensitivity claim.

Before any runtime, freeze the source, binary, dependencies and exact relevant
assertion/profile mapping. A source edit or successful compile is setup only.
Only the mapped semantic assertion with ordinary failure exit is a detection.
Crashes, timeouts, watchdogs, launch/build failures, and unrelated assertions
are inconclusive. Redundant guards can legitimately let a fault survive.
Retain failed runs and source revisions; report detected/survived/inconclusive
separately. Native execution and official-SDK nongate evidence must be reported
separately from these pure/source-bound checks.
