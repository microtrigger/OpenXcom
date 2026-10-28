# Reserve Aimed / Auto contract

Schema 29 extends the existing reserve controls with `reserve_aimed` and
`reserve_auto`, both without arguments. The existing `reserve_indicator` shape
is unchanged. All four modes use the same painted radio-group proof and native
player-turn/busy guards. Each command routes its actual button sender through
the ordinary reserve-click handler; no direct model mutation or hidden state
projection is added. Reserve-kneel remains outside this slice.

Bounded native and adapter checks cover all four selections, freshness,
unavailable presentation, invalid arguments and ordinary availability guards.
Short isolated live checks in both UFO and TFTD confirmed Aimed and Auto through
fresh displayed state, ordinary movement with Auto retained, and return to None.
Gameplay controls used MCP; passive video was used without visual gameplay
fallback. These checks do not constitute full-mission or campaign acceptance.

The wider test suite is not claimed fully passing. Crash-recovery modal natural
acceptance remains pending; this slice does not change that status or complete
the full player-equivalent UFO/TFTD interface.
