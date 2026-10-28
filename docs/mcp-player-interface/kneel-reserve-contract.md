# Standalone kneeling TU reserve

Schema 30 adds a required nullable `kneel_reserve_indicator` (`on`, `off`, or
null) and the no-argument `toggle_kneel_reserve` action on Battlescape. This is
reserve for a later kneel, not the soldier's current pose. The existing
`kneel_indicator` and four-mode `reserve_indicator` remain independent.

A separate standalone-toggle paint witness requires an ungrouped invert-toggle
button, current rendered source/destination, revisions and unclipped visible
presentation. The existing radio-group witness still requires its group and
is not weakened. Actions retain player-turn, busy, freshness and overlay guards
and use the real reserve-kneel button sender through the ordinary handler.

Native and adapter checks cover the separate state and authority, invalid
arguments, stale/unavailable presentation, and independence from pose/radio
selection. The wider suite is not claimed fully passing. This bounded contract
does not complete the full player-equivalent UFO/TFTD interface.

Short isolated live qualification in both UFO and TFTD confirmed off → on → off
through fresh MCP observations while displayed pose, TU and radio-reserve mode
remained unchanged. Gameplay used MCP and passive video, without visual gameplay
fallback. This qualifies the bounded toggle, not every interaction or campaign.
