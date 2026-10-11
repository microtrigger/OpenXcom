<!-- SPDX-License-Identifier: GPL-3.0-or-later. Original contribution 2026-10-11. See LICENSE.txt and LICENSE-NOTICES.md. -->
# Supervised desired-stance pilot

This optional practical-play/1 pilot stages a single **desired** kneel or stand
request for local approval. It has no generic key, click, unit-ID, movement, attack
or hidden-state query. `EXPERIMENTAL_SUPERVISED_STANCE` is OFF by default and
requires the existing cursor-presentation build option. Runtime use additionally
requires the explicit `--supervised-stance` launcher option. The ordinary default
build has no control activation or implementation.

Local confirmation is a deliberately narrow pilot boundary, not a permanent
requirement for a future separately authorized joint-play controller. Accepting
practical play does not establish the original full Stage 5 fairness gate.
Residual request/response timing channels remain an experimental limitation;
using delays for reconnaissance remains prohibited. There is no timing-isolation
broker, constant-latency guarantee or new timing acceptance gate here.

## Observation and approval

`read_stance_context({})` requests a naturally composed later frame. Only an
initialized, normally operable player Battlescape can supply its actual 320×56
bottom panel. The PNG contains the normal displayed soldier name (including
ordinary truncation), stance button, TU and reservation indicators. It is neither
an engine identity nor four numbers substituted for context. The initial profile
supports only 320×200 logical software output at uniform integer scales 1–6,
with exact palette application, successful flip, focus and no panel-overlapping
cursor/FPS or warning. Unsupported or missing context fails generically.

The producer captures final software-display RGB **after** successful ordinary
`SDL_Flip`. Every physical pixel represented by one logical pixel must agree.
It does not force a frame, advance simulation, refresh FOV, decode a name or
export any numeric/unit state. Private guards compare existing displayed-widget
context to the exact selected actor and public stance/TU/reservation state;
they only reject, never report a changed field or predict action eligibility.
Desktop occlusion and physical viewing are not proven.

The returned opaque capability binds that exact image and private actor/context
to a dedicated random control epoch. This epoch is independent of the existing
Screen-lifetime presentation epoch and changes across state/save/display changes.
A separate private selection generation distinguishes identical or truncated
names and selection-away-and-back races. Stance generation and UI revision guard
similar reversions. No pointer, unit ID, internal revision, RNG state or hidden
statistic is returned. A capability expires after two minutes and is one-use.

`request_stance` accepts only the capability, control epoch, random command ID,
visually observed stance and desired stance (`kneel` / `stand`). Equal observed
and desired stances are rejected without toggling. The request reserves the
capability and opens a local modal showing the exact proposal panel, the desired
stance, ordinary 4/8 TU cost, reservations and reaction-fire warning. It returns
`pending_confirmation`. There is no MCP approval boolean or confirmation tool.
The exact modal must first complete a successful normal presentation. Held or
queued-at-open keys/buttons and repeated keydown events cannot approve. Only the
modal's ordinary local Confirm button or a fresh deliberate Y key can approve;
Cancel/Escape, focus changes, unrelated input, or changed context cannot dispatch.
Mouse movement inside the modal is permitted so the local operator can reach its buttons.

On confirmation, the main thread verifies the exact modal, actor, battle/control
context, current displayed values, stance, reservations, connection, focus and
normal UI readiness. It closes only its own modal, validates the restored battle
again, consumes permission and records `dispatched` **before** calling the normal
`BattlescapeState::btnKneelClick` once. There is no delayed synthetic key or input
queue and no validation-only-before-enqueue shortcut. The normal engine alone
handles TU, reservations, FOV and reaction fire. Other state/input/save transitions
invalidate outstanding permission before objects can be replaced. Manual play
remains available; the modal can always be cancelled.

## Receipts and uncertainty

`read_stance_status` returns `pending_confirmation`, `dispatched`, or generic
`unavailable` for a known command ID. `dispatched` means only that the ordinary
handler was invoked. It is **not** a claim that stance changed, that TU was spent,
that reactions completed or that the result was visibly verified. A fresh
post-action image and explicit visual checking establish the separate visible
outcome. Failure to obtain that image stops this client rather than reissuing an
action. Reaction cases may require subsequent normal observation; this pilot
has no hidden reaction-queue endpoint.

The native non-evicting ledger holds at most 64 command IDs per transient launch.
Exact duplicate requests retrieve the existing receipt; conflicting reuse is
rejected. Consumed capabilities cannot authorize new IDs. The adapter queries
status for an explicit duplicate instead of sending D again. Unknown outcome,
disconnect, timeout, malformed/oversized reply or failed observation closes the
adapter channel; it never reconnects or retries an action automatically. EOF
closes control, without terminating or taking over the game. A new launch is a
new explicit operator action, not an outcome-recovery shortcut.

## Wire and build

The explicitly launched game inherits one connected nonblocking AF_UNIX
SOCK_SEQPACKET descriptor through `OPENXCOM_STANCE_FD`. There is no listener,
discovery, durable registration or authentication secret. Native responses are
bounded below 128 KiB; all errors are generic and ordinary diagnostics stay
outside the model transport under the existing diagnostic profile.

- Header: `experimental-stance/1`, `control_epoch` (32 lowercase hex)
- Observe: `O` + 32-hex challenge
- Context: `experimental-stance-context/1`, challenge, control_epoch, capability,
  semantics=`presented_after_request`, panel={width:320,height:56,rgb_hex}
- Propose: `D` + epoch32 + capability32 + command_id32 + observed (`k`/`s`) + desired
  (`k`/`s`), exactly 99 bytes
- Status: `S` + command_id32
- Receipt: `experimental-stance-status/1`, command_id, status
- Generic transport/context rejection: `{"error":"unavailable"}`

The Python public schemas have the `openxcom-mcp-` prefix and encode the same RGB
as a lossless PNG image block instead of exposing hex. Receipt metadata explicitly
states `dispatch_status_not_success`. Existing presentation `/1`, displayed HUD
`/2`, audio and historical read-only APIs retain their previous protocols.

Build with the two opt-in flags, then launch the existing adapter with an explicit
game command and `--supervised-stance`. `EXPERIMENTAL_STANCE_CONTROL_GATE` is a
separate default-OFF fixture option and must not be enabled for ordinary play.
See [the finite gate](../../tests/stance-control/README.md) for reproducible tests,
real-source sensitivity controls and the boundary between dispatch parity and
post-action visual verification. Test source does not itself establish runtime
success; recorded operator-run evidence must identify the actual frozen source.

## Recorded verification

The [sanitized evidence](SUPERVISED-STANCE-EVIDENCE.md) records both-game native
parity/adversarial passes, ten intended actual-source sensitivity failures, and
separate real SDK/adapter/game demonstrations. Each demonstration used two local
confirmations and three fresh panel images to verify standing 40 TU → kneeling
36 TU → standing 28 TU. Earlier failed attempts remain recorded separately.
These results do not establish completed reaction resolution, full Stage 5
fairness, timing isolation or token savings.
