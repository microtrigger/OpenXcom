<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised inventory left click: evidence and limits

**Bounded CLICK verification complete, with qualified prior-control regressions.**
See the [bounded pilot contract](SUPERVISED-INVENTORY-CLICK-PILOT.md).
Full controller acceptance and the complete fairness gate remain open.

## Revision and isolation

Source baseline: `e6350878fbc50b3a9e908f3044eafdc574989803`, plus this contribution.
The reviewed production executable used for both UFO and TFTD SDK sequences has
SHA-256 `257a0dd2dad974b396f59987e5d507905d301d94b7b3d42bddd6d7e5e65ead9b`.
It contains no native test gate. CLICK+Ground, CLICK without Ground, CLOSE-only
and default-OFF builds pass; the missing-CLOSE configuration is rejected.
Static checks distinguish activation strings and test-gate symbols.

## Python and synthetic transport

- Full Python discovery: 505 methods in each normal/optimized mode, comprising
  503 passes and two expected native-only skips.
- The new 43-method suite covers strict coordinates, all 64,000 canonical pointer
  locations, authority-kind separation, the shared command ledger, duplicate
  behavior, mandatory I and failures.
- All ten official-SDK synthetic smoke scripts pass under normal and optimized
  invocation. Only the new click smoke explicitly propagates optimization to
  its launcher/producer subprocesses; prior scripts are not claimed to do so.
- All 1,024 prior flag combinations retain exactly the same tool definitions
  with CLICK disabled. Seven separately defined Python source faults are
  detected through relevant assertion failures.
- The private explicitly stepped live driver passes 56 pure methods per mode,
  including filesystem-inbox and fake-SDK run-loop checks. No native process is
  launched by those tests. Its nonterminating transport is byte-identical to
  the previous reviewed Ground driver.

Initial full discovery used an incorrect working directory and failed an
existing cwd-sensitive subprocess test. The documented invocation passes;
that failed invocation is retained. A private driver-test setup path error was
corrected before runtime; the earlier failed attempt remains recorded.

## Source review corrections before runtime

Independent review found that natural animation made a simple changed-frame
check insufficient to prove E overlay presentation. The gate now checks the
actual physical composition of its frame, text, buttons and marker, including
the ordinary cursor.

Review also found an invalid-presentation input edge: ownership must persist
when a confirmation child becomes hidden/moved, so the invalidating event cannot
reach the held item. E now also retains exact-button release consumption after
an owned DOWN, including retirement/disconnect and stale underlying pressed bits.
Normal mouse confirmation remains routed to a valid owned overlay. These were
source-review corrections before runtime, not mutation detections.

## Initial native outcomes retained

The first seven focused profiles in each game produced one pass and thirteen
failures overall. The failures exposed two fixture/oracle errors:

1. The inventory initially has natural animation phase 0 before its first timer
   tick; frame retention and final-phase collection had prematurely required
   phases 1–8. Initial/transient phase 0 may now be retained without resetting
   timers. It never counts toward the eight strict final paired phases.
2. The invalid-child test helper queued motion before its intended DOWN/UP.
   That motion correctly retired and consumed the overlay; the later DOWN was
   then ordinary manual input. The intended owned-edge fixture now injects its
   immediate DOWN/UP without that unrelated precursor motion. Production is
   unchanged by these two corrections.

Both initial source/binary revisions, raw results and source-based explanations
are retained. The second focused UFO run passed six of seven profiles, including all 96 core
worlds (48 manual/E pairs). Its domain profile exposed a further composition
oracle omission: at target (31,110), the ring reaches below the hand region and
is normally overdrawn by the later banner. The oracle had omitted that layer.
The target is inferred from the fixed sequence and counters; the failing frame
was not retained. The next revision records the first overlay before assertions,
composes all later components at ring pixels, and still requires exposed marker
pixels. No image region is ignored and production remains unchanged.

The second TFTD core emitted native PASS, but retention rejected original build
dependency drift after the gate was edited during that run. That result is
provenance-failed, not verified. Source/dependency edits were paused throughout the
subsequent frozen runs. No initial failure is relabeled as a pass or a behavioral
source-fault detection. The corrected complete run below passed; these earlier outcomes remain separate.

## Actual official-SDK paths in both games

One production run in each game completed 25 explicit tool calls and one
explicit stop: V/B/status/I; four H/E/status/I blocks; a manual-cancellation
checkpoint I; J/C/status/V. Every proposal had separate local consent. There
were **50 tool calls, two stop receipts, 26 exact SDK PNGs, eight physical E
overlay images and two post-stop physical images**. No action was retried.

| Game | Visible soldier | Displayed weapon | Ground pointer | TU before / after |
|---|---|---|---|---|
| UFO | Martina Bauer | Heavy Laser | (96,176) | 54 / 52 |
| TFTD | Atte Aaltonen | Disruptor Pulse Launcher | (208,176) | 54 / 52 |

In both runs the first pickup at (16,87) and separately confirmed ground click
moved the weapon out of the right hand into a visibly empty ground location.
A further pickup and attempted drop at (144,87), on the occupied left hand,
left it held without further TU loss. One ordinary manual right-click cancelled
that selection; checkpoint I showed the weapon back on ground and the unchanged
Electro-Flare (UFO) or Chemical-Flare (TFTD) in the left hand. J/C/status/V then
returned to the tactical view with TU 52.

The initial UFO visual guess of rifle was corrected by the displayed Heavy
Laser label. No transient refusal-warning message was captured in either live
run; there is no claim to complete warning history. The two-click transfer is
not inferred from a dispatch receipt alone: before/after inventory views show
its visible result. Held state during the refused drop is interpreted jointly
with its changed sprite position and subsequent ordinary cancellation result.

