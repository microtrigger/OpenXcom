# Transfer public-UI milestone

This adds the normal Basescape Transfer route, not a new transfer rule:

`basescape/open_transfer` → `transfer_destination/choose_visible_row` →
`transfer_items/change_transfer_quantity` → `review_transfer` →
`transfer_confirm/confirm_transfer`.

Cancel confirmation returns to the draft; cancel items discards the draft;
only the ordinary confirmation callback commits. Every command requires a fresh
capability. A dispatch receipt does not establish acceptance or shipment: inspect
subsequent rendered values, warnings and both bases' ordinary Stores/Transfers UI.

The initial all-items list is supported. Category dropdown control is deliberately
not included; an open dropdown, modal, running arrow-repeat timer, unhealthy paint,
or wrong top state fails closed. Projection uses rendered labels, clipped list-cell
ink and admitted visible arrows. Unknown cells remain null. No numeric capacity is
invented for screens that do not display it.

## Checks

From this checkout, with outputs **outside** it:

```sh
python tests/agent-control/run-transfer-rules.py --output /tmp/transfer-rules-new
python tests/agent-control/run-transfer-rules.py --sanitize --output /tmp/transfer-rules-asan-new
python tests/agent-control/run-transfer-providers.py --output /tmp/transfer-providers-new
python tests/agent-control/run-transfer-providers.py --sanitize --output /tmp/transfer-providers-asan-new
c++ -std=c++11 -Wall -Wextra -pedantic src/Observer/AgentProtocol.cpp tests/agent-control/AgentTransferProtocolTest.cpp -o /tmp/transfer-protocol
/tmp/transfer-protocol
(cd tools/mcp-observer && python -m unittest test_agent_transfer)
```

Rules tests extract actual transfer callbacks and `Base::storesOverfull`, with
explicit domain seams. Providers tests extract actual providers/dispatchers and
binding logic, with synthetic paint/widget/game/controller seams. Neither is
live-game, final-pixel or socket-controller proof.

Current ordinary non-alien item rules allow outbound transfer from overfull source
stores. Full/overfull destination rejects positive-sized items. Exactly-full stores
can accept zero-sized items; already-overfull stores reject those too. These are
observed controller rules, not a replacement policy. Personnel, craft, alien
containment and mission-loot acceptance need separate coverage.

## Outstanding live qualification

Run real UFO and TFTD routes with ordinary UI-created two-base scenarios, checking
source-full/overfull and destination-full/overfull separately, quantity changes,
visible refusal, cancellation and committed totals at both bases. Mission debrief
may force ordinary selling when storage limits are enforced; `SellState` remains
unsupported, so this milestone must not bypass or claim that causal route covered.

### Earliest bounded live route without custom fixtures

Build-new-base and build-facility MCP controls are not provided. Set up an isolated
campaign using ordinary human-visible CUA: choose a second base location and name,
then return to the first base. Do not edit saves, inject inventory or infer model
state. Record both bases' visible BaseInfo/Stores values and screenshots.

With a newly founded second base lacking storage facilities, try a positive-sized
ordinary item from the first base using the new MCP route. Record the visible
capacity refusal, unchanged draft quantity and cancellation back to the source.
This proves **zero available destination storage**, not a full stocked warehouse,
not overflow caused by mission loot, and not successful delivery.

For a positive commit route, use CUA to build the required storage facility and
advance the campaign normally until the UI shows it completed. Then transfer a
small visible quantity, check source decrement and destination pending Transfers,
and advance ordinary time to verify arrival. Later full/overfull and cancellation
matrices remain distinct scenarios; do not rename this first setup as their proof.
