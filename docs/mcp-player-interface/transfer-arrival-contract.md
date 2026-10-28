# Ordinary transfer and natural arrival

Bounded UFO and TFTD runs load the previously UI-created construction-complete
save through the normal load screen, verify two owned bases and completed
storage, and transfer one visible ammunition item into the second base. This
qualifies an existing route; it adds no action or schema change.

The runs select a visible destination and item, increase the draft quantity by
one, review the displayed cost, cancel confirmation, review again and explicitly
confirm. Draft editing and confirmation cancellation preserve the exported
owned resources and funds. Normal campaign-clock advancement is not described
as a byte-identical snapshot. Canceling the entire transfer draft is a different
branch and is not claimed here.

| Game | Visible item | Source stock | Charge | Native transfer duration |
|---|---|---|---|---|
| UFO | Pistol Clip | 8 to 7 | 9 | 6 hourly ticks |
| TFTD | Dart Clip | 8 to 7 | 18 | 7 hourly ticks |

Each explicit confirmation creates one in-transit item. The corresponding
craft inventory remains unchanged. Destination storage usage already includes
the incoming item's reservation; a positive used-storage value alone does not
prove that the item has arrived.

Ordinary Geoscape time then produces `items_arriving`. Its visible row identifies
the item, quantity one and destination. Owned-base reads show destination stock
increasing from zero to one and the transfer record disappearing, without a
second charge. `open_arrival_base` invokes the native callback and opens the
correct destination Basescape. The UFO run also dismisses a public contact
notice through its offered control; neither run launches combat.

The arrival screen's destination heading is null in these observations while
the relevant row is readable. This is bounded row/action acceptance, not a
claim that every heading or arbitrary arrival list is readable.

Loading verifies the prior saves' content through ordinary UI and owned-state
reads; no identity continuity across replacement of SavedGame is asserted.
The original save hashes remain unchanged. Each run creates a distinct new
save and verifies its presence in the UI. The subsequent
[Monthly Costs](monthly-costs-contract.md) stage loads these arrival-complete
saves normally in both games and verifies their two-base owned state. Save
contents were not parsed, and cross-load ID continuity is not claimed.

Gameplay uses MCP without visual fallback. Full/overfull capacity, personnel,
craft, alien containment, multiple simultaneous arrivals and debrief-loot
transfer chains remain separate qualification work. The wider interface goal
and existing regression-suite backlog remain open.
