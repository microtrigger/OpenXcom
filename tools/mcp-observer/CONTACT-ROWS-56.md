# Printed contact tables — wire schema 56

Stage58 adds `contact_rows` and `hyperwave_rows` to `ufo_contact`. Each is null
or at most four rows of exactly two independently nullable strings, in current
native label/value order. Each string retains the existing 1024-byte limit and
renderer-string rules. Packet bounds and all existing actions remain unchanged.

`details_status` is `partial` if either table is non-null, otherwise
`unavailable`. Neither status asserts complete interception/gameplay coverage.
A null table/cell is unknown. Empty rows do not enumerate hidden information.

Native visibility is essential: the constructor populates the hyperwave list
even during ordinary radar contact. Hidden or stale child contents must never be
exported. Only the list's own current visibility, paint and cell witnesses can
permit the hyperwave table. Adapter structure alone cannot prove that condition.

Preserve printed localized strings without numeric coercion or model fallback.
No contact ID, position, hidden race/mission, inferred flight status, tracking
history, row actions or inferred glossary labels are introduced. Existing
intercept/dismiss actions retain their normal side effects and freshness guards.
The displayed Center button does not gain a new action in this slice.

Dogfight damage graphics, minimized/multiple-window layouts and remaining full
player-equivalent coverage remain open. Synthetic adapter checks and native
packet validation are not live campaign acceptance.

## Bounded live acceptance 2026-10-29

Build `3088c3d0af10bb69647f1f15b224eb6c5f060cedf30684d43a84c492aaba24d5`, dedicated test campaigns, pure MCP with passive real-PCM video: UFO radar reported SMALL / VERY LOW / SOUTH WEST / 480; TFTD reported SMALL / AIRBORNE / SOUTH / 650. These are native printed values, not inferred world state. Hyperwave rows stayed null in both ordinary contacts. Intercept then Cancel returned the same contact and same values; Dismiss returned to campaign and a unique test save was made. No visual fallback or unknown replay.

Visible hyperwave remains native-fixture-only, not live accepted. GROUNDED is a separate untested branch; ordinary radar acceptance does not imply complete contact/dogfight coverage.
