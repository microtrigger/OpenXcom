<!-- SPDX-License-Identifier: GPL-3.0-or-later. New contribution 2026-10-10.
See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Local fixed-window mixed-audio MCP adapter

This optional prototype exposes one request-bound slice of application SDL_mixer
postmix output. It does not measure physical audibility, instantaneous current
game state, or synchronization with a presentation/HUD observation. No source,
actor, position, track, simulation counter, callback count or timestamp is
exported. It records no continuous history; sounds between requests can be missed.

The existing historical and presentation version 1/version 2 protocols and public
schemas are unchanged. Audio has an independent socket, epoch and admission state.
There is no joined audiovisual result, shared clock or synchronization claim.
The small stdio server processes tool calls synchronously, so a pending audio call
can delay processing a later visual call despite their separate stream identities.

## Explicit local launch

Build the native producer with the optional mixed-audio feature enabled; see the
native audio documentation for build gates and qualification. The Python server
and launcher themselves use only the standard library, Python 3.10+ on supported
POSIX systems with AF_UNIX SOCK_SEQPACKET. No install or registration is required.

From this directory, add audio to the existing presentation launcher:

```
python launch_presentation.py --mixed-audio -- /path/to/openxcom [game arguments]
python launch_presentation.py --mixed-audio --presented-hud -- /path/to/openxcom [game arguments]
```

For audio without a presentation channel:

```
python launch_presentation.py --audio-only -- /path/to/openxcom [game arguments]
```

The default launcher invocation still enables presentation version 1 only.
`--presented-hud` still explicitly selects version 2. `--audio-only` implies audio
and rejects `--presented-hud`. Optional historical `--hud-dir` and `--cursor-dir`
arguments remain available. Game arguments and these paths are trusted operator
configuration, never MCP tool arguments. No MCP tool can launch a game or select
an executable, device, output profile, volume, duration, path or source.

The launcher scrubs ambient observation descriptors/version selection, creates
anonymous connected socketpairs for exactly the explicitly enabled streams, and
passes only those descriptors to the child. There is no socket pathname, listener,
port, registration, credential or network service. Both child stdout and stderr
are discarded at process creation with `subprocess.DEVNULL`, including the engine
logger fallback, startup and shutdown. There are no captured diagnostic pipes or
buffers. Observation EOF closes the channels without terminating the game.
Display/audio settings and game arguments are not changed. This boundary assumes
cooperative trusted local processes and the matching native build; it does not
defend against a modified game or malicious same-user process. Raw engine log
files are not observation inputs.

The shared [diagnostic qualification](DIAGNOSTIC-ISOLATION.md) requires effective
`debug`, `debugUi`, `traceAI` and `verboseLogging` to be false and the logger level
to be `LOG_FATAL` through `LOG_INFO`, inclusive, after configuration parsing and
logger setup. Initialization must precede game construction and observer hello.
A main-thread recheck rejects newly unqualified settings before admission or
publication, including a completed PCM window. Once rejected, qualification is
lost for that process; restoring settings does not reopen observation. Retirement
occurs at each channel's next observer boundary, not an instantaneous global tick.
The audio callback does not read the options, and settings are never rewritten
to qualify. Existing format, catalog and lifecycle gates remain in force.

## Tool result

`read_mixed_audio({})` takes no arguments. It is advertised only when an audio
channel was explicitly launched. A success contains:

1. One text content block with compact JSON metadata: schema
   `openxcom-mcp-mixed-audio/1`, epoch, challenge, semantics
   `post_admission_mixed_output`, profile
   `sdl-mixer-postmix-s16le-stereo-22050`, duration_ms 500, sample_rate 22050,
   channels 2, sample_format `s16le`, and frames 11025.
2. One native MCP content block with type `audio`, mimeType `audio/wav` and
   base64-encoded canonical WAV bytes.

