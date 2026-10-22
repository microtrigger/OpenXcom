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

The [2026-10-22 bounded qualification](../../tests/agent-control/INTERCEPTION_QUALIFICATION_20261022.md)
now covers one natural-contact cautious-attack/withdrawal/return route in each
game. It does not establish hits, destruction, every combat mode or every warning.
The next narrow warning surface is the depth/airborne engagement popup: expose
its two explicit choices without deciding for the player, then separately verify
actual pursuit or return. Native handler coverage is not natural-event coverage.

## 4. Geoscape event-type inventory and coverage

Inventory **all** event types for both games from source and public UI, with
per-type checked/pending/not-applicable status and evidence links. Qualify modal
interruption, time pause, offered buttons, Cancel/return/resume, and repeated
events where applicable. A few alert examples cannot stand in for all event
types. Source inventory is not proof of live handling; retain that distinction.

## Manufacture limits milestone — 2026-10-22

`tests/agent-control/research-live/run_manufacture_limits.py` is a separate,
manual-entry launcher. Supply `--source`, `--binary`, `--data`, `--master`, a new
`--output`, and a nonexistent `--focus-ready-file` (optional `--sdk`). Using ordinary
UI, prepare a legal campaign with the stock pistol researched, open an **empty
Manufacture list**, focus the game, then create that marker. The bounded setup wait
is at most 480 seconds. Run it within the working graphical session: on Linux,
`DISPLAY` must identify that desktop. Other platforms use their native graphical
session; this is not a platform-global `DISPLAY` requirement or a focus bypass.
This launcher does not automate research, load/copy saves,
forward audio, advance campaign time or manufacture completed items. Missing
prerequisites stop; there is no hidden unlock or automatic retry.

| Check | Evidence and limit |
| --- | --- |
| Engineers 1/7/10/100000, max, above available, clear | UFO live: personnel-limited clamp; separate workshop-limited case remains pending live (scripted unit model only). |
| Units 1/7/1000/100000, decrease, clear | UFO live; 100000 is the adapter finite-input bound, not resource/storage capacity. |
| Units 100001 | Exact JSON-RPC -32602 rejection, then unchanged fresh visible projection; not a native over-limit dispatch. |
| Units max twice, clear | UFO live: max means infinity; clear means produced+1, here 1. |
| Accept 7 units/7 engineers, reopen, clear and Stop | UFO live persistence and restored empty list/staff/workshops. Stop is not Cancel; no time advancement or completion claim. |
| Funds | Observed 4,134,000 → 4,126,000: accepting the first item can charge funds. No refund or inventory-increment claim. |
| Other inputs/cases | TFTD live, keyboard/wheel, independently workshop-limited and material/funds/storage-full cases remain pending. |

The UFO run finished **2026-10-22 14:48:23 UTC**, with 29 dispatched action receipts
and one expected schema-rejection probe; inputs were unchanged. It used a private
instrumented recording wrapper and operator UI setup. The public route core and
its used helper methods were compared by AST with that measured implementation
(excluding only the reporting-key rename `dispatches` → `requests`, since the
30-request budget includes the rejected probe);
this is code-equivalence evidence, **not a new live run of the public launcher**.
The public launcher has unit qualification only. Its transport additionally
requires exact JSON-RPC 2.0 response envelopes with matching integer IDs and a
result; malformed replies stop without replay. Scripted transport tests also
cover unknown outcomes, absent actions, changed readback and existing-order
refusal; they do not establish native game-rule coverage. Native/game/schema code
is unchanged. Every unknown action outcome stops without replay or cleanup calls.
