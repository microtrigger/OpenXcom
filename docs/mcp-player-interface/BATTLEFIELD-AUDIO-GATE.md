<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- New contribution maintained in this OpenXcom fork, 2026-10-10. -->
<!-- Free software under GNU GPL version 3 or later; without warranty. -->
<!-- See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Bounded normal battlefield-audio gate

This separate, default-OFF test exercises the native mixed-audio observer during
a witnessed normal grenade event. It covers visible and outside-current-player-
team-FOV event origins, with an idle-observer/requested-observer pair for each
orientation. A passing run establishes exact equality to an independent copy of
the same application mixer output and semantic game-state/engine-RNG equality
within each pair. It does not establish physical audibility, isolate a grenade
waveform from the mix, or close Stage 5. The controller remains disabled.

## Build and run

With the project's ordinary dependencies available:

```sh
cmake -S . -B build-battlefield-audio -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_MIXED_AUDIO_OBSERVER=ON \
  -DEXPERIMENTAL_BATTLEFIELD_AUDIO_GATE=ON
cmake --build build-battlefield-audio
python3 tests/battle-observer/check_audio_boundary.py
python3 tests/battle-observer/run-audio.py /absolute/path/openxcom /absolute/path/data xcom1
python3 tests/battle-observer/run-audio.py /absolute/path/openxcom /absolute/path/data xcom2
```

Supply the built executable and separately obtained lawful game resources. The
gate needs a focused graphical SDL display and an initialized SDL dummy audio
device. The ordinary native audio qualification still applies: SDL_mixer
1.2.12, 22050 Hz, signed little-endian 16-bit stereo, and a supported fully
software-mixed music catalog. The fixture does not bypass that qualification.
The ELF linker wrapper makes this test Linux-oriented.

The runner uses a fresh disposable profile, loads no campaign saves, and leaves
sound/music/UI option levels at 84/84/42. These are configured option values;
normal Game::setVolume applies its ordinary exponential conversion. The YAML
option values are nested under options; the active vanilla master list remains
top-level. Music stays enabled and ordinary battle-music selection is preserved.
No request changes mute, channel count, volume, rate, music, or game options.
Only fresh-world setup halts leftover effect channels from the preceding world;
music is not halted, and measured event playback is never altered by the fixture.
The runner's process bound is 90 seconds, including resource loading.

Only fixed aggregate results or fixed source-assertion labels are printed.
Assets, binaries, packets, PCM, runtime state, tokens, private paths, and hashes
of private game output are not published. Documentation of a test protocol is
not runtime evidence: build, normal-run, and mutation results must be recorded
separately before any claim that they passed.

## Four worlds per master

The setup reuses the small land-only 40×40×1 battle with native patched BLANKS
scorched-earth floor, a real Base/Soldier at (20,20,0), and a healthy unarmed
Sectoid/Aquatoid at (2,2,0). A native grenade starts on the ground at (20,10,0)
with fuse zero: STR_GRENADE in UFO, STR_MAGNA_BLAST_GRENADE in TFTD. Both native
rules have power 50 and blast radius 5. Setup validates the rules and keeps both
units outside the blast radius. There are no additional player units.

| Pair | Soldier setup direction | Event origin | First world | Second world |
|---|---:|---|---|---|
| 1 | 0, north | Inside current player-team terrain FOV | Observer idle | One native request |
| 2 | 4, south | Outside current player-team terrain FOV | Observer idle | One native request |

Natural occupancy, lighting, and FOV initialization determine visibility. The
fixture does not set visibility/discovery flags. Position and direction remain
unchanged until the event. Tile::getVisible is checked at the event; the hidden
origin starts undiscovered. In this fresh, unchanged-facing, single-player-unit
fixture the terrain visibility result describes the whole player team. It is
not merely the selected soldier's view or whether the tile is on screen.
BattleUnit::getVisibleTiles is not used as a list of all visible terrain: in
this engine it records visible enemy-occupied tiles. No general claim about
terrain visibility after arbitrary earlier movement or FOV changes is made.

The only gameplay action is an ordinary SDL key-down/key-up for
Options::keyBattleEndTurn. Game event dispatch invokes btnEndTurnClick,
requestEndTurn, the ordinary end-turn queue, and the hot-ground-grenade path in
BattlescapeGame::endTurn. That path creates and initializes ExplosionBState,
which plays the normal small-explosion sound without an FOV gate. The fixture
does not construct the explosion, call its handlers, or replay a source chunk.

Normal animation and simulation timers finish the explosion and side change.
The ordinary NextTurnState screen then awaits input because the regular
skipNextTurnScreen option is false. No further key-down is sent, so hostile AI
does not run. This is a natural input-wait checkpoint; no timer, RNG, game loop,
AI, animation, or frame scheduling is frozen to manufacture it. Ordinary blast
flash and camera recentering are preserved. An outside-FOV origin can still
produce legitimate visual clues in vanilla rendering.

## Independent same-output PCM oracle