The metadata has no PCM hex or next admission token. The audio is exactly
11,025 stereo frames, 22,050 Hz, signed 16-bit little-endian PCM, 44,100 bytes.
Channel order and every sample bit are preserved. The WAV is exactly a 44-byte
RIFF/WAVE PCM header plus those bytes, with matching RIFF and data lengths.
There are no optional chunks, timestamps, labels or comments. The adapter does
not resample, normalize, downmix, filter, classify, trim, synthesize silence or
interpret source IDs. An all-zero clip is a valid full-size clip.

The native collector admits on an audio callback and discards that callback's
block. The window consists of the following 11,025 postmix frames. It neither
requests gameplay/input nor causes the adapter to read a hidden source channel.
This is a post-admission output window, not a promise that its sounds reached the
user's speakers. Qualification of the loaded music catalog is a native aggregate
property; Python performs no catalog inspection or per-track query. Unsupported
music backends, audio profiles and all stream failures return the same generic
unavailable result, with no source-specific error details.

## Private transport contract

The inherited variable is `OPENXCOM_MIXED_AUDIO_FD`. Both ends require a connected
anonymous AF_UNIX SOCK_SEQPACKET socket. Messages are atomic packets, never an
unbounded byte stream.

- Hello: exactly `OPENXCOM-MIXED-AUDIO/1 ` followed by a 32-character lowercase
  hexadecimal epoch and a 32-character lowercase hexadecimal admission token.
  Total: 87 bytes. Native sends it only after the effective diagnostic profile,
  aggregate resource readiness and exact actual mix format are qualified.
- Request: exactly 96 lowercase hexadecimal ASCII bytes: epoch, current
  admission token, and a new OS-random 128-bit challenge, each 32 characters.
- Reply: one UTF-8 JSON object with exactly `schema`, `epoch`, `challenge`,
  `next_token`, `semantics`, `profile`, `duration_ms`, `sample_rate`, `channels`,
  `sample_format`, `frames`, and `pcm_hex`. Schema is
  `experimental-mixed-audio/1`; other fixed values match the metadata above.
  `pcm_hex` is exactly 88,200 lowercase hexadecimal characters. The returned
  next token is private transport state and must differ from the current token.

Native JSON uses hex for a simple bounded prototype transport. Hex is not compact:
its 88,200 characters are twice the PCM byte count. The full native packet limit is
102,400 bytes; the current compact fixed envelope is 88,571 bytes. The initial
native representation is not a compressed-audio or resource-savings claim.

Python rejects unknown/missing/duplicate keys, nonfinite values, bools/floats where
fixed integers are required, unknown profiles/versions, wrong sample counts,
noncanonical hex, malformed/oversized/truncated packets, ancillary data, stale or
unsolicited packets, and mismatched epochs/challenges/tokens. It accepts no old
reply as a new window. There is one request in flight; a concurrent or too-fast
request fails closed. A new random challenge binds every accepted result and the
admission token rotates. Random epochs describe audio-stream lifetimes, never
battle state, simulation time or visual-stream identity.

## Deadlines, startup and terminal failure

Native BOOTSTRAP is bounded to 30 seconds from construction. Ordinary startup
resource reloads before the first hello can occur without invalidating a never-used
channel, but do not extend that absolute bound. After hello, an audio/catalog
reset closes that channel permanently. There is no automatic retry or reconnect.

The adapter allows up to 31 seconds for the first hello, separately from its
strict two-second acceptance deadline starting at the capture request. A slow
first normal asset load does not spend the two-second capture budget. A client
using real game startup should allow at least 35 seconds for the first tool call;
the SDK smoke uses a shorter bound only because its peer is synthetic.

The collector's window itself is fixed at 500 ms of samples. No claim is made that
wall-clock call latency is exactly 500 ms. The native minimum is one second between
admissions. Python conservatively requires one full second after each successful
result before another request, which also satisfies that native bound.

