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
