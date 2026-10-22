# Error handling: decisions, not just logging

Scope: semantic agent-control tools. This is an additive classification at the
adapter boundary, not a change to gameplay or permission to replay actions.
Existing `error`, receipt `status`, `command_id`, and uncertainty semantics remain.
A classification describes the current operation; it never proves that an earlier
command did not mutate the game.

## Requirements and safe reactions

The [action loop](README.md#correct-action-loop) requires fresh authority, one
command, a trustworthy receipt and a separate visible postcondition. The
[gameplay plan](gameplay-plan.md) stops on unknown outcomes. Diagnostics must not
read hidden world state, leak packets/tokens/paths/exception messages, or restore
runtime glyph/readability acceptance gates. Missing text is not empty text.

| Category | Evidence available | Safe reaction |
| --- | --- | --- |
| Unsupported/partial UI, absent optional detail, audio unavailable | Valid state/field explicitly reports limited coverage | Not a tool fault. Use only offered controls; a bounded fresh read may wait for a known transition. Never invent missing values or commands. |
| Caller schema invalid | JSON-RPC invalid-params response before execution | Correct the request. This does not authorize retrying a different, already attempted command. |
| Stale authority, action/target not offered | Local pre-send guard failed | Obtain fresh observation and make a new decision only when no previous outcome is unknown. No automatic mutation retry. |
| Concurrent use, conflicting command ID, exhausted ledger | Exact local guard | Respect serialization/identity; stop on resource exhaustion. Never clear the ledger to force a retry. |
| Transport timeout/EOF/I/O, framing or unsolicited packet | Exact channel boundary | Stop the affected session. EOF does not identify why the native process closed its channel. Starting a launcher again can create another game, not recover this session. |
| JSON parsing, schema/identity/freshness validation | Exact validation phase | Fail closed. Do not consume questionable state, raw text or authority. No automatic retry. |
| Native generic unavailable envelope | Valid native error, but no reason/stage in current wire format | Report unspecified native refusal, not guessed timeout, unsupported UI or malformed JSON. Stop; no retry contract exists for this envelope. |
| Mutating send attempted, receipt not trustworthy | Adapter send-attempt boundary | Critical unknown outcome. Stop/reconcile, never resend with a new ID. |
| Valid native receipt `unknown_after_dispatch` or result-query `unknown` | Native explicitly reports uncertain result | Stop/reconcile. This is distinct from losing a receipt in transport. |
| Valid native receipt `unavailable` | Native uses this both before and after entering a handler | Conservatively unknown action outcome. Never claim “nothing changed” from this status alone. |
| Valid receipt `dispatched` | Handler dispatch established | Not gameplay success. Verify later visible state; a subsequent read failure does not invalidate or replay the dispatch. |
| Unclassified legacy failure | Only old generic exception is available | Explicit unknown classification; stop rather than invent a cause. |

No category enables automatic action replay. Bounded waiting is only for valid
observations of a supported transition on a usable channel, not for a broken
transport or unknown mutation outcome.

## Complete path-family inventory

Python semantic paths: `agent_control.py` parsing and validators; `exclusive`,
`packet`, `exchange`, `observe`, `read_events`, `execute`, `_read_result`, and
`_accept_result`. `adapter.py` separately handles JSON-RPC framing/argument
validation, semantic tool errors, and outer process failures. `snapshots.py`
provides a shared legacy exception and historical observation checks.

Native paths (`src/Observer/AgentController.cpp`): setup/send/counter/token/event
journal failures close the channel; receive EOF/truncation/ancillary data and
invalid handshake also close it. Parse failure after readiness, overlapping
requests, event-page refusal, and read-presentation deadline return a generic
error. Grant/generation/focus/physical-input/queued-frame checks reject pending
commands. Ledger conflict/full and handler-false may yield `unavailable` receipts.
Handler entry precedes the handler-false status; exception or missing post-frame
can yield `unknown_after_dispatch`. Paint-health checks concern draw bookkeeping,
not a glyph-readability requirement. A destructor closing a channel is normal
cleanup, not evidence of a fault.

Non-semantic profiles (historical snapshots, supervised controls, presentation,
HUD/cursor images) have separate availability and freshness contracts. This
bounded implementation does not silently reclassify all those profiles. Their
legacy generic failures remain a documented migration gap, not semantic success.

## Runtime versus development evidence

Runtime failure metadata is a fixed vocabulary sufficient for safe action:
code, category, severity, phase, action outcome and recovery. It contains no raw
native payload, capability, exception text, hidden values or filesystem path.
Development tests inject each boundary independently. Native-wire reasons that
are not transmitted cannot be reconstructed from a generic failure.

The earlier production read failure remains unresolved: its runner retained only
`tool_error_stop_no_replay`, discarding the detailed tool result. New classification
cannot retroactively identify that failure as a timeout, cursor issue or validation
fault. Legacy cursor-related action omission and that unavailable error are distinct
observations; validation-related causes have not been excluded.

## Qualification boundary

Fault-injection and regression results accompany this change. They do not claim
that the old production failure was reproduced or fixed, nor that every native
close cause is now remotely distinguishable. No game campaign is required to test
these transport/contract decisions; future live qualification remains separate.
