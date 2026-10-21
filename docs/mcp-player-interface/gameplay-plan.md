# Next gameplay qualification

Status: planned/under test, not completed live acceptance. Run UFO Defense and
Terror from the Deep separately with lawful stock assets and isolated campaigns.
The previous bounded checkpoint covered research completion, article navigation,
staff release, manufacture detail/Cancel and retained-history paging; it did not
qualify production orders or varied allocation quantities.

## 1. Allocation and production quantity matrix

| Surface | Cases | Expected evidence |
| --- | --- | --- |
| Scientists | 0, 1, several, large, all available, laboratory limit, one more at limit; decrease to zero and below | Allocated/free scientists and lab space conserve staff; rendered values agree with MCP; rejected/no-op requests do not over-allocate. |
| Engineers | Same cases, separately personnel-limited and workshop-limited | Allocated/free engineers and workshop space, including pre-existing projects. |
| Manufacture units | 1, several, large, finite set boundary, over-boundary request, decrease/clear, max, repeated max | Finite/infinite mode, produced floor, correct quantity and receipt. Non-craft max is **infinite**; clear is **produced + 1**, not zero. Craft quantity uses its own hangar rules. |
| Lifecycle | Back/reopen, Start/accept, permitted Cancel, stop, completion of multiple units | Verify persistence, staff release, costs/material consumption and visible inventory result; do not assume Cancel rolls back an existing edited project. |

Compare offered controls, one fresh-authority action/receipt and the resulting
visible UI after each step. Unknown outcome stops the route: never replay it.
Exercise ordinary left/right click, wheel and hold/repeat where actually bound;
exercise MCP single-step and quantity adjust/set/max/clear where actually offered.
Keyboard/direct text entry is a coverage item only if the screen exposes it; do
not invent unsupported MCP methods. Current scientists expose single-step MCP
increase/decrease, while production supports bounded quantity intent. Large UI
and semantic requests need separate tests: their input ranges are not identical.

Qualification layers:
1. Asset-free current-source arithmetic regression with explicit domain seams.
2. Native widget/controller tests: visibility, offered actions, valid/stale
   authority, schema rejection, refresh, warning and cancellation semantics.
3. Real UFO/TFTD routes and screenshots: matched visible numbers, multiple-item
   completion, return paths and resources. This alone can establish live coverage.

`run-allocation-matrix.py` covers layer 1 only, not rendering, MCP dispatch,
resource charging, cancellation or actual game campaigns. Its seams and result
metadata must remain explicit. Do not label the matrix exhaustive.

## 2. Transfers: source versus destination capacity

Queued after the allocation/production matrix. Test full and overfull **source**
and **destination** independently, for both games. The requested hypothesis is
that outbound stock can leave an overfull source while inbound stock is refused
at a full destination; verify actual game rules rather than changing them to fit
that expectation. Vary quantity and item size, inspect visible refusal reasons
and offered MCP actions, check Cancel without mutation and both bases' visible
outcomes. Keep stores, living quarters, hangars and alien containment distinct;
one resource-capacity result does not qualify the others.

Include the causal live route **mission debrief → recovered loot accepted into a
base → overfull stores → outbound/inbound transfer comparison**. Verify that loot
and transfer quantities are not lost and MCP does not assume an overfull save is
invalid. A seeded fixture or replay can supplement this but cannot stand in for
a completed tactical mission and its actual debrief/loot flow.

## 3. Bounded interception route

Then qualify detection → launch interceptor → approach → combat and available
combat modes → observable success/failure or withdrawal → return, separately
for UFO/TFTD. Inventory existing coverage first. Use only offered MCP commands
and visible damage/outcomes, never hidden enemy statistics. Include interrupted
and repeated transitions where safe; a successful single interception is not
exhaustive air-combat coverage.

## 4. Geoscape event-type inventory and coverage

Inventory **all** event types for both games from source and public UI, with
per-type checked/pending/not-applicable status and evidence links. Qualify modal
interruption, time pause, offered buttons, Cancel/return/resume, and repeated
events where applicable. A few alert examples cannot stand in for all event
types. Source inventory is not proof of live handling; retain that distinction.