The test-only Mix_SetPostMix linker wrapper interposes callback registration.
It installs a transparent trampoline which copies the incoming final mix,
invokes the exact production callback/context once per callback, and checks
that the production call did not alter the output buffer. The production
callback implementation is exercised unchanged; the registered pointer itself
is interposed. There is no production observer hook for supplying test PCM.

The private oracle has its own preallocated capture storage and ownership
state. It reads neither the native slot nor its private state or offset. After
a qualified hello, a narrow test-only audio-lock barrier aligns independent
oracle arming with the one ordinary native admission poll. The first callback
is discarded by both, then the oracle copies the next 11025 consecutive stereo
frames, exactly 44100 bytes. Attaching or detaching a postmix callback while
that barrier is held fails the test before invoking the real locking setter,
rather than attempting a recursive SDL audio lock. This controlled test
scheduling is not a change to production admission behavior.

Both idle and requested worlds use equivalent oracle/barrier scheduling. End
Turn is queued only after the independent oracle has observed its discarded
callback, allowing the normal event to occur inside the 500 ms capture window.
The successful ordinary playback of the expected native small-explosion Sound
on an allocated mixer channel is witnessed while the oracle is collecting.
That witness and its private event/FOV checks are test data, not observer fields.

Each requested result is parsed using the exact existing twelve-field native
mixed-audio schema, challenge, epoch, and fresh admission-token checks. Every
decoded PCM byte must match the independent reference for that same capture.
The native response remains only bounded mixed output; it contains no actor,
tile, event name, source identifier, FOV, callback count, or controller data.
The normal music path is retained: nonzero PCM alone cannot establish that a
grenade was audible. The narrower result is normal mixed output during a witnessed,
successfully scheduled grenade event. PCM need not match between worlds.

## Read-only effects comparison

At setup and at the ordinary next-turn input-wait checkpoint, each requested
world is compared with its same-orientation idle twin. The comparison uses the
whole semantic SavedBattleGame YAML structure and additional runtime fields,
not emitted YAML text, pointer values, raw memory, timing, or callback counts.
Item/tile relationships, grenade removal, units, tiles, animation offsets,
fire/smoke, current action, and settled runtime conditions are relevant to this
fixture. The action must finish normally, both units remain alive, and no AI
module may have executed.

RNG::getSeed is read without drawing a random number. Snapshotting must itself
leave the seed unchanged; the paired checkpoints must agree. Seeds are set
only during fresh-world setup, never between a world's checkpoints. The gate
does not claim to observe or prove libc RNG state. Wall-clock pacing, allocation
effects, every intermediate state, and audio callback alignment across separate
worlds are outside this equality claim.

## Controls, boundaries, and limits

Required mutation controls should be run against isolated mutant builds:

1. Corrupt a serialized PCM byte. Exact independent PCM comparison must fail.
2. Consume an extra engine RNG draw on an admitted request. The paired seed or
   semantic-state comparison must fail.
3. Omit or prevent successful allocation of the intended ordinary grenade cue.
   Its playback witness must fail even if normal music makes the mix nonzero.

A changed-buffer control can additionally test the trampoline's before/after
read-only check. An early-window control must first demonstrate sensitivity to
the actual waveform; equal silence or repeating output can conceal a shift.
Centered grenade audio may have equal stereo channels, so this fixture alone
does not promise stereo-swap sensitivity. The synthetic native mixer suite
separately covers asymmetric stereo, source substitution, and admission shifts.

Structural checks require Python assertions and reject optimized Python. They
check default-OFF isolation, normal dispatch and polling seams, source boundaries,
and the absence of direct fixture gameplay/audio injection. They supplement
runtime evidence and code review, not a formal proof of noninterference. Default-
OFF builds must separately show that fixture symbols and activation strings are
absent; the native observer/adapter suites remain required regression checks.

This bounded land-only slice does not validate hostile AI, arbitrary battle
actions, all hidden information, broad mixer backends, device output, what a
person heard, frame/audio synchronization, timing equality, or repeated-run
robustness. It adds no controller and leaves Stage 5 open.

## Recorded validation: 2026-10-10

[Aggregate evidence and source hashes](battlefield-audio-evidence.json) record
one successful normal run per vanilla master, UFO and TFTD. Each run completed
four worlds and two requested windows with the same-buffer PCM oracle, successful
cue witness and paired semantic-state/engine-RNG checks. Both normal runs passed
on their first attempt.

All three isolated source mutations were rejected in both games at their intended
checks: altered PCM at assertion 553, an extra request-path engine RNG draw at
assertion 124, and omitted grenade playback at assertion 320. These six rejections
were assertion failures, not crashes or timeouts. The enabled and default-OFF
builds, native audio regressions, 190 Python adapter tests, structural checks and
runner mock cases passed. The default-OFF binary has no gate/audio-observer or
wrapper symbols. No hosted CI result is implied by these checks.

This is one bounded event and land-only fixture per orientation/master, with
normal music and the initialized dummy backend. The qualifications and exclusions
above still apply; Stage 5 remains open.
