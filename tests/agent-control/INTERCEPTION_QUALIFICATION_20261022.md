## Interception paint-audit correction — 22 October 2026

Source commit: `d89f9a88149ef75af23ee0663208faa2d5dc0989`.
Release binary SHA-256:
`6d6de9a5a3ed2880a9842cfe9459c511a6feafaaf36f45077ca746cf455f9c6c`.

### Correction and reproducible checks

A live interception identified a connection closure after SDL_gfx
`texturedPolygon` returned a negative result. Native reproduction established
that a completely clipped polygon can produce that result without changing any
pixels. The experimental-only correction recognizes a valid polygon whose
bounding box cannot intersect the destination clip as a no-op. Intersecting
operations and genuinely invalid polygons retain the existing error audit.
This corrects raster bookkeeping, not a text-readability policy or combat rule.

- Full experimental Release build: **passed**.
- Real Surface/SDL_gfx regression: **50 normal + 50 sanitizer checks passed**;
  unchanged-source negative control reproduces the original audit failure.
  Sanitizer coverage is ASan/UBSan, excluding vptr and leak checking.
- Existing native dogfight provider/radio regressions: **344 passed**.
- Actual-controller regressions using synthetic projections: **371 passed**.
- Python regression suite: **1,234 tests run, 2 diagnostic tests skipped**.
- Without the experimental flag, baseline and corrected Surface preprocessing
  are identical after normalizing only source-root `__FILE__` paths. This is
  preprocessing parity, not a separate full non-agent build qualification.

### Live evidence and remaining limits

The corrected TFTD run reached a real dogfight and successfully read its public
MCP state without the former transport closure. It then stopped because the
private runner incorrectly required an interception number that the expanded
combat window deliberately hides. The runner was corrected without exposing
that hidden field or changing game code; visible ammunition, offered controls
and fresh native authority remain required.

A subsequent TFTD run using that corrected runner stopped safely at an
unsupported popup. Direct operator inspection of the displayed game window
identified **“UNABLE TO ENGAGE ENEMY AT THIS DEPTH”**; no saved screenshot
supports that identification, and the MCP unsupported tag alone does not
establish its cause. That run did not establish combat or an ammunition decrease
and is **not a successful interception route**.

The UFO run completed the bounded public-control route at **05:06:08 UTC**:
**16 MCP actions**, a natural airborne contact, the selected interceptor,
cautious mode, visible missile ammunition **6 → 5** (cannon ammunition remained
200), deliberate disengagement and verified return. The craft showed
**REPAIRS**, **1% damage**, **89% fuel**, and **5/6 Stingray ammunition**, followed
by the pause menu. This qualifies that one UFO route and a visible shot, not
an enemy hit, kill, every combat mode or every encounter condition.

The complementary TFTD run completed at **05:09:06 UTC**, also with **16 MCP
actions**: natural contact, selected Barracuda, cautious mode, visible missile
ammunition **6 → 5**, deliberate disengagement, verified return and pause. The
return screen showed **REFUELING**, **0% damage**, **91% fuel**, **AJAX 6/6** and
**craft gas cannon 200/200**. These are the values visible at return inspection,
not an assertion that ammunition stayed constant throughout the route.

Both successful runs retained unchanged source/runner inputs and used the
release binary identified above. They qualify one bounded UFO route and one
bounded TFTD route, not every enemy type, depth, target-loss condition or combat
mode. The previous safe stops remain separate evidence; they are not rewritten
as successes.

Audio/video recording is unavailable for these runs. Actual testing remains
within the approved scope, but no recording-based verification is claimed.
