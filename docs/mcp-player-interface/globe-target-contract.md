# Visible globe targets and target information

Geoscape exposes `targets` as observation-local marker handles with a single
currently painted screen-pixel witness. `click_visible_globe_target` accepts only
a fresh offered handle and invokes the native globe click at that witness. It
accepts neither world coordinates nor a persistent target identifier. A click
may open a chooser, a target information screen, interception, or another native
popup; dispatch alone does not establish the destination.

`multiple_targets` exposes only currently painted target-button labels as local
single-cell rows. `choose_globe_target` invokes the native button callback.
`cancel_globe_targets` is the ordinary keyboard cancellation, advertised with
control semantics rather than an invented painted Cancel button. Nearby targets
are disclosed only when the native chooser actually displays their labels.

`target_info` exposes the painted editable name, targetted-by heading, followers
text and Intercept/close buttons. `replace_target_name` uses the native edit
callback with safe UTF-8 text of at most 128 bytes; it changes the campaign target
name, not merely a draft. `focus_target_name` uses the ordinary editor click;
`finish_target_name` uses ordinary Return. Only current editor controls are
offered, and interception/close are unavailable while the name editor is modal.
Replacement follows native commit behavior; inspect focus state through the
newly offered actions rather than assuming the editor closed. `open_target_interception` opens the native interception
chooser; it does not launch a craft. `close_target_info` invokes ordinary OK.
No model IDs, hidden target positions, hidden follower records or world-coordinate
search are provided.

Own base and craft marker inspection does not expand the existing craft
assignment destination API. Final marker composition, lifetime, current-state,
paint, overlay and fresh-grant checks remain required. Speed handles and target
handles are separate authorities. Test only dedicated campaigns and inspect each
post-action state; do not replay an action with an unknown outcome. This contract
covers both UFO and TFTD but does not imply complete globe-navigation coverage.
