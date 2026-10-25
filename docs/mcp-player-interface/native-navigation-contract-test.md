# Native navigation payload integration

The `geoscape_craft_navigation` button order follows the actual native serializer: REDIRECT emits `base, patrol, target, last_known`; ordinary Cancel mode emits `target, cancel`. The validator retains strict ordered-subset validation and mutually exclusive mode controls. Button order is not inferred from screen geometry.

`test_native_navigation_order.py` consumes actual serialized provider output, not a dictionary-derived mock. Run `tests/agent-control/run-geoscape-craft-navigation-tests.py` with its required source, SDK, data and fresh output arguments. Its test log includes `NATIVE_NAVIGATION_PAYLOAD ` lines from the literal native constructors/providers and real widgets for both stock masters. Parse the JSON after that fixed prefix into a JSON array file, without modifying the payloads.

Then, from `tools/mcp-observer`:

```sh
NATIVE_NAVIGATION_PAYLOADS=/path/to/native-payloads.json python3 -B -m unittest -v test_native_navigation_order
NATIVE_NAVIGATION_PAYLOADS=/path/to/native-payloads.json python3 -B -O -m unittest -v test_native_navigation_order
```

The native run and both Python modes with the actual artifact are required to qualify this integration. Generic discovery without the environment variable explicitly skips the two integration tests; a skip is not an integration PASS. A supplied missing/malformed artifact fails rather than skips.

The transport envelope is synthetic; native state/visible/cursor/actions are consumed unchanged. Game/domain/render seams in the native fixture remain explicit. This does not qualify a full running game, current global cursor policy, or a historical rejected packet whose contents were not retained. Do not publish real campaign payloads or private logs as fixtures.
