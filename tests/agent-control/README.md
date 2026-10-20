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
