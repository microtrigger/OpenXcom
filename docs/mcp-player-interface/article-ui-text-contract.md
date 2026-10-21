# Active article UI text contract

The MCP article body is UI text from the currently open article and its actual
wrapped/scrolled viewport. It is not an OCR or pixel-readability certificate.
Overlapping glyph rows or poor contrast do not turn that text into `null`.
The separately approved game/readable-ui visual changes are not prerequisites
for MCP access.

`Text::agentEnableViewportLines` captures typed lines during ordinary layout
and draw traversal, without copying glyph pixels, building pixel ownership
maps, or comparing article glyphs against the framebuffer. The renderer and
its scroll rules are unchanged. Only lines intersecting the current viewport
are considered; vertically/horizontally truncated lines remain unavailable.
Missing glyphs follow the renderer's question-mark substitution.

This is a body-text-only change: titles retain their existing text contract;
RTL/debug-UI modes remain unsupported by this capture path.

ArticleState still requires the active top state, no modal, a fresh full
presentation, matching widget revisions, and clear exposed regions. External
widgets/overlays may still make a line unavailable. Known owned transparent
ammo-artwork layers retain their bounded transparency checks; those checks are
not article-glyph readability tests. Existing navigation/action
bindings and freshness checks remain. No closed article, undisplayed article
portion, rules database, or hidden world state is exposed.

The original `agentEnableVisibleLines`/`agentVisibleLines` per-pixel proof is
retained as an explicit diagnostic/test path. It still detects overlapping
stock glyphs, and tests compare that diagnostic with the new UI-text contract.
ArticleState does not enable that diagnostic at runtime. Generic button,
list and action presentation guards are not relaxed by this change.

Validation scope: focused native stock-font fixtures and full build are run
separately; these do not alone establish a live game/MCP acceptance result.

## Qualification (2026-10-21)

- Separate full Linux agent-profile build passed.
- 2,705 focused stock-font/layout and retained diagnostic checks passed in
  normal and ASan/UBSan builds.
- 3,864 actual article-constructor/provider/report-stack checks passed in
  normal and ASan/UBSan builds, with explicit owning/resource/availability
  seams. Includes legacy overlapping Laser/Gauss article bodies, viewport
  scrolling, modal/clip/freshness rejection and text-budget rollback.
- Current renderer OFF/ON raster parity passed; 49 research adapter tests passed.
- No live game/MCP acceptance is claimed for this contract change.
