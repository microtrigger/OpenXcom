# Supervised stance integration gate

SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-11.
See `LICENSE.txt` and `LICENSE-NOTICES.md`.

This finite gate is compiled only with `EXPERIMENTAL_STANCE_CONTROL_GATE=ON`
(default OFF), requiring `EXPERIMENTAL_SUPERVISED_STANCE=ON` and its cursor
presentation prerequisites. It activates only when
`OPENXCOM_STANCE_CONTROL_GATE=1`. Do not enable it in an ordinary play build.
The operator harness deliberately approves its own controlled fixture dialogs
with SDL mouse events. It does not grant an MCP client any confirmation API.

## Run

Use legally supplied local UFO or TFTD resources and a real focused graphical
SDL display. The wrapper creates and removes a disposable configuration, sets
initialized-muted-dummy audio, and suppresses ordinary engine output.

```sh
python3 tests/stance-control/run-stance-control.py /absolute/openxcom /absolute/data xcom1
python3 tests/stance-control/run-stance-control.py /absolute/openxcom /absolute/data xcom2
python3 tests/stance-control/test_runtime_wrapper.py
python3 tests/stance-control/test_runtime_mutants.py
```

The first two commands are GUI runs and should be performed by the operator who
owns the test display. Unit checks never launch the game. The watchdogs prevent
an incomplete test from running forever; elapsed times are not fairness gates.

## Independent checks

An actual temporary native `BattlescapeButton` also reproduces the TFTD
resize/inverted-surface eligibility regression at startup. Through the real
`Surface*` virtual interface, an inverted ready alternate must qualify even
while the unused base has pending redraw; the pending base itself, hidden or
cropped controls, and an alternate with mismatched geometry must fail. This
component regression does not replace the actual game-panel checks below.

Twelve pairs reconstruct identical actual native battle fixtures. One member
uses the ordinary kneel hotkey; the other obtains a post-request natural frame,
submits the bound command and approves the actual owned confirmation dialog only
after a completed modal flip. Nominal stand uses a fresh Y edge; other valid
confirmations use a fresh mouse-down/up pair. Both
travel `BattlescapeState::btnKneelClick` and the unmodified normal action logic.

The pairs cover kneel 4 TU and stand 8 TU; exact 4/8; insufficient 3/7; a loaded
weapon's native snap-shot reservation at the boundary; reserved kneeling's
normal exception; standing with reservations; and kneel/stand under an armed,
high-reaction hostile. A callback at the **end of the normal UI handler** reads
the actual queued `BattleState` actions before normal simulation consumes them.
For the reaction pairs, a real hostile snap shot targeting the player must be
present and hostile TU must be spent. Merely comparing two empty queues cannot
pass. Semantic battle saves, native action queues, RNG, selected actor, TU,
stance and visible-unit/tile sets are compared in memory. Independent expected
4/8 costs also prevent an equally wrong manual and controlled path from passing.

Nineteen adversarial worlds check same-name, same-HUD-appearance actor selection;
manual stance input; pause overlay; delivered SDL focus-loss event; full
`SavedGame`/`BattlescapeState` replacement; changed TU and reserve state; no
observation capability; contradictory observed stance; cancellation; changed
context during local confirmation; selection of a distinct same-name unit
during confirmation; identical-command replay/status, conflicting-command-ID
reuse and consumed-capability reuse; approval Y queued before the modal first appears; held-Y repeat and held-mouse release; a post-dispatch observation rejected under a pause overlay (without retrying the command); and disconnect while confirmation is open.
The same-looking selection check compares the complete final displayed panel,
not just names. The focus test exercises the ordinary SDL notification route;
it does not claim to manipulate the operating system window manager. Load
coverage is actual save/state object replacement, not a disk load-dialog test.

The two nominal controlled kneel/stand pairs also make a **new O request after
the dispatched receipt** and compare the returned final displayed panel to the
corresponding natural manual post-action panel. Thus fresh post-action visible
evidence is required for those pairs. Reaction pairs intentionally compare the
immediate native queue before it is consumed; they do not claim a completed
post-reaction visible outcome. Stale confirmation is a separate pre-dispatch
failure test; the post-dispatch unavailable-observation test checks that the
harness stops without another D command after failing to observe the result.

Context RGB is checked byte-for-byte against a separate read of the actual
physical SDL display after its normal flip, including all scaled source pixels.
The gate never calls the producer's pixel helper. No internal unit identifiers,
state, private values, response packets, names, tokens, assets or captures are
emitted or written. The public result is one fixed aggregate line. Assertion
failures identify a compile-time source line only.

## Real-source sensitivity controls

