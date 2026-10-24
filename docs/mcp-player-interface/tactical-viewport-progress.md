# Tactical viewport progress — 2026-10-24

The current private build adds bounded engine-backed viewport context to existing tactical map controls: tile coordinates, independently known/unknown terrain parts, selected own position, and conservative visible unit markers. Targets are fresh grant-local handles. Center-prioritized geometry keeps the centered neighborhood within the unchanged payload limit. No hidden enemy state, armor, AI, voxel data or invented passability is exposed.

## Actual gameplay evidence

A resumed disposable battle was played through MCP, with 161 native dispatch receipts, including 49 primary map activations, 11 end-turn requests, 22 continuation actions and 8 weapon-menu choices. The route included movement/exploration, enemy contact through visible enemy buttons, Snap/Aimed/Auto selections, an observed ammunition change from 20 to 9, and an ordinary unconscious-unit notification.

These observations do not prove hits, kills, victory, debriefing, or a complete campaign loop. A dispatch receipt confirms handler dispatch, not its gameplay outcome. A non-own visible map marker is not automatically an enemy.

The run stopped when a subsequent read timed out (`select_timeout`, approximately 3962 ms), before the next planned end-turn dispatch. The stopped action was not automatically replayed. After termination, one separately identified visual NEW-save operation preserved the test campaign without replacing the prior save; that operation is not presented as MCP-only route coverage.

## Qualification and remaining limits

Focused native/Python checks and the current build/link passed within their documented scopes. The full regression suite remains **NOT PASS**; this is not blanket gameplay readiness or a claim that prior intermittent failures are fixed. Native model/render-witness fixtures are separate from actual live evidence. The initial LeakSanitizer execution failed in the available ptrace environment; the narrower ASan/UBSan run excluded leak checking.

The first map increment remains viewport-only. Terrain objects/walls are generic; window and door-open classification, reliable passability/line-of-fire feedback, displayed path preview, and explicit hit/unit-outcome projection remain open. There is no hidden-state fallback and no guaranteed successful attack or completed mission.

Publication integration preserves the previously published failure-classification and safe-evidence changes while adding the newer local development. The publication tree must be reviewed and qualified separately from the frozen build used for this session.
