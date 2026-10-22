# Active contact-title UI text contract

## Bounded change — 2026-10-22

Published source object: `97af703b1e4256b0eaf0fd18902cf3ce5c679289`.
Qualified equivalent local commit: `638a68a864866cfcf1dd3488a7953faa01e2e66d`.
Source tree: `22052cb7ab257822e922bb6368f54a84440cdebd`.

Only `UfoDetectedState`'s `title` projection changes. It reports the current
opened popup's logical Text caption, not an OCR or glyph-readability certificate.
A cosmetic cursor crossing that caption, or incomplete glyph-ink coverage, no
longer converts the otherwise admitted caption into `null`. No UFO model details,
closed-window text, depth/speed fields, gameplay rules or visual layout are added
or changed.

The active top state, absence of a modal, full current presentation, matching
text/presentation revisions, actual canvas bounds and logical layout fit remain
required. Scrollable titles are conservatively unavailable. Later overlapping
widgets and the FPS overlay still exclude the title. The stock title's 17-pixel
allocation contains 16-pixel text: occlusion uses its logical text-height extent,
not the spare allocation row shared with the Detected label.

The title is observation-only and is not appended to actionable overlay regions.
Its widget revisions and explicit x/y/width/height still bind freshness. A scoped
non-cursor overlay helper checks the title; the original global overlay check,
buttons, other contact labels, action admission and other screens are unchanged.
This is **not** a general removal of runtime text-readability gates elsewhere.

## Verified build and regression evidence

Full Linux Release agent-profile build passed. Binary SHA-256:
`275f4ca85285a742d92c42f270201416e2ccd0ade65dce34d4dc981f802936e5`.

| Check | Result |
| --- | --- |
| Actual contact constructor/provider, native SDL widgets | 275 passed |
| Same fixture, scoped ASan/UBSan | 275 passed |
| Existing interception helper/provider/controller seams | 114 passed |
| Existing native dogfight provider/radio checks | 344 passed |
| Existing actual-controller deterministic checks | 371 passed |
| Python adapter suite | 1,240 run, passed; 2 existing skips |

The contact fixture covers eight combinations: UFO/TFTD, normal/hyperwave popup,
flying/landed contact. It checks cursor-over-title and cursor-away equivalence,
the outer overlay admission, unchanged cursor-over-button rejection, missing
glyph coverage, FPS/later-widget occlusion, hidden/off-canvas title, stale text
and geometry ABA, wrong top state, modal, destination clip and layout overflow.
Both final contact runs verified unchanged input hashes. An earlier run whose
source changed during compilation is not counted as qualification.

These are native constructor/provider checks with explicit Game, Mod/interface,
Ufo/Geoscape/SavedGame and rendering-loop seams and a synthetic grayscale palette;
they do not themselves prove a live game route. Extracted callbacks are compiled,
not dispatched by the new fixture. Sanitizer coverage excludes vptr and leak
checking. Scrollability rejection and visibility ABA are source-reviewed rather
than separately asserted by this fixture. Independent source/native review passed.

## Bounded live title probes — passed

A bounded private probe uses a natural contact, then public MCP cursor motion
far → operator-confirmed title point → far, requiring the same non-null title
at every observation. It leaves the contact popup open; it does not intercept or
dismiss it. Time acceleration is a semantic UI action, not a claim of no mutation.
The overlap coordinate must be confirmed against the actual visible popup;
coordinate bounds alone do not prove that it overlaps text.

The first TFTD point `(130, 58)` was outside the visible glyphs and is **not**
accepted as cursor-over-title regression evidence. Corrected TFTD and UFO probes at `(42, 58)` passed on 2026-10-22 at
06:57:05 UTC and 07:01:53 UTC respectively. Each dispatched four MCP actions
(one semantic time-speed action and three cursor moves), retained identical
non-null titles (`ALIEN SUB-1` / `UFO-1`), and verified unchanged source inputs.
Actual recorded frames independently confirm cursor overlap with title glyphs
at physical `(126, 174)` in the 960-by-600 game window. Setup and post-route
cleanup used ordinary desktop UI; no desktop input occurred during the measured
MCP sequence. No hidden world-state reads or save editing were used.

The adjacent `detected_label` still becomes unavailable at the overlap position
and recovers after moving away: that unchanged legacy gate is outside this
title-only fix. The test does not claim every screen or label has been migrated.
No full interception or depth-warning-button qualification is claimed here.
Recorded-media delivery checks are separate from these source/runtime results.
