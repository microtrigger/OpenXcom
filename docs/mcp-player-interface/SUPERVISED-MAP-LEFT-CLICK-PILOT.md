<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised ordinary map left-click

This bounded `practical-play/1` extension proposes exactly one ordinary left-click
at a location in a freshly presented tactical image. It may start walking, select
a visible friendly unit or do nothing. It is not a guaranteed move-to command.
The normal game owns pathfinding, TU, energy, reservations, turning, doors,
contacts, reaction fire and interruptions. Controller code does not query a
route, target occupancy, passability, destination eligibility or predicted cost.

## Explicit opt-in

Build the default-OFF `EXPERIMENTAL_SUPERVISED_MAP_LEFT_CLICK` option together
with its supervised-turn/stance prerequisites. Launch with
`--supervised-stance --supervised-turn --supervised-map-left-click`.
The launcher scrubs inherited activation. Native L activation requires both
`OPENXCOM_TURN_ENABLED=1` and `OPENXCOM_MAP_LEFT_CLICK_ENABLED=1`, with the
stance transport explicitly configured. Existing controls retain their opt-ins.

The first left-click profile requires the normal default path-preview option,
`PATH_NONE`. A nonzero option makes L unavailable without rewriting the user's
setting. This check applies only to L, not to the right-click tool. With previews
enabled, normal movement may require a later matching click; this pilot never
synthesizes that second click. A future preview-capable proposal requires its own
observation and confirmation contract.

## Proposal and consent

Use `read_tactical_context({})` for a newly presented 320×200 final-pixel PNG.
`request_map_left_click` accepts its opaque `capability`, `control_epoch`, a random
`command_id`, and integer screen coordinates x=16–303/y=40–127. It accepts no
world coordinates, unit IDs, button selector, modifiers or approval field.

A local modal shows the captured image, target ring and a warning that the click
may move/select and can trigger reaction fire. Only fresh local input after that
modal was actually presented can approve it. Camera, actor pose, UI and session
bindings are checked again before the ordinary action is invoked once.

Tactical V capabilities authorize one R or L proposal; panel O capabilities
remain limited to stance/selection. All actions share the non-evicting 64-command
ledger and `read_stance_status`. Exact repeats query status only. Reusing an ID
with different coordinates/action is rejected. A dispatched receipt records
ordinary-handler invocation, not movement, arrival, selection or survival.

## Natural completion and manual control

A new tactical view is mandatory after dispatched L. A bounded wait can cover the
same live actor/battle's ordinary walking and turning, including its temporarily
hidden map cursor. The busy-wait budget ends eight seconds after dispatch;
status/observation calls do not restart it. The adapter allows ten seconds for
that post-L view only. Ordinary action/status and R observation budgets remain
unchanged. An already-ready observation retains the ordinary presentation window.

The single matching key release completing a fresh Y confirmation is allowed
to finish that consent gesture; it does not authorize another action. Any other
manual input retires the busy-wait permission. It is also retired by focus or state/lifetime changes,
disconnection and unsupported transient states. Projectile/explosion/falling,
actor-loss, overlays and visible warnings remain outside this narrow wait profile.
They still run through the game normally. A failed view stops MCP control without
resending the click, accelerating the game or cancelling its queued walk.
Manual right-click retains the game's normal cancellation behavior; an active
step may finish before stopping.

An idle newly selected supported soldier can receive a fresh view. A normal click
may instead select an actor outside the inherited soldier-only observer profile,
such as a vehicle, leaving the subsequent view unavailable. No pre-filter or
hidden target query prevents this case. Slow, interrupted or unsupported outcomes
require manual reconciliation. Transient audio/visual history and full fairness
are not established by a final ready image.

## Transport

The L packet is exactly 103 ASCII bytes:

`L` + epoch32 + capability32 + command_id32 + x3 + y3.

Coordinates are zero-padded decimal. Existing V canonical visible-RGB encoding,
PNG transport, size bounds and generic errors are unchanged. Unknown/disabled
syntax fails closed; no transparent fallback or public busy/queue query is added.

## Verification status

Both UFO/TFTD normal native gates pass 19 manual/control pairs, 36 adversarial
worlds and 38 paired outcome views each. All 18 actual-source faults are detected in both games; prior stance, selection
and right-click gates also pass in both. See [recorded evidence and limits](SUPERVISED-MAP-LEFT-CLICK-EVIDENCE.md).
Existing right-click evidence retains its original source and scope. This milestone does not implement inventory, attacks,
end-turn, safe batches, full Stage 5 acceptance or actual token benchmarking.
