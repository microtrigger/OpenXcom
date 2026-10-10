<!-- New project documentation: SPDX-License-Identifier: GPL-3.0-or-later. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# OpenXcom: Fair-Play MCP Interface Work Plan

English visitor-facing version · [Русская рабочая версия / Russian working version](PLAN.ru.md)

Status as of October 10, 2026. We maintain the Russian version for day-to-day project work; this English version presents the same scope, stages, and acceptance gates for visitors.

## Plan versus actual progress

Displayed-HUD evidence baseline: [b8d9806](https://github.com/microtrigger/OpenXcom/commit/b8d9806aa83151c4da1805c691f50903512c7f65). The [natural-FOV/pause gate](natural-fov-evidence.json) is based on `a8750ad` plus recorded source hashes. The latest bounded slice is [request-bound mixed audio](MIXED-AUDIO-NATIVE.md), with [adapter measurements](../../tools/mcp-observer/mixed-audio-evidence.json), 2026-10-10. **Overall: in progress; read-only prototypes, not a completed player interface.** Update this summary and the Russian working version after each verified delivery, with its evidence revision, remaining gates, and failed/not-run checks. A delivered subset does not close its parent stage.

| Stage / planned outcome | Actual result | Remaining acceptance gate |
| --- | --- | --- |
| 0 · Perception contract | Vanilla perceptual parity selected; [policy documented](PERCEPTION-CONTRACT.md). | Per-field validation and pilot boundaries. |
| 1 · Reproducible baseline/build | Clean baseline and opt-in/default-OFF builds pass; selected combat/save/load/audio checks in both games ([build evidence](HUD-PROTOTYPE.md)). | Broader repeatability and complete stage acceptance. |
| 2 · Source audit | Pinned source audit and selected presentation boundaries documented. | Complete field/action and side-channel audit. |
| 3 · Native observer | Historical HUD/cursor slices, request-bound pixels, optional four-number displayed HUD and default-off [500 ms final-mix audio](MIXED-AUDIO-NATIVE.md) implemented. | Full approved projection, event coverage, broader game-audio/lifecycle validation and parity. |
| 4 · IPC/MCP | Local read-only stdio tools, request-bound `/1`/`/2` visual protocols and a separate [native MCP audio tool](../../tools/mcp-observer/AUDIO-ADAPTER.md) implemented; SDK checks pass. | Production recovery and broader observation coverage; no audiovisual synchronization. |
| 5 · Non-disclosure gate | Native/synthetic, source-boundary and transport tests pass; [15 production UI/transport paired tests and two deliberate leak controls](PAIRED-UI-GATE.md), plus [one controlled in-engine hidden-occupant render pair](HIDDEN-OCCUPANT-GATE.md) in UFO/TFTD, also pass. A separate [request-effects gate](OBSERVER-EFFECTS-GATE.md) passes normal kneel/stand checkpoints in both games and rejects engine-RNG consumption mutants. The [natural-FOV/pause gate](NATURAL-FOV-GATE.md) passes six real input/timer-driven presentations with independent crop/HUD pixel checks in each game. A real-SDL synthetic audio oracle checks byte-exact final-mix capture and rejects deliberate corruptions. | Full paired hidden-engine-state, audio and timing tests; controller gate remains open. |
| 6 · Normal-action controller | Not implemented; no gameplay inputs exposed. | Stage 5 and an agreed command scope. |
| 7 · Sequential batches | Not implemented. | Controller, interruption and duplicate-execution guarantees. |
| 8 · UFO/TFTD matrix | Selected visual checks and [mixed-audio menu/zero-volume/exit checks](mixed-audio-game-evidence.json) pass in both games; earlier failures and unmatched comparisons are retained in [bounded visual evidence](presented-hud-build-evidence.json). | Full regression matrix, including unsupported/not-run cases. |
| 9 · Resource comparison | Protocol, offline harness and usage-ledger format prepared ([benchmark documentation](../../tools/benchmarks/README.md)); bounded audio byte counts and component microbenchmarks recorded. | Actual end-to-end model-token/latency/resource measurement; no token-savings claim. |
| 10 · Guide and joint pilot | Prototype usage documentation exists; gameplay guide and campaign pilot not completed. | Earlier acceptance gates and agreement on a bounded pilot. |
| 11 · Full HD / YouTube | Deferred. | Separate recording and publication approval. |

Earlier displayed-HUD verification: **125 Python tests pass**, together with native/source-boundary tests and historical plus `/1`/`/2` SDK checks. Fourteen successful displayed-HUD reads span both games. Separately sampled screenshot/crop comparisons: nine exact matches, two nonmatches, three not compared; an initial UFO menu request failed for an unknown reason. These are selected results, not full perceptual or timing equivalence. No CI run was recorded for the evidence revision; local test results are not a CI pass.

Additional test-only verification: 15 actual NumberText/Surface-to-projection and native producer–Python adapter tests pass, including all 0–999 numerals and replay/reset/timeout cases. Two deliberate production-source leaks are rejected; [coverage and limits](PAIRED-UI-GATE.md) keep this UI-boundary result separate from full battle simulation. The first [controlled in-engine hidden-occupant render pair](HIDDEN-OCCUPANT-GATE.md) now passes eight full-frame matches and visible-crop positive controls in each game profile; its [source hashes and bounded evidence](hidden-occupant-evidence.json) are recorded. Normal simulation/timers are skipped and audio is disabled in this test. The [remaining proposed hidden-world fixtures](FULL-BATTLE-GATE-PROPOSAL.md) and timing matrix must still be implemented before controller acceptance. Full event/audio coverage, the broad game matrix and real token benchmarking remain open.

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

The combined Python suite now passes **190 tests**. Native slot/protocol tests and
a real SDL dummy-mixer oracle pass, including overlapping panned sounds, custom
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
coverage are not established. The independent final-buffer oracle belongs to
the synthetic mixer tests; these game clips establish bounded integration only.
[Payload measurements](../../tools/mcp-observer/mixed-audio-evidence.json) and
component microbenchmarks are not model-token counts or end-to-end savings.
Stage 5, audio parity and controller acceptance remain open.

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

See the [selected policy, pinned source audit and revised paired-test oracle](PERCEPTION-CONTRACT.md). The policy choice is confirmed; detailed schema validation and stage acceptance remain open. The four-number HUD prototype remains unchanged. No controller or campaign has been enabled.

### Proposed architecture

1. A native OpenXcom module constructs the player-permitted projection at defined points in the game loop. Full state stays inside the engine.
2. Local IPC transports only that projection and bounded requests. Prefer a local socket/pipe restricted to the current user; the minimum version does not need a network server.
3. An external MCP server exposes compact observations, public events, and later bounded commands. It does not enrich responses by reading save files or process memory.

Implement an observer first. Enable control only after non-disclosure tests pass; a local process being able to connect to the game does not itself grant permission to control it.

## Stages

### 0. Define the contract and pilot boundaries

**Status:** vanilla perceptual parity selected by the owner; the detailed per-field contract and pilot boundaries still require validation.

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

**Status:** cache, transport, structural and bounded UI/render-pair tests pass. A separate same-binary request-effects test now passes three normal standing/kneeling/standing checkpoints in UFO/TFTD and rejects an engine-RNG consumption mutant in both. This adds bounded functional-effects evidence; the earlier hidden-occupant pair still skips simulation/timers. A further natural-FOV/pause fixture verifies six sampled presentations per game against independent final-pixel crop/HUD oracles. A real-SDL synthetic final-mix oracle also passes with byte-exact PCM and corruption controls. Full hidden-world, lifecycle, audio parity, libc RNG and timing coverage remains pending. This mandatory controller gate has not passed.

**Dependencies:** stages 3–4 and the approved contract.

**Work and output:** construct pairs with equivalent final presented video/audible output and identical permitted input/camera history but different hidden state: enemy positions/counts, unseen items, doors, damage, smoke/fire, and changes outside visibility. Compare responses, public IDs, revisions, message sizes/order, errors, events, command availability, and diagnostic logs. Separately measure correlations between response timing and secrets.

**Acceptance:** until a legitimate observable event occurs, the pair is indistinguishable through the available interface. After that event, allow only differences explained by the contract. Tests fail when deliberate leaks are introduced. Document acceptable timing noise and its evaluation method; do not claim absolute protection against all side channels without proof. Any unexplained leak blocks the controller.

### 6. Minimal controller through the normal action pipeline

**Status:** not started; blocked by stage 5.

**Dependencies:** stages 1–5 and an agreed set of control commands.

**Work and output:** gradually add soldier selection, turning/movement, stance, permitted inventory operations, firing, and ending the turn. Every request uses normal validation and the game-action queue; preserve TU, ammunition, animations, reaction fire, panic, death, and interruptions. Do not invoke internal mutators to bypass that pipeline.

A request includes a session ID, command ID, expected public revision, and parameters. Repeating the same ID with the same parameters returns the recorded status/result without executing the action twice; conflicting parameters are rejected. After an uncertain response, the client queries command status. If the outcome cannot be recovered, stop control for reconciliation rather than submitting the command again under a new ID. A stale snapshot or human intervention requires a fresh observation.

**Acceptance:** UI-versus-MCP comparisons on identical scenarios confirm identical gameplay consequences; retries, lost responses, or reconnects cause no double movement/shots; errors do not disclose hidden causes. The complete stage 5 test suite passes again after the controller is added.

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

**Dependencies:** a visual baseline can be collected after stage 1; the final comparison follows stage 8.

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

Main sequence: 0 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10. Stage 1 is required before implementation and reproducible validation; matrix preparation and the visual baseline run alongside early stages. Stage 11 is independent of the minimum deliverable.

The minimum version is ready for a pilot when the contract is selected, both games pass the stated matrix, non-disclosure tests pass, normal action rules are preserved, retries/interruptions are safe, the human can regain control, and honest resource measurements exist. Recheck these conditions whenever the contract, projection, or controller changes.

Next steps and their current status are tracked in the plan-versus-actual summary above. Continue paired-engine and timing validation; no controller is enabled.

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

The 190-test Python suite, native slot/protocol checks, real-SDL dummy-mixer oracle
and SDK audio-content checks pass within their documented scope. The separate
[UFO/TFTD menu GUI record](mixed-audio-game-evidence.json) contains eight successful
clips and four generic post-exit rejections, with verified enabled and zero-volume
settings. It checks application dummy-mixer output, not physical audibility or
full gameplay. Timing/functional noninterference and the full UFO/TFTD audio
parity matrix remain unverified. No gameplay control is added;
Stage 5 and actual end-to-end resource/token comparison remain open.
