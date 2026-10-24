# Tactical viewport map context (private MVP)

`tactical.map_context` is required alongside the existing camera grid. It exposes engine-backed player knowledge, not OCR, glyph completeness or reconstructed game saves. Scope is only the current viewport and selector level; whole discovered-map paging is not implemented.

Shape: `scope: "viewport"`, nullable `selected_position: {x,y,z}`, `cells`, `truncated`. Each cell is paired in order with the same current `grid` handle (`gN`) and contains `position`, four `parts`, and `unit`. Handles are revision-local, not persistent map IDs. At most 128 pairs survive; compact grid-array plus cell-array length is at most 23500 bytes. Truncation is paired and explicit. Candidate order prioritizes the existing public camera-focus activation point by squared screen distance, then y/x. A bounded top-128 heap scans only public viewport lattice geometry; it never uses hidden map dimensions, occupants or selected-unit lookup. After ordinary center-selected-unit, the centered neighborhood therefore survives the unchanged packet-prefix budget.

Positions use the ordinary camera screen-to-map conversion, its `spriteHeight/4` input offset and native clamp. Cell x/y may be -1 at an off-map border, otherwise nonnegative; z is nonnegative. All are signed-32-bit integers. Selected own position is nonnegative. Off-map cells contain unknown parts and unknown unit, not fabricated empty terrain. Duplicate positions from different ordinary screen points are permitted.

The four parts are `floor`, `west`, `north`, `object`:
- Floor: `unknown | empty | floor`.
- West/north: `unknown | empty | wall | door`.
- Object: `unknown | empty | object`.

Content discovery (native bit 2) allows known empty/present parts only for cells visited by the actual terrain rendering branch. Without content discovery, native west/north discovery bits 0/1 admit only the corresponding actual door/UFODoor sprite branch; an ordinary wall or absent wall stays unknown. Missing sprites stay unknown. `wall` is generic: no invented window classification, passability, material, armor, resistance, voxel geometry or raw ruleset IDs. Door open/closed state is not supplied in this increment. Presence of a floor/object is not a movement-success prediction.

`unit` is `unknown | own | visible`. Unknown never asserts absence. `visible` means a native currently visible non-own unit; it is **not** a hostile-enemy designation. This MVP conservatively reports stationary origin cells only; moving units, secondary large-unit footprint cells and units seen from below remain unknown. Existing enemy-button controls retain their separate native meaning. Own selected position may be offscreen because the player controls and knows that selection.

The ordinary terrain UI reads current discovered terrain; this is not a new historical last-seen memory model. Dynamic changes already visible through native discovered-terrain rendering may legitimately alter the context. Hidden unit mutations must not alter projected facts or readiness. Debug visibility is not supported.

`map_primary` / `map_secondary` remain unchanged ordinary native activation actions. The context is included in their private authority binding together with existing selection, camera, input-mode and geometry guards. A dirty/remapped map, changed selected position or changed admitted semantic facts invalidates the old snapshot/grant. Overlay checks still apply to admitted grid targets. This does not automatically select, move, aim, fire, or replay an uncertain action.

Unavailable or hidden-movement projections contain empty cells and null selected position. No internal path vector, raw TU-cost lookup, hidden occupancy or AI state is exported. Native displayed path-arrow/TU feedback is a separate next increment; it is not claimed here.

Qualification: focused tests extract the literal projection/record/freshness methods with explicit Tile/Game/Camera and render-witness-seeding seams. They do not constitute actual renderer, full-build, saved-battle-resume, or live gameplay qualification. The future gameplay test resumes the existing disposable V8 save, uses these semantic observations to choose a known tile, checks ordinary movement, then acts only on an independently visible enemy if encountered. It does not require or guarantee a kill or victory.
