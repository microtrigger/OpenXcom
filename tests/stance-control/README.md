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
