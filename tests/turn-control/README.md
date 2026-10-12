# Native supervised right-click integration gate

SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12.
See `LICENSE.txt` and `LICENSE-NOTICES.md`.

This is a finite, default-OFF native fixture. Enable
`EXPERIMENTAL_TURN_CONTROL_GATE=ON` together with its supervised-turn, selection,
stance, stance-gate and presentation prerequisites. The fixture runs only with
`OPENXCOM_TURN_CONTROL_GATE=1`; the wrapper sets `OPENXCOM_TURN_ENABLED=1` and
`OPENXCOM_SELECTION_ENABLED=1`. It neither runs in ordinary play nor exposes a
public MCP approval, hidden-state query, or private oracle.

## Run

Use legally supplied local UFO or TFTD resources and a real focused graphical
SDL display. The local operator owns this test display. The wrapper creates and
removes a disposable configuration, uses initialized muted dummy audio, strips
inherited experimental endpoints, and suppresses ordinary game logs.

```sh
python3 tests/turn-control/run-turn-control.py /absolute/openxcom /absolute/data xcom1
python3 tests/turn-control/run-turn-control.py /absolute/openxcom /absolute/data xcom2
python3 tests/turn-control/test_turn_wrapper.py
python3 tests/turn-control/test_turn_mutants.py
```

The first two commands launch native GUI runs. The latter checks test wrapper
and mutation setup only; they do not establish native coverage. Watchdogs bound
unfinished runs, never compare timing, and never advance the game simulation.

For explicit operator visual inspection only:

```sh
python3 tests/turn-control/run-turn-control.py --preview-modal /absolute/openxcom /absolute/data xcom1
python3 tests/turn-control/run-turn-control.py --preview-modal /absolute/openxcom /absolute/data xcom2
```

Preview presents the first actual R modal, holds it for 20 seconds, then quits.
Its distinct `TURN_CONTROL_GATE PREVIEW` result has `runtime_gate_pass:false`
and is never accepted as an ordinary gate PASS. This is a fixture-local option,
not a production runtime control. The gate itself never saves a screenshot.

## Native outcome checks

Fifteen independently reconstructed manual/control pairs cover:

- One-step clockwise turning, clockwise and counterclockwise multistep turning
- Wrap in both directions
- Already-facing no-op, zero TU, exact TU, and partial TU exhaustion
- Loaded-weapon snapshot reservation at zero, partial, and exact free-TU limits
- Kneel reservation with one free TU
- First-contact interruption before the requested final direction
- An ordinary same-facing right-click opening an actual regular resource door

The manual member uses normal SDL motion and right-button edges. The controlled
member obtains a fresh V context, submits R, and approves the actual native
modal with a fresh local mouse or Y edge only after its natural SDL flip.
The temporary fixture's own SDL approval is deliberately local; no automation
approval route is added to public MCP or production gameplay.

A hook immediately after ordinary `secondaryAction` checks both paths' actual
map target, actor, handler count, and queued `UnitTurnBState`. A non-no-op must
still be turning, with unchanged facing and TU at this point. The fixture then
waits for ordinary `BattlescapeState::think` and native timers to complete the
queue. Literal independently expected direction and TU cost must match. It
compares full semantic battle saves, current and queued actions, RNG, selected
actor, camera, TU, stance, visible units/tiles, and spotted-unit sets. No fixture
call forces `UnitTurnBState::think`, opens the door directly, or fabricates a
reaction-fire check. Native turning has no added reaction-fire call.

The multistep controlled pair sends V immediately after the dispatched receipt
while the native turn remains busy, and requires a successfully completed fresh
view after natural settling. Every pair obtains a fresh V after its outcome,
including the manual member. A dispatched receipt alone never passes an outcome. Zero/partial-TU and
reserved-TU refusals must show the actual native warning widget. A fixture-only
read-only subclass (no behavior overrides) witnesses its visibility through the
native state's surfaces. The gate waits for its ordinary timer to hide it and
for the supported HUD to settle before requesting V; it never hides a warning,
forces a timer, or retries the action.
The exact regular door costs are independently asserted as 8 TU in UFO and
4 TU in TFTD; the tile must contain the resource's ordinary opened-door part.
Sliding/UFO doors and reserved-door combinations are not separately verified by
this gate.

## Independent complete physical-display oracle

Every V response has exactly the tactical context fields, 320x200 dimensions,
128000 lowercase index hex characters, and 1536 lowercase palette hex
characters. The fixture independently reads the actual 640x400 physical SDL
video surface only after its normal flip. Each logical pixel's complete 2x2
scaled block must agree in RGB. All 64000 logical pixels are checked against the
response. Independently rebuilding a first-seen RGB palette requires canonical
indices and zeroed unused palette entries. O responses used in cross-kind
checks retain their separate existing panel contract.

