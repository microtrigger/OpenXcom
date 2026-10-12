<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised inventory RIGHT: bounded evidence

See the [control contract](SUPERVISED-INVENTORY-RIGHT-CLICK-PILOT.md) and
[machine-readable evidence](supervised-inventory-right-click-evidence.json).

Evidence snapshot: 2026-10-12T17:35:04.848794+00:00.

Status: Completed bounded evidence. All planned finite runs finished. The source-fault phase was not accepted and full planned source-fault sensitivity remains unsatisfied.

## Current evidence

- Python suite: 546 tests in each of normal and optimized modes, with two existing native-only diagnostic skips in each mode. The focused RIGHT suite has 41 tests in each mode.
- Synthetic SDK: 17 RIGHT scenarios in each mode; seven earlier supervised SDK smoke families also pass in each mode. These use SDK 1.29.0 and protocol 2025-11-25. They do not establish native UI behavior.
- Default catalogue: all 2,048 prior flag combinations produce the same serialized tool definitions as historical commit 7b8954. RIGHT defaults to disabled. This compares the actual historical adapter, rather than only comparing current default and current explicit-false settings.
- r3 gate preparation: 23 pure tests in each mode and a C++ syntax check pass. The separate actual-SDK driver has 84 pure checks in each mode with real process launch guarded.
- Focused native r3: all 16 cells pass, eight selected profiles in each game. Full baseline status is stated separately below.
- Full native r3 baseline: Completed 92 of 92: 92 passed, 0 failed, 0 detected, 0 survived, 0 inconclusive. Phase acceptance: accepted. Terminal seal and recursive evidence pins verified; 92 raw/decoded pairs verified.
- Source-fault sensitivity: Completed 50 of 50: 0 passed, 0 failed, 46 detected, 2 survived, 2 inconclusive. Phase acceptance: not accepted. Terminal seal and recursive evidence pins verified; 50 raw/decoded pairs verified.
- Earlier-control native regressions: Completed 298 of 298: 298 passed, 0 failed, 0 inconclusive. Phase acceptance: accepted. Terminal seal and recursive evidence pins verified; 298 raw/decoded pairs verified.
- Actual SDK smoke: completed in UFO and TFTD, each with 20 explicit tool calls, one local stop and ten returned PNGs. Machine evidence and separately labelled manual UI observations were reviewed; no independent native event trace was recorded.

The build matrix compiles full control support, RIGHT without CLICK or Ground, CLOSE-only, and default-OFF variants. Configuration without the required CLOSE chain is rejected. Compilation is not runtime evidence.

## Bounded control contract

RIGHT is experimental and disabled by default. It requires the CLOSE prerequisite chain, while CLICK and Ground remain independent options. A context request is exactly 33 bytes and an action request exactly 103 bytes. The action accepts only capability, epoch, command identifier, and strict integer coordinates inside the declared hand or ground regions of the canonical 320 by 200 inventory view.

Local confirmation visibly states both ordinary outcomes: cancel a held selection, otherwise close inventory with normal gravity, lighting and field-of-view restoration. Confirmation dispatches one ordinary inventory hover and RIGHT callback, including its normal callback tail. This scope is ordinary TU inventory, without prebattle/base or prime/deactivate support. The interface supplies no held-item, predicted-outcome, eligibility or next-view metadata. A receipt does not select a branch for the caller.

An unresolved observation obligation is installed before dispatch. The actual witnessed transition resolves it once: exact surviving inventory requires I; an ordinary close requires V from the exact original parent after normal restoration. The caller chooses I or V explicitly. A wrong choice remains fail-closed, and the client makes no fallback probe. Only successful transmission of the required actual image satisfies the obligation. Status queries, duplicates, unrelated contexts, failed sends, lost destinations, reopening and unexpected lineage changes do not repair it.

Decline, expiry and disconnect do not cancel a held item automatically. CLOSE and Ground still refuse held selections. Local UI remains the source of consent and ordinary inventory behavior. The shared bounded command ledger keeps duplicates status-only.

## Retained revision history

r1 completed 16 focused cells: 12 passed and four failed. Core assertion 278 failed in both games. This is a captured context-shape failure. Activated-grenade warning refusal is a source-based inference, because the complete unavailable payload was not retained. Nested-transition assertion 102 also failed in both games: the fixture mistook an ordinary restoration mouse-motion hover for the synthetic RIGHT release used by dispatch.

