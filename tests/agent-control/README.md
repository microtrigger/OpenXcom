# Native smoke tests

From the repository root: `sh tests/agent-control/run-portable-smoke.sh`.

Requires a C++11 compiler and Linux/POSIX. Covers geometry, socket contracts, render witnesses and event-ring logic without game assets. It does not test the GUI or full game. Other historical fixtures may need matching protocol versions or generated includes.
