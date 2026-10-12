<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised inventory Ground evidence

Date: 2026-10-12. Baseline: `0d87e15b329a20279eb3a5060dfba72367743192`.

This evidence covers the default-OFF, locally confirmed ordinary Ground button:
request-bound K pixels, G admission/consent, one ordinary handler invocation,
and mandatory fresh capless I from the same surviving inventory lifetime.
The button rearranges ground slots/stacks and advances or wraps its page.
It is not a read-only page query. No model or token benchmark ran.
Full fairness and full controller acceptance remain unproven.

## Build and isolation

The production build has all native test gates disabled. Actual SDK sessions
used that build. Native positive/adversarial tests and source faults used
separate gate-enabled builds. Earlier controls used a separate regression
build with Ground compiled in and its test gate disabled. The existing
wrappers retain their original runtime activation: inherited activation is
scrubbed and they do not enable Ground.

The all-experiments-OFF binary is byte-identical to the frozen predecessor.
Ground activation/schema/gate strings are absent. A CLOSE-only/Ground-OFF build
contains CLOSE activation/schema but no Ground activation/schema/overlay/gate
strings; no byte-identity claim is made for that build. Invalid Ground-without-
CLOSE and gate-without-prerequisite configurations were rejected.
Source and binary SHA-256 values are in the
[companion machine-readable report](supervised-inventory-ground-evidence.json).

## Python and official SDK synthetic checks

The adapter/client suite ran 462 tests in normal and optimized Python modes:
460 passed and two native-only logger checks were expected skips in each mode.
The 36 Ground-specific tests passed in both modes. The native wrapper/mutator
suite passed 28 pure tests in both modes, and the existing CLOSE wrapper/mutator
suite passed 28 in both modes after its J-only mutation anchor was maintained.
The new synthetic Ground SDK smoke passed in both modes. Eight earlier SDK
synthetic smokes passed in normal mode. These are synthetic protocol checks,
not production-game execution.

## Native ordinary-handler parity

The final frozen gate passed **84/84 executions**, 42 profiles in each game.
Both actual expiry waits ran in each game. Independent retained-file review
verified all 360 physical frames and all 40 exact whole-frame parity pairs.
The amended focused overlay/initial-I run had already passed 4/4.

Each game's core constructs 40 independent worlds: ten manual/G pairs with
maximize-info off and on, yielding 20 comparisons per game. Cases cover empty
and sparse ground, manually noncanonical slot positions, multiple pages,
last-page wrap, exactly-full-page advancement to an empty page, stackable and
nonstackable items, zero TU, primed items and loaded weapons with different
ammunition quantities. Manual worlds receive the actual ordinary button click;
G worlds receive the real request and local-confirmation flow.

The oracle checks complete battle-save serialization and relevant runtime, item owner/
slot/ammunition/TU, simulation RNG state, page/stack cache and explicit ordinary
handler/init/lifetime counts. Ground must call the ordinary handler and arrange
exactly once, with no close, construction, destruction or reinitialization.
Initial I has its own pre-request preservation snapshot, so an initial-I-only
side effect cannot be absorbed into a baseline. K and post-G I are independently
compared with the full physical SDL surface, including cursor and tooltip.

Final manual/G full-frame RGB comparison uses naturally occurring inventory
primer animation phases 1 through 8. No timer is frozen/reset, no forced redraw,
crop or pixel mask is used. Each core retains 180 physical PPMs, 360 across both
games. This measures simulation RNG via RNG::getSeed(); libc/seedless RNG state
is not captured or compared. Equal semantic/frame results are not equal timing.

The 42 fixed profiles additionally cover input consumption, held items, stale
actor/input/reinitialization, focus/display/palette/overlay changes, local
consent presentation and held/queued/modifier protection, both actual 120-second
expiry paths, disconnect, deduplication/conflicts/full ledger, activation gates,
unsupported modes, type-separated authority, Ground visibility/geometry,
failed/refused mandatory I observation, and lost/reopened inventory/parent/save lifetimes.

## Preserved earlier outcomes and oracle correction

The first focused run passed 16/16 profile executions on the earlier gate.
The first complete run produced 82 passes and two failures out of 84. Both
failures were the same overlay-profile assertion, one in each game. All raw
outcomes and frame records were retained; they were not relabeled as passes.

That test canceled an overlay, then itself popped an unrelated State. Ordinary
Game::popState schedules inventory reinitialization and ground arrangement.
The old assertion incorrectly attributed those later ordinary effects to
cancellation. The corrected oracle first proves cancellation has no such effect
before its own pop, then explicitly expects one ordinary init/arrange afterward.
Production code was unchanged. Independent source review agreed with this
attribution. Pre-initial-I preservation was also strengthened, and a 25th
initial-I-only side-effect fault was declared before any mutant runtime.
Earlier build-only revisions and the original 24 unexecuted mutant binaries
remain separate from the final runtime evidence.

## Source-fault sensitivity

The initial 50 runs (25 faults × two games) produced **46 detections at the
predeclared assertions, two survivors and two inconclusive results**. The
wrong-handler fault called ordinary OK and destroyed inventory, reaching the
unexpected-lifetime assertion 611 before the planned assertion 281. Those two
original outcomes remain inconclusive under their original plan.

