<!-- New project documentation: SPDX-License-Identifier: GPL-3.0-or-later. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# OpenXcom: Fair-Play MCP Interface Work Plan

English visitor-facing version · [Русская рабочая версия / Russian working version](PLAN.ru.md)

Status as of October 10, 2026. We maintain the Russian version for day-to-day project work; this English version presents the same scope, stages, and acceptance gates for visitors.

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

**Status:** experimental HUD and final-pixel cursor slices are implemented. A bounded pixel channel now acknowledges presentation after each request. These subsets do not complete the full contract or this stage.

**Dependencies:** stages 0–2.

**Work and output:** implement read-only snapshots/deltas and an allowed-event log. Include fields through an allowlist. Capture HUD snapshots at stable points; capture transient visual/audio cues only after actual presentation, never by exposing intermediate simulation/queue state. Retain only permitted presentation history. Preserve real transient visual/audio clues rather than discarding them through stable-turn-only sampling; do not serialize hidden state or reconstruct missed clues.

**Acceptance:** the interface does not change simulation, RNG, or resources; disabling the module preserves vanilla behavior; snapshots and deltas are consistent; lost history or an incompatible version yields a safe full public snapshot. No control commands are exposed at this stage.

### 4. Local IPC and external MCP observer

**Status:** experimental historical-file and request-bound pixel transports are implemented with read-only stdio MCP tools. Production recovery, broader projection and full-stage acceptance remain open.

**Dependencies:** stage 3; schema design can begin earlier.

**Work and output:** define a versioned schema, message-size and request-rate limits, connection/disconnection, timeouts, and recovery. Public IDs are opaque, stable within their defined lifetime, and do not reveal the creation order of hidden entities. `public_revision` changes only when permitted observations change, not when a hidden simulation counter changes. A new session/load must not accidentally link old IDs to different objects.

**Acceptance:** MCP exposes only the permitted projection; reconnecting does not disclose history preceding allowed observation; logs and errors follow the same filtering rules; the interface is disabled by default and remains local. A timeout does not become an automatic repeated command.

### 5. Security gate: paired hidden-state tests

**Status:** cache, transport and structural tests pass for the HUD slice; paired hidden-engine-state tests remain pending. This mandatory controller gate has not passed.

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

**Status:** not started.

**Dependencies:** test suites begin at stage 3; final acceptance follows stage 7.

**Work and output:** for both games, cover new and loaded campaigns, Geoscape/Battlescape, day/night conditions, elevation levels, doors/stairs, disappearing and reappearing enemies, inventory, different weapon types, reaction fire, turn/mission completion, save/load, connection loss, and manual takeover. For TFTD, also include underwater/land weapon restrictions and multi-stage missions. List unsupported modes explicitly.

**Acceptance:** the matrix contains reproducible scenarios and distinct passed/failed/not-run statuses for each game and version; no critical information leaks, rule bypasses, or loss-of-control issues remain open. Test fixtures and CI do not redistribute original game assets.

### 9. Measure value against visual control

**Status:** not started; no resource-savings figures are available.

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

Next step: validate final-frame cursor/overlay fixtures under the selected perceptual-parity contract, then review the next read-only presentation slice. Continue paired-engine testing; no controller is enabled.

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
