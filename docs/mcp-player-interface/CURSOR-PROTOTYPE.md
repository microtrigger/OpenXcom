<!-- New project documentation: SPDX-License-Identifier: GPL-3.0-or-later. See repository LICENSE.txt and LICENSE-NOTICES.md. -->
# Experimental final-pixel cursor-region observer

This is a separate, default-off read-only experiment implementing the selected
[vanilla perceptual parity policy](PERCEPTION-CONTRACT.md). It does not classify
cursor colors as enemies, expose target validity or query arbitrary map cells.
It has no input API, network server or MCP transport.

## Boundary and profile

The render loop composes all visible game states, overlays, FPS display and the
normal UI cursor. The observer then reads a bounded region from the final
software display surface after scaling, palette application and successful
`SDL_Flip`. It never reads map occupancy, units, sound sources or saved games.
Menu/overlay pixels therefore replace the underlying map in the crop.

The first supported profile is software, single-buffer, 8-bit palette output,
uniform integer scaling from 1× to 6×, no black bands and no asynchronous blit.
OpenGL, hardware/double buffering, 32-bit filters, inexact palettes and debug
options fail closed. A hidden cursor or inactive/unfocused/mouse-outside game
window yields unavailable when a frame is next presented, rather than treating
old pointer coordinates as current hover. If no frame is rendered, the previous
historical file may remain unchanged. This is an application-output boundary; external desktop
occlusion, physical monitor output and the consumer's freshness are not proven.

A crop contains at most 128×128 display pixels, clipped at screen edges. Position
and dimensions describe screen pixels only. It is centered on the actual UI
cursor anchor rather than a caller-supplied coordinate. Semantic interpretation
of cursor appearance or surrounding terrain remains unvalidated.

## Build and local output

Configure the normal project with `-DEXPERIMENTAL_CURSOR_OBSERVER=ON`; the HUD
flag is independent. Use the same pinned dependency/toolchain procedure as the
[HUD prototype](HUD-PROTOTYPE.md). For a run, create a new directory owned by the
running user with mode 0700 and set `OPENXCOM_EXPERIMENTAL_CURSOR_DIR` to its
absolute path. Without the runtime opt-in no observation file is produced.

The POSIX writer uses a private lock, random OS-generated session nonce and
atomic replacement of `cursor.json`, mode 0600. The single bounded JSON object
contains schema `experimental-presented-cursor/0`, session, `public_revision`,
`semantics: last_observation`, `freshness: unverified`, an unavailable flag and
`crop`. The crop is null when unavailable or unsupported; otherwise it contains
`screen_width`, `screen_height`, `x`, `y`, `width`, `height`,
`format: rgb24-hex-row-major` and hex-encoded RGB bytes. Decode only after
validating bounded dimensions and exact byte length. No game asset is embedded
in the source repository; observations may naturally contain rendered assets.

Revision changes depend on exact allowed crop bytes/geometry or availability,
not internal ticks. Identical reads do not create heartbeats. This is only the
latest observation, not a reliable animation history: intermediate frames can
be overwritten before a consumer reads them. A crashed process or publication
failure may leave an old file. Consumers must not treat file presence or an old
non-null crop as current state; an independently verified live session is needed.

## Cost and current limits

A full crop is 49,152 raw RGB bytes or 98,304 hex characters plus JSON metadata.
It is a presentation-boundary experiment, not the planned compact semantic MCP
interface. Resource savings have not been measured; per-frame extraction and
publication can add CPU/I/O overhead. No performance/noninterference claim is
made. Future compression or semantic fields need separate validation against
these exact presented pixels.

## Validation status

Independent source review found no blocker for controlled runtime verification.
Synthetic surface tests pass for clipping, palette equivalence, scaling and
unsupported-profile rejection; file/cache tests pass for publication and
revision semantics. Structural boundary checks and the existing HUD tests also
pass. The build with both observers disabled contains no observer symbols or
cursor runtime/schema strings. These checks do not substitute for paired
actual-engine fixtures or visual verification in both games.

Controlled runtime checks on 2026-10-10 passed at 960×600 with 3× software
scaling: UFO and TFTD main-menu, combat cursor-region and in-game menu-overlay
crops each matched the game's built-in PNG screenshot byte-for-byte. Each full
crop comparison covered all 49,152 RGB bytes. UFO top-left clipping matched a
64×64 crop (12,288 bytes); pointer exit and focus loss produced null. A TFTD
static overlay remained byte-identical for ten seconds. Both games produced
null on graceful exit. These checks cover only the supported SDL rendering
profile and do not certify other backends. No hidden save data was read. See [machine-readable evidence](cursor-build-evidence.json).

The legacy black-tile yellow-cursor scenario, audio parity, complete transient
history, performance/noninterference and full observer/controller gates remain
open. No campaign is authorized by this prototype.