r2 changed only fixture preparation and event discrimination. It waited for the primed-pickup warning to fade naturally and kept the prehandler unresolved-barrier assertion on the actual synthetic dispatch event. Its 16 focused cells produced 14 passes and two real whole-frame failures at assertion 105. UFO worlds 14/15 and TFTD worlds 38/39 differ by exactly nine primer pixels at x=1..3, y=153..155 in the compared phase. These failures remain failed evidence.

Source inspection explains the phase-label ambiguity: ordinary selection cancellation redraws item surfaces without advancing the primer animation phase label. The next natural timer redraw can therefore differ from a frame bearing the same stale phase label. This explains a possible transient mismatch; it does not erase the observed pixel difference.

r3 changes measurement only. It retains the actual returned post-view frame, which may already be settled, and separately retains the actual collection-entry frame and phase. In both manual and controlled paths it then requires an observed natural phase advance before collecting all eight whole phases. There are no pixel masks, forced draws, timer resets or elapsed-time matching. The resulting scope is settled-natural-phase parity. It does not establish equality at arbitrary transient moments or claim to capture the literal first post-dispatch transient. Returned I/U/V physical-image checks and callback/semantic assertions remain in place.

Production code is unchanged across these fixture revisions. Failed r1 and r2 results are retained, with no retrospective endpoint reassignment or conversion to passes.

## Native acceptance and sensitivity scope

The full baseline declares 46 profiles per game. The core profile covers ordinary manual RIGHT and supervised X across hand and ground selections, singleton and stacked items, page offsets, tall and primed items, refused placement with a held selection, and both inventory information modes. Semantic comparisons cover save/runtime state, TU, ownership, ammunition, fuse, held selection, page, stack, callback and text state, plus simulation RNG state. Empty-selection close uses the ordinary restoration path. Physical comparison covers the whole canonical view after the documented natural phase condition.

Barrier and input profiles cover exact I/V, wrong-family requests, send failure, lost original destinations, reopening, save notifications, synchronous nested transitions, duplicate/status behavior, stale owned releases and later ordinary input pairs. These are finite fixtures, not unrestricted gameplay coverage. The save-notification profile invokes the real continuity hook; it does not destructively replace a save during the callback.

Twenty-five actual-source mutation recipes and their exact profile/assertion endpoints were declared before runtime, for 50 game-specific cells. A detected fault must reach its original mapped failure; a clean pass is a survivor; other outcomes are inconclusive. A phase can finish with all cells conclusively classified while still containing survivors. The report must retain those distinctions.

Source-fault acceptance was not met. The retained non-detections are:

- UFO: close-prerequisite-bypass in close-disabled (mapped assertion 112): survived; ordinary profile PASS.

- UFO: duplicate-right-click in core (mapped assertion 101): inconclusive; mutant-reported segmentation fault in native stderr, empty native stdout and wrapper-generated generic SUITE FAIL; no mapped assertion observed.

- TFTD: close-prerequisite-bypass in close-disabled (mapped assertion 112): survived; ordinary profile PASS.

- TFTD: duplicate-right-click in core (mapped assertion 101): inconclusive; mutant-reported segmentation fault in native stderr, empty native stdout and wrapper-generated generic SUITE FAIL; no mapped assertion observed.

Original classifications and endpoints are retained. A generic failure is not converted into a mapped detection, and a survivor is not called detected. The reported crashes are in the deliberately mutated duplicate-dispatch binaries; no crash in normal production is claimed.

Separate source-only interpretation: the prerequisite recipe changes the constructor conjunction while other CLOSE admission and validation checks remain unchanged. Those checks are consistent with the survivor; the run does not establish isolated sensitivity to that constructor condition. Repeating the ordinary RIGHT callback can repeat a state-pop transition against the next stack entry, which is a plausible explanation for the deliberate mutant crash. No crash stack was retained, so the exact crash site is unproved. The generic crash-handler suggestion about mod data does not establish a mod defect. No additional causal runtime experiments were performed, and none of these interpretations changes the 46/2/2 classifications.

## Binary and evidence lineage

