<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised ordinary inventory right click

**Implemented as a bounded experimental pilot; default OFF. Full sensitivity acceptance remains open.**
This bounded `practical-play/1` slice proposes one ordinary right click in the
supported battle inventory. The ordinary UI has two outcomes: when an item is
held, it cancels that selection; otherwise it closes inventory. Closing runs
the normal restoration, gravity, lighting and field-of-view behavior. A right
click is therefore not an unconditional cancellation operation.

## Activation and visible authority

`EXPERIMENTAL_SUPERVISED_INVENTORY_RIGHT_CLICK` requires the CLOSE build chain.
LEFT-click and Ground support remain independently optional. Launch with
`--supervised-stance --supervised-turn --supervised-inventory
--supervised-inventory-close --supervised-inventory-right-click`.
Native activation requires literal `OPENXCOM_INVENTORY_RIGHT_CLICK_ENABLED=1`
and the existing prerequisites. The launcher removes inherited activation.

`read_inventory_right_click_context({})` returns canonical 320×200 presented
pixels and opaque request authority. It adds no held-state, item, eligibility,
outcome or next-view field. Coordinates use the same static hand and ground
pointer domains as the [LEFT-click pilot](SUPERVISED-INVENTORY-CLICK-PILOT.md):
right hand `x=0..31, y=63..110`, left hand `x=128..159, y=63..110`, and ground
`x=0..319, y=152..199`, with strict integer validation and
actual supported-widget presentation checks. They describe the ordinary
pointer position, not an item identifier or a predicted branch.

`request_inventory_right_click` takes capability, control epoch, command ID,
and `x`, `y`. U authority authorizes X alone. The shared non-evicting command
ledger preserves exact duplicates as status-only and refuses ID conflicts.
Existing CLOSE and Ground authority continues to refuse held items.

## Confirmation and normal dispatch

The local overlay must state both possible outcomes. Ordinary inventory,
held-item presentation and natural primer animation remain visible. Only fresh
unmodified local consent after actual presentation dispatches the proposal.
Declining, invalidating, expiring or disconnecting a proposal sends no ordinary
right click and does not drop, return, initialize or rearrange an item.

Dispatch calls the normal pointer hover and RIGHT mouse-click path, including
its registered callback tail, exactly once. It must not substitute a direct
selection change, close helper, stack-cache update or state pop. Admission is
limited to supported TU battle inventory; pre-battle grenade priming and deactivation
is outside this pilot.

## Mandatory observation and explicit view choice

The controller installs an unsatisfied observation obligation before entering
the ordinary handler. It binds the required destination once from the actual
synchronous transition: the exact surviving inventory requires fresh capless
I; an ordinary close requires fresh V from the exact original battlescape
parent after normal restoration. Unexpected state or lineage changes remain
fail closed. Later manual closure cannot convert an outstanding I into V, and
reopening inventory cannot repair the lost lifetime.

The receipt reports dispatch, without predicting which branch occurred. The
caller explicitly chooses I or V from the previously visible situation and
local operator's visible result. There is no automatic I-then-V fallback. Choosing
the wrong family returns generic native unavailability without clearing its
barrier; the adapter then closes its control channel under the existing error
policy. Ambiguity needs manual reconciliation. This is a
finite joint-play interface, not an autonomous hidden-state recovery service.

Only successful transmission of the required actual presented image clears
the barrier. Status, duplicate requests, other contexts and failed sends do
not substitute. Ordinary manual input can still change the scene before the
observation; the image represents its actual current presentation.

## Wire and verification boundary

Context: `U` plus challenge32, exactly 33 ASCII bytes. Native schema:
`experimental-inventory-right-click-context/1`; public schema:
`openxcom-mcp-inventory-right-click-context/1`.
Action: `X` plus epoch32, capability32, command_id32, xDDD and yDDD, exactly
103 ASCII bytes. Existing generic errors, packet bounds, timeouts and local
consent lifetime remain unchanged.

The final r3 focused run passes 16/16 profiles and its separate full baseline
passes 92/92 across UFO/TFTD. Actual official-SDK/production-game chains complete
20 explicit tool calls and one local stop per game, with ten returned images
and separately labelled local-UI attestations. The native whole-phase comparison
requires observed natural animation advance; arbitrary transient equality and
the literal first post-dispatch frame are not established.

The 50 predeclared source-fault cells finish with 46 mapped detections, two
survivors and two inconclusive deliberate-mutant crashes. The fault phase keeps
its acceptance=false result. Remaining CLOSE admission checks are consistent
with the survivors; no retained crash stack proves the duplicate-callback
mutant's exact crash site. These limits are not normal-binary failures or extra
mapped detections. Earlier r1/r2 fixture failures remain recorded. The separate
298 prior-control regression cells pass (149 per game), without retries. See
[evidence and retained limits](SUPERVISED-INVENTORY-RIGHT-CLICK-EVIDENCE.md).
This pilot does not establish unrestricted controller or full-fairness acceptance.
