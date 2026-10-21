# Readable UI changes

This branch is based on `7dac39f3dad63433bae0b229be0ea2c6a597cde3`, which
already contains MCP integration. It isolates the new game-UI changes below;
it is not a clean upstream fork or an MCP-free version of the game.

## Article body spacing

Add opt-in extra line spacing to Text; article bodies in UFO and TFTD use one
additional logical pixel between lines. Other Text users keep the existing
spacing. Glyphs, font metrics, article wording and gameplay rules are unchanged.
Scroll limits and line selection use the same adjusted spacing.

Includes focused article-text and pixel-parity regression assertions. This is
the same source patch previously qualified, separated from MCP gameplay-plan
and runner changes; this branch does not claim fresh live acceptance.

## Calendar text layout

Separate the displayed calendar rows and restore the underlying panel before
redrawing them. Covers the regular and funds-display layouts in UFO/TFTD;
does not change game time, calendar values or simulation rules.

Includes the focused calendar-layout regression test and runner. Source and
tests are copied exactly from the previously qualified local revision; this
split excludes allocation runners and MCP planning documentation.
