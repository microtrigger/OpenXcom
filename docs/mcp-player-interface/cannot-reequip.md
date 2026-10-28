# Cannot Reequip — agent-control/43

`cannot_reequip` presents the ordinary post-mission re-equipment warning. Its
payload contains exactly:

- `title`: nullable displayed text.
- `headings`: exactly three nullable displayed text strings.
- `rows`: at most 128 read-only viewport rows, each exactly
  `{"cells": [item_caption, quantity_caption, craft_caption]}`. Cells are nullable
  strings, not item IDs, craft IDs, or numeric quantities.
- `scroll`: exactly `{"up": boolean, "down": boolean}` for admitted controls.
- `buttons`: ordered subset of `[{"id":"ok","label":"OK"}]`, using the
  actual nonempty localized displayed label.

Each caption is bounded to 1024 UTF-8 bytes. Null means unavailable visible text,
not zero, no missing equipment, or no craft. Rows carry no handles, model totals,
selection identity, or offscreen inventory. Empty projected rows do not prove
that the full underlying report is empty.

## Actions

`acknowledge_cannot_reequip` takes `args: {}` and invokes the ordinary visible OK
callback. Its label must match the visible button. Closing may reveal Promotions
or another stacked state; it does not guarantee a return to Geoscape or prove
that any equipment was replenished.

The existing `scroll_visible_list` takes only `{"direction":"up"}` or
`{"direction":"down"}`. It requires a currently advertised scroll action and
that exact visible direction. It makes one ordinary scroll step; a boundary
no-op remains possible. No row-selection action is available.

Existing paint admission, state guards, fresh single-use capabilities, and
receipt rules remain unchanged. Read again after an action; dispatch alone
proves no outcome. Never automatically retry an uncertain mutating send.

`test_agent_cannot_reequip.py` covers strict shape, nullable strings, numeric
quantity rejection, read-only rows, button and scroll authority, wrong-screen
and extra-argument rejection, and synthetic socket freshness/idempotence. These
are contract checks, not live campaign outcome qualification.