Make five separate disposable source copies outside the normal checkout, apply
one mutation in each, rebuild each with the same options and run **both** master
wrappers. Every mutant must fail. A script/setup failure is not a detected
runtime mutant. Save only aggregate verdicts and source fingerprints.

```sh
python3 tests/stance-control/make-runtime-mutant.py wrong-actor /absolute/separate/source
python3 tests/stance-control/make-runtime-mutant.py duplicate /absolute/other/source
python3 tests/stance-control/make-runtime-mutant.py wrong-tu /absolute/another/source
python3 tests/stance-control/make-runtime-mutant.py reaction-bypass /absolute/fourth/source
python3 tests/stance-control/make-runtime-mutant.py unseen-modal /absolute/fifth/source
```

The controls change real native code: forward dispatch to a different real unit;
call the normal handler twice; charge 3/7 instead of 4/8 TU; remove the actual
reaction-queue call; or remove modal-presentation guards from native Y admission
and final confirmation. They do not alter the fixtures/assertions, add test-case
branches to gameplay, mutate adapter output, or claim mock failures as native
coverage. Anchors reject changed or already-mutated source, and the script
refuses its own normal checkout.

This gate establishes these bounded action and presentation properties only.
`dispatched` is a receipt for a normal handler call, not visible success. There
is no timing-equality, universal information-flow, or complete-fairness claim.
No assets, executables, saves, raw states or captures belong in publication.

## Native selection extension gate

The selection gate is a separate finite mode, requiring both
`EXPERIMENTAL_SUPERVISED_SELECTION=ON` and the existing stance/gate options. The
wrapper sets `OPENXCOM_SELECTION_CONTROL_GATE=1` and
`OPENXCOM_SELECTION_ENABLED=1`; it never enables the stance fixture at the same
time. Both gates retain separate unchanged public result schemas. The original
stance gate and its existing sensitivity controls must still pass independently.

```sh
python3 tests/stance-control/run-selection-control.py /absolute/openxcom /absolute/data xcom1
python3 tests/stance-control/run-selection-control.py /absolute/openxcom /absolute/data xcom2
python3 tests/stance-control/test_selection_wrapper.py
python3 tests/stance-control/test_selection_mutants.py
```

Ten pairs reconstruct native battles containing three player soldiers and a
hostile. The nominal cases have three simultaneously eligible players, so next
and previous have independently different expected actors. The pairs cover
both directions, wrap-around, do-not-reselect flags, out-of-play units, and a
single remaining eligible actor's native no-op. Ordinary hotkeys and controlled
N packets enter the exact same next/previous handlers. End-of-handler callbacks
assert the actual handler direction and independently expected selected actor,
not just that selection changed. TU and stance are checked independently for
all three soldiers and the hostile. Full semantic saves, runtime state, queued
actions, selected/current actor, camera, visible sets, and RNG are compared.
Every pair also obtains a fresh O context after the dispatched receipt and
compares its displayed panel against the natural ordinary post-action panel.

N uses the existing bounded socket, observation capability, confirmation,
status schema, and command ledger: exactly 99 bytes consisting of N, epoch32,
capability32, command-ID32, n/p, and literal 0. The gate neither exposes unit IDs
nor predicts a candidate through the protocol. Nineteen adversarial worlds
exercise same-name/same-panel selection, manual stance, overlays, focus,
load/state replacement, changed TU/reserve state, missing context, cancellation,
changed confirmation context/actor, pre-presented and held approval input,
post-dispatch observation failure, replay, and disconnect. Both N-first and
D-first worlds check exact replay/status, conflicting payloads under the same
ID, consumed-capability reuse, cross-action conflicting IDs, and cross-action
capability reuse. The modes run separately to stay below the shared 64-entry
ledger bound. Local fixture confirmations follow actual modal flips.

Additional real-source sensitivity setup is available. Each requires a distinct
disposable source copy and rebuilt binary; run both selection master wrappers
against each binary and require runtime failure. Merely applying the mutation
or a compilation/setup failure is not a detected runtime mutant.

```sh
python3 tests/stance-control/make-selection-mutant.py next-as-previous /absolute/copy1
python3 tests/stance-control/make-selection-mutant.py previous-as-next /absolute/copy2
python3 tests/stance-control/make-selection-mutant.py duplicate-next /absolute/copy3
python3 tests/stance-control/make-selection-mutant.py selection-charges-tu /absolute/copy4
```

These faults alter actual native dispatch direction/count or charge TU in the
ordinary selection handler. They do not change test assertions, fixtures,
protocol output, or callbacks. Results are fixed aggregates and source hashes;
private assets, source states, tokens, names, captures, and child logs are not
emitted or retained by either wrapper. This is a bounded correctness and
presentation gate, with no timing-equality or complete-fairness claim.
