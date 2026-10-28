# Visible facility placement

UI schema 33 adds `place_facility_cell` with the `facility_cell` argument kind:
exact integer `x` and `y` cell coordinates from 0 through 5. These are base-grid
coordinates, not desktop coordinates or arbitrary pointer input.

The current `place_facility` presentation adds `grid`, either null or an object
with `columns`, `rows`, `origin_x`, `origin_y`, `cell_size` and `cells`. The
standard grid has six columns, six rows and 32-logical-pixel cells. Offered
`{x,y}` cells are unique and ordered by row then column. They describe the
currently shown interactive grid, not placement eligibility.

Read-only `read_player_bases` may supply all owned layouts, footprints and
construction state for planning without navigating bases. That snapshot is
never action authority. Placement requires a fresh single-use UI capability,
the corresponding actually open base placement window, a currently offered
cell and real-overlay/current-presentation checks. It cannot act on a hidden
base. A cell can be occupied or disconnected: the ordinary native handler,
not the MCP provider, decides whether construction is allowed.

Dispatch uses a real native Action with the BaseView sender, normal mouse-over
cell selection and the registered mouse-click handler. The native handler
retains all ordinary placement, funds, charging and construction rules. A
`dispatched` receipt alone does not establish success. Read the successor
window and owned-base snapshot to verify the new footprint, exact funds change
and remaining construction days. Starting construction is not finishing it.

Existing preview and Cancel remain separate actions. Native error dialogs use
their existing explicit acknowledgement; an error is not successful placement.
The wider player-equivalent interface and rare-scenario matrix remain open.

Focused native placement checks (85), preview regression (181), protocol,
strict native-to-adapter projection checks and the production build pass.
The native placement fixture uses the real BaseView rendering/hover/click and
base/facility/funds models with literal placement-handler bodies. Game ownership,
synthetic resources and error-popup construction are explicit fixture seams;
ordinary error-dismiss callbacks are checked. Native cases cover occupied and
disconnected cells, insufficient funds, success costs/layout/construction time,
translated coordinates, real overlays, freshness and replay refusal.

Short live UFO and TFTD test campaigns each plan from their owned-base snapshot,
open the actual General Stores placement window and start one construction via
MCP. Each creates exactly one footprint at the selected visible cell, deducts
$150,000 and reports ten construction days remaining. Gameplay uses MCP with
passive video and no visual fallback. Construction completion, queued building,
multi-cell placement and dismantling are not qualified by these short runs.

The selected 230 adapter tests pass normally and with Python optimization.
Full discovery remains not passing: 1,559 tests, 14 failures, 16 errors and seven
skips; the failure/error IDs match baseline `bab31b5` exactly. This is not a
waiver of the remaining regression backlog or a claim of full interface completion.
