# Inventory selection — wire schema 54

Stage Inventory56 adds `previous_inventory_unit` and `next_inventory_unit` only
in `inventory`, with empty arguments and fixed semantic labels `Previous
inventory unit` / `Next inventory unit`. The visible shape does not change.

Each dispatch invokes exactly one ordinary native arrow handler. It is not a
promise of a different unit: native cycling can wrap to the same unit, refuse
while an item is held, or close through initialization. No unit identity, roster,
eligibility count, coordinates or next-unit preview is exposed. A fresh displayed
name and loadout establish the observed result; a name is not an identity.

No time-unit mode or cost is changed. Native held-item refusal is retained; the
adapter never cancels a hold or retries automatically. Native visibility, actual
button paint, overlays, state and actor binding remain authoritative. Synthetic
adapter tests are not proof of those native guards or of real campaign play.

Dedicated adapter tests cover noarg shapes, inventory-only scope, exact labels,
independent arrow absence, closed visible fields, zero-TU/held display acceptance,
stale capability refusal, duplicate receipt behavior and old schema rejection.
The broader player-equivalent goal remains open; this slice does not implement
inventory templates or other remaining controls.
