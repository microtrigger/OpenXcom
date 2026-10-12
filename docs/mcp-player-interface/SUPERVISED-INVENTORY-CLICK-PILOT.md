<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised ordinary inventory left click

**Bounded native and actual SDK/game paths verified. Experimental and default OFF.**
This bounded `practical-play/1` slice proposes one ordinary inventory left click
at a displayed pointer coordinate. It can pick up, place, stack or reload an
item, show the ordinary warning, or do nothing. A receipt establishes dispatch,
not a successful transfer. A useful pickup/drop sequence consists of two
independent proposals, two local confirmations, and a fresh inventory view
between them. There is no atomic transfer or automatic second click.

## Explicit activation and request-bound pixels

`EXPERIMENTAL_SUPERVISED_INVENTORY_CLICK` requires the existing CLOSE build chain.
Ground support is independently optional. Launch with `--supervised-stance
--supervised-turn --supervised-inventory --supervised-inventory-close
--supervised-inventory-click`. Native activation requires literal
`OPENXCOM_INVENTORY_CLICK_ENABLED=1` and its prerequisites. The launcher scrubs
inherited activation variables.

`read_inventory_click_context({})` returns canonical 320×200 presented pixels,
an opaque capability and control epoch. Its envelope matches the existing
inventory context. It adds no item, slot, held-state, cost, page, eligibility or
outcome fields. Admission checks the actual supported TU inventory and ordinary
presentation; it does not enumerate hidden items or off-page ground contents.
An ordinary visible warning that obscures the admitted ground region refuses a
new click context while capless viewing remains available. No warning text or
hidden reason is exposed, and warning timers are not reset.

`request_inventory_click` accepts capability, control epoch, command ID, and
strict integer `x`, `y` in canonical screen coordinates. The initial static
domain is the stock right hand (`x=0..31, y=63..110`), left hand
(`x=128..159, y=63..110`), and ground (`x=0..319, y=152..199`). Actual inventory
section and interactive-control presentation must also match the supported
layout; the rectangle alone does not authorize an altered or obscured UI.
Booleans, fractional values and coercions are rejected. These are pointer
coordinates, not item-slot or item-top-left coordinates.

## Local confirmation and held-item behavior

H authority alone permits E. Existing J→C and K→G authority still refuses a held
item. The private binding includes ordinary actor/lifetime/UI generations and
a held-selection presentation revision, without exposing item identities or
fingerprinting collections.

The existing inventory owns the confirmation overlay. For E, the ordinary
inventory, held sprite and natural primer animation remain live underneath.
A 32-pixel confirmation band uses one of two supported positions outside the
pointer domain, chosen to avoid the actual held-sprite rectangle. A marker with
a transparent center identifies the pointer; the text gives its coordinates
and warns that the ordinary click may reload. It does not predict an item,
valid drop, TU charge or successful outcome. J/K retain their frozen-proposal
background, with their already documented natural underlying animation.

Only fresh unmodified local consent after actual overlay presentation can
approve. Showing, dismissing, expiring or invalidating a proposal does not
initialize inventory, arrange ground, drop an item, or send a compensating
right click. Cancelling a second proposal leaves the ordinary item held until
the player chooses what to do. Dismissal input is consumed even when overlay
presentation has become invalid. Pending authority is retired on disconnect,
expiry or stale state; a held item is not an implicit rollback obligation.

## Ordinary dispatch and mandatory observation

After validation and local consent, the controller retires authority and marks
the shared receipt dispatched before calling the ordinary inventory hover and
left-click path once. The normal `Inventory::mouseOver` establishes pointer
anchoring, then `Inventory::mouseClick` and its ordinary callback tail own the
result. There are no direct item-placement, TU-spending, reload, rearrangement
or selection helpers substituted for this path.

Before dispatch, the controller installs a mandatory fresh capless I obligation
for that exact surviving inventory lifetime. Only successful transmission of
its actual I satisfies it. O/V/J/K/H cannot substitute; failed or lost I does
not authorize a retry. Losing inventory, its parent or save remains fail closed,
even if another inventory later occupies the same address. Ordinary manual
input may change the world before I; the image is the actual current view,
not a guarantee that no intervening action occurred.

The shared non-evicting 64-command ledger preserves exact duplicates as
status-only. Reusing an ID with changed coordinates or another action kind is
refused. Status and duplicate reads neither replay gameplay nor rearm an
already satisfied observation barrier.

## Wire contract

Context: `H` plus challenge32, exactly 33 ASCII bytes. Native schema:
`experimental-inventory-click-context/1`; public schema:
`openxcom-mcp-inventory-click-context/1`. The envelope contains only schema,
challenge, control_epoch, capability, semantics and canonical view.

Action: `E` plus epoch32, capability32, command_id32, xDDD and yDDD, exactly
103 ASCII bytes. Existing generic errors, packet bounds, status API,
1.5-second native observation bound, 2-second client exchange timeout and
120-second consent lifetime remain unchanged.

## Verification boundary

Native UI parity and safety profiles pass 54/54 per game. Actual official-SDK →
production-game pickup/drop/refusal chains pass once in each game. The
[evidence report](SUPERVISED-INVENTORY-CLICK-EVIDENCE.md) preserves prior failures,
source-fault sensitivity limits, regression results and operator attestations.
Synthetic protocol tests alone do not establish local consent or item outcomes. Full campaign
acceptance, dedicated reload/prime proposals, broader inventory sections,
physical timing equivalence and model/token measurements remain open.
