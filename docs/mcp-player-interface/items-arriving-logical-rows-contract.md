# Items Arriving: current logical viewport cells

This increment changes only the existing `items_arriving` row observation policy. It adds no action, public field, model query, transfer mutation or UI-layout change.

The screen opts into a separate default-off three-cell TextList capture. Each cell comes from the currently painted Text widget, not a Transfer or Base. Logical captions do not require fitting glyphs, a single line, available glyph coverage or unchanged ink pixels. Existing arrival normalization removes only the color-flip control token. Bounds, identity, actual visibility, paint destination, clipping, allocation geometry, text/presentation revisions and real occlusion still apply. Unknown cells remain null. Entirely unavailable/empty rows retain the existing omission policy.

The stock list is271pixels wide, with margin2 and columns155/41/98. The last column's native allocation intersects the viewport over73pixels; its25pixel overhang does not truncate the logical string. Native9pixel cell allocation with spacing−1 advances8pixels; only that content-independent row strip is observed. Wholly offscreen rows/cells are not recovered. Malformed same-list layout is rejected before offscreen omission. Cosmetic cursor position is not a row-text readability gate; later widgets and noncursor overlays remain checked. Actual OK, Go to Base and scrolling controls retain full hitbox/cursor exclusion.

Private capture epoch advances on actual enabled draw, not cached blit, and fails closed on overflow. Cell identity/revisions and list paint provenance are checked before capture is returned. Provider admission additionally requires the list presentation to be painted to the current Game screen; a same-sized replacement destination is not equivalent to the recorded old target. Private authority includes list identity/revisions, epoch and geometry. The provider preserves these five context entries after its two scroll flags, and scroll dispatch requires the exact seven-entry current binding. None of this private context becomes public IDs or coordinates.

Title/headings retain their existing policy in this row-only increment. In particular, the stock DESTINATION heading extends beyond the320pixel screen and can remain null. No inferred heading or base identity substitutes for an unavailable field. Other TextList modes and legacy helper defaults are unchanged.

## Ordinary callbacks, not delivery commands

The native constructor already processes completed transfer records, may call ordinary craft reuse for applicable item rules, deletes completed transfer objects and remembers the last processed base. Observation must never repeat this work. Actual inventory delivery is not proven by opening this popup.

`dismiss_items_arriving` remains ordinary pop-only. `open_arrival_base` remains timer reset, pop, then opening Basescape for the popup's stored base/globe. It is not a row selection action. `scroll_visible_list` remains the ordinary current admitted arrow operation. All public schemas and argument kinds are unchanged.

## Qualification boundaries

New native fixtures use literal constructor/provider/callback code with explicit Game/Mod/domain/render/overlay seams. Capture of the pushed Basescape arguments is not actual Basescape integration or a live transfer scenario. Privacy, stale authority, logical-text, same-object opt-in and scoped legacy parity are qualified separately from aggregate adapter tests. Historical unsupported observations are not attributed to this source-policy mismatch.

A separate private one-GoTo harness v2 accepts documented nullable headings while retaining exact sole item/quantity/base row and fresh action ownership. Frozen v1's pre-live heading overconstraint is preserved as a compatibility bug, not retrospectively passed. Inventory live permission remains separate.
