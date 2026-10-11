<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original project documentation, 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Bounded native return-path timing calibration

This default-off fixture records descriptive timing for the controlled
hidden-occupant render pair. It is a calibration of this native harness and its
sensitivity control. It is not a timing-equivalence, confidentiality, statistical
significance, or full-fairness test. The first bounded graphical run completed all four planned profiles. The
measured results and the inconclusive UFO control are recorded below.

## Scope and measurement boundaries

Both endpoints are in one process, on the same main thread, connected by a Unix
socketpair. Each half constructs the controlled battle again, then issues one
request before the ordinary observer frame boundary. Its three timestamps use
`std::chrono::steady_clock`, reported privately as integer nanosecond offsets
from that run's start:

- `t0`: immediately before the native peer sends the request
- `flip`: the successful `SDL_Flip` hook, before subsequent observer publication
- `t1`: the first operation after the native peer's single response `recvmsg`,
  before JSON parsing and validation

The reported metrics are `round_trip_ns = t1 - t0`, the native harness
round-trip/return path, and `post_flip_ns = t1 - flip`, the post-flip native
harness return path. The latter still includes same-thread return flow and
receive overhead. It is not an independently scheduled arrival timestamp or a
pure producer/send-duration measurement. There is no concurrent receiver.
Neither metric measures Python-adapter stdio latency, an external client,
network delay, arbitrary request admission timing, or a general gameplay timing
channel. Request placement is fixed before the frame boundary.

A fixed, preallocated sample slot holds timestamps. During the timed frame the
fixture retains only bounded frame metadata. The full palette-resolved display
copy, record serialization, protocol parsing, normalization and equality checks
happen after `t1`. Post-measurement work can affect later samples; successive
pairs are not assumed independent. After each result, the next request waits until at least 300 ms after the
previous `t1`; the wrapper independently checks this spacing in every observed
record, including across pair boundaries. No time sample is silently removed because
it is large, small, negative after subtraction, or inconvenient.

## Fixed scenarios, schedule and counts

This is the existing controlled hidden-occupant construction, not a naturally
reached battle history. One selected player and one alive invisible hostile are
present in the same fixed roster. For an A half the hostile occupies the hover
tile. For a B half in an A/B pair it is parked away from that tile. In an A/A
pair both halves have the same occupied-hover construction; A and B are simply
the two half labels used to define the subtraction. Warmups use A/B geometry.

Every half starts with engine RNG seed `123456789` and C-library RNG seed
`12345` before scenario construction. Ordinary Battlescape initialization is
followed by one explicit animation step, fixing the sampled animation phase at
1. The same resources, selected unit, camera, discovery, HUD, cursor and setup
rules apply to both halves. These fixed seeds and phase deliberately narrow the
claim; they do not represent a distribution over battles or animation phases.

Before sampling, the full order schedule is computed using a separate
xorshift32 state seeded with `0x4f584354`. Each pair advances that state once by
shifts 13, 17 and 5 with 32-bit masking; odd state chooses BA, even state AB.
Both masters use the same schedule. Each normal and mutant process restarts the
same seed. The wrapper independently validates this exact schedule, including
half order and sequential pair/sample indexes; measured results never choose
which sample comes next.

The immutable plan is:

- Normal UFO: 8 warmup pairs, then 16 measured A/A pairs, then 32 measured A/B
  pairs; 56 pairs and 112 half samples total
- Normal TFTD: the same 56 pairs and 112 half samples
- Delay mutant UFO: 4 warmup pairs, then 16 measured A/B pairs; 20 pairs and
  40 half samples total
- Delay mutant TFTD: the same 20 pairs and 40 half samples

There are no replacement pairs, retries, adaptive counts, trimming or optional
second runs. The order is normal UFO, normal TFTD, mutant UFO, mutant TFTD.
Warmup chronology is retained privately, and public warmup output contains
counts and AB/BA order counts only. Warmup timings never enter measured
statistics or the sensitivity-control decision.

Normal state thinking, simulation and frame-rate waiting are excluded during
sampling. Real frame composition and successful presentation are retained.
Initialization uses SDL's dummy audio driver; before resource/battle loading the
native fixture closes the mixer, shuts down the SDL audio subsystem and pins
mute. This does not measure audio equivalence. A real focused SDL graphical
window and the fixture's ordinary pointer/focus preconditions are required.
Unsupported focus/display conditions produce incomplete output rather than a
headless substitute. Queued focus/quit events are checked after the spacing
wait and after the final surface copy, outside the timed interval, including
before accepting the last pair. SDL focus state is not a desktop-occlusion or
physical-display visibility proof.

## Build and run

Use lawful, separately installed UFO/TFTD resources and ordinary OpenXcom build
dependencies. Never use an ordinary gameplay user directory for this fixture.
Build a normal executable with the dedicated option, leaving every other
experimental gate and incompatible observer option OFF:

```sh
cmake -S /absolute/normal-source -B /absolute/normal-build \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_CURSOR_OBSERVER=ON -DEXPERIMENTAL_TIMING_CALIBRATION=ON
cmake --build /absolute/normal-build
```

