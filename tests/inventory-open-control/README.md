# Finite native supervised inventory OPEN and capless-view gate

SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12.
See `LICENSE.txt` and `LICENSE-NOTICES.md`.

This isolated, default-OFF native fixture exercises ordinary inventory opening
and a pixels-only inventory read. It implements no controller CLOSE command,
item command, test approval endpoint, hidden item preflight, or controller reset.
The local operator wrapper approves only the synthetic fixture's real dialogs.

## Run locally

Build `EXPERIMENTAL_INVENTORY_OPEN_CONTROL_GATE` and its supervised inventory,
turn, selection, map-left-click, stance-gate and presentation prerequisites.
Use legally supplied resources and a real focused, indexed 640x400 SDL display.

```sh
python3 tests/inventory-open-control/run-inventory-open-control.py /absolute/openxcom /absolute/data xcom1
python3 tests/inventory-open-control/run-inventory-open-control.py /absolute/openxcom /absolute/data xcom2
python3 tests/inventory-open-control/test_inventory_open_wrapper.py
python3 tests/inventory-open-control/test_inventory_open_mutants.py
```

Only the first two commands launch native games. Python unit tests validate
wrapper/mutation setup; they are not native coverage or mutant detection.

Each master invocation runs these 14 explicit, finite profiles in independent
processes and disposable configuration directories:

- `core`: 12 independently reconstructed manual/B opening pairs (24 worlds)
  plus 24 adversarial worlds, totaling 48 core worlds per game
- `focus`, `overlay`, `pop`, `state`, `save`, `display`: pending-I interruption
  and the mandatory B observation barrier after the destination is lost
- `ledger-limit`: one shared typed ledger's exact 64-entry boundary
- `post-r`, `post-l`: I cannot discharge a preceding R/L requirement for V
- `disabled`, `turn-disabled`: B is unavailable without both explicit enables
- `nontu`, `debug`: ordinary non-TU/deployment-style or debug inventory cannot
  supply a supervised inventory read

The wrapper removes inherited `OPENXCOM_*` settings/endpoints, sets only the
profile's explicit flags, uses muted dummy audio, redacts child output, and
requires one exact, matching native PASS for every profile before printing the
suite PASS. Wrong master/profile, duplicate or mixed verdicts, nonzero exit,
and timeout are failures. At most one exact fixed assertion label can escape;
no raw paths, packets, names, state or child logs are forwarded.

A terminal B obligation is never cleared privately to run another fixture.
The affected profile ends, and the next profile starts a new process/ledger.
Per-profile watchdogs and process timeouts only stop unfinished tests.

## Ordinary opening and independent expectations

Six fixture types run with `maximizeInfoScreens` both false and true:

1. Empty carried inventory with zero TU
2. Empty carried inventory with 40 TU
3. A loaded rifle/jet harpoon
4. Every configured carried slot occupied and both hands occupied
5. Twenty-five distinct medikits on the ground, initially noncanonical and
   spanning more than one page
6. A primed grenade with ordinary primer animation

Every manual member uses ordinary SDL inventory-key edges. Every controlled
member obtains V, sends a single B `o0` request, witnesses the real full-view
modal, and approves with fresh local mouse or Y edges. Read-only handler-entry
and ordinary-init callbacks independently require one entry and one init.
Opening preserves literal TU, energy, position, direction and stance. Both
members compare semantic battle saves, current/queued actions, engine RNG, camera,
selection, visibility and spotted-unit sets after opening and manual closing.

The ground fixture uses legitimate, distinct medikit charge combinations to
prevent stacking. Its literal independent expectation is x=0..24, y=0 after
ordinary arrangement, rather than a second call to `arrangeGround`. The gate
never invokes inventory init/draw/arrangement helpers itself. Normal constructor
and init arrangement remain part of the action being compared.

The first I is queued immediately after the opening dispatch, before ordinary
InventoryState initialization. Native `_refreshMouse` warp events are allowed
through the real event loop, and their occurrence is witnessed. Successful
reads are followed by repeated I reads, with no additional init, serialized
item/TU changes or RNG changes. Primer fixtures wait for an actual natural
pixel-phase change across reads; they do not force animation timers.

Every successful opening ultimately closes through ordinary local input and
obtains a fresh V. The test does not equate a dispatched receipt with success.

## Physical-display and knowledge oracles

