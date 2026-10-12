<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised ordinary map right-click

This bounded `practical-play/1` extension proposes an ordinary right-click at
an observed screen location. The normal game path can turn the selected unit or
attempt to open a door. It applies the game's normal TU, reservation and contact
rules. It is not a desired-facing setter, movement command, path query or target
oracle. A dispatched receipt does not mean the turn completed or a door opened.

## Explicit opt-in and observation

Build `EXPERIMENTAL_SUPERVISED_TURN=ON` with
`EXPERIMENTAL_SUPERVISED_STANCE=ON` and its presentation prerequisites. The feature
is OFF by default. Launch with both `--supervised-stance` and `--supervised-turn`;
the launcher scrubs inherited turn activation. Native activation additionally
requires the literal runtime value `1`.

`read_tactical_context({})` requests a newly presented 320×200 image and opaque
one-use capability. Only final presented pixels are exported. The native indexed
representation is reconstructed from visible RGB in first-seen color order,
with unused palette slots zeroed; it does not copy an unused engine palette.
The adapter returns a PNG and opaque metadata, without world coordinates,
unit IDs, target lists, passability, action-cost predictions or hidden state.

`request_turn` accepts `capability`, `control_epoch`, a random `command_id`, and
integer screen coordinates `x` (16–303) and `y` (40–127). No MCP tool approves
execution. A local modal shows the captured view and location marker. Its top
banner explains that the ordinary right-click may turn or open a door and that
normal TU and reservations apply. Only a fresh local confirmation after the
modal was presented can dispatch the ordinary action once.

## Binding and interruption

The capability is bound privately to the current player actor, camera geometry,
view level, pose and UI generation. Camera or actor-pose changes away and back
invalidate it. Unsupported targeting, previews, scrolling, modifiers, overlays,
focus loss or changed context fail closed. Pixel animation alone is not used as
a full-frame staleness test: ordinary smoke, fire and cursor animation must not
invalidate an otherwise unchanged geometric context.

Tactical capabilities authorize only right-click proposals. Panel capabilities
from `read_stance_context` authorize only stance/selection proposals. Reading
either retires the previous capability. All actions share the non-evicting
64-command ledger and `read_stance_status`; reuse with different parameters or
a different action is rejected. Exact duplicates read status without resending.

After a dispatched right-click, a fresh tactical view is required before another
context/action. The view request may wait briefly for the ordinary animation to
reach a naturally ready frame. It never advances the simulation or forces a
render. Timeout, disconnection or failed observation stops the client without
automatically resending the action. Slow or otherwise unsupported outcomes can
therefore require manual recovery. In particular, the inherited HUD guard rejects
a view while a normal TU/reservation warning is visible. A failed read is not
automatically retried by the adapter, even though the game warning may later clear.

## Transport

The native view request is `V` plus a 32-character challenge. The response schema
is `experimental-tactical-context/1`, containing that challenge, control epoch,
capability, `presented_after_request` semantics and a 320×200 view: 64,000 canonical
indices plus a fixed 256-entry RGB palette, hex encoded. The public schema is
`openxcom-mcp-tactical-context/1`. The adapter uses an indexed PNG so the maximum
image remains within the existing response limit.

The action packet is exactly 103 ASCII bytes:

`R` + epoch32 + capability32 + command_id32 + x3 + y3.

Coordinates are zero-padded decimal. Unknown fields, malformed tokens, unsupported
coordinates and disabled commands fail closed. No feature negotiation or fallback
to direct engine setters is added.

## Verification status

Both games pass 15 ordinary-UI/controlled pairs and 29 adversarial worlds, with
30 fresh paired outcome views per game. Nine actual-source faults are detected
in both games. See [results, earlier failures and limits](SUPERVISED-TURN-EVIDENCE.md).
These finite results do not establish full fairness, timing isolation or a
model/token result. The separate native test gate is OFF by default and is never
a production observer.
This milestone does not implement movement, inventory, attacks or end-turn and
does not close the original Stage 5 acceptance gate.
