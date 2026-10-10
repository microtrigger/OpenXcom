<!-- SPDX-License-Identifier: GPL-3.0-or-later. New project documentation, 2026-10-10. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Request-bound pixel presentation prototype

The optional `read_presented_cursor({})` tool returns a bounded PNG crop that was
normally presented **after its particular request was consumed**. It does not
claim an instantaneous view of the game world. Historical HUD/cursor tools keep
their existing semantics, and requests requiring fresh HUD values still fail
closed. No gameplay commands, entity queries or arbitrary cursor probes are added.

## Ordering and connection boundary

An explicitly launched game inherits one connected, nonblocking Unix
`SOCK_SEQPACKET` descriptor. There is no listener or durable registration.
A random producer epoch identifies the presentation stream, independently of
battle/HUD sessions. The consumer sends one fixed-size epoch/admission/challenge
packet. On the main thread, the producer consumes it before normal composition
begins and binds it to that frame. Only successful final presentation of an
eligible crop permits a reply. The post-flip path never reads a new request.

Admission tokens rotate. Replays, extra queued packets, malformed replies,
backpressure, deadline expiry or failed presentation close this prototype channel
and produce generic unavailability. A consumed token cannot cross a display reset;
an idle reset may preserve the stream because the next request is still consumed
before a later normal composition. There is no forced frame, simulation update,
FOV calculation, input or game-RNG use. Explicit relaunch establishes a new channel.

The consumer accepts one request at a time, uses a bounded deadline and permits
at most four successful requests per second. Calling earlier also closes this
conservative prototype channel. EOF closes observation without terminating the
game. The supported software rendering, palette, cursor and focus restrictions
are unchanged from the [pixel-crop prototype](CURSOR-PROTOTYPE.md).

See [adapter instructions and protocol details](../../tools/mcp-observer/README.md)
for the launcher interface. The game command is trusted operator configuration,
not an MCP tool argument. No new Python runtime dependency is required.

## Verification

- The opt-in and default-OFF builds pass. The OFF binary has no presentation
  observer activation or observer symbols.
- Historical HUD, pixel-crop and presentation C++ tests pass. The ordering guard
  rejects a mutation that moves request admission after presentation.
- 43 Python tests pass, covering historical APIs, challenge/epoch/token checks,
  deadlines, malformed and bounded payloads, PNG roundtrips and fresh-HUD denial.
- UFO and TFTD menu, battle and in-game overlay requests each matched the game's
  own screenshot: six full 128×128 crops, 49,152 RGB bytes each, zero differences.
- Repeated UFO requests used different challenges while retaining the same stream
  epoch and identical pixels. Normal menu-to-battle transitions worked in both games.
- Pointer exit produced generic unavailability and no old pixels were returned
  after the channel closed. The game remained usable after adapter disconnect.
- Requests after TFTD exited were rejected. Fresh-HUD requests remained rejected
  in both games.

These are selected runtime and unit tests, not a complete timing, fairness or
platform certification. Exact pixel equality establishes the tested visual output;
source ordering and challenge tests establish the separate request-binding logic.
No wall-clock, model-token or end-to-end resource saving is claimed. Paired hidden-
state timing tests, broader transitions, fresh HUD, transient audio/history and the
controller acceptance gate remain open.
