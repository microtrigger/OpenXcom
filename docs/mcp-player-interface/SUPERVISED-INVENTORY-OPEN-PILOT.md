<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised inventory opening and capless observation

This bounded `practical-play/1` slice proposes ordinary inventory opening from
a fresh tactical view, then observes only the actual presented inventory image.
It does not move, equip, reload, prime, drop, pick up or describe items. Closing
remains an ordinary manual game action.

## Explicit opt-in and proposal

`EXPERIMENTAL_SUPERVISED_INVENTORY` is default OFF and requires supervised TURN
and its STANCE/cursor prerequisites. Launch with
`--supervised-stance --supervised-turn --supervised-inventory`.
Native activation additionally requires literal `OPENXCOM_INVENTORY_ENABLED=1`
and TURN activation; inherited flags are scrubbed by the launcher. Selection and
map left-click are not prerequisites.

`request_open_inventory` accepts only the fresh tactical `capability`,
`control_epoch` and `command_id`. It takes no actor/item IDs, coordinates, item
lists or approval field. Only new local confirmation of its actually shown modal
can invoke the ordinary inventory-button handler once. The proposal uses the
same idle, non-debug, player-soldier tactical context and visible button boundary
as the existing controls. No hidden inventory-eligibility query guarantees that
the normal handler opens a window.

The V capability may authorize one R, L or inventory-open proposal. O remains
limited to D/N. All commands share the non-evicting 64-command ledger and status
API; exact repeats query status only, and cross-action/conflicting reuse fails.
A dispatched receipt records handler invocation, not successful opening.

## A read-only inventory view grants no action

After dispatched opening, `read_inventory_view({})` must verify a fresh image from
the exact destination inventory before other supervised context/action calls.
It may also observe a manually opened supported inventory. I can never replace
the V required after a right/left map click.

The response is a canonical lossless 320×200 final-pixel PNG, captured only after
the actual initialized top inventory has followed ordinary blitting and a
successful flip. It contains **no control epoch or capability**, actor/item IDs,
item list, inferred labels, ammunition values or ground-page enumeration.
Any item name, quantity, tooltip or held item is visible only to the extent the
normal UI actually drew it. Off-page, clipped, undisplayed and research-gated
information is not reconstructed.

Same-state input, hover, page changes, selection and ordinary primer animation
may continue while the read is pending: the response means the supported frame
was presented after the request, not that it is unchanged since admission.
This grants no action. Existing action grants are still invalidated by input.
The fixed native 1.5-second deadline and client two-second timeout do not reset.
Opening may normally defer a full palette until the first flip. Waiting accepts
that bounded lifecycle stage; capture still requires the exact applied palette
after the successful flip. No observer forces initialization or palette updates.
State/save/display replacement or focus loss cancels an outstanding read,
including away-and-back transitions before presentation.

Successful I retires prior action-grant fields and leaves a new O/V necessary
before an action. The user closes the inventory manually, then requests a fresh
battle context. Losing the expected destination before the mandatory I succeeds
fails closed; it does not permit bypassing the obligation with V. Failure never
retries the open, clicks items, closes the game or drives its initialization.

## Ordinary effects and deliberate exclusions

Opening is an ordinary UI action, not a side-effect-free read. The game's normal
handler cancels existing actions (the pilot admits only an idle context), and
inventory initialization arranges current-tile ground display slots and stacks.
These normal effects are neither reproduced nor suppressed by the observer.
Repeated I reads must not initialize or arrange inventory again.

Normal manual closing retains ordinary deferred destruction, gravity,
terrain-lighting/FOV updates and any display restoration. Deployment/non-TU
inventory and debug mode are unsupported. No observer reads raw item collections
or fingerprints undisplayed item properties to decide availability.

Automatic closing is deliberately deferred. Returning from a canceled confirmation
modal would normally reinitialize inventory, reset its ground page and rearrange
manually placed display slots. A future closing slice requires a narrowly tested
noninterfering modal-return design; there is no dormant close dispatcher here.

## Transport

Opening: `B` + epoch32 + capability32 + command_id32 + `o0` (99 ASCII bytes).
`c0` and every other action encoding are rejected.

Observation: `I` + challenge32. The native `experimental-inventory-view/1` reply
has only `schema`, `challenge`, `semantics` and canonical `view`; public metadata
uses `openxcom-mcp-inventory-view/1` and accompanies PNG content. I has no action
capability. Existing packet limits and generic errors remain unchanged.

## Verification status

All 14 finite native profiles and one real official-SDK/production-game sequence
pass in each game. Fifteen actual-source faults are detected in both games;
see [evidence and limits](SUPERVISED-INVENTORY-OPEN-EVIDENCE.md). Earlier milestones
retain their original evidence.
This slice does not complete inventory operations, full controller acceptance,
Stage 5, campaign coverage, sequential batches or actual token benchmarking.
