## Schema 27: visible TU reserve controls

`reserve_indicator` is required on Battlescape observations: `none`, `snap`,
`aimed`, `auto`, or `null` when the complete radio group is not demonstrably
painted and uncovered. Only `reserve_none` and `reserve_snap` are added; both
have empty arguments and use the ordinary reserve button handler. A dispatch
receipt is not confirmation: obtain a fresh observation of the indicator.
Schema 26 clients must upgrade with the producer; do not silently accept the
new closed observation shape as schema 26. These controls do not expose reserved
TU amounts, predicted action eligibility, hidden units or path costs.

# MCP adapter

See [build, use and tests](../../docs/mcp-player-interface/README.md). Runtime: Python 3 standard library; pair with the matching native executable.

### Interception warning choices

The additive schema-26 `dogfight_error` state reports only the active popup's
`craft_name`, `message` (nullable when unavailable), and currently offered buttons.
It does not expose hidden altitude, depth, target identity or engagement eligibility.
Use a fresh `read_ui_state` grant for either explicit, empty-argument action:

- `continue_pursuit`: invoke the ordinary continue-pursuit handler (close popup).
- `return_interceptor_to_base`: invoke ordinary return-to-base and close popup.

No choice is made automatically, including when reading the warning. Do not map a
generic Cancel/OK intention onto this popup: the two choices have different game
consequences. A dispatched receipt proves neither pursuit nor return completion;
inspect later public UI observations. Old adapters safely refuse this new state;
use matching game/adapter versions. Existing freshness and single-use rules apply.

### Graphs (agent-control/50)

`open_graphs` uses the displayed Geoscape Graphs button. The six graph modes expose
only current painted labels, visible legend toggles with their actual button
palette indices (not a unique line identity), and anonymous horizontal
palette-color runs clipped to the plot. These are quantized rendered pixels,
not raw monthly activity/funding histories or hidden events. Month/year cells carry their logical displayed label, a clipped flag and visible
ink bounds. Partial clipping does not erase unrelated cells; fully occluded cells
have null label/bounds. These are current UI labels, not underlying history. The plot bounds are logical pixels.

Mode buttons, visible series toggles, current legend scrolling and return use
native controls. Row handles are fresh local grants, not country/region IDs.
A dispatch receipt proves only dispatch; read again to establish the changed
mode or toggle. Hidden/covered series and underlying raw histories are not
reconstructed. Charts exceeding the bounded wire budget are unavailable rather
than silently truncated. Native/live coverage is recorded separately.
