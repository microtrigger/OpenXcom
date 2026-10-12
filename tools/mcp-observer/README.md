<!-- New project documentation: SPDX-License-Identifier: GPL-3.0-or-later. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Local OpenXcom observation and supervised stance MCP prototype

The default tools are read-only observations. A separately opted-in
[supervised stance pilot](../../docs/mcp-player-interface/SUPERVISED-STANCE-PILOT.md)
adds `read_stance_context`, `request_stance` and `read_stance_status` through
`launch_presentation.py --supervised-stance -- /absolute/game ...`. The game must
be built with the default-OFF stance option. An MCP proposal cannot approve
itself: the displayed local confirmation must be accepted before the normal
kneel/stand handler runs. A dispatch receipt is not a verified outcome.
[Bounded evidence](../../docs/mcp-player-interface/SUPERVISED-STANCE-EVIDENCE.md)
records both-game checks and earlier unsuccessful attempts.

The adapter installs no connector or network service. MCP callers cannot supply
arbitrary filesystem paths, hidden-unit queries or a general input command.
The historical tools below remain read-only and never authorize an action.


The separate [next/previous selection extension](../../docs/mcp-player-interface/SUPERVISED-SELECTION-PILOT.md)
adds `request_selection` when `--supervised-selection` is also supplied. The
game must separately be built with `EXPERIMENTAL_SUPERVISED_SELECTION=ON`; there
is no feature negotiation or transparent fallback. It uses the same
fresh panel capability and `read_stance_status` ledger, requires local
confirmation, and dispatches the ordinary unit-cycling handlers. A dispatch
receipt does not claim selection changed; inspect a fresh context afterward.

## Run and boundary

Python 3.10+ on POSIX with `flock` and `O_NOFOLLOW`; the server itself uses only the
standard library. Launch locally as a subprocess (no installation/registration):

```
python adapter.py --hud-dir /absolute/private/hud --cursor-dir /absolute/private/cursor
```

Directories are explicit trusted operator launch configuration. Either may be
omitted, making that tool unavailable. The MCP tools are:

- `read_last_hud({require_current: false})`: four already-public HUD values and
  their existing source envelope, or the source's absent-HUD observation.
- `read_last_cursor({require_current: false})`: metadata plus one bounded PNG image
  content block of the existing displayed cursor-region RGB pixels, or null crop.
  Output schema `openxcom-mcp-last-cursor/0` preserves `source_schema` separately
  because PNG transport replaces the original hex pixel field.
- Both accept `require_current: true`, which **always** returns the same
  `unsupported_freshness` tool error without opening a file. Omitted means false.

Tool names, descriptions and every successful observation retain the historical
contract. HUD and cursor sessions have different lifecycle scopes and are never
joined or described as synchronized. `session`/`public_revision` describe producer
provenance only; they do not attest current liveness or session identity of a lock
holder. These historical tools cannot authorize gameplay, and clients must not treat
their records as current-state authority. This policy is a consumer requirement, not a
claim that software can stop a malicious consumer using historical observations.

## File and schema safeguards

For historical reads only, `snapshot.json` plus `observer.lock`, or `cursor.json` plus `cursor.lock`, are
opened. All directory path components reject symlinks. The pinned leaf must be
owned by the effective user, mode 0700. Opened files must be regular, owned by that
user, mode 0600, single-link, no symlinks. `O_NONBLOCK` prevents FIFO opens hanging.
Unknown schema keys and values fail closed; bools are not integers. HUD values
use the producer's unsigned 32-bit range, 0..4,294,967,295. Input JSON has
no duplicate keys or nonfinite constants. Files are bounded at 102,400 bytes
before/during reads, and mutation of the opened inode during reading is rejected.
Atomic producer replacement may yield a valid historical observation or a conservative rejection if the opened inode was unlinked during the read.

The lock must reject a nonblocking shared lock, and its inode must remain present
at the end of the read. This is only a conservative conflicting-lock gate. It
neither identifies the holder nor binds it to a snapshot session, and holding an
exclusive lock proves no frame was rendered. Lock probing itself is not a proven
noninterfering operation. Same-user malicious file modification is outside this
cooperative local prototype's trust boundary. If the producer exits immediately
after the check, a result remains historical. Missing/free/replaced locks fail
closed with one generic error; no detailed failure reason or private path leaves
the process. A new writer could hold the lock while old snapshot bytes remain;
those bytes must still be interpreted solely as historical, never fresh.

