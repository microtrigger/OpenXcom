# Readable UI changes

## Article body spacing

Add opt-in extra line spacing to Text; article bodies in UFO and TFTD use one
additional logical pixel between lines. Other Text users keep the existing
spacing. Glyphs, font metrics, article wording and gameplay rules are unchanged.
Scroll limits and line selection use the same adjusted spacing.

Includes focused article-text and pixel-parity regression assertions. This is
the same source patch previously qualified, separated from MCP gameplay-plan
and runner changes; this branch does not claim fresh live acceptance.
