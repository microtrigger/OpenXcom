# Freshness test fixtures and negative checks

Two fixtures were preventing their intended authority checks from being reached.
The production validators and native handlers are unchanged.

The Geoscape fixture now maps `open_interception` to the actual shown
`intercept` button caption, as the native provider does. Previously it invented
an action-name caption, which the strict validator correctly rejected before
dispatch. An explicit negative test still rejects inferred captions and an
action whose corresponding shown button is absent. Existing single-use,
stale-membership, duplicate-status-only and uncertain-outcome cases remain.

The empty tactical viewport fixture now clears both the grid and its paired
map-context cells. Native projection creates or clears these together; leaving
one populated is invalid, not a valid empty-view test. Empty-grid camera action
refusal remains tested. Added negative cases reject mismatched arrays and
reordered or incorrect context handles, even without a camera action.

The Geoscape group passes 32 tests and the viewport group passes 35, both
normally and with Python optimization. This is fixture/contract validation,
not a new live gameplay session or proof of an engine defect. The full
regression suite and broader player-interface goal remain open.

Full discovery changes from baseline `c477d250` (1,577 tests, 14 failures,
16 errors, seven skips) to 1,580 tests, 14 failures, seven errors and seven
skips. Exactly these nine error diagnostics disappear; none are added:

- `test_agent_viewport.ViewportValidationTests.test_camera_pan_requires_current_nonempty_map_grid`
- `test_agent_geoscape.GeoscapeClientTests.test_connected_navigation_each_handler_once`
- `test_agent_geoscape.GeoscapeClientTests.test_duplicate_status_only_changed_args_conflict`
- `test_agent_geoscape.GeoscapeClientTests.test_fresh_projection_replaces_membership_and_state_clear`
- `test_agent_geoscape.GeoscapeContractTests.test_geoscape_unknown_model_and_old_field_limits`
- `test_agent_geoscape.GeoscapeClientTests.test_largest_fresh_handles_are_actionable_without_identity`
- `test_agent_geoscape.GeoscapeClientTests.test_real_server_invalid_params_and_structured_output`
- `test_agent_geoscape.GeoscapeContractTests.test_speed_selection_exactly_matches_shown_pressed_label`
- `test_agent_geoscape.GeoscapeClientTests.test_unlisted_handles_do_not_consume_fresh_grant`
