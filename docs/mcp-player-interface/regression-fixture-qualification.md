# Adapter regression fixture corrections

Four test files were corrected to match existing native contracts, without
changing production transport limits, action grants or visibility checks.

- The Geoscape catalog expectation now includes the implemented interception
  action.
- Gameplay fixtures update `map_context.cells` together with their tactical
  `grid`; event-only handles still cannot grant actions.
- The fake event server respects the native conservative ASCII byte budget as
  well as the record-count limit. PCM sequence numbers are monotonic. Tests
  verify complete retained pagination, no holes, identical rereads and no
  action dispatch rather than expecting sixteen oversized PCM frames at once.
- The retained multibyte research-event fixture fits native admission bounds;
  the separate oversized-event rejection test remains.

The selected 225-test sample passes normally and with Python optimization.
The 34-test research-completion sample also passes in both modes. Full adapter
unittest discovery remains **not passing**: 1,554 tests report 14 failures,
16 errors and 7 skips, compared with 16 failures, 19 errors and 7 skips on
baseline `942af3509f6891b292761f8cc5858ad57a00aaed`. The final comparison adds no
failure/error diagnostic and removes five:

- Geoscape `ListValidationTests.test_closed_list_action_matrix_kinds_and_sources`.
- `GameplayValidationTests.test_action_catalog_requires_the_corresponding_visible_set`.
- `GameplayClientTests.test_hidden_hand_and_event_only_handles_cannot_broaden_action_grant`.
- `EventClientTests.test_history_retained_independently_and_exact_public_ring_gap`
  and its failing teardown.

Other catalog-count, descriptor/order, viewport-parity and repository-history
fixtures remain unresolved. Matching a baseline does not waive those failures,
and the narrower passing samples are not a full-suite acceptance claim.
