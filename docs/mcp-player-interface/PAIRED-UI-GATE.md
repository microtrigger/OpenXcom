<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original project documentation, 2026-10-10. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Paired production UI-boundary and transport gate

This test-only slice advances stage 5 without completing it. It runs the real
`NumberText`, `Surface`, final-RGB extraction/decoder and native presentation
producer together with the actual Python presentation client and MCP adapter.
It does not run a battle, a `Screen`, `Game`, `SDL_Flip`, physical display or audio.
There are no original game assets, save files or gameplay inputs in the fixture.
No production code or observation schema changes are introduced.

## Reproduce

On Linux with a GNU/ELF-compatible C++ linker, Python 3 and SDL 1.2 development
headers/libraries (including the SDL_image and SDL_gfx headers used by Surface):

```sh
tests/paired-observer/run.sh
```

A private dependency prefix can be selected with `SDL_PREFIX=/path/to/usr`.
Alternatively `SDL_CONFIG=/path/to/sdl-config` supplies SDL flags. Dependencies
must already be installed. The runner neither downloads nor installs software.
It uses a temporary directory, removes it on exit, and does not modify source.
`-ffunction-sections -fdata-sections -Wl,--gc-sections` removes unused asset-loading
routines and their unneeded link dependencies. Production Surface methods are
compiled intact; no constructor, renderer or observer method is stubbed. Assertions
remain enabled even if ambient CXXFLAGS includes `-DNDEBUG` or
PYTHONOPTIMIZE requests optimized Python execution.

[Recorded results and source hashes](paired-ui-evidence.json) identify the tested
source boundary. Other linker and operating-system profiles remain unverified.
The runner is not wired into hosted CI; these results are not a hosted CI run.

## Oracle and coverage

The native fixture creates four genuine NumberText widgets. Ordinary setters,
`NumberText::blit` and production Surface drawing compose an 80×40 offscreen
8-bit image. Only actual widget blits produce candidates. Extraction resolves
all 3,200 image pixels through their palette; the fixed cursor crop covers this
entire small fixture. Test-control output and public observation packets use
separate pipes/socket descriptors. No test-control interface is installed in
the game or exposed through MCP.

Before comparing observations, every selected pair asserts equality of complete
final RGB pixels and geometry. These pairs vary only the explicitly described
widget values or rendering details. They do not prove that every possible change
to UI provenance is observationally equivalent whenever pixels happen to match.
Pairs then compare exact native packet order, count,
length and bytes, except specifically identified independent random transport
tokens. Token normalization is bijective within each trace, preserves 32-byte
width and equality/reuse relationships, and touches only epoch/challenge/admission
fields. Pixels, HUD, availability, schemas, errors and any other fields remain
untouched. A comparator self-test detects changed pixels, values, revisions and
reused tokens. New producer epochs are checked as distinct before normalization.

The 15 test methods cover:

- Every supported value 0–999 rendered by production NumberText, plus rejection
  of 1000, 9999, 99990 and UINT32_MAX. No duplicated test glyph alphabet is used.
- 9999 and 99990 clipped into the same 15-pixel widget: equal RGB, both null.
- Different real source values 58/59 covered by the same later composed numeral
  58: equal RGB and the same displayed 58. The later composition uses another
  real NumberText, not handcrafted digit pixels.
- Different source values under real visibility, temporary hiding, offscreen
  placement, source cropping, target SDL clipping, full opaque coverage and
  bordered-number paths: equal RGB and generic null HUD.
- Different palette indices with identical RGB: equal observation.
- Valid→hidden→valid→missing-candidate sequences: no previous HUD substitution.
- Real native producer packets and real Python `PresentationClient`/`Server`
  outputs for paired HUD and cursor tools. Historical fallback is guarded by a
  deliberately failing historical-reader replacement in those adapter tests.
- Old admission replay rejection; new producer epoch separation and old epoch
  rejection; idle and queued reset behavior; consumed-reset/interrupted-frame
  closure; requests during composition or after extraction await a later begin.
- Native captured-response replay through a test-only socketpair fault relay:
  the real Python client rejects the stale challenge and does not revive.
- Real client deadline expiry and reset closure: one generic unavailable result,
  no late-response recovery and no fallback to old HUD/history.

Pipe acknowledgements and readable-request barriers establish ordering without
sleep-only races. The timeout test intentionally withholds a native response
until the real client has timed out. A replay test respects the production client
rate limit before issuing its second request. These tests do not evaluate timing
correlation or performance and make no latency noninterference claim.

The producer epoch identifies a Screen-lifetime presentation stream, not a
campaign or saved world. Idle reset and a request queued but not yet consumed
preserve the epoch. Reset after request admission closes the channel. A queued
request surviving reset still needs a subsequent composition/extraction.

## Deliberate leak controls

The runner compiles two temporary copies of production source and requires the
same assertions to fail for the expected reason:

1. `PresentedHud.cpp` is deliberately changed to return the real source widget
   value instead of its final-pixel-decoded first field. The equal-RGB 58/59 pair
   detects different public bytes.
2. `PresentationObserver.cpp` is deliberately changed to omit admission-token
   comparison. The old-request replay test detects an incorrectly open channel.

The source-value hook exists solely in the native test fixture. Unmutated
production code cannot reference it. Both negative controls must fail with the
specific intended assertion; a crash, compile error or unrelated test error
is not a successful mutation kill. Temporary mutations are never copied into
production sources, an installed binary or the checkout.

## Limits and next gate

This establishes selected production UI-internal noninterference properties,
not hidden-world battle equivalence. The fixture does not test normal top-state
Battlescape arming, Game composition, Screen focus/palette/scaling gates or the
actual post-SDL_Flip callback. Existing source-order checks continue to cover
those connections structurally, not dynamically. Reset is exercised at the
native producer API, not by an actual load, menu transition or display reset.
Top-state overlays and FPS/cursor exclusion remain outside this runtime fixture;
its later-surface coverage is a composition-boundary test.

No enemy positions/counts, real map occupancy, smoke/fire density, RNG behavior,
simulation noninterference, audio mixing, full-frame battle histories or physical
side channels are certified. No controller is enabled. The
[proposed subsequent full-battle fixture gate](FULL-BATTLE-GATE-PROPOSAL.md)
explains what would be needed next; it is not implemented by these tests.
