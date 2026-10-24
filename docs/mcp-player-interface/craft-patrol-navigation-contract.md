# Narrow patrol redirect and safe craft-window close

This records the original two-action increment. The later
[picker-open/Cancel extension](craft-destination-cancel-contract.md) adds the
shown Target control; actual destination selection remains open.

Schema26 additively adds `redirect_patrolling_craft`,
`cancel_craft_navigation`, and `geoscape_craft_navigation`.
Catalog:152 actions,65 semantic states.

CraftPatrol exposes OK then Redirect when their real controls are admitted.
Redirect invokes the ordinary handler: pop CraftPatrol, then open the craft
window with the same craft/globe and a null waypoint. It does not select a new
target. The new window's exact visible object is `{title,status,buttons}`;
title/status are nullable current logical Text values, not recomputed telemetry.
Only the admitted Cancel button/action is exposed.

The provider rejects the native nonnull-waypoint mode entirely: its apparent
Cancel handler would assign the last-known-position waypoint. The private
branch-ownership check is not a serialized model value. Current localized
Cancel label and hidden redirect heading are additional consistency checks,
not a localization-based inference of mode. Return To Base, Patrol, Select New
Target, waypoint/map selection and go-to-last-known-position are unsupported.

The admitted Cancel invokes the ordinary zero-waypoint callback, which only
pops the window. It returns to Geoscape, not the already-popped patrol popup.
It does not alter destination or speed. No automatic choice is made.

Active/modal/current-paint/widget-revision/geometry/control-ownership guards
remain. Logical captions are not gated by glyph ink, missing characters,
line count, width or text fitting. Stock title/status allocations are 17px high and share their final spare row
with the next caption. For those exact allocations, the structural observation
strip is 16px; other allocation heights use their full height. This boundary is
independent of the current font (which Text can auto-switch based on content). Full widget viewport/presentation checks still apply. Genuine
later-widget occlusion of the retained region and FPS overlays are checked;
cursor overlap alone does not suppress captions. This is not a claim that all
caption pixels are readable. No game UI layout is changed.

Use fresh authority for each action. A receipt is dispatch evidence, not proof
of navigation success; inspect subsequent UI. Duplicate request handling,
ledger limits and unknown-outcome no-replay policy are unchanged.

Native fixtures extract actual constructor/provider/callback bodies and use
native widgets with explicit Game/domain/render seams; Redirect's destination
constructor can be a capture seam in its separate callback test. Adapter tests
are synthetic-channel tests. Full build and live two-action qualification
remain independent gates; no complete craft-management capability is claimed.

## Later ordinary REDIRECT mode extension

The historical nonnull-mode refusal above is superseded only for the native
visible REDIRECT window. The exact visible object now also requires `redirect`,
a nullable current logical caption. Null-waypoint Target/Cancel behavior stays
unchanged. The nonnull constructor-owned waypoint is paired with the visible
REDIRECT heading and last-known-position button; private weak lifetime and
identity guards reject expired/replaced ownership without reading coordinates.

Only shown controls authorize these empty-argument commands:
- `return_craft_to_base`: ordinary pop, return request, temporary-waypoint delete.
- `patrol_craft_current_position`: ordinary pop, clear destination, temporary delete.
- `go_to_craft_last_known_position`: ordinary waypoint registration/destination
  assignment and pop. **This is not Cancel**; it does not prove reacquisition.
- `open_craft_destination`: ordinary picker transition and temporary deletion.

No choice is automatic. Only dispatch is receipted, not arrival, patrol movement,
reacquisition or combat. Hidden telemetry, world coordinates and target IDs are
not exposed. Current painted controls/captions, freshness, occlusion and modality
remain required; glyph readability is not a runtime condition. Synchronized
native/adapter exact-shape readers fail closed on old/missing or new/extra keys;
unknown new action IDs are rejected by old native peers. No wire framing change.

The stock REDIRECT allocation overlaps the first row of Return To Base. For
those exact owned native rectangles only, authority uses the remaining visible
11-pixel button hitbox. The caption remains a full 17-pixel occluder; no glyph
transparency is inferred. Full widget paint/viewport and all later overlay and
cursor checks remain, and changed geometry does not inherit this exception.
