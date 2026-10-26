# Base-name replacement: ordinary width refusal

The initial base-name editor is 127 logical pixels wide and uses the big font.
Wire-valid UTF-8 text (up to 128 bytes) is not a guarantee that the native
editor can accept it. Character and rendered-width restrictions still apply.

`replace_base_name` invokes the current offered editor. A valid but refused
atomic replacement is now a **handled request with no committed name edit**,
not a generic unavailable result. `applyNameChange(false)` runs only after a
successful replacement; refusal does not update the base model, reveal OK,
or confirm the name. Read the fresh displayed name before deciding another
action. A dispatched receipt never establishes successful replacement.
Malformed input still fails explicit native `agentInputText` validation;
fresh offers, modal/visibility/overlay checks and frame bindings remain intact.
The general controller's false-dispatch classification is unchanged.

Glyph/render rejection can redraw the restored editor and change presentation
revisions. The promise is no committed name change, not byte-identical UI
internals. No automatic retry is added, including after an unknown outcome.

## Observed trigger and isolated qualification

V22 on 2026-10-26 returned unavailable for `MCP Second Campaign` and stopped
without replay. Its blank editor was subsequently reconciled visually by the
operator, followed by a new save; this was mixed control, not pure MCP.

With stock UFO fonts, native measurement established:

- `MCP Second Campaign`: 175 pixels, exceeding the 127-pixel field; atomic
  replacement refused and the blank prior name/input revision were retained.
- `MCP Two`: 72 pixels, accepted.

The isolated actual provider/dispatcher/editor fixture passed 15 checks,
including valid edit, refusal from blank and nonblank names, malformed input,
hidden/modal/cover rejection, and prior-frame invalidation. Owning-state/model
objects are explicit test seams. This is not new live acceptance or a TFTD
qualification; it does not replay or roll back V22.
