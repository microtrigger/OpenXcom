# Playable save/load increment

Four explicit actions: `select_new_save_slot` (empty args), `replace_new_save_name` (`text`), `save_new_game` (empty args), `load_saved_game` (`row`, an advertised ephemeral rN handle).

Save is intentionally NEW-only. Existing rows are read-only; no overwrite, rename-existing, delete or original-save import capability. Ironman save presentation is not supported. Keyboard-off character-preview mode can still be observed, but it does not grant save-name editing/saving. Input obeys the ordinary widget's character and width constraints; no glyph/ink completeness admission test is introduced. Save names are bounded and cannot contain path separators/control characters.

Rows contain only actual current viewport captions: name/date/time, or one NEW-slot caption. `new_save_name` is the actual currently painted editor child caption. No filename, path, raw timestamp, mod list, serialized campaign, hidden ironman flag or filesystem hash is exported. Identical display names retain separate fresh handles. Handles and permissions must be reacquired after UI mutation, refresh, sorting, scrolling or screen replacement.

`save_in_progress` and `load_in_progress` expose only current painted status-message text and no actions. Native delayed think callbacks remain unchanged. A dispatch receipt establishes only that the ordinary handler was invoked; it does not establish successful persistence or loading. Fresh actual UI and visible list readback are required, followed by explicit load for a round-trip qualification. Read waits are bounded to known transitions. If the sent action's result becomes unknown, stop; never automatically resubmit Save or Load.

Loading explicitly replaces the active campaign through the native row callback. It may lead to Geoscape, Battlescape, Statistics, an error, or the native missing-content confirmation. This first ordinary-save increment does not automate the missing-content prompt: unsupported remains an explicit stop, not consent or automatic Yes. No claim of guaranteed Geoscape return.

Qualification distinction: literal constructors/widgets/handlers with explicit Game, model, path-operation and queued-state seams are not disk-persistence or full-game proof. The normal game persistence backend is unchanged; actual end-to-end disposable save/load and audio/video qualification remain a separate gate. Existing user files are never touched by the native fixture.

The row Date/Time cells are the filesystem-modification timestamps already shown by the native UI, not the saved campaign clock. In-game date/hour/minute appears only in the native hover details. Do not infer the campaign clock from the row timestamp or inspect private save files through MCP.

NEW-slot collision avoidance is the unchanged native sequential check, not an atomic reservation: another writer could create the chosen filename before the deferred save writes it. This increment does not fix that native filesystem race. Actual qualification therefore requires a new isolated disposable profile with no other writer; it does not claim race-free overwrite protection for shared save directories.

### Save-only readability correction

The initial implementation's no-glyph-gate claim was incorrect: name replacement
called `agentEditable()`, which transitively required legacy glyph completeness.
The follow-up passes the current canvas directly into the save-only replacement
helper and requires its logical painted-child capture, focus and KEYBOARD_ON.
Generic editor admission remains unchanged. A native regression paints a missing
glyph, demonstrates the legacy refusal and then successfully replaces the name;
current-destination/focus/keyboard negatives remain enforced. The corrected native
fixture passes84 checks across both masters in normal and scoped-sanitizer modes.
These are not persistence or live-game results; the original qualification is
preserved separately rather than relabelled.

Viewport filename pairings can conservatively invalidate availability when an
external catalog mutation affects a covered row. No filename values are exported.
General external-writer behavior is not qualified; the first actual roundtrip
requires a fresh isolated sole-writer profile and stops on observed uncertainty.


### Visible native popup transition

While the current painted save/load window is running its ordinary popup animation,
the provider exposes the existing list kind with null captions, empty rows and no
semantic actions. Current canvas, viewport, actual hidden/visible occlusion and
non-cursor overlays are checked. The exact timer-running predicate includes the
final interval before native showAll; no timing fraction or catalog data is exposed.
Both subclasses explicitly short-circuit this private presentation result. Once
animation stops, the previous catalog/cell/freshness checks apply unchanged.
Generic unsupported remains fail-closed; the private harness waits only after its
own acknowledged navigation to an expected supported menu, without resending.

Scoped native qualification:140 checks across both masters in normal and scoped
sanitizer modes, plus10+10 validator tests. The first linked fixture compile failed
on a nonexistent getter and is preserved; the correction uses the shared agent-only
Surface::agentBlitEnabled accessor, not a global admission relaxation. These results
do not establish disk persistence, live roundtrip success, or the exact internal
predicate behind earlier unsupported observations.

### Decorative caret versus semantic save-name binding

A deterministic actual-constructor fixture demonstrated that ordinary100ms caret
blink changed the draw-epoch binding despite unchanged logical name/input revision.
The immutable characterization is preserved separately; it does not prove the
historical cause of a live unavailable receipt, whose edit outcome stays unknown.

The NEW save editor now binds private logical child identity/text revision and
current canvas identity rather than decorative capture draw epoch. Parent input
and structural revisions, actor/catalog identity, visible fields and regions stay
in the binding. Every projection/dispatch still revalidates the complete current
painted child capture; no unpainted `_value` fallback exists. Child presentation
revision is still checked for current capture validity but is not an equality token:
native draw calls child setText even when only the caret blink changes. Generic
BaseInfo/Basescape capture semantics, native timer/draw/edit handlers, ledger and
transport guards are unchanged.

Final scoped native qualification182 checks/two masters in normal and scoped
sanitizer modes: blink preserves queued semantic binding; actual text/caret/focus,
parent or child geometry, current canvas, child identity and catalog changes still
invalidate or refuse. Existing missing-glyph/rollback/privacy tests remain. This is
not fullgame/disk persistence proof; unknown sent edits are never automatically replayed.
