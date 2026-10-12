<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised ordinary inventory closing

This bounded `practical-play/1` slice proposes the ordinary inventory OK action.
It does not move, equip, reload, prime, drop, pick up or describe items. A receipt
means that the ordinary handler was dispatched once, not that closing and its
normal consequences succeeded.

## Separate opt-in

`EXPERIMENTAL_SUPERVISED_INVENTORY_CLOSE` is default OFF and requires supervised
INVENTORY, TURN and their STANCE/cursor prerequisites. Launch with
`--supervised-stance --supervised-turn --supervised-inventory --supervised-inventory-close`.
Native activation additionally requires literal `OPENXCOM_INVENTORY_CLOSE_ENABLED=1`
and the dependency chain; the launcher scrubs inherited experimental flags.
Selection and map left-click are not prerequisites.

`read_inventory_close_context({})` returns a request-bound canonical 320×200
final-pixel inventory PNG plus an opaque `control_epoch` and `capability`.
It requires the supported, initialized TU inventory above its exact live
battlescape parent, a player soldier, ordinary ready presentation and no held
item. It provides no item list, slot query, hidden eligibility result, actor ID,
path information or ground-page enumeration. Off-page information is not read
back or reconstructed. Input and state transitions invalidate the grant.

`request_close_inventory` accepts only that fresh capability, control epoch and
command ID. It accepts no approval field, coordinates, actor/item selector or
item operation. J authorizes only C. O remains limited to D/N and V to R/L/B.
The existing `read_inventory_view` I remains capless and authorizes nothing.
The shared non-evicting 64-command ledger retains exact-repeat status lookup,
conflicting-ID rejection and one-use authority. Repeated status or command
receipts never redispatch the handler or rearm a completed observation barrier.

## Confirmation without inventory reinitialization

A presentation-only overlay belongs to the existing InventoryState. Showing or
canceling it does not push/pop a State, initialize inventory, arrange ground,
change the ground page or drop a held item. Its background is the captured
proposal image. Ordinary underlying animation continues; the proposal image is
frozen, so its primer animation need not match the current underlying frame.
The private validity binding uses presentation/lifetime generations and the
current selected soldier, not item collections or full-image equality.

Only a new local Confirm/Y gesture after that exact overlay has actually blitted
and successfully flipped may approve. Queued, repeated or already-held consent
is rejected. Overlay input, including an event that dismisses it, is consumed
before ordinary inventory hotkeys, extra mouse buttons or item handling can see
it. Cancellation, expiry, disconnect and invalidation retire the overlay without
performing the proposed action. A fresh J is required for another proposal.

The dispatch invokes the unchanged ordinary OK handler once. It does not use
right-click as a close shortcut: with a held item, ordinary right-click cancels
the drag. Unsupported held-item admission never clears or relocates that item.

## Natural restoration and mandatory tactical verification

After dispatched C, a fresh V from the exact original battlescape parent is
mandatory before another observation or action. I, J and O cannot substitute.
The controller waits for the game's ordinary deferred inventory destruction,
parent initialization and strict applied-palette readiness. It does not force
cleanup, a draw/flip, initialization or a palette update. The fixed native
1.5-second restoration bound and existing client timeout are not extended by
activity. Losing the expected parent/save preserves the unsatisfied obligation
and fails closed.

The normal destructor retains gravity, terrain-lighting/FOV updates and display
restoration. Gravity can move ground items and schedule ordinary falling.
There is no hidden floor/path preflight and no suppression of those effects.
Unsupported transient presentation may make the required V unavailable; the
receipt alone must not be described as verified closing. Failure never retries
C or terminates the game.

## Transport

Context: `J` + challenge32 (33 ASCII bytes), native schema
`experimental-inventory-close-context/1`, public
`openxcom-mcp-inventory-close-context/1`. Exact fields are schema, challenge,
control_epoch, capability, semantics and canonical view; public PNG accompanies
the metadata. Capture follows normal blitting and successful presentation.

Proposal: `C` + epoch32 + capability32 + command_id32 (97 ASCII bytes), with no
suffix. Existing packet limits, generic errors and status API remain unchanged.
B opening and I observation retain their existing formats and boundaries.

## Verification status

The bounded official-SDK → production-game normal-close path has passed once
in each game: V → B → explicit status → I → J → C → explicit status → fresh V.
Both proposals received actual local confirmation, all returned images were
inspected, and the game remained alive after SDK EOF. All 34 native profiles
pass per game, including 14 manual/C pairs; 17 of 18 actual-source fault recipes
are detected in both games. The surviving single-guard mutation and earlier
fixture failures remain explicit in [evidence and limits](SUPERVISED-INVENTORY-CLOSE-EVIDENCE.md).
Full inventory operations, campaign coverage, full controller acceptance, Stage 5,
sequential batches and actual model/token benchmarking remain outside this slice.
