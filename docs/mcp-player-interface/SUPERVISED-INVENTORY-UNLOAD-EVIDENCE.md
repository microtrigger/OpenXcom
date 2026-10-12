<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised inventory Unload: bounded evidence

The experimental, default-OFF Unload slice completes the ordinary visible-button
route in stock UFO and TFTD tactical TU inventory. Corrected focused checks pass
28/28 and full checks pass 124/124. All 390 prior-control checks pass. Each actual
SDK/game path completes 24 calls plus explicit stop. The fixed fault matrix has
**70 mapped detections, two survivors and zero inconclusive outcomes** across
36 mutations × two games. Complete fault sensitivity is not established.

Earlier results remain part of the evidence: the first full run passed 122/124
and failed both palette-fixture cases. The corrected frozen revision is a separate
attempt. Neither those failures nor the two surviving faults are renamed as passes.

The [pilot contract](SUPERVISED-INVENTORY-UNLOAD-PILOT.md),
[native gate](../../tests/inventory-unload-control/README.md) and
[machine-readable evidence](SUPERVISED-INVENTORY-UNLOAD-EVIDENCE.json) define the
bounded result. This is not full-fairness acceptance or a completed gameplay campaign.

## Implemented boundary

- Q returns presented 320×200 pixels plus opaque Unload-only authority. F accepts
  epoch, capability and command ID, without item, ammunition, held-state,
  coordinates, affordability, fit, outcome or approval arguments.
- Fresh local confirmation invokes unmodified LEFT DOWN/UP at stock button point
  `(304,44)`, through the complete ordinary surface-dispatch order. The inventory
  hover/click/stats route precedes the button callback. No direct `unload()` shortcut
  substitutes for that route; the physical confirmation release remains consumed.
- Before DOWN, an unsatisfied exact-I obligation has no destination. State/parent/save
  continuity is rechecked before UP and before binding the surviving inventory.
  Lost lifetime leaves the obligation unsatisfied and does not touch destroyed state.
- Every dispatched no-op, refusal or successful attempt requires a fresh capless I.
  Only successful transmission of that presented inventory image clears the barrier.
  Status, duplicate commands, failed sends, reopening and V do not substitute for I.
- Cancellation, expiry and disconnect preserve the held selection. CLOSE and Ground
  still refuse held selections. Independent CLICK/RIGHT/Ground activation stays optional;
  Unload requires the CLOSE prerequisite chain and its own explicit activation.
- The stock outcomes remain ordinary: no-op, no-ammunition/both-hands-empty/TU refusal,
  or eight-TU unload with weapon in the right hand and detachable ammunition in the
  left. Zero-round detachable ammunition is not excluded. No hidden eligibility probe,
  added fit/reserve preflight, rollback or automatic pickup is provided.

Confirmation bands avoid adding coverage over the held-selection rectangle,
soldier-name region and Unload button. Ordinary animation continues; this does not
prove that preexisting sprites never cover those regions. Arbitrary modded item
sizes are outside the stock fixed-point routing evidence.

## Frozen revisions and retained palette failure

Both freezes contain 1,402 source files. The base is
`641c639e9087cc76770a6e9efa9b33baef1e72a9`; builds use GCC 14.2.0.

- r1 source digest: `08a631d2d7bb1133fe5906e42d5ff68484b4fa8d45d1a361ce4e3c56cfae8b66`.
  Focused: 28/28 passed, 2,238 retained frames and 768 settled whole-image comparisons.
  Full: 122 passed, two failed, 2,294 frames and 768 settled comparisons. Each game’s
  palette profile failed STALE116. The independent decision was HOLD.
- r2 source digest: `6ee35bf2e16b02805a6e74d21551db0876f3343df8296cd81d12be8a1ea97fe0`.
  Only the palette gate fixture and its source test changed between these freezes;
  the production implementation did not change.
- The initial fixture changed an intermediate surface palette, without establishing
  the physical Screen palette condition. The corrected fixture uses normal display
  reset plus the Screen palette path and checks unsupported presentation explicitly
  at SUPPORT9000 before F. STALE116 remains the refusal/no-dispatch endpoint.
