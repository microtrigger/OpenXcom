<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- New contribution maintained in this OpenXcom fork, 2026-10-10. -->
<!-- Free software under GNU GPL version 3 or later; without warranty. -->
<!-- See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Experimental native mixed-audio observer

This local-only prototype observes the final **application SDL_mixer output**.
It does not establish what reached a physical device or what a person heard.
A returned clip is a bounded observation made after admission, not a claim about
sound at response time. It has no frame, battle, simulation-tick, or HUD pairing.

## Build and profile

The independent `EXPERIMENTAL_MIXED_AUDIO_OBSERVER` option defaults to `OFF`.
With it off, the game has no mixed-audio object, postmix hook, resource capability
calls, or observer polling. It does not enable the existing pixel/HUD channel.

Using the normal OpenXcom development dependencies:

```sh
cmake -S . -B build-audio -DBUILD_PACKAGE=OFF \
  -DEXPERIMENTAL_MIXED_AUDIO_OBSERVER=ON
cmake --build build-audio -j
./tests/mixed-audio-native/run.sh
```

The test runner also accepts `SDL_PREFIX` for a non-system dependency prefix and
`CXX`, `CXXFLAGS`, or `SDL_CONFIG`. Its dummy-mixer oracle currently uses ELF linker
wrapping; that test harness is Linux-oriented even though the observer is POSIX.
No game data, graphics, window, external model service, or network listener is
needed for the synthetic tests. Building does not launch the game. The normal
runtime still needs lawfully supplied game resources kept outside source control.

Readiness is deliberately conservative:

- The linked SDL_mixer version must be exactly 1.2.12.
- `Mix_QuerySpec` must report 22050 Hz, `AUDIO_S16LSB`, and two channels.
- Every loaded music object must qualify as software-mixed. The explicit accepted
  SDL_mixer types are WAV, MOD, MODPLUG, OGG, MP3, MP3_MAD, and FLAC. The existing
  Adlib custom mixer-hook implementation qualifies separately. Empty catalogs
  qualify, but an unopened or mismatched mixer still does not.
- Native MIDI, external-command music, empty base Music objects, and unknown types
  are rejected. All MIDI is conservatively rejected, including software MIDI.
  The requested music preference alone is insufficient because loading falls back
  to other available formats.

Catalog inspection runs on the resource-loading thread after the entire load.
Only a release/acquire atomic aggregate is published to the main thread. No
active-track query, resource name, actor identifier, coordinate, or music-selection
state is used to admit individual requests. Unsupported configurations expose
only channel unavailability. The observer never reconfigures rate, channel count,
format, playback, effect hooks, volume, or game options to meet this profile.

## Capture ownership

One preallocated slot contains 44100 bytes: exactly 11025 stereo frames at four
bytes per frame, representing 500 ms of nominal application-output sample time.
No pre-request or rolling history is stored.

1. Main thread owns IDLE, validates the request context, and release-stores ARMED.
2. The audio callback acquire-loads ARMED, resets its private offset, release-stores
   CAPTURING, and returns **without copying any of that callback block**. That block
   may have begun mixing before admission.
3. Later callbacks copy complete consecutive stereo frames until exactly 11025
   frames have been collected. The unused tail of the final block is discarded.
4. Callback release-stores READY and never touches the PCM while READY.
5. Main thread acquire-observes READY, serializes and sends the result, then
   release-stores IDLE only after a successful complete packet send.

Callback work is limited to proven-lock-free ownership atomics, bounded checks,
and at most 44100 bytes of `memcpy` across a capture. Callback inputs must be
non-null, positive, four-byte aligned in length, and no larger than 262144 bytes.
Invalid input becomes a terminal failure observed by the main thread. There is
no callback allocation, logging, JSON/hex encoding, RNG, socket I/O, clock, SDL
call, lock, wait, or game-state access. The input buffer is not changed.

The callback is attached after static qualification and before the ready hello.
While enabled but idle it performs only the ownership check; it does not copy
PCM. Serialization is main-thread work. There is no resampling, normalization,
downmixing, source reconstruction, zero suppression, or silence generation.
`Options::mute` is deliberately not read: the upstream flag gates future
playback, and an already-playing channel can remain audible in the mix.

