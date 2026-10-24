# Tracking-lost notification: bounded MCP contract

Schema 26 adds `ufo_lost` and the empty-argument action
`acknowledge_ufo_lost`. The exact visible object is `message` (nullable current
logical caption) and `buttons` (only admitted `ok`, with its current label).
The message comes only from the active `_txtTitle` widget, never a contact lookup,
model identifier, coordinates, detection flag or reconstructed destination.

OK invokes the ordinary native popup-close callback once. It does not reacquire
a contact, redirect or return a craft, or establish its next destination. A
receipt means dispatch, not a guaranteed Geoscape return; inspect the actual
continuation and stop on unknown outcome without automatic replay.

Current-state/modal ownership, current paint and widget revisions, viewport
allocation and later-widget/FPS occlusion remain required. Caption admission
uses logical text, not glyph ink, text fitting or pointer readability. The
actual OK hitbox retains control-overlay checks. Hidden/stale captions are null;
a missing admitted OK leaves this provider unavailable. Text resource bounds
are protocol limits, not layout tests. Unsupported transitions stay unsupported.

Native qualification uses extracted actual constructor/provider/callback and
native widgets with explicit domain/render/Game seams; it is not a full-game
or live scenario claim. Adapter tests separately cover strict fields, empty
arguments, fresh authority, duplicate receipt queries and unknown transitions.

A natural tracking-lost popup requires an ordinary pursuing follower at the
native trigger. Passive disappearance alone is not sufficient. A future bounded
visible-contact pursuit may end inconclusively if no such popup occurs; no
reload/reroll, inventory-sale workaround or hidden target inspection is implied.
Actual free-globe destination selection remains a separate blocked design item.