- r2 focused: 28/28 passed, 2,238 frames, 768 settled comparisons. r2 full: 124/124
  passed, 2,292 frames, 768 settled comparisons. All cells ran once; no native retry.

This correction demonstrates the declared unsupported-presentation condition with
normal reset invalidation. It does not isolate an individual palette guard or prove
arbitrary-palette stale guarantees. The original failure preceded Unload dispatch;
it is not evidence that an unauthorized Unload occurred.

The r2 non-gate production binary digest is
`1b4cdab2247d229e3251aaccebfa46a48740952c6eec09372e57c21138176779`.
Minimal/full gate builds have separate identities in the JSON. Their native checks
cover normal-route callback order and stock outcomes, held-safe interruption,
release ownership, exact post-I, lifetime loss, stale context, type isolation,
command deduplication and fail-closed ledger exhaustion.

## Prior controls and pure checks

The retained prior matrix passes **390/390**: per game, stance 1, selection 1,
turn 1, map LEFT 1, inventory OPEN 14, CLOSE 34, Ground 42, CLICK 55 and RIGHT 46.
These are the original r1 A/B/C binaries with Unload compiled but runtime-disabled.
Their original binary/provenance labels remain intact. A separate compile-input
comparison shows the two r2 fixture changes are outside their compiled closure;
it does not relabel these as rebuilt r2 binaries or prove simultaneous activation.
There are 390 single exit-zero executions, no retries and 3,899 retained frames.

The main Python suite ran 587 tests in each of normal and optimized modes, with
no failures and two existing diagnostic skips per mode. This includes 41 focused Unload checks. The
Unload native-fixture pure suite passes 26 checks in both modes. Syntax/configuration
checks cover minimal Unload, CLOSE-only, all-inventory and default-OFF builds;
missing CLOSE is rejected. All 4,096 legacy flag configurations retain byte-identical tool catalogues.

Three legacy RIGHT mutation-helper anchors were repaired and their pure tests
rerun. Initial anchor/development failures remain retained. No older native mutants
were rerun: this maintenance does not improve the published RIGHT fault result.

## Fault matrix: complete classification, incomplete sensitivity

All 36 mutation binaries were built without build retries. Each rebuilds one mutated
translation unit and reuses 328 corresponding normal-r2 objects; these are not 36
fresh full-tree rebuilds. The predefined 72 mutant/game executions finish with
70 mapped detections, two survivors, zero inconclusive outcomes and no retries.
There are 2,175 retained frames. No fault endpoint was remapped after observing it.

Both survivors are `close-prerequisite-bypass` in the `close-disabled` profile.
The mutation removes the constructor’s CLOSE conjunction. The source still has a
CLOSE check at Q admission, so the unavailable-Q assertion remains satisfied.
Binding/frame-arming checks also retain CLOSE guards. This is a **source-supported
redundant-guard explanation**, not a separate causal experiment, proof of global
semantic equivalence or independent sensitivity of that constructor clause.

`fault_classification_complete` and the recorded phase acceptance mean the fixed
matrix and its classifications are complete. `all_mapped_faults_detected` remains
false. The two survivors must not be reported as detected or as a fully passed
sensitivity phase.

A preparation-only compatibility issue was retained separately: five tool-path
aliases required a narrow resolved-path input view. Original inputs and corpus
remained unchanged; the final plan also binds the complete original corpus and
intermediate link nodes. One view-input file is covered by exact whole-tree
entries rather than a direct flat pin. This distinction is preserved, not presented
as missing coverage or retroactively added runtime coverage.

## Actual official-SDK → game checks

Both games used the non-gate r2 production build and official MCP SDK 1.29.0.
The reviewed driver performed one command at a time, with separate local consent
for each proposal and no automatic probe, approval, retry or status polling.
Each path completed 24 calls, then explicit stop, with process exit zero.

