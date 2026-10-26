# Terminal campaign UI controls

The ordinary end-of-campaign sequence now has three distinct, explicit controls:

- `acknowledge_monthly_failure {}`: only the painted large monthly failure OK,
  using its existing `btnOkClick` callback. The normal report remains
  `acknowledge_monthly_report`. Admission uses visible widget mode, not hidden
  defeat criteria. The failure callback can queue the native ending; it is not
  a generic close, success, save, or rollback operation.
- `advance_slideshow {}`: one normal current-slide `screenClick`, not skip-all.
  Only the current caption (nullable) is projected. “Advance current slide” is
  an API semantic label, not a claim that these words are drawn on the slide.
  Future captions, slide counts, filenames and underlying rules are omitted.
- `close_statistics {}`: existing statistics OK callback. Only current title
  (nullable) and the visible button are projected; statistics rows and hidden
  totals are not recomputed or exposed. The native callback can dispose the
  ended campaign and return to the main menu. Dispatch is not proof of saving.

All actions require a fresh painted current state and its offered control.
Full destination paint, revisions, actual window/overlay checks and the
controller's one-use capability/command ledger remain required. The ordinary
cursor is not an occluding window. A timed slide change invalidates the prior
slide binding; no automatic retry or automatic ending traversal is authorized.
An intentional current caption over the clickable slide is not another input
window. Actual later overlapping surfaces still block the slide action.

The wire schema remains 26; the closed client catalog is now 182 actions and
80 states. Use the matching adapter; older closed parsers cannot accept the
new state/action inventory.

## Triggering live observation and acceptance limits

On 2026-10-26, the UFO V21 campaign reached an actual terminal campaign loss.
That session was **mixed MCP and visual recovery**, not a pure-MCP completion:

| UTC | Gap and one visual recovery | Observed result |
| --- | --- | --- |
| 12:39:49 | Monthly failure visible OK absent from MCP; one click | Ending presentation; MCP resumed and reported unsupported |
| 12:40:32 | Ending slideshow unsupported; Return once | Defeat statistics; MCP resumed and reported unsupported |
| 12:40:57 | Statistics visible OK unsupported; one click | MCP main menu, then session finish |

This patch fixes those observed control gaps after the session. It does not
retroactively classify the session as native-only, replay the loss, undo it,
or establish a new live acceptance of the added actions. Isolated fixtures use
explicit owning-system seams and are not live gameplay evidence. TFTD assets
were unavailable for this qualification; no TFTD acceptance is claimed.

## Isolated qualification

- Full candidate native build: passed.
- Monthly-report native fixture: 111 checks passed.
- Statistics native fixture: 45 checks passed, with actual constructor tracking
  extracted; widget construction and owning campaign objects are explicit seams.
- Slideshow native fixture: 23 checks passed, using actual source-extracted
  provider/dispatcher/callbacks and constructor tracking assertions.
- Focused Python contract suite: 26 tests passed; terminal/monthly subset also
  passed 18 tests under optimized Python.
- Broad Python discovery: 1502 tests, 6 failures, 19 errors, 7 skips. No new
  failure/error names relative to the preserved pre-change baseline; this is
  **not** a full-suite pass. Existing unrelated fixtures/tool-count assumptions
  and missing private fixture dependencies remain out of this scoped fix.

Early isolated tests exposed missing presentation tracking for the new slide
and statistics widgets. The constructors were corrected and tests rerun before
qualification. Failed precursor logs were retained privately, not overwritten.
