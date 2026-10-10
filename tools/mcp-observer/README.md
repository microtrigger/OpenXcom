<!-- New project documentation: SPDX-License-Identifier: GPL-3.0-or-later. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Local OpenXcom historical-observation MCP prototype

**Read-only inspection, not a controller or current-state observer.** This is an
standalone Python prototype in this repository. Running it changes no engine
source, game installation, profile, save, connector registration or network service.
There are no gameplay/input tools, arbitrary paths supplied by MCP callers,
credentials, uploads, subscriptions, timers, background polling or network sockets.

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
holder. The adapter cannot authorize gameplay, and clients must not treat these
records as current-state authority. This policy is a consumer requirement, not a
claim that software can stop a malicious consumer using historical observations.

## File and schema safeguards

Only `snapshot.json` plus `observer.lock`, or `cursor.json` plus `cursor.lock`, are
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

## Freshness: unresolved producer change, not a TTL workaround

Existing producer files explicitly say `last_observation` / `unverified`, retain
unchanged bytes during idle/blocked phases, and may survive a crash. File age,
open descriptors, locked files, a live PID, changed revisions or session nonces do
not establish that a presentation occurred after this request. No combination is
promoted to proof here. An idle unchanged frame must not be mistaken for dead, and
a hung live process must not be mistaken for current.

A possible **future** protocol, requiring separately reviewed engine work:

1. The trusted launcher establishes a bounded local request/response channel
   directly with the intended producer, with an OS-generated process-session
   binding. No networking or external registration is needed. The observer's
   existing per-Battlescape HUD session remains distinct from presentation session.
2. A read carries one random single-use challenge. The producer responds only
   after a successful normal presentation following that request, binding the
   challenge, presentation session, relevant HUD session and exact public payload.
   No game tick, busy reason, wall-clock producer timestamp or hidden counter is
   returned. The challenge does not consume game RNG or force simulation progress.
3. Pixel acknowledgements refer to the final displayed crop. A HUD response needs
   its own demonstrated connection to the same presented HUD values; merely
   echoing an older think-time cache is insufficient. Reload, teardown, focus and
   unsupported rendering must invalidate the relevant scopes.
4. The consumer accepts the matching acknowledgement once within a bounded request
   lifetime; missing, mismatched, replayed or ambiguous acknowledgements return
   generic unavailable. Deadline is a safety limit, **not** evidence of freshness.
   The result would mean “presented after this request,” never an instantaneous
   view of an unchanging world.
5. No unsolicited heartbeat, frame counter, polling task or durable event history
   is added. Nevertheless request completion and timeout timing can reveal state
   or create load; cadence normalization/rate limiting and paired hidden-state
   timing tests would still be required. A challenge alone does not solve timing
   side channels or prove simulation noninterference.

This handshake is design only, not implemented. It must not be enabled by merely
changing `freshness` strings, trusting caller assertions or adding a bypass flag.

## Tests and evidence

```
python -m unittest -v
python sdk_smoke.py
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
TFTD adapter integration is **untested**. Cross-platform behavior, adversarial
timing parity and producer freshness remain separate gates.