SDL_mixer 1.2.12 runs music, channels and effects before its final postmix callback.
Its `Mix_SetPostMix` setter takes the audio lock internally. Thus the main-thread
`Mix_SetPostMix(NULL, NULL)` call synchronizes with an in-progress callback before
slot invalidation or destruction; adding an outer audio lock would deadlock.
See the pinned upstream [mixer implementation](https://github.com/libsdl-org/SDL_mixer/blob/release-1.2.12/mixer.c)
and [music decoder dispatch](https://github.com/libsdl-org/SDL_mixer/blob/release-1.2.12/music.c).
These facts are specific to the qualified library version, not a universal mixer
backend guarantee.

## Startup, reset, and wire contract

`OPENXCOM_MIXED_AUDIO_FD` identifies a separate inherited, connected, unnamed
`AF_UNIX/SOCK_SEQPACKET` socketpair endpoint. There is no path discovery, bind,
listen, reconnect, or service registration. Both local and peer addresses must
be anonymous. A valid descriptor is duplicated with close-on-exec, the inherited
original is closed, and nonblocking operation is enabled.

The constructor owns the socket but neither attaches a hook nor sends a hello.
BOOTSTRAP has an absolute 30-second allowance from native construction. Ordinary
startup mixer reopens before hello preserve bootstrap; a resource reload clears
catalog readiness. Neither extends the startup deadline. Qualified readiness
attaches the hook and sends one 87-byte packet:

```text
OPENXCOM-MIXED-AUDIO/1 <32 lowercase hex epoch><32 lowercase hex admission>
```

No client packet is valid before this hello. The adapter has a separate 31-second
hello allowance, then a strict two-second result acceptance deadline for each
capture. Native capture timeout is two seconds from main-thread admission; it
also checks again after serialization and before sending. These are availability
bounds, not sample timestamps.

A request is exactly 96 lowercase hexadecimal bytes: the current 32-character
epoch, current single-use 32-character admission token, and a client-generated
32-character challenge. At most one request may be in flight. Admissions must
be at least one second apart. Multiple queued packets, consumed/stale admission
tokens, wrong epoch, truncation, oversize, non-hex data, disconnect, or deadlines
close the channel. A fresh admission token is generated on each admitted request.
Tokens come from the OS randomness source, independently of game RNG.

The one native reply has exactly these fields:

```json
{"schema":"experimental-mixed-audio/1","epoch":"<32hex>","challenge":"<32hex>","next_token":"<32hex>","semantics":"post_admission_mixed_output","profile":"sdl-mixer-postmix-s16le-stereo-22050","duration_ms":500,"sample_rate":22050,"channels":2,"sample_format":"s16le","frames":11025,"pcm_hex":"<88200 lowercase hex characters>"}
```

For this fixed schema the compact native JSON is 88571 bytes, below the 102400-byte
packet cap. The PCM is 44100 bytes. Hex is an internal prototype transport encoding
and is **not compact**. The adapter wraps identical PCM in a 44144-byte WAV, which
has 58860 base64 characters before the MCP envelope. There are no timestamps,
callback counts, device identifiers, source labels, or semantic event summaries.
See [the adapter contract](../../tools/mcp-observer/AUDIO-ADAPTER.md).

Every reset after hello permanently closes this epoch, even before its first
request. After admission, timeout/disconnect/reset is never handled by canceling
and reusing a callback-owned slot: detach first, then invalidate permanently.
The lifecycle guards run before resource reload in `StartState::init`, before
`Game::initAudio` reopens, before both cutscene audio reopen/close paths, and at the
start of `Game` destruction. Existing music hooks remain unchanged. Polling lives
in the outer game loop and also runs while PAUSED; it does not force state think,
render, input, or volume changes. Blocking work elsewhere in that loop can make
an otherwise valid capture unavailable rather than returning stale data. An intro
that reopens audio after hello also closes the channel; a menu-only validation
profile can use the game's ordinary `playIntro: false` setting.

## Verification and limits

The checked-in native suite covers:

- 200 exact varied-chunk captures, skipped admission block, complete-frame limit,
  no tail or pre-request reuse, unchanged callback source, genuine zero PCM,
  malformed callback lengths, and 3000 threaded producer/consumer handoffs.
- Exact hello/result dimensions and generic fail-closed behavior for startup,
  profile/type rejection, malformed packets, replay, duplicate/queued requests,
  rate limiting, capture deadline, disconnect, reset, small-buffer send failure,
  and allocation failures.
- A real SDL dummy mixer with an independent final-buffer oracle, overlapping
  panned channels, a synthetic custom music hook, and a post-effect. The oracle
  compares every captured byte and verifies the observer leaves output unchanged.
- Failed channel allocation and pan-only changes, plus real production `Sound`
  behavior with zero volume, mute before playback, and
  mute while a channel is already playing. Different source objects/channels with
  identical final PCM yield identical PCM; synthetic transport payload comparison
  normalizes only the independent epoch/admission/challenge tokens.
- Positive controls detect byte corruption, swapping stereo channels, including
  the skipped admission block, and substituting pre-effect/source PCM.
- Source boundary tripwires, which complement tests but are not a formal proof.

ASan/UBSan slot, protocol, and dummy-mixer runs passed with leak detection disabled.
LeakSanitizer was not validated. The 3000-
handoff slot stress also passed TSan. Those checks do not instrument the entire
game or every mixer backend. Full enabled and default-OFF builds are separate
checks; actual game-data audio and GUI smoke tests are separate from this suite.

`bench_slot.cpp` provides a bounded cached-memory microbenchmark, compiled with
optimization and assertions enabled. A five-run development sample measured
0.334–0.521 ns per inlined enabled-idle slot call, with two million calls and no PCM
copy. SDL callback dispatch and scheduling are outside this measurement.
The active-loop measurement was 133.2–185.5 ns per callback including its main-side
arm/sent/inspection driver: 20000 captures, 882000000 bytes total, and 23 calls per
capture (one skipped, 22 copied; 2048-byte block, 1092-byte final partial block).
These compiler/host-specific hot-cache numbers are not real game CPU/RSS, audio
continuity, end-to-end latency, bandwidth savings, or a real-time guarantee.
Hardware/device output, every accepted codec implementation, real Adlib playback,
cutscene GUI transitions, and full resource-loading races are not established by
these synthetic tests. No native MIDI completeness claim is made.


## Actual-game smoke evidence

[mixed-audio-game-evidence.json](mixed-audio-game-evidence.json) records one menu
run for each of UFO and TFTD under enabled and zero-volume profiles. Each run
returned two new 500 ms windows through the real stdio adapter: four enabled clips
contained stereo PCM, and four zero-volume clips preserved exact silence. Each
closed-producer request returned generic unavailable. Effective saved options were
checked; enabled runs used music/sound/UI volumes 84/84/42, and zero runs used
0/0/0. A prior invalid zero-profile attempt had misplaced YAML settings and used
defaults; it is excluded and disclosed in the evidence.

These tests used the initialized SDL dummy backend and ordinary menu playback
with intro disabled. They establish bounded application-mixer integration, not
speaker output, battlefield audio completeness or an independent full-buffer
comparison for the game clips. The byte-for-byte final-buffer oracle belongs to
the separate synthetic mixer tests above. No game audio or resource payload is
included in this repository.
