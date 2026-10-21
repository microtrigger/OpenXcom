# Readability and allocation checkpoint (2026-10-21)

Historical checkpoint: the subsequent [article UI-text contract](article-ui-text-contract.md)
separates runtime article text delivery from these retained pixel diagnostics.
The visual game fixes remain independent of the MCP text contract.

The UI change is deliberately narrow: Ufopaedia body text opts into one extra
pixel between rows, with matching layout height and wheel step. Other Text
widgets keep their default spacing. Calendar rows have full glyph-height
viewports; clock/funds geometry and the original themed sidebar background are
adjusted together. No observer visibility/ownership guard is relaxed.

Fresh checks on the changed source:
- Full game build (agent profile, Linux/SDL 1.2 compatibility runtime).
- Article text: 2,659 normal and ASan/UBSan checks; the legacy overlap negative
  control still refuses incomplete lines, while the separated stock body passes.
- Current renderer OFF/ON pixel parity, including both legacy and added spacing.
- Calendar: 111,347 normal and ASan/UBSan checks using real UFO/TFTD backgrounds,
  both funds modes, two sizes, month labels and mixed-case/Cyrillic samples.
- Actual article constructors/report stack: 3,701 normal and ASan/UBSan checks with the existing
  explicit ownership/resource seams (not a complete game loop).
- Real UFO and TFTD article navigation produced paired before/after screenshots
  at 640x400. Those visual checks used ordinary UI input, not MCP acceptance.
- Fresh-campaign **MCP scientist allocation** passed in each game: 32 dispatches,
  zero through all ten available scientists, an extra increase at the limit,
  return to zero and an extra decrease. Receipts and fresh visible staff/lab
  counts were checked; source/binary hashes remained unchanged.

Limits: the scientist route does not start research or production; it does not
qualify keyboard/repeat inputs, engineer quantities, workshop-bound capacity,
large rosters, production completion, transfers or interception. Arithmetic
matrix tests are a separate, explicitly seamed layer. Modded fonts, every
translation, all title lengths and every possible calendar value are not claimed.
Screenshots/video and lawful original assets remain private, not in this repo.

See [gameplay plan](gameplay-plan.md) and
[Geoscape event inventory](geoscape-event-coverage.md) for remaining slices.
