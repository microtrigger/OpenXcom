# Regression fixture contracts

The current UI action/state catalogs and protocol identity are checked against
`AgentProtocol.cpp`, not inferred from earlier feature-stage totals. Negative
cases continue to reject older schemas, unknown actions and actions not offered
by the current visible state.

Craft navigation and redirect are separate UI modes. Redirect button ordering
matches the native presented order. Normal and failure monthly-report controls
are qualified in separate fixtures rather than granting their union in either
screen. Capabilities remain fresh, single-use and invalid after dispatch.

Public events contain metadata, not inline PCM bytes. Public audio is fetched
through its separate endpoint. Partial event rows and duplicate command status
queries retain their existing assertions.

Advertised UI protocol metadata derives its version from the actual schema.
Save/Load is limited to offered ordinary UI actions and current visible file
rows; this does not grant unrestricted paths or implicit overwrite authority.
The independently authorized owned-base snapshot is read-only and never grants
an action capability.

These fixture checks do not replace live campaign acceptance. Historical-source
comparison tests require their exact pinned source artifacts; an unavailable
artifact is not a passing test and must not be replaced with a reconstructed
historical claim.