All SDK image hashes/byte counts and the exact 25-call receipt sequences were
checked. Physical E images show all three warning-text lines, numeric target,
local buttons, the displayed soldier name and an unobscured target. The live
runs witness the lower banner band; alternate-band coverage belongs to the
separate native fixtures.

The driver leaves visual outcome, held preservation, consent chronology and
native identity unverified. The separate operator review supplies visual
observations and input attestations; it is not an independently replayable event
trace. Physical post-stop imagery supports the observed visible window, not a
complete process-lifecycle proof. Each disposable game was then closed through
ordinary game UI. The private driver issues no termination signal, automatic
consent, action retry, status poll or hidden-state query. Native results and source-fault sensitivity are recorded separately below.

No model/token benchmark, timing-equivalence, libc RNG, full controller or full
fairness result is claimed. Only simulation RNG state is available to the native
fixture comparisons. No game assets, runtime images, native executables,
credentials or private execution details are part of this source contribution.

## Frozen native acceptance

The final revision passed **54 profiles per game, 108/108 overall**, with all
retained source, build-dependency and runner inputs unchanged. Its core covers
24 fixture types at both maximize-info settings: 96 worlds / 48 manual-versus-E
pairs per game. Comparisons include full battle-save/runtime state, ownership,
held selection, TU, ammunition, ground-page/stack state and simulation RNG.
Eight naturally reached primer-animation phases are compared without timer
reset or image masks: **384 complete physical-frame comparisons per game**.
All 2,204 retained PPM hashes and byte counts were independently checked
(1,104 UFO; 1,100 TFTD), including all 768 paired whole-frame comparisons.
Transient warning frames are retained, but exact paired warning-phase timing
is not claimed. Both games include two genuine 120.1-second expiry profiles.

The gate also exercises strict pointer domains, held-item preservation,
mandatory successful I transmission (including real send failure), ledger and
cross-kind refusal, stale contexts, invalid/hidden confirmation children,
paired input-release consumption, cancellation, disconnect and overlay bands.
The preceding targeted corrected run passed 7/7; it is distinct from the full
108-profile acceptance run.

## Native source-fault sensitivity and limits

Thirty-five predeclared production-source fault recipes were built into
separate executables with distinct verified hashes and run once per game.
The 70-run result is **64 intended detections, four survivors and two
inconclusive outcomes**; each game has 32 detections, two survivors and one
inconclusive outcome. All 70 raw labels, exit codes, declared endpoint mappings
and unchanged-input receipts were independently checked, with 160 run-long
input pins still matching. No endpoint mapping was changed after runtime.

- `close-prerequisite-bypass` survives in both games. Source inspection shows
  other admission/binding guards still require CLOSE. This is a redundant-guard
  explanation, not an additional executed causal experiment.
- `stale-reinit` survives in both games. Ordinary init queues mouse-refresh
  events, and event-loop input invalidation can revoke H even with its UI-revision
  comparison removed. The retained evidence does not identify which remaining guard
  fired; isolated sensitivity of that revision comparison is unproved.
- `click-kind-confusion` fails early at overlay assertion 745 in both games,
  instead of its predeclared receipt-refusal assertion 233. Those two runs remain
  inconclusive, even though the mismatch is visible to a different oracle.

An earlier build-metadata attempt was rejected before runtime because duplicate
metadata keys recorded the baseline hash instead of each mutant hash. The
helper was corrected and independently tested, then a separately retained set
was rebuilt and verified. That artifact failure is not a behavioral detection.
No production guard or test criterion was weakened to obtain a higher count.

## Prior-control regressions

CLICK is compiled into the new regression binaries; the unchanged wrappers
scrub and leave runtime CLICK disabled. The initial 14 aggregates passed eight:
selection, stance, CLOSE (34 profiles) and Ground (42 profiles) in each game.
Turn, map-left-click and inventory-open failed in both games at assertions
284, 317 and 224 respectively. The public wrappers did not retain raw native
output. These six initial failures remain recorded, not passes.

A separately frozen 12-cell diagnostic matrix compared the preceding recorded
Ground executable and the new executable for those three gates in both games.
All 12 failed at the same respective assertions, with unchanged inputs and
retained raw output. The preceding executable matches its recorded hash; its
older source lineage has a partial source manifest, not a complete dependency
archive. The shared failures do not by themselves identify a cause.

Source inspection identified a fixture precondition worth testing: synthetic
motion events do not reposition the physical pointer. After ordinary dialog
pop, the engine initializes the restored state from actual mouse coordinates.
A pointer over the HUD or edge can therefore alter observation readiness.
A second, separately predeclared 12-cell matrix uses the same binaries,
wrappers, timeouts and criteria, with the physical pointer first placed at
canonical map coordinate (160,50). Before/after captures and window inventories
support operator attestations; they are not a continuous SDL input/focus trace
or proof of the particular internal rejection branch. The second matrix also
has explicit inter-run checkpoints/pauses, so it is not a strict single-variable
causal experiment. All 12 cells pass, with all frozen inputs unchanged and all
24 before/after evidence checkpoints retained. Both executable versions now
pass turn, map-left-click and all 14 inventory-open profiles in each game.
Read-only mid-run observations separately confirm the expected focused native
window geometry in UFO and TFTD; one earlier capture missed the live window and
is not counted as native-focus evidence.

Taken together, all seven prior controls have passing results in both games,
qualified by this physical-pointer/display precondition. The initial six
failures and first 12-cell failed matrix are preserved. This is a documented
fixture/profile boundary, not a source fix, a random-flake dismissal, or proof
of a particular internal guard. No production or gate source was changed.
