# Commendations title and acknowledgement (experimental schema 26)

`commendations` exposes exactly `title` (the current active Text string or null)
and `buttons` (the admitted OK label). Report rows are deliberately **unexposed**;
read-only two-column viewport coverage remains a separate open increment. No
soldier identifiers, medal criteria, decoration levels or diary statistics are
read or reconstructed by the provider.

`close_commendations` takes exactly `{}` with fresh single-use authority for the
shown OK control. It invokes the native pop-only callback once. The next state may
be another queued continuation, not necessarily Geoscape. No automatic next click
or retry after an uncertain outcome is permitted. Exact duplicate requests query
the existing receipt rather than dispatching again.

The ordinary constructor already calls `makeOld` on displayed new commendations.
That constructor behavior is not an MCP acknowledgement effect and is not changed
here. Observation and OK do not award medals or recompute diary statistics.

Active/top/modal, full destination paint, widget visibility/viewport, current
presentation/text revisions, real later-widget occlusion and action hitbox guards
remain. Logical current title text has no glyph-ink, fit or cursor-readability
gate; resource bounds remain. The unexposed list can still occlude an admitted
widget through ordinary allocation geometry. This does not expose report contents.

Tests distinguish literal constructor/provider/callback with explicit domain and
render seams from a full game or live encounter. Scoped sanitizer exclusions and
aggregate outcomes are reported separately. This increment has no GUI/live claim,
no tactical entry and no report-row completeness claim.
