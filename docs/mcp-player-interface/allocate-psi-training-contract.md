# Psi allocation (experimental agent control)

`allocate_psi_training` exposes only current active widgets: nullable `title`
and `remaining`, four nullable `headings`, visible `rows` with ephemeral `rN`
handles, four nullable `cells` and a `selected` flag, shown `scroll` directions,
and the OK button. Names, strength/`Unknown`, skill/improvement and Yes/No are
copied from the list's current Text cells. No hidden statistics, eligibility,
numeric capacity, persistent soldier IDs or research state are reconstructed.
Duplicate names are not identities. Row order is privately bound to the complete
constructor roster; changed membership or order refuses stale targeting.

- `toggle_psi_training {"row":"rN"}` invokes one ordinary left-click callback.
  Enrollment changes immediately, not in a draft. Adding at capacity silently
  does nothing; removing frees a slot. Inspect a fresh row and remaining label.
- `scroll_visible_list {"direction":"up"|"down"}` performs one ordinary step
  from a shown arrow. Obtain new handles after scrolling.
- `close_psi_training_allocation {}` invokes native OK: recalculates each
  soldier's stat string, then pops. It is not Cancel, rollback, or training.

There is no sorting, base selection, desired-state assignment, automatic retry,
training-completion claim or guarantee of the following screen. A receipt proves
only dispatch; uncertain outcome means stop and reconcile without replay.
PsiTraining's base captions remain read-only in this increment.

The TextList logical-cell mode is explicitly enabled only by this screen.
Existing projections are unchanged. It retains current paint, dirty/revision,
actual viewport, allocation and occlusion guards without glyph-ink, fitting or
character-completeness admission. Native fixed caption allocation overlaps use
content-independent structural regions: title's 17-pixel allocation retains16,
remaining's10 retains8, and strength heading's80-pixel width retains64 before
its neighboring heading. Full widget viewport bounds still apply, and logical
strings are not cropped. These regions are ownership checks, not a promise that
every glyph is pixel-readable. Action hitboxes retain cursor/overlay checks.

Additive schema26:162 actions/72 semantic states. Native fixtures use actual
constructor, list and callbacks with explicit Base/Soldier/resource/game seams;
not full-game or live psi qualification. Natural live prerequisites remain
pending. No saved-game editing, inventory sale or tactical prerequisite bypass.
