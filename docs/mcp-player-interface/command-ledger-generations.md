# Acknowledged command ledger generations (schema 41)

Use matching native and adapter versions. Command identity is the pair
`(command_generation, command_id)`, not the bare ID. Each generation retains at
most 256 admitted commands. The initial generation is 1; omitted generation
arguments mean **1**, never whichever generation happens to be current.

`execute_ui_action` and `read_ui_action_result` accept `command_generation`.
Receipts include it. Requests for an old generation are rejected before lookup or
admission, including when the same bare ID exists in the current generation.
A new generation does not change the session epoch, public event cursor or
owned-base identifiers. All action visibility, freshness and single-use grant
requirements remain in force.

## Explicit acknowledgement

Call `retire_action_receipts` with `acknowledge: true` only after preserving and
reconciling the current generation's receipts. It performs no gameplay action.
Retirement requires a nonempty ledger whose every entry has a final `dispatched`
receipt, with no queued command, dispatch, unresolved result or read recovery.
A dispatched receipt acknowledges handler dispatch, **not** successful gameplay:
inspect subsequent observations for the actual result.

Successful retirement returns `status: retired`, `previous_generation`,
`generation` and `retired_count`. The next action must specify the new generation
and obtain a fresh observation. Neither a capacity error nor a blocked retirement
implicitly clears the ledger, reconnects the game or replays an action.

The transition uses a challenge-bound acknowledgement. If its reply is delayed,
the adapter retains the entire old ledger, drains the outstanding response and
queries the same transition. A consumed late acknowledgement is reconciled by a
read-only status query. A permanently missing or unknown response stays blocked;
no fresh transition or gameplay replay is inferred. The native endpoint retains
only the latest transition receipt, so memory remains bounded.

Reusing a bare ID with an explicitly new generation and fresh authority describes
a **new command**, not a retry of an old one. Clients must preserve old generation
information when referring to an earlier action.

## Qualification boundary

Native protocol/controller tests and adapter transport tests exercise repeated
rollover, stale identities, duplicate requests, unresolved receipts and delayed
or missing acknowledgements. Synthetic completion fixtures are not live gameplay
acceptance. Long-session gameplay qualification remains separate; this change
alone does not establish exhaustive UFO/TFTD coverage.
