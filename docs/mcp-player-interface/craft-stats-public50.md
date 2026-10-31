# CRAFT article text: public50 candidate

Base: `5ab560ef15211d599b9f6a49162dc64696bf6236`.
Exact wire identity: `openxcom-agent-control/50-craft-stats1`.
This is a narrow branch-specific extension of public50, not numeric protocol51,
not local63, and not a superset of either. Native and adapter must match exactly;
plain50, other numeric versions and other suffixes are rejected.

## Contract

`ufopaedia_article.visible` has exactly `title`, `body_lines`, `buttons`,
`details_status`, and `craft_stats`. The new field is null when unavailable,
otherwise an array of exact LF-separated logical lines from the painted native
CRAFT Text widget. Empty lines, color markers and nonbreaking spaces are
preserved. No RuleCraft/model data is queried by observation. The adapter permits
1..32 strings, at most 1024 UTF-8 bytes per line and 8192 aggregate bytes, with
no embedded LF. Native admission additionally retains the existing whole-Text
qualification, so this is not a promise that every adapter-valid array can be
produced by every layout.

`details_status` is `partial` only when `craft_stats` is non-null, otherwise
`unavailable`. Non-Craft articles have null `craft_stats`; there is no ITEM/table
backport. Body-text rules, button authority, fog-of-war, freshness, presentation
certification, region/overlay guards and action catalogs remain unchanged.
The public catalog remains 261 actions, 106 states and seven MCP tools.

The provider registers the ordinary UFO and TFTD CRAFT Text widgets. Readability
requires current painted content on the logical destination, correct top state,
unobscured/disjoint regions and the existing native Text qualification. Dirty,
hidden, clipped, offscreen and obscured text is not reported as readable.

## Qualification and limits

- Offscreen native harness: 108 assertions PASS across Interceptor, Skyranger,
  Barracuda and Triton layouts, compiled from the actual public source. Real
  Font/Text/Surface rasterization is used; constructor/lifecycle seams remain.
- Actual rendered provider output passes the public adapter's state and
  presentation validation for all four layouts. Envelopes/actions in this
  boundary check are fixtures, not native socket evidence.
- Focused adapter suite: 43 PASS in normal and optimized Python.
- Full adapter regression, normal and optimized Python: 1717 methods each,
  1710 PASS and seven inherited skips, zero failures/errors. Immutable public50
  baseline: 1708 methods each, 1701 PASS and the same seven skips.
- Full build using this repository's actual CMake project: configure/build PASS,
  330 translation units; all 737 native/CMake input files unchanged during build.
  Release `-O2 -DNDEBUG`, experimental agent control and loading presentation
  helpers enabled, OpenGL disabled. Dynamic dependencies resolve. This is a
  compile/link result, not execution of the resulting game.
- Live gameplay, native socket/controller dispatch, final video/audio and seven
  inherited environment-dependent integration checks: **NOT_COVERED** for this
  candidate. Earlier results from other protocol versions are not acceptance
  evidence for this branch. No deployment or full player-equivalent claim.

Run adapter tests from their documented working directory:

```sh
cd tools/mcp-observer
python3 -B -m unittest discover
python3 -B -O -m unittest discover
```

The source-only native harness and its scope/requirements are in
`tests/agent-control/craft-stats/README.md`. Supply legally owned resources and
SDK dependencies separately; no game data, saves, binaries, private diagnostic
logs or machine-specific profiles are distributed here. The 35 existing fixture
files only migrate exact protocol literals and article DTO shape; existing
negative checks are retained. A new test module covers the closed schema,
malformed values, bounds, wire mismatch and unchanged authority/body rules.

Video availability or synchronization does not gate gameplay testing. Record
media quality separately; missing or failed media is not video evidence. All
non-media safety, freshness and unknown-command outcome constraints still apply.
