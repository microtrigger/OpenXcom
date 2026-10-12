<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-12. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised next/previous unit selection

This bounded `practical-play/1` extension proposes the ordinary next/previous
unit control, with the same local confirmation boundary as the
[stance pilot](SUPERVISED-STANCE-PILOT.md). It does not target an engine unit ID,
list eligible units, predict the destination, move a unit or spend TU directly.
The ordinary game handler determines the result, including wraparound, skipped
units, camera centering and cancellation of the current action.

## Explicit opt-in

Build `EXPERIMENTAL_SUPERVISED_SELECTION=ON` together with
`EXPERIMENTAL_SUPERVISED_STANCE=ON` and its cursor-presentation prerequisites.
Selection is OFF by default. Launch with both `--supervised-stance` and
`--supervised-selection`. The latter alone is rejected. The launcher removes
any inherited selection activation and enables it only for this explicit launch.
The native build additionally requires literal runtime activation `1`.

Use `read_stance_context({})` for a newly presented 320×56 normal bottom-panel
image and its opaque capability. `request_selection` accepts that capability,
control epoch, random command ID and `direction` (`next` or `previous`). A local
modal displays the exact observed panel and the proposed ordinary control.
Only a fresh deliberate local confirmation after the modal was presented can
dispatch. No MCP endpoint can approve it. `read_stance_status` reads the shared
command ledger; it applies to both action kinds.

Selection and stance share one-use capabilities and a bounded non-evicting
64-command ledger. A capability cannot authorize both actions, and a command ID
cannot be reused for another action or direction. An exact duplicate retrieves
status rather than resending a command. Disconnect, malformed reply, timeout
or failed post-action observation stops control without an automatic retry.
A dispatched receipt means only that the ordinary handler ran. Obtain and inspect
a fresh panel before proposing another action; it is not proof of a distinct
unit, a particular destination or completion of a larger gameplay task.

## Scope and safeguards

The starting context deliberately retains the stance pilot's narrower support:
an initialized, idle, focused player Battlescape with a normally displayed,
conscious, grounded original-player soldier and supported software rendering.
Selection additionally requires the actual next-unit button, which hosts both
cycling hotkeys, to be visible and wholly inside that panel. The same actor,
selection generation, displayed context, UI revision and control epoch must
still match when the request is accepted and again immediately before dispatch.
State, input, focus and display changes invalidate outstanding authority.

The normal cycle can select another eligible player unit, including a unit
outside the supported starting-context profile. In that case a subsequent
context can be unavailable and the client stops. No target-state preflight is
added to avoid that case. Selection may also leave the same unit selected.

This extends a limited supervised pilot, not the full controller acceptance
claim. It does not close Stage 5, establish timing isolation, implement movement
or attacks, or establish token savings. Historical stance and observation evidence retains its original source and scope.

## Transport

The existing transient stance channel, context and status schemas remain intact.
The new request is exactly 99 ASCII bytes:

`N` + epoch32 + capability32 + command_id32 + direction (`n`/`p`) + literal `0`.

Unknown, disabled or malformed action syntax fails closed. No additional fields,
unit counts, destination IDs, availability descriptions or hidden-state values
are returned. Errors remain generic. A selection-capable adapter must be paired
with a selection-capable explicitly enabled game; this pilot adds no negotiation
or transparent fallback.

## Verification

Focused adapter regression checks and an isolated native selection gate accompany
this extension. The gate uses an asymmetric three-unit ring so an incorrectly
reversed direction cannot pass by selecting the same second unit. Runtime pass
claims require actual recorded runs; the existence of the harness is not evidence
that UFO or TFTD passed. Full gameplay, physical display visibility and model/token
measurements remain outside this milestone.
