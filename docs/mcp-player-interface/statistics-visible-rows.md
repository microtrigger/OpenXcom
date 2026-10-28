# Memorial Statistics — agent-control/45

`open_statistics` uses only the currently displayed Memorial Statistics button
and invokes its ordinary callback. It takes no arguments. It does not select a
campaign, reconstruct hidden information, or authorize navigation from a stored
own-base snapshot.

The `statistics` view exposes exactly:

- `title`: nullable displayed caption.
- `table`: `status` (`shown` or `unavailable`) and `rows`. Each row has only
  `cells`, exactly two nullable displayed strings. Unavailable requires no rows.
  At most 128 current viewport rows are accepted; there is no row identity,
  selection handle, total row count, numeric conversion, or offscreen table.
- `scroll`: boolean `up` and `down`, reflecting currently admitted controls.
- `buttons`: the currently admitted `ok` button with its displayed caption.

Rows come from already painted widgets. Ordinary screen construction computes
its vanilla report; observing through MCP does not recompute campaign statistics
or query underlying models. Null means unavailable text, not zero. An empty
viewport does not prove an empty campaign history.

`scroll_visible_list` uses one fresh advertised direction and the ordinary
scroll handler. A boundary may be a normal no-op. Report rows are read-only.
`close_statistics` invokes ordinary OK: during an ongoing campaign it returns to
the previous screen, while an ended campaign follows the existing disposal and
main-menu continuation. A dispatch receipt does not guarantee either successor.

Fresh single-use authority, real overlay checks, and no-replay receipt semantics
remain unchanged. Read fresh state after dispatch. Synthetic adapter tests cover
shape, authority, bounds, labels and receipts, not a full campaign or native
report calculation.
