# Printed two-column article stats — wire59

Stage63 adds required nullable `article_stats` alongside independent nullable
`item_combat`. Its exact object is `{rows, scroll}`: at most32 rows of exactly two
nullable printed strings, and strict boolean `scroll.up/down`. Native separators
and actual missing cells are retained, without fabricating omitted zero/default
stats. Existing per-string1024-byte and packet limits remain unchanged.

`details_status` is partial if either registered panel is non-null, unavailable
if both are null. It never becomes complete: images and other families remain
open. No family IDs, rule lookups, hidden resistance/default values or normalized
numeric fields are added.

`scroll_article_stats` takes only `{direction:"up"|"down"}`. It is offered only
for certified native stat-list controls (arrow or visible viewport wheel); the fresh advertised direction is required. Native wheel settings govern movement, endpoints may no-op, and no one-row distance or hidden row count is promised.
It is distinct from existing body-text scroll. No row selection is introduced.
Stale capabilities and duplicate command receipts retain existing protections.

This family covers registered craft-weapon, facility and armor stat widgets in
both games; availability and current viewport remain native. Adapter validation
and synthetic tests cannot prove final paint, clipping, overlays or live access.