The RIGHT-only and full-control r3 gate binaries were built from the frozen r3 source snapshot. Final publication documentation was refreshed after verification. Of 1,387 existing frozen entries, 1,381 working files match exactly, with six documentation differences. Existing non-documentation working-file/build-input pins match and compiled production inputs are unchanged. The reviewed Git comparison has 1,377 exact blob matches: four inherited Windows project files additionally differ between checkout and Git only by CRLF conversion. These four Git blobs are unchanged from the parent revision. No whole-tree Git byte-identity claim is made. The four source-relative filenames and comparison hashes are recorded in the companion JSON.

Binary SHA-256 values are also recorded in the companion JSON. The production binary used for both actual SDK smokes was compiled from r2 and is reused unchanged. Its 682 production source files are recorded as identical to r3; this is not a claim that the production binary was rebuilt from r3. Both SDK records identify that production binary rather than a gate binary.

The machine-readable evidence records source, fixture, mapping, binary and result digests. Terminal native results require their completion seal and recursive retained input/output checks. A summary file alone cannot promote a pending phase. Evidence generation does not launch the game.

After completed baseline validation, five generated bytecode files were added to the frozen source snapshot. They were preserved separately with their bytes and timestamps intact; the original source files and runtime results were unchanged. The exact recursive source entry set and all completed evidence pins were revalidated after restoration. Continuous post-baseline immutability is not claimed, and no runtime result was replaced or rerun for this restoration.

## Earlier tests and maintenance limits

Pure maintenance repaired five earlier mutation helpers and one OPEN test anchor. The five maintained suites pass 108 checks per mode. Historical anchor failures were already present in the retained earlier baseline for OPEN, map-left, stance and turn. These repairs do not establish any new historical native mutation kills. The 298 current earlier-control native cells are assessed separately from helper maintenance; their status is recorded above.

Earlier Python invocation failures are preserved. One invocation used a location incompatible with an older relative-file test; another exposed the existing delayed-extra-receipt scheduling race. A deterministic old-versus-revised comparison showed the same delayed-packet behavior and closure before the next send once the extra packet was queued. No RIGHT-specific causal claim is made from that observation.

## Actual SDK evidence and remaining limits

Both actual-SDK smokes preserve the exact fixed 20-call sequence followed by an explicit local stop. Each has ten canonical 320 by 200 PNGs with verified lengths, SHA-256 values, CRCs and format. The chain includes pickup, a held RIGHT proposal with a normal unavailable receipt followed by explicit I, another held RIGHT with a dispatched receipt followed by explicit I, and unheld RIGHT with a dispatched receipt followed by explicit V. No receipt selected the observation family and no fallback probe occurred. EOF sent no termination signal.

Separate manual attestations record visible local confirmation, actual decline with held-selection preservation, held cancellation while inventory remained open, ordinary close, and post-EOF responsiveness through an ordinary Options interaction. Independent review inspected all 20 returned images and five supplemental images per game alongside those attestations. Those are human-style visual observations, not an independent native event trace or proof of exact internal object identity. A normal unavailable receipt alone does not prove decline, and endpoint images or dispatch receipts alone do not prove cancellation. The driver deliberately leaves its native-outcome and liveness verification flags false.

The UFO bytecode alteration described above occurred after baseline and was restored before gameplay calls. TFTD produced no additional frozen-source entries. A mistaken TFTD cleanup click opened a Save dialog, which was cancelled without saving before ordinary cleanup continued. This did not add or repeat an SDK gameplay call and remains disclosed in the manual record.

The native setup includes an operator-attested physical-pointer precondition. That does not establish continuous focus or actual operating-system focus loss. Missing-release regressions model omission of the release event. Exact allocator same-address reuse is unproved. Simulation RNG comparisons do not cover libc rand().

An optimized gate build retained a vector erase/remove compiler warning. Independent source review judged it an apparent optimizer false positive; no sanitizer proof was run. This is a gate-build limitation, not a claim about a production warning.

Timing equality, full gameplay fairness, campaign readiness, model-driven play and token efficiency are unproved. No model calls or token benchmark were performed.

## License notices

Source contributions are licensed under GPL-3.0-or-later. Existing upstream copyright and license notices must be preserved. Optional external SDK licensing remains separate. This report contains text and evidence digests only.
