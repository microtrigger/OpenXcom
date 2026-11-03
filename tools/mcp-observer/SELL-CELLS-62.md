# Sell62 caption/value evidence — unchanged wire58

This slice improves native evidence for the existing four independent nullable
Sell cells: name, remaining quantity, draft quantity and unit value. Public shape,
schema, actions and argument kinds do not change. Shown captions/prices stay
formatted strings; unknown or clipped cells remain null independently.

A caption or price is not an item identity, stock lookup or permission to sell.
Revision-local row handles and painted arrow controls remain mandatory; a stale
row cannot be revived by matching its old caption. Quantity changes are one-step
drafts. Only the separately offered ordinary accept action commits a sale;
replaying an adjustment receipt must not repeat the adjustment or commit it.

The adapter does not certify the native glyph/clipping/overlay proof. Synthetic
acceptance tests and native packet validation are separate from stock-renderer
and live campaign evidence. No other null fields or broader game coverage are
closed by this slice.

Live62 passed in both games on unchanged wire58: full ordinary row names/values, launcher draft +1/-1, one native scroll, Cancel/reopen zero quantities and exact own-record equality (144 UFO /111 TFTD). Bottom-clipped values remain null. No sale committed, no visual fallback. Audio qualification and private test-save hashes are retained separately; this does not complete the full interface.