Prepare the delay-control executable from a separate copy/worktree of the same
frozen source revision. Apply the reviewed mutation exactly once, then configure
with the same options in its own build directory:

```sh
python3 tests/battle-observer/make-timing-mutant.py /absolute/separate-mutant-source
cmake -S /absolute/separate-mutant-source -B /absolute/mutant-build \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_CURSOR_OBSERVER=ON -DEXPERIMENTAL_TIMING_CALIBRATION=ON
cmake --build /absolute/mutant-build
python3 tests/battle-observer/run-timing-calibration.py \
  /absolute/normal-build/bin/openxcom /absolute/mutant-build/bin/openxcom \
  /absolute/lawful-data /absolute/fresh-private-output
```

All four wrapper arguments must be absolute. The private output directory must
not already exist, including as a symlink; its parent must exist. The wrapper
creates it with mode 0700 and creates each record/diagnostic file with mode 0600.
Each run uses a fresh disposable user/configuration profile containing only the
selected vanilla master, English and disabled intro. Those temporary profiles
are removed after execution, including ordinary errors and timeouts. The
private output directory remains for review and is never automatically uploaded.
Keep it private; engine diagnostics can contain local paths and private details.

The wrapper supplies `OPENXCOM_TIMING_CALIBRATION=normal` or `mutant`, and
`OPENXCOM_TIMING_RECORD_FD` names the inherited descriptor of that profile's
private regular file. The descriptor uses subprocess `pass_fds`; stdout and
stderr go to separate private files. No raw timing rows use the public stdout
result channel.

The hard process timeout is 120 seconds for each normal profile and 90 seconds
for each mutant profile. One 480-second budget starts immediately before the
first process launch. Before each subsequent launch, including after preparing
its disposable profile, the wrapper recomputes the remaining time and lowers
the process timeout if needed. No new process is launched after the budget is
exhausted. A timeout stops that process, retains existing records, and makes the
profile incomplete. These bounds do not guarantee exact wall-clock termination:
process creation, termination/reaping, filesystem work and report generation
can add overhead. Engine diagnostic output is duration-bounded, not subject to
a strict file-size quota. Parser reads are separately bounded to 128 KiB of
records and 8 MiB of each diagnostic stream; exceeding a parsing bound makes
instrumentation invalid without exposing the contents.

An ordinary incomplete profile does not prevent the next independent profile
within the budget. Malformed records/markers or a recognized shared
instrumentation failure make the current profile incomplete and all remaining
profiles `not-run`. These bounded classifications are not a promise to identify
every possible failure cause. A failed run is never replaced with a later run.

## Equality qualification and private chronology

Every observed half emits one JSONL `sample` record immediately after `t1`,
before any protocol parsing or later oracle. It contains only record type,
sequential sample/pair indexes, kind, scheduled order, A/B label and the three
time offsets. A record can therefore remain even if the response or frame
subsequently proves invalid. A `pair` record appears only after both halves pass
all checks, including exact complete-display RGB equality and exact normalized
full request/response transcript equality. Raw RGB, response bytes, world/unit
state, resources, saves and random transport tokens are not timing records.

The native oracle checks ordinary producer protocol behavior, exact request and
response counts, non-null cursor/HUD payloads, crop/bounds consistency, epoch
continuity and fresh token binding. Normalization is limited to explicitly
validated transport-random fields with a consistent width-preserving bijection.
No timing difference is used to excuse an RGB or transcript mismatch.

The wrapper requires exact record keys and types, the fixed schedule, contiguous
sample indexes and pair markers after exactly two matching halves. It rejects
duplicate JSON keys, non-finite values, extra records, bad framing, zero or
non-increasing timestamps, less than 300 ms between a previous `t1` and the next
`t0`, and offsets beyond the profile cap. A complete,
unterminated half/pair is valid partial data but contributes no pair statistics.
A torn JSON line is malformed. Previously validated marker-qualified pairs may
still be summarized when a later record is malformed; the profile remains
incomplete and subsequent profiles are not run. The raw bytes remain untouched.

A successful native process prints exactly one
`TIMING_CALIBRATION COMPLETE` marker. Ordinary engine diagnostics may surround
it privately. A native assertion uses one fixed
`TIMING_CALIBRATION INCOMPLETE <bounded-label>` stderr marker. Duplicate,
contradictory, wrong-channel or malformed markers are rejected. A successful
exit alone, missing records, or a completion marker on a failing exit cannot
establish completion.

## Public aggregate and sensitivity control

The wrapper prints one strict JSON document with schema `timing-calibration/1`.
It contains the four fixed profile statuses, bounded reason categories, planned
and completed counts, observed/unpaired counts, and AB/BA counts by kind. Counts
refer to the validated record prefix, not rejected raw tail bytes. For an ordinary
valid partial trace, `remaining_not_run_samples` explicitly reports planned
samples without an observed record. For invalid instrumentation that count is
null, and `unvalidated_or_unrun_samples` gives the planned count minus validated
samples without falsely claiming that a rejected tail was never executed. A
not-run profile has its full expected count remaining. For
each measured A/A and A/B group it reports A-minus-B differences in nanoseconds
for both metrics: count, mean, median, minimum, maximum and negative/zero/positive
counts. The sign always refers to the scenario label, regardless of execution
order. Empty groups use count zero and null numeric summaries. No individual
samples, chronology, host paths, engine diagnostics or raw state appear in this
public document. The private files, not the aggregate, preserve chronology for
review.