The cursor accepts only the existing exact software profile; screen bounds are
1..4096 and crop bounds 1..128 per dimension, contained in the screen. Pixel text
must be exact lowercase RGB24 hex with six characters per pixel. PNG contains only
IHDR/IDAT/IEND, dimensions, and lossless RGB scanlines; no timestamps, comments,
paths, palette inference or hidden data. PNG bytes are bounded at 50,000. No
external PNG is decoded by the adapter.

The server rereads each historical request and retains no image/session cache.
`CursorCache` is an internal tested foundation only: it rejects regression or
conflicting same-revision payloads and old-session reappearance; it is not wired
to MCP or a freshness mechanism. No event history is reconstructed or retained.

## MCP compatibility and limits

Wire target: MCP **2025-11-25**, newline-delimited UTF-8 JSON-RPC over stdin/stdout.
`initialize`, `notifications/initialized`, `ping`, `tools/list`, and `tools/call`
are supported. No other capabilities are advertised. An unsupported requested
version receives the supported version, allowing the client to disconnect. Batch
messages are rejected. Notifications do not receive responses. Request lines are
limited to 16,384 bytes; an oversized line gets one bounded error and connection
close without draining unbounded data. Responses are bounded to 131,072 bytes.
EOF exits. Diagnostic errors are generic; stdout contains protocol messages only.
The trusted parent client owns wall-clock timeouts/termination for incomplete
stdin lines. This is a deliberately narrow handwritten protocol implementation,
not a general-purpose MCP stack or proof of complete conformance.

Official sources consulted 2026-10-10:

- https://modelcontextprotocol.io/specification/2025-11-25/basic/transports
- https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle
- https://modelcontextprotocol.io/specification/2025-11-25/server/tools
- https://github.com/modelcontextprotocol/python-sdk

The current official Python SDK documentation presents v2 APIs. Interoperability was checked with official Python SDK 1.29.0, which declares
protocol 2025-11-25. No assumption of v2 API compatibility
is made. The standalone server does not
import the SDK. `sdk_smoke.py` is optional and requires the installed SDK.

## Optional post-request presentation channel

The historical APIs above are unchanged. Fresh pixels are a separate experimental
slice, enabled only in a build with EXPERIMENTAL_CURSOR_OBSERVER=ON (default OFF)
and explicitly launched with a transient inherited channel:

```
python launch_presentation.py -- /path/to/openxcom [operator game arguments]
```

The launcher uses only the Python standard library. Optional --hud-dir and
--cursor-dir preserve historical file reads. The game command is trusted operator
configuration, never an MCP tool argument. The launcher creates one connected
AF_UNIX SOCK_SEQPACKET pair; there is no socket pathname, listener, port,
registration, credential, or durable channel. Only the intended launched child
inherits the producer descriptor. Existing display settings are preserved. Both
game stdout and stderr are discarded at process creation with `subprocess.DEVNULL`,
including startup, shutdown and logger fallback; neither stream is captured,
forwarded or decoded. Protocol EOF closes observation without killing the game.
Unsupported systems/socket types fail closed. This is cooperative trusted local
IPC, not protection against a malicious same-user process or modified executable.

The matching native build requires `debug`, `debugUi`, `traceAI` and
`verboseLogging` to be false and the effective logger level to be `LOG_FATAL`
through `LOG_INFO`, inclusive. Qualification is initialized after actual
configuration parsing and logger setup, before game construction or any observer
hello. A debug build at `LOG_DEBUG` is unqualified even with all four options
false. Neither the launcher nor the native checks rewrite settings to qualify.

Qualification is rechecked at observer admission/publication boundaries, including
before sending a completed audio window. A rejected profile cannot become qualified
again in that process. Each writer/channel retires when it next reaches its check,
not through an instantaneous global tick. Historical writers release their locks
and retain previously lawful snapshot bytes as historical/unverified records.
The saved-battle debug-mode HUD guard remains separate. Raw engine log files retain
their normal behavior and are never observation inputs. Existing hello/schema
strings do not authenticate the executable. See the [complete diagnostic-isolation
contract and checks](DIAGNOSTIC-ISOLATION.md).

