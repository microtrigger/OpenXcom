<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised ordinary inventory Unload button

**Implemented bounded pilot. Experimental and default OFF; broader acceptance remains open.**
This bounded `practical-play/1` slice proposes one ordinary click on the visible
Unload button in supported TU battle inventory. It does not pick up an item,
select ammunition, promise success, or expose an eligibility query.

## Activation and authority

`EXPERIMENTAL_SUPERVISED_INVENTORY_UNLOAD` requires the CLOSE prerequisite chain.
CLICK, RIGHT and Ground remain independently optional. The explicit launcher
flag is `--supervised-inventory-unload`, alongside the existing prerequisite
flags; native activation requires literal `OPENXCOM_INVENTORY_UNLOAD_ENABLED=1`.
Inherited activation is removed by the launcher.

`read_inventory_unload_context({})` returns canonical 320×200 presented pixels
and opaque authority. `request_inventory_unload(capability, control_epoch,
command_id)` takes no coordinates, item identifiers, held-state fields,
modifiers, success prediction or approval argument. Q authority authorizes F
only; the shared non-evicting command ledger keeps exact duplicates status-only.

Context admission checks the supported visible UI and private continuity
bindings. It must not inspect ammunition, hand occupancy, selected-item dimensions,
fit, affordability or reserve rules to predict the ordinary outcome. Both held
and non-held presentations are allowed, so ordinary no-op/refusal is preserved.

## Normal action and local confirmation

Fresh local confirmation is required after the actual overlay is presented.
The ordinary inventory and held-sprite presentation remain live. Confirmation
bands avoid the held-selection rectangle, soldier-name region and Unload-button
region. This does not establish that preexisting sprites or noninteractive
layers cannot already cover those regions.
Declining, invalidating, expiring or disconnecting a proposal does not cancel a
held selection, rearrange ground or spend TU.

The proposed action is an unmodified LEFT DOWN/UP at canonical `(304,44)`, inside
the stock Unload button `(288,32,32,25)`. It uses the complete normal surface
routing order. The full-screen inventory receives hover and click processing,
including its normal stats callback, before the button receives the click.
Calling `unload()` or the button handler alone would omit these UI effects.
The owned physical confirmation release must remain consumed independently of
the synthetic button gesture. There is no exposed generic input API.

For stock UFO/TFTD items, ordinary Unload has these outcomes:

- With no held selection or intrinsic ammunition, it makes no unloading change.
- An unloaded ammo-accepting item gives the normal no-ammunition warning.
- Another carried item in a hand causes the normal both-hands-empty warning.
- Fewer than eight TU causes the ordinary insufficient-TU warning.
- Success spends eight TU, puts the weapon in the right hand and its detachable
  ammunition in the left, transfers ownership, and clears the held selection.

A detachable ammunition object with zero rounds is not excluded merely for its
quantity. The ordinary handler does not add a general fit or reserve-TU check.
Success retains the normal item/ammo-text and preview clearing, stats refresh,
and existing sound call. Ground-origin ownership and turn-flag changes remain
normal engine behavior. Unload itself does not initialize, close or rearrange
inventory. No replacement movement or rollback helper is added.

The preliminary inventory click is part of the normal button route. Stock item
sizes place its drop anchor outside the stock slots at the fixed point. Static
visible-layout checks do not prove this for arbitrary modded item dimensions;
modded behavior is not claimed covered, and hidden dimension/fit preflight is
not an acceptable substitute for ordinary routing.

## Mandatory observation and lifetime

Before any ordinary DOWN processing, the controller installs an unsatisfied I
obligation with no destination. It binds that obligation only to the exact
surviving inventory after synchronous state, parent and save continuity checks.
An unexpected transition during DOWN prevents the subsequent UP. Lost or changed
lineage keeps the obligation unsatisfied without dereferencing a destroyed state.

Every dispatched attempt, including no-op and normal refusal, requires a fresh
capless `read_inventory_view({})`. Only successful transmission of that actual
presented image clears the barrier. Status reads, duplicate receipts, failed
sends, reopening inventory and V do not replace it. There is no fallback view,
automatic recovery or replay; receipts report dispatch rather than unloading
success. The caller inspects the resulting visible inventory.

## Wire and verification limits

Context is `Q` plus challenge32, exactly 33 ASCII bytes. Native schema is
`experimental-inventory-unload-context/1`; public schema is
`openxcom-mcp-inventory-unload-context/1`. Action is `F` plus epoch32,
capability32 and command_id32, exactly 97 ASCII bytes.

Corrected UFO/TFTD focused/full native checks pass 28/28 and 124/124, covering
ordinary manual/proposed pairs, callback order, release ownership, stock outcomes,
held-safe consent/interruption, type/ledger isolation and exact mandatory I.
All 390 prior-control checks pass with their original build attribution retained.
Both actual official-SDK/game paths complete 24 calls plus stop. The fixed source-fault
matrix yields 70 mapped detections and two survivors; it does not establish complete
sensitivity. Original palette-fixture failures, corrected-boundary limits and the
source-based redundant-guard explanation remain in the
[evidence report](SUPERVISED-INVENTORY-UNLOAD-EVIDENCE.md).
Prebattle/non-TU inventory, broad mod coverage, full fairness, arbitrary timing
parity and model-token efficiency remain outside this slice.