The source mutation adds a nominal `SDL_Delay(5)` request directly before the
real native response send when the actual current hover tile has a unit and
that unit is not visible. It reads the selector, tile and unit, not a scheduled
trial label. The delay belongs only in the separately prepared mutant source;
the normal production observer source is unchanged. The source-preparation
script checks the exact expected source hash and anchors and rejects a different
or already-mutated file. A requested 5 ms delay is not an assurance of an exact
observed 5 ms difference under scheduling and rendering noise.

For each mutant master, `detectable_control` is true only when the profile is
complete, all 16 measured A/B pairs have equality-qualified pair markers, and
both metrics separately satisfy:

- At least 14 of 16 A-minus-B differences are positive
- Median A-minus-B difference is at least 3,000,000 ns (3 ms)

Otherwise its control result is `inconclusive`. This fixed threshold is a
sensitivity-control rule, not a significance test or evidence of timing equality
in the normal binary. There is no confidentiality PASS. Exit code zero means all
four planned profiles completed the record/equality contract, even if a mutant
control remains inconclusive; it is not a successful timing-equivalence verdict.
Incomplete runs return nonzero; invalid arguments or an unusable private output
directory return 2 without launching profiles.

## First bounded runtime results

The [exact aggregate and frozen implementation/binary hashes](timing-calibration-evidence.json)
record the first and only attempt of each profile on 2026-10-11. All four
profiles completed their fixed plans and the wrapper exited zero: 304 observed
half samples, including 48 warmup and 256 measured samples, formed 152 pairs
that passed the full RGB and normalized packet equality checks. No samples or
profiles remained unrun or unpaired. There were no retries, replacements,
trimming or sample increases. Each normal master completed 8 warmup, 16 A/A and
32 A/B pairs; each mutant master completed 4 warmup and 16 A/B pairs. The source
and tests stayed frozen throughout the run. Raw chronology and engine logs are
not included in the public evidence.

Normal-binary A/B median occupied-minus-empty differences were:

- UFO: native harness round-trip −0.448138 ms; post-flip +0.0315695 ms
- TFTD: native harness round-trip −0.0401025 ms; post-flip +0.036584 ms

These are descriptive measurements, not timing-equality or absence-of-leak
findings. The linked aggregate retains all paired means, medians, extrema, sign
counts, A/A results and realized order counts.

The deliberately delayed mutant had different sensitivity outcomes:

- UFO: inconclusive under the fixed two-metric rule. The round-trip median was
  +4.8181875 ms, but only 13 of 16 differences were positive, below the required
  14. Its minimum difference was −287.572706 ms and mean −12.7005265625 ms;
  that observation was retained. The post-flip metric independently had 16 of
  16 positive differences and a +5.119208 ms median. Its result cannot replace
  the missed round-trip criterion.
- TFTD: detectable control under the predeclared rule. The round-trip metric
  had 14 of 16 positive differences and a +7.235343 ms median; the post-flip
  metric had 16 of 16 positive differences and a +5.2240165 ms median. All 16
  measured pairs retained equal final RGB and normalized packet bytes.

Practical implication: this exact native calibration detected the deliberate
5 ms control in TFTD, while the UFO result remains inconclusive. The large
retained UFO round-trip difference limits what can be concluded from that
metric in this run; the measurements do not establish its cause. Do not pool
masters, substitute the post-flip result, rerun UFO, or remove the extreme
observation to change the decision. Neither control result certifies the normal
binary's confidentiality or closes the broader timing/full-battle gate.

## Verification and limits

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tests/battle-observer/test_timing_calibration_wrapper.py
PYTHONDONTWRITEBYTECODE=1 python3 tests/battle-observer/test_timing_structure.py
```

Wrapper tests use synthetic JSONL and fake subprocess results to exercise schema
validation, chronology, fixed counts/order, statistics, outliers, control
thresholds, private-file permissions, cleanup, diagnostics filtering and budgets.
They neither render the game nor establish runtime sensitivity. Structural
checks and compilation are likewise separate from graphical measurements.

Any actual report should preserve the frozen source revision/hashes, build
options, normal and mutant binary hashes, aggregate output, attempted profile
counts and incomplete/not-run outcomes. Do not publish the private raw files.
Do not rerun or discard unfavorable samples to obtain a preferred result. A
native-source change requires a separately identified new calibration, never a
silent replacement for the existing one.

This fixture makes no claim about natural FOV history, all hidden state,
simulation, movement, arbitrary admission points, all animation phases, audio,
other operating systems/displays/scales, production workloads, attacker
observations or future commands. The result is descriptive evidence for one
fixed same-process/same-thread return path and one deliberate delay control.
