<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Finite ordinary inventory Unload gate

This default-OFF local operator fixture tests the bounded Q/F Unload pilot. It
is not a model benchmark or full-fairness verdict. Build
`EXPERIMENTAL_INVENTORY_UNLOAD_CONTROL_GATE` and its declared prerequisites, then
explicitly opt in with `OPENXCOM_INVENTORY_UNLOAD_CONTROL_GATE=1`.
The feature itself independently requires CLOSE, without CLICK, RIGHT or Ground.

The wrapper declares **62 isolated profiles per game**. Native execution must
be serialized with every other GUI/native operator. The following checks are
pure and do not launch the game:

```
PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s tests/inventory-unload-control -p 'test*.py'
PYTHONDONTWRITEBYTECODE=1 python3 -B -O -m unittest discover -s tests/inventory-unload-control -p 'test*.py'
```

## Independent ordinary-route oracle

`core` reconstructs 96 worlds: 24 fixtures × manual LEFT DOWN/UP versus locally
confirmed F × maximize-info off/on. The manual control queues actual SDL motion
and LEFT DOWN/UP at (304,44). It never calls the new dispatch helper,
`Inventory::unload`, or `btnUnloadClick` directly.

The private fixture observes the actual `State::handle` entry/return, both
ordinary inventory hovers, the existing inventory click and invClick tail,
then the actual Unload button callback. It requires this exact order:

1. DOWN, inventory hover, DOWN return
2. UP, inventory hover, inventory click, invClick, Unload button, UP return

Both the inventory and button must be pressed after DOWN and released after UP.
TFTD's ordinary button inversion is checked at those same boundaries. A separate
observer distinguishes real confirmation-UP/Y events from synthetic action
edges. A mandatory unsatisfied, null-destination I obligation must already exist
before the first synthetic DOWN and every ordinary dispatch callback.

Complete battle-save/runtime and simulation RNG snapshots include TU, energy,
item ammunition/quantity/fuse/slot/position, owner and previousOwner, isAmmo,
ground turn flag, ground/unit vector order, held identity, page/stack cache,
text, ammo-preview pixels and button state. Libc RNG is not an oracle. Explicit
expected outcomes supplement manual/F parity, so changing the same ordinary
handler in both paths cannot silently redefine the expected cost or result.

The 24 stock UFO/TFTD fixtures are:

- 0: empty inventory, nothing held
- 1–3: loaded right-hand, left-hand and ground weapon at 40 TU
- 4–6: unloaded compatible-ammo weapon, intrinsic weapon and nonweapon
- 7: loaded weapon with a detachable zero-round clip
- 8–11: hand/ground weapon at 7 TU refusal and 8 TU success
- 12–15: the other hand occupied, from right hand, left hand and ground
- 16–19: no-ammo, intrinsic, nonweapon and zero-round loaded weapon with another
  hand occupied at 7 TU, testing ordinary refusal precedence
- 20: loaded ground weapon among a same-ammo stack and another ground item
- 21: loaded ground weapon after enough items to require ground-page movement
- 22: loaded weapon at zero TU with the other hand occupied
- 23: no held item while loaded weapon and occupied hands exist at zero TU

Success must spend exactly 8 TU, place the gun in the right hand and detached
ammo in the left, preserve ammo quantity, clear selection, and perform ordinary
ownership/vector/ground-turn-flag updates. Refusal must leave the complete
battle save and simulation RNG unchanged. Immediate warning visibility and exact
translated warning text distinguish no-ammo, hands-occupied and insufficient-TU
refusals; no hidden quantity/fit/reserve preflight is introduced.

The fixture observes the **actual** `Mod::getSoundByDepth` invocation and requires
one depth-0 ITEM_DROP lookup on success and none on no-op/refusal. The subsequent
ordinary `play()` call is source-checked. Dummy muted audio is deliberate: this
is not an audible playback, mixer-output or full audio-parity claim.

## Physical images and natural timing

Every returned Q/I and relevant cross-kind image is checked independently
against the complete physical SDL display and canonical RGB palette encoding.
The actual first rendered frame after action, returned post-I frame, and actual
rendered collection-entry frame are retained separately. Ordinary warning
visibility/text are checked immediately after UP. Settled collection starts
only after a warning retires through its normal timer, then requires an observed
natural primer-phase advance from collection entry before comparing eight
whole, unmasked physical phases. Initialized phase zero is not a parity phase.

No image masks, timer resets, forced redraws, elapsed-time matching, or image
substitution are used. Later settled equality does not claim equality of the
first post-action or transient-warning frames. Already-rendered overlay text,
buttons and cursor are checked against the physical display separately. The
live banner must avoid adding coverage over the held selection, soldier name
and Unload button; there is no Unload pointer marker.

## Negative profiles and boundaries

Profiles cover denial, real 120-second proposal/capability expiry, disconnect,
unseen or held consent, invalid hidden/moved overlay children, real Y/mouse
release tails and omitted-release/new-gesture recovery. They include stale
input/actor/reinit/held revision/held position, focus/palette change, capless-I
revocation, widget/slot/button geometry, disabled runtime/CLOSE, independent
Unload flags, prebattle/base/debug refusal, cross-kind grants and exact receipts,
pending and dispatched duplicates/conflicts, and the shared 64-command ledger.

Malformed lengths, extra coordinates/data, token syntax, Q lengths and NUL are
separate fresh processes because malformed wire input closes the transport.
After every success/no-op/refusal, wrong views must leave exact-I outstanding.
Failed I admission, failed I transport-send, lost/reopened inventory and changed
parent/save are isolated. Real push+pop, pop, and actual save-change notification
between DOWN and UP must suppress UP and keep the unresolved null-I barrier;
there is no V fallback. The save-notification fixture does not free a save under
live callback pointers. Other profiles replace the actual save after dispatch.
Reopen does not force allocator address reuse and makes no exact-address claim.

Arbitrary modded item dimensions are outside this finite stock-layout matrix.
The full preliminary inventory click remains normal production behavior;
production admission must not convert hidden item-fit or ammo eligibility into
an oracle. Source tests expressly reject those shortcuts.

## Predeclared production-source sensitivity

`make-inventory-unload-mutant.py` contains **36** real production-source recipes.
`fault-map.json` fixes each profile and named numeric semantic endpoint before
native execution. No mapped endpoint depends on a physical source line number.
Generic fixture/schema failures are separate support IDs and are inconclusive.
The helper requires a disposable checkout; aliases, hardlinks to the normal
checkout, duplicate anchors, combined/repeated mutations, and oracle/test
mutations are refused. Composite recipes are marked in the map.

Application and compilation are setup only. Detection requires ordinary failure
exit at exactly the predeclared semantic endpoint in the mapped profile. Exact
native profile PASS is a survivor. Crashes, timeouts, watchdogs, build/launch
failures, unrelated endpoints, or changed frozen inputs are inconclusive. Never
expand mappings or rerun to improve a result. A duplicate route is checked at
prehandler entry before a second callback can mutate state; this does not make
crashes count as detections. Retain survivors and all failed attempts.

Freeze source, binary, dependencies and mapping before each runtime campaign.
Any fixture correction starts a distinct frozen revision and preserves earlier
outcomes. Pure/source-bound checks and syntax-only checks are setup evidence;
actual results and retained earlier failures are recorded separately in the
[bounded evidence report](../../docs/mcp-player-interface/SUPERVISED-INVENTORY-UNLOAD-EVIDENCE.md).
The finite gate does not establish full-fairness acceptance. Pre-freeze Python bytecode caches are not source
inputs and must be explicitly excluded before the first frozen snapshot, with
no later exclusion changes.