Timeout, peer EOF, malformed traffic, reset, overload or rate violation permanently
closes only the audio channel and returns one text-only error:

```
{"available":false,"reason":"observation_unavailable"}
```

Late data and cached audio are never substituted. Relaunch explicitly to create a
new channel. Ordinary audio transport failures leave historical/presentation
channels independent; failure does not relabel another stream's result or claim a
synchronized fallback. Diagnostic-profile rejection is process-wide and each
observer retires at its next qualification check. Historical files are retained
with their old, unverified historical meaning; their writers release the locks.

## MCP compatibility and verification

MCP wire target remains 2025-11-25. The response uses the specification's native
AudioContent shape. Official sources consulted 2026-10-10:

- https://modelcontextprotocol.io/specification/2025-11-25/server/tools
- https://modelcontextprotocol.io/specification/2025-11-25/basic/transports
- https://github.com/modelcontextprotocol/python-sdk

The official SDK's current documentation describes v2 APIs. The optional smoke
was actually checked against official Python SDK 1.29.0,
which negotiated protocol 2025-11-25. No v2 compatibility is inferred. The server
has no SDK dependency; SDK source is not vendored. The optional official SDK is
MIT-licensed separately from these GPL-3.0-or-later additions.

Run from this directory:

```
python -m unittest -v
python sdk_smoke.py
python sdk_presented_hud_smoke.py
python sdk_mixed_audio_smoke.py
python measure_mixed_audio.py
```

Tests use synthetic bytes and anonymous local peers. They cover exact signed PCM
and stereo channel order, unchanged all-zero clips, canonical WAV lengths/header,
deterministic PCM-to-WAV/base64 output, strict fields/values, request binding,
rotation/replay/EOF/deadline/rate rejection, concurrent closure, no-argument tool
validation, audio-only and combined launch modes, and preserved historical and
presentation behavior. The SDK smoke checks actual AudioContent parsing through
the real stdio launcher in audio-only, version 1 combined and version 2 combined
modes. It verifies distinct visual/audio epochs and continued visual reads after
audio's terminal rate rejection. There are no API or model calls.

`mixed-audio-evidence.json` records separately: 44,100 PCM bytes, 88,200 native hex
characters, 88,571 native JSON packet bytes, 44,144 WAV bytes, 58,860 base64
characters, and 59,342 MCP JSON-RPC line bytes including newline for integer id 1.
The existing 131,072-byte response bound accommodates this result without change.
Other request-ID lengths change the line count. These are bytes/characters, never
model tokens or costs.

The evidence's 1,000-iteration microbenchmark measures only in-memory JSON
validation, WAV/base64 encoding and MCP serialization for synthetic zero PCM.
It reports wall time, process CPU time and a separate Python traced-allocation
peak; that peak is not process RSS. It does not measure game capture latency,
socket/device I/O, RSS, model costs, end-to-end savings or timing parity. It is not
a noninterference proof. Real-game/backend tests and broader platform validation
remain separate from these adapter checks.

Actual UFO/TFTD menu integration and volume-zero controls are recorded separately
in [game evidence](../../docs/mcp-player-interface/mixed-audio-game-evidence.json).
Those earlier smoke checks do not replace the synthetic independent final-buffer
oracle. Separate [diagnostic-isolation menu checks](../../docs/mcp-player-interface/diagnostic-isolation-evidence.json)
now obtain a 44,144-byte WAV alongside cursor PNG and null HUD in each final normal
UFO/TFTD run. UFO `traceAI=true` and TFTD `debug=true` reject all three tools with
generic unavailable while game menus remain available; all four final runs have
zero launcher stderr bytes. Earlier failed UFO attempts and their unestablished
cause are retained in the [results and limits](DIAGNOSTIC-ISOLATION.md#verified-results-and-limits).
These bounded menu checks add no battle or full-fairness claim; Stage 5, full audio
parity and timing parity remain open.