The optional tool read_presented_cursor({}) returns one bounded final RGB PNG with
semantics presented_after_request. It means that a normal presentation occurred
after that specific request was consumed, not instantaneous current game-world
state, desktop visibility, or authority to act. It can show menus or loading if
that is what was actually displayed. It never forces a frame, simulation update,
FOV calculation, input, cursor motion, or battle transition. The default version 1
channel supports pixels only; historical HUD sessions remain independent and are
never joined to pixels.

Displayed HUD is a separate explicit version 2 opt-in:

```
python launch_presentation.py --presented-hud -- openxcom [operator game arguments]
```

Only this flag sets the child's OPENXCOM_PRESENTATION_VERSION=2. The launcher
ignores an inherited value of that variable when choosing a version. Both ends
must use the selected version: there is no automatic upgrade or downgrade.
Version 1 keeps its original hello, reply and public cursor-output schema.

Version 2 adds read_presented_hud({}). Its text-only result uses
openxcom-mcp-presented-hud/1 with exactly schema, epoch, challenge, semantics,
profile and hud. The hud value is either null or an exact object containing
time_units, energy, health and morale. Each is a strict integer from 0 through
999; booleans, floats, strings, negative values, extra keys and larger values
are rejected. The producer decodes complete numbers from the final displayed
RGB, requiring a blank trailing glyph slot. It returns generic null when the
four numbers cannot all be proven from an eligible display. No reason for a null
HUD is exposed. A null HUD is a successful presentation observation; it does not
imply anything about undisplayed game state.

The HUD tool validates the entire same-request version 2 reply, including its
unchanged eligible cursor crop proof, before exposing the four values or null.
It never reads historical HUD files, promotes historical observations, or falls
back to cached values. Separate calls to the HUD and cursor tools are separate
presentation requests, not an automatically synchronized pair. The existing
read_presented_cursor output retains its exact public schema and key set even
on version 2; it does not add a hud key. Historical read_last_hud behavior,
unsigned 32-bit values and require_current denial are unchanged.

A one-time hello contains protocol version, OS-random producer epoch and random
admission token. Each fixed 96-byte request contains epoch, admission token and
OS-random challenge. Exactly one request is consumed on the main thread before
normal composition begins. Its challenge remains immutable for that frame; the
admission token rotates on consumption. Only after a successful SDL_Flip and
eligible exact final crop does the producer send an atomic bounded reply containing
the challenge, epoch, next admission token and exact crop. The post-flip code never
reads requests. A request arriving during composition or after flip must await a
later normally composed frame. No engine frame/tick counts, timestamps, busy reasons,
world IDs or game RNG enter this protocol. Unchanged public pixels need no changed
revision to acknowledge a later presentation.

Version 2 uses OPENXCOM-PRESENTATION/2 in the hello and
experimental-presentation/2 in the reply, with the same fixed request length and
the additional required hud key. Version 1 rejects that key and version 2 rejects
a version 1 reply. Reply and hello versions must match the explicit launch choice.

The adapter permits one outstanding request, with a one-second monotonic deadline
(maximum two seconds in the internal API), and at most four successful requests per
second. Deadline is an acceptance limit, never evidence of freshness. It validates
all reply fields, epoch/challenge/token, strict crop geometry and exact RGB bytes;
then exposes only the PNG, public geometry and presentation envelope. The next
admission token stays transport-internal. Malformed, oversized, extra queued,
replayed, mismatched, late or missing responses produce one generic unavailable.
Unsupported presentation, interrupted composition, peer closure and backpressure
never substitute cached pixels. In these cases the fresh channel closes; there is
no automatic retry or reconnect. Relaunch explicitly to establish a new channel.
A display reset closes any already-consumed pending request. An idle reset may
retain the Screen-lifetime epoch: queued requests are consumed only before a later
normal composition. Epoch is a pixel stream, never a battle/HUD session.
Calling faster than the stated bound also closes this prototype channel. Historical
reads and the normally running game remain independent.

