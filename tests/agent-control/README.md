# Native smoke tests

From the repository root: `sh tests/agent-control/run-portable-smoke.sh`.

Requires a C++11 compiler and Linux/POSIX. Covers geometry, socket contracts, render witnesses and event-ring logic without game assets. It does not test the GUI or full game. Other historical fixtures may need matching protocol versions or generated includes.

Typed-line WIP regression (not a full game or article-route acceptance):

```sh
python3 tests/agent-control/run-article-text-tests.py --data /path/to/runtime/data --output /tmp/article-test
python3 tests/agent-control/run-article-text-tests.py --sanitize --data /path/to/runtime/data --output /tmp/article-asan
python3 tests/agent-control/run-article-text-pixels.py --data /path/to/runtime/data --output /tmp/article-pixels
```

Requires SDL 1.2 development libraries and existing UFO/TFTD stock assets; optional
`--sdk /prefix` supplies headers/libraries. Output directories must be new and outside
the checkout. Tests cover opaque final-screen canvases, strict source/format guards,
clipping, stale mutations and exact current-source OFF/ON raster parity. This does
not resolve the separate v22 replay/live acceptance gaps or promote WIP to main.

Replay independently generated v22 native packet schedules through the real Python
client and MCP server (no game execution):

```sh
python3 tests/agent-control/replay_client_server_v22.py --source . --packets /path/to/native-packets --output /tmp/replay
python3 tests/agent-control/check-research-replay.py --source . --packets /path/to/native-packets
```

Repeat with `python3 -O` for optimized-mode checks. Packet input must contain native
`*-schedule.json` and their response files; focused checks use the stock
`xcom1-article-type-5` schedule. Unadvertised-action negatives must be rejected by
MCP before any native exchange, without changing client state or its command ledger.
Offered actions (including native stale-grant rejection) and exact duplicate receipt
queries still traverse the socket and match their recorded responses. Generated
packets/results are external test evidence, not game assets or live acceptance.