No producer pixel helper or internal framebuffer is the oracle. The manual and
controlled semantic outcomes match; their independent post-outcome physical
view checks do not demand identical animation phase or cursor location across
two separate runs. The gate makes no timing-equality or complete-fairness claim.

## Fail-closed cases and shared ledger

Twenty-nine adversarial worlds cover changed selection, ordinary manual turn,
overlay, SDL focus-loss notification, full native save/state replacement, changed
TU/reserve state, missing context, local cancellation, stale confirmation and
changed actor during confirmation, pre-presented Y, held-Y repeat, held-mouse
release, camera and actor-pose away-and-back both before command admission and
while confirmation is open, animation-surviving grants, three shared-ledger
origins, all wrong observation kinds, unavailable post-dispatch observation,
replacement while a busy-turn V is pending, and disconnect while confirmation is open.

Camera/pose round trips restore the visible position/facing within one fixture
step, so only equality of current values cannot catch them. The animation case
retains its V grant with no input or intervening request until a naturally
changed physical display is observed, then requires that original R to be
admitted. The ordinary pre-blit controller poll has already run before that
changed display, so a harmless animation redraw invalidation fails this case.

A separate multistep world admits V during its ordinary busy interval, then
replaces the actual battle/state after a complete busy frame. That pending
challenge must fail, rather than return pixels from the replacement scene. The
next stable world can obtain its own fresh V. Likewise, a post-dispatch V that
fails under an overlay does not permanently prevent a new explicit V after a
stable state replacement; the last disconnect world witnesses that recovery.

R-first, D-first and N-first ledger worlds check exact replay, status retrieval,
conflicting same-ID payloads, consumed-capability reuse, fresh-context
cross-action same-ID conflicts and opposite-kind consumed-capability reuse.
The R-first world additionally requires native O to fail before the mandatory
post-R V read, then permits fresh O after that V. Standalone O-for-R, V-for-D and
V-for-N worlds require unavailable status without any action handler. Thus the
shared ledger and observation-kind binding are not inferred from matching
packet lengths or an unrelated stale grant. The ledger stays below its fixed
64-entry bound; old IDs are never evicted or reset between these worlds.

## Actual-source sensitivity controls

Each mutation requires a separate disposable source copy, rebuild, and both
master wrappers. A compile/setup failure is not a detected native runtime fault.
Run the original gate first; retain only fixed aggregate verdicts and source
fingerprints, never private fixtures or child logs.

```sh
python3 tests/turn-control/make-turn-mutant.py stale-selector /absolute/copy1
python3 tests/turn-control/make-turn-mutant.py wrong-pixel /absolute/copy2
python3 tests/turn-control/make-turn-mutant.py duplicate-turn /absolute/copy3
python3 tests/turn-control/make-turn-mutant.py free-turn /absolute/copy4
python3 tests/turn-control/make-turn-mutant.py reserve-bypass /absolute/copy5
python3 tests/turn-control/make-turn-mutant.py camera-generation /absolute/copy6
python3 tests/turn-control/make-turn-mutant.py pose-generation /absolute/copy7
python3 tests/turn-control/make-turn-mutant.py unseen-modal /absolute/copy8
python3 tests/turn-control/make-turn-mutant.py wire-pixel /absolute/copy9
```

These remove the actual native cached-selector refresh, change its real target
pixel, dispatch the ordinary secondary action twice, remove the native turn TU
cost or reservation guard, suppress actual camera/pose generation updates, or
remove the real modal-presentation guards. The ninth fault corrupts one valid
index in the actual native full-view encoder after canonicalization, so the
independent physical pixel oracle must reject it. These faults never change
fixtures, oracle assertions, adapter responses, or mocks. A separate pending-V
invalidation mutant is not included; that case has a direct native runtime
assertion rather than a separate fault-injection control. Anchors reject modified or ambiguous source
and the script refuses the normal checkout. Reports are sanitized fixed labels;
no assets, executables, saves, names, raw states, private paths, command tokens,
response packets or captures belong in publication.

## Physical pointer precondition

Keep the real pointer inside the map, away from the HUD and scroll edges, before
launching this focused-display fixture (the recorded profile uses canonical
320x200 point (160,50), scaled into its 640x400 window). Synthetic SDL motion
events do not reposition the physical pointer queried by ordinary state
restoration after a dialog. A different physical hover can therefore change
observation readiness. Do not treat an unavailable response as a passing test.

The later [CLICK regression evidence](../../docs/mcp-player-interface/SUPERVISED-INVENTORY-CLICK-EVIDENCE.md#prior-control-regressions)
retains the original failures and separate old/current comparisons with this
explicit pointer condition. Their extra inter-run checkpoints/pauses prevent a
strict single-variable causal claim; source, oracle and timeout stayed unchanged.
