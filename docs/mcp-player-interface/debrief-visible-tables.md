# Debriefing: presented report cells

`debriefing.visible` retains `title`, `rating`, and the observational `stats_label`.
It now includes `table_status` (`shown`, `partial`, or `unavailable`) and `tables`:

- `stats`: `{status, rows: [{cells: [label, quantity, score]}]}`
- `recovery`: same three nullable string cells
- `total`: `{status, rows: [{cells: [label, score]}]}`

Per-table status is `shown` or `unavailable`. `shown` means the current painted
list projection was admitted, not that every campaign statistic or offscreen row
is present. Cells may be null. Empty rows do not establish that nothing was
recovered. Values come only from the currently painted native TextList logical
cells, including native formatting; they are not recomputed or parsed into game
statistics. No model score, hidden casualty identity, unpainted row, or unit state
is consulted. The soldier-stat page and its toggle remain outside this scope.

Fresh paint, list/cell revisions, capture epoch, clipped cell bands, later-painted
siblings and real overlay checks remain required. An ordinary cursor is not an
overlay. A bounded projection that cannot be admitted is explicitly unavailable,
not silently completed or replaced by model data. Native `close_debriefing` is
unchanged; closing is not evidence of battle success or campaign continuation.

## Logical-caption viewport contract

This is the established current-painted **logical widget caption** contract,
not OCR or proof that every glyph pixel is readable. A currently painted cell
whose allocation intersects the viewport may retain its logical caption even
when the flooding allocation extends horizontally. It never enumerates model
rows or wholly offscreen cells. Only complete vertical row strips from the
actual current draw are admitted. Hidden/stale widgets, destination clips and
real later windows remain gates; covered cells are null and fully covered rows
are omitted. The ordinary cursor is not a covering window. Exact formatting
color tokens are removed; visible dot padding is not guessed away.

The same capture now supports `lost_in_service`: `title`, `table_status`,
`rows:[{cells:[name-or-caption, blank-or-caption, rank-or-caption,
blank-or-caption, kills-or-caption]}]`, and existing `buttons`. All five values
are nullable displayed strings, not typed Soldier/Diary data or parsed totals.
Medal lines retain their displayed five-cell shape; no hidden classification,
row ID, selection, scrolling action or additional acknowledgement is invented.

Qualification uses isolated native widget/provider and adapter contract tests.
It does not establish live end-to-end acceptance of these new projections.
