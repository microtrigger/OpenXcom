# Graphs: schema 50 qualification

The six ordinary graph modes are exposed through the native Graphs screen:
alien activity by region/country, X-COM activity by region/country, funding and
finance. MCP offers native mode buttons, current visible legend toggles,
available scrolling and return. Observation-local handles and existing paint,
overlap, freshness and single-use guards remain in force.

The representation includes displayed axes and title, legend labels and button
palette colors, and anonymous clipped horizontal palette-color runs from the
painted plot. It does not export raw history vectors, hidden events or an
unavailable series identity. Palette colors need not uniquely identify a series;
that association limitation remains explicit. Partially clipped axis labels are
reported with logical displayed text, visible ink bounds and clipped metadata;
fully invisible cells have null label/bounds.

Final live qualification exercised all six modes and visible toggles in both
UFO and TFTD on isolated test campaigns, returning to Geoscape. Twelve month
labels and displayed year labels were verified. The final passes used MCP
without visual input or screenshots and retained passive real game audio.
The initial UFO pass failed because axis labels were empty; one read-only
visual diagnostic established the issue. That failure is preserved separately
and is not counted as a successful pass.

Final native fixtures passed 158 checks in normal and sanitizer configurations,
including actual mode/toggle/scroll/scale handlers with explicit synthetic
layout and line-data seams. Legend scrolling is fixture-tested only: the real
campaigns' 15 regions and 16 countries did not offer that action. The final full
Python suite passed 1708 tests with seven explicit integration skips, in normal
and optimized mode. Earlier failed font/axis fixtures remain failed historical
attempts; the final semantic-axis fixture and rebuilt game are the accepted
configuration. Local tests do not imply a GitHub CI run.

This closes the bounded ordinary Graphs scenario, not the full two-game
player-equivalent project. Remaining ordinary combinations and deferred rare,
base-defense/destruction and campaign-final matrices remain open.
