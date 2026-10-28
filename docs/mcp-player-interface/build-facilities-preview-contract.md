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

This bounded slice does **not** expose a base grid, place a facility, spend
funds, or establish construction. The separately authorized read-only all-base snapshot provides owned layouts;
a qualified visible-grid action target and ordinary placement route are still
necessary for mutations. Choosing a row or seeing a
preview is not a completed construction.

Focused native (78 checks), adapter and production-build checks pass. Short UFO/TFTD live runs qualify the General Stores selection, displayed
cost/time/maintenance and Cancel/Close route, with unchanged base snapshots.
A TFTD visual diagnostic identified a real completeness gap: some visibly shown
facility rows, including the highlighted selection after Cancel, are omitted
by the projection. Full list coverage is not qualified; correct this witness
before claiming complete selection or proceeding to placement. The wider regression suite is
not fully passing; those existing failures are not waived by focused checks.
