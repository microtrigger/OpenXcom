# OpenXcom with experimental semantic MCP control

OpenXcom runs X-COM: UFO Defense and Terror from the Deep using separately supplied original game data.

The MCP interface exposes fresh visible UI, offered semantic actions and public events.
See [capabilities, build and agent connection guide](docs/mcp-player-interface/README.md),
including a Codex stdio configuration, safe action loop and known limits.
The experimental control profile currently requires a POSIX/Linux environment;
it is not a native Windows MCP service.
`mcp-player-interface` contains the v21d snapshot; `wip/named-button-v21e` and `wip/research-v22` retain unfinished work.

License: [GPL](LICENSE.txt), with [component notices](LICENSE-NOTICES.md).
