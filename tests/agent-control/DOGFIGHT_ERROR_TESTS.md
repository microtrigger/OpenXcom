# Interception eligibility popup

`DogfightErrorState` exposes the current popup's craft caption, message and
explicit **Continue Interception Pursuit** / **Return To Base** choices. The
provider does not choose automatically. Continue calls the ordinary pop-only
handler; Return calls the ordinary `Craft::returnToBase()` handler and pops the
popup. It does not expose altitude, depth limits or other world-model fields.

Captions come from active UI widgets. Layout overflow is withheld; font-ink
completeness and pixel readability are not prerequisites. Fresh presentation,
allocation geometry, occlusion, modal/top-state ownership and action authority
still apply. The experimental `TextButton::agentLayoutLabel()` accessor is used
only by this popup; existing providers retain their existing behavior.

## Native constructor and handler regression

```sh
python3 tests/agent-control/run-dogfight-error-tests.py \
  --source /path/to/checkout --sdk /path/to/sdk/usr \
  --data /path/to/runtime-data --output /new/external/evidence-directory
# Repeat with --sanitize and another new output directory.
```

The harness extracts the actual constructor, ordinary callbacks and provider
methods and links native SDL widgets. It loads both stock UFO/TFTD English
captions. Game, Mod/interface configuration and Craft domain objects are explicit
seams; the palette is synthetic. This is not a real interception or stock-image
qualification.

Checks cover both explicit handlers, invalid protocol arguments, prepaint and
popup animation, stale text/geometry/visibility, partial destination clips,
wrong top state, nested modal, occlusion, changed craft authority, hidden and
layout-overflow captions, and overlapping glyph rows with valid layout. The
last case must still return logical UI text rather than require pixel-ink proof.

Each output contains commands, source/test/data hashes, logs and a terminal
`RESULT.json`. Sanitizer mode enables AddressSanitizer/UBSan but excludes vptr
and leak checking because of the explicit domain seams. Existing dogfight
provider/radio and actual-controller regressions should also be rerun. Native
results do not establish live popup acceptance; that requires a natural visible
error and an explicitly selected ordinary outcome in each intended game.
