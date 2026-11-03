# Normal craft navigation — wire schema 57

Stage59 admits existing Return to Base and Patrol actions when their ordinary
buttons are actually painted in normal navigation (`redirect: null`). No action,
argument, callback or visible field is added. The previous redirect-only filter
incorrectly suppressed two normal player controls.

Button evidence remains mandatory. Low-fuel and mission-complete modes that hide
controls do not gain authority. Last-known-position stays redirect-only; Cancel
stays absent in redirect mode. No model fuel/status, hidden destination or
eligibility flag is exposed. Native callbacks retain their usual side effects;
a dispatch receipt does not prove return, patrol, arrival or mission success.

Native paint/overlay/freshness and binding checks remain unchanged. Adapter tests
validate only the closed contract; native fixtures and real campaign acceptance
are separate evidence. Broader player-equivalent coverage remains open.


## Bounded live acceptance, 2026-10-29

Both UFO and TFTD executed ordinary native Patrol and Return through MCP on dedicated test campaigns, with owned destination changes and actual docking. UFO reloaded a previously fired Stingray 5 to 6 (stock25 to24); TFTD legitimately remounted a gas cannon and reloaded0 to200 (returned4clips then consumed4). Positive owned refuel ETA matched printed CraftInfo: UFO4hours and TFTD1hour, followed by full-fuel READY/null ETA. No injected resources or damage. An initial read-only UFO diagnostic confirmed missing normal buttons before the fix. A later read-only TFTD diagnostic identified a separate visible rearming ammo/ETA block omitted by the old CraftInfo text admission; this remains open. No visual gameplay input. Current full-game E2E, positive repair and actual shortages are not claimed by this slice.
