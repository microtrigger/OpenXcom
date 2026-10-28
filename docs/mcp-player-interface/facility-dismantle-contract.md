# Visible non-lift facility cancellation and dismantling

UI schema 34 adds `click_base_facility_cell` for the actually open Basescape
window, with exact integer `x,y` cell coordinates 0–5 and the same closed grid
geometry as facility placement. A fresh offered cell is required. For Basescape,
the authority region is the actual ordinary center click target, not every
decorative pixel of the 32-pixel cell: a native header touching its border does
not hide an otherwise unobstructed click. Real overlays covering the target
still withhold the action; the entire BaseView presentation must be current.
The command
uses ordinary BaseView hover/click and the registered native base handler; it
does not precompute occupancy, use, connectivity or successful removal.

The native handler can do nothing for an empty cell, show an ordinary error, or
open `dismantle_facility`. That presentation contains shown `title` and
`facility` text, `confirmation_allowed` and currently exposed `buttons`.
`cancel_dismantle_facility` invokes native Cancel. The separate explicit
`confirm_dismantle_facility` invokes native OK only for a non-lift structure,
with current shown identifying text and button evidence. Both take empty args.

Access-lift OK can delete a whole base. This bounded contract therefore exposes
Cancel only for lift confirmation; it does not qualify or authorize base
abandonment. `confirmation_allowed` describes the non-lift safety gate, not an action
grant by itself or a hidden prediction that the earlier facility-click will
succeed. The current action and its shown button must also be offered.

All mutations remain visible in their corresponding actual window with fresh
single-use capability, top-state, presentation and real-overlay guards. The
read-only owned-base snapshot can plan a cell and verify the result, but never
mints or broadens an action grant.

Native economics remain unchanged: removing already-started construction does
not refund its cost; the native unstarted-queue refund is a separate case.
Dispatch is not proof of removal. Verify exactly the intended structure was
removed, the base remains and the actual funds/layout match the native result.

Focused native checks (76), strict schema/protocol checks, native-to-adapter
samples and production build pass. The native fixture exercises the real
BaseView, full Basescape provider/dispatch, ordinary facility-click and popup
handlers, with explicit Game/resources/base-constructor seams. It verifies
Cancel, non-lift removal, native economics, lift confirmation refusal, empty
caption refusal, fresh target guards and stock-shaped hover-header overlap.

Final short live UFO/TFTD test campaigns each start one non-lift General Stores
as setup, open its visible confirmation and Cancel with unchanged snapshot,
then reopen and explicitly confirm. Exactly the new structure disappears;
the base and other facilities remain and the spent construction cost is not
refunded, matching native started-construction semantics. These final runs use
MCP gameplay with passive video and no visual fallback. An earlier UFO attempt
was stopped before an unoffered click; one read-only visual inspection revealed
the overly broad cell-border check that the target-center regression fixes.

Selected 236 adapter tests pass normally and with Python optimization. Full
1,565-test discovery remains at the same 14 failures, 16 errors and seven skips
as baseline `309e0e3`, with identical failure/error IDs. Separate legacy
constructor and items-navigation runners remain blocked by baseline-identical
audio/Game test-seam compile/link dependencies; their runtime tests did not run.
The historical logical-name runner requires unavailable `ae1529` source and is
not reported as passing. No access-lift abandonment, queued-refund live route,
whole dismantling matrix or whole-project completion is claimed.
