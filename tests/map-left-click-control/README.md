# Finite native supervised map left-click gate

SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12.
See `LICENSE.txt` and `LICENSE-NOTICES.md`.

This isolated, default-OFF fixture tests one ordinary map left-click, not a
privileged movement command. Build `EXPERIMENTAL_MAP_LEFT_CLICK_CONTROL_GATE`
with its map-left-click, turn, selection, stance-gate and presentation
prerequisites. The gate runs only with `OPENXCOM_MAP_LEFT_CLICK_CONTROL_GATE=1`;
its wrapper also explicitly enables map-left-click, turn and selection. It adds
no public approval or hidden-state interface and does not run in ordinary play.

## Local operator commands

Use legally supplied local resources and a real focused 640x400 SDL display.
The wrapper creates and removes a temporary profile, initializes muted dummy
audio, strips all inherited experimental endpoints/options and suppresses raw
child output.

```sh
python3 tests/map-left-click-control/run-map-left-click-control.py /absolute/openxcom /absolute/data xcom1
python3 tests/map-left-click-control/run-map-left-click-control.py /absolute/openxcom /absolute/data xcom2
python3 tests/map-left-click-control/test_map_left_click_wrapper.py
python3 tests/map-left-click-control/test_map_left_click_mutants.py
```

Only the first two commands launch native games. Python wrapper/mutator unit
checks are not native runtime coverage. A native run's sole successful verdict
is its exact aggregate PASS label. Duplicate, mixed, wrong-master or preview
verdicts are rejected. Only a single fixed assertion label can escape on failure;
paths, packets, assets, states, names and child logs are never forwarded.

An explicit operator-only modal inspection has a separate, non-PASS result:

```sh
python3 tests/map-left-click-control/run-map-left-click-control.py --preview-modal /absolute/openxcom /absolute/data xcom1
python3 tests/map-left-click-control/run-map-left-click-control.py --preview-modal /absolute/openxcom /absolute/data xcom2
```

It presents the first actual L modal for 20 seconds and quits. It records no
capture and emits `runtime_gate_pass:false`. Watchdogs bound unfinished tests;
they never advance simulation or timers.

## Manual/control parity and independent expectations

Nineteen pairs reconstruct the same setup independently. The manual member
uses SDL motion and ordinary left-button down/up events. The control member
obtains V, submits one L packet and approves its real presented native modal
using fresh local mouse or Y input. Y approval includes its normal key-up.

The pairs cover cardinal and diagonal walking, multistep movement with initial
turning, selected-tile no-op, zero/exact/partial TU, loaded-weapon snapshot
reservations at zero/partial/exact free TU, kneel reservation, insufficient and
partially sufficient energy, kneeled stand-and-walk, natural first contact
while initially turning, visible-friendly selection, visible-hostile no-op,
a deliberately slow four-step walk and an ordinary regular door.

Positions, direction, TU and energy results are literal fixture expectations,
not a second call to the implementation's pathfinder or cost helper. BLANKS
floor 1 is independently required to be a level, walkable floor with cost 4.
Cardinal steps cost 4 TU/2 energy and diagonals 6 TU/3 energy. Initial facing
changes cost no additional TU when walking proceeds; the first-contact fixture
instead stops during its first facing change and costs 1 TU. Standing first
costs 8 TU. The loaded rifle/jet-harpoon rule must have literal 25%/35% snapshot
costs, yielding a 15/21 TU reserve for a 60-TU soldier.

Regular-door expectations are distinct from the right-click pilot: UFO URBAN
object 28 has closed northwall cost 8 and opened alternate 27; TFTD PORT01
object 48 has closed northwall cost 4 and opened alternate 49. Both are regular
(non-UFO) doors and the opened part is a westwall. A northward walk adds the
level destination floor's 4 TU before opening, yielding literal 12 TU/6 energy
for UFO and 8 TU/4 energy for TFTD. The gate checks the actual opened map part
and final position. These resource records were independently checked from
MCD bytes, with applicable standard patches reviewed. Sliding doors and
reserved-door combinations are not covered.

