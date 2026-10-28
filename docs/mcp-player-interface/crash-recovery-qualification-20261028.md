# Crash-recovery modal contract

Schema 28 adds `crash_recovery` with the currently displayed `message` and the
no-argument `acknowledge_crash_recovery` action. Its label is the visible OK
caption. Native dispatch uses the ordinary UI handler and does not infer or
modify crash outcomes. No hidden game-state fields are exposed.

The provider requires current top-state, painted controls, current widget
revisions and clear overlay regions. Unavailable or obscured presentation does
not authorize acknowledgement. Observation capabilities and transport
uncertainty remain fail-closed; a dispatch receipt is not success evidence.

The code has bounded native and contract checks, including visible-message,
freshness, clipping, coverage and wrong-state cases. These checks are not a
live mission acceptance. The new modal-to-debriefing MCP route remains
**live-unqualified**. Future acceptance requires a fresh displayed modal,
one authorized acknowledgement and a fresh debriefing observation.

This change does not complete the full player-equivalent UFO/TFTD interface.
