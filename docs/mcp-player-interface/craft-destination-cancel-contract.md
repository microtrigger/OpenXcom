# Destination picker: cancel and visible-marker selection

Schema26 additively adds `open_craft_destination`,
`cancel_craft_destination`, and `craft_destination_selection`.
The action catalog is additive; arbitrary point/waypoint selection remains open.

The existing zero-waypoint `geoscape_craft_navigation` window now offers
`target` / `open_craft_destination` only when its actual Select New Target
control is currently admitted. Native nonnull-waypoint mode remains entirely
unsupported. No extra model eligibility, fuel, destination or research query
is made by either provider. The game's own hidden Target control stays absent.

The ordinary Target callback pops that window and opens SelectDestination
with the same craft/globe. The picker exposes exactly `{title,targets,buttons}`:
nullable current logical title, admitted `cancel` / `cancel_craft_destination`,
and at most32 surviving painted marker-pixel witnesses. Each target is exactly
`{row,x,y,width,height}`; `row` is an ephemeral sequential `r0`..`r31` handle.
Each rectangle is a 1×1 pixel remaining in the final painted marker layer, not
a whole-marker bounding box or a glyph-completeness claim. Fully offscreen or
overpainted attempts do not consume the visible-choice budget.
Coordinates are logical **screen pixels**, never world coordinates. No target
name, model ID, hidden roster, eligibility or target properties are exported.
The native provider additionally checks actual canvas/Globe containment and
paint/occlusion; adapter rectangle bounds alone do not prove actual visibility.
No arbitrary map coordinates, waypoint creation, zoom/rotation, Return To Base,
Patrol or Cydonia actions are exposed.

The existing navigation/cancel route consists of three explicit commands:

1. `redirect_patrolling_craft`: patrol popup → craft navigation.
2. `open_craft_destination`: craft navigation → destination picker.
3. `cancel_craft_destination`: picker → Geoscape, **not** the popped craft window.

Opening runs ordinary presentation behavior, including stopping globe
rotation; the native picker may display a craft-range overlay. Closing clears
that displayed range through the destructor. These are not destination
mutations. Cancel invokes the ordinary pop-only callback. It does not click
the globe, create a waypoint, select or confirm a target. **Actual destination
point/waypoint selection and confirmed retargeting remain OPEN.**

`choose_visible_craft_destination` takes exactly `{row:"rN"}` from the fresh
visible marker list. Its fixed semantic label is `Select visible target`; this
is not an invented target caption or a literal arbitrary coordinate click. It
selects only the privately bound, still-live painted target through the native
single-target confirmation path, without consulting `Globe::getTargets` or
hidden mission-site presence. Selection does not itself confirm a destination.
Separate confirmation remains required. Empty targets means no selection action.
Cached authority is invalidated by paint/view changes or target lifetime changes;
no global hidden-roster counter is serialized or used for admission.

Current top/modal, paint health, paint destination, widget revisions, viewport
geometry, genuine later-widget occlusion and control ownership remain guarded.
The picker title uses its full native allocation, without a glyph/ink/fit/line
count admission test. Pointer overlap alone does not hide the logical title;
actual control overlap still prevents the affected action. Resource limits
remain bounds on serialized text, not layout tests. No UI layout is changed.

Each step requires fresh authority. Verify the subsequent visible state;
receipts do not establish success. Exact duplicates query existing receipts,
and uncertain outcomes stop without replay. Observing the picker never selects
anything automatically. Existing global cursor motion is not a map click.

Native tests use literal constructor/provider/callback bodies and native
widgets with explicit Game/domain/render seams; synthetic adapter tests cover
strict DTO/action ownership and the three-request flow. These are not full-game
or live proofs. Opening/Cancel coverage must never be reported as completing
the destination-selection goal.
