# Monthly Costs UI and owned expense ledgers

UI schema 36 adds `open_monthly_costs` on the actual Base Information button
and `close_monthly_costs` on the ledger's ordinary OK button. Both require a
fresh visible control and invoke existing native handlers. Reading the ledger
or closing it does not allocate staff, spend funds or advance construction.

The `monthly_costs` presentation contains the shown title, three nullable
headings, rental/salary labels, income and global-maintenance text, craft and
salary rows with four nullable cells, maintenance and base-total rows with two
nullable cells, and the OK button. Each table is bounded to 32 visible rows.
Rows have no handles or selection authority. Captions include native formatting
such as dot leaders; unavailable cells remain null, without model substitution.

Current destination paint, viewport, identity/revisions, modal state and real
occlusion remain guarded. Native small-text sections use an eight-pixel logical
row band despite their nine-pixel allocation, so the adjoining section's border
does not erase a readable label. This is not an intact-glyph pixel test.

The separate `openxcom-player-bases/3` read adds campaign monthly income and
monthly maintenance, plus each base's expense ledger. Ledger entries carry
`type`, `unit_cost`, `quantity` and `total` for eligible craft rentals and
soldier-type, engineer and scientist salaries. The native researched/nonzero
rule selection is preserved, including eligible zero-quantity entries; locked
rules are not disclosed. Native counts include applicable in-transit personnel
and craft. These reads work from other ordinary windows and grant no UI action.

Focused checks pass: 62 native UI checks, 206 native owned-ledger checks,
17 protocol checks, and 131 selected adapter tests both normally and under
Python optimization. Native UI fixtures use real widgets and models with
explicit owning-Game/resource seams. Full discovery is still not passing:
1,577 tests retain the same 30 failure/error diagnostic IDs as the preceding
baseline. This does not waive the regression backlog.

UFO and TFTD live acceptance each loads the previously created arrival-complete
save through the normal UI, reads both owned-base ledgers from Options, opens
Monthly Costs, reconciles its shown rows/income/global maintenance/base total
with those reads, and returns to Base Information without owned-state mutation
or visual fallback.
The Quantity heading remains nullable in both observations; this is not a claim
that every stock heading or arbitrary modded report is fully readable.

No save contents are parsed, and IDs are not claimed continuous across loading
a replacement SavedGame. The full player-equivalent goal remains open.
