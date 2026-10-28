# Build Facilities selection and preview

Schema 32 adds the ordinary Basescape `open_build_facilities` route. The
`build_facilities` presentation contains the rendered title, bounded visible
facility-name rows, shown scrolling directions and close button. A fresh local
row handle permits `select_build_facility` through the ordinary list handler;
`close_build_facilities` returns without choosing a facility.

The resulting `place_facility` presentation contains only the displayed
facility, cost, construction-time and maintenance labels/values and its Cancel
button. `cancel_place_facility` returns through the ordinary cancel handler.
Unproven or covered text remains unavailable. No rule identifiers, hidden
research requirements, placement eligibility or internal facility list is
serialized. Freshness, current-paint, top-state and real-overlay guards remain
required.

Selection and preview alone do **not** place a facility, spend funds or
establish construction. Schema 33 separately adds the visible-grid action
specified in [facility placement](facility-placement-contract.md). The
read-only all-base snapshot supplies owned layouts for planning, not action
authority. Choosing a row or seeing a preview is not completed construction.

The facility list uses the GUI caption actually drawn in the current viewport,
its native row index and active click band. It does not require pixel-perfect
glyph preservation: normal adjacent glyph overlap and the native selection band
do not remove otherwise visible, interactive rows. Child-text presentation,
viewport clipping, top state, real overlays and current presentation remain
checked. The generic list projections are unchanged.

Focused native checks (181), adapter and production build pass. Short UFO/TFTD
live runs expose all nine stock facility rows, preserve the selected General
Stores row after preview/Cancel, and correctly open the previously omitted
Workshop through its fresh native row handle. Both return through Cancel/Close
with unchanged owned-base snapshots. These checks repair the observed missing-row
gap; they do not qualify actual facility placement or every modded list layout.
The wider regression suite is not fully passing; see
[fixture qualification](regression-fixture-qualification.md) for the exact
baseline comparison and remaining limitations.