The order is V, B, S, I; CLICK pickup, S, I; Q, declined F, S, I;
Q, successful F, S, I; CLICK pickup, S, I; Q, no-ammunition F, S, I.
Each CLICK consists of its own context/action calls. Stop is separate from those
24 calls. Driver native-outcome flags remain unpromoted.

- UFO: Joon-ho Seomun held the loaded Auto-Cannon (14). Decline preserved the held
  loaded weapon and 58 TU. Confirmed ordinary unload placed weapon/ammunition in
  the two hands and left 50 TU. After picking up the unloaded weapon, the next
  confirmed attempt visibly showed “No Ammunition Loaded!” and retained 50 TU.
- TFTD: Ivan Damyanov held the loaded Disruptor Pulse Launcher (1). Decline preserved
  the held weapon and 60 TU. Successful unload left 52 TU and the expected hand
  placement. The subsequent unloaded-weapon attempt visibly showed the same normal
  warning, preserved the held selection and retained 52 TU.

The warning was observed immediately in each live session; it had faded before the
later returned I image. The report does not claim that those I images show it.
After stop/EOF, ordinary visible interaction remained responsive before cleanup.
These native outcomes, local consent and liveness are manual observations, checked
against retained images/receipts where applicable. Protocol receipts or PNG hashes
alone do not establish native success, TU cost, refusal or liveness.

Each SDK path has before/after checks of 1,419 selected input files and the exact
1,540-entry frozen source tree, without new exclusions. These are named checkpoints,
not a complete SDK/interpreter/runtime closure or continuous immutability proof.
Terminal seals separately bind final bytes; they do not independently observe exit
or turn finalization-time equality into a before/after runtime claim.

## Evidence integrity and explicit limits

Independent fault review verifies 56,683 current input pins, 3,332 tree/file-record
specifications, all 3,106 sealed output entries and all 72 audit/stream/frame chains.
Separate selected-file checks cover 16 ancillary files at focused/full/fault
boundaries. These files are transitively freeze-bound; the separate checks do not
retroactively add them to the main runtime audit. Two live geometry checkpoints and
one final checkpoint do not establish uninterrupted focus or per-cell physical
pointer position. A rendered cursor image is not physical-pointer telemetry.

Battle/runtime and simulation RNG equality are native fixture assertions. libc RNG
is not used as a deterministic oracle. Whole-image parity compares naturally advanced
settled phases; first-rendered, post-view and collection-entry snapshots remain
separate. No arbitrary timing parity is claimed. Depth-zero sound lookup is observed;
the following `play()` call is source-only evidence. Muted audio proves no audibility
or mixer-output result.

Four inherited Windows project files retain Git/CRLF-checkout byte differences.
Four pre-freeze Python bytecode files were excluded before the initial freeze; exclusions
were not expanded afterward. Source/dependency checks apply at recorded boundaries,
not continuously or as a hermetic build/runtime closure.

The report consumer’s earlier freeze-schema import failure and later direct-pin
versus whole-tree-reader failure remain retained, alongside their narrow corrections
and development failures. Neither is a native retry. The final combined import
completed with exit zero; its integrity receipt is
`c1ffdda6af6eed13be0a041e18f3c44d6b7244313089923d3b128e58939ec5ef`.

The public JSON explicitly wraps a display projection of original report
`f640f3c50da29afd244728578d64d75634f395ba7b98c90c194e4292c28b37c2`
and unchanged supplement
`d67c4a9cf9841cd9e62e872fe0f3a774c5e989db3ab3af52662bd573abd3e63e`.
Projection metadata identifies three generalized preparation-only sentences, one
verification-scope clarification and one omitted nontechnical preparation field.
All technical outcomes, failures, correction records and evidence identities remain
unchanged. The original outputs remain intact. Evidence-generator publication flags
remain false: they are not an automated publication decision or overall green gate.

No prebattle/non-TU inventory, arbitrary mod, full-fairness, sanitizer, model/token
benchmark or complete campaign result is claimed. Reload and priming remain separate
unfinished inventory slices.