Every I reply has exactly `schema`, `challenge`, `semantics`, and `view`.
There is no epoch, capability, item list, hidden field or raw object identity.
The view must be 320x200, with exactly 128000 lowercase index-hex characters
and 1536 lowercase palette-hex characters. The first-seen RGB palette and its
zeroed unused entries are reconstructed independently from the physical SDL
video surface after its normal flip. Every 2x2 physical block must have one
RGB color, and every logical pixel is checked. No producer canonicalization
helper or internal framebuffer is used as the oracle. V and O retain their
separate full-view and 320x56-panel contracts.

Two additional independently reconstructed worlds vary an undisplayed unit's
loaded ammunition and an off-page tail medikit's charge. The test first requires
exact equality of actual presented RGB, then compares normalized I views.
This is a bounded knowledge-equality check, not a claim about every hidden-item
configuration. No hidden value is copied into a public verdict.

## Capless reads, local input, and fail-closed behavior

The core also checks:

- Same-state hover, page-button input, next-unit selection (including its normal
  extra init/warps), and holding a loaded item while I is pending
- A further actual read with the cursor over the ground area
- Manual close refusal while an item is held, then ordinary right-click return
  and ordinary close, without altering item ownership or TU
- I grants none of B/D/N/R/L, O cannot authorize B, and V cannot authorize D
- Stale camera/pose away-and-back before and during B confirmation
- Cancelled, unpresented, held-Y and held-mouse confirmation
- Exact replay, status retrieval, same-ID changed payload and every cross-kind
  same-ID conflict for B/D/N/R/L, plus consumed-capability reuse
- O and V cannot replace mandatory I after B; losing its exact destination
  cannot make V a substitute

Delayed-input worlds use an ordinary FPS=1 display cap to create a pending
presentation window, restoring FPS afterwards. They still run ordinary input,
think, blit and flip. A separate deadline world deliberately stalls the main
thread once for 1600ms after admission. The next ordinary poll must fail I
before capturing. This is artificial-starvation deadline sensitivity, not a
natural-play timing result. No simulation timer or clock is forced.

A display-only world resets the display before I admission and defers only one
palette color through the public Screen API. I must fail before another flip;
a partial palette cannot establish an exact full palette. After restoring the
full saved palette and allowing an ordinary flip, I must succeed with unchanged
initialization count, items, TU and RNG. Actual SDL driver palette failure is
not fabricated or claimed as tested.

Interruption profiles require the same generic unavailable response. The state
profile replaces the stack while retaining the battle save. The save profile
first cleans up the old stack safely, then replaces the save and reconstructs
the battle. It does not replace a save beneath a live InventoryState's raw
pointer, and therefore does not isolate save-only invalidation from stack
invalidation. The display profile invokes the normal display-reset path.

The ledger-limit profile admits 64 unavailable typed commands, retrieves the
first exact request again, and requires the 65th distinct ID to fail/close.
It never evicts, resets or enlarges the production ledger.

## Actual-source sensitivity controls

```sh
python3 tests/inventory-open-control/make-inventory-open-mutant.py duplicate-open /absolute/disposable-copy
```

Use a separate disposable source copy for each fault, rebuild it, and run the
complete wrapper for both masters. The mutator refuses the normal checkout,
aliases/hardlinks, modified or ambiguous anchors, and repeated/combined faults.
It changes one production source file, never gate assertions, packets, adapter
results or mocks. Setup/build failure is not a native detected fault.

The supported faults are `duplicate-open`, `wrong-handler`, `spend-TU`,
`wire-pixel`, `inventory-capability`, `B-barrier-bypass`,
`I-clears-post-map-barrier`, `pending-I-input-cancel`, `forced-init`,
`stale-camera`, `stale-pose`, `unseen-modal`, `deadline-extension`,
`id-conflict-bypass`, and `missing-first-flip-palette-wait`.

Keep mutation setup, successful rebuild, native baseline PASS and native mutant
rejection as separate results. A mutator unit test alone proves no runtime kill.

## Limits

No CLOSE control, equipment mutation command, deployment workflow, save/load
menu workflow, arbitrary mod, every HUD overlay, or every hidden-data world is
covered. The disabled profiles directly probe B, not every possible malformed
I packet. No completed projectile/reaction sequence is part of this milestone.
The inventory fixture makes no timing-equality or full-fairness claim.

Do not publish resources, executables, private saves, raw states, command
identifiers/capabilities, response packets, child logs or screenshots. Public
results may retain fixed aggregate verdicts and source fingerprints only.

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
