# Ufopaedia ITEM combat panels — wire schema 55

Stage57 adds required nullable `item_combat` to `ufopaedia_article`. Existing
title, body lines, buttons and actions remain unchanged. Registered ITEM panels
are `details_status: partial`; all other article layouts remain `unavailable`
with `item_combat: null`. Partial does not mean full article/image equivalence.

The non-null object has exactly:

- `shot_headings`: three independently nullable displayed strings.
- `shots`: null or at most three rows, each exactly three nullable strings,
  preserving native mode/accuracy/TU column order and printed formatting.
- `damage_rows`: null or at most three `{type, power}` nullable string pairs.
- `damage_headings`: exactly two nullable strings in `[damage, ammo]` order.

Null means unknown/unavailable; empty lists do not enumerate hidden compatible
ammunition. Damage type is **not** ammunition name. No rule IDs, ammo names inferred
from images, hidden compatibility, clip size, arithmetic, image payloads or other
article stats are added. All text uses the existing renderer-string policy and
1024-byte per-string cap; the existing total packet cap remains in force.

Native registration, painted text completeness, per-cell clipping/overlays,
final-frame freshness and retained private widget binding remain the authority.
Adapter fixtures establish only strict shape/bounds and transport acceptance,
not native visibility, ordinary campaign discovery or research availability.
The wider player-equivalent UFO/TFTD goal remains open.
