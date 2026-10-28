# Visible alien containment management

UI schema 37 adds `right_click_base_facility_cell` to the current visible
Basescape grid. It dispatches the ordinary right-button callback with the
actual BaseView sender. The native facility determines which ordinary window
opens; this action does not promise containment for arbitrary grid cells.
Existing left-click placement and dismantling routes remain separate.

The `manage_containment` presentation contains the displayed title, available
and used space, four nullable headings, visible four-cell rows, current scroll
controls, and only the actually shown Remove Selected and Cancel buttons.
Each row has a fresh opaque handle and separately witnessed increase/decrease
arrows. `increase_containment_removal` and `decrease_containment_removal` invoke
one ordinary arrow click, including native row selection and press/release
handling. There is no arbitrary quantity, item-ID or hidden-base mutation API.

All actions require the actual open current window, current paint and viewport,
revision/capability freshness and clear action targets. Real overlays remain
blocking. The dedicated four-column TextList capture is opt-in; it does not
relax other list contracts. Rows are described from the displayed allocation,
not by requiring every glyph pixel to survive unchanged. Full text must fit
its visible logical text extent without clipping or a covering native arrow;
otherwise that cell stays null. Missing headings or cells stay null. The own-base read snapshot remains independent of UI and does
not authorize these actions.

Draft arrows use native clamping and do not change owned inventory. Cancel
abandons the draft. `remove_selected_aliens` is the ordinary **Remove Selected**
button, not a harmless close: it removes the drafted live aliens. With the
native selling option disabled it adds their corresponding corpses; with that
option enabled it credits the native sale value instead. Native storage limits
can subsequently open the storage error/Sell route. Overcrowding can hide
Cancel and Remove until the draft resolves the excess. The MCP offers only
buttons actually shown, not assumptions based on those conditions.

## Qualification boundary

The bounded route has native checks and synthetic live acceptance in UFO and
TFTD. Synthetic own-stock acceptance is not evidence of natural capture in
combat. Other removal policies and unusual branches have separate coverage;
not every containment or modded scenario is live-qualified. Full regression
is not passing, and the full player-equivalent goal remains open.

## Native test tooling

Reusable test launchers are maintained separately on the
`tools/native-test-runner-20261028` branch. This gameplay branch contains the
native fixture sources and API under test, not the launcher scripts. Consult
the tool branch for its supported gameplay revision and integration status;
the generic runner core alone does not qualify the containment fixture.
