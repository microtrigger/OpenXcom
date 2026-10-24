# Promotions: explicit native acknowledgment

This increment supersedes only the actionless boundary of the earlier title and
viewport contracts. Existing `promotions` title, three headings and three current
widget cells per admitted row retain their privacy, viewport and freshness rules.
No row identity, selection, model getters, hidden rank or promotion criteria are
added. The report is not reconstructed from soldiers or bases.

`visible` has exactly `title`, `headings`, `rows`, `buttons`. The last field is
empty when no control is admitted, or contains the current visible OK caption as
`{"id":"ok","label":...}`. `close_promotions` takes exactly empty arguments and
is offered only with that actual current control. There are no row/scroll actions.

The normal `PromotionsState::btnOkClick` callback only pops this state. It does not
perform promotion, consume promotion flags, change rank, award medals, select
Geoscape speed or promise a Geoscape continuation. The ordinary constructor has
already called `Soldier::isPromoted`, which consumes its recently-promoted flag.
Projection and acknowledgment never repeat that work. Debriefing can queue this
screen above commendations; inspect the actual later state without automatically
acknowledging another continuation.

Admission binds the active top state, modal exclusion, current destination paint,
widget presentation and label revisions, full widget geometry and actual button
hitbox ownership. Later-widget and cursor/FPS overlays can withhold the action.
Current logical captions have no glyph/pixel-fit/readability gate. Hidden/stale
widgets cannot supply authority. No fallback click, retry or inferred success.
The unchanged controller grants/ledger govern duplicate and stale requests.

Qualification must distinguish native-widget/constructor callback fixtures with
explicit domain/render seams from a full game or live run. Required negatives
include stale/hidden/moved/occluded OK, wrong action, repeated dispatch after pop,
and preservation of constructor-consumed flags and current viewport privacy.
Native parser and adapter require the empty-argument action shape. No live or
battle setup is authorized by this contract.