Wait for the desired normal eligible display before the first request. A loading
screen, invisible cursor, loss of focus or unsupported renderer can legitimately
make that request unavailable. The exact software-only capture gates remain in
force. This prototype is deliberately conservative and does not yet provide graceful
recovery from every presentation change.

No unsolicited heartbeat, producer polling thread, hidden-event history or
persistent monitoring is added. Main-thread socket operations are nonblocking and
bounded: one request, one queue check, one reply. Entropy and serialization remain
bounded work. Completion/timeout timing and request-driven load are not proved
noninterfering; paired hidden-state timing tests and broader platform validation
remain separate gates. No complete fair-play timing-parity claim is made.

## Optional fixed-window mixed-audio channel

A separate default-off [native collector](../../docs/mcp-player-interface/MIXED-AUDIO-NATIVE.md)
provides `read_mixed_audio({})` only when explicitly launched. See the complete
[audio contract and launch instructions](AUDIO-ADAPTER.md):

```
python launch_presentation.py --mixed-audio -- /path/to/openxcom [operator game arguments]
python launch_presentation.py --mixed-audio --presented-hud -- /path/to/openxcom [operator game arguments]
python launch_presentation.py --audio-only -- /path/to/openxcom [operator game arguments]
```

The native build option `EXPERIMENTAL_MIXED_AUDIO_OBSERVER` defaults to OFF and is
independent of the pixel/HUD feature. Qualified capture requires SDL_mixer 1.2.12,
actual 22050 Hz signed-16-bit little-endian stereo and an entirely qualified
software-mixed music catalog. MIDI, external-command and unknown music types are
conservatively unavailable. No audio setting is changed to make the profile fit.

A success returns compact text metadata plus native MCP `audio` content containing
a canonical WAV: exactly 500 ms, 11025 stereo frames, 44100 unchanged PCM bytes,
and a 44-byte WAV header. The admission callback block is discarded; only later
final postmix blocks are captured. No actor/source ID, position, track, timestamp
or simulation counter is exported. There is no continuous audio history or
physical-device audibility guarantee, and sounds between requests can be missed.

Audio has its own socket, epoch and admission tokens. Existing historical and
visual `/1`/`/2` schemas are unchanged; there is no common audio/video clock or
frame/HUD synchronization. The stdio server processes tool calls synchronously,
so waiting for audio can delay a subsequent visual call. Startup allows 31 seconds
for the ready hello; each capture then has a separate strict two-second acceptance
deadline. After success the adapter requires one second before another request.
Malformed, replayed, late, reset or disconnected audio closes that channel with
one generic unavailable result. No cached clip or automatic reconnect is used.

The earlier mixed-audio evidence records 190 passing Python tests; native
slot/protocol tests and a real-SDL dummy-mixer oracle verify sample bits, channel order, final effects,
zero/mute cases and corruption controls. The official SDK checks audio-only and
combined visual/audio modes. Separate [actual-game menu GUI checks](../../docs/mcp-player-interface/mixed-audio-game-evidence.json)
pass in UFO/TFTD in audio-only mode with intros disabled: eight fresh clips cover
verified effective music/sound/UI volumes 84/84/42 and 0/0/0, and four post-exit
requests become generically unavailable. Independent full-buffer comparison is
provided by the synthetic mixer tests, not these bounded game-integration reads. These use the application
dummy-mixer backend; physical audibility, full gameplay/audio parity and Stage 5
acceptance remain unverified. The controller remains unimplemented.

[Audio payload evidence](mixed-audio-evidence.json) records 88200 native hex
characters, 88571 native packet bytes, 44144 WAV bytes and 58860 base64 characters.
Hex is not compact; byte/character counts and component microbenchmarks are not
model-token measurements, physical-audio tests or end-to-end resource savings.

## Tests and evidence

