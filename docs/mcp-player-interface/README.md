# Experimental semantic MCP control

**v18s** — Experimental implementation; partial coverage, not a complete interface.

## Build and use

Requires CMake, a C++ compiler, SDL 1.2, SDL_image, SDL_mixer, SDL_gfx and yaml-cpp development libraries. Supply original UFO/TFTD game data separately.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_AGENT_CONTROL=ON \
  -DOPENXCOM_BUILD_LOADING_PRESENTATION_HELPERS=ON
cmake --build build -j2
python3 tools/mcp-observer/launch_presentation.py --agent-control -- /path/to/openxcom [game arguments]
```

Use matching game and adapter versions. The feature is default-off; do not enable test fixtures for normal play.

- `read_ui_state`: displayed state and offered actions.
- `execute_ui_action`: perform an offered action with fresh authority and a unique command ID.
- `read_ui_action_result`: inspect its receipt.
- `read_public_events`: read public presentation/audio events.

Read the resulting state after acting; dispatch does not prove success. Do not automatically retry an unknown outcome.

## Coverage and limits

Coverage varies by snapshot and visible screen; use the offered-action catalog. Work spans menus, Geoscape/base management, tactical actions and inventory. Full UFO/TFTD coverage is unfinished. Normal game rules apply; hidden units, damage and event causality are not exposed. Missing text remains unknown.

```sh
(cd tools/mcp-observer && python3 -B -m unittest discover -v)
(cd tools/mcp-observer && python3 -B -O -m unittest discover -v)
```

Adapter and asset-free smoke tests do not replace full native/live-game acceptance. Historical fixtures may require matching protocol versions or generated includes; a portable run-all native suite is unfinished. WIP branches are not releases. The MCP-versus-screen/input benchmark has not run.

This series reconstructs saved development snapshots; it does not reproduce their original commit history.