A read-only handler-entry witness counts every native left `mapClick`, including
a second click rejected by the normal busy guard. A second witness after
`primaryAction` checks the actual actor, target and `UnitWalkBState` queue.
The actor must still occupy its original position with unchanged TU and energy
at this dispatch point. Selection and no-op outcomes must have an empty queue.
No fixture code forces think, movement, turning, FOV or timers during an action.
Setup-only map/unit construction and FOV initialization are explicit.

After ordinary native completion, pairs compare semantic battle saves, current
and queued actions, RNG, selection, camera, position, TU, energy, stance,
visibility and spotted-unit sets. Each pair member then obtains a fresh V.
Refusal cases must show the real native warning widget and wait for its ordinary
expiry before requesting the next supported view. A receipt never counts as an
outcome. The long controlled pair requests V while busy and requires natural
completion more than 1.5 seconds later, within the L continuation window.

## Complete physical-display oracle

Every V has exactly the tactical context fields, 320x200 dimensions, 128000
lowercase index-hex characters and 1536 lowercase palette-hex characters.
Only after a real SDL flip does an independent reader inspect the physical
640x400 video surface. Every 2x2 physical block must be one RGB color. Every
logical pixel is checked against the response, together with an independently
rebuilt first-seen RGB palette and zeroed unused entries. The producer's pixel
helpers and internal framebuffer are not the oracle. O retains its existing
separate 320x56 panel contract.

The two independent runs need not show identical animation phase or cursor
position. Neither timing equality nor complete player/agent fairness is claimed.

## Fail-closed and noninterference worlds

Thirty-six adversarial worlds cover stale selection, manual movement, overlay,
focus notification, full native state replacement, TU/reserve changes, missing
context, cancellation, stale/pending actor changes, unpresented or held input,
camera/pose away-and-back before and during confirmation, harmless naturally
changed animation, wrong observation kinds, four ledger origins, pending-V
replacement, unavailable post-action view and path-preview safety.

With path preview enabled, L is rejected without rewriting the option; R still
works. Enabling preview during L's modal also cancels dispatch. L/R/D/N share
the same ledger: exact replay, status retrieval, conflicting payloads, consumed
capability reuse and cross-kind same-ID conflicts are checked. The private
harness counts unique IDs and refuses to exceed the unmodified 64-entry limit.

Five additional consequence checks are included in the 36 worlds:

- A normal walking step invokes the real reaction logic and queues an actual
  `ProjectileFlyBState` for an armed hostile. Its actor, attack type, target and
  spent hostile TU are witnessed privately, while pending V fails closed.
- An ordinary local right-click mid-step cancels the remaining path; the current
  step finishes with literal one-step position/TU/energy.
- A walk lasting longer than eight seconds makes the pending V fail while the
  walk remains busy, then reaches its ordinary final position/cost naturally.
- Focus loss during a pending walking V invalidates that request, while the
  normal walk continues after the ordinary focus notification is restored.
- Controller disconnect after dispatch leaves the ordinary walk running to its
  expected final position/TU/energy.

The reaction test proves real queue formation, not completed projectile damage.
Falling consequences, actor loss during walking, and every other possible map
terrain/action combination are not separately verified here. The gate does not
turn those gaps into successful coverage claims.

## Actual-source sensitivity controls

`make-map-left-click-mutant.py` requires a separate disposable source copy for
each fault. It refuses the normal checkout, ambiguous/modified anchors and
reapplication. It alters production C++ only, never fixture assertions, packet
responses, adapters or mocks. Rebuild each mutated source and run both master
wrappers. A source/setup/build failure is not a detected native runtime fault.

```sh
python3 tests/map-left-click-control/make-map-left-click-mutant.py duplicate-click /absolute/disposable-copy
```

Supported faults are `stale-selector`, `wrong-pixel`, `duplicate-click`,
`wrong-button`, `free-walk`, `free-energy`, `reserve-bypass`, `contact-bypass`,
`reaction-bypass`, `door-right-cost`, `preview-bypass`, `camera-generation`,
`pose-generation`, `wire-pixel`, `unseen-modal`, `short-busy-wait`,
`timeout-disabled` and `id-conflict-bypass`.

Retain only fixed aggregate verdicts and source fingerprints in public reports.
Never publish resource files, executables, private saves, raw state, command
identifiers/capabilities, response packets, child logs or captures.