New diagnostic isolation has separate [native and launcher checks](DIAGNOSTIC-ISOLATION.md#bounded-checks),
including the real logger fallback, startup/runtime/shutdown output and terminal
profile rejection. Run `tests/diagnostic-isolation/run.sh` from the repository root.
The Python suite completes 208 test methods in normal and optimized modes, with
two native-only tests skipped in ordinary unittest. The focused runner separately
passes all 18 sentinel tests and 24 native stage/flag cases; do not sum these counts.
[Final bounded menu checks](../../docs/mcp-player-interface/diagnostic-isolation-evidence.json)
succeed for normal UFO/TFTD cursor PNG, null HUD and 44,144-byte WAV reads. UFO
`traceAI=true` and TFTD `debug=true` make all three tools generically unavailable
while leaving game menus available. All four final runs have empty launcher stderr.
Three earlier UFO visual failures remain recorded without a demonstrated cause;
see the [results and limits](DIAGNOSTIC-ISOLATION.md#verified-results-and-limits).
Earlier runtime results below remain separate evidence. No new battle or complete
fairness claim is made; Stage 5 and controller acceptance remain open.

```
python -m unittest -v
python sdk_smoke.py
python sdk_presented_hud_smoke.py
python sdk_mixed_audio_smoke.py
```

Unit and SDK smoke fixture content is synthetic; separate runtime integration reads
only already-public observations. No raw saves or asset files are read. Unit coverage:
protocol parsing/initialization/unsupported methods, size and numeric limits,
fixed request arguments, strict schemas, malformed geometry/pixels, EOF, actual
stdio process, modes, symlinks/hardlinks/FIFO, no/free/replaced lock, in-place
snapshot and lock replacement races, independent HUD reading, revision conflicts
and session reset, deterministic PNG CRCs and exact decoded RGB roundtrip. The
SDK smoke initializes an actual subprocess, lists both tools, obtains synthetic
historical image content, and verifies current-state requests remain denied.

Displayed-HUD unit tests additionally cover both negotiated versions, strict
shape/type/range checks, duplicate keys, missing or extra fields, cross-version
rejection, stale or mismatched replies, generic null, historical/fresh separation,
unchanged cursor output, and explicit launcher selection. The separate
sdk_presented_hud_smoke.py uses a synthetic subprocess peer through the real
launcher and the official SDK. It checks both versions, exact PNG
bytes, request-bound displayed HUD, generic null and overflow rejection. This
is transport integration evidence, not a game-runtime or timing-parity proof.

`cost-evidence.json` measures **bytes and characters only** for 128×128 synthetic
RGB: solid PNG 125 bytes, patterned 302, incompressible-like noise 49,363, versus
98,304 hex characters. Base64 costs are recorded separately. Compression depends
on image content. These are not model vision-token, CPU, latency, noninterference
or total-resource savings claims. Actual UFO integration also passed through the official SDK against already-public
producer files: HUD 58/62/33/100 and 128×128 PNG matched the independently read
same-session/revision RGB exactly. Current-state requests were denied; after normal
game exit both historical tools rejected the unlocked snapshots. The two
`actual-ufo-*-evidence.json` files record metadata-only results (no assets/paths).
The live capture preceded the final cursor transport-schema naming refinement
and HUD unsigned-range correction; the behavioral results are retained, with run session identifiers omitted. The
filesystem and PNG paths are unchanged. The final wrapper and corrected boundary
validation were rechecked with unit and SDK smoke tests; observed runtime HUD
values remain valid under both ranges.
Historical file-based TFTD adapter integration is **untested**. The optional
request-bound channel has separate [UFO/TFTD validation](../../docs/mcp-player-interface/PRESENTATION-PROTOTYPE.md).
Broader platform behavior and paired hidden-state timing parity remain separate gates.

### Selected displayed-HUD runtime evidence

The [/2 build and runtime record](../../docs/mcp-player-interface/presented-hud-build-evidence.json)
reports 14 successful reads across UFO/TFTD, including null HUD for menus,
overlays and cursor overlap, displayed stat changes after kneeling, and reload.
TFTD pointer exit closed observation without stale output. Nine comparisons
against separately sampled screenshots matched; two did not, and one initial UFO
menu unavailability remains unexplained. These are bounded checks, not full-frame
or simultaneous-screenshot equivalence, hidden-state timing parity, or controller
acceptance. Raw screenshots and packets are not published.
