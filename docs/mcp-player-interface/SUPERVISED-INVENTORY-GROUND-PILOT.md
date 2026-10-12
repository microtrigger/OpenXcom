<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised ordinary inventory Ground button

**Bounded implementation verified; experimental and default OFF.** This bounded `practical-play/1`
slice proposes one ordinary Ground button action. The button rearranges ground
item slot positions and stacking, then advances or wraps its displayed page.
It is not read-only scrolling. A dispatch receipt does not prove that the page
changed or that any particular item became visible.

## Explicit activation and request-bound pixels

`EXPERIMENTAL_SUPERVISED_INVENTORY_GROUND` is default OFF and requires the existing
CLOSE build and runtime chain. This is an implementation prerequisite for its
in-place confirmation machinery, not permission to reuse CLOSE authority.
Launch with `--supervised-stance --supervised-turn --supervised-inventory
--supervised-inventory-close --supervised-inventory-ground`. Native activation
additionally requires literal `OPENXCOM_INVENTORY_GROUND_ENABLED=1` and the
prerequisites; inherited activation variables are scrubbed by the launcher.

`read_inventory_ground_context({})` returns a fresh canonical 320×200 final-pixel
inventory image, opaque capability and control epoch. Admission requires the
supported initialized TU inventory, its live battlescape/save/player soldier,
ordinary presentation, no held item and the visible ordinary Ground button.
The normal button's canonical geometry includes its clipped final column.
No item list, item identity, page number/count, next-page availability or hidden
eligibility explanation is returned. The controller does not enumerate ground
items or construct an off-page preview.

`request_inventory_ground` accepts only capability, control epoch and command ID.
K authorizes G only. J still authorizes C only; I is still capless. Existing O/V
controls and their authority types are unchanged. There are no coordinates,
direction, desired page, item selector or approval argument.

## Local confirmation and unchanged ordinary behavior

The existing inventory owns the in-place presentation overlay. Its text explicitly
identifies ground rearrangement and paging; its background is the frozen proposal
image. Normal underlying primer animation continues. Showing, canceling or
invalidating the proposal never initializes inventory or arranges ground.
Only fresh local consent after the actual overlay has blitted and successfully
flipped may approve. Held, queued, repeated and modified consent is rejected;
dismissal events are consumed before underlying inventory input can see them.

The private binding reuses actor, presentation and lifetime generations. It does
not fingerprint item collections or compare animated full frames for freshness.
Refusing a held-item context does not clear, drop or relocate the held item.
Cancellation, expiry, disconnect and stale state consume authority without action.

On valid confirmation the controller retires the overlay, validates again,
consumes the grant and marks the shared ledger receipt dispatched before invoking
`InventoryState::btnGroundClick(0)` once. The ordinary handler owns
`Inventory::arrangeGround()`, including off-page slot rearrangement, stack layout,
page advance/wrap and redraw. These normal consequences are not MCP observation
fields. The controller does not call internal rearrangement helpers directly,
write page offsets, push/pop a confirmation state or change TU/item rules.

The ordinary `xMax >= offset + slotsX` paging rule is preserved: an exactly full
page may advance to an empty page. Empty ground and unchanged-looking results
remain ordinary outcomes rather than hidden preflight rejections.

## Mandatory inventory verification

After G dispatch, a fresh capless I from the exact surviving inventory lifetime
is mandatory before another context or new action. O/V/J/K cannot substitute.
Only successfully sending that actual presented I discharges the obligation.
If inventory, parent or save is lost, the destination is retired but the
obligation remains unsatisfied; closing/reopening cannot satisfy it by reusing
an address. No action retry or forced rendering repairs a failed observation.

The returned I shows the actual resulting inventory. Ordinary manual input within
the same live inventory may change its displayed actor or layout before that
observation; a receipt is not a promise of an unchanged world. Exact duplicate
requests and status reads remain status-only and never redispatch or rearm an
already fulfilled obligation. The shared non-evicting 64-command ledger and
conflicting-ID refusal remain in force.

## Wire contract

Context: `K` + challenge32, exactly 33 ASCII bytes; native schema
`experimental-inventory-ground-context/1`, public schema
`openxcom-mcp-inventory-ground-context/1`. Fields match the close context:
schema, challenge, control_epoch, capability, semantics and canonical view.

Action: `G` + epoch32 + capability32 + command_id32, exactly 97 ASCII bytes,
without suffix. Existing generic errors, packet bounds, status API, 1.5-second
native observation bound, 2-second client exchange timeout and 120-second consent
lifetime remain unchanged.

## Verification scope

Both games pass all 42 native profiles, including 20 actual ordinary-click/G
pairs per game and full physical-frame checks at natural inventory-animation
phases. Actual official-SDK → nongate production-game B/I/K/G/I/J/C/V chains
also pass once per game. Prior controls have passing regressions in both games.
See [the evidence and retained failed outcomes](SUPERVISED-INVENTORY-GROUND-EVIDENCE.md)
for the 50 initial source-fault runs, two separately predeclared follow-up runs,
the surviving UI-generation guard, and operator-attestation limits.

Item transfer, reload/prime proposals, full campaign/controller acceptance and
model/token benchmarks remain outside this slice. Simulation RNG was compared;
libc/seedless RNG state and timing fairness were not established.
