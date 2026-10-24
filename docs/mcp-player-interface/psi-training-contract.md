# Psi-training selector (experimental agent control)

`psi_training` exposes only the active UI's logical `title` (nullable), `bases`
(array of at most eight currently admitted base-button caption strings), and
`buttons`. Duplicate or empty captions are retained. Hidden, stale or occluded
captions are omitted: array positions are not handles, base identities, counts of
all bases, capacity, eligibility, or training statistics. Text admission does not
require glyph/ink completeness, fitting, or an unobscured cursor. Active screen,
modal ownership, current paint, allocation bounds, revisions, other-widget and
FPS occlusion guards remain; the actionable OK retains its cursor/hitbox guard.

The only action is `close_psi_training`, with exactly `{}` arguments and the
fresh single-use authority for the shown `ok` button. It invokes the native OK
handler, which pops this screen once. It neither selects a base nor changes
soldier allocation, trains anyone, or establishes a training result. Base
selection and `AllocatePsiTrainingState` are a separate, unimplemented increment.
A dispatch receipt is not a guarantee of the next screen. Unknown outcomes stop;
never replay or automatically choose a response.

This is additive schema 26 coverage (160 actions / 71 semantic states), not a
new wire version. Native fixtures use actual extracted constructor/provider/OK
and native widgets with explicit game, resource and domain seams; they are not a
full-game or live training qualification. Natural live setup requires ordinary
researched, completed psi facilities and the normal monthly flow; no live
coverage is claimed here. Tactical prerequisites must not be skipped or synthesized.
