# Native smoke tests

From the repository root: `sh tests/agent-control/run-portable-smoke.sh`.

Requires a C++11 compiler and Linux/POSIX. Covers geometry, socket contracts, render witnesses and event-ring logic without game assets. It does not test the GUI or full game. Other historical fixtures may need matching protocol versions or generated includes.

## Named Basescape buttons (native fixture)

With Python 3, a C++11 compiler, SDL 1.2 development libraries (image, gfx,
 mixer), yaml-cpp and legally supplied game data:

```sh
python3 tests/agent-control/run-named-button-tests.py \
  --data /path/to/runtime/data --output /tmp/named-button-check
```

The data directory must contain `common/Language/Font.dat`, `UFO` and `TFTD`.
Use `--sdk /path/to/sdk/usr` for a non-system dependency prefix, `--source`
for another checkout, or `--sanitize` for ASan/UBSan (leak detection disabled).
The output directory must be new and outside the checkout. It contains logs,
source/asset hashes, generated fixture includes, a binary and `RESULT.json`.
No assets are downloaded or modified. Normal and sanitizer runs need separate
output directories.

This compiles the current renderer and extracts the current provider/dispatcher
and Controller into explicit dependency/time seams. It tests label visibility,
cursor overlap, stale mutations, clipping and exactly-once dispatch offscreen.
Constructor layout is reproduced; unrelated destination callbacks are inert.
It does **not** run the full game, establish live route acceptance, or prove the
cause of a previously observed GUI failure. Run live UFO/TFTD routes separately.

For exact current-source agent-OFF/ON offscreen button raster parity, run
`run-named-button-pixels.py` with the same `--data`, optional `--sdk`/`--source`,
and a separate new `--output`. This covers both stock games and button style,
label and layout variants; it is not a historical-baseline or full-GUI test.
