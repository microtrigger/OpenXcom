<!-- New project documentation: SPDX-License-Identifier: GPL-3.0-or-later. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# OpenXcom: Fair-Play MCP Interface Work Plan

English visitor-facing version · [Русская рабочая версия / Russian working version](PLAN.ru.md)

Status as of October 12, 2026. We maintain the Russian version for day-to-day project work; this English version presents the same scope, stages, and acceptance gates for visitors.

## Plan versus actual progress

Displayed-HUD evidence baseline: [b8d9806](https://github.com/microtrigger/OpenXcom/commit/b8d9806aa83151c4da1805c691f50903512c7f65). The [natural-FOV/pause gate](natural-fov-evidence.json) is based on `a8750ad` plus recorded source hashes. The earlier [normal battlefield-audio gate](BATTLEFIELD-AUDIO-GATE.md) has [aggregate runtime evidence](battlefield-audio-evidence.json), 2026-10-10. The latest boundary correction is [diagnostic isolation](../../tools/mcp-observer/DIAGNOSTIC-ISOLATION.md), with separate [bounded menu and test evidence](diagnostic-isolation-evidence.json). The new [smoke-density render gate](SMOKE-DENSITY-GATE.md) passes its first normal UFO/TFTD runs and rejects both source mutants in each game; see [aggregate evidence](smoke-density-evidence.json), 2026-10-11. The separate [smoke viewport-clipping gate](SMOKE-CLIPPING-GATE.md) passes its first normal UFO/TFTD runs on one frozen binary and rejects the HUD-leak and centered-positive-control mutants in both games; see [aggregate evidence](smoke-clipping-evidence.json). The [bounded native timing calibration](TIMING-CALIBRATION.md) completed all four planned profiles; its deliberate delay control meets the frozen rule in TFTD but is inconclusive in UFO, so timing remains open; see [aggregate evidence](timing-calibration-evidence.json). Earlier [request-bound mixed audio](MIXED-AUDIO-NATIVE.md) and its [adapter measurements](../../tools/mcp-observer/mixed-audio-evidence.json) remain separate evidence. **Overall: in progress; observation prototypes and limited locally confirmed stance/unit-selection/right-click/left-click/inventory-opening pilots with bounded combined SDK/game evidence, not a completed player interface.** Update this summary and the Russian working version after each verified delivery, with its evidence revision, remaining gates, and failed/not-run checks. A delivered subset does not close its parent stage.

**Selected limited route (2026-10-11):** the owner accepted
[`practical-play/1`](PERCEPTION-CONTRACT.md#practical-first-version-acceptance-profile):
ordinary presented UI/mixed-audio clues, no hidden-state API or timing
reconnaissance, with residual timing channels disclosed as an experimental
limitation. The locally human-confirmed kneel/stand implementation and bounded two-game
demonstration are verified below. Actual token benchmarking remains pending. This is a separate
bounded acceptance profile; the original full Stage 5/controller gate stays
open. The scope decision alone was not implementation evidence; the separate results
below establish the individually bounded control milestones, with no savings claim.
Historical evidence retains its original scope.

| Stage / planned outcome | Actual result | Remaining acceptance gate |
| --- | --- | --- |
| 0 · Perception contract | Vanilla perceptual parity and the limited `practical-play/1` route selected; [policy documented](PERCEPTION-CONTRACT.md). | Per-field validation and focused pilot checks. |
| 1 · Reproducible baseline/build | Clean baseline and opt-in/default-OFF builds pass; selected combat/save/load/audio checks in both games ([build evidence](HUD-PROTOTYPE.md)). | Broader repeatability and complete stage acceptance. |
| 2 · Source audit | Pinned source audit and selected presentation boundaries documented. | Complete field/action and side-channel audit. |
| 3 · Native observer | Historical HUD/cursor slices, request-bound pixels, optional four-number displayed HUD and default-off [500 ms final-mix audio](MIXED-AUDIO-NATIVE.md) implemented. | Full approved projection, event coverage, broader game-audio/lifecycle validation and parity. |
| 4 · IPC/MCP | Local read-only stdio tools, request-bound `/1`/`/2` visual protocols and a separate [native MCP audio tool](../../tools/mcp-observer/AUDIO-ADAPTER.md) implemented; SDK checks pass. | Production recovery and broader observation coverage; no audiovisual synchronization. |
| 5 · Non-disclosure gate | Native/synthetic, source-boundary and transport tests pass; [15 production UI/transport paired tests and two deliberate leak controls](PAIRED-UI-GATE.md), plus [one controlled in-engine hidden-occupant render pair](HIDDEN-OCCUPANT-GATE.md) in UFO/TFTD, also pass. A separate [request-effects gate](OBSERVER-EFFECTS-GATE.md) passes normal kneel/stand checkpoints in both games and rejects engine-RNG consumption mutants. The [natural-FOV/pause gate](NATURAL-FOV-GATE.md) passes six real input/timer-driven presentations with independent crop/HUD pixel checks in each game. A real-SDL synthetic audio oracle checks byte-exact final-mix capture and rejects deliberate corruptions. The [normal battlefield-audio gate](BATTLEFIELD-AUDIO-GATE.md) passes visible/outside-player-team-FOV grenade events, exact independent same-buffer PCM and idle/requested-observer state/engine-RNG comparisons in each game, rejecting PCM-corruption, extra-RNG-draw and missing-cue controls in both. The separate [smoke-density gate](SMOKE-DENSITY-GATE.md) passes eight whole-frame 7/8 matches, 24 native responses and eight density-6 positive phases per game, with both source mutants rejected in each game. The separate [smoke viewport-clipping gate](SMOKE-CLIPPING-GATE.md) passes eight equal clipped complete frames and equal complete native transcripts, 32 native responses and eight centered 6/7 positive phases per game; the HUD-leak and centered-positive-control mutants are rejected in both games. The [bounded timing calibration](TIMING-CALIBRATION.md) completed four profiles and 304 half samples with pairwise RGB/transcript equality; the 5 ms sensitivity control is inconclusive in UFO and meets the frozen rule in TFTD. | Full paired hidden-engine-state, audio and timing tests, including an established timing sensitivity method; controller gate remains open. |
| 6 · Normal-action controller | Default-off locally confirmed stance pilot implemented; [bounded native and actual MCP/game checks](SUPERVISED-STANCE-EVIDENCE.md) pass in both games. The separately opted-in [next/previous selection extension](SUPERVISED-SELECTION-EVIDENCE.md) adds bounded native parity and interruption/replay checks. The [ordinary right-click extension](SUPERVISED-TURN-EVIDENCE.md) adds full-view binding and bounded turn/door checks. The [ordinary left-click pilot](SUPERVISED-MAP-LEFT-CLICK-EVIDENCE.md) adds bounded movement/selection and consequence checks. The [inventory-opening slice](SUPERVISED-INVENTORY-OPEN-EVIDENCE.md) adds capless inventory pixels and real SDK/game checks. The [combined production smoke](COMBINED-SDK-LIVE-EVIDENCE.md) verifies selection, turn, movement and inventory/return in one run per game, retaining three TFTD nonmovement attempts. | Full controller: Stage 5 and agreed commands. Limited pilots: focused `practical-play/1` checks and local in-game confirmation. |
| 7 · Sequential batches | Not implemented. | Controller, interruption and duplicate-execution guarantees. |
| 8 · UFO/TFTD matrix | Selected visual and [diagnostic-profile menu checks](diagnostic-isolation-evidence.json), [mixed-audio menu/zero-volume/exit checks](mixed-audio-game-evidence.json), the [bounded land-only battlefield-audio slice](battlefield-audio-evidence.json), the [controlled land-only smoke-density render pair](SMOKE-DENSITY-GATE.md) and the [full right-edge smoke-clipping pair](SMOKE-CLIPPING-GATE.md) pass in both games; earlier failures and unmatched comparisons are retained in [bounded visual evidence](presented-hud-build-evidence.json). All four [timing-calibration profiles](timing-calibration-evidence.json) completed, but UFO sensitivity remains unestablished. | Full regression matrix, including unsupported/not-run cases. |
| 9 · Resource comparison | Protocol, offline harness and usage-ledger format prepared ([benchmark documentation](../../tools/benchmarks/README.md)); bounded audio byte counts and component microbenchmarks recorded. | Actual end-to-end model-token/latency/resource measurement; no token-savings claim. |
| 10 · Guide and joint pilot | Prototype usage documentation exists; gameplay guide and campaign pilot not completed. | Earlier acceptance gates and agreement on a bounded pilot. |
| 11 · Full HD / YouTube | Deferred. | Separate recording and publication approval. |

### Combined official-SDK / production-game smoke (2026-10-12)

The [combined smoke](COMBINED-SDK-LIVE-EVIDENCE.md) now verifies the complete
selection → turn → visible movement → inventory opening/view → manual close →
fresh tactical view chain in UFO attempt 01 and TFTD attempt 04. Each uses 14
recorded tool calls and an explicit observation stop, with four locally confirmed
proposals and fresh post-action images on the unchanged nongate production binary.
UFO shows TU 56 → 52 → 38 and energy 43 → 36; TFTD shows TU 64 → 60 → 52 and
energy 65 → 61. TFTD 01 selected another soldier; 02/03 showed no movement and
stopped before inventory. All five attempts and 26 image hashes remain in the
[aggregate](combined-sdk-live-evidence.json).

Independent archive review verifies receipts and visible outcomes. Local Y/input
gestures, ordinary TFTD 04 crew setup and post-stop game/window observations are
operator-attested, without independent stored UI-event or post-stop-image proof.
This closes the narrowly scoped combined live SDK turn/movement demonstration
previously missing from the individual slices. It adds no code, timing/fairness
acceptance, campaign acceptance or model/token measurement. Item manipulation and
inventory closing remain manual.

### Inventory opening and read-only view (2026-10-12)

The opt-in [slice](SUPERVISED-INVENTORY-OPEN-PILOT.md) proposes ordinary inventory
opening, then a fresh final-pixel view without any action capability. It exposes
no hidden item list, ammunition value, ground page or inferred label. All 14 native
profiles pass in each game: 12 opening-equivalence pairs and 24 additional core
worlds, plus lifecycle, shared-ledger and interoperation profiles. All 15 real
source faults are detected in both games. Official-SDK → nongate production-game
runs verify the complete proposal/local-confirmation/inventory-image/manual-close/
fresh-tactical-image path in UFO and TFTD. See [evidence](SUPERVISED-INVENTORY-OPEN-EVIDENCE.md).

Closing and item operations remain manual. Automated closing is deferred because
canceling its confirmation would otherwise reinitialize and rearrange the ground
view. Inventory operations and full controller acceptance remain open.

### Ordinary map left-click (2026-10-12)

The bounded [left-click pilot](SUPERVISED-MAP-LEFT-CLICK-PILOT.md) uses fresh
presented tactical pixels and local confirmation. One ordinary map click may
walk, select a visible friendly or do nothing. The first profile requires the
normal default path-preview setting, never changes it and never synthesizes a
second click. No hidden route, occupancy or predicted-cost query is added.

Both games pass 19 manual/control pairs, 36 adversarial worlds and 38 paired
outcome views, covering normal TU/energy/reserves, first contact, a regular door,
a real reaction-fire queue and manual cancellation. Natural movement continues
after an observation timeout or disconnection. The post-L wait has a fixed
8-second dispatch deadline; unrelated input revokes it and a fresh V remains
mandatory. 352 Python tests pass normally and with optimization, with two
expected native-only skips each; six synthetic-peer SDK smokes pass.

[Evidence and earlier failure](SUPERVISED-MAP-LEFT-CLICK-EVIDENCE.md) record the
fixture-only out-of-bounds timeout target correction. All 18 actual-source faults
are detected in both games; prior stance, selection and right-click gates also pass. Completed reaction damage,
falling, broader terrain coverage, preview-path movement, full Stage 5 and real
token measurements remain open. The later [combined production smoke](COMBINED-SDK-LIVE-EVIDENCE.md)
adds bounded live SDK left-click movement evidence in both games.

### Verified ordinary map right-click extension (2026-10-12)

The separately opted-in [supervised right-click pilot](SUPERVISED-TURN-PILOT.md)
uses a newly presented full tactical image and an integer screen location.
Local confirmation dispatches the normal turn/door path, preserving ordinary TU,
reservations, timer completion and first-contact interruption. No world targets,
passability queries or cost predictions are exposed. Camera/pose generations
protect against changes away and back; a new tactical view is required after R.

Both UFO/TFTD pass 15 manual/controlled pairs and 29 adversarial worlds, with
30 fresh paired outcome views per game. Nine actual-source faults are detected
in both. Existing stance/selection gates pass on the same final binary. The
315-test Python suite passes normally and optimized (two existing native-only
skips each); official-SDK tests remain synthetic transport evidence. Actual
modal previews were visually checked in both games.

[Evidence and earlier failures](SUPERVISED-TURN-EVIDENCE.md) preserve the corrected
fixture projection and visible-warning timing issues. A failed view during a
TU/reservation warning still stops the adapter without retry. Sliding/UFO doors,
reserved-door refusal, full Stage 5 and actual token measurements remain open.
The later [combined production smoke](COMBINED-SDK-LIVE-EVIDENCE.md) adds bounded
live SDK turn evidence in both games. The right-click slice itself did not close
movement, inventory, attacks, end-turn or full controller acceptance.

### Supervised unit-selection extension (2026-10-12)

The independently opted-in [next/previous unit-selection pilot](SUPERVISED-SELECTION-PILOT.md)
uses the same observed panel capability, local approval and bounded command ledger
as stance. It invokes ordinary next/previous controls, never target IDs or a
hidden eligibility query. Native UFO/TFTD checks compare ten manual/controlled
pairs and nineteen adversarial worlds per game, including a three-unit ring,
wrap/skip/no-op behavior and a fresh post-selection panel for every pair.
The original stance gate is rerun separately. See [verification and limits](SUPERVISED-SELECTION-EVIDENCE.md).
This selection-only delivery did not include movement, inventory, firing,
end-turn control or safe batches; it does not close Stage 5 or establish token savings.

### Verified supervised stance pilot (2026-10-11)

The [pilot](SUPERVISED-STANCE-PILOT.md) provides fresh rendered selected-soldier context, an explicit desired stance and a one-use actor/control-session binding. Only a fresh local confirmation of the displayed dialog dispatches the normal UI action; command receipts distinguish pending confirmation, dispatch and unavailable, while visual verification is a separate observation. Ordinary TU reservations, FOV updates and possible reaction fire remain active.

Both games pass 12 ordinary-UI/controller parity pairs and 19 bounded safety cases, including two fresh post-action panel checks. Five actual-source faults per game are detected: wrong actor, duplicate dispatch, wrong TU cost, reaction bypass and unseen-dialog approval. The initial TFTD response-shape failure is retained; checking the actual inverted-button source surface corrected it without changing default-OFF behavior.

Separate official-SDK → adapter → game demonstrations each completed seven MCP calls and two local confirmations. Actual game views and received panels showed standing 40 TU, kneeling 36 TU and standing 28 TU. Earlier setup, unavailable-context and unconfirmed-operator attempts remain recorded; no failed attempt was counted as success. See the [evidence report](SUPERVISED-STANCE-EVIDENCE.md) and [aggregate evidence](supervised-stance-evidence.json). The controlled fixtures and demonstrations do not establish completed reaction outcomes, a campaign controller or the original full Stage 5. Residual timing channels remain a practical-play/1 limitation. The next comparison requires actual model-usage telemetry; no token savings are claimed.

Earlier displayed-HUD verification: **125 Python tests pass**, together with native/source-boundary tests and historical plus `/1`/`/2` SDK checks. Fourteen successful displayed-HUD reads span both games. Separately sampled screenshot/crop comparisons: nine exact matches, two nonmatches, three not compared; an initial UFO menu request failed for an unknown reason. These are selected results, not full perceptual or timing equivalence. No CI run was recorded for the evidence revision; local test results are not a CI pass.

Additional test-only verification: 15 actual NumberText/Surface-to-projection and native producer–Python adapter tests pass, including all 0–999 numerals and replay/reset/timeout cases. Two deliberate production-source leaks are rejected; [coverage and limits](PAIRED-UI-GATE.md) keep this UI-boundary result separate from full battle simulation. The first [controlled in-engine hidden-occupant render pair](HIDDEN-OCCUPANT-GATE.md) now passes eight full-frame matches and visible-crop positive controls in each game profile; its [source hashes and bounded evidence](hidden-occupant-evidence.json) are recorded. Normal simulation/timers are skipped and audio is disabled in this test. The [remaining proposed hidden-world fixtures](FULL-BATTLE-GATE-PROPOSAL.md) and timing matrix must still be implemented before full controller acceptance; the limited `practical-play/1` route has separate checks. Full event/audio coverage, the broad game matrix and real token benchmarking remain open.

A later bounded functional check now passes in both games: the [observer-request
effects gate](OBSERVER-EFFECTS-GATE.md) runs actual normal kneel/stand input and
timers, compares pretraffic setup and three post-flip states/RNGs, and rejects
an engine-RNG request-path mutation in each profile. Each normal/mutant profile
ran once; libc RNG, timing, audio and initialization/OFF-vs-ON effects remain
unverified. See [aggregate evidence](observer-effects-evidence.json).

The latest [natural-FOV/pause lifecycle gate](NATURAL-FOV-GATE.md) passes six
sampled presentations in each game through normal input and timers. Turning
reveals the hostile, removes it from the player's current FOV list, and
reacquires it; vanilla's remembered rendered visibility remains after list loss.
Independent final-display RGB and glyph oracles check the crop and four HUD
numbers, with null HUD under the pause menu and current HUD after recovery.
Three production-path controls (crop byte, HUD number and pause HUD availability)
are rejected in each game. The final profile uses an initialized muted dummy mixer; the earlier closed-mixer
profile crashed in the normal popup sound callback and is retained as a failed
attempt. Each successful profile ran once. Timing, audible equivalence, AI/end-turn
and full fairness remain unverified; Stage 5 stays open and control stays off.
See [aggregate evidence](natural-fov-evidence.json).

The latest optional [mixed-audio prototype](MIXED-AUDIO-NATIVE.md) captures exactly
500 ms of final application SDL_mixer output after request admission, at 22050 Hz,
signed 16-bit little-endian stereo, with a qualified software-mixed music catalog.
It discards the admission callback block and preserves every sample bit and stereo
channel. It exports no actors, source IDs or positions and retains no continuous
history; sounds between requests can be missed. Its socket and epoch are independent
of pixels/HUD, with no frame synchronization or physical-audibility claim.

The earlier mixed-audio verification recorded **190 passing Python tests**. Native
slot/protocol tests and a real SDL dummy-mixer oracle pass, including overlapping panned sounds, custom
music, post-effects, zero/mute cases, byte-exact output and deliberate-corruption
controls. Official SDK checks cover audio-only and combined visual/audio launch
modes. Separate [actual-game GUI checks](mixed-audio-game-evidence.json) pass for UFO
and TFTD menus in audio-only mode with intros disabled: each has two fresh, nonzero,
distinct clips at verified effective music/sound/UI volumes 84/84/42 and two
exact-silence clips at verified 0/0/0 volumes. All four profiles
reject a request after game exit with the generic unavailable result. An initial
zero-volume attempt actually used default volumes due to misplaced configuration
settings and was excluded before the corrected run; production code was unchanged.
The eight successful clips observe the application dummy-mixer backend only; full gameplay, physical-device output and broad backend/lifecycle
coverage are not established. This earlier menu record has no independent
final-buffer oracle; those clips establish bounded integration only. The separate battlefield gate below adds
an independent same-output oracle during a normal grenade event.
[Payload measurements](../../tools/mcp-observer/mixed-audio-evidence.json) and
component microbenchmarks are not model-token counts or end-to-end savings.
Stage 5, audio parity and controller acceptance remain open.

The [normal battlefield-audio gate](BATTLEFIELD-AUDIO-GATE.md) now passes once
per UFO/TFTD master: four fresh land-only worlds form two idle/requested-observer
pairs, with a grenade origin inside or outside current player-team terrain FOV.
Two native audio requests per master match an independent copy of the exact same
final mixer buffers; the production callback leaves those buffers unchanged.
Successful ordinary grenade Sound playback on an allocated channel is witnessed
during capture. Whole semantic battle state, checked runtime fields and engine
RNG agree within each pair at setup and the normal next-turn input-wait checkpoint.
Three isolated controls per game are rejected at the intended assertions: a corrupt
PCM byte, an extra engine-RNG draw on admission and omission of the ordinary cue.
All six controls ran once without crashes or timeouts.

Normal SDL End Turn dispatch, simulation/animation timers, flash and camera
recentering remain active. The ordinary next-turn screen awaits input, so hostile
AI is not exercised. The profile uses an initialized dummy mixer, land depth 0,
normal music and configured music/sound/UI levels 84/84/42. This is mixed output
during a witnessed event, not isolated grenade PCM or proof of physical audibility.
No new observer fields or controller are added. Full hidden-world/audio coverage,
libc RNG, timing equality, audiovisual synchronization and repeated-run robustness
remain unverified; Stage 5 stays open. See [aggregate evidence](battlefield-audio-evidence.json).

### Diagnostic-isolation update

The [supported launcher boundary](../../tools/mcp-observer/DIAGNOSTIC-ISOLATION.md)
now discards both child stdout and stderr with `DEVNULL`, including startup,
shutdown and the logger's stderr fallback. Native qualification requires effective
`debug`, `debugUi`, `traceAI` and `verboseLogging` to be false and logger severity
`LOG_FATAL` through `LOG_INFO`, inclusive. Initialization follows actual config
parsing and logger setup, before game construction or an observer hello. A debug
build at `LOG_DEBUG` is rejected even with all four options false. No user
configuration is rewritten.

Admission/publication checks reject a changed profile permanently for that process,
including before a completed audio window is sent. Each observer retires at its
next check, not an instantaneous global tick. Historical writers release their
locks and leave prior lawful snapshots on disk with their historical/unverified
meaning. Raw log files are not an observation API. The boundary assumes cooperative
trusted local processes and a matching build/launcher; the unchanged protocol does
not authenticate the executable. All pixel/HUD/audio and timing limits remain.

The Python suite completes 208 test methods in both normal and optimized modes,
with two native-only tests skipped in ordinary unittest runs. The focused runner
separately passes all 18 sentinel tests and 24 native stage/flag cases; these are
not additive counts. In each final normal UFO/TFTD menu profile, cursor PNG,
null displayed HUD and a 44,144-byte audio WAV succeed. UFO with `traceAI=true`
and TFTD with `debug=true` each return generic unavailable for all three tools
while their menus remain available. All four final runs have zero launcher stderr
bytes. See [bounded evidence](diagnostic-isolation-evidence.json).

Three earlier normal UFO attempts returned unavailable visual results: audio
succeeded on the first, was not requested on the second, and a late audio request
was unavailable on the third. The exact rejection cause was not instrumented.
The final unchanged binary succeeded after explicit input focus before the first
request and supported `asyncBlit=false` / filters-off configuration; this does not
establish the cause of the earlier failures. Earlier slice evidence remains
separate. These are bounded menu checks, with no new battle or full-fairness claim.
No controller is added; Stage 5, full audio parity and timing parity remain open.

### Controlled smoke-density render update (2026-10-11)

The separately compiled, default-off [smoke-density gate](SMOKE-DENSITY-GATE.md)
passes its first normal run in each vanilla UFO/TFTD master. In each run, smoke
densities 7 and 8 produce matching complete 640×400 RGB frames at each of
the eight animation phases. Three scenarios yield 24 successful production-native
cursor/HUD responses. Density 6 changes the complete frame and actual cursor
crop in every phase. Whole-frame, geometry and real smoke-frame preconditions
are checked before comparing the pair's complete native transcripts; only
validated transport-random tokens are normalized. The Python adapter is not
exercised by this fixture.

Both RNG seeds are pinned before each fresh scenario's unit/smoke setup. A witness
after the actual production smoke blit verifies the tile, phase, selected frame
and shade. The discovered map has no terrain assets, and depth is fixed at 0
for both masters. Ordinary composition/flip and explicit animation remain, but
simulation/timer-driven thinking is skipped and the whole audio subsystem is
disabled. No new public smoke field or controller is added.

Both real-source mutation controls are rejected in each game at the intended
assertion: hidden density substituted into a native HUD response fails
`smoke-pair-public-transcript` after the visual preconditions pass; a wrong selected
production smoke frame for density 8 fails `smoke-pair-final-rgb` before transcript
comparison. These four runtime mutant rejections are separate from comparator
self-checks. All six normal/mutant runs completed on their first attempt, without
crashes, timeouts or excluded runs. See [aggregate evidence](smoke-density-evidence.json).
One normal run per master does not establish repeated-run robustness. Smoke
simulation, fire, underwater rendering, natural battle histories, load/reconnect,
audio and timing parity remain untested here. Stage 5 remains open.

### Controlled smoke viewport-clipping update (2026-10-11)

The separately compiled, default-off [smoke-clipping gate](SMOKE-CLIPPING-GATE.md)
passes its first normal run in each vanilla UFO/TFTD master on the same frozen
binary. Four fresh worlds form a centered smoke-6/7 positive-control pair and a
wholly clipped smoke-6/7 comparison pair. All eight complete 640×400 RGB frames
and normalized native transcripts match within the clipped pair; all 32 native
cursor/HUD responses are valid. In all eight centered phases, both the complete
frame and the actual produced cursor crop differ. These are separate fresh-world
histories, with camera/input fixed within each pair, not a pan between setups or
a claim of equality across different camera histories.

A witness immediately after the real production smoke blit checks the actual
sprite/source/destination domains: x=336 is inside Map's traversal margin but
wholly beyond the 320-pixel-wide map surface, giving zero horizontal intersection.
Different density groups still select different real smoke frames. Both RNG seeds
are pinned before each world's unit/smoke setup. The discovered, terrain-empty
fixture has depth 0 in both masters; normal composition/flip and explicit animation
remain, while simulation/timer-driven thinking is skipped and audio is disabled.
The Python adapter is not exercised and no public field or controller is added.

Both source mutants are rejected in each game at their intended assertions:
the actual-density-to-HUD leak fails `clipped-pair-public-transcript` after the
visual preconditions pass; collapsing the centered density groups fails
`centered-density-pixel-positive-control`. The latter tests sensitivity of the
centered positive control, not a clipping bypass. All six normal/mutant runs
completed on their first attempt without crashes, timeouts or exclusions. See
[aggregate evidence](smoke-clipping-evidence.json) for the frozen source/binary
and recorded outcomes. One normal run per master does not establish repeatability.
Partial clipping, other edges/occlusion, hidden movement/queues, natural battle histories, lifecycle, audio and timing parity
remain outside this result. Full Stage 5, controller acceptance and actual
end-to-end token measurements remain open.

### Bounded native timing calibration update (2026-10-11)

The separately compiled, default-off [timing calibration](TIMING-CALIBRATION.md)
completed all four frozen profiles: one normal and one delay-mutant run in each
UFO/TFTD master, totaling 304 half samples. Each normal profile completed eight
warmup pairs, 16 measured A/A pairs and 32 measured A/B pairs (112 halves);
each mutant completed four warmup pairs and 16 measured A/B pairs (40 halves).
Every pair, including warmups and mutants, has equal complete-display RGB and
equal normalized full native request/response transcripts. No samples were
trimmed or excluded, and no profiles were retried or replaced. See
[aggregate evidence](timing-calibration-evidence.json).

Both endpoints run in one process on the same main thread. The metrics cover
request-send to return from native response receive, and successful flip to that
same return point. They include same-thread return/receive overhead, not an
independently scheduled arrival timestamp. There is no concurrent receiver or
Python stdio adapter measurement. Request placement, both RNG seeds and animation
phase 1 are fixed; simulation/timer-driven thinking and audio are excluded.

The separate source mutant requests a nominal 5 ms delay before the native
response send when the actual hover tile contains an invisible unit. The frozen
rule requires each metric to have at least 14 positive A-minus-B differences out
of 16 and a median of at least 3 ms. UFO has only 13 positive round-trip
differences, despite 16 positive post-flip differences and both medians exceeding
3 ms: its control is **inconclusive**. TFTD meets the rule for both metrics.
Completion and frame/transcript equality do not establish absence of a timing
leak, timing equivalence or full-gate acceptance.

Unestablished UFO sensitivity remains a blocker to the full timing gate under
this measurement and scheduling design. Retain this result and its frozen
criteria unchanged; do not repeat runs to obtain a preferred result. The chosen
`practical-play/1` route does not require another calibration before the bounded
stance trial. Timing, full Stage 5/controller acceptance and actual end-to-end
token measurements remain open.

## Goal and boundaries

Make joint human–assistant play in OpenXcom more convenient and resource-efficient by exchanging compact observations and normal game commands instead of continuously interpreting the entire screen. The interface must preserve game rules, fog of war, action costs, and the human player's ability to take control at any time.

This document describes staged work. Incremental build and prototype work has begun; publication of the plan alone does not approve control integration or a campaign launch. See the verified progress below. No calendar deadlines are assigned: progress depends on the verifiable results and decisions below.

The minimum version excludes assistant access to complete save files, an omniscient debug map, direct health/TU/ammunition changes, bypassing engine checks, RNG access, probing hidden state through errors, and advancing simulation beyond normal game commands. Original game assets, credentials, and personal materials must not be added to the repository.

## Starting point

- The preliminary source audit is pinned to `630130c5c9ac236b9e1d8496005fb23e84e397ca`.
- The checked fork `master` was `4a4bae5fa2e5a09282928e84943fc84b093e34ff`, dated 2026-09-28, and matched upstream at the initial check, before project documentation changes. The five commits after the audited revision affect the nightly workflow; game code is unchanged in that comparison.
- Game video and audio capture at 960×600 has been verified. This does not establish readiness for 1920×1080 recording.
- The clean-source baseline and experimental observer ON/OFF builds now compile. An opt-in four-number HUD-only snapshot prototype matches UFO and TFTD screens in selected smoke tests. The complete MCP observer, controller and resource-savings measurements are not ready. See [prototype scope, build and validation](HUD-PROTOTYPE.md).

Source references: [audited revision](https://github.com/microtrigger/OpenXcom/tree/630130c5c9ac236b9e1d8496005fb23e84e397ca), [branch base](https://github.com/microtrigger/OpenXcom/tree/4a4bae5fa2e5a09282928e84943fc84b093e34ff), [revision comparison](https://github.com/microtrigger/OpenXcom/compare/630130c5c9ac236b9e1d8496005fb23e84e397ca...4a4bae5fa2e5a09282928e84943fc84b093e34ff), and [project instructions](https://github.com/microtrigger/OpenXcom/blob/4a4bae5fa2e5a09282928e84943fc84b093e34ff/README.md).

## Key decisions

### Perception contract: vanilla perceptual parity selected

The owner chose **vanilla perceptual parity**: preserve information actually conveyed by the normal interface, including legitimate shadow animations, sounds and cursor/aiming hints. Do not suppress such clues merely because their causes are outside current LOS, and do not upgrade ambiguous clues into exact hidden identities, coordinates or statistics. The earlier strict-last-seen recommendation is superseded.

The boundary is final presented pixels, audible mixed output and verified UI-equivalent semantic fields, with the build/assets/options and input/camera history pinned. A legacy-game recollection is a test hypothesis, not proof of this OpenXcom version's behavior. Known terrain follows actual normal rendering; every structured field still needs a reproducible UI justification.

See the [selected policy, practical first-version profile, pinned source audit and paired-test oracle](PERCEPTION-CONTRACT.md). Policy revision `vanilla-perceptual-parity/1-practical` keeps the original full acceptance requirements and adds only the bounded `practical-play/1` route. Per-field validation remains required. This amendment changes no observer field and makes no controller/campaign implementation claim.

### Proposed architecture

1. A native OpenXcom module constructs the player-permitted projection at defined points in the game loop. Full state stays inside the engine.
2. Local IPC transports only that projection and bounded requests. Prefer a local socket/pipe restricted to the current user; the minimum version does not need a network server.
3. An external MCP server exposes compact observations, public events, and later bounded commands. It does not enrich responses by reading save files or process memory.

Implement an observer first. Full control still requires the full non-disclosure gate. The separate `practical-play/1` stance trial requires its focused correctness/information-boundary checks and local in-game human confirmation for each request; a tool argument cannot self-approve. This pilot safeguard is not a permanent approval rule for later joint automation. Connection alone never grants control.

## Stages

### 0. Define the contract and pilot boundaries

**Status:** vanilla perceptual parity and the bounded `practical-play/1` scope selected by the owner; per-field validation and focused pilot checks remain pending.

**Dependencies:** the starting point and preliminary audit.

**Work and output:** apply the selected vanilla perceptual parity policy; describe permitted fields, observation sources, Battlescape/Geoscape boundaries, control ownership, and explicitly unsupported actions. Record the decision and contract version in the repository.

**Acceptance:** every field has a defined player-visible source and update condition; unknown and stale information are distinguishable; the selected option is confirmed before observer implementation.

### 1. Reproducible vanilla baseline and branch build

**Status:** clean-source baseline and experimental branch ON/OFF builds pass; both games pass selected combat/save/reload smoke tests. Audio smoke tests pass for both games on the baseline and HUD binaries. The broader matrix and full stage acceptance remain pending. See [build evidence](HUD-PROTOTYPE.md).

**Dependencies:** approval to begin implementation/build work; stage 0 may run in parallel.

**Work and output:** pin the SHA, toolchain, dependencies, CMake parameters, configuration, mod list, and binary checksums. Build a clean vanilla baseline and the fork's separate working branch. Use separate configuration/save directories, backups, and legally obtained assets for both games. Check launch, entering combat, save/load, audio, and reverting to the original build.

**Acceptance:** the build is repeatable using recorded commands; both games pass smoke tests; the unchanged working branch behaves like vanilla in selected scenarios. Binary equivalence requires a separately verified source/build match.

### 2. Complete the audit of observation and action sources

**Status:** a preliminary audit exists; validation against the approved contract is incomplete.

**Dependencies:** stage 0 and the pinned revision from stage 1.

**Work and output:** trace visibility and tile discovery, map/object/enemy rendering, events, command handling, the game-state queue, pathfinding, TU/ammunition costs, and reaction fire. Produce a mapping of “projection field → source → filter → test” and a list of leakage channels: errors, identifiers, counters, sorting, revisions, routes, delays, and logs.

**Acceptance:** every exported field is justified; hidden state is not disclosed directly or through derived values; dangerous or unclear fields remain excluded until proven safe. Action-availability checks must not become a free oracle for hidden obstacles or enemies.

### 3. Native observer and public projection

**Status:** experimental HUD and final-pixel cursor slices are implemented. A bounded pixel channel now acknowledges presentation after each request; optional `/2` HUD values are decoded from the same presented final pixels, or the entire HUD is null. A separate default-off mixed-audio slice collects fixed 500 ms post-admission final SDL_mixer PCM under its qualified profile. These subsets do not complete the full contract or this stage.

**Dependencies:** stages 0–2.

**Work and output:** implement read-only snapshots/deltas and an allowed-event log. Include fields through an allowlist. Capture HUD snapshots at stable points; capture transient visual/audio cues only after actual presentation, never by exposing intermediate simulation/queue state. Retain only permitted presentation history. Preserve real transient visual/audio clues rather than discarding them through stable-turn-only sampling; do not serialize hidden state or reconstruct missed clues.

**Acceptance:** the interface does not change simulation, RNG, or resources; disabling the module preserves vanilla behavior; snapshots and deltas are consistent; lost history or an incompatible version yields a safe full public snapshot. No control commands are exposed at this stage.

### 4. Local IPC and external MCP observer

**Status:** experimental historical-file and request-bound pixel/HUD transports are implemented with read-only stdio MCP tools. A separate fixed-window audio transport exposes canonical WAV in native MCP audio content; visual and audio requests are not synchronized. Production recovery, broader projection and full-stage acceptance remain open.

**Dependencies:** stage 3; schema design can begin earlier.

**Work and output:** define a versioned schema, message-size and request-rate limits, connection/disconnection, timeouts, and recovery. Public IDs are opaque, stable within their defined lifetime, and do not reveal the creation order of hidden entities. `public_revision` changes only when permitted observations change, not when a hidden simulation counter changes. A new session/load must not accidentally link old IDs to different objects.

**Acceptance:** MCP exposes only the permitted projection; reconnecting does not disclose history preceding allowed observation; logs and errors follow the same filtering rules; the interface is disabled by default and remains local. A timeout does not become an automatic repeated command.

### 5. Security gate: paired hidden-state tests

**Status:** cache, transport, structural and bounded UI/render-pair tests pass. The new [smoke-density pair](SMOKE-DENSITY-GATE.md) passes one render-only run per UFO/TFTD master: eight full-frame matches, 24 native responses and eight boundary-positive phases, with audio disabled; both source mutants are rejected in each game. The separate [smoke viewport-clipping pair](SMOKE-CLIPPING-GATE.md) passes eight equal clipped whole frames and equal complete native transcripts, 32 native responses and eight centered 6/7 positive phases per game; the HUD-leak and centered-positive-control mutants are rejected in both games. A separate same-binary request-effects test now passes three normal standing/kneeling/standing checkpoints in UFO/TFTD and rejects an engine-RNG consumption mutant in both. This adds bounded functional-effects evidence; the earlier hidden-occupant pair still skips simulation/timers. A further natural-FOV/pause fixture verifies six sampled presentations per game against independent final-pixel crop/HUD oracles. A real-SDL synthetic final-mix oracle also passes with byte-exact PCM and corruption controls. The [normal battlefield-audio gate](BATTLEFIELD-AUDIO-GATE.md) now passes one land-only run per game: visible/outside-player-team-FOV grenade events, exact independent same-buffer PCM, unchanged output buffers and paired semantic battle-state/checked-runtime/engine-RNG equality. Normal End Turn input and timers reach the ordinary next-turn input wait; hostile AI is not exercised. PCM-corruption, extra-RNG-draw and missing-cue controls are rejected once each in both games at the intended assertions. The [bounded native timing calibration](TIMING-CALIBRATION.md) completed all four profiles with equal pairwise frames/transcripts, but its 5 ms control is inconclusive in UFO and meets the frozen rule only in TFTD. This is descriptive same-thread native return-path evidence; timing sensitivity remains unestablished for UFO. Full hidden-world, lifecycle, audio parity, libc RNG and timing coverage remains pending. This original full-controller gate has not passed; the separate limited `practical-play/1` route does not claim otherwise.

**Dependencies:** stages 3–4 and the approved contract.

**Work and output:** construct pairs with equivalent final presented video/audible output and identical permitted input/camera history but different hidden state: enemy positions/counts, unseen items, doors, damage, smoke/fire, and changes outside visibility. Compare responses, public IDs, revisions, message sizes/order, errors, events, command availability, and diagnostic logs. Separately measure correlations between response timing and secrets.

**Acceptance:** until a legitimate observable event occurs, the pair is indistinguishable through the available interface. After that event, allow only differences explained by the contract. Tests fail when deliberate leaks are introduced. Document acceptable timing noise and its evaluation method; do not claim absolute protection against all side channels without proof. Any unexplained leak blocks full controller acceptance. The separately selected `practical-play/1` profile permits the bounded supervised trial with disclosed residual timing limitations, while unresolved non-timing disclosure, rule bypass or loss of control still blocks that trial.

### 6. Minimal controller through the normal action pipeline

**Status:** the default-off supervised stance pilot is implemented and verified in the bounded native and actual MCP/game demonstrations above. The separately opted-in next/previous unit-selection extension is also implemented with bounded native checks; see its [evidence](SUPERVISED-SELECTION-EVIDENCE.md). The separately opted-in ordinary right-click pilot also passes bounded turn/door checks in both games; see its [evidence](SUPERVISED-TURN-EVIDENCE.md). The separately opted-in [ordinary left-click pilot](SUPERVISED-MAP-LEFT-CLICK-EVIDENCE.md) adds bounded movement/selection and consequence checks. The [inventory-opening/read-only-view slice](SUPERVISED-INVENTORY-OPEN-EVIDENCE.md) and [combined SDK/production-game chains](COMBINED-SDK-LIVE-EVIDENCE.md) are verified within their stated limits. Full controller acceptance remains blocked by stage 5; broader movement, inventory item manipulation, automated closing, firing and end-turn commands remain open.

**Dependencies:** full controller: stages 1–5 and agreed commands. Limited stance pilot: the source/presentation boundary and focused correctness, one-use request, local confirmation and safe-stop checks in `practical-play/1`. The broader command list below remains outside that pilot.

**Work and output:** gradually add soldier selection, turning/movement, stance, permitted inventory operations, firing, and ending the turn. Every request uses normal validation and the game-action queue; preserve TU, ammunition, animations, reaction fire, panic, death, and interruptions. Do not invoke internal mutators to bypass that pipeline.

A request includes a session ID, command ID, expected public revision, and parameters. Repeating the same ID with the same parameters returns the recorded status/result without executing the action twice; conflicting parameters are rejected. After an uncertain response, the client queries command status. If the outcome cannot be recovered, stop control for reconciliation rather than submitting the command again under a new ID. A stale snapshot or human intervention requires a fresh observation.

**Acceptance:** UI-versus-MCP comparisons on identical scenarios confirm identical gameplay consequences; retries, lost responses, or reconnects cause no double movement/shots; errors do not disclose hidden causes. For full controller acceptance, the complete stage 5 test suite passes again after the controller is added. The limited stance pilot must pass its focused checks separately, including normal TU/FOV/reaction consequences, no duplicate dispatch and human takeover; distinguish request/dispatch from a visually verified outcome.

### 7. Safe sequential batches

**Status:** not started.

**Dependencies:** stage 6 and another successful stage 5 run.

**Work and output:** limit batch length; execute commands strictly sequentially, checking current public conditions after each. Do not promise atomicity or rollback of actions that have already happened. Stop the remainder on a newly observed enemy, reaction fire, injury/death, a visible obstacle, exhausted TU/ammunition, a turn change, a dialog, manual intervention, or cancellation. Stopping and its explanation must be based on a public event.

**Acceptance:** reproducible tests establish the exact stopping boundary; unexecuted commands do not resume after reconnection without a new decision; results include the executed portion and a safe explanation. Batches cannot investigate hidden facts through a series of free probes.

### 8. UFO + TFTD matrix and regressions

**Status:** selected UFO/TFTD prototype checks completed; the full regression matrix and stage acceptance remain open. See the plan-versus-actual summary above.

**Dependencies:** test suites begin at stage 3; final acceptance follows stage 7.

**Work and output:** for both games, cover new and loaded campaigns, Geoscape/Battlescape, day/night conditions, elevation levels, doors/stairs, disappearing and reappearing enemies, inventory, different weapon types, reaction fire, turn/mission completion, save/load, connection loss, and manual takeover. For TFTD, also include underwater/land weapon restrictions and multi-stage missions. List unsupported modes explicitly.

**Acceptance:** the matrix contains reproducible scenarios and distinct passed/failed/not-run statuses for each game and version; no critical information leaks, rule bypasses, or loss-of-control issues remain open. Test fixtures and CI do not redistribute original game assets.

### 9. Measure value against visual control

**Status:** protocol and offline measurement harness prepared; audio payload sizes and bounded component microbenchmarks recorded. Actual end-to-end token/resource comparison has not run and no token-savings result is established.

**Dependencies:** a visual baseline can be collected after stage 1; the full comparison follows stage 8. A bounded `practical-play/1` GUI/MCP token comparison may follow the verified supervised stance trial, with its narrower scope reported explicitly.

**Work and output:** perform the same tasks through screenshots/normal UI and through MCP in comparable scenarios. Record tool-call and screenshot counts, data and token volume, action-completion latency, CPU/RAM, errors, and human interventions. Report benchmark settings, versions, repetitions, and variation; account separately for observer and IPC overhead. Retain visual checks against the actual game screen as a correctness control.

**Acceptance:** publish the method and results, including regressions and unsuccessful runs. Claim savings only from measured data. If there is no benefit, revisit observation granularity/deltas without weakening the fair-play contract.

### 10. Short instruction guide and joint-campaign pilot

**Status:** not started.

**Dependencies:** stages 8–9 and separate agreement to launch the pilot.

**Work and output:** prepare a small gameplay skill/guide covering connection, observation schema, unknown and last-seen semantics, available actions, the “observe → decide → act → verify” loop, human takeover, cancellation, and recovery after uncertain outcomes. Agree on the game, difficulty, pilot duration/boundaries, and the assistant's decision-making authority. Begin with one bounded mission or another preselected episode.

**Acceptance:** the human and assistant complete the episode without forbidden information, duplicate commands, or rule changes; the human successfully takes control. The report includes problems found, metrics, a next-step decision, and how to return to vanilla. A successful pilot is not presented as proof of readiness for a full campaign or every mod.

### 11. Optional: Full HD and YouTube

**Status:** deferred; separate approval required before starting.

**Dependencies:** a useful stage 10 pilot; agreement to record and, separately, to publish.

**Work and output:** verify actual game output and the final 1920×1080 file, aspect ratio, UI readability, audio/synchronization, recording overhead, and absence of personal information in the frame. Distinguish native output, scaling, and borders where applicable. Then prepare the selected video, title, description, and visibility settings for review before publication.

**Acceptance:** technical properties are confirmed by file inspection and sample playback. Uploading/publishing to YouTube requires explicit permission for the specific video and visibility settings. This stage does not block MCP and is outside minimum readiness.

## Progression and definition of ready

Full-acceptance sequence: 0 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10. Stage 1 is required before implementation and reproducible validation; matrix preparation and the visual baseline run alongside early stages. Stage 11 is independent of the minimum deliverable.

Under the original full-acceptance route, the minimum version is ready for a pilot when the contract is selected, both games pass the stated matrix, non-disclosure tests pass, normal action rules are preserved, retries/interruptions are safe, the human can regain control, and honest resource measurements exist. Recheck these conditions whenever the contract, projection, or controller changes.

The selected route is `practical-play/1`: the locally confirmed stance implementation and supervised two-game demonstrations are complete within their recorded limits. The next/previous selection extension adds a separately verified bounded control. Actual paired GUI/MCP token telemetry still requires an authorized instrumented run. Preserve existing evidence and its frozen criteria; Stage 5 stays open. This route changes the pilot prerequisite, not the reported test outcomes or the original full-acceptance definition.

### Additional presented-pixel experiment

The separately default-off [cursor-region prototype](CURSOR-PROTOTYPE.md) passed selected exact pixel comparisons in both games, plus edge/focus/overlay checks. It is latest-only RGB output, not compact semantic MCP; resource savings and full perceptual parity are unmeasured. The controller gate remains open.

### Local historical-only MCP adapter

The [stdio inspection adapter](../../tools/mcp-observer/README.md) exposes two read-only historical tools for existing HUD observations and a lossless PNG cursor crop. Its 23 unit tests and official Python SDK interoperability pass. Actual UFO historical reads and post-exit rejection were checked; TFTD adapter integration is not yet tested. Requests requiring current state always fail closed. It runs as a local subprocess and provides no gameplay control.

### Resource comparison protocol

The [benchmark protocol and offline harness](../../tools/benchmarks/README.md) define paired GUI/MCP tasks, correctness/freshness gates and a complete model-usage ledger. Offline encoding/parsing measurements are only component baselines. A final end-to-end comparison requires actual token telemetry for every call and retry; unavailable values stay unknown and block token-savings conclusions.

### Request-bound pixel presentation

The [presentation prototype](PRESENTATION-PROTOTYPE.md) now acknowledges a normal displayed crop after its specific request. Selected UFO/TFTD menu, battle and overlay comparisons pass, alongside 43 Python tests and native ordering/transport checks. Historical HUD remains separate; protocol `/1` does not provide request-bound HUD, and gameplay control is unsupported. Timing parity and resource savings are not yet established.

### Request-bound displayed HUD (2026-10-10)

The separately opted-in [/2 displayed-HUD slice](PRESENTED-HUD-PROTOTYPE.md)
decodes exactly four final-pixel numbers in 0–999 or returns a generic null HUD,
bound to the same request and presentation. Historical tools and default `/1`
remain unchanged. Synthetic C++/source-boundary checks, 125 Python tests and
both protocol versions' SDK interoperability pass. Fourteen selected reads across
UFO and TFTD checked menu/battle, kneeling, overlays, cursor overlap and reload;
TFTD pointer exit closed the channel without stale HUD. Nine separately sampled
screenshot/crop comparisons matched, two did not; an initial UFO menu failure
remains unexplained. See the [bounded evidence](presented-hud-build-evidence.json).
Full paired hidden-state timing tests, broader perceptual parity and the
controller acceptance gate remain open.

## Bounded observer-request effects test

The separate default-off [observer-effects gate](OBSERVER-EFFECTS-GATE.md) passed
three real standing/kneeling/standing checkpoints in UFO and TFTD land profiles.
Each compares two same-binary worlds: connected idle observer versus three valid
requests, with ordinary event dispatch, think, timers and FPS scheduling intact.
Internal gameplay state and engine RNG match after each checkpoint presentation. A temporary
request-path engine-RNG consumption mutant is rejected by the actual RNG
comparison in both profiles. See [aggregate evidence and source hashes](observer-effects-evidence.json).

Each normal profile and each mutant control ran once. Default-OFF and opt-in
builds and existing regression tests pass; no hosted CI or repeatability
distribution is claimed. Libc RNG state remains dynamically unverified, audio is
disabled, and initialization effects, observer-OFF/ON equivalence, timing, pixel
equality and full fairness remain outside this gate. Stage 5 stays open and no
MCP controller is enabled. Broader hidden-world, lifecycle and event/audio
coverage remain next work.

## Bounded natural-FOV and pause-menu lifecycle test

The separately enabled [natural-FOV fixture](NATURAL-FOV-GATE.md) passes one
six-sample land profile in each of UFO and TFTD. Normal right-click turning
charges one TU per step and drives current FOV membership 0/1/0/1. The stationary
hostile's retained visibility follows 0/1/1/1, preserving vanilla clues after
turn-away. Ordinary options/cancel keys open and close PauseState with normal
Battlescape reinitialization and timers on recovery.

Every response crop is compared byte-for-byte with an independent read of the
final physical SDL display, and all four HUD values are independently decoded
from those pixels. The pause response has null HUD and current menu pixels;
recovery restores a current non-null HUD. Three isolated production-source controls were rejected in both games at their
intended assertions: a changed crop byte, a wrong HUD number and a non-null pause
HUD. Each mutation/master ran once. No gameplay controls or contact fields
are added to MCP. Default-OFF/ON builds and existing regressions pass; see
[aggregate source/binary identification](natural-fov-evidence.json).

These are sampled presentation and lifecycle checks, not every transient frame
or a paired hidden-world proof. The initial/reveal image difference also includes
possible orientation, discovery and cursor changes. The final initialized-muted-
dummy audio profile preserves ordinary popup callbacks without claiming audible
parity. An earlier closed-audio attempt plus two diagnostics crashed there and
are recorded separately. No hosted CI, timing equivalence, AI/end-turn coverage
or full fairness is claimed. Stage 5 remains open; the controller is disabled.

## Fixed-window mixed-audio observation (2026-10-10)

The independently enabled [native collector](MIXED-AUDIO-NATIVE.md) and
[local MCP audio adapter](../../tools/mcp-observer/AUDIO-ADAPTER.md) expose
read-only `read_mixed_audio({})`. The native feature defaults to OFF; launch audio
explicitly with `--mixed-audio` alongside the existing presentation channel or
`--audio-only` without one. Existing historical and `/1`/`/2` schemas stay unchanged.

Qualification requires SDL_mixer 1.2.12, its actual 22050 Hz signed-16-bit
little-endian stereo format and a fully loaded software-mixed music catalog.
All MIDI, external-command and unknown music types fail closed;
qualification uses only an aggregate catalog property, not the current track or
an individual source. The observer changes no audio settings to qualify.

A success carries 11025 stereo frames: 44100 PCM bytes in a 44144-byte canonical
WAV. Internal native hex occupies 88200 characters; the 88571-byte native JSON
packet is not compact audio. Base64 uses 58860 characters before the MCP envelope.
These are byte/character measurements, not model tokens. One request is in flight;
malformed, replayed, late or reset streams become generically unavailable without
reusing stale audio. There is no continuous history, source/actor identity,
physical-device audibility guarantee, or audiovisual clock/frame pairing.

For that earlier slice, the 190-test Python suite, native slot/protocol checks,
real-SDL dummy-mixer oracle
and SDK audio-content checks pass within their documented scope. The separate
[UFO/TFTD menu GUI record](mixed-audio-game-evidence.json) contains eight successful
clips and four generic post-exit rejections, with verified enabled and zero-volume
settings. It checks application dummy-mixer output, not physical audibility or
full gameplay. The separate battlefield gate below adds bounded functional-effects
evidence; timing noninterference and the full UFO/TFTD audio parity matrix remain
unverified. No gameplay control is added;
Stage 5 and actual end-to-end resource/token comparison remain open.

## Bounded normal battlefield-audio test (2026-10-10)

The separate default-OFF [battlefield-audio gate](BATTLEFIELD-AUDIO-GATE.md)
passes one normal run for UFO and one for TFTD, each using four fresh land-only
worlds at depth 0. One same-orientation idle/requested-observer pair has the grenade
origin inside current player-team terrain FOV; the other has it outside. Natural
FOV determines visibility. Each master performs two native audio requests through
the existing twelve-field mixed-audio schema; this test adds no observer fields
or MCP gameplay controls.

Ordinary SDL End Turn input triggers the native ground-grenade and explosion path.
The expected ordinary grenade Sound successfully obtains a mixer channel during
capture. An independent test-only postmix trampoline copies the same final output
before invoking the production observer callback, verifies that callback leaves
the output unchanged, and checks every returned PCM byte against its own capture.
It reads neither the native capture slot nor its private state. The normal music
path and configured music/sound/UI levels 84/84/42 are retained with an initialized
dummy backend. Nonzero mixed PCM alone would not establish grenade audibility;
this result covers mixed output during a separately witnessed normal event.

Each requested world matches its idle twin in the whole semantic SavedBattleGame
structure, checked additional runtime fields and engine RNG at setup and after the
normal side change. The ordinary NextTurnState input wait prevents hostile AI from
running; simulation and animation timers, blast flash and camera recentering are
preserved. Equality is between each same-orientation pair's checked state, not
between different worlds' audio waveforms, wall-clock pacing or callback counts.

Three isolated mutation controls are rejected in each game: corruption of a
serialized PCM byte fails the exact independent comparison; an extra engine-RNG
draw on an admitted request fails paired equality; omission of the ordinary
grenade cue fails the playback witness despite music remaining enabled. All six
controls ran once and failed at their intended assertions, without a crash or
timeout. Both normal runs passed on the first attempt without runtime source
corrections. See [aggregate runtime evidence](battlefield-audio-evidence.json).

One normal run per master does not establish repeatability, physical-device
audibility, an isolated grenade waveform, libc RNG equality, hostile AI behavior, full gameplay
or hidden-world coverage, timing equality, broad backend support or audiovisual
synchronization. The earlier **190 Python tests** remain a separate regression
suite. Stage 5 and full audio parity remain open; the controller stays disabled.