Source review explained the earlier endpoint: ordinary OK/pop interrupts the
queued Y-keyup; the dispatch check waits for an empty input queue, while the
next loop performs deferred destruction first. A separately predeclared targeted
repeat used the exact same binaries and unmodified native gate. It required
exit 1, assertion 611, and the full fixture signature: command world 1,
WAIT_DISPATCH phase 14, init 1, Ground 0, arrange 2, Close 1, destroyed 1,
shown 1 and dismissed 1. Frame serial was allowed to vary. Generic unexpected
destruction alone could not count. Both new runs met that signature. All 580
original-panel files were hash-guarded unchanged; independent review also
verified both result tuples and all 16 retained repeat frames. The 35 classifier
checks passed in normal and optimized modes, with 31 independent checks. An
initial read-only preflight found a build-object path binding error in the
private driver; it was corrected and rechecked before either new native run.

Thus **24 of 25 distinct recipes have detections in both games across the
50 initial runs plus two separately declared targeted repeats**. This is not
50/50 or 52/52 success, and the first two inconclusive results are not rewritten.
The single-guard stale-reinit fault survived in both games with a clean PASS.
It removes only UI-revision equality. Ordinary InventoryState::init still refreshes
the mouse; SDL_WarpMouse events can independently invalidate the grant.
That is a source-based explanation, not a new causal isolation experiment.
Baseline stale-reinit refusal passes, but individual UI-generation-guard
sensitivity is not established.

All faults are actual production-source edits in disposable builds, never
injected oracle failures. Source, compiler/linker dependency, original object,
plan and binary hashes are checked against frozen inputs. The relevant semantic
assertions/profile are declared before execution. Only an expected behavioral
assertion with the expected exit counts as detection. Compile failures, crashes,
timeouts, watchdogs and unrelated assertions are not detections. A clean PASS
counts as survival. The unseen-consent recipe is explicitly a composite
three-guard fault, not proof of each individual guard's sensitivity.

## Earlier-control regressions

All six earlier controls have passing results in UFO and TFTD: selection,
stance, turn, ordinary map-left-click, inventory open and inventory close.
The initial runner passed 11 of 12 invocations; UFO CLOSE hit its existing
watchdog (assertion 395). That first outcome remains inconclusive, not a pass.
The existing wrapper retained the aggregate failure but discarded the child
stage/profile diagnostics, so its cause is not established. No source, timeout
limit or oracle was changed. An isolated core rerun passed both games; all 252
physical frames and 28 exact manual/C pairs were independently audited. A full
UFO CLOSE rerun with retained per-profile diagnostics then passed all 34 profiles.
TFTD CLOSE had already passed all 34 in the original runner.

## Actual official-SDK to production-game sessions

Both UFO and TFTD passed the complete bounded sequence:
V → B → status → I → K → G → status → I → J → C → status → V → stop.
That is 12 tool calls and one explicit stop per game, 24 calls and 12 canonical
PNGs in total. There were no failed tool calls or action retries. The driver
allowed only one B/G/C each and one operator-selected step at a time; it did not
approve locally, poll automatically or query hidden state. Its 50 pure checks
passed in normal and optimized modes before execution. SDK 1.29.0 negotiated
protocol 2025-11-25. The nonterminating transport was unchanged.

Both sessions used ordinary New Battle/deployment setup. After the opening I,
ordinary local input picked up a visible flare and placed it in a later empty
ground slot. K showed that displaced flare. There was no intervening local input
between K and G, or J and C. Post-G I showed the flare repacked into the first
slot, with the same visible TU. The B/G/C overlays were each read and confirmed
with fresh local input. Final V showed return to the ordinary tactical scene.

- UFO: Martina Bauer, 58 TU, Skyranger/farm, Electro-Flare.
- TFTD: Atte Aaltonen, 58 TU, Triton/underwater Atlantis, Chemical-Flare.

All six images per session were visually inspected. Receipt order, successful
results, canonical PNG structure, bytes and hashes were separately audited.
In each session K differs from post-G I, while post-G I matches the earlier
pre-setup I and the close J image. Image equality alone is not handler proof;
that comes from the separately instrumented native gate.

Local setup/input chronology, consent and post-stop process liveness are
operator attestations: the operator inspected fresh desktop views after SDK
EOF, then closed the games
normally. There is no separate retained event recording or post-stop screenshot
file. Native identity/lifetime enforcement is not independently established by
SDK response fields. Automated terminal manifests deliberately do not assert
visual correctness, native success, identity or process liveness.

## Correction to earlier CLOSE evidence

The published CLOSE report previously said "engine/libc RNG snapshots". Source
review confirms that only simulation RNG was compared; seeding libc during
fixture setup did not measure its subsequent state. The CLOSE report now states
that narrower result explicitly. Its recorded outcomes and historical source
hashes are unchanged. This is a documentation correction, not a new causal test.

## Limits

This slice does not expose item/page enumeration, off-page previews, destination
availability, pathfinding/target queries or hidden eligibility reasons. It does
not implement individual item transfer, reload or prime proposals. The overlay
shows frozen proposal pixels while ordinary primer animation continues below.
A dispatched receipt alone proves neither a page change nor a specific visible
item; the mandatory I must be inspected. Full campaign play, autonomy, timing
fairness and model/token benchmarks are outside this evidence.
