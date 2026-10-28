# Debriefing STATS/SCORE — agent-control/44

`toggle_debriefing_stats` is a no-argument ordinary button action on both report
views. On `debriefing`, it uses the current displayed `stats_label`; the existing
score payload is unchanged. On `debriefing_soldier_stats`, it uses the displayed
`stats` button caption (normally localized SCORE). Toggling does not acknowledge
or close the report.

## Soldier-stats payload

The exact keys are:

- `title`: nullable displayed string.
- `headings`: exactly 12 nullable displayed strings.
- `table`: exactly `status` (`shown` or `unavailable`) and `rows`. Each row is
  exactly `{"cells": [...]}` with 12 nullable strings. Unavailable requires an
  empty row array; at most 128 current viewport rows are admitted.
- `tooltip`: nullable current displayed tooltip.
- `scroll`: exactly boolean `up` and `down`.
- `buttons`: ordered subset of `ok`, then `stats`, each exactly `id` and a
  nonempty displayed localized `label`.

The native dummy thirteenth column is never exported. Values remain displayed
strings, including native dot padding; no numeric conversion, invented zeros,
model stats, soldier IDs, row handles, selection identities, or hidden totals.
Null means unavailable displayed text. Empty rows do not establish an empty
underlying soldier list. Individual captions are bounded to 1024 UTF-8 bytes;
the existing aggregate packet bound still applies.

## Control authority

`close_debriefing` uses the ordinary OK control from either view and may advance
the report stack; it promises no particular destination. The stats-page
`scroll_visible_list` accepts only one fresh advertised `up` or `down` direction.
Rows cannot be selected. A visible caption alone does not grant a hidden button.

Paint/state guards, fresh single-use capability rules, and command receipts
remain unchanged. Dispatch proves only handler dispatch. Obtain a fresh read to
establish the new view or scroll result; never retry uncertain mutations blindly.

`test_agent_debrief_stats.py` checks exact shape and 12-column bounds, nullable
captions, readonly rows, label/direction authority, preserved score shape,
malformed arguments, and synthetic socket freshness and duplicate receipts.
These tests do not qualify live campaign results or statistic computation.
