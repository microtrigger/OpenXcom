# Read-only presentation events and action authority

A retained `presentation` event carries only the visible projection, not the live
UI grant or offered actions. Its validator checks the same closed fields, labels,
row order, geometry and resource bounds, but does not require the action-presence
equivalences for nonempty psi-training rows or craft destination targets. Fresh
UI state validation still requires those exact equivalences. No action is inferred,
constructed or authorized from event history.

A saved native-generated V9 public destination projection deterministically exposes
the old mismatch: its nonempty target list is valid with the offered live action,
but the old read-only event path supplies an empty action set and rejects it. The
failed V9 event packet was not retained, so this reproduction does not establish
that packet's exact contents or retrospectively attribute its failure.

The focused integration test requires `PRESENTATION_PUBLIC_FIXTURE` (saved public
projection JSON, no raw private game state) and `PRESENTATION_BASELINE` (old validator
directory). Without both it explicitly skips; with provided malformed inputs it
fails. Scoped qualification must supply both. No full-suite or new live claim.
